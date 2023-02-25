
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

#if !defined(GFX_UTILS_HPP)
#    define GFX_UTILS_HPP

#    include "file_io.hpp"
#    include "utils.hpp"

// SRC: https://github.com/gboisse/gfx/blob/53c97ba9d60a07dda3042fe6d21ee621caae82d4/gfx_scene.h#L997
static DXGI_FORMAT GetImageFormat(GfxImage const &image) {
    if (image.format != DXGI_FORMAT_UNKNOWN) return image.format;
    if (image.bytes_per_channel != 1 && image.bytes_per_channel != 2 && image.bytes_per_channel != 4) return DXGI_FORMAT_UNKNOWN;
    uint32_t const bits = (image.bytes_per_channel << 3);
    switch (image.channel_count) {
    case 1: return (bits == 8 ? DXGI_FORMAT_R8_UNORM : bits == 16 ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R32_FLOAT);
    case 2: return (bits == 8 ? DXGI_FORMAT_R8G8_UNORM : bits == 16 ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R32G32_FLOAT);
    case 4: return (bits == 8 ? DXGI_FORMAT_R8G8B8A8_UNORM : bits == 16 ? DXGI_FORMAT_R16G16B16A16_UNORM : DXGI_FORMAT_R32G32B32A32_FLOAT);
    default: break;
    }
    return DXGI_FORMAT_UNKNOWN;
}

