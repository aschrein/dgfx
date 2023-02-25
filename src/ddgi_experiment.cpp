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

GFX_JIT_MAKE_GLOBAL_RESOURCE(g_ddgi_radiance_probes, Texture3D_f32x4_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_ddgi_distance_probes, Texture3D_f32x2_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_ddgi_cascade_min, f32x3Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_ddgi_cascade_max, f32x3Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_ddgi_cascade_dim, f32x3Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_ddgi_cascade_spacing, f32Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_diffuse_gi, Texture2D_f32x3_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_prev_closest_gbuffer_world_normals, Texture2D_f32x3_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_prev_closest_gbuffer_world_position, Texture2D_f32x3_Ty);

// UniquePtr<GfxTextureResource> g_radiance_probes = GFX_JIT_MAKE_TEXTURE(gfx, "g_radiance_probes", num_probes_x *radiance_probe_size, num_probes_y *radiance_probe_size,
// num_probes_z, u32(1), DXGI_FORMAT_R16G16B16A16_FLOAT, u32(1));

static var SampleDDGIProbe(var p, var n) {
    var uv                          = Octahedral::Encode(n);
    var irradiance_acc              = var(f32x3_splat(0.0)).Copy();
    var irradiance_no_shadowing_acc = var(f32x3_splat(0.0)).Copy();
    var weight_no_shadowing_acc     = var(f32(0.0)).Copy();
    var weight_acc                  = var(f32(0.0)).Copy();
    EmitIfElse((p > g_ddgi_cascade_min + f32x3_splat(g_ddgi_cascade_spacing)).All() && (p < g_ddgi_cascade_max - f32x3_splat(g_ddgi_cascade_spacing)).All(), [&] {
        var rp            = (p - g_ddgi_cascade_min) / g_ddgi_cascade_spacing - f32x3_splat(0.5);
        var base_probe_id = rp.ToU32();
        var frac_rp       = frac(rp);

        TRILINEAR_WEIGHTS(frac_rp);

        zfor(2) {
            yfor(2) {
                xfor(2) {
                    var probe_id  = base_probe_id + u32x3(x, y, z);
                    var probe_pos = (probe_id.ToF32() + f32x3_splat(0.5)) * g_ddgi_cascade_spacing + g_ddgi_cascade_min;
                    var dr        = probe_pos - p;
                    var dist      = length(dr);
                    // EmitIfElse(dist > f32(1.0e-3), [&] {
                    var falloff = (f32(1.0) - exp(-f32(2.0) * dist / g_ddgi_cascade_spacing)) * max(f32(0.0), f32(0.5) + f32(0.5) * dot(normalize(dr), n));
                    // var falloff               = f32(0.5) + f32(0.5) * dot(normalize(dr), n);
                    probe_id["xyz"]           = probe_id["xzy"];
                    var suv                   = lerp(f32x2_splat(1.0) / f32x2_splat(8.0), f32x2_splat(7.0) / f32x2_splat(8.0), saturate(uv));
                    var full_uv               = make_f32x3(suv + probe_id.xy().ToF32(), probe_id.z().ToF32() + f32(0.5)) / g_ddgi_cascade_dim;
                    var suv_dist              = lerp(f32x2_splat(1.0) / f32x2_splat(16.0), f32x2_splat(15.0) / f32x2_splat(16.0), saturate(uv));
                    var full_uv_dist          = make_f32x3(suv_dist + probe_id.xy().ToF32(), probe_id.z().ToF32() + f32(0.5)) / g_ddgi_cascade_dim;
                    var probe_dist_mean_mean2 = g_ddgi_distance_probes.Sample(g_linear_sampler, full_uv_dist).xy();
                    var weight                = square(falloff) * trilinear_weights[z][y][x];
                    var sample                = (max(f32x3_splat(0.0), g_ddgi_radiance_probes.Sample(g_linear_sampler, full_uv).xyz()));
                    var mean                  = probe_dist_mean_mean2.x();
                    var mean2                 = probe_dist_mean_mean2.y();

                    irradiance_no_shadowing_acc += weight * sqrt(sample);
                    weight_no_shadowing_acc += weight;

                    // Chebyshev
                    EmitIfElse(mean < dist, [&] {
                        var variance = abs(square(mean) - mean2);
                        variance     = max(f32(1.0e-3), variance);
                        weight *= saturate(variance / (f32(1.0e-6) + variance + square(dist - mean)));
                    });
                    // weight = (weight);
                    irradiance_acc += weight * sqrt(sample);
                    weight_acc += weight;
                    //});
                }
            }
        }
    });
    var irradiance              = irradiance_acc / max(f32(1.0e-4), weight_acc);
    var irraidance_no_chebyshev = irradiance_no_shadowing_acc / max(f32(1.0e-4), weight_no_shadowing_acc);
    irradiance                  = lerp(irraidance_no_chebyshev, irradiance, saturate(weight_acc)); // For low weight fall back to non-shadowed interpolation
    return square(irradiance);
}
class DDGI {
private:
    GfxContext gfx                    = {};
    GPUKernel  kernel                 = {};
    GPUKernel  dup_border_kernel      = {};
    GPUKernel  dup_border_dist_kernel = {};
    GPUKernel  apply_kernel           = {};
    u32        radiance_probe_size    = u32(8);
    u32        distance_probe_size    = u32(16);
    GfxTexture result                 = {};
    GfxTexture radiance_probes        = {};
    GfxTexture distance_probes        = {};
    u32        num_probes_x           = u32(0);
    u32        num_probes_y           = u32(0);
    u32        num_probes_z           = u32(0);
    f32x3      lo                     = {};
    f32        spacing                = {};
    u32        frame_idx              = u32(0);
    u32        width                  = u32(0);
    u32        height                 = u32(0);

    var g_radiance_probes = ResourceAccess(Resource::Create(RWTexture3D_f32x4_Ty, "g_radiance_probes"));
    var g_distance_probes = ResourceAccess(Resource::Create(RWTexture3D_f32x2_Ty, "g_distance_probes"));
    var g_slice_idx       = ResourceAccess(Resource::Create(u32Ty, "g_slice_idx"));
    var g_output          = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_output"));
    // var g_direction       = ResourceAccess(Resource::Create(u32x2Ty, "g_direction"));

public:
    GfxTexture &GetDiffuseGI() { return result; }
    GfxTexture &GetRadianceProbeAtlas() { return radiance_probes; }
    GfxTexture &GetDistanceProbeAtlas() { return distance_probes; }
    u32         GetNumProbesX() { return num_probes_x; }
    u32         GetNumProbesY() { return num_probes_y; }
    u32         GetNumProbesZ() { return num_probes_z; }
    f32         GetSpacing() { return spacing; }
    f32x3       GetLo() { return lo; }
    f32x3       GetHi() { return lo + f32x3(GetNumProbesX(), GetNumProbesY(), GetNumProbesZ()) * spacing; }

    SJIT_DONT_MOVE(DDGI);
    ~DDGI() {
        kernel.Destroy();
        dup_border_kernel.Destroy();
        dup_border_dist_kernel.Destroy();
        gfxDestroyTexture(gfx, radiance_probes);
        gfxDestroyTexture(gfx, radiance_probes);
        gfxDestroyTexture(gfx, result);
    }
    void PushGizmos(GfxGizmoManager &gizmo_manager) {
        xfor(num_probes_x) {
            yfor(num_probes_y) {
                zfor(num_probes_z) {
                    f32x3 p    = (f32x3(x, y, z) + f32x3(0.5, 0.5, 0.5)) * spacing + lo;
                    f32   size = f32(spacing) / f32(2.0);
                    gizmo_manager.AddLineAABB(p - f32x3_splat(size), p + f32x3_splat(size), f32x3(1.0, 0.0, 0.0));
                }
            }
        }
    }
    DDGI(GfxContext _gfx) {
        u32   _width        = gfxGetBackBufferWidth(_gfx);
        u32   _height       = gfxGetBackBufferHeight(_gfx);
        u32   _num_probes_x = u32(16);
        u32   _num_probes_y = u32(16);
        u32   _num_probes_z = u32(16);
        f32   ddgi_size     = f32(16.0);
        f32x3 _lo           = -f32x3(ddgi_size, ddgi_size, ddgi_size) / f32(2.0);
        f32   _spacing      = f32(ddgi_size) / f32(16.0);

        gfx             = _gfx;
        num_probes_x    = _num_probes_x;
        num_probes_y    = _num_probes_y;
        num_probes_z    = _num_probes_z;
        lo              = _lo;
        spacing         = _spacing;
        result          = gfxCreateTexture2D(gfx, _width, _height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        radiance_probes = gfxCreateTexture3D(gfx, num_probes_x * radiance_probe_size, num_probes_y * radiance_probe_size, num_probes_z, DXGI_FORMAT_R16G16B16A16_FLOAT);
        distance_probes = gfxCreateTexture3D(gfx, num_probes_x * distance_probe_size, num_probes_y * distance_probe_size, num_probes_z, DXGI_FORMAT_R16G16_FLOAT);
        width           = _width;
        height          = _height;

        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(4), u32(4), u32(4)});

