// MIT License
//
// Copyright (c) 2023 Anton Schreiner
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// clang-format off


#define GFX_IMPLEMENTATION_DEFINE

#include <dgfx/gfx_jit.hpp>
#include <dgfx/camera.hpp>
#include <glm/ext/vector_common.hpp>
#include <glm/ext/vector_relational.hpp>
#include <unordered_set>

// clang-format on

namespace GfxJit {
using namespace SJIT;
using var = ValueExpr;

// A quick experiment based on
// https://www.youtube.com/watch?v=oza36AqcLW8
// https://www.youtube.com/watch?v=oQLmC0e-hpg
// Using a simple spatial hash to do world space filtering

struct HashItem {
    u32   hash;
    f32x3 p;
    f32   v;
    f32   n;
};
static SharedPtr<Type> HashItem_Ty = Type::Create("HashItem", {
                                                                  {"hash", u32Ty}, //
                                                                  {"p", f32x3Ty},  //
                                                                  {"v", f32Ty},    //
                                                                  {"n", f32Ty},    //
                                                              });
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_hash_grid_size, f32Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_hash_table_size, u32Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_hash_table, Type::CreateRWStructuredBuffer(HashItem_Ty));
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_prev_hash_table, Type::CreateRWStructuredBuffer(HashItem_Ty));

HashMap<String, double> g_pass_durations = {};

static var spatial_hash(var i3) {
    // p      = p + sin(p * PI * f32(2.0) + f32x3(28.913363, 91.336453, 133.6453));
    // assert(i3->InferType() == u32x3Ty);
    return pcg(i3.x().AsU32() + pcg(i3.y().AsU32() + pcg(i3.z().AsU32())));
}
class RelocateHashItems {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};

public:
    SJIT_DONT_MOVE(RelocateHashItems);
    ~RelocateHashItems() { kernel.Destroy(); }
    RelocateHashItems(GfxContext _gfx) {
        gfx = _gfx;
        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(64), u32(1), u32(1)});

        var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["x"];
        EmitIfElse(tid < g_hash_table_size, [&] {
            var item = g_prev_hash_table.Load(tid);
            EmitIfElse(item["hash"] != u32(0), [&] {
                var grid_size = g_hash_grid_size.Copy();
                grid_size     = grid_size * pow(f32(2.0), floor(log(f32(1.0) + length(g_camera_pos - item["p"]))));

#if 0
				            
                var sp          = item["p"] / grid_size - f32x3_splat(0.5);
                var probe_coord = (sp).ToI32();
                ifor(3) EmitIfElse(sp[i] < f32(0.0), [&] { probe_coord[i] = probe_coord[i] - i32(1); });

                var v_acc        = Make(f32Ty);
                var v_weight_acc = Make(f32Ty);

                zfor(2){                //
                        yfor(2){        //
                                xfor(2){//

                                        var _probe_coord = probe_coord + i32x3(x, y, z);
                var _hash = spatial_hash(_probe_coord);
                var _item = g_hash_table.Load(_hash % g_hash_table_size);

                EmitIfElse(_item["hash"] == _hash, [&] {
                    var weight = exp(-length(_item["p"] - item["p"]) / grid_size);
                    v_acc += _item["v"];

                });
                        }
                    }
    }
    

            });
#endif // 0
                {
                    var sp          = item["p"] / grid_size;
                    var probe_coord = (sp).ToI32();
                    ifor(3) EmitIfElse(sp[i] < f32(0.0), [&] { probe_coord[i] = probe_coord[i] - i32(1); });
                    var hash     = spatial_hash(probe_coord);
                    item["hash"] = hash;
                    g_hash_table.Store(hash % g_hash_table_size, item);
                }
            });
        });

        // fprintf(stdout, GetGlobalModule().Finalize());

        kernel = CompileGlobalModule(gfx, "RelocateHashItems");
    }
    void Execute(u32 hash_table_size) {
        kernel.Begin();
        kernel.CheckResources();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (hash_table_size + num_threads[0] - 1) / num_threads[0];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, 1, 1);
        }
        kernel.ResetTable();
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
    }
};
class AOPass {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    GfxTexture result = {};
    u32        width  = u32(0);
    u32        height = u32(0);