static u32 GetNumComponents(DXGI_FORMAT fmt) {
    switch (fmt) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: return u32(4);
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return u32(4);
    case DXGI_FORMAT_R32G32B32A32_UINT: return u32(4);
    case DXGI_FORMAT_R32G32B32A32_SINT: return u32(4);
    case DXGI_FORMAT_R32G32B32_TYPELESS: return u32(3);
    case DXGI_FORMAT_R32G32B32_FLOAT: return u32(3);
    case DXGI_FORMAT_R32G32B32_UINT: return u32(3);
    case DXGI_FORMAT_R32G32B32_SINT: return u32(3);
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return u32(4);
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return u32(4);
    case DXGI_FORMAT_R16G16B16A16_UNORM: return u32(4);
    case DXGI_FORMAT_R16G16B16A16_UINT: return u32(4);
    case DXGI_FORMAT_R16G16B16A16_SNORM: return u32(4);
    case DXGI_FORMAT_R16G16B16A16_SINT: return u32(4);
    case DXGI_FORMAT_R32G32_TYPELESS: return u32(2);
    case DXGI_FORMAT_R32G32_FLOAT: return u32(2);
    case DXGI_FORMAT_R32G32_UINT: return u32(2);
    case DXGI_FORMAT_R32G32_SINT: return u32(2);
    case DXGI_FORMAT_R32G8X24_TYPELESS: return u32(2);
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return u32(1);
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: return u32(1);
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: return u32(1);
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return u32(4);
    case DXGI_FORMAT_R10G10B10A2_UNORM: return u32(4);
    case DXGI_FORMAT_R10G10B10A2_UINT: return u32(4);
    case DXGI_FORMAT_R11G11B10_FLOAT: return u32(3);
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return u32(4);
    case DXGI_FORMAT_R8G8B8A8_UNORM: return u32(4);
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return u32(4);
    case DXGI_FORMAT_R8G8B8A8_UINT: return u32(4);
    case DXGI_FORMAT_R8G8B8A8_SNORM: return u32(4);
    case DXGI_FORMAT_R8G8B8A8_SINT: return u32(4);
    case DXGI_FORMAT_R16G16_TYPELESS: return u32(2);
    case DXGI_FORMAT_R16G16_FLOAT: return u32(2);
    case DXGI_FORMAT_R16G16_UNORM: return u32(2);
    case DXGI_FORMAT_R16G16_UINT: return u32(2);
    case DXGI_FORMAT_R16G16_SNORM: return u32(2);
    case DXGI_FORMAT_R16G16_SINT: return u32(2);
    case DXGI_FORMAT_R32_TYPELESS: return u32(1);
    case DXGI_FORMAT_D32_FLOAT: return u32(1);
    case DXGI_FORMAT_R32_FLOAT: return u32(1);
    case DXGI_FORMAT_R32_UINT: return u32(1);
    case DXGI_FORMAT_R32_SINT: return u32(1);
    case DXGI_FORMAT_R24G8_TYPELESS: return u32(2);
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return u32(1);
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS: return u32(1);
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return u32(1);
    case DXGI_FORMAT_R8G8_TYPELESS: return u32(2);
    case DXGI_FORMAT_R8G8_UNORM: return u32(2);
    case DXGI_FORMAT_R8G8_UINT: return u32(2);
    case DXGI_FORMAT_R8G8_SNORM: return u32(2);
    case DXGI_FORMAT_R8G8_SINT: return u32(2);
    case DXGI_FORMAT_R16_TYPELESS: return u32(1);
    case DXGI_FORMAT_R16_FLOAT: return u32(1);
    case DXGI_FORMAT_D16_UNORM: return u32(1);
    case DXGI_FORMAT_R16_UNORM: return u32(1);
    case DXGI_FORMAT_R16_UINT: return u32(1);
    case DXGI_FORMAT_R16_SNORM: return u32(1);
    case DXGI_FORMAT_R16_SINT: return u32(1);
    case DXGI_FORMAT_R8_TYPELESS: return u32(1);
    case DXGI_FORMAT_R8_UNORM: return u32(1);
    case DXGI_FORMAT_R8_UINT: return u32(1);
    case DXGI_FORMAT_R8_SNORM: return u32(1);
    case DXGI_FORMAT_R8_SINT: return u32(1);
    case DXGI_FORMAT_A8_UNORM: return u32(1);
    case DXGI_FORMAT_R1_UNORM: return u32(1);
    default: UNIMPLEMENTED;
    }
}
static GfxTexture load_texture(GfxContext gfx, char const *asset_file) {
    GFX_ASSERT(asset_file != nullptr);
    int32_t  image_width, image_height, channel_count;
    stbi_uc *image_data        = nullptr;
    uint32_t bytes_per_channel = 2;

    if (stbi_is_16_bit(asset_file)) image_data = (stbi_uc *)stbi_load_16(asset_file, &image_width, &image_height, &channel_count, 0);
    if (image_data == nullptr) {
        image_data        = stbi_load(asset_file, &image_width, &image_height, &channel_count, 0);
        bytes_per_channel = 1;
    }

    assert(image_data != nullptr);
    uint32_t const resolved_channel_count = (uint32_t)(channel_count != 3 ? channel_count : 4);
    char const    *file                   = GFX_MAX(strrchr(asset_file, '/'), strrchr(asset_file, '\\'));
    file                                  = (file == nullptr ? asset_file : file + 1); // retrieve file name
    size_t const image_data_size          = (size_t)image_width * image_height * resolved_channel_count * bytes_per_channel;
    GfxImage     image_ref                = {};
    image_ref.data.resize(image_data_size);
    image_ref.width             = (uint32_t)image_width;
    image_ref.height            = (uint32_t)image_height;
    image_ref.channel_count     = resolved_channel_count;
    image_ref.bytes_per_channel = bytes_per_channel;
    image_ref.format            = GetImageFormat(image_ref);
    if (bytes_per_channel == 1) {
        uint8_t *data        = image_ref.data.data();
        uint8_t  alpha_check = 255; // check alpha
        for (int32_t y = 0; y < image_height; ++y)
            for (int32_t x = 0; x < image_width; ++x)
                for (int32_t k = 0; k < (int32_t)resolved_channel_count; ++k) {
                    int32_t const dst_index = (int32_t)resolved_channel_count * (x + y * image_width) + k;
                    int32_t const src_index = channel_count * (x + y * image_width) + k;
                    uint8_t const source    = (k < channel_count ? image_data[src_index] : (uint8_t)255);
                    if (k == 3) alpha_check &= source;
                    data[dst_index] = source;
                }
        image_ref.flags = (alpha_check == 255 ? 0 : kGfxImageFlag_HasAlphaChannel);
    } else {
        uint16_t *data        = (uint16_t *)image_ref.data.data();
        uint16_t  alpha_check = 65535; // check alpha
        for (int32_t y = 0; y < image_height; ++y)
            for (int32_t x = 0; x < image_width; ++x)
                for (int32_t k = 0; k < (int32_t)resolved_channel_count; ++k) {
                    int32_t const  dst_index = (int32_t)resolved_channel_count * (x + y * image_width) + k;
                    int32_t const  src_index = channel_count * (x + y * image_width) + k;
                    uint16_t const source    = (k < channel_count ? image_data[src_index] : (uint16_t)65535);
                    if (k == 3) alpha_check &= source;
                    data[dst_index] = source;
                }
        image_ref.flags = (alpha_check == 65535 ? 0 : kGfxImageFlag_HasAlphaChannel);
    }

    stbi_image_free(image_data);

    GfxTexture     texture               = gfxCreateTexture2D(gfx, image_ref.width, image_ref.height, image_ref.format, gfxCalculateMipCount(image_ref.width, image_ref.height));
    uint32_t const texture_size          = image_ref.width * image_ref.height * image_ref.channel_count * image_ref.bytes_per_channel;
    GfxBuffer      upload_texture_buffer = gfxCreateBuffer(gfx, texture_size, image_ref.data.data(), kGfxCpuAccess_Write);
    gfxCommandCopyBufferToTexture(gfx, texture, upload_texture_buffer);
    gfxDestroyBuffer(gfx, upload_texture_buffer);
    gfxCommandGenerateMips(gfx, texture);
    return texture;
}
// static GfxBuffer write_texture_to_buffer(GfxContext gfx, GfxTexture &input) {
//     GfxBuffer dump_buffer = gfxCreateBuffer(gfx, sizeof(f32x4) * g_window_size.x * g_window_size.y);
//     GfxBuffer cpu_buffer  = gfxCreateBuffer(gfx, sizeof(f32x4) * g_window_size.x * g_window_size.y, nullptr, kGfxCpuAccess_Read);
//     defer(gfxDestroyBuffer(gfx, dump_buffer));
//
//     gfxProgramSetParameter(gfx, g_write_texture_to_buffer_program, "g_input", input);
//     gfxProgramSetParameter(gfx, g_write_texture_to_buffer_program, "g_output", dump_buffer);
//
//     u32 const *num_threads  = gfxKernelGetNumThreads(gfx, g_write_texture_to_buffer_kernel);
//     u32        num_groups_x = (input.getWidth() + num_threads[0] - 1) / num_threads[0];
//     u32        num_groups_y = (input.getHeight() + num_threads[1] - 1) / num_threads[1];
//
//     gfxCommandBindKernel(gfx, g_write_texture_to_buffer_kernel);
//     gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
//
//     gfxCommandCopyBuffer(gfx, cpu_buffer, dump_buffer);
//
//     return cpu_buffer;
// }

