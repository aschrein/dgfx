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

static var sample_env(var dir) { return lerp(f32x3(17, 13, 140) / f32(255.0), f32x3(95, 190, 245) / f32(255.0), f32(0.5) * dir.y() + f32(0.5)); }
namespace specular {

static var get_checkerboard_offset(var pixel_coord) {
    var _pcg          = pcg(pixel_coord.x() + pcg(pixel_coord.y() + pcg(g_frame_idx)));
    var sample_offset = Make(u32x2Ty);
    sample_offset.x() = (_pcg >> u32(0)) & u32(1);
    sample_offset.y() = (_pcg >> u32(1)) & u32(1);
    return sample_offset;
}
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_roughness_grid_size, f32Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_global_roughness, f32Ty);

class ProceduralRoughness {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    u32        width  = u32(0);
    u32        height = u32(0);

#define TEXTURE_LIST                                                                                                                                                               \
    TEXTURE(Roughness, DXGI_FORMAT_R8_UNORM, f32, width, height)                                                                                                                   \
    TEXTURE(PrevRoughness, DXGI_FORMAT_R8_UNORM, f32, width, height)

#define TEXTURE(_name, _fmt, _ty, _width, _height) GfxTexture _name = {};
    TEXTURE_LIST
#undef TEXTURE

public:
#define TEXTURE(_name, _fmt, _ty, _width, _height)                                                                                                                                 \
    static var g_rw_##_name() { return ResourceAccess(Resource::Create(RWTexture2D_##_ty##_Ty, "g_" #_name)); }                                                                    \
    static var g_##_name() { return ResourceAccess(Resource::Create(Texture2D_##_ty##_Ty, "g_" #_name)); }

    TEXTURE_LIST
#undef TEXTURE

    u32 GetWidth() { return width; }
    u32 GetHeight() { return height; }

#define TEXTURE(_name, _fmt, _ty, _width, _height)                                                                                                                                 \
    GfxTexture &Get##_name() { return _name; }
    TEXTURE_LIST
#undef TEXTURE

    SJIT_DONT_MOVE(ProceduralRoughness);
    ~ProceduralRoughness() {
        kernel.Destroy();
#define TEXTURE(_name, _fmt, _ty, _width, _height) gfxDestroyTexture(gfx, _name);
        TEXTURE_LIST
#undef TEXTURE
    }
    ProceduralRoughness(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;

#define TEXTURE(_name, _fmt, _ty, _width, _height) _name = gfxCreateTexture2D(gfx, (_width), (_height), (_fmt));
        TEXTURE_LIST
#undef TEXTURE

        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

        var dim = u32x2(width, height);

        var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];

        var roughness = g_global_roughness.Copy();
        var P         = g_gbuffer_world_position.Load(tid);
        var cw        = (P / g_scene_size * g_roughness_grid_size);
        cw            = cw + sin(cw["yzx"]);
        var icw       = cw.ToI32();
        ifor(3) EmitIfElse(P[i] < f32(0.0), [&] { icw[i] = icw[i] - i32(1); });
        var ucw   = abs(icw).AsU32();
        var b_x   = ucw.x() & u32(1);
        var b_y   = ucw.y() & u32(1);
        var b_z   = ucw.z() & u32(1);
        var b     = (b_x ^ b_y) ^ b_z;
        roughness = roughness * b.ToF32(); // * (f32(1.0) - length(frac(cw) - f32x3_splat(0.5)) * f32(2.0));

        g_rw_Roughness().Store(tid, roughness);

        kernel = CompileGlobalModule(gfx, "ProceduralRoughness");
    }
    void Execute() {
        std::swap(PrevRoughness, Roughness);

#define TEXTURE(_name, _fmt, _ty, _width, _height) kernel.SetResource(g_rw_##_name(), _name);
        TEXTURE_LIST
#undef TEXTURE

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

#undef TEXTURE_LIST
};

class Raw_GGX_Gen {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    u32        width  = u32(0);
    u32        height = u32(0);

#define TEXTURE_LIST                                                                                                                                                               \
    TEXTURE(NormalBRDF, DXGI_FORMAT_R32_UINT, u32, width, height)                                                                                                                  \
    TEXTURE(PackedGbuffer, DXGI_FORMAT_R32_UINT, u32, width, height)                                                                                                               \
    TEXTURE(Roughness, DXGI_FORMAT_R8_UNORM, f32, width, height)

#define TEXTURE(_name, _fmt, _ty, _width, _height) GfxTexture _name = {};
    TEXTURE_LIST
#undef TEXTURE

public:
#define TEXTURE(_name, _fmt, _ty, _width, _height)                                                                                                                                 \
    static var g_rw_##_name() { return ResourceAccess(Resource::Create(RWTexture2D_##_ty##_Ty, "g_" #_name)); }                                                                    \
    static var g_##_name() { return ResourceAccess(Resource::Create(Texture2D_##_ty##_Ty, "g_" #_name)); }

    TEXTURE_LIST
#undef TEXTURE

    u32 GetWidth() { return width; }
    u32 GetHeight() { return height; }

#define TEXTURE(_name, _fmt, _ty, _width, _height)                                                                                                                                 \
    GfxTexture &Get##_name() { return _name; }
    TEXTURE_LIST
#undef TEXTURE

    SJIT_DONT_MOVE(Raw_GGX_Gen);
    ~Raw_GGX_Gen() {
        kernel.Destroy();
#define TEXTURE(_name, _fmt, _ty, _width, _height) gfxDestroyTexture(gfx, _name);
        TEXTURE_LIST
#undef TEXTURE
    }
    Raw_GGX_Gen(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width / 2;
        height      = _height / 2;

#define TEXTURE(_name, _fmt, _ty, _width, _height) _name = gfxCreateTexture2D(gfx, (_width), (_height), (_fmt));
        TEXTURE_LIST
#undef TEXTURE

        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

        var dim = u32x2(width, height);

        var tid     = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
        var src_tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"] * u32(2) + get_checkerboard_offset(tid);
        EmitIfElse((tid < dim).All(), [&] {
            var  xi             = GetNoise(tid);
            var  N              = g_gbuffer_world_normals.Load(src_tid);
            var  P              = g_gbuffer_world_position.Load(src_tid);
            var  packed_gbuffer = g_gbuffer_encoded.Load(src_tid);
            var  is_bg          = g_background.Load(src_tid) > f32(0.5);
            auto earlyOut       = [&] {
                g_rw_NormalBRDF().Store(tid, u32(0));
                g_rw_PackedGbuffer().Store(tid, u32(0));
                g_rw_Roughness().Store(tid, f32(0.0));
                EmitReturn();
            };
            EmitIfElse(is_bg, [&] { earlyOut(); });
            var roughness = g_gbuffer_roughness.Load(src_tid);
            var V         = normalize(P - g_camera_pos);
            var attempts  = var(i32(4)).Copy();
            EmitWhileLoop([&] {
                EmitIfElse(attempts < i32(0), [&] { EmitReturn(); });
                attempts       = attempts - i32(1);
                var normal_pdf = SJIT::GGXHelper::SampleNormal(V, N, roughness, xi);

                var ray_dir = reflect(V, normal_pdf.xyz());

                EmitIfElse(dot(ray_dir.xyz(), N) > f32(1.0e-3), [&] {
                    normal_pdf.w() = max(f32(1.0e-3), min(f32(1.0e3), normal_pdf.w()));
                    var pack       = Make(u32Ty);
                    pack           = pack | SJIT::Octahedral::EncodeNormalTo16Bits(normal_pdf.xyz());
                    pack           = pack | (normal_pdf.w().ToF16().f16_to_u32() << u32(16));
                    g_rw_NormalBRDF().Store(tid, pack);
                    g_rw_PackedGbuffer().Store(tid, packed_gbuffer);
                    g_rw_Roughness().Store(tid, roughness);
                    EmitReturn();
                });

                xi = frac(xi + f32x2_splat(GOLDEN_RATIO));
            });
            earlyOut();
        });

        kernel = CompileGlobalModule(gfx, "Raw_GGX_Gen");
    }
    void Execute() {
#define TEXTURE(_name, _fmt, _ty, _width, _height) kernel.SetResource(g_rw_##_name(), _name);
        TEXTURE_LIST
#undef TEXTURE

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

#undef TEXTURE_LIST
};
static var GetAlbedo(var P, var instance_id) {
    var c   = random_albedo(instance_id.ToF32());
    var cw  = (P / g_scene_size * g_roughness_grid_size * f32(2.0));
    cw      = cw; // + sin(cw["yzx"]);
    var icw = cw.ToI32();
    ifor(3) EmitIfElse(P[i] < f32(0.0), [&] { icw[i] = icw[i] - i32(1); });
    var ucw = abs(icw).AsU32();
    var b_x = ucw.x() & u32(1);
    var b_y = ucw.y() & u32(1);
    var b_z = ucw.z() & u32(1);
    var b   = (b_x ^ b_y) ^ b_z;
    c       = c * (f32(0.1) + f32(0.9) * b.ToF32());
    return c;
}
class Raw_GGX_ReflectionsPass {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    u32        width  = u32(0);
    u32        height = u32(0);

#define TEXTURE_LIST                                                                                                                                                               \
    TEXTURE(IndirectWorldPosition, DXGI_FORMAT_R32G32B32A32_FLOAT, f32x3, width, height)                                                                                           \
    TEXTURE(IndirectAlbedo, DXGI_FORMAT_R8G8B8A8_UNORM, f32x3, width, height)                                                                                                      \
    TEXTURE(IndirectNormalBRDF, DXGI_FORMAT_R32G32B32A32_FLOAT, f32x4, width, height)

#define TEXTURE(_name, _fmt, _ty, _width, _height) GfxTexture _name = {};
    TEXTURE_LIST
#undef TEXTURE

public:
#define TEXTURE(_name, _fmt, _ty, _width, _height)                                                                                                                                 \
    static var g_rw_##_name() { return ResourceAccess(Resource::Create(RWTexture2D_##_ty##_Ty, "g_" #_name)); }                                                                    \
    static var g_##_name() { return ResourceAccess(Resource::Create(Texture2D_##_ty##_Ty, "g_" #_name)); }