    var g_output     = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_output"));
    var g_ray_length = ResourceAccess(Resource::Create(f32Ty, "g_ray_length"));

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return result; }

    SJIT_DONT_MOVE(AOPass);
    ~AOPass() {
        kernel.Destroy();
        gfxDestroyTexture(gfx, result);
    }
    AOPass(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;
        result      = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

        var dim = u32x2(width, height);

        var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
        EmitIfElse((tid < dim).All(), [&] {
            var xi = GetNoise(tid);
            var N  = g_gbuffer_world_normals.Load(tid);
            var P  = g_gbuffer_world_position.Load(tid);

            EmitIfElse((N == f32x3_splat(0.0)).All(), [&] {
                g_output.Store(tid, f32x4_splat(0.0));
                EmitReturn();
            });

            var diffuse_ray = GenDiffuseRay(P, N, xi);

            var ray_desc          = Zero(RayDesc_Ty);
            ray_desc["Direction"] = diffuse_ray["d"];
            ray_desc["Origin"]    = diffuse_ray["o"];
            ray_desc["TMin"]      = f32(1.0e-3);
            ray_desc["TMax"]      = g_ray_length;
            var anyhit            = RayTest(g_tlas, ray_desc);
            var v                 = MakeIfElse(anyhit, f32(0.0), f32(1.0));

            var grid_size        = g_hash_grid_size.Copy();
            var linear_grid_size = grid_size * length(g_camera_pos - P);
            var grid_xi          = xi * linear_grid_size;
            var tbn              = GetTBN(N);
            P                    = P + tbn[u32(0)] * grid_xi.x() + tbn[u32(1)] * grid_xi.y();

            grid_size       = grid_size * pow(f32(2.0), floor(log(f32(1.0) + length(g_camera_pos - P))));
            var sp          = P / grid_size;
            var probe_coord = (sp).ToI32();
            ifor(3) EmitIfElse(sp[i] < f32(0.0), [&] { probe_coord[i] = probe_coord[i] - i32(1); });
            var hash = spatial_hash(probe_coord);
            var item = g_hash_table.Load(hash % g_hash_table_size);
            EmitIfElse(
                item["hash"] == hash,
                [&] {
                    item["n"]          = min(f32(64.0), item["n"] + f32(1.0));
                    var history_weight = f32(1.0) / item["n"];
                    var prev_val       = item["v"];
                    var new_val        = lerp(prev_val, v, history_weight);
                    item["v"]          = new_val;
                    item["p"]          = P;

                    g_hash_table.Store(hash % g_hash_table_size, item);
                },
                [&] {
                    item["v"]    = v;
                    item["p"]    = P;
                    item["hash"] = hash;
                    item["n"]    = f32(1.0);
                    g_hash_table.Store(hash % g_hash_table_size, item);
                });

            g_output.Store(tid, v["xxxx"]);
        });

        // fprintf(stdout, GetGlobalModule().Finalize());

        kernel = CompileGlobalModule(gfx, "AOPass");
    }
    void Execute(f32 ray_length) {
        kernel.Begin();
        kernel.SetResource(g_ray_length->resource->GetName().c_str(), ray_length);
        kernel.SetResource(g_output->resource->GetName().c_str(), result);
        kernel.CheckResources();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.ResetTable();
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
    }
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
    }
};
class HashDebug {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    u32        width  = u32(0);
    u32        height = u32(0);

    var g_output = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_output"));

public:
    u32 GetWidth() { return width; }
    u32 GetHeight() { return height; }

    SJIT_DONT_MOVE(HashDebug);
    ~HashDebug() { kernel.Destroy(); }
    HashDebug(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;
        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

        var dim = u32x2(width, height);

        var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];

