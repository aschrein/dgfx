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

#include "dgfx/file_io.hpp"
#include <vector>

namespace stb {

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include <3rdparty/stb/stb_image.h>
#include <3rdparty/stb/stb_image_write.h>
} // namespace stb

void write_f32x4_png(char const *filename, void const *src_data, u64 width, u64 height, u64 pitch) {
    if (pitch == u64(-1)) pitch = width * sizeof(f32x4);
    std::vector<u8> data = {};
    data.resize(width * height * 4);
    // convert to rgba8_unorm
    yfor(height) {
        xfor(width) {
            f32x4 src                                      = ((f32x4 const *)((u8 *)src_data + pitch * y))[x];
            data[x * u64(4) + y * width * u64(4) + u64(0)] = u8(glm::clamp(src.x, f32(0.0), f32(1.0)) * f32(255.0));
            data[x * u64(4) + y * width * u64(4) + u64(1)] = u8(glm::clamp(src.y, f32(0.0), f32(1.0)) * f32(255.0));
            data[x * u64(4) + y * width * u64(4) + u64(2)] = u8(glm::clamp(src.z, f32(0.0), f32(1.0)) * f32(255.0));
            data[x * u64(4) + y * width * u64(4) + u64(3)] = 255u;// u8(glm::clamp(src.w, f32(0.0), f32(1.0)) * f32(255.0));
        }
    }
    stb::stbi_write_png(filename, i32(width), i32(height), stb::STBI_rgb_alpha, &data[0], i32(width) * i32(4));
}
