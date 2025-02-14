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

#if !defined(FILE_IO_HPP)
#    define FILE_IO_HPP

#    include "common.h"

// https://github.com/dscharstein/pfmLib/blob/master/ImageIOpfm.cpp
static void write_f32x4_to_pfm(const char *file_name, void const *src_data, u64 width, u64 height, u64 pitch = u64(-1)) {
    if (pitch == u64(-1)) pitch = width * sizeof(f32x4);
    FILE *file = NULL;
    int   err  = fopen_s(&file, file_name, "wb");
    if (err) {
        assert(false);
        return;
    }
    fprintf(file, "PF\n%d %d\n%lf\n", (u32)width, (u32)height, -1.0f);
    ifor(height) {
        jfor(width) {
            f32 const *src = &((f32 const *)src_data)[pitch / sizeof(f32) * i + j * u64(4)];
            fwrite((void const *)(src + u64(0)), u64(1), u64(4), file);
            fwrite((void const *)(src + u64(1)), u64(1), u64(4), file);
            fwrite((void const *)(src + u64(2)), u64(1), u64(4), file);
        }
    }
    fclose(file);
}

void write_f32x4_png(char const *filename, void const *src_data, u64 width, u64 height, u64 pitch = u64(-1));

#endif // FILE_IO_HPP