    TEXTURE_LIST
#undef TEXTURE

    u32 GetWidth() { return width; }
    u32 GetHeight() { return height; }

#define TEXTURE(_name, _fmt, _ty, _width, _height)                                                                                                                                 \
    GfxTexture &Get##_name() { return _name; }
    TEXTURE_LIST
#undef TEXTURE

    SJIT_DONT_MOVE(Raw_GGX_ReflectionsPass);
    ~Raw_GGX_ReflectionsPass() {
        kernel.Destroy();
#define TEXTURE(_name, _fmt, _ty, _width, _height) gfxDestroyTexture(gfx, _name);
        TEXTURE_LIST
#undef TEXTURE
    }
    Raw_GGX_ReflectionsPass(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width / 2;
        height      = _height / 2;

#define TEXTURE(_name, _fmt, _ty, _width, _height) _name = gfxCreateTexture2D(gfx, (_width), (_height), (_fmt));
        TEXTURE_LIST
#undef TEXTURE

        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

        var dim = u32x2(width, height);

        var tid     = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
        var src_tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"] * u32(2) + get_checkerboard_offset(tid);
        EmitIfElse((tid < dim).All(), [&] {
            var xi = GetNoise(tid);
            var N  = g_gbuffer_world_normals.Load(src_tid);
            var P  = g_gbuffer_world_position.Load(src_tid);

            var  is_bg    = g_background.Load(src_tid) > f32(0.5);
            auto earlyOut = [&] {
                g_rw_IndirectAlbedo().Store(tid, f32x3_splat(0.0));
                g_rw_IndirectNormalBRDF().Store(tid, f32x4_splat(0.0));
                g_rw_IndirectWorldPosition().Store(tid, f32x3_splat(0.0));
                EmitReturn();
            };
            EmitIfElse(is_bg, [&] { earlyOut(); });
            // var roughness       = g_gbuffer_roughness.Load(src_tid);
            var pack_normal_pdf = Raw_GGX_Gen::g_NormalBRDF().Load(tid);
            var normal          = SJIT::Octahedral::DecodeNormalFrom16Bits(pack_normal_pdf & u32(0xffff));
            var pdf             = ((pack_normal_pdf >> u32(16)) & u32(0xffff)).u32_to_f16().ToF32();
            var V               = -normalize(P - g_camera_pos);
            var ray_dir         = reflect(-V, normal.xyz());

            EmitIfElse(
                dot(ray_dir.xyz(), N) > f32(1.0e-3),
                [&] {
                    var ray_desc          = Zero(RayDesc_Ty);
                    ray_desc["Direction"] = ray_dir.xyz();
                    ray_desc["Origin"]    = P + N * f32(1.0e-3);
                    ray_desc["TMin"]      = f32(1.0e-3);
                    ray_desc["TMax"]      = f32(1.0e6);
                    // var ray_query         = RayQuery(g_tlas, ray_desc);

                    var ray_query = RayQueryTransparent(g_tlas, ray_desc);

                    EmitIfElse(
                        ray_query["hit"],
                        [&] {
                            var hit = GetHit(ray_query);
                            var w   = hit["W"];
                            var n   = hit["N"];

                            var instance          = g_InstanceBuffer[ray_query["instance_id"]];
                            var mesh              = g_MeshBuffer[instance["mesh_id"]];
                            var material          = g_MaterialBuffer[mesh["material_id"]];
                            var albedo            = material["albedo"];
                            var albedo_texture_id = albedo.w().AsU32();
                            albedo.w()            = f32(1.0);
                            EmitIfElse(albedo_texture_id != u32(0Xffffffff), [&] {
                                var tex_albedo = g_Textures[albedo_texture_id.NonUniform()].Sample(g_linear_sampler, hit["UV"]);
                                albedo *= tex_albedo;
                            });

                            // var c = GetAlbedo(w, ray_query["instance_id"]);

                            g_rw_IndirectAlbedo().Store(tid, albedo.xyz());
                            g_rw_IndirectNormalBRDF().Store(tid, make_f32x4(n, pdf.x()));
                            g_rw_IndirectWorldPosition().Store(tid, w);
                        },
                        [&] { earlyOut(); });
                },
                [&] {
                    g_rw_IndirectAlbedo().Store(tid, f32x3_splat(0.0));
                    g_rw_IndirectNormalBRDF().Store(tid, f32x4_splat(0.0));
                    g_rw_IndirectWorldPosition().Store(tid, f32x3_splat(0.0));
                });
        });

        kernel = CompileGlobalModule(gfx, "Raw_GGX_ReflectionsPass");
    }
    void Execute(Raw_GGX_Gen *gen) {
#define TEXTURE(_name, _fmt, _ty, _width, _height) kernel.SetResource(g_rw_##_name(), _name);
        TEXTURE_LIST
#undef TEXTURE
        kernel.SetResource(Raw_GGX_Gen::g_NormalBRDF(), gen->GetNormalBRDF());
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

#undef TEXTURE_LIST
};
class ShadeReflectionsPass {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    u32        width  = u32(0);
    u32        height = u32(0);

#define TEXTURE_LIST TEXTURE(IndirectShade, DXGI_FORMAT_R11G11B10_FLOAT, f32x3, width, height)

#define TEXTURE(_name, _fmt, _ty, _width, _height) GfxTexture _name = {};
    TEXTURE_LIST
#undef TEXTURE

public:
#define TEXTURE(_name, _fmt, _ty, _width, _height)                                                                                                                                 \
    static var g_rw_##_name() { return ResourceAccess(Resource::Create(RWTexture2D_##_ty##_Ty, "g_" #_name)); }                                                                    \
    static var g_##_name() { return ResourceAccess(Resource::Create(Texture2D_##_ty##_Ty, "g_" #_name)); }

    TEXTURE_LIST
#undef TEXTURE

    u32 GetWidth() { return width; }
    u32 GetHeight() { return height; }

#define TEXTURE(_name, _fmt, _ty, _width, _height)                                                                                                                                 \
    GfxTexture &Get##_name() { return _name; }
    TEXTURE_LIST
#undef TEXTURE

