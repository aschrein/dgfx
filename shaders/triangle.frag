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

Texture2D<float4> g_input;
Texture2D<float4> g_ui;
u32               g_tonepam_mode;

// https://www.shadertoy.com/view/lslGzl

static f32 gamma = 2.2;

f32x3 simpleReinhardToneMapping(f32x3 color) {
    f32 exposure = 1.5;
    color *= exposure / (1. + color / exposure);
    color = pow(color, f32(1. / gamma));
    return color;
}

f32x3 lumaBasedReinhardToneMapping(f32x3 color) {
    f32 luma           = dot(color, f32x3(0.2126, 0.7152, 0.0722));
    f32 toneMappedLuma = luma / (1. + luma);
    color *= toneMappedLuma / luma;
    color = pow(color, f32(1. / gamma));
    return color;
}

f32x3 whitePreservingLumaBasedReinhardToneMapping(f32x3 color) {
    f32 white          = 2.;
    f32 luma           = dot(color, f32x3(0.2126, 0.7152, 0.0722));
    f32 toneMappedLuma = luma * (1. + luma / (white * white)) / (1. + luma);
    color *= toneMappedLuma / luma;
    color = pow(color, f32(1. / gamma));
    return color;
}

f32x3 RomBinDaHouseToneMapping(f32x3 color) {
    color = exp(-1.0 / (2.72 * color + 0.15));
    color = pow(color, f32(1. / gamma));
    return color;
}

f32x3 filmicToneMapping(f32x3 color) {
    color = max(f32x3(0.0, 0.0, 0.0), color - f32x3(0.004, 0.004, 0.004));
    color = (color * (6.2 * color + .5)) / (color * (6.2 * color + 1.7) + 0.06);
    return color;
}

f32x3 Uncharted2ToneMapping(f32x3 color) {
    f32 A        = 0.15;
    f32 B        = 0.50;
    f32 C        = 0.10;
    f32 D        = 0.20;
    f32 E        = 0.02;
    f32 F        = 0.30;
    f32 W        = 11.2;
    f32 exposure = 2.;
    color *= exposure;
    color     = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    f32 white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
    color /= white;
    color = pow(color, f32(1. / gamma));
    return color;
}

float4 main(float4 pos : SV_Position) : SV_Target {
    f32x4 ui    = g_ui[u32x2(pos.xy)];
    f32x3 color = g_input[u32x2(pos.xy)].xyz;
    // f32x3 color = lumaBasedReinhardToneMapping(g_input[u32x2(pos.xy)].xyz);
    f32x3 final = lerp(color.xyz, ui.xyz, ui.w);
    return float4(final, 1.0f);
}