            u32x3 num_probes = u32x3(num_probes_x, num_probes_y, num_probes_z);

            var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xyz"];
            EmitIfElse((tid < num_probes).All(), [&] {
                var offset = lo + (tid["xzy"].ToF32() + f32x3(0.5, 0.5, 0.5)) * spacing;

                EmitForLoop(u32(0), u32(32), [&](var iter) {
                    var xi = frac(GetNoise(tid.xy()) + (PHI * (pcg(g_frame_idx + pcg(iter)) % u32(79)).ToF32())["xx"]);
                    // var xi = Hammersley(g_frame_idx % u32(1024), u32(1024));

                    var sub_coord      = f32x2(1.0, 1.0) + xi * f32x2(radiance_probe_size - u32(2), radiance_probe_size - u32(2));
                    var sub_dist_coord = f32x2(1.0, 1.0) + xi * f32x2(distance_probe_size - u32(2), distance_probe_size - u32(2));

                    var dst_coord      = tid * u32x3(radiance_probe_size, radiance_probe_size, 1) + make_u32x3((sub_coord).ToU32(), 0);
                    var dst_dist_coord = tid * u32x3(distance_probe_size, distance_probe_size, 1) + make_u32x3((sub_dist_coord).ToU32(), 0);

                    var dir = Octahedral::Decode(xi);

                    var ray_desc          = Zero(RayDesc_Ty);
                    ray_desc["Direction"] = dir;
                    ray_desc["Origin"]    = offset;
                    ray_desc["TMin"]      = f32(1.0e-3);
                    ray_desc["TMax"]      = f32(1.0e6);
                    var ray_query         = RayQuery(g_tlas, ray_desc);
                    var prev              = g_radiance_probes.Load(dst_coord);
                    var prev_dist         = g_distance_probes.Load(dst_dist_coord);
                    var new_val           = var(f32x4_splat(0.0)).Copy();
                    var new_dist_val      = var(f32x2(0.0, 0.0)).Copy();

                    EmitIfElse(
                        ray_query["hit"],
                        [&] {
                            var hit = GetHit(ray_query);
                            var w   = hit["W"];
                            var n   = hit["N"];
                            EmitIfElse(dot(dir, n) < f32(0.0), [&] {
                                var l        = GetSunShadow(w, n);
                                var c        = random_albedo(ray_query["instance_id"].ToF32());
                                new_val      = make_f32x4(c * l, f32(1.0));
                                var dist     = length(w - offset);
                                new_dist_val = make_f32x2(dist, dist * dist);
                            });
                        },
                        [&] { new_val = f32x4_splat(0.0); });
                    EmitIfElse((prev == f32x4(0.0, 0.0, 0.0, 0.0)).All(), [&] {
                        prev      = new_val;
                        prev_dist = new_dist_val;
                    }); // Reset
                    var result      = lerp(prev, new_val, f32(1.0) / f32(64.0));
                    var result_dist = lerp(prev_dist, new_dist_val, f32(1.0) / f32(64.0));
                    g_radiance_probes.Store(dst_coord, result);
                    g_distance_probes.Store(dst_dist_coord, result_dist);
                });
            });

            // fprintf(stdout, GetGlobalModule().Finalize());

            kernel = CompileGlobalModule(gfx, "DDGI/Trace");
        }
        {
            HLSL_MODULE_SCOPE;

            u32 group_size = u32(32);

            GetGlobalModule().SetGroupSize({group_size, u32(1), u32(1)});

            var group_idx = Input(IN_TYPE_DISPATCH_GROUP_ID)["xy"];
            var tid       = Input(IN_TYPE_DISPATCH_THREAD_ID)["x"];
            var gid       = Input(IN_TYPE_GROUP_THREAD_ID)["x"];
            // var num_groups           = num_probes_x * num_probes_z;
            // var num_pixels_per_group = u32(32); // 6 + 6 + 6 + 6 + 1 + 1 + 1 + 1 = 28 + 4 dead lanes
            // var groupd_idx           = tid / num_pixels_per_group;

            //             8x8
            //  +---+-----------------+---+
            //  | 1 |       6         | 1 |
            //  +---+-----------------+---+
            //  |   |                 |   |
            //  |   |                 |   |
            //  |   |                 |   |
            //  | 6 |      6X6        | 6 |
            //  |   |                 |   |
            //  |   |                 |   |
            //  +---+-----------------+---+
            //  | 1 |       6         | 1 |
            //  +---+-----------------+---+
            //

            var dst_coords = MakeStaticArray({
                //
                u32x2(0, 0), //
                u32x2(1, 0), //
                u32x2(2, 0), //
                u32x2(3, 0), //

                u32x2(4, 0), //
                u32x2(5, 0), //
                u32x2(6, 0), //
                u32x2(7, 0), //

                u32x2(7, 0), //
                u32x2(7, 1), //
                u32x2(7, 2), //
                u32x2(7, 3), //

                u32x2(7, 4), //
                u32x2(7, 5), //
                u32x2(7, 6), //
                u32x2(7, 7), //

                u32x2(0, 7), //
                u32x2(1, 7), //
                u32x2(2, 7), //
                u32x2(3, 7), //

                u32x2(4, 7), //
                u32x2(5, 7), //
                u32x2(6, 7), //
                u32x2(7, 7), //

                u32x2(0, 0), //
                u32x2(0, 1), //
                u32x2(0, 2), //
                u32x2(0, 3), //

                u32x2(0, 4), //
                u32x2(0, 5), //
                u32x2(0, 6), //
                u32x2(0, 7), //
            });

            var src_coords = MakeStaticArray({
                //
                u32x2(7 - 0, 1), //
                u32x2(7 - 1, 1), //
                u32x2(7 - 2, 1), //
                u32x2(7 - 3, 1), //

                u32x2(7 - 4, 1), //
                u32x2(7 - 5, 1), //
                u32x2(7 - 6, 1), //
                u32x2(7 - 7, 1), //

                u32x2(6, 7 - 0), //
                u32x2(6, 7 - 1), //
                u32x2(6, 7 - 2), //
                u32x2(6, 7 - 3), //

                u32x2(6, 7 - 4), //
                u32x2(6, 7 - 5), //
                u32x2(6, 7 - 6), //
                u32x2(6, 7 - 7), //

                u32x2(7 - 0, 6), //
                u32x2(7 - 1, 6), //
                u32x2(7 - 2, 6), //
                u32x2(7 - 3, 6), //

                u32x2(7 - 4, 6), //
                u32x2(7 - 5, 6), //
                u32x2(7 - 6, 6), //
                u32x2(7 - 7, 6), //

                u32x2(1, 7 - 0), //
                u32x2(1, 7 - 1), //
                u32x2(1, 7 - 2), //
                u32x2(1, 7 - 3), //

                u32x2(1, 7 - 4), //
                u32x2(1, 7 - 5), //
                u32x2(1, 7 - 6), //
                u32x2(1, 7 - 7), //
            });

            var dst_coord = dst_coords[gid].ToU32() + group_idx * u32(8);
            var src_coord = src_coords[gid].ToU32() + group_idx * u32(8);
            var val       = g_radiance_probes.Load(make_u32x3(src_coord, g_slice_idx));
            g_radiance_probes.Store(make_u32x3(dst_coord, g_slice_idx), val);

            // fprintf(stdout, GetGlobalModule().Finalize());

            dup_border_kernel = CompileGlobalModule(gfx, "DDGI/Clone8");
        }
        {
            HLSL_MODULE_SCOPE;

            u32 group_size = u32(64);

            GetGlobalModule().SetGroupSize({group_size, u32(1), u32(1)});

            var group_idx = Input(IN_TYPE_DISPATCH_GROUP_ID)["xy"];
            var tid       = Input(IN_TYPE_DISPATCH_THREAD_ID)["x"];
            var gid       = Input(IN_TYPE_GROUP_THREAD_ID)["x"];

            //             16x16
            //  +---+-----------------+---+
            //  | 1 |       14        | 1 |
            //  +---+-----------------+---+
            //  |   |                 |   |
            //  |   |                 |   |
            //  |   |                 |   |
            //  |14 |     14X14       |14 |
            //  |   |                 |   |
            //  |   |                 |   |
            //  +---+-----------------+---+
            //  | 1 |      14         | 1 |
            //  +---+-----------------+---+
            //

            var dst_coords = MakeStaticArray({
                //
                u32x2(0, 0),  //
                u32x2(1, 0),  //
                u32x2(2, 0),  //
                u32x2(3, 0),  //
                u32x2(4, 0),  //
                u32x2(5, 0),  //
                u32x2(6, 0),  //
                u32x2(7, 0),  //
                u32x2(8, 0),  //
                u32x2(9, 0),  //
                u32x2(10, 0), //
                u32x2(11, 0), //
                u32x2(12, 0), //
                u32x2(13, 0), //
                u32x2(14, 0), //
                u32x2(15, 0), //

                u32x2(15, 0),  //
                u32x2(15, 1),  //
                u32x2(15, 2),  //
                u32x2(15, 3),  //
                u32x2(15, 4),  //
                u32x2(15, 5),  //
                u32x2(15, 6),  //
                u32x2(15, 7),  //
                u32x2(15, 8),  //
                u32x2(15, 9),  //
                u32x2(15, 10), //
                u32x2(15, 11), //
                u32x2(15, 12), //
                u32x2(15, 13), //
                u32x2(15, 14), //
                u32x2(15, 15), //

                u32x2(0, 15),  //
                u32x2(1, 15),  //
                u32x2(2, 15),  //
                u32x2(3, 15),  //
                u32x2(4, 15),  //
                u32x2(5, 15),  //
                u32x2(6, 15),  //
                u32x2(7, 15),  //
                u32x2(8, 15),  //
                u32x2(9, 15),  //
                u32x2(10, 15), //
                u32x2(11, 15), //
                u32x2(12, 15), //
                u32x2(13, 15), //
                u32x2(14, 15), //
                u32x2(15, 15), //

                u32x2(0, 0),  //
                u32x2(0, 1),  //
                u32x2(0, 2),  //
                u32x2(0, 3),  //
                u32x2(0, 4),  //
                u32x2(0, 5),  //
                u32x2(0, 6),  //
                u32x2(0, 7),  //
                u32x2(0, 8),  //
                u32x2(0, 9),  //
                u32x2(0, 10), //
                u32x2(0, 11), //
                u32x2(0, 12), //
                u32x2(0, 13), //
                u32x2(0, 14), //
                u32x2(0, 15), //
            });

            var src_coords = MakeStaticArray({
                //
                u32x2(15 - 0, 1),  //
                u32x2(15 - 1, 1),  //
                u32x2(15 - 2, 1),  //
                u32x2(15 - 3, 1),  //
                u32x2(15 - 4, 1),  //
                u32x2(15 - 5, 1),  //
                u32x2(15 - 6, 1),  //
                u32x2(15 - 7, 1),  //
                u32x2(15 - 8, 1),  //
                u32x2(15 - 9, 1),  //
                u32x2(15 - 10, 1), //
                u32x2(15 - 11, 1), //
                u32x2(15 - 12, 1), //
                u32x2(15 - 13, 1), //
                u32x2(15 - 14, 1), //
                u32x2(15 - 15, 1), //

                u32x2(14, 15 - 0),  //
                u32x2(14, 15 - 1),  //
                u32x2(14, 15 - 2),  //
                u32x2(14, 15 - 3),  //
                u32x2(14, 15 - 4),  //
                u32x2(14, 15 - 5),  //
                u32x2(14, 15 - 6),  //
                u32x2(14, 15 - 7),  //
                u32x2(14, 15 - 8),  //
                u32x2(14, 15 - 9),  //
                u32x2(14, 15 - 10), //
                u32x2(14, 15 - 11), //
                u32x2(14, 15 - 12), //
                u32x2(14, 15 - 13), //
                u32x2(14, 15 - 14), //
                u32x2(14, 15 - 15), //

                u32x2(15 - 0, 14),  //
                u32x2(15 - 1, 14),  //
                u32x2(15 - 2, 14),  //
                u32x2(15 - 3, 14),  //
                u32x2(15 - 4, 14),  //
                u32x2(15 - 5, 14),  //
                u32x2(15 - 6, 14),  //
                u32x2(15 - 7, 14),  //
                u32x2(15 - 8, 14),  //
                u32x2(15 - 9, 14),  //
                u32x2(15 - 10, 14), //
                u32x2(15 - 11, 14), //
                u32x2(15 - 12, 14), //
                u32x2(15 - 13, 14), //
                u32x2(15 - 14, 14), //
                u32x2(15 - 15, 14), //

                u32x2(1, 15 - 0),  //
                u32x2(1, 15 - 1),  //
                u32x2(1, 15 - 2),  //
                u32x2(1, 15 - 3),  //
                u32x2(1, 15 - 4),  //
                u32x2(1, 15 - 5),  //
                u32x2(1, 15 - 6),  //
                u32x2(1, 15 - 7),  //
                u32x2(1, 15 - 8),  //
                u32x2(1, 15 - 9),  //
                u32x2(1, 15 - 10), //
                u32x2(1, 15 - 11), //
                u32x2(1, 15 - 12), //
                u32x2(1, 15 - 13), //
                u32x2(1, 15 - 14), //
                u32x2(1, 15 - 15), //
            });

            var dst_coord = dst_coords[gid].ToU32() + group_idx * u32(16);
            var src_coord = src_coords[gid].ToU32() + group_idx * u32(16);
            var val       = g_distance_probes.Load(make_u32x3(src_coord, g_slice_idx));
            g_distance_probes.Store(make_u32x3(dst_coord, g_slice_idx), val);

            // fprintf(stdout, GetGlobalModule().Finalize());

            dup_border_dist_kernel = CompileGlobalModule(gfx, "DDGI/Clone16");
        }
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var dim = u32x2(_width, _height);

            var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];

            EmitIfElse((tid < dim).All(), [&] {
                var N = g_gbuffer_world_normals.Load(tid);
                var P = g_gbuffer_world_position.Load(tid);
                EmitIfElse((N == f32x3_splat(0.0)).All(), [&] {
                    g_output.Store(tid, f32x4_splat(0.01));
                    EmitReturn();
                });
                var ao            = g_ao.Load(tid);
                var visibility    = g_visibility_buffer.Load(tid);
                var barys         = visibility.xy().AsF32();
                var instance_idx  = visibility.z();
                var primitive_idx = visibility.w();
                var l             = GetSunShadow(P, N);
                var gi            = SampleDDGIProbe(P, N);
                var c             = random_albedo(instance_idx.ToF32());
                g_output.Store(tid, make_f32x4(ao.x() * gi, f32(1.0)));
            });

            apply_kernel = CompileGlobalModule(gfx, "DDGI/Apply");
        }
    }
    void Execute() {
        defer(frame_idx++);
        {
            kernel.SetResource(g_radiance_probes->GetResource()->GetName().c_str(), radiance_probes);
            kernel.SetResource(g_distance_probes->GetResource()->GetName().c_str(), distance_probes);
            kernel.CheckResources();
            {
                u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
                u32        num_groups_x = (num_probes_x + num_threads[0] - 1) / num_threads[0];
                u32        num_groups_y = (num_probes_z + num_threads[1] - 1) / num_threads[1];
                u32        num_groups_z = (num_probes_y + num_threads[1] - 1) / num_threads[1];

                gfxCommandBindKernel(gfx, kernel.kernel);
                gfxCommandDispatch(gfx, num_groups_x, num_groups_y, num_groups_z);
            }
            kernel.ResetTable();
        }
        {
            u32 slice_idx = frame_idx % num_probes_y;
            dup_border_kernel.SetResource(g_radiance_probes->GetResource()->GetName().c_str(), radiance_probes);
            dup_border_kernel.SetResource(g_slice_idx->GetResource()->GetName().c_str(), slice_idx);
            dup_border_kernel.CheckResources();
            gfxCommandBindKernel(gfx, dup_border_kernel.kernel);
            gfxCommandDispatch(gfx, num_probes_x, num_probes_z, u32(1));

            dup_border_kernel.ResetTable();
        }
        {
            u32 slice_idx = frame_idx % num_probes_y;
            dup_border_dist_kernel.SetResource(g_distance_probes->GetResource()->GetName().c_str(), distance_probes);
            dup_border_dist_kernel.SetResource(g_slice_idx->GetResource()->GetName().c_str(), slice_idx);
            dup_border_dist_kernel.CheckResources();
            gfxCommandBindKernel(gfx, dup_border_dist_kernel.kernel);
            gfxCommandDispatch(gfx, num_probes_x, num_probes_z, u32(1));

            dup_border_dist_kernel.ResetTable();
        }
        {
            apply_kernel.SetResource(g_output->resource->GetName().c_str(), result);
            apply_kernel.CheckResources();
            {
                u32 const *num_threads  = gfxKernelGetNumThreads(gfx, apply_kernel.kernel);
                u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
                u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

                gfxCommandBindKernel(gfx, apply_kernel.kernel);
                gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
            }
            kernel.ResetTable();
        }
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
class PreFilterAO {
private:
    GfxContext gfx       = {};
    GPUKernel  kernel    = {};
    GfxTexture result    = {};
    u32        width     = u32(0);
    u32        height    = u32(0);
    PingPong   ping_pong = {};

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return result; }

    SJIT_DONT_MOVE(PreFilterAO);
    ~PreFilterAO() {
        kernel.Destroy();
        gfxDestroyTexture(gfx, result);
    }
    PreFilterAO(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        //, DXGI_FORMAT _format = DXGI_FORMAT_R16G16B16A16_FLOAT
        gfx    = _gfx;
        width  = _width;
        height = _height;
        // u32 num_components = GetNumComponents(_format);
        result = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var gid = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];

            var g_rw_result = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_rw_result"));
            var g_input     = ResourceAccess(Resource::Create(Texture2D_f32x4_Ty, "g_input"));
            var dim         = u32x2(width, height); // g_rw_result.GetDimensions().Swizzle("xy");

            var uv         = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var velocity   = g_velocity.Load(tid);
            var tracked_uv = uv - velocity;

            var  lds        = AllocateLDS(u32x2Ty, u32(16 * 16), "lds_values");
            var  gid_center = gid.xy() + u32x2(4, 4);
            auto linear_idx = [](var xy) { return (xy.x().ToI32() + xy.y().ToI32() * i32(16)).ToU32(); };
            var  group_tid  = u32(8) * (tid / u32(8));

            Init_LDS_16x16(lds, [&](var src_coord) {
                var in          = g_input.Load(src_coord);
                var val         = Zero(u32x2Ty).Copy();
                var gbuffer_val = g_gbuffer_encoded.Load(src_coord);
                val.x()         = gbuffer_val;
                val.y()         = in.x().AsU32();
                return val;
            });
            EmitGroupSync();
            var l              = lds.Load(linear_idx(gid_center));
            var value_acc      = l.y().AsF32();
            var weigth_acc     = var(f32(1.0)).Copy();
            var ray            = GenCameraRay(uv);
            var xi             = GetNoise(tid);
            var center_gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());
            var eps            = GetEps(center_gbuffer["P"]);

            var halton_sample_offsets = MakeStaticArray(halton_samples);

            EmitForLoop(i32(0), i32(halton_sample_count), [&](var iter) {
                var soffset = halton_sample_offsets[iter];
                EmitIfElse((g_frame_idx & u32(1)) != u32(0), [&] { soffset.xy() = soffset.yx(); });
                var l       = lds.Load(linear_idx(gid_center.ToI32() + soffset));
                var uv      = (tid.ToF32() + halton_sample_offsets[iter].ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                var ray     = GenCameraRay(uv);
                var xi      = GetNoise(tid);
                var gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());
                var weight  = GetWeight(center_gbuffer["N"], center_gbuffer["P"], gbuffer["N"], gbuffer["P"], eps);
                // weight *= Gaussian(length(soffset.ToF32()));
                var val = l.y().AsF32();
                value_acc += weight * val;
                weigth_acc += weight;
            });
            value_acc /= weigth_acc;

            g_rw_result.Store(tid, make_f32x4(value_acc["xxx"], f32(1.0)));

            kernel = CompileGlobalModule(gfx, "PreFilterAO");
        }
    }
    void Execute(GfxTexture input) {
        ping_pong.Next();
        kernel.SetResource("g_rw_result", result);
        kernel.SetResource("g_input", input);
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
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
    }
};
class TemporalFilter {
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

