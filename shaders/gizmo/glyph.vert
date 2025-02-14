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

f32x2 glyph_uv_size;
f32x2 glyph_size;
f32x2 viewport_size;

struct PSInput {
    f32x4 pos : SV_POSITION;
    f32x2 uv : TEXCOORD0;
    f32x3 color : TEXCOORD1;
};

struct VSInput {
    f32x3 vertex_position : POSITION;
    f32x3 instance_offset : TEXCOORD0;
    f32x2 instance_uv_offset : TEXCOORD1;
    f32x3 instance_color : TEXCOORD2;
};

PSInput main(VSInput input) {
    PSInput output;
    output.color   = input.instance_color;
    output.uv      = input.instance_uv_offset + (input.vertex_position.xy * float2(1.0, 1.0) + float2(0.0, 0.0)) * glyph_uv_size;
    float4 sspos   = float4(input.vertex_position.xy * glyph_size + input.instance_offset.xy, 0.0, 1.0);
    int    pixel_x = int(viewport_size.x * (sspos.x * 0.5 + 0.5));
    int    pixel_y = int(viewport_size.y * (sspos.y * 0.5 + 0.5));
    sspos.x        = 2.0 * (f32(pixel_x)) / viewport_size.x - 1.0;
    sspos.y        = 2.0 * (f32(pixel_y)) / viewport_size.y - 1.0;
    sspos.z        = input.instance_offset.z;
    output.pos     = sspos;
    return output;
}