class GfxBufferSubAllocator {
public:
    struct Allocation {
        u8       *host_dst      = {};
        u32       device_offset = {};
        u32       size          = {};
        GfxBuffer buffer        = {};
        bool      IsValid() const { return host_dst != NULL; }
        template <typename T>
        void CopyIn(std::vector<T> const &src) {
            memcpy(host_dst, &src[0], sizeof(T) * src.size());
        }
    };

protected:
    u32                     size             = {};
    GfxBuffer               upload_buffer    = {};
    OffsetAllocator         offset_allocator = {};
    u8                     *host_map         = {};
    std::vector<Allocation> deferred_free_queue[3];
    u32                     frame_idx = u32(0);

public:
    GfxBuffer  GetBuffer() { return upload_buffer; }
    Allocation Allocate(u64 needed_size, u32 alignment = u32(256)) {
        OffsetAllocator::Allocation a = offset_allocator.Allocate(u32(needed_size), alignment);
        if (a.IsValid() == false) return Allocation{NULL};
        Allocation dev_a    = {};
        dev_a.device_offset = a.offset;
        dev_a.host_dst      = host_map + a.offset;
        dev_a.size          = u32(needed_size);
        dev_a.buffer        = upload_buffer;
        return dev_a;
    }
    void FlushDeferredFreeQueue() {
        frame_idx++;
        for (auto &a : deferred_free_queue[frame_idx % 3]) {
            Free(a);
        }
        deferred_free_queue[frame_idx % 3].clear();
    }
    void DeferFree(Allocation al) { deferred_free_queue[frame_idx % 3].push_back(al); }
    bool CanAllocate(u64 needed_size, u32 alignment = u32(256)) { return offset_allocator.CanAllocate(u32(needed_size), alignment); }
    void Free(Allocation const &allocation) { offset_allocator.Free(OffsetAllocator::Allocation{allocation.device_offset, allocation.size}); }
    void Release(GfxContext gfx) { gfxDestroyBuffer(gfx, upload_buffer); }
};

