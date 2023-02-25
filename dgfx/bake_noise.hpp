
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

#if !defined(BAKE_NOISE_HPP)
#    define BAKE_NOISE_HPP

#    include "common.h"

namespace lut {
#    include "3rdparty/samplerCPP/samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_256spp.cpp"
};

class BlueNoiseBaker {
    GfxContext gfx                    = {};
    GfxBuffer  sobol_buffer           = {};
    GfxBuffer  ranking_tile_buffer    = {};
    GfxBuffer  scrambling_tile_buffer = {};
    GfxTexture noise_texture          = {};
    GfxProgram program                = {};
    GfxKernel  kernel                 = {};
    u32        idx                    = u32(0);

public:
    void Init(GfxContext _gfx, char const *_shader_path = "shaders/") {
        gfx                    = _gfx;
        sobol_buffer           = gfxCreateBuffer(gfx, sizeof(lut::sobol_256spp_256d), (void *)&lut::sobol_256spp_256d[0]);
        ranking_tile_buffer    = gfxCreateBuffer(gfx, sizeof(lut::rankingTile), (void *)&lut::rankingTile[0]);
        scrambling_tile_buffer = gfxCreateBuffer(gfx, sizeof(lut::scramblingTile), (void *)&lut::scramblingTile[0]);
        noise_texture          = gfxCreateTexture2D(gfx, u32(128), u32(128), DXGI_FORMAT_R8G8_UNORM);
        program                = gfxCreateProgram(gfx, "bake_noise", _shader_path);
        kernel                 = gfxCreateComputeKernel(gfx, program, "bake_noise");
        assert(program);
        assert(kernel);
    }
    GfxTexture GetTexture() { return noise_texture; }
    void       Bake() {
        defer(idx++);
        gfxProgramSetParameter(gfx, program, "g_frame_index", idx);
        gfxProgramSetParameter(gfx, program, "g_sobol_buffer", sobol_buffer);
        gfxProgramSetParameter(gfx, program, "g_ranking_tile_buffer", ranking_tile_buffer);
        gfxProgramSetParameter(gfx, program, "g_scrambling_tile_buffer", scrambling_tile_buffer);
        gfxProgramSetParameter(gfx, program, "g_noise_texture", noise_texture);

        u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel);
        u32        num_groups_x = (128 + num_threads[0] - 1) / num_threads[0];
        u32        num_groups_y = (128 + num_threads[1] - 1) / num_threads[1];

        gfxCommandBindKernel(gfx, kernel);
        gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
    }
    void Release() {
        gfxDestroyBuffer(gfx, sobol_buffer);
        gfxDestroyBuffer(gfx, ranking_tile_buffer);
        gfxDestroyBuffer(gfx, scrambling_tile_buffer);
        gfxDestroyTexture(gfx, noise_texture);

        *this = {};
    }
};

#endif // BAKE_NOISE_HPP