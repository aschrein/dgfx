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

// https://developer.download.nvidia.com/video/gputechconf/gtc/2020/presentations/s22699-fast-denoising-with-self-stabilizing-recurrent-blurs.pdf
// https://github.com/EmbarkStudios/kajiya/blob/a0eac7d8402b1c808419fd66db7dc46ae6cf7e51/docs/gi-overview.md

static var sample_env(var dir) { return f32x3(0.5, 0.5, 0.5) + f32x3(0.2, 0.4, 0.5) * dir.y(); }

class Raw_GGX_ReflectionsPass {
private:
    GfxContext gfx        = {};
    GPUKernel  kernel     = {};
    GfxTexture radiance   = {};
    GfxTexture ray_length = {};
    GfxTexture confidence = {};
    GfxTexture brdf       = {};
    u32        width      = u32(0);
    u32        height     = u32(0);

    var g_rw_radiance   = ResourceAccess(Resource::Create(RWTexture2D_f32x3_Ty, "g_rw_radiance"));
    var g_rw_ray_length = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_ray_length"));
    var g_rw_confidence = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_confidence"));
    var g_rw_brdf       = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_brdf"));

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return radiance; }
    GfxTexture &GetBRDF() { return brdf; }

    SJIT_DONT_MOVE(Raw_GGX_ReflectionsPass);
    ~Raw_GGX_ReflectionsPass() {
        kernel.Destroy();
        gfxDestroyTexture(gfx, radiance);
        gfxDestroyTexture(gfx, ray_length);
        gfxDestroyTexture(gfx, confidence);
    }
    Raw_GGX_ReflectionsPass(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;
        radiance    = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R11G11B10_FLOAT);
        ray_length  = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16_FLOAT);
        confidence  = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8_UNORM);
        brdf        = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16_FLOAT);
        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

        var dim = u32x2(width, height);

        var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
        EmitIfElse((tid < dim).All(), [&] {
            var xi = GetNoise(tid);
            var N  = g_gbuffer_world_normals.Load(tid);
            var P  = g_gbuffer_world_position.Load(tid);

            var is_bg = g_background.Load(tid) > f32(0.5);

            // EmitIfElse((N == f32x3_splat(0.0)).All(), [&] {
            EmitIfElse(is_bg, [&] {
                g_rw_radiance.Store(tid, f32x3_splat(0.0));
                g_rw_confidence.Store(tid, f32(0.0));
                g_rw_ray_length.Store(tid, f32(0.0));
                g_rw_brdf.Store(tid, f32(0.0));
                EmitReturn();
            });

            var roughness = g_gbuffer_roughness.Load(tid);
            var V         = normalize(P - g_camera_pos);
            var ray       = GGXHelper::SampleReflectionVector(V, N, roughness, xi);
            ray.w()       = max(f32(0.0), min(f32(1.0e3), ray.w()));
            EmitIfElse(
                dot(ray.xyz(), N) > f32(1.0e-3),
                [&] {
                    var ray_desc          = Zero(RayDesc_Ty);
                    ray_desc["Direction"] = ray.xyz();
                    ray_desc["Origin"]    = P + N * f32(1.0e-3);
                    ray_desc["TMin"]      = f32(1.0e-3);
                    ray_desc["TMax"]      = f32(1.0e6);
                    var ray_query         = RayQuery(g_tlas, ray_desc);

                    EmitIfElse(
                        ray_query["hit"],
                        [&] {
                            var hit        = GetHit(ray_query);
                            var w          = hit["W"];
                            var ray_length = length(w - P);
                            var n          = hit["N"];
                            var l          = GetSunShadow(w, n);
                            var c          = random_albedo(ray_query["instance_id"].ToF32());
                            g_rw_radiance.Store(tid, (l["xxx"]) * c);
                            g_rw_confidence.Store(tid, ray_length);
                            g_rw_ray_length.Store(tid, f32(1.0));
                            // g_rw_brdf.Store(tid, f32(1.0) / max(f32(1.0e-3), ray.w()));
                            g_rw_brdf.Store(tid, ray.w());
                        },
                        [&] {
                            g_rw_radiance.Store(tid, sample_env(ray.xyz()));
                            g_rw_confidence.Store(tid, f32(0.0));
                            g_rw_ray_length.Store(tid, f32(0.0));
                            g_rw_brdf.Store(tid, ray.w());
                        });
                },
                [&] {
                    g_rw_radiance.Store(tid, f32x3_splat(0.0));
                    g_rw_confidence.Store(tid, f32(0.0));
                    g_rw_ray_length.Store(tid, f32(0.0));
                    g_rw_brdf.Store(tid, f32(0.0));
                });
        });

        kernel = CompileGlobalModule(gfx, "Raw_GGX_ReflectionsPass");
    }
    void Execute() {
        kernel.SetResource(g_rw_radiance->resource->GetName().c_str(), radiance);
        kernel.SetResource(g_rw_ray_length->resource->GetName().c_str(), ray_length);
        kernel.SetResource(g_rw_confidence->resource->GetName().c_str(), confidence);
        kernel.SetResource(g_rw_brdf, brdf);
        kernel.CheckResources();
        kernel.Begin();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
        kernel.ResetTable();
    }
};
class ReflectionsReprojectPass {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    GfxTexture result = {};
    u32        width  = u32(0);
    u32        height = u32(0);

