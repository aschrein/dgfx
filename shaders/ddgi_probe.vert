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

float4x4 g_ViewProjection;
// float3   g_offset;
// float g_scale;

Texture3D<f32x4> g_ddgi_radiance_probes;
Texture3D<f32x2> g_ddgi_distance_probes;

SamplerState g_linear_sampler;

f32x3 g_ddgi_cascade_min;
f32x3 g_ddgi_cascade_max;
f32x3 g_ddgi_cascade_dim;
u32x3 g_probe_cursor;
f32   g_ddgi_cascade_spacing;

struct VSInput {
    f32x3 vertex_position : POSITION;
    f32x4 instance_offset_scale : TEXCOORD0;
    uint  InstanceId : SV_InstanceID;
};

struct Params {
    float4 position : SV_Position;
    float3 src_pos : TEXCOORD0;
    float3 src_offset : TEXCOORD1;
    f32    InstanceId : TEXCOORD2;
};

Params main(VSInput input) {
    f32x3 position = input.instance_offset_scale.xyz + input.vertex_position * input.instance_offset_scale.w;

#if 1
    f32x3 dir = normalize(input.vertex_position);
    f32x2 uv  = Octahedral::Encode(dir);
    f32x3 p   = input.instance_offset_scale.xyz;
    if (all(p > g_ddgi_cascade_min) && all(p < g_ddgi_cascade_max)) {
        f32x3 rp = (p - g_ddgi_cascade_min) / g_ddgi_cascade_spacing;
        u32x3 ip = u32x3(rp);
        if (all(g_probe_cursor == ip)) {
            ip.xyz        = ip.xzy;
            f32x2 suv     = lerp(f32x2_splat(1.0) / f32x2_splat(16.0), f32x2_splat(15.0) / f32x2_splat(16.0), saturate(uv));
            f32x3 full_uv = f32x3((suv + f32x2(ip.xy)), (f32(ip.z) + f32(0.5))) / g_ddgi_cascade_dim.xyz;
            f32   dist    = g_ddgi_distance_probes.SampleLevel(g_linear_sampler, full_uv, f32(0.0)).x;
            position      = input.instance_offset_scale.xyz + input.vertex_position * dist;
        }
    } else {
    }
#endif

    Params params;
    params.position   = mul(g_ViewProjection, f32x4(position, f32(1.0)));
    params.src_pos    = input.vertex_position;
    params.src_offset = input.instance_offset_scale.xyz;
    params.InstanceId = f32(input.InstanceId);
    return params;
}
