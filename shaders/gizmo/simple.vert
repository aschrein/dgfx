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

#include "common.h"

f32x4x4 g_viewproj;

struct PSInput {
    f32x4 pos : SV_POSITION;
    f32x4 pixel_color : TEXCOORD0;
};

struct VSInput {
    f32x3 position : POSITION;
    f32x4 m0 : TEXCOORD0;
    f32x4 m1 : TEXCOORD1;
    f32x4 m2 : TEXCOORD2;
    f32x4 m3 : TEXCOORD3;
    f32x4 color : TEXCOORD4;
};

PSInput main(in VSInput input) {
    PSInput output;
    output.pixel_color = input.color;
    output.pos         = mul(g_viewproj, mul(float4(input.position, f32(1.0)), f32x4x4(input.m0, input.m1, input.m2, input.m3)));
    return output;
}