    SJIT_DONT_MOVE(ShadeReflectionsPass);
    ~ShadeReflectionsPass() {
        kernel.Destroy();
#define TEXTURE(_name, _fmt, _ty, _width, _height) gfxDestroyTexture(gfx, _name);
        TEXTURE_LIST
#undef TEXTURE
    }
    ShadeReflectionsPass(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width / 2;
        height      = _height / 2;

#define TEXTURE(_name, _fmt, _ty, _width, _height) _name = gfxCreateTexture2D(gfx, (_width), (_height), (_fmt));
        TEXTURE_LIST
#undef TEXTURE

        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

        var dim = u32x2(width, height);
        var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
        EmitIfElse((tid < dim).All(), [&] {
            var N_brdf  = Raw_GGX_ReflectionsPass::g_IndirectNormalBRDF().Load(tid);
            var P       = Raw_GGX_ReflectionsPass::g_IndirectWorldPosition().Load(tid);
            var src_tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"] * u32(2) + get_checkerboard_offset(tid);
            var is_bg   = g_background.Load(src_tid) > f32(0.5);
            var wP      = g_gbuffer_world_position.Load(src_tid);
            var wN      = g_gbuffer_world_normals.Load(src_tid);
            EmitIfElse(
                is_bg, [&] { g_rw_IndirectShade().Store(tid, f32x3(0.0, 0.0, 0.0)); },
                [&] {
                    EmitIfElse((P.AsU32() == u32x3_splat(0)).All(),
                               [&] {
                                   var V       = -normalize(wP - g_camera_pos);
                                   var ray_dir = reflect(-V, wN);
                                   g_rw_IndirectShade().Store(tid, sample_env(ray_dir));
                               },
                               [&] {
                                   var A       = Raw_GGX_ReflectionsPass::g_IndirectAlbedo().Load(tid);
                                   var l       = GetSunShadow(P, N_brdf.xyz());
                                   var c       = A.xyz();
                                   var ambient = f32x3(0.1, 0.12, 0.2) / f32(8.0) * sample_env(wN);

                                   g_rw_IndirectShade().Store(tid, c * (l["xxx"] + ambient));
                               });
                });
        });

        kernel = CompileGlobalModule(gfx, "Raw_GGX_ReflectionsPass");
    }
    void Execute(Raw_GGX_ReflectionsPass *reflection_pass) {
        kernel.SetResource(Raw_GGX_ReflectionsPass::g_IndirectWorldPosition(), reflection_pass->GetIndirectWorldPosition());
        kernel.SetResource(Raw_GGX_ReflectionsPass::g_IndirectNormalBRDF(), reflection_pass->GetIndirectNormalBRDF());
        kernel.SetResource(Raw_GGX_ReflectionsPass::g_IndirectAlbedo(), reflection_pass->GetIndirectAlbedo());

#define TEXTURE(_name, _fmt, _ty, _width, _height) kernel.SetResource(g_rw_##_name(), _name);
        TEXTURE_LIST
#undef TEXTURE

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

#undef TEXTURE_LIST
};
class SpatialFilter {
private:
    GfxContext gfx      = {};
    GPUKernel  kernel   = {};
    GfxTexture result   = {};
    GfxTexture variance = {};
    // GfxTexture history_uv = {};
    u32      width     = u32(0);
    u32      height    = u32(0);
    PingPong ping_pong = {};

#define TEXTURE_LIST                                                                                                                                                               \
    TEXTURE(Result, DXGI_FORMAT_R16G16B16A16_FLOAT, f32x3, width, height, 1, 1)                                                                                                    \
    TEXTURE(Variance, DXGI_FORMAT_R16G16B16A16_FLOAT, f32x3, width, height, 1, 1)

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) GfxTexture _name = {};
    TEXTURE_LIST
#undef TEXTURE

public:
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    static var g_rw_##_name() {                                                                                                                                                    \
        if (_depth == u32(1))                                                                                                                                                      \
            return ResourceAccess(Resource::Create(RWTexture2D_##_ty##_Ty, "g_" #_name));                                                                                          \
        else                                                                                                                                                                       \
            return ResourceAccess(Resource::Create(RWTexture3D_##_ty##_Ty, "g_" #_name));                                                                                          \
    }                                                                                                                                                                              \
    static var g_##_name() {                                                                                                                                                       \
        if (_depth == u32(1))                                                                                                                                                      \
            return ResourceAccess(Resource::Create(Texture2D_##_ty##_Ty, "g_" #_name));                                                                                            \
        else                                                                                                                                                                       \
            return ResourceAccess(Resource::Create(Texture3D_##_ty##_Ty, "g_" #_name));                                                                                            \
    }

    TEXTURE_LIST
#undef TEXTURE

    u32 GetWidth() { return width; }
    u32 GetHeight() { return height; }

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    GfxTexture &Get##_name() { return _name; }
    TEXTURE_LIST
#undef TEXTURE

    SJIT_DONT_MOVE(SpatialFilter);
    ~SpatialFilter() {
        kernel.Destroy();
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) gfxDestroyTexture(gfx, _name);
        TEXTURE_LIST
#undef TEXTURE
    }
    SpatialFilter(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width / 2;
        height      = _height / 2;

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    {                                                                                                                                                                              \
        sjit_assert((_width) >= u32(1));                                                                                                                                           \
        sjit_assert((_height) >= u32(1));                                                                                                                                          \
        sjit_assert((_depth) >= u32(1));                                                                                                                                           \
        sjit_assert((_mips) >= u32(1));                                                                                                                                            \
        if (_depth == u32(1))                                                                                                                                                      \
            _name = gfxCreateTexture2D(gfx, (_width), (_height), (_fmt), (_mips));                                                                                                 \
        else                                                                                                                                                                       \
            _name = gfxCreateTexture3D(gfx, (_width), (_height), (_depth), (_fmt), (_mips));                                                                                       \
    }
        TEXTURE_LIST
#undef TEXTURE
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];

            var dim = g_rw_Result().GetDimensions().Swizzle("xy");

            var  gid        = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];
            var  lds        = AllocateLDS(u32x4Ty, u32(16 * 16), "lds_values");
            var  gid_center = gid.xy() + u32x2(4, 4);
            auto linear_idx = [](var xy) { return (xy.x().ToI32() + xy.y().ToI32() * i32(16)).ToU32(); };
            var  group_tid  = u32(8) * (tid / u32(8));

            Init_LDS_16x16(lds, [&](var src_coord) {
                var in = ShadeReflectionsPass::g_IndirectShade().Load(src_coord);
                // var history_uv  = g_history_uv.Load(src_coord);
                var val         = Zero(u32x4Ty).Copy();
                var gbuffer_val = Raw_GGX_Gen::g_PackedGbuffer().Load(src_coord);
                val.x()         = gbuffer_val;
                var pack_rg     = PackFp16x2ToU32(in.xy().ToF16());
                var brdf        = ((Raw_GGX_Gen::g_NormalBRDF().Load(src_coord) >> u32(16)) & u32(0xffff)).u32_to_f16().ToF32();
                var pack_ba     = PackFp16x2ToU32(make_f32x2(in.z(), brdf).ToF16());
                val.y()         = pack_rg.AsU32();
                val.z()         = pack_ba.AsU32();
                // val.w()         = PackFp16x2ToU32(history_uv.xy());
                return val;
            });

            auto lds_to_rgba = [&](var l) {
                var result  = Make(f32x4Ty);
                result.xy() = UnpackU32ToF16x2(l.y()).ToF32();
                result.zw() = UnpackU32ToF16x2(l.z()).ToF32();
                return result;
            };
            /*auto lds_to_uv = [&](var l) {
                var result  = Make(f32x2Ty);
                result.xy() = UnpackU32ToF16x2(l.w()).ToF32();
                return result;
            };*/

            EmitGroupSync();

            // var is_bg = g_background.Load(tid) > f32(0.5);

            EmitIfElse(
                //! is_bg,
                true,
                [&] {
                    var roughness = Raw_GGX_Gen::g_Roughness().Load(tid);
                    var l         = lds.Load(linear_idx(gid_center));
                    var src_value = lds_to_rgba(l);
                    // var src_history_uv = lds_to_uv(l);
                    //  var src_num_samples = f32(1.0); // src_value.w();
                    var uv  = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                    var ray = GenCameraRay(uv);

                    var xi             = GetNoise(tid);
                    var center_gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());
                    var eps            = GetEps(center_gbuffer["P"]);
                    //

                    var value_acc   = src_value.xyz().Copy();
                    var value_2_acc = value_acc * value_acc;
                    // var history_uv_acc = src_history_uv.Copy();
                    var weigth_acc = var(f32(1.0)).Copy();
                    // var weigth_acc = src_value.w().Copy();
                    // weigth_acc *= exp(-GetLuminance(src_value.xyz()));
                    //  var uv_weight_acc  = var(f32(1.0)).Copy();
                    value_acc *= weigth_acc;
                    value_2_acc *= weigth_acc;
                    // history_uv_acc *= uv_weight_acc;
                    var gamma = pow(f32(1.0) - roughness, f32(8.0)) * f32(2.0);

                // #define HALTON

#if defined(HALTON)
                    var halton_sample_offsets = MakeStaticArray(halton_samples);
#endif // defined(HALTON)

#if defined(HALTON)
                    EmitForLoop(i32(0), i32(halton_sample_count), [&](var iter) {
                        var soffset = halton_sample_offsets[iter];
                        EmitIfElse((g_frame_idx & u32(1)) != u32(0), [&] { soffset.xy() = soffset.yx(); });
#else  // !defined(HALTON)
                    EmitForLoop(i32(-4), i32(4), [&](var _y) {
                        EmitForLoop(i32(-4), i32(4), [&](var _x) {
                            EmitIfElse(_x == i32(0) && _y == i32(0), [&] { EmitContinue(); });

                            var soffset = Make(i32x2Ty);
                            soffset.x() = _x;
                            soffset.y() = _y;
#endif // !defined(HALTON)

                        var l     = lds.Load(linear_idx(gid_center.ToI32() + soffset));
                        var value = lds_to_rgba(l);
                        // var history_uv = lds_to_uv(l);
                        var uv      = (tid.ToF32() + soffset.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                        var ray     = GenCameraRay(uv);
                        var xi      = GetNoise(tid);
                        var gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());

                        var weight = var(f32(1.0)).Copy();
                        weight *= Gaussian(length(soffset.ToF32()) * gamma);
                        // var uv_weight = weight * exp(-f32(1.0) * length(history_uv - src_history_uv));
                        weight *= GetWeight(center_gbuffer["N"], center_gbuffer["P"], gbuffer["N"], gbuffer["P"], eps);
                        // weight *= value.w();
                        // weight *= exp(-GetLuminance(value.xyz()));
                        //   l.z().AsF32() *
                        value_acc += weight * value.xyz();
                        value_2_acc += weight * value.xyz() * value.xyz();
                        // history_uv_acc += uv_weight * history_uv;
                        weigth_acc += weight;
                    // uv_weight_acc += uv_weight;
#if defined(HALTON)
                    });
#else  // !defined(HALTON)
                        });
                    });
#endif // !defined(HALTON)

#undef HALTON

#if 0
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
                        value_acc += weight * value.xyz();
                        value_2_acc += weight * value.xyz() * value.xyz();
                        weigth_acc += weight;
                    });
#endif // 0

                    // history_uv_acc /= max(f32(1.0e-3), uv_weight_acc);
                    value_acc /= max(f32(1.0e-3), weigth_acc);
                    value_2_acc /= max(f32(1.0e-3), weigth_acc);

                    var dst = value_acc.xyz();
                    // var dst_num_samples = value_acc.w();

                    var variance = sqrt(abs(value_2_acc - value_acc * value_acc));

                    // var final = (src_value["xyz"] * src_num_samples + dst["xyz"] * dst_num_samples) / max(f32(1.0e-3), src_num_samples + dst_num_samples);

                    // g_rw_result.Store(tid, make_f32x4(final["xyz"], dst_num_samples));
                    // g_rw_history_uv.Store(tid, history_uv_acc.xy());
                    g_rw_Result().Store(tid, value_acc.xyz());
                    g_rw_Variance().Store(tid, variance.xyz());
                },
                [&] {
                    // g_rw_history_uv.Store(tid, f32x2_splat(0.0));
                    g_rw_Result().Store(tid, f32x3_splat(0.0));
                    g_rw_Variance().Store(tid, f32x3_splat(0.0));
                });
            kernel = CompileGlobalModule(gfx, "SpatialFilter");
        }
    }
    void Execute(Raw_GGX_Gen *gen, ShadeReflectionsPass *shade) {
        ping_pong.Next();

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) kernel.SetResource(g_rw_##_name(), _name);
        TEXTURE_LIST
#undef TEXTURE

        kernel.SetResource(ShadeReflectionsPass::g_IndirectShade(), shade->GetIndirectShade());
        kernel.SetResource(Raw_GGX_Gen::g_Roughness(), gen->GetRoughness());
        kernel.SetResource(Raw_GGX_Gen::g_PackedGbuffer(), gen->GetPackedGbuffer());
        kernel.SetResource(Raw_GGX_Gen::g_NormalBRDF(), gen->GetNormalBRDF());
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
class Upscale2X {
protected:
    GfxContext gfx       = {};
    GPUKernel  kernel    = {};
    u32        width     = u32(0);
    u32        height    = u32(0);
    String     pass_name = "Upscale2X";

#define TEXTURE_LIST                                                                                                                                                               \
    TEXTURE(UpscaledRadiance, DXGI_FORMAT_R16G16B16A16_FLOAT, f32x3, width, height, 1, 1)                                                                                          \
    TEXTURE(UpscaledVariance, DXGI_FORMAT_R16G16B16A16_FLOAT, f32x3, width, height, 1, 1)

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) GfxTexture _name = {};
    TEXTURE_LIST
#undef TEXTURE

public:
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    static var g_rw_##_name() {                                                                                                                                                    \
        if (_depth == u32(1))                                                                                                                                                      \
            return ResourceAccess(Resource::Create(RWTexture2D_##_ty##_Ty, "g_" #_name));                                                                                          \
        else                                                                                                                                                                       \
            return ResourceAccess(Resource::Create(RWTexture3D_##_ty##_Ty, "g_" #_name));                                                                                          \
    }                                                                                                                                                                              \
    static var g_##_name() {                                                                                                                                                       \
        if (_depth == u32(1))                                                                                                                                                      \
            return ResourceAccess(Resource::Create(Texture2D_##_ty##_Ty, "g_" #_name));                                                                                            \
        else                                                                                                                                                                       \
            return ResourceAccess(Resource::Create(Texture3D_##_ty##_Ty, "g_" #_name));                                                                                            \
    }

    TEXTURE_LIST
#undef TEXTURE

    u32 GetWidth() { return width; }
    u32 GetHeight() { return height; }

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    GfxTexture &Get##_name() { return _name; }
    TEXTURE_LIST
#undef TEXTURE

    SJIT_DONT_MOVE(Upscale2X);
    ~Upscale2X() {
        kernel.Destroy();
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) gfxDestroyTexture(gfx, _name);
        TEXTURE_LIST
#undef TEXTURE
    }
    Upscale2X(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    {                                                                                                                                                                              \
        sjit_assert((_width) >= u32(1));                                                                                                                                           \
        sjit_assert((_height) >= u32(1));                                                                                                                                          \
        sjit_assert((_depth) >= u32(1));                                                                                                                                           \
        sjit_assert((_mips) >= u32(1));                                                                                                                                            \
        if (_depth == u32(1))                                                                                                                                                      \
            _name = gfxCreateTexture2D(gfx, (_width), (_height), (_fmt), (_mips));                                                                                                 \
        else                                                                                                                                                                       \
            _name = gfxCreateTexture3D(gfx, (_width), (_height), (_depth), (_fmt), (_mips));                                                                                       \
    }
        TEXTURE_LIST
#undef TEXTURE
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid  = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var dim  = u32x2(width, height);
            var hdim = u32x2(width / 2, height / 2);

            var  gid        = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];
            var  lds        = AllocateLDS(u32x4Ty, u32(16 * 16), "lds_values");
            var  gid_center = gid.xy() + u32x2(4, 4);
            auto linear_idx = [](var xy) { return (xy.x().ToI32() + xy.y().ToI32() * i32(16)).ToU32(); };

            Init_LDS_16x16(lds, [&](var src_coord) {
                var packed_gbuffer  = Raw_GGX_Gen::g_PackedGbuffer().Load(src_coord);
                var roughness       = Raw_GGX_Gen::g_Roughness().Load(src_coord);
                var radiance        = SpatialFilter::g_Result().Load(src_coord);
                var packed_radiance = Make(u32x2Ty);
                packed_radiance.x() = PackFp16x2ToU32(radiance["xy"]);
                packed_radiance.y() = PackFp16x2ToU32(make_f32x2(radiance["z"], roughness));

                var val = Zero(u32x4Ty).Copy();
                val.x() = packed_gbuffer;
                val.y() = packed_radiance.x();
                val.z() = packed_radiance.y();
                return val;
            });

            EmitGroupSync();

            var xi = GetNoise(tid);

            yfor(2) {
                xfor(2) {
                    var _tid                  = tid * u32(2) + u32x2(x, y);
                    var uv                    = (_tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                    var ray                   = GenCameraRay(uv);
                    var center_packed_gbuffer = g_gbuffer_encoded.Load(_tid);
                    var center_roughness      = g_gbuffer_roughness.Load(_tid);
                    var center_gbuffer        = DecodeGBuffer32Bits(ray, center_packed_gbuffer, xi.x());
                    var eps                   = GetEps(center_gbuffer["P"]);

                    var radiance_acc = Make(f32x3Ty);
                    var variance_acc = Make(f32x3Ty);
                    var weight_acc   = Make(f32Ty);
                    var gamma        = pow(f32(1.0) - center_roughness, f32(2.0));

                    var fradius = lerp(f32(4), f32(1), gamma);
                    var iradius = i32(4); // fradius.ToI32();
                    EmitForLoop(-iradius, iradius, [&](var _y) {
                        EmitForLoop(-iradius, iradius, [&](var _x) {
                            var soffset   = Make(i32x2Ty);
                            soffset.x()   = _x;
                            soffset.y()   = _y;
                            var src_coord = tid.AsI32() * i32(2) + get_checkerboard_offset(tid).AsI32() + soffset;
                            var uv        = (src_coord.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                            var ray       = GenCameraRay(uv);
                            var l         = lds.Load(linear_idx(gid_center.ToI32() + soffset));
                            var gbuffer   = DecodeGBuffer32Bits(ray, l.x(), xi.x());
                            var radiance  = Make(f32x4Ty);
                            radiance.xy() = UnpackU32ToF16x2(l.y()).ToF32();
                            radiance.zw() = UnpackU32ToF16x2(l.z()).ToF32();

                            var weight = var(f32(1.0)).Copy();
                            weight *= GetWeight(center_gbuffer["N"], center_gbuffer["P"], gbuffer["N"], gbuffer["P"], eps);
                            weight *= Gaussian(length((src_coord.AsI32() - _tid.AsI32()).ToF32()) * gamma * f32(2.0));

                            weight *= exp(-abs(radiance.w() - center_roughness) * f32(1.0));

                            // EmitIfElse(weight > f32(0.5), [&] {
                            radiance_acc += radiance.xyz() * weight;
                            variance_acc += radiance.xyz() * radiance.xyz() * weight;
                            weight_acc += weight;
                            //});
                        });
                    });

                    radiance_acc /= max(f32(1.0e-3), weight_acc);
                    variance_acc /= max(f32(1.0e-3), weight_acc);

                    var variance = sqrt(abs(variance_acc - radiance_acc * radiance_acc));

                    variance = max(variance, SpatialFilter::g_Variance().Sample(g_linear_sampler, uv).xyz());

                    g_rw_UpscaledRadiance().Store(_tid, radiance_acc.xyz());
                    g_rw_UpscaledVariance().Store(_tid, variance.xyz());
                }
            }
            kernel = CompileGlobalModule(gfx, "Upscale2X");
        }
    }
    void Execute(Raw_GGX_Gen *gen, ShadeReflectionsPass *shade, SpatialFilter *spatial_filter) {
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) kernel.SetResource(g_rw_##_name(), _name);
        TEXTURE_LIST
#undef TEXTURE
        kernel.SetResource(Raw_GGX_Gen::g_Roughness(), gen->GetRoughness());
        kernel.SetResource(Raw_GGX_Gen::g_PackedGbuffer(), gen->GetPackedGbuffer());
        // kernel.SetResource(ShadeReflectionsPass::g_IndirectShade(), shade->GetIndirectShade());
        kernel.SetResource(SpatialFilter::g_Result(), spatial_filter->GetResult());
        kernel.SetResource(SpatialFilter::g_Variance(), spatial_filter->GetVariance());
        kernel.CheckResources();
        kernel.Begin();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width / 2 + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height / 2 + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
        kernel.ResetTable();
    }
#undef TEXTURE_LIST
};
class ReflectionsReprojectPass {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    GfxTexture result = {};
    u32        width  = u32(0);
    u32        height = u32(0);

    var g_rw_result        = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_rw_result"));
    var g_history_uv       = ResourceAccess(Resource::Create(Texture2D_f32x2_Ty, "g_history_uv"));
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

            var tid      = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var dim      = u32x2(width, height);
            var uv       = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var velocity = g_velocity.Load(tid);
            // var tracked_uv = uv - velocity;
            var tracked_uv = g_history_uv.Load(tid); // uv - velocity;
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

                           BILINEAR_WEIGHTS(frac_uv);

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

                           EmitIfElse(
                               weight_acc > f32(0.8) && !isnan(weight_acc) && !isinf(weight_acc),
                               [&] {
                                   var prev            = prev_acc / max(f32(1.0e-5), weight_acc);
                                   var num_samples     = prev.w();
                                   var new_num_samples = min(f32(64.0), num_samples + f32(1.0));
                                   // var history_weight  = f32(1.0) - f32(1.0) / new_num_samples;
                                   // var mix             = lerp(cur, prev, history_weight);
                                   EmitIfElse(
                                       isnan(prev.xyz()).Any(), [&] { g_rw_result.Store(tid, make_f32x4(cur.xyz(), f32(1.0))); },
                                       [&] { g_rw_result.Store(tid, make_f32x4(prev.xyz(), new_num_samples)); });
                               },
                               [&] { g_rw_result.Store(tid, make_f32x4(cur.xyz(), f32(1.0))); });
                       },
                       [&] { g_rw_result.Store(tid, make_f32x4(cur.xyz(), f32(1.0))); });

            kernel = CompileGlobalModule(gfx, "ReflectionsReprojectPass");
        }
    }
    void Execute(                    //
        GfxTexture input,            //
        GfxTexture input_ray_length, //
        GfxTexture input_history_uv, //
        GfxTexture confidence,       //
        GfxTexture prev_input        //
    ) {

        kernel.SetResource(g_rw_result, result);
        kernel.SetResource(g_input, input);
        kernel.SetResource(g_history_uv, input_history_uv);
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
static var GetHistoryLength(var roughness) { return lerp(f32(4.0), f32(64.0), pow(roughness, f32(1.0 / 8.0))); }
class SpatialFilterLarge {
private:
    GfxContext gfx        = {};
    GPUKernel  kernels[2] = {};
    GfxTexture results[2] = {};
    u32        width      = u32(0);
    u32        height     = u32(0);

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return results[1]; }

    SJIT_DONT_MOVE(SpatialFilterLarge);
    ~SpatialFilterLarge() {
        ifor(2) kernels[i].Destroy();
        ifor(2) gfxDestroyTexture(gfx, results[i]);
    }
    SpatialFilterLarge(GfxContext _gfx) {
        u32 _width         = gfxGetBackBufferWidth(_gfx);
        u32 _height        = gfxGetBackBufferHeight(_gfx);
        gfx                = _gfx;
        width              = _width;
        height             = _height;
        ifor(2) results[i] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        u32x2 dirs[]       = {
            u32x2(1, 0), //
            u32x2(0, 1), //
        };
        ifor(2) {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid            = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var gid            = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];
            var g_rw_result    = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_rw_result"));
            var g_input        = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_input"));
            var g_blur_mask    = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_blur_mask"));
            var dim            = u32x2(width, height);
            var input          = g_input.Load(tid);
            var uv             = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var xi             = GetNoise(tid);
            var ray            = GenCameraRay(uv);
            var center_gbuffer = DecodeGBuffer32Bits(ray, g_gbuffer_encoded.Load(tid), xi.x());
            var eps            = GetEps(center_gbuffer["P"]);
            var num_samples    = u32(4);
            var roughness      = g_gbuffer_roughness.Load(tid);
            var history_length = GetHistoryLength(roughness);
            var blur_mask      = g_blur_mask.Load(tid);
            var fstride        = lerp(f32(4.0), f32(0.0), (f32(1.0) - blur_mask) * saturate(input.w() / history_length));
            var stride         = fstride.ToU32();

            EmitIfElse(
                stride == u32(0), [&] { g_rw_result.Store(tid, input); },
                [&] {
                    var value_acc  = input;
                    var weigth_acc = input.w().Copy();
                    value_acc *= input.w();
                    EmitForLoop(u32(0), num_samples * u32(2) + u32(1), [&](var iter) {
                        var j       = stride.ToI32() * (iter.ToI32() - num_samples.ToI32()).ToI32();
                        var soffset = var(dirs[i]).ToI32() * j;
                        var src_pos = soffset + tid.ToI32();
                        var uv      = (src_pos.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                        var ray     = GenCameraRay(uv);
                        var gbuffer = DecodeGBuffer32Bits(ray, g_gbuffer_encoded.Load(src_pos), xi.x());
                        var weight  = GetWeight(center_gbuffer["N"], center_gbuffer["P"], gbuffer["N"], gbuffer["P"], eps);
                        weight *= Gaussian(length(soffset.ToF32()) * (f32(1.0) - blur_mask));
                        var value = g_input.Load(src_pos);
                        weight *= value.w();
                        value_acc += weight * value;
                        weigth_acc += weight;
                    });

                    value_acc /= max(f32(1.0e-3), weigth_acc);

                    g_rw_result.Store(tid, value_acc);
                });
            kernels[i] = CompileGlobalModule(gfx, "SpatialFilterLarge");
        }
    }
    void Execute(GfxTexture input, GfxTexture blur_mask) {
        kernels[0].Begin();
        {
            kernels[0].SetResource("g_rw_result", results[0]);
            kernels[0].SetResource("g_input", input);
            kernels[0].SetResource("g_blur_mask", blur_mask);
            kernels[0].CheckResources();
            {
                u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernels[0].kernel);
                u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
                u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

                gfxCommandBindKernel(gfx, kernels[0].kernel);
                gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
            }
            kernels[0].ResetTable();
        }
        {
            kernels[1].SetResource("g_rw_result", results[1]);
            kernels[1].SetResource("g_input", results[0]);
            kernels[1].SetResource("g_blur_mask", blur_mask);
            kernels[1].CheckResources();

            {
                u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernels[1].kernel);
                u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
                u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

                gfxCommandBindKernel(gfx, kernels[1].kernel);
                gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
            }
            kernels[1].ResetTable();
        }
        kernels[0].End();
        g_pass_durations[kernels[0].name] = kernels[0].duration;
    }
};
class ReflectionsTemporalPass {
private:
    GfxContext gfx              = {};
    GPUKernel  kernel           = {};
    GPUKernel  expand_blur_mask = {};
    u32        width            = u32(0);
    u32        height           = u32(0);

#define TEXTURE_LIST                                                                                                                                                               \
    TEXTURE(BlurMask, DXGI_FORMAT_R8_UNORM, f32, width, height, 1, 1)                                                                                                              \
    TEXTURE(FinalBlurMask, DXGI_FORMAT_R8_UNORM, f32, width, height, 1, 1)                                                                                                         \
    TEXTURE(Result, DXGI_FORMAT_R16G16B16A16_FLOAT, f32x4, width, height, 1, 1)                                                                                                    \
    TEXTURE(PrevResult, DXGI_FORMAT_R16G16B16A16_FLOAT, f32x4, width, height, 1, 1)

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) GfxTexture _name = {};
    TEXTURE_LIST
#undef TEXTURE

public:
    u32 GetWidth() { return width; }
    u32 GetHeight() { return height; }

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    static var g_rw_##_name() {                                                                                                                                                    \
        if (_depth == u32(1))                                                                                                                                                      \
            return ResourceAccess(Resource::Create(RWTexture2D_##_ty##_Ty, "g_" #_name));                                                                                          \
        else                                                                                                                                                                       \
            return ResourceAccess(Resource::Create(RWTexture3D_##_ty##_Ty, "g_" #_name));                                                                                          \
    }                                                                                                                                                                              \
    static var g_##_name() {                                                                                                                                                       \
        if (_depth == u32(1))                                                                                                                                                      \
            return ResourceAccess(Resource::Create(Texture2D_##_ty##_Ty, "g_" #_name));                                                                                            \
        else                                                                                                                                                                       \
            return ResourceAccess(Resource::Create(Texture3D_##_ty##_Ty, "g_" #_name));                                                                                            \
    }

    TEXTURE_LIST
#undef TEXTURE

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    GfxTexture &Get##_name() { return _name; }
    TEXTURE_LIST
#undef TEXTURE

    SJIT_DONT_MOVE(ReflectionsTemporalPass);
    ~ReflectionsTemporalPass() {
        kernel.Destroy();
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) gfxDestroyTexture(gfx, _name);
        TEXTURE_LIST
#undef TEXTURE
    }
    ReflectionsTemporalPass(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    {                                                                                                                                                                              \
        sjit_assert((_width) >= u32(1));                                                                                                                                           \
        sjit_assert((_height) >= u32(1));                                                                                                                                          \
        sjit_assert((_depth) >= u32(1));                                                                                                                                           \
        sjit_assert((_mips) >= u32(1));                                                                                                                                            \
        if (_depth == u32(1))                                                                                                                                                      \
            _name = gfxCreateTexture2D(gfx, (_width), (_height), (_fmt), (_mips));                                                                                                 \
        else                                                                                                                                                                       \
            _name = gfxCreateTexture3D(gfx, (_width), (_height), (_depth), (_fmt), (_mips));                                                                                       \
    }
        TEXTURE_LIST
#undef TEXTURE

        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid        = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var dim        = u32x2(width, height);
            var roughness  = g_gbuffer_roughness.Load(tid);
            var cur        = Upscale2X::g_UpscaledRadiance().Load(tid);
            var raw_input  = ShadeReflectionsPass::g_IndirectShade().Load(tid / u32(2));
            var src_coord  = (tid / u32(2)) * u32(2) + get_checkerboard_offset(tid / u32(2));
            var velocity   = g_velocity.Load(tid);
            var uv         = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var tracked_uv = uv - velocity;

            var reproj = Make(f32x4Ty);
            EmitIfElse((tracked_uv > f32x2(0.0, 0.0)).All() && (tracked_uv < f32x2(1.0, 1.0)).All(), [&] {
                var N = g_gbuffer_world_normals.Load(tid);
                var P = g_gbuffer_world_position.Load(tid);

                var scaled_uv = tracked_uv * dim.ToF32() - f32x2(0.5, 0.5);
                var frac_uv   = frac(scaled_uv);
                var uv_lo     = scaled_uv.ToU32();

                var prev_acc   = Zero(f32x4Ty).Copy();
                var weight_acc = var(f32(0.0)).Copy();

                var eps = GetEps(P);

                BILINEAR_WEIGHTS(frac_uv);

                var history_length = GetHistoryLength(roughness);

                yfor(2) {
                    xfor(2) {
                        var rN     = g_prev_gbuffer_world_normals.Load(uv_lo + u32x2(x, y));
                        var rP     = g_prev_gbuffer_world_position.Load(uv_lo + u32x2(x, y));
                        var w      = GetWeight(N, P, rN, rP, eps);
                        var weight = bilinear_weights[y][x] * w;
                        EmitIfElse(w > f32(0.8), [&] {
                            prev_acc += weight * g_PrevResult().Load(uv_lo + u32x2(x, y));
                            weight_acc += weight;
                        });
                    }
                }

                EmitIfElse(weight_acc > f32(0.8) && !isnan(weight_acc) && !isinf(weight_acc), [&] {
                    var prev            = prev_acc / max(f32(1.0e-5), weight_acc);
                    var num_samples     = prev.w();
                    var new_num_samples = min(history_length, num_samples + f32(1.0));
                    EmitIfElse(
                        isnan(prev.xyz()).Any(), [&] {}, [&] { reproj = make_f32x4(prev.xyz(), new_num_samples); });
                });
            });

            var  num_samples    = max(f32(1.0), reproj.w());
            var  history_weight = f32(1.0) - f32(1.0) / max(f32(1.0), num_samples);
            var  gamma          = pow(roughness, f32(1.0 / 4.0));
            var  variance       = Upscale2X::g_UpscaledVariance().Load(tid); // * (f32(1.0) + f32(2.0) * gamma); // * (f32(1.0) + f32(4.0) * length(velocity));
            var  clip_size      = variance + f32x3_splat(5.0e-2);
            auto smooth_clip    = [&](var x, var c, var size) {
                var a    = clamp(x, c - size, c + size);
                var diff = a - x;
                return x + diff * f32(0.9);
            };
            var clipped_reproj = smooth_clip(reproj.xyz(), cur.xyz(), clip_size);

            // num_samples *= ;

            raw_input = smooth_clip(raw_input.xyz(), cur.xyz(), clip_size);
            EmitIfElse((src_coord != tid).Any(), [&] { gamma = f32(1.0); });
            cur = lerp(raw_input, cur, gamma);

            var diff           = length(reproj.xyz() - cur.xyz());
            var blur_mask      = pow(roughness, f32(1.0 / 8.0)) * (f32(1.0) - exp(-diff * diff * f32(16.0)));
            var prev_blur_mask = g_FinalBlurMask().Sample(g_linear_sampler, tracked_uv);
            var mix            = lerp(cur.xyz(), clipped_reproj.xyz(), history_weight);
            var blur_mask_mix  = lerp(blur_mask, prev_blur_mask, history_weight * (f32(1.0) - blur_mask) /* * f32(0.8)*/);
            g_rw_BlurMask().Store(tid, blur_mask_mix);

            g_rw_Result().Store(tid, make_f32x4(mix.xyz(), num_samples));

            kernel = CompileGlobalModule(gfx, "ReflectionsTemporalPass");
        }
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var gid = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];
            var dim = u32x2(width, height);

            var  lds        = AllocateLDS(u32x2Ty, u32(16 * 16), "lds_values");
            var  gid_center = gid.xy() + u32x2(4, 4);
            auto linear_idx = [](var xy) { return (xy.x().ToI32() + xy.y().ToI32() * i32(16)).ToU32(); };

            Init_LDS_16x16(lds, [&](var src_coord) {
                var in          = g_BlurMask().Load(src_coord);
                var val         = Zero(u32x2Ty).Copy();
                var gbuffer_val = g_gbuffer_encoded.Load(src_coord);
                val.x()         = gbuffer_val;
                val.y()         = in.AsU32();
                return val;
            });

            EmitGroupSync();

            var uv             = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var ray            = GenCameraRay(uv);
            var xi             = GetNoise(tid);
            var l              = lds.Load(linear_idx(gid_center));
            var center_gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());
            var eps            = GetEps(center_gbuffer["P"]);
            var acc            = Make(f32Ty);
            var weight_acc     = Make(f32Ty);

            EmitForLoop(i32(-4), i32(4), [&](var _y) {
                EmitForLoop(i32(-4), i32(4), [&](var _x) {
                    var soffset = Make(i32x2Ty);
                    soffset.x() = _x;
                    soffset.y() = _y;
                    var l       = lds.Load(linear_idx(gid_center.ToI32() + soffset));
                    var uv      = (tid.ToF32() + soffset.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                    var ray     = GenCameraRay(uv);
                    var xi      = GetNoise(tid);
                    var gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());
                    var weight  = var(f32(1.0)).Copy();
                    weight *= Gaussian(length(soffset.ToF32()) * f32(0.125));
                    weight *= GetWeight(center_gbuffer["N"], center_gbuffer["P"], gbuffer["N"], gbuffer["P"], eps);
                    acc += l.y().AsF32() * weight;
                    weight_acc += weight;
                });
            });

            acc /= max(f32(1.0e-3), weight_acc);

            g_rw_FinalBlurMask().Store(tid, acc);

            expand_blur_mask = CompileGlobalModule(gfx, "ReflectionsTemporalPass/expand_blur_mask");
        }
    }
    void Execute(                                    //
        Raw_GGX_ReflectionsPass *reflections,        //
        ShadeReflectionsPass    *shade,              //
        Upscale2X               *upscale,            //
        SpatialFilterLarge      *prev_spatial_filter //
    ) {
        std::swap(Result, PrevResult);
        {
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) kernel.SetResource(g_rw_##_name(), _name);
            TEXTURE_LIST
#undef TEXTURE

            kernel.SetResource(g_PrevResult(), prev_spatial_filter->GetResult(), /*_override*/ true);
            kernel.SetResource(ShadeReflectionsPass::g_IndirectShade(), shade->GetIndirectShade());
            kernel.SetResource(Upscale2X::g_UpscaledRadiance(), upscale->GetUpscaledRadiance());
            kernel.SetResource(Upscale2X::g_UpscaledVariance(), upscale->GetUpscaledVariance());

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
        {
            auto &kernel = expand_blur_mask;
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) kernel.SetResource(g_rw_##_name(), _name);
            TEXTURE_LIST
#undef TEXTURE

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
    }
};
} // namespace specular
class Shade {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    u32        width  = u32(0);
    u32        height = u32(0);

    var g_output      = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_output"));
    var g_specular_gi = ResourceAccess(Resource::Create(Texture2D_f32x4_Ty, "g_specular_gi"));

public:
    u32 GetWidth() { return width; }
    u32 GetHeight() { return height; }

    SJIT_DONT_MOVE(Shade);
    ~Shade() { kernel.Destroy(); }
    Shade(GfxContext _gfx) {
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
            var visibility    = g_visibility_buffer.Load(tid);
            var barys         = visibility.xy().AsF32();
            var instance_idx  = visibility.z();
            var primitive_idx = visibility.w();
            var l             = GetSunShadow(P, N);
            var ambient       = f32x3(0.1, 0.12, 0.2) / f32(8.0) * sample_env(N);
            var specular_gi   = g_specular_gi.Load(tid);
            var V             = -normalize(P - g_camera_pos);
            var L             = reflect(-V, N);
            var f             = pow(f32(1.0) - saturate(dot(L, N) / f32(2.0)), f32(5.0));
            var c             = specular::GetAlbedo(P, instance_idx);

            var instance          = g_InstanceBuffer[instance_idx];
            var mesh              = g_MeshBuffer[instance["mesh_id"]];
            var material          = g_MaterialBuffer[mesh["material_id"]];
            var albedo            = material["albedo"];
            var albedo_texture_id = albedo.w().AsU32();
            albedo.w()            = f32(1.0);
            EmitIfElse(albedo_texture_id != u32(0Xffffffff), [&] {
                var hit        = GetHit(barys, instance_idx, primitive_idx);
                var tex_albedo = g_Textures[albedo_texture_id.NonUniform()].Sample(g_linear_sampler, hit["UV"]);
                albedo *= tex_albedo;
            });

            var irradiance = l["xxx"] + ambient;
            var color      = albedo.xyz() * irradiance + f * specular_gi.xyz();
            // color                   = pow(color, f32(1.0) / f32(2.2));
            g_output.Store(tid, make_f32x4(color, f32(1.0)));
            // g_output.Store(tid, make_f32x4(pow(gi, f32(1.0) / f32(2.2)), f32(1.0)));
        });

        kernel = CompileGlobalModule(gfx, "Shade");

        // fprintf(stdout, kernel.isa.c_str());
    }
    void Execute(GfxTexture result, GfxTexture specular_gi) {
        kernel.SetResource(g_output->resource->GetName().c_str(), result);
        kernel.SetResource("g_specular_gi", specular_gi);
        kernel.CheckResources();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.ResetTable();
    }
};
class Experiment : public ISceneTemplate {
protected:
#define PASS_LIST                                                                                                                                                                  \
    PASS(specular::ProceduralRoughness, procedural_roughness)                                                                                                                      \
    PASS(EncodeGBuffer, encode_gbuffer)                                                                                                                                            \
    PASS(EdgeDetect, edge_detect)                                                                                                                                                  \
    PASS(specular::Raw_GGX_Gen, ggx_gen)                                                                                                                                           \
    PASS(specular::Upscale2X, upscale)                                                                                                                                             \
    PASS(specular::ShadeReflectionsPass, specular_shade_pass)                                                                                                                      \
    PASS(specular::ReflectionsTemporalPass, specular_temporal_pass)                                                                                                                \
    PASS(specular::SpatialFilter, specular_spatial_filter)                                                                                                                         \
    PASS(specular::SpatialFilterLarge, specular_spatial_filter_large)                                                                                                              \
    PASS(GBufferFromVisibility, gbuffer_from_vis)                                                                                                                                  \
    PASS(NearestVelocity, nearest_velocity)                                                                                                                                        \
    PASS(Shade, shade)                                                                                                                                                             \
    PASS(TAA, taa)                                                                                                                                                                 \
    PASS(specular::Raw_GGX_ReflectionsPass, specular_trace)                                                                                                                        \
    PASS(specular::ReflectionsReprojectPass, specular_reproject)

#define PASS(t, n) UniquePtr<t> n = {};
    PASS_LIST
#undef PASS