class GfxUploadBuffer : public GfxBufferSubAllocator {
public:
    void Init(GfxContext gfx, u32 _size = u32(100 << 20)) {
        size = _size;
        offset_allocator.Init(size);
        upload_buffer = gfxCreateBuffer(gfx, size, NULL, kGfxCpuAccess_Write);
        host_map      = gfxBufferGetData<u8>(gfx, upload_buffer);
    }
};

class GfxDownloadBuffer : public GfxBufferSubAllocator {
public:
    void Init(GfxContext gfx, u32 _size = u32(100 << 20)) {
        size = _size;
        offset_allocator.Init(size);
        upload_buffer = gfxCreateBuffer(gfx, size, NULL, kGfxCpuAccess_Read);
        host_map      = gfxBufferGetData<u8>(gfx, upload_buffer);
    }
};

static GfxBuffer write_texture_to_buffer(GfxContext gfx, GfxTexture &input) {
    static GfxProgram g_write_texture_to_buffer_program = {};
    static GfxKernel  g_write_texture_to_buffer_kernel  = {};
    if (!g_write_texture_to_buffer_program) {
        g_write_texture_to_buffer_program = gfxCreateProgram(gfx, "write_texture_to_buffer", DGFX_PATH "shaders/");
        assert(g_write_texture_to_buffer_program);
        g_write_texture_to_buffer_kernel = gfxCreateComputeKernel(gfx, g_write_texture_to_buffer_program, "write_texture_to_buffer");
        assert(g_write_texture_to_buffer_kernel);
    }

    GfxBuffer dump_buffer = gfxCreateBuffer(gfx, sizeof(f32x4) * input.getWidth() * input.getHeight());
    GfxBuffer cpu_buffer  = gfxCreateBuffer(gfx, sizeof(f32x4) * input.getWidth() * input.getHeight(), nullptr, kGfxCpuAccess_Read);
    defer(gfxDestroyBuffer(gfx, dump_buffer));
    // defer(gfxDestroyBuffer(g_gfx, cpu_buffer));

    gfxProgramSetParameter(gfx, g_write_texture_to_buffer_program, "g_input", input);
    gfxProgramSetParameter(gfx, g_write_texture_to_buffer_program, "g_output", dump_buffer);

    u32 const *num_threads  = gfxKernelGetNumThreads(gfx, g_write_texture_to_buffer_kernel);
    u32        num_groups_x = (input.getWidth() + num_threads[0] - 1) / num_threads[0];
    u32        num_groups_y = (input.getHeight() + num_threads[1] - 1) / num_threads[1];

    gfxCommandBindKernel(gfx, g_write_texture_to_buffer_kernel);
    gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);

    gfxCommandCopyBuffer(gfx, cpu_buffer, dump_buffer);

    return cpu_buffer;
}
static void WaitIdle(GfxContext gfx) {
    // gfxFrame(g_gfx);
    GfxInternal *_gfx = GfxInternal::GetGfx(gfx);
    _gfx->finish();
}
template <typename T = u8>
static std::vector<T> read_device_buffer(GfxContext gfx, GfxBuffer buf) {
    GfxBuffer cpu_buffer = gfxCreateBuffer(gfx, buf.getSize(), nullptr, kGfxCpuAccess_Read);
    gfxCommandCopyBuffer(gfx, cpu_buffer, buf);
    WaitIdle(gfx);
    T             *host_data = gfxBufferGetData<T>(gfx, cpu_buffer);
    u32            num       = u32(buf.getSize() / sizeof(T));
    std::vector<T> out       = {};
    out.resize(num);
    ifor(num) { out[i] = host_data[i]; }
    defer(gfxDestroyBuffer(gfx, cpu_buffer));
    return out;
}
static void write_texture_to_file(GfxContext gfx, GfxTexture texture, char const *filename) {
    GfxBuffer dump_buffer = write_texture_to_buffer(gfx, texture);
    WaitIdle(gfx);
    f32x4 *host_rgba_f32x4 = gfxBufferGetData<f32x4>(gfx, dump_buffer);
    write_f32x4_png(filename, host_rgba_f32x4, texture.getWidth(), texture.getHeight());
    gfxDestroyBuffer(gfx, dump_buffer);
}
#endif //  GFX_UTILS_HPP