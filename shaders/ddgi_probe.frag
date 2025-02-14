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

#include "dgfx/common.h"

Texture3D<f32x4> g_ddgi_radiance_probes;
Texture3D<f32x4> g_ddgi_distance_probes;

SamplerState g_linear_sampler;

f32x3 g_ddgi_cascade_min;
f32x3 g_ddgi_cascade_max;
f32x3 g_ddgi_cascade_dim;
f32   g_ddgi_cascade_spacing;

struct Params {
    float4 position : SV_Position;
    float3 src_pos : TEXCOORD0;
    float3 src_offset : TEXCOORD1;
    f32    InstanceId : TEXCOORD2;
};

struct Result {
    f32x4 color : SV_Target0;
};

Result main(Params params) {
    f32x3 dir   = normalize(params.src_pos);
    f32x2 uv    = Octahedral::Encode(dir);
    f32x3 p     = params.src_offset + f32x3_splat(1.0e-3);
    f32x3 color = f32x3_splat(0.0);
    if (all(p > g_ddgi_cascade_min) && all(p < g_ddgi_cascade_max)) {
        f32x3 rp = (p - g_ddgi_cascade_min) / g_ddgi_cascade_spacing;
        // f32x3 frp = rp - f32x3_splat(0.5);
        // f32x3 frac_p = frac(frp);
        u32x3 ip    = u32x3(rp);
        ip.xyz      = ip.xzy;
        f32x2 suv   = lerp(f32x2_splat(1.0) / f32x2_splat(8.0), f32x2_splat(7.0) / f32x2_splat(8.0), saturate(uv));
        // u32x2 coord = u32x2(suv * f32(8.0)) + ip.xy * u32(8);

        f32x3 full_uv = f32x3((suv + f32x2(ip.xy)), (f32(ip.z) + f32(0.5))) / g_ddgi_cascade_dim.xyz;
        // f32x3 full_uv = f32x3(suv / g_ddgi_cascade_dim.xz + ip.xy * f32x2_splat(8.0) / g_ddgi_cascade_dim.xz, (ip.z + f32(0.5)) / g_ddgi_cascade_dim.y);
        // color       = g_ddgi_radiance_probes[u32x3(coord, u32(ip.z))];
        color = g_ddgi_radiance_probes.SampleLevel(g_linear_sampler, full_uv, f32(0.0)).xyz;
    }

    Result o = (Result)0;
    // o.color    = f32x4(uv, f32(0.0), f32(1.0));
    o.color = f32x4(color, f32(1.0));
    return o;
}