    SJIT_DONT_MOVE(TemporalFilter);
    ~TemporalFilter() {
        kernel.Destroy();
        ifor(2) gfxDestroyTexture(gfx, results[i]);
    }
    TemporalFilter(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        //, DXGI_FORMAT _format = DXGI_FORMAT_R16G16B16A16_FLOAT
        gfx    = _gfx;
        width  = _width;
        height = _height;
        // u32 num_components = GetNumComponents(_format);
        ifor(2) results[i] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var gid = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];

            var g_rw_result  = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_rw_result"));
            var g_input      = ResourceAccess(Resource::Create(Texture2D_f32x4_Ty, "g_input"));
            var g_prev_input = ResourceAccess(Resource::Create(Texture2D_f32x4_Ty, "g_prev_input"));
            var dim          = u32x2(width, height); // g_rw_result.GetDimensions().Swizzle("xy");

            // var disocc     = g_disocclusion.Load(tid);
            var uv         = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var velocity   = g_velocity.Load(tid);
            var tracked_uv = uv - velocity;

            var cur = g_input.Load(tid);
            EmitIfElse(
                /*disocc > f32(0.5) && */ (tracked_uv > f32x2(0.0, 0.0)).All() && (tracked_uv < f32x2(1.0, 1.0)).All(),
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
                            var rN = g_prev_gbuffer_world_normals.Load(uv_lo + u32x2(x, y));
                            var rP = g_prev_gbuffer_world_position.Load(uv_lo + u32x2(x, y));
                            var w  = GetWeight(N, P, rN, rP, eps);
                            EmitIfElse(w > f32(0.8), [&] {
                                var weight = bilinear_weights[y][x] * w;
                                prev_acc += weight * g_prev_input.Load(uv_lo + u32x2(x, y));
                                weight_acc += weight;
                            });
                        }
                    }
                    EmitIfElse(
                        weight_acc > f32(0.5), //
                        [&] {
                            var prev            = prev_acc / max(f32(1.0e-5), weight_acc);
                            var num_samples     = prev.w();
                            var new_num_samples = min(f32(32.0), num_samples + f32(1.0));
                            var history_weight  = f32(1.0) - f32(1.0) / new_num_samples;
                            var mix             = lerp(cur, prev, history_weight);
                            g_rw_result.Store(tid, make_f32x4(mix.xyz(), new_num_samples));
                        },
                        [&] { //
                            g_rw_result.Store(tid, make_f32x4(cur.xyz(), f32(1.0)));
                        });
                },
                [&] { g_rw_result.Store(tid, make_f32x4(cur.xyz(), f32(1.0))); });

            kernel = CompileGlobalModule(gfx, "TemporalFilter");
        }
    }
    void Execute(GfxTexture input, GfxTexture prev) {
        ping_pong.Next();
        kernel.SetResource("g_rw_result", results[ping_pong.ping]);
        kernel.SetResource("g_prev_input", prev);
        kernel.SetResource("g_input", input);
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
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
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
            var g_input     = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_input"));
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
                val.y()         = in.x().AsU32();
                val.z()         = in.w().AsU32();
                return val;
            });

            EmitGroupSync();

            var l               = lds.Load(linear_idx(gid_center));
            var src_ao          = l.y().AsF32();
            var src_num_samples = l.z().AsF32();
            var value_acc       = make_f32x2(l.y().AsF32(), src_num_samples);
            var weigth_acc      = var(value_acc.y()).Copy();
            value_acc *= weigth_acc;
            var uv             = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var ray            = GenCameraRay(uv);
            var xi             = GetNoise(tid);
            var center_gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());
            var eps            = GetEps(center_gbuffer["P"]);

            var halton_sample_offsets = MakeStaticArray(halton_samples);

            // EmitIfElse(value_acc.y() < f32(16.0), [&] {
            EmitForLoop(i32(0), i32(halton_sample_count), [&](var iter) {
                var soffset = halton_sample_offsets[iter];
                EmitIfElse((g_frame_idx & u32(1)) != u32(0), [&] { soffset.xy() = soffset.yx(); });
                var l       = lds.Load(linear_idx(gid_center.ToI32() + soffset));
                var uv      = (tid.ToF32() + halton_sample_offsets[iter].ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                var ray     = GenCameraRay(uv);
                var xi      = GetNoise(tid);
                var gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());

                var weight = GetWeight(center_gbuffer["N"], center_gbuffer["P"], gbuffer["N"], gbuffer["P"], eps);
                weight *= l.z().AsF32() * Gaussian(length(soffset.ToF32()) * f32(0.25));
                var val = make_f32x2(l.y().AsF32(), l.z().AsF32());

                value_acc += weight * val;
                weigth_acc += weight;
            });
            //});
            value_acc /= weigth_acc;

            var dst_ao          = value_acc.x();
            var dst_num_samples = value_acc.y();

            var final_ao = (src_ao * src_num_samples + dst_ao * dst_num_samples) / max(f32(1.0e-3), src_num_samples + dst_num_samples);

            g_rw_result.Store(tid, make_f32x4(final_ao["xxx"], dst_num_samples));
            // g_rw_result.Store(tid, make_f32x4(src_ao["xxx"], dst_num_samples));

            // var num_samples = g_input.Load(tid).w();
            // EmitIfElse(
            //    l.z().AsF32() < f32(16.0),                                                     //
            //    [&] { g_rw_result.Store(tid, make_f32x4(value_acc["xxx"], value_acc["y"])); }, //
            //    [&] { g_rw_result.Store(tid, make_f32x4(l.y().AsF32()["xxx"], value_acc["y"])); });
            // g_rw_result.Store(tid, make_f32x4(l.y().AsF32()["xxx"], value_acc["y"]));

            // fprintf(stdout, GetGlobalModule().Finalize());

            kernel = CompileGlobalModule(gfx, "SpatialFilter");
        }
    }
    void Execute(GfxTexture input) {
        ping_pong.Next();
        kernel.SetResource("g_rw_result", results[ping_pong.ping]);
        kernel.SetResource("g_input", input);
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
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
    }
};
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
            var dim            = u32x2(width, height);
            var input          = g_input.Load(tid);
            var uv             = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var xi             = GetNoise(tid);
            var ray            = GenCameraRay(uv);
            var center_gbuffer = DecodeGBuffer32Bits(ray, g_gbuffer_encoded.Load(tid), xi.x());
            var eps            = GetEps(center_gbuffer["P"]);
            var num_samples    = u32(2);
            var fstride        = lerp(f32(16.0), f32(0.0), saturate(input.w() / f32(16.0)));
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
                        // weight *= Gaussian(length(soffset.ToF32()));
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
    void Execute(GfxTexture input) {
        {
            kernels[0].SetResource("g_rw_result", results[0]);
            kernels[0].SetResource("g_input", input);
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
class TemporalFilterFinal {
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

    SJIT_DONT_MOVE(TemporalFilterFinal);
    ~TemporalFilterFinal() {
        kernel.Destroy();
        ifor(2) gfxDestroyTexture(gfx, results[i]);
    }
    TemporalFilterFinal(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        //, DXGI_FORMAT _format = DXGI_FORMAT_R16G16B16A16_FLOAT
        gfx    = _gfx;
        width  = _width;
        height = _height;
        // u32 num_components = GetNumComponents(_format);
        ifor(2) results[i] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var gid = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];

            var g_rw_result  = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_rw_result"));
            var g_input      = ResourceAccess(Resource::Create(Texture2D_f32x4_Ty, "g_input"));
            var g_prev_input = ResourceAccess(Resource::Create(Texture2D_f32x4_Ty, "g_prev_input"));
            var dim          = u32x2(width, height); // g_rw_result.GetDimensions().Swizzle("xy");

            var  lds        = AllocateLDS(u32x3Ty, u32(16 * 16), "lds_values");
            var  gid_center = gid.xy() + u32x2(4, 4);
            auto linear_idx = [](var xy) { return (xy.x().ToI32() + xy.y().ToI32() * i32(16)).ToU32(); };
            var  group_tid  = u32(8) * (tid / u32(8));

            Init_LDS_16x16(lds, [&](var src_coord) {
                var in  = g_input.Load(src_coord);
                var val = Zero(u32x2Ty).Copy();
                return in.xyz().AsU32();
            });

            EmitGroupSync();

            var mean  = Make(f32x3Ty);
            var mean2 = Make(f32x3Ty);
            for (i32 y = i32(-1); y <= i32(1); y++) {
                for (i32 x = i32(-1); x <= i32(1); x++) {
                    var l   = lds.Load(linear_idx(gid_center.ToI32() + i32x2(x, y)));
                    var val = l.xyz().ToF32();
                    mean += val;
                    mean2 += val * val;
                }
            }
            var variance = sqrt(abs(mean * mean - mean2));

            // var disocc     = g_disocclusion.Load(tid);
            var uv         = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var velocity   = g_velocity.Load(tid);
            var tracked_uv = uv - velocity;

            var cur = g_input.Load(tid);
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
                                   var rN = g_prev_gbuffer_world_normals.Load(uv_lo + u32x2(x, y));
                                   var rP = g_prev_gbuffer_world_position.Load(uv_lo + u32x2(x, y));
                                   var w  = GetWeight(N, P, rN, rP, eps);
                                   EmitIfElse(w > f32(0.8), [&] {
                                       var weight = bilinear_weights[y][x] * w;
                                       prev_acc += weight * g_prev_input.Load(uv_lo + u32x2(x, y));
                                       weight_acc += weight;
                                   });
                               }
                           }
                           EmitIfElse(
                               weight_acc > f32(1.0e-3), //
                               [&] {
                                   var prev           = prev_acc / max(f32(1.0e-5), weight_acc);
                                   var history_weight = f32(0.5);
                                   var aabb_size      = variance.x() / f32(4.0);
                                   prev.xyz() =
                                       clamp(prev.xyz(), prev.xyz() - make_f32x3(aabb_size, aabb_size, aabb_size), prev.xyz() + make_f32x3(aabb_size, aabb_size, aabb_size));
                                   var mix = lerp(cur, prev, history_weight);
                                   g_rw_result.Store(tid, mix);
                               },
                               [&] { //
                                   g_rw_result.Store(tid, make_f32x4(cur.xyz(), f32(1.0)));
                               });
                       },
                       [&] { g_rw_result.Store(tid, make_f32x4(cur.xyz(), f32(1.0))); });

            kernel = CompileGlobalModule(gfx, "TemporalFilter");
        }
    }
    void Execute(GfxTexture input) {
        ping_pong.Next();
        kernel.SetResource("g_rw_result", results[ping_pong.ping]);
        kernel.SetResource("g_prev_input", results[ping_pong.pong]);
        kernel.SetResource("g_input", input);
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
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
    }
};
class Raw_GGX_ReflectionsPass {
private:
    GfxContext gfx        = {};
    GPUKernel  kernel     = {};
    GfxTexture radiance   = {};
    GfxTexture ray_length = {};
    GfxTexture confidence = {};
    u32        width      = u32(0);
    u32        height     = u32(0);