    var g_rw_result        = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_rw_result"));
    var g_input            = ResourceAccess(Resource::Create(Texture2D_f32x3_Ty, "g_input"));
    var g_input_ray_length = ResourceAccess(Resource::Create(Texture2D_f32_Ty, "g_input_ray_length"));
    var g_input_confidence = ResourceAccess(Resource::Create(Texture2D_f32_Ty, "g_input_confidence"));
    var g_prev_input       = ResourceAccess(Resource::Create(Texture2D_f32x4_Ty, "g_prev_input"));

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return result; }

    SJIT_DONT_MOVE(ReflectionsReprojectPass);
    ~ReflectionsReprojectPass() {
        kernel.Destroy();
        gfxDestroyTexture(gfx, result);
    }
    ReflectionsReprojectPass(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;
        result      = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid        = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var dim        = u32x2(width, height);
            var uv         = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var velocity   = g_velocity.Load(tid);
            var tracked_uv = uv - velocity;
            var cur        = g_input.Load(tid);
            var group_tid  = u32(8) * (tid / u32(8));

            EmitIfElse((tracked_uv > f32x2(0.0, 0.0)).All() && (tracked_uv < f32x2(1.0, 1.0)).All(),
                       [&] {
                           var N = g_gbuffer_world_normals.Load(tid);
                           var P = g_gbuffer_world_position.Load(tid);

                           var scaled_uv  = tracked_uv * dim.ToF32() - f32x2(0.5, 0.5);
                           var frac_uv    = frac(scaled_uv);
                           var uv_lo      = scaled_uv.ToU32();
                           var prev_acc   = Zero(f32x4Ty).Copy();
                           var weight_acc = var(f32(0.0)).Copy();

                           var eps = GetEps(P);

                           var bilinear_weights[2][2] = {
                               {(f32(1.0) - frac_uv.x()) * (f32(1.0) - frac_uv.y()), //
                                (frac_uv.x()) * (f32(1.0) - frac_uv.y())},           //
                               {(f32(1.0) - frac_uv.x()) * (frac_uv.y()),            //
                                (frac_uv.x()) * (frac_uv.y())},                      //
                           };

                           yfor(2) {
                               xfor(2) {
                                   var rN     = g_prev_gbuffer_world_normals.Load(uv_lo + u32x2(x, y));
                                   var rP     = g_prev_gbuffer_world_position.Load(uv_lo + u32x2(x, y));
                                   var w      = GetWeight(N, P, rN, rP, eps);
                                   var weight = bilinear_weights[y][x] * w;
                                   EmitIfElse(w > f32(0.8), [&] {
                                       prev_acc += weight * g_prev_input.Load(uv_lo + u32x2(x, y));
                                       weight_acc += weight;
                                   });
                               }
                           }

                           var prev            = prev_acc / max(f32(1.0e-5), weight_acc);
                           var num_samples     = prev.w();
                           var new_num_samples = max(f32(64.0), num_samples + f32(1.0));
                           var history_weight  = f32(1.0) - f32(1.0) / new_num_samples;
                           var mix             = lerp(cur, prev, history_weight);
                           g_rw_result.Store(tid, make_f32x4(mix.xyz(), new_num_samples));
                       },
                       [&] { g_rw_result.Store(tid, make_f32x4(cur.xyz(), f32(1.0))); });

            kernel = CompileGlobalModule(gfx, "ReflectionsReprojectPass");
        }
    }
    void Execute(                    //
        GfxTexture input,            //
        GfxTexture input_ray_length, //
        GfxTexture confidence,       //
        GfxTexture prev_input        //
    ) {

        kernel.SetResource(g_rw_result, result);
        kernel.SetResource(g_input, input);
        kernel.SetResource(g_input_ray_length, input_ray_length);
        kernel.SetResource(g_input_confidence, confidence);
        kernel.SetResource(g_prev_input, prev_input);

        kernel.CheckResources();
        kernel.Begin();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
        kernel.ResetTable();
    }
};
class SpatialFilter {
private:
    GfxContext gfx        = {};
    GPUKernel  kernel     = {};
    GfxTexture results[2] = {};
    u32        width      = u32(0);
    u32        height     = u32(0);
    PingPong   ping_pong  = {};

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return results[ping_pong.ping]; }
    GfxTexture &GetPrevResult() { return results[ping_pong.pong]; }

    SJIT_DONT_MOVE(SpatialFilter);
    ~SpatialFilter() {
        kernel.Destroy();
        ifor(2) gfxDestroyTexture(gfx, results[i]);
    }
    SpatialFilter(GfxContext _gfx) {
        u32 _width         = gfxGetBackBufferWidth(_gfx);
        u32 _height        = gfxGetBackBufferHeight(_gfx);
        gfx                = _gfx;
        width              = _width;
        height             = _height;
        ifor(2) results[i] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];

            var g_rw_result = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_rw_result"));
            var g_input     = ResourceAccess(Resource::Create(RWTexture2D_f32x3_Ty, "g_input"));
            var g_brdf      = ResourceAccess(Resource::Create(Texture2D_f32_Ty, "g_brdf"));
            var dim         = g_rw_result.GetDimensions().Swizzle("xy");

            var  gid        = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];
            var  lds        = AllocateLDS(u32x3Ty, u32(16 * 16), "lds_values");
            var  gid_center = gid.xy() + u32x2(4, 4);
            auto linear_idx = [](var xy) { return (xy.x().ToI32() + xy.y().ToI32() * i32(16)).ToU32(); };
            var  group_tid  = u32(8) * (tid / u32(8));

            Init_LDS_16x16(lds, [&](var src_coord) {
                var in          = g_input.Load(src_coord);
                var val         = Zero(u32x3Ty).Copy();
                var gbuffer_val = g_gbuffer_encoded.Load(src_coord);
                val.x()         = gbuffer_val;
                var pack_rg     = PackFp16x2ToU32(in.xy().ToF16());
                var brdf        = g_brdf.Load(src_coord);
                var pack_ba     = PackFp16x2ToU32(make_f32x2(in.z(), brdf).ToF16());
                val.y()         = pack_rg.AsU32();
                val.z()         = pack_ba.AsU32();
                return val;
            });

            auto lds_to_rgba = [=](var l) {
                var result  = Make(f32x4Ty);
                result.xy() = UnpackU32ToF16x2(l.y()).ToF32();
                result.zw() = UnpackU32ToF16x2(l.z()).ToF32();
                return result;
            };

            EmitGroupSync();

            var is_bg = g_background.Load(tid) > f32(0.5);

            EmitIfElse(
                !is_bg,
                [&] {
                    var roughness = g_gbuffer_roughness.Load(tid);
                    var l         = lds.Load(linear_idx(gid_center));
                    var src_value = lds_to_rgba(l);
                    // var src_num_samples = f32(1.0); // src_value.w();
                    var uv  = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                    var ray = GenCameraRay(uv);

                    var xi                    = GetNoise(tid);
                    var center_gbuffer        = DecodeGBuffer32Bits(ray, l.x(), xi.x());
                    var eps                   = GetEps(center_gbuffer["P"]);
                    var halton_sample_offsets = MakeStaticArray(halton_samples);

                    var value_acc  = src_value.Copy();
                    var weigth_acc = src_value.w().Copy();
                    value_acc *= weigth_acc;
                    var gamma = pow(f32(1.0) - roughness, f32(2.0));
                    EmitForLoop(i32(0), i32(halton_sample_count), [&](var iter) {
                        var soffset = halton_sample_offsets[iter];
                        EmitIfElse((g_frame_idx & u32(1)) != u32(0), [&] { soffset.xy() = soffset.yx(); });

                        var l     = lds.Load(linear_idx(gid_center.ToI32() + soffset));
                        var value = lds_to_rgba(l);

                        var uv      = (tid.ToF32() + halton_sample_offsets[iter].ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                        var ray     = GenCameraRay(uv);
                        var xi      = GetNoise(tid);
                        var gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());

                        var weight = GetWeight(center_gbuffer["N"], center_gbuffer["P"], gbuffer["N"], gbuffer["P"], eps);
                        weight *= Gaussian(length(soffset.ToF32()) * gamma);
                        weight *= value.w();
                        weight *= exp(-GetLuminance(value.xyz()));
                        // l.z().AsF32() *
                        value_acc += weight * value;
                        weigth_acc += weight;
                    });
                    value_acc /= weigth_acc;

                    var dst             = value_acc.xyz();
                    var dst_num_samples = value_acc.w();

                    // var final = (src_value["xyz"] * src_num_samples + dst["xyz"] * dst_num_samples) / max(f32(1.0e-3), src_num_samples + dst_num_samples);

                    // g_rw_result.Store(tid, make_f32x4(final["xyz"], dst_num_samples));
                    g_rw_result.Store(tid, value_acc);
                },
                [&] { g_rw_result.Store(tid, f32x4_splat(0.0)); });
            kernel = CompileGlobalModule(gfx, "SpatialFilter");
        }
    }
    void Execute(GfxTexture input, GfxTexture brdf) {
        ping_pong.Next();
        kernel.SetResource("g_rw_result", results[ping_pong.ping]);
        kernel.SetResource("g_input", input);
        kernel.SetResource("g_brdf", brdf);
        kernel.CheckResources();
        kernel.Begin();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
        kernel.ResetTable();
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
    PASS(EncodeGBuffer, encode_gbuffer)                                                                                                                                            \
    PASS(EdgeDetect, edge_detect)                                                                                                                                                  \
    PASS(SpatialFilter, spatial_filter)                                                                                                                                            \
    PASS(GBufferFromVisibility, gbuffer_from_vis)                                                                                                                                  \
    PASS(PrimaryRays, primary_rays)                                                                                                                                                \
    PASS(NearestVelocity, nearest_velocity)                                                                                                                                        \
    PASS(Raw_GGX_ReflectionsPass, reflections)                                                                                                                                     \
    PASS(ReflectionsReprojectPass, reflections_reproject)

#define PASS(t, n) UniquePtr<t> n = {};
    PASS_LIST
#undef PASS

    u32  frame_idx    = u32(0);
    bool render_gizmo = false;
    bool debug_probe  = false;

    f32 global_roughness = f32(0.1);

    void InitChild() override {}
    void ResizeChild() override {
        ReleaseChild();

#define PASS(t, n) n.reset(new t(gfx));
        PASS_LIST
#undef PASS
    }
    void Render() override {
        defer(frame_idx++);

        g_global_runtime_resource_registry = {};
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

        gbuffer_from_vis->SetGlobalRoughness(global_roughness);
        gbuffer_from_vis->Execute();
        set_global_resource(g_gbuffer_roughness, gbuffer_from_vis->GetRoughness());
        set_global_resource(g_prev_gbuffer_roughness, gbuffer_from_vis->GetPrevRoughness());
        g_global_runtime_resource_registry[g_gbuffer_world_normals->GetResource()->GetName()]       = gbuffer_from_vis->GetNormals();
        g_global_runtime_resource_registry[g_gbuffer_world_position->GetResource()->GetName()]      = gbuffer_from_vis->GetWorldPosition();
        g_global_runtime_resource_registry[g_prev_gbuffer_world_normals->GetResource()->GetName()]  = gbuffer_from_vis->GetPrevNormals();
        g_global_runtime_resource_registry[g_prev_gbuffer_world_position->GetResource()->GetName()] = gbuffer_from_vis->GetPrevWorldPosition();

        encode_gbuffer->Execute();
        set_global_resource(g_gbuffer_encoded, encode_gbuffer->GetResult());
        set_global_resource(g_background, encode_gbuffer->GetBackground());

        edge_detect->Execute();
        set_global_resource(g_edges, edge_detect->GetResult());

        nearest_velocity->Execute();
        primary_rays->Execute();

        g_global_runtime_resource_registry[g_nearest_velocity->GetResource()->GetName()] = nearest_velocity->GetResult();

        reflections->Execute();
        spatial_filter->Execute(reflections->GetResult(), reflections->GetBRDF());
        // reflections_reproject->Execute(reflections->GetResult());

        static bool slow_down = false;
        if (slow_down) Sleep(100);
        ImGui::Begin("Reflections");
        {
            ImVec2 wsize = GetImGuiSize();
            wsize.y      = wsize.x;

            ImGui::Text("raw reflections");
            ImGui::Image((ImTextureID)&reflections->GetResult(), wsize);
            ImGui::Text("brdf");
            ImGui::Image((ImTextureID)&reflections->GetBRDF(), wsize);
            ImGui::Text("edge_detect");
            ImGui::Image((ImTextureID)&edge_detect->GetResult(), wsize);
            ImGui::Text("background");
            ImGui::Image((ImTextureID)&encode_gbuffer->GetBackground(), wsize);
        }
        ImGui::End();

        ImGui::Begin("Config");
        {
            ImGui::SliderFloat("global_roughness", &global_roughness, f32(0.0), f32(1.0));
            for (auto &i : g_pass_durations) {
                ImGui::Text("%s %f", i.first.c_str(), i.second);
            }
            ImGui::Checkbox("Slow down", &slow_down);
            ImGui::Checkbox("Render Gizmo", &render_gizmo);
            ImGui::Checkbox("Debug Probe", &debug_probe);
            ImVec2 wsize = GetImGuiSize();
            wsize.y      = wsize.x;
        }
        ImGui::End();
    }
    // GfxTexture GetResult() override { return color_buffer; }
    GfxTexture GetResult() override { return spatial_filter->GetResult(); }
    void       ReleaseChild() override {
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