    GfxTimestampQuery reflection_timestamps[3] = {};

    std::vector<std::pair<char const *, GfxTexture &>> debug_views = {};

    bool render_gizmo        = false;
    bool debug_probe         = false;
    bool enable_taa          = false;
    bool animate_sun         = false;
    bool enable_taa_jitter   = false;
    f32  global_roughness    = f32(0.1);
    f32  roughness_grid_size = f32(64.0);
    u32  debug_view_id       = u32(0);

    void InitChild() override {}
    void ResizeChild() override {
        WaitIdle(gfx);
        ReleaseChild();

        ifor(3) if (!reflection_timestamps[i]) reflection_timestamps[i] = gfxCreateTimestampQuery(gfx);

#define PASS(t, n) n.reset(new t(gfx));
        PASS_LIST
#undef PASS
    }
    void Render() override {

        gbuffer_from_vis->Execute();
        set_global_resource(specular::g_roughness_grid_size, roughness_grid_size);
        set_global_resource(specular::g_global_roughness, global_roughness);
        set_global_resource(g_gbuffer_roughness, gbuffer_from_vis->GetRoughness());
        set_global_resource(g_prev_gbuffer_roughness, gbuffer_from_vis->GetPrevRoughness());
        g_global_runtime_resource_registry[g_gbuffer_world_normals->GetResource()->GetName()]       = gbuffer_from_vis->GetNormals();
        g_global_runtime_resource_registry[g_gbuffer_world_position->GetResource()->GetName()]      = gbuffer_from_vis->GetWorldPosition();
        g_global_runtime_resource_registry[g_prev_gbuffer_world_normals->GetResource()->GetName()]  = gbuffer_from_vis->GetPrevNormals();
        g_global_runtime_resource_registry[g_prev_gbuffer_world_position->GetResource()->GetName()] = gbuffer_from_vis->GetPrevWorldPosition();

        procedural_roughness->Execute();

        set_global_resource(g_gbuffer_roughness, procedural_roughness->GetRoughness());
        set_global_resource(g_prev_gbuffer_roughness, procedural_roughness->GetPrevRoughness());

        nearest_velocity->Execute();

        u32 timestamp_idx = frame_idx % u32(3);
        {
            gfxCommandBeginTimestampQuery(gfx, reflection_timestamps[timestamp_idx]);
            gfxCommandBeginEvent(gfx, "Reflections");

            encode_gbuffer->Execute();
            set_global_resource(g_gbuffer_encoded, encode_gbuffer->GetResult());
            set_global_resource(g_background, encode_gbuffer->GetBackground());

            edge_detect->Execute();
            set_global_resource(g_edges, edge_detect->GetResult());

            g_global_runtime_resource_registry[g_nearest_velocity->GetResource()->GetName()] = nearest_velocity->GetResult();

            ggx_gen->Execute();
            specular_trace->Execute(ggx_gen.get());
            specular_shade_pass->Execute(specular_trace.get());
            specular_spatial_filter->Execute(ggx_gen.get(), specular_shade_pass.get());
            upscale->Execute(ggx_gen.get(), specular_shade_pass.get(), specular_spatial_filter.get());

            specular_temporal_pass->Execute(specular_trace.get(), specular_shade_pass.get(), upscale.get(), specular_spatial_filter_large.get());
            specular_spatial_filter_large->Execute(specular_temporal_pass->GetResult(), specular_temporal_pass->GetFinalBlurMask());

            gfxCommandEndTimestampQuery(gfx, reflection_timestamps[timestamp_idx]);
            gfxCommandEndEvent(gfx);
        }

        f64 duration = (f64)gfxTimestampQueryGetDuration(gfx, reflection_timestamps[timestamp_idx]);

        shade->Execute(color_buffer, specular_spatial_filter_large->GetResult());

        taa->Execute(color_buffer);

        debug_views = {
            {"taa", taa->GetResult()},                                                                //
            {"edge_detect", edge_detect->GetResult()},                                                //
            {"ggx_gen->GetRoughness", ggx_gen->GetRoughness()},                                       //
            {"specular_trace->GetIndirectAlbedo", specular_trace->GetIndirectAlbedo()},               //
            {"specular_shade_pass->GetIndirectShade", specular_shade_pass->GetIndirectShade()},       //
            {"specular_spatial_filter->GetResult", specular_spatial_filter->GetResult()},             //
            {"specular_spatial_filter->GetVariance", specular_spatial_filter->GetVariance()},         //
            {"upscale->GetUpscaledRadiance", upscale->GetUpscaledRadiance()},                         //
            {"upscale->GetUpscaledVariance", upscale->GetUpscaledVariance()},                         //
            {"specular_temporal_pass->GetFinalBlurMask", specular_temporal_pass->GetFinalBlurMask()}, //
            {"specular_temporal_pass->GetResult", specular_temporal_pass->GetResult()},               //
            {"specular_spatial_filter_large->GetResult", specular_spatial_filter_large->GetResult()}, //
        };

        static bool slow_down = false;
        if (slow_down) Sleep(100);
        /* ImGui::Begin("Reflections");
         {
             ImVec2 wsize = GetImGuiSize();
             wsize.y      = wsize.x;
             ImGui::Text("specular_temporal_pass->GetFinalBlurMask");
             ImGui::Image((ImTextureID)&specular_temporal_pass->GetFinalBlurMask(), wsize);
         }
         ImGui::End();*/

        ImGui::Begin("Zoom");
        {
            ImVec2 wsize                           = GetImGuiSize();
            wsize.y                                = wsize.x;
            GfxImguiTextureParameters &config      = GfxImguiTextureParameters::GetConfig()[&GetResult()];
            f32                        size        = f32(1.0 / 16.0);
            ImVec2                     impos       = ImGui::GetMousePos();
            auto                       wpos        = ImGui::GetCursorScreenPos();
            auto                       window_size = f32x2(gfxGetBackBufferWidth(gfx), gfxGetBackBufferHeight(gfx));
            // impos.x -= wpos.x;
            // impos.y -= wpos.y;
            i32x2 mpos = i32x2(impos.x, impos.y);
            f32x2 uv   = f32x2(mpos.x, mpos.y);
            uv /= f32x2(window_size.x, window_size.y);
            // uv       = f32(2.0) * uv - f32x2(1.0, 1.0);
            // uv.y     = -uv.y;
            f32x2 mouse_uv = uv;
            // f32x2  mouse_uv = g_camera.mouse_uv * f32(0.5) + f32x2(0.5, 0.5);
            ImVec2 uv0 = ImVec2(mouse_uv.x - size, mouse_uv.y - size);
            ImVec2 uv1 = ImVec2(mouse_uv.x + size, mouse_uv.y + size);
            ImGui::Image((ImTextureID)&GetResult(), wsize, uv0, uv1);
        }
        ImGui::End();

        ImGui::Begin("Config");
        {
            ImGui::DragInt("debug_view_id", (int *)&debug_view_id, f32(0.01));
            debug_view_id = std::max(u32(0), std::min(u32(debug_views.size()), debug_view_id));
            ImGui::Text("[VIEW] %s", GetNamedResult().first);
            ImGui::SliderFloat("global_roughness", &global_roughness, f32(0.0), f32(1.0));
            ImGui::SliderFloat("roughness_grid_size", &roughness_grid_size, f32(16.0), f32(256.0));
            ImGui::Checkbox("animate_sun", &animate_sun);
            ImGui::Checkbox("taa", &enable_taa);
            ImGui::Checkbox("taa jitter", &enable_taa_jitter);
            // ImGui::Text("Specular GI Total %f ms", duration);
            for (auto &i : g_pass_durations) {
                ImGui::Text("%s %f ms", i.first.c_str(), i.second);
                // ImGui::Text("--- %s %f ns per pixel", i.first.c_str(), f64(1000000.0) * i.second / (width * height));
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
    std::pair<char const *, GfxTexture &> GetNamedResult() {
        if (debug_view_id < u32(debug_views.size())) return debug_views[debug_view_id];
        return debug_views[0];
    }
    GfxTexture &GetResult() override { return GetNamedResult().second; }
    void        ReleaseChild() override {
        ifor(3) if (reflection_timestamps[i]) {
            gfxDestroyTimestampQuery(gfx, reflection_timestamps[i]);
            reflection_timestamps[i] = {};
        }
        debug_views.clear();
#define PASS(t, n) n.reset();
        PASS_LIST
#undef PASS
    }
    void UpdateChild() override {
        if (enable_taa_jitter) {
            g_camera.jitter.x = (CalculateHaltonNumber(frame_idx % u32(12), 2) * f32(2.0) - f32(1.0)) / f32(width) / f32(2.0);
            g_camera.jitter.y = (CalculateHaltonNumber(frame_idx % u32(12), 3) * f32(2.0) - f32(1.0)) / f32(height) / f32(2.0);
        } else {
            g_camera.jitter = f32x2(0.0, 0.0);
        }
        if (animate_sun) sun.phi += f32(cur_delta_time) * f32(1.0e-3);
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
    sprintf_s(scene_path, "%sscenes\\stylised_sky_player_home_dioroma\\scene.gltf", _working_directory);

    GfxJit::Experiment exp = {};
    exp.Init(scene_path, shader_path, shader_include_path);

    exp.WindowLoop();

    exp.Release();

    return 0;
}