    var g_rw_radiance   = ResourceAccess(Resource::Create(RWTexture2D_f32x3_Ty, "g_rw_radiance"));
    var g_rw_ray_length = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_ray_length"));
    var g_rw_confidence = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_confidence"));

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return radiance; }

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
        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

        var dim = u32x2(width, height);

        var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
        EmitIfElse((tid < dim).All(), [&] {
            var xi = GetNoise(tid);
            var N  = g_gbuffer_world_normals.Load(tid);
            var P  = g_gbuffer_world_position.Load(tid);

            EmitIfElse((N == f32x3_splat(0.0)).All(), [&] {
                g_rw_radiance.Store(tid, f32x3_splat(0.0));
                g_rw_confidence.Store(tid, f32(0.0));
                g_rw_ray_length.Store(tid, f32(0.0));
                EmitReturn();
            });

            var ray_query = TraceGGX(N, P, f32(0.1), xi);

            EmitIfElse(
                ray_query["hit"],
                [&] {
                    var hit        = GetHit(ray_query);
                    var w          = hit["W"];
                    var ray_length = length(w - P);
                    var n          = hit["N"];
                    var l          = GetSunShadow(w, n);
                    var gi         = SampleDDGIProbe(w, n);
                    var c          = random_albedo(ray_query["instance_id"].ToF32());
                    g_rw_radiance.Store(tid, (gi + l["xxx"]) * c);
                    g_rw_confidence.Store(tid, ray_length);
                    g_rw_ray_length.Store(tid, f32(1.0));
                },
                [&] {
                    g_rw_radiance.Store(tid, f32x3_splat(0.0));
                    g_rw_confidence.Store(tid, f32(0.0));
                    g_rw_ray_length.Store(tid, f32(0.0));
                });
        });

        kernel = CompileGlobalModule(gfx, "Raw_GGX_ReflectionsPass");
    }
    void Execute() {
        kernel.SetResource(g_rw_radiance->resource->GetName().c_str(), radiance);
        kernel.SetResource(g_rw_ray_length->resource->GetName().c_str(), ray_length);
        kernel.SetResource(g_rw_confidence->resource->GetName().c_str(), confidence);
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
class Raw_PerPixelGI {
private:
    GfxContext gfx        = {};
    GPUKernel  kernel     = {};
    GfxTexture radiance   = {};
    GfxTexture ray_length = {};
    GfxTexture confidence = {};
    u32        width      = u32(0);
    u32        height     = u32(0);

    var g_rw_radiance   = ResourceAccess(Resource::Create(RWTexture2D_f32x3_Ty, "g_rw_radiance"));
    var g_rw_ray_length = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_ray_length"));
    var g_rw_confidence = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_confidence"));
    var g_ray_length    = ResourceAccess(Resource::Create(f32Ty, "g_ray_length"));

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return radiance; }

    SJIT_DONT_MOVE(Raw_PerPixelGI);
    ~Raw_PerPixelGI() {
        kernel.Destroy();
        gfxDestroyTexture(gfx, radiance);
        gfxDestroyTexture(gfx, ray_length);
        gfxDestroyTexture(gfx, confidence);
    }
    Raw_PerPixelGI(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;
        radiance    = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R11G11B10_FLOAT);
        ray_length  = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16_FLOAT);
        confidence  = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8_UNORM);
        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

        var dim = u32x2(width, height);

        var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
        EmitIfElse((tid < dim).All(), [&] {
            var xi = GetNoise(tid);
            var N  = g_gbuffer_world_normals.Load(tid);
            var P  = g_gbuffer_world_position.Load(tid);

            EmitIfElse((N == f32x3_splat(0.0)).All(), [&] {
                g_rw_radiance.Store(tid, f32x3_splat(0.0));
                g_rw_confidence.Store(tid, f32(0.0));
                g_rw_ray_length.Store(tid, f32(0.0));
                EmitReturn();
            });

            var diffuse_ray       = GenDiffuseRay(P, N, xi);
            var ray_desc          = Zero(RayDesc_Ty);
            ray_desc["Direction"] = diffuse_ray["d"];
            ray_desc["Origin"]    = diffuse_ray["o"];
            ray_desc["TMin"]      = f32(1.0e-3);
            ray_desc["TMax"]      = g_ray_length;
            var ray_query         = RayQuery(g_tlas, ray_desc);

            EmitIfElse(
                ray_query["hit"],
                [&] {
                    var hit        = GetHit(ray_query);
                    var w          = hit["W"];
                    var ray_length = length(w - P);
                    var n          = hit["N"];
                    var l          = GetSunShadow(w, n);
                    var gi         = SampleDDGIProbe(w, n);
                    var c          = random_albedo(ray_query["instance_id"].ToF32());
                    g_rw_radiance.Store(tid, (gi + l["xxx"]) * c);
                    g_rw_confidence.Store(tid, ray_length);
                    g_rw_ray_length.Store(tid, f32(1.0));
                },
                [&] {
                    g_rw_radiance.Store(tid, f32x3_splat(0.0));
                    g_rw_confidence.Store(tid, f32(0.0));
                    g_rw_ray_length.Store(tid, f32(0.0));
                });
        });

        kernel = CompileGlobalModule(gfx, "Raw_PerPixelGI");
    }
    void Execute(f32 _ray_length) {
        kernel.SetResource(g_ray_length->resource->GetName().c_str(), _ray_length);
        kernel.SetResource(g_rw_radiance->resource->GetName().c_str(), radiance);
        kernel.SetResource(g_rw_ray_length->resource->GetName().c_str(), ray_length);
        kernel.SetResource(g_rw_confidence->resource->GetName().c_str(), confidence);
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
class ReflectionsReprojectPass {
private:
    GfxContext gfx        = {};
    GPUKernel  kernel     = {};
    PingPong   ping_pong  = {};
    GfxTexture results[2] = {};
    u32        width      = u32(0);
    u32        height     = u32(0);

    var g_rw_result  = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_rw_result"));
    var g_input      = ResourceAccess(Resource::Create(Texture2D_f32x4_Ty, "g_input"));
    var g_prev_input = ResourceAccess(Resource::Create(Texture2D_f32x4_Ty, "g_prev_input"));

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return results[ping_pong.ping]; }

    SJIT_DONT_MOVE(ReflectionsReprojectPass);
    ~ReflectionsReprojectPass() {
        kernel.Destroy();
        ifor(2) gfxDestroyTexture(gfx, results[i]);
    }
    ReflectionsReprojectPass(GfxContext _gfx) {
        u32 _width         = gfxGetBackBufferWidth(_gfx);
        u32 _height        = gfxGetBackBufferHeight(_gfx);
        gfx                = _gfx;
        width              = _width;
        height             = _height;
        ifor(2) results[i] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var  tid        = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var  gid        = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];
            var  dim        = u32x2(width, height);
            var  disocc     = g_disocclusion.Load(tid);
            var  uv         = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var  velocity   = g_velocity.Load(tid);
            var  tracked_uv = uv - velocity;
            var  cur        = g_input.Load(tid);
            var  lds        = AllocateLDS(u32x3Ty, u32(16 * 16), "lds_values");
            auto linear_idx = [](var xy) { return (xy.x().ToI32() + xy.y().ToI32() * i32(16)).ToU32(); };
            var  group_tid  = u32(8) * (tid / u32(8));

            EmitGroupSync();

            EmitIfElse(
                disocc > f32(0.5) && (tracked_uv > f32x2(0.0, 0.0)).All() && (tracked_uv < f32x2(1.0, 1.0)).All(),
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
                            var weight = bilinear_weights[y][x] * GetWeight(N, P, rN, rP, eps);
                            prev_acc += weight * g_prev_input.Load(uv_lo + u32x2(x, y));
                            weight_acc += weight;
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
    void Execute(GfxTexture input) {
        ping_pong.Next();
        kernel.SetResource("g_rw_result", results[ping_pong.ping]);
        kernel.SetResource("g_prev_input", results[ping_pong.pong]);
        kernel.SetResource("g_input", input);
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
            g_output.Store(tid, MakeIfElse(anyhit, f32x4_splat(0.0), f32x4_splat(1.0)));
        });

        // fprintf(stdout, GetGlobalModule().Finalize());

        kernel = CompileGlobalModule(gfx, "AOPass");
    }
    void Execute(f32 ray_length) {
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
class Shade {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    u32        width  = u32(0);
    u32        height = u32(0);

    var g_output = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_output"));

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
            var ao                  = g_ao.Load(tid);
            var visibility          = g_visibility_buffer.Load(tid);
            var barys               = visibility.xy().AsF32();
            var instance_idx        = visibility.z();
            var primitive_idx       = visibility.w();
            var l                   = GetSunShadow(P, N);
            var indirect_irradiance = g_diffuse_gi.Load(tid);
            var c                   = random_albedo(instance_idx.ToF32());
            var irradiance          = l["xxx"] + indirect_irradiance;
            var color               = c * irradiance;
            color                   = pow(color, f32(1.0) / f32(2.2));
            g_output.Store(tid, make_f32x4(color, f32(1.0)));
            // g_output.Store(tid, make_f32x4(pow(gi, f32(1.0) / f32(2.2)), f32(1.0)));
        });

        kernel = CompileGlobalModule(gfx, "Shade");

        // fprintf(stdout, kernel.isa.c_str());
    }
    void Execute(GfxTexture result) {
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
    PASS(AOPass, ao_pass)                                                                                                                                                          \
    PASS(Raw_PerPixelGI, raw_per_pixel_gi)                                                                                                                                         \
    PASS(TemporalFilter, temporal_filter)                                                                                                                                          \
    PASS(EncodeGBuffer, encode_gbuffer)                                                                                                                                            \
    PASS(SpatialFilter, spatial_filter)                                                                                                                                            \
    PASS(Discclusion, disocclusion)                                                                                                                                                \
    PASS(PreFilterAO, prefilter_ao)                                                                                                                                                \
    PASS(GBufferFromVisibility, gbuffer_from_vis)                                                                                                                                  \
    PASS(PrimaryRays, primary_rays)                                                                                                                                                \
    PASS(SpatialFilterLarge, spatial_filter_large)                                                                                                                                 \
    PASS(DDGI, ddgi)                                                                                                                                                               \
    PASS(NearestVelocity, nearest_velocity)                                                                                                                                        \
    PASS(Shade, shade)                                                                                                                                                             \
    PASS(TemporalFilterFinal, temporal_filter_final)                                                                                                                               \
    PASS(Raw_GGX_ReflectionsPass, reflections)                                                                                                                                     \
    PASS(ReflectionsReprojectPass, reflections_reproject)

#define PASS(t, n) UniquePtr<t> n = {};
    PASS_LIST
#undef PASS

    u32  frame_idx    = u32(0);
    bool render_gizmo = false;
    bool debug_probe  = false;

    GfxDrawState ddgi_probe_draw_state = {};
    GfxProgram   ddgi_probe_program    = {};
    GfxKernel    ddgi_probe_kernel     = {};

    void InitChild() override {}
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

        g_global_runtime_resource_registry                                                   = {};
        g_global_runtime_resource_registry[g_frame_idx->GetResource()->GetName()]            = frame_idx;
        g_global_runtime_resource_registry[g_ddgi_radiance_probes->GetResource()->GetName()] = ddgi->GetRadianceProbeAtlas(); // Texture3D_f32x4_Ty);
        g_global_runtime_resource_registry[g_ddgi_distance_probes->GetResource()->GetName()] = ddgi->GetDistanceProbeAtlas(); // Texture3D_f32x4_Ty);
        g_global_runtime_resource_registry[g_ddgi_cascade_min->GetResource()->GetName()]     = ddgi->GetLo();                 // f32x3Ty);
        g_global_runtime_resource_registry[g_ddgi_cascade_max->GetResource()->GetName()]     = ddgi->GetHi();                 // f32x3Ty);
        g_global_runtime_resource_registry[g_ddgi_cascade_dim->GetResource()->GetName()] = f32x3(ddgi->GetNumProbesX(), ddgi->GetNumProbesY(), ddgi->GetNumProbesZ()); // f32x3Ty);
        g_global_runtime_resource_registry[g_ddgi_cascade_spacing->GetResource()->GetName()] = ddgi->GetSpacing();                                                     // f32Ty);

        g_global_runtime_resource_registry[g_tlas->GetResource()->GetName()]                    = gpu_scene.acceleration_structure;
        g_global_runtime_resource_registry[g_linear_sampler->GetResource()->GetName()]          = linear_sampler;
        g_global_runtime_resource_registry[g_nearest_sampler->GetResource()->GetName()]         = nearest_sampler;
        g_global_runtime_resource_registry[g_velocity->GetResource()->GetName()]                = velocity_buffer;
        g_global_runtime_resource_registry[g_noise_texture->GetResource()->GetName()]           = blue_noise_baker.GetTexture();
        g_global_runtime_resource_registry[g_MeshBuffer->GetResource()->GetName()]              = gpu_scene.mesh_buffer;
        g_global_runtime_resource_registry[g_IndexBuffer->GetResource()->GetName()]             = gpu_scene.index_buffer;
        g_global_runtime_resource_registry[g_VertexBuffer->GetResource()->GetName()]            = gpu_scene.vertex_buffer;
        g_global_runtime_resource_registry[g_InstanceBuffer->GetResource()->GetName()]          = gpu_scene.instance_buffer;
        g_global_runtime_resource_registry[g_MaterialBuffer->GetResource()->GetName()]          = gpu_scene.material_buffer;
        g_global_runtime_resource_registry[g_TransformBuffer->GetResource()->GetName()]         = gpu_scene.transform_buffer;
        g_global_runtime_resource_registry[g_PreviousTransformBuffer->GetResource()->GetName()] = gpu_scene.previous_transform_buffer;
        g_global_runtime_resource_registry[g_Textures->GetResource()->GetName()]                = ResourceSlot(gpu_scene.textures.data(), (uint32_t)gpu_scene.textures.size());
        // g_global_runtime_resource_registry[g_TextureSampler->GetResource()->GetName()]          = gpu_scene.texture_sampler;
        g_global_runtime_resource_registry[g_visibility_buffer->GetResource()->GetName()]   = visibility_buffer;
        g_global_runtime_resource_registry[g_camera_pos->GetResource()->GetName()]          = g_camera.pos;
        g_global_runtime_resource_registry[g_camera_look->GetResource()->GetName()]         = g_camera.look;
        g_global_runtime_resource_registry[g_camera_up->GetResource()->GetName()]           = g_camera.up;
        g_global_runtime_resource_registry[g_camera_right->GetResource()->GetName()]        = g_camera.right;
        g_global_runtime_resource_registry[g_camera_fov->GetResource()->GetName()]          = g_camera.fov;
        g_global_runtime_resource_registry[g_camera_aspect->GetResource()->GetName()]       = g_camera.aspect;
        g_global_runtime_resource_registry[g_sun_shadow_matrices->GetResource()->GetName()] = sun.GetMatrixBuffer();
        g_global_runtime_resource_registry[g_sun_shadow_maps->GetResource()->GetName()]     = ResourceSlot(sun.GetTextures().data(), (uint32_t)sun.GetTextures().size());
        g_global_runtime_resource_registry[g_sun_dir->GetResource()->GetName()]             = sun.GetDir();

        gbuffer_from_vis->Execute();
        g_global_runtime_resource_registry[g_gbuffer_world_normals->GetResource()->GetName()]       = gbuffer_from_vis->GetNormals();
        g_global_runtime_resource_registry[g_gbuffer_world_position->GetResource()->GetName()]      = gbuffer_from_vis->GetWorldPosition();
        g_global_runtime_resource_registry[g_prev_gbuffer_world_normals->GetResource()->GetName()]  = gbuffer_from_vis->GetPrevNormals();
        g_global_runtime_resource_registry[g_prev_gbuffer_world_position->GetResource()->GetName()] = gbuffer_from_vis->GetPrevWorldPosition();

        encode_gbuffer->Execute();
        disocclusion->Execute();
        nearest_velocity->Execute();

        raw_per_pixel_gi->Execute(ddgi->GetSpacing());
        primary_rays->Execute();

        if (render_gizmo) ddgi->PushGizmos(gizmo_manager);

        g_global_runtime_resource_registry[g_nearest_velocity->GetResource()->GetName()] = nearest_velocity->GetResult();

        g_global_runtime_resource_registry["g_disocclusion"] = disocclusion->GetDisocclusion();

        g_global_runtime_resource_registry["g_gbuffer_encoded"] = encode_gbuffer->GetResult();

        reflections->Execute();
        reflections_reproject->Execute(reflections->GetResult());

        ao_pass->Execute(ddgi->GetSpacing());
        prefilter_ao->Execute(ao_pass->GetResult());
        temporal_filter->Execute(prefilter_ao->GetResult(), spatial_filter_large->GetResult());
        spatial_filter->Execute(temporal_filter->GetResult());
        spatial_filter_large->Execute(spatial_filter->GetResult());
        temporal_filter_final->Execute(spatial_filter_large->GetResult());

        g_global_runtime_resource_registry[g_ao->GetResource()->GetName()] = temporal_filter_final->GetResult();

        ddgi->Execute();

        g_global_runtime_resource_registry[g_diffuse_gi->GetResource()->GetName()] = ddgi->GetDiffuseGI();

        shade->Execute(color_buffer);

        if (debug_probe) {

            std::vector<f32x4> instance_infos = {};

            zfor(ddgi->GetNumProbesZ()) {
                yfor(ddgi->GetNumProbesY()) {
                    xfor(ddgi->GetNumProbesX()) {
                        f32x3 p    = (f32x3(x, y, z) + f32x3(0.5, 0.5, 0.5)) * ddgi->GetSpacing() + ddgi->GetLo();
                        f32   size = f32(0.05);
                        instance_infos.push_back(f32x4(p.x, p.y, p.z, size));
                    }
                }
            }

            u32x3 probe_cursor = {};
            if (all(glm::greaterThan(g_camera.look_at, ddgi->GetLo())) && all(glm::lessThan(g_camera.look_at, ddgi->GetHi()))) {
                f32x3 rp     = (g_camera.look_at - ddgi->GetLo()) / ddgi->GetSpacing();
                u32x3 irp    = u32x3(rp);
                probe_cursor = irp;

                f32x3 p    = (f32x3(probe_cursor) + f32x3(0.5, 0.5, 0.5)) * ddgi->GetSpacing() + ddgi->GetLo();
                f32   size = f32(ddgi->GetSpacing()) / f32(2.0);
                gizmo_manager.AddLineAABB(p - f32x3_splat(size), p + f32x3_splat(size), f32x3(1.0, 0.0, 0.0));
            }

            GfxUploadBuffer::Allocation device_memory = upload_buffer.Allocate(instance_infos.size() * sizeof(instance_infos[0]));
            upload_buffer.DeferFree(device_memory);
            assert(device_memory.IsValid());
            device_memory.CopyIn(instance_infos);

            gfxCommandBindKernel(gfx, ddgi_probe_kernel);
            gfxCommandBindVertexBuffer(gfx, gizmo_manager.icosahedron_wrapper_x2.vertex_buffer, /* index */ 0, /* offset */ 0, /* stride */ u64(12));
            gfxCommandBindVertexBuffer(gfx, device_memory.buffer, /* index */ 1, /* offset */ device_memory.device_offset, /* stride */ u64(16));
            gfxCommandBindIndexBuffer(gfx, gizmo_manager.icosahedron_wrapper_x2.index_buffer);
            gfxProgramSetParameter(gfx, ddgi_probe_program, "g_ViewProjection", transpose(g_camera.view_proj));
            gfxProgramSetParameter(gfx, ddgi_probe_program, "g_probe_cursor", probe_cursor);

            gfxProgramSetParameter(gfx, ddgi_probe_program, g_linear_sampler->GetResource()->GetName().c_str(), linear_sampler);
            gfxProgramSetParameter(gfx, ddgi_probe_program, g_ddgi_radiance_probes->GetResource()->GetName().c_str(), ddgi->GetRadianceProbeAtlas());
            gfxProgramSetParameter(gfx, ddgi_probe_program, g_ddgi_distance_probes->GetResource()->GetName().c_str(), ddgi->GetDistanceProbeAtlas());
            gfxProgramSetParameter(gfx, ddgi_probe_program, g_ddgi_cascade_min->GetResource()->GetName().c_str(), ddgi->GetLo());
            gfxProgramSetParameter(gfx, ddgi_probe_program, g_ddgi_cascade_max->GetResource()->GetName().c_str(), ddgi->GetHi());
            gfxProgramSetParameter(gfx, ddgi_probe_program, g_ddgi_cascade_dim->GetResource()->GetName().c_str(),
                                   f32x3(ddgi->GetNumProbesX(), ddgi->GetNumProbesY(), ddgi->GetNumProbesZ()));
            gfxProgramSetParameter(gfx, ddgi_probe_program, g_ddgi_cascade_spacing->GetResource()->GetName().c_str(), ddgi->GetSpacing());

            gfxCommandDrawIndexed(gfx, gizmo_manager.icosahedron_wrapper_x2.num_indices, u32(instance_infos.size()), u32(0), u32(0), u32(0));
        }

        g_global_runtime_resource_registry[g_color_buffer->GetResource()->GetName()] = color_buffer;

        // LaunchKernel(gfx, {128/8, 128/8, 1}, [&] {
        //     var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
        //     g_color_buffer.Store(tid, f32x4(1.0, 0.0, 0.0, 1.0));
        // });
        static bool slow_down = false;
        if (slow_down) Sleep(100);
        ImGui::Begin("AO");
        {
            ImVec2 wsize = GetImGuiSize();
            wsize.y      = wsize.x;

            ImGui::Text("ao");
            ImGui::Image((ImTextureID)&ao_pass->GetResult(), wsize);
            ImGui::Text("prefilter_ao");
            ImGui::Image((ImTextureID)&prefilter_ao->GetResult(), wsize);
            ImGui::Text("temporal_filter");
            ImGui::Image((ImTextureID)&temporal_filter->GetResult(), wsize);
            ImGui::Text("spatial_filter");
            ImGui::Image((ImTextureID)&spatial_filter->GetResult(), wsize);
            ImGui::Text("spatial_filter_large");
            ImGui::Image((ImTextureID)&spatial_filter_large->GetResult(), wsize);
        }
        ImGui::End();

        ImGui::Begin("DDGI");
        {
            ImVec2 wsize = GetImGuiSize();
            wsize.y      = wsize.x;

            ImGui::Text("DDGI");
            static u32 slice = u32(0);
            ImGui::DragInt("Slice", (int *)&slice);
            slice                             = std::max(u32(0), std::min(u32(ddgi->GetRadianceProbeAtlas().getDepth()) - u32(1), slice));
            GfxImguiTextureParameters &config = GfxImguiTextureParameters::GetConfig()[&ddgi->GetRadianceProbeAtlas()];
            config.slice                      = slice;
            config.disable_alpha              = true;
            ImGui::Image((ImTextureID)&ddgi->GetRadianceProbeAtlas(), wsize);

            GfxImguiTextureParameters &dist_config = GfxImguiTextureParameters::GetConfig()[&ddgi->GetDistanceProbeAtlas()];
            dist_config.slice                      = slice;
            dist_config.disable_alpha              = true;
            ImGui::Image((ImTextureID)&ddgi->GetDistanceProbeAtlas(), wsize);
        }
        ImGui::End();

        ImGui::Begin("raw_per_pixel_gi");
        {
            ImVec2 wsize = GetImGuiSize();
            wsize.y      = wsize.x;
            ImGui::Text("raw_per_pixel_gi");
            ImGui::Image((ImTextureID)&raw_per_pixel_gi->GetResult(), wsize);
        }
        ImGui::End();

        ImGui::Begin("Config");
        {
            ImGui::Checkbox("Slow down", &slow_down);
            ImGui::Checkbox("Render Gizmo", &render_gizmo);
            ImGui::Checkbox("Debug Probe", &debug_probe);
            ImVec2 wsize = GetImGuiSize();
            wsize.y      = wsize.x;

            ImGui::Text("Reflections");
            ImGui::Image((ImTextureID)&reflections->GetResult(), wsize);
            ImGui::Image((ImTextureID)&reflections_reproject->GetResult(), wsize);

            {}
            ImGui::Text("DiffuseGI");
            ImGui::Image((ImTextureID)&ddgi->GetDiffuseGI(), wsize);
            ImGui::Text("Normals");
            ImGui::Image((ImTextureID)&gbuffer_from_vis->GetNormals(), wsize);
            ImGui::Text("nearest_velocity");
            ImGui::Image((ImTextureID)&nearest_velocity->GetResult(), wsize);
            ImGui::Text("Disocclusion");
            ImGui::Image((ImTextureID)&disocclusion->GetDisocclusion(), wsize);
        }
        ImGui::End();
    }
    GfxTexture GetResult() override { return color_buffer; }
    // GfxTexture GetResult() override { return temporal_filter_final->GetResult(); }
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