        EmitIfElse((tid < dim).All(), [&] {
            var N = g_gbuffer_world_normals.Load(tid);
            var P = g_gbuffer_world_position.Load(tid);
            EmitIfElse((N == f32x3_splat(0.0)).All(), [&] {
                g_output.Store(tid, f32x4_splat(0.01));
                EmitReturn();
            });

            var visibility       = g_visibility_buffer.Load(tid);
            var barys            = visibility.xy().AsF32();
            var instance_idx     = visibility.z();
            var primitive_idx    = visibility.w();
            var grid_size        = g_hash_grid_size.Copy();
            var linear_grid_size = grid_size * length(g_camera_pos - P);
            var xi               = GetNoise(tid) * linear_grid_size;
            var tbn              = GetTBN(N);
            P                    = P + tbn[u32(0)] * xi.x() + tbn[u32(1)] * xi.y();
            grid_size            = grid_size * pow(f32(2.0), floor(log(f32(1.0) + length(g_camera_pos - P))));

            var gp  = (P) / grid_size - f32x3_splat(0.5);
            var igp = (gp).ToI32();
            ifor(3) EmitIfElse(gp[i] < f32(0.0), [&] { igp[i] = igp[i] - i32(1); });
            var frac_rp = frac(gp);

            TRILINEAR_WEIGHTS(frac_rp);

            var color_acc  = Make(f32x3Ty);
            var weight_acc = Make(f32Ty);

            zfor(2) {
                yfor(2) {
                    xfor(2) {
                        var probe_coord = igp + i32x3(x, y, z);

                        var hash = spatial_hash(probe_coord);

                        var item = g_hash_table.Load(hash % g_hash_table_size);

                        EmitIfElse(
                            item["hash"] == hash,
                            [&] {
                                // var color = random_rgb((hash & u32(0xffff)).ToF32());
                                var color = item["v"]["xxx"];
                                color_acc += trilinear_weights[z][y][x] * color;
                                weight_acc += trilinear_weights[z][y][x];
                            },
                            [&] {

                            });
                    }
                }
            }
            color_acc /= max(f32(1.0e-3), weight_acc);
            g_output.Store(tid, make_f32x4(color_acc, f32(1.0)));
        });

        kernel = CompileGlobalModule(gfx, "HashDebug");

        // fprintf(stdout, kernel.isa.c_str());
    }
    void Execute(GfxTexture result) {
        kernel.Begin();
        kernel.SetResource(g_output->resource->GetName().c_str(), result);
        kernel.CheckResources();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.ResetTable();
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
    }
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
    }
};
class Experiment : public ISceneTemplate {
protected:
#define PASS_LIST                                                                                                                                                                  \
    PASS(RelocateHashItems, relocate_hash_grid_pass)                                                                                                                               \
    PASS(AOPass, ao_pass)                                                                                                                                                          \
    PASS(HashDebug, hash_debug)                                                                                                                                                    \
    PASS(EncodeGBuffer, encode_gbuffer)                                                                                                                                            \
    PASS(GBufferFromVisibility, gbuffer_from_vis)

#define PASS(t, n) UniquePtr<t> n = {};
    PASS_LIST
#undef PASS

    u32  frame_idx    = u32(0);
    bool render_gizmo = false;
    bool debug_probe  = false;

    GfxDrawState ddgi_probe_draw_state = {};
    GfxProgram   ddgi_probe_program    = {};
    GfxKernel    ddgi_probe_kernel     = {};

    GfxBuffer hash_tables[2]  = {};
    u32       hash_table_size = u32(1 << 26);
    f32       hash_grid_size  = f32(1.0e-2);

    PingPong ping_pong = {};

    void InitChild() override { ifor(2) hash_tables[i] = gfxCreateBuffer<HashItem>(gfx, hash_table_size); }
    void ResizeChild() override {
        ReleaseChild();

#define PASS(t, n) n.reset(new t(gfx));
        PASS_LIST
#undef PASS

        gfxDrawStateSetColorTarget(ddgi_probe_draw_state, 0, color_buffer);
        gfxDrawStateSetDepthStencilTarget(ddgi_probe_draw_state, depth_buffer);
        gfxDrawStateSetDepthCmpOp(ddgi_probe_draw_state, D3D12_COMPARISON_FUNC_GREATER);
        gfxDrawStateSetInstanceInputSlot(ddgi_probe_draw_state, u32(1));

        ddgi_probe_program = gfxCreateProgram(gfx, "ddgi_probe", shader_path);
        ddgi_probe_kernel  = gfxCreateGraphicsKernel(gfx, ddgi_probe_program, ddgi_probe_draw_state);
        assert(ddgi_probe_program);
        assert(ddgi_probe_kernel);
    }
    void Render() override {
        defer(frame_idx++);
        ping_pong.Next();

        g_global_runtime_resource_registry = {};

        set_global_resource(g_hash_grid_size, hash_grid_size);
        set_global_resource(g_hash_table_size, hash_table_size);
        set_global_resource(g_hash_table, hash_tables[ping_pong.ping]);
        set_global_resource(g_prev_hash_table, hash_tables[ping_pong.pong]);

        set_global_resource(g_frame_idx, frame_idx);
        set_global_resource(g_tlas, gpu_scene.acceleration_structure);
        set_global_resource(g_linear_sampler, linear_sampler);
        set_global_resource(g_nearest_sampler, nearest_sampler);
        set_global_resource(g_velocity, velocity_buffer);
        set_global_resource(g_noise_texture, blue_noise_baker.GetTexture());
        set_global_resource(g_MeshBuffer, gpu_scene.mesh_buffer);
        set_global_resource(g_IndexBuffer, gpu_scene.index_buffer);
        set_global_resource(g_VertexBuffer, gpu_scene.vertex_buffer);
        set_global_resource(g_InstanceBuffer, gpu_scene.instance_buffer);
        set_global_resource(g_MaterialBuffer, gpu_scene.material_buffer);
        set_global_resource(g_TransformBuffer, gpu_scene.transform_buffer);
        set_global_resource(g_PreviousTransformBuffer, gpu_scene.previous_transform_buffer);
        set_global_resource(g_Textures, ResourceSlot(gpu_scene.textures.data(), (uint32_t)gpu_scene.textures.size()));
        set_global_resource(g_visibility_buffer, visibility_buffer);
        set_global_resource(g_camera_pos, g_camera.pos);
        set_global_resource(g_camera_look, g_camera.look);
        set_global_resource(g_camera_up, g_camera.up);
        set_global_resource(g_camera_right, g_camera.right);
        set_global_resource(g_camera_fov, g_camera.fov);
        set_global_resource(g_camera_aspect, g_camera.aspect);
        set_global_resource(g_sun_shadow_matrices, sun.GetMatrixBuffer());
        set_global_resource(g_sun_shadow_maps, ResourceSlot(sun.GetTextures().data(), (uint32_t)sun.GetTextures().size()));
        set_global_resource(g_sun_dir, sun.GetDir());

        relocate_hash_grid_pass->Execute(hash_table_size);

        gbuffer_from_vis->Execute();
        set_global_resource(g_gbuffer_world_normals, gbuffer_from_vis->GetNormals());
        set_global_resource(g_gbuffer_world_position, gbuffer_from_vis->GetWorldPosition());
        set_global_resource(g_prev_gbuffer_world_normals, gbuffer_from_vis->GetPrevNormals());
        set_global_resource(g_prev_gbuffer_world_position, gbuffer_from_vis->GetPrevWorldPosition());

        hash_debug->Execute(color_buffer);

        encode_gbuffer->Execute();

        set_global_resource(g_gbuffer_encoded, encode_gbuffer->GetResult());

        ao_pass->Execute(f32(1.0));

        static bool slow_down = false;
        if (slow_down) Sleep(100);

        ImGui::Begin("Config");
        {
            ImGui::Checkbox("Slow down", &slow_down);
            ImVec2 wsize = GetImGuiSize();
            wsize.y      = wsize.x;

            for (auto &i : g_pass_durations) {
                ImGui::Text("%s %f", i.first.c_str(), i.second);
            }

            ImGui::SliderFloat("hash_grid_size", &hash_grid_size, f32(1.0e-2), f32(1.0));

            ImGui::Text("Normals");
            ImGui::Image((ImTextureID)&gbuffer_from_vis->GetNormals(), wsize);
        }
        ImGui::End();
    }
    GfxTexture GetResult() override { return color_buffer; }
    // GfxTexture GetResult() override { return hash_debug->GetResult(); }
    void ReleaseChild() override {
#define PASS(t, n) n.reset();
        PASS_LIST
#undef PASS
    }
};
} // namespace GfxJit

int main() {
    char const *_working_directory = DGFX_PATH;

    char shader_include_path[0x100];
    char shader_path[0x100];
    char scene_path[0x100];

    sprintf_s(shader_include_path, "%sdgfx", _working_directory);
    sprintf_s(shader_path, "%sshaders", _working_directory);
    sprintf_s(scene_path, "%sscenes\\medieval_weapon_market\\scene.gltf", _working_directory);

    GfxJit::Experiment exp = {};
    exp.Init(scene_path, shader_path, shader_include_path);

    exp.WindowLoop();

    exp.Release();

    return 0;
}
