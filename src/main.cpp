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

#define GFX_IMPLEMENTATION_DEFINE
#define _CRT_SECURE_NO_WARNINGS
#include "gfx_imgui.h"
#include "gfx_scene.h"
#include "gfx_window.h"
#undef _CRT_SECURE_NO_WARNINGS
#include "3rdparty/samplerCPP/samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_256spp.cpp"
#include "common/common.h"
#include <3rdparty/embree/include/embree3/rtcore.h>
#include <3rdparty/embree/include/embree3/rtcore_builder.h>
#include <3rdparty/embree/include/embree3/rtcore_scene.h>
#include <cassert>
#include <chrono>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "shaders/material.h"
#include "common/gizmo.hpp"

#include <3rdparty/SDL2/include/SDL.h>
#include <3rdparty/pugixml/src/pugixml.hpp>

#define XML_READ_F32(x)                                                                                                                                                            \
    if (strcmp(attr.name(), #x) == i32(0)) {                                                                                                                                       \
        x = attr.as_float();                                                                                                                                                       \
    }

#define XML_WRITE_F32(x) fprintf(file, #x "=\"%f\" ", x);

static std::string read_file(char const *filename) {
    u64   size      = 0;
    FILE *text_file = NULL;
    int   err       = fopen_s(&text_file, filename, "rb");
    if (err) return "";
    if (text_file == NULL) return "";
    fseek(text_file, 0, SEEK_END);
    long fsize = ftell(text_file);
    fseek(text_file, 0, SEEK_SET);
    size       = (u64)fsize;
    char *data = (char *)malloc((u64)fsize + 1);
    if (data == NULL) return "";
    fread(data, 1, (u64)fsize, text_file);
    data[size] = '\0';
    fclose(text_file);
    std::string out = data;
    free(data);
    return out;
}
struct Value {
    std::string name;
    f32         f32_value;
};
struct XMLConfig {
    void Restore(std::function<void(pugi::xml_node)> child_callback) {
        pugi::xml_document doc         = {};
        pugi::xml_node     config_node = {};
        std::string        state       = read_file("config.xml");
        if (state.size()) {
            pugi::xml_document     doc    = {};
            pugi::xml_parse_result result = doc.load_buffer(state.c_str(), state.size());
            if (result) {
                if (config_node = doc.child("config")) {
                    for (auto &c : config_node.children()) {
                        child_callback(c);
                    }
                }
            }
        }
    }
    void Store(std::function<void(FILE *)> callback) {
        FILE *config_file = NULL;
        int   err         = fopen_s(&config_file, "config.xml", "wb");
        if (err) return;
        fprintf(config_file, "<config>\n");
        callback(config_file);
        fprintf(config_file, "</config>\n");
        fflush(config_file);
        fclose(config_file);
    }
};

struct SinSoundEffect {
#define ATTRIBUTE_LIST                                                                                                                                                             \
    ATTRIBUTE(duration)                                                                                                                                                            \
    ATTRIBUTE(amplitude)                                                                                                                                                           \
    ATTRIBUTE(frequency0)                                                                                                                                                          \
    ATTRIBUTE(frequency1)                                                                                                                                                          \
    ATTRIBUTE(frequency_dt)                                                                                                                                                        \
    ATTRIBUTE(fade_in_gain)                                                                                                                                                        \
    ATTRIBUTE(fade_out_gain)

    void Load(pugi::xml_node node) {
        for (auto &attr : node.attributes()) {
#define ATTRIBUTE(x) XML_READ_F32(x)

            ATTRIBUTE_LIST

#undef ATTRIBUTE
        }
    }

    void Store(char const *name, FILE *file) {
        fprintf(file, "<%s ", name);

#define ATTRIBUTE(x) XML_WRITE_F32(x)

        ATTRIBUTE_LIST

#undef ATTRIBUTE

        fprintf(file, "/>\n");
    }

#define ATTRIBUTE(x) f32 x = {};
    ATTRIBUTE_LIST
#undef ATTRIBUTE

#undef ATTRIBUTE_LIST
};
SinSoundEffect creation_sound_effect[3] = {};
SinSoundEffect destruction_sound_effect = {};

static_assert(sizeof(AABB) == sizeof(D3D12_RAYTRACING_AABB), "");

// https://github.com/aschrein/VulkII/blob/master/include/scene.hpp#L1617
namespace cpubvh {
// Based on
// https://interplayoflight.wordpress.com/2020/07/21/using-embree-generated-bvh-trees-for-gpu-raytracing/
struct LeafNode;
struct Node {
    AABB aabb = {};

    virtual f32 sah() = 0;
    virtual ~Node() {}
    virtual u32   GetNumChildren() { return u32(0); }
    virtual Node *GetChild(u32 i) { return NULL; }
    virtual bool  IsLeaf() { return false; }
    bool          AnyHit(Ray const &ray, std::function<bool(Node *)> fn) {
        if (IsLeaf())
            if (fn(this)) return true;
        ifor(GetNumChildren()) if (GetChild(i) && GetChild(i)->aabb.ray_test(ray.o, f32(1.0) / ray.d) && GetChild(i)->AnyHit(ray, fn)) return true;
        return false;
    }
    bool CheckAny(f32x3 p) {
        if (IsLeaf())
            if (aabb.contains(p)) return true;
        ifor(GetNumChildren()) if (GetChild(i) && GetChild(i)->CheckAny(p)) return true;
        return false;
    }
};
struct InnerNode : public Node {
    u32    num_children = u32(0);
    Node **children     = {};
    bool   sah_dirty    = true;
    f32    sah_cache    = f32(0.0);

    ~InnerNode() override {}
    InnerNode(Node **_children, u32 _num_children) : num_children(_num_children), children(_children) {}
    u32   GetNumChildren() override { return num_children; }
    Node *GetChild(u32 i) override { return children[i]; }
    f32   sah() override {
        if (!sah_dirty) return sah_cache;
        assert(num_children != u32(0));
        AABB b   = children[0]->aabb;
        f32  sum = f32(0.0);
        ifor(num_children) {
            sum += children[i]->aabb.area() * children[i]->sah();
            b.expand(children[i]->aabb);
        }
        sah_cache = f32(1.0) + sum / std::max(sum * f32(1.0e-6), b.area());
        sah_dirty = false;
        return sah_cache;
    }
};
struct LeafNode : public Node {
    u32 primitive_idx = 0;

    LeafNode *GetAsLeaf() { return this; }
    bool      IsLeaf() override { return true; }
    float     sah() override { return f32(1.0); }
    ~LeafNode() override {}
    LeafNode(u32 primitive_idx, const AABB &aabb) : primitive_idx(primitive_idx) { this->aabb = aabb; }
};
class BVH {

private:
    RTCDevice device = {};

    static void *CreateLeaf(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive *prims, u64 numPrims, void *userPtr) {
        assert(numPrims == 1);
        void *ptr  = rtcThreadLocalAlloc(alloc, sizeof(LeafNode), u64(16));
        AABB  aabb = {};
        aabb.lo    = f32x3(prims->lower_x, prims->lower_y, prims->lower_z);
        aabb.hi    = f32x3(prims->upper_x, prims->upper_y, prims->upper_z);
        return (void *)new (ptr) LeafNode(prims->primID, aabb);
    }
    static void *CreateNode(RTCThreadLocalAllocator alloc, unsigned int numChildren, void *userPtr) {
        void  *ptr            = rtcThreadLocalAlloc(alloc, sizeof(InnerNode), u64(16));
        Node **children_array = (Node **)rtcThreadLocalAlloc(alloc, sizeof(InnerNode *) * numChildren, u64(16));
        return (void *)new (ptr) InnerNode(children_array, numChildren);
    }
    static void SetChildren(void *nodePtr, void **childPtr, unsigned int numChildren, void *userPtr) { ifor(numChildren)((InnerNode *)nodePtr)->children[i] = (Node *)childPtr[i]; }
    static void SetBounds(void *nodePtr, const RTCBounds **bounds, unsigned int numChildren, void *userPtr) {
        assert(numChildren > u32(1));
        ((Node *)nodePtr)->aabb.lo = f32x3(bounds[0]->lower_x, bounds[0]->lower_y, bounds[0]->lower_z);
        ((Node *)nodePtr)->aabb.hi = f32x3(bounds[0]->upper_x, bounds[0]->upper_y, bounds[0]->upper_z);
        ifor(numChildren) {
            ((Node *)nodePtr)->aabb.expand(f32x3(bounds[i]->lower_x, bounds[i]->lower_y, bounds[i]->lower_z));
            ((Node *)nodePtr)->aabb.expand(f32x3(bounds[i]->upper_x, bounds[i]->upper_y, bounds[i]->upper_z));
        }
    }
    static void SplitPrimitive(const RTCBuildPrimitive *prim, unsigned int dim, float pos, RTCBounds *lprim, RTCBounds *rprim, void *userPtr) {
        assert(dim < 3);
        *(RTCBuildPrimitive *)lprim = *(RTCBuildPrimitive *)prim;
        *(RTCBuildPrimitive *)rprim = *(RTCBuildPrimitive *)prim;
        (&lprim->upper_x)[dim]      = pos;
        (&rprim->lower_x)[dim]      = pos;
    }

public:
    void Init() { device = rtcNewDevice(NULL); }
    void Release() { rtcReleaseDevice(device); }
    struct BVHResult {
        RTCBVH bvh  = {};
        Node  *root = {};

        void Release() {
            if (bvh) {
                rtcReleaseBVH(bvh);
            }
            bvh  = {};
            root = NULL;
        }
        bool IsValid() const { return bvh && root; }
    };
    BVHResult Build(AABB *elems, u64 num_elems) {
        u64                            extra_nodes = num_elems;
        u64                            num_nodes   = num_elems;
        u64                            capacity    = num_elems + extra_nodes;
        std::vector<RTCBuildPrimitive> prims       = {};
        prims.resize(capacity);
        ifor(num_elems) {
            RTCBuildPrimitive prim = {};
            AABB              aabb = elems[i];
            prim.lower_x           = aabb.lo.x;
            prim.lower_y           = aabb.lo.y;
            prim.lower_z           = aabb.lo.z;
            prim.upper_x           = aabb.hi.x;
            prim.upper_y           = aabb.hi.y;
            prim.upper_z           = aabb.hi.z;
            prim.geomID            = u32(0);
            prim.primID            = i; // primitive_ids[i];
            prims[i]               = prim;
        }

        RTCBVH            bvh            = rtcNewBVH(device);
        RTCBuildArguments arguments      = rtcDefaultBuildArguments();
        arguments.byteSize               = sizeof(arguments);
        arguments.buildFlags             = RTC_BUILD_FLAG_NONE;
        arguments.buildQuality           = RTC_BUILD_QUALITY_LOW;
        arguments.maxBranchingFactor     = u32(4);
        arguments.maxDepth               = u32(1024);
        arguments.sahBlockSize           = u32(1);
        arguments.minLeafSize            = u32(1);
        arguments.maxLeafSize            = u32(1);
        arguments.traversalCost          = f32(1.0);
        arguments.intersectionCost       = f32(1.0);
        arguments.bvh                    = bvh;
        arguments.primitives             = &prims[0];
        arguments.primitiveCount         = num_nodes;
        arguments.primitiveArrayCapacity = capacity;
        arguments.createNode             = CreateNode;
        arguments.setNodeChildren        = SetChildren;
        arguments.setNodeBounds          = SetBounds;
        arguments.createLeaf             = CreateLeaf;
        arguments.splitPrimitive         = SplitPrimitive;
        arguments.buildProgress          = nullptr;
        arguments.userPtr                = nullptr;
        BVHResult out                    = {};
        out.bvh                          = bvh;
        out.root                         = (Node *)rtcBuildBVH(&arguments);
        if (out.root == NULL) {
            fprintf(stdout, "[ERROR] Embree device error code: %i\n", rtcGetDeviceError(device));
        }
        assert(out.root && "Might be not enough max_depth");

        return out;
    }
};
} // namespace cpubvh

struct Cube {
    u32      id       = u32(-1);
    i32x3    ipos     = {};
    Material material = {};
    bool     IsValid() { return id != u32(-1); }
};
struct Camera {
    f32x3 pos;
    f32x3 look_at;
    f32   fov;
    f32   scale;

    f32   xscale;
    f32   yscale;
    f32x3 look;
    f32x3 up;
    f32x3 right;
    // f32x4x4 view;
    // f32x4x4 proj;

    void UpdateMatrices() {
        look  = normalize(look_at - pos);
        right = normalize(cross(look, f32x3(0.0, 1.0, 0.0)));
        up    = normalize(cross(right, look));
        // proj  = f32x4x4(0.0);
        /*f32 aspect = f32(1.0);

        f32 tfov   = std::tan(fov * f32(0.5));
        proj[0][0] = f32(1.0) / (aspect * tfov);
        proj[1][1] = f32(1.0) / (tfov);
        proj[2][2] = f32(0.0);
        proj[2][3] = f32(-1.0);
        proj[3][2] = f32(1.0e-3);
        proj       = glm::transpose(proj);*/
        /*proj[0][0] = xscale;
        proj[1][1] = yscale;
        proj[2][2] = f32(1.03 - 6);
        view       = glm::transpose(glm::lookAt(pos, look_at, f32x3(0.0, 1.0, 0.0)));*/
    }

    Ray GenRay(f32x2 uv) {
        uv    = uv * f32x2(2.0, -2.0) - f32x2(1.0, -1.0);
        Ray r = {};
        r.o   = pos + right * uv.x * xscale + up * uv.y * yscale;
        r.d   = look;
        r.ird = f32(1.0) / r.d;
        return r;
    }
};
enum Constants {
    INVALID_ID = u32(-1),
};
struct CubeCreateInfo {
    Material material = {};
    i32x3    ipos     = {};
};

struct Scene {
    std::vector<bool>      alive_flags     = {};
    std::vector<i32x3>     ipos            = {};
    std::vector<AABB>      aabbs           = {};
    std::vector<Material>  materials       = {};
    std::vector<u32>       free_ids        = {};
    cpubvh::BVH            cpu_bvh_builder = {};
    cpubvh::BVH::BVHResult cpu_bvh         = {};
    void                   UpdateBVH() {
        cpu_bvh.Release();
        cpu_bvh = {};
        if (aabbs.size()) cpu_bvh = cpu_bvh_builder.Build(&aabbs[0], aabbs.size());
    }
    void Init() { cpu_bvh_builder.Init(); }
    void Release() {
        aabbs.clear();
        materials.clear();
        free_ids.clear();
        cpu_bvh.Release();
        cpu_bvh_builder.Release();
    }
    u32 AddCube(CubeCreateInfo const &cinfo) {
        u32 id = u32(-1);
        if (free_ids.size()) {
            id = free_ids.back();
            free_ids.pop_back();
        } else {
            id = u32(aabbs.size());
            alive_flags.push_back({});
            aabbs.push_back({});
            ipos.push_back({});
            materials.push_back({});
        }
        alive_flags[id] = true;
        ipos[id]        = cinfo.ipos;
        materials[id]   = cinfo.material;
        aabbs[id]       = AABB{f32x3(cinfo.ipos), f32x3(cinfo.ipos + i32x3(1, 1, 1))};
        return id;
    }
    void RemoveCube(u32 id) {
        ipos[id]        = {};
        aabbs[id]       = {};
        materials[id]   = {};
        alive_flags[id] = false;
        free_ids.push_back(id);
    }
};

struct BVH {
    GfxAccelerationStructure as;
    GfxRaytracingPrimitive   primitive;
};

u32x2 g_window_size = u32x2(1024, 1024);

GfxWindow  g_window                          = {};
GfxContext g_gfx                             = {};
GfxProgram g_write_texture_to_buffer_program = {};
GfxKernel  g_write_texture_to_buffer_kernel  = {};
Camera     g_camera                          = {};
BVH        g_bvh                             = {};
Scene      g_scene                           = {};
GfxBuffer  g_aabb_buffer                     = {};
GfxBuffer  g_material_buffer                 = {};
GfxBuffer  g_sobol_buffer                    = {};
GfxBuffer  g_ranking_tile_buffer             = {};
GfxBuffer  g_scrambling_tile_buffer          = {};
GfxBuffer  g_radiance_hash_table             = {};
// SDL_AudioDeviceID g_audio_device                    = {};
XMLConfig g_config = {};

f32  g_env_color[3]          = {f32(17.0) / 255.0, f32(80.0) / 255.0, f32(247.0) / 255.0};
f32  g_block_color[3]        = {};
f32  g_block_emissiveness[3] = {};
f32  g_block_emission_power  = f32(10.0);
bool g_block_metalness       = false;
bool g_block_transparent     = false;

void Restore() {
    g_config.Restore([&](pugi::xml_node n) {
        ifor(ARRAYSIZE(creation_sound_effect)) {
            char buf[100];
            snprintf(buf, sizeof(buf), "creation_sound_effect_%i", i);
            if (strcmp(n.name(), buf) == i32(0)) {
                creation_sound_effect[i].Load(n);
            }
        }
    });
}
void Store() {
    g_config.Store([&](FILE *f) {
        ifor(ARRAYSIZE(creation_sound_effect)) {
            char buf[100];
            snprintf(buf, sizeof(buf), "creation_sound_effect_%i", i);
            creation_sound_effect[i].Store(buf, f);
        }
    });
}
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
GfxTexture load_texture(char const *asset_file) {
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

    GfxTexture     texture               = gfxCreateTexture2D(g_gfx, image_ref.width, image_ref.height, image_ref.format, gfxCalculateMipCount(image_ref.width, image_ref.height));
    uint32_t const texture_size          = image_ref.width * image_ref.height * image_ref.channel_count * image_ref.bytes_per_channel;
    GfxBuffer      upload_texture_buffer = gfxCreateBuffer(g_gfx, texture_size, image_ref.data.data(), kGfxCpuAccess_Write);
    gfxCommandCopyBufferToTexture(g_gfx, texture, upload_texture_buffer);
    gfxDestroyBuffer(g_gfx, upload_texture_buffer);
    gfxCommandGenerateMips(g_gfx, texture);
    return texture;
}
void UpdateBVH() {
    // std::vector<Material> materials = {};
    // std::vector<AABB>     aabbs     = {};
    //// std::vector<u32>      ids       = {};
    // aabbs.resize(g_scene.cubes.size());
    //// ids.resize(g_scene.cubes.size());
    // materials.resize(g_scene.cubes.size());
    // for (auto c : g_scene.cubes) {
    //     if (c->IsValid() == false) continue;
    //     AABB aabb = {};
    //     aabb.lo.x = f32(c->ipos.x);
    //     aabb.lo.y = f32(c->ipos.y);
    //     aabb.lo.z = f32(c->ipos.z);
    //     aabb.hi.x = f32(c->ipos.x + i32(1));
    //     aabb.hi.y = f32(c->ipos.y + i32(1));
    //     aabb.hi.z = f32(c->ipos.z + i32(1));
    //     aabbs[c->id] = aabb;
    //     // ids[c->id]       = c->id;
    //     materials[c->id] = c->material;
    // }
    if (g_aabb_buffer) gfxDestroyBuffer(g_gfx, g_aabb_buffer);
    if (g_material_buffer) gfxDestroyBuffer(g_gfx, g_material_buffer);
    if (g_bvh.primitive) gfxDestroyRaytracingPrimitive(g_gfx, g_bvh.primitive);
    if (g_bvh.as) gfxDestroyAccelerationStructure(g_gfx, g_bvh.as);

    g_aabb_buffer = gfxCreateBuffer(g_gfx, sizeof(AABB) * g_scene.aabbs.size(), &g_scene.aabbs[0]);
    g_aabb_buffer.setStride(sizeof(AABB));
    g_material_buffer = gfxCreateBuffer(g_gfx, sizeof(Material) * g_scene.materials.size(), &g_scene.materials[0]);
    g_material_buffer.setStride(sizeof(Material));
    GfxAccelerationStructure as        = gfxCreateAccelerationStructure(g_gfx);
    GfxRaytracingPrimitive   primitive = gfxCreateRaytracingPrimitive(g_gfx, as);
    gfxRaytracingPrimitiveSetInstanceID(g_gfx, primitive, u32(0));
    f32 transform[3][4] = {};
    transform[0][0]     = f32(1.0);
    transform[1][1]     = f32(1.0);
    transform[2][2]     = f32(1.0);
    gfxRaytracingPrimitiveSetTransform(g_gfx, primitive, (f32 *)transform);
    gfxRaytracingPrimitiveBuildProcedural(g_gfx, primitive, g_aabb_buffer, u32(g_scene.aabbs.size()), 0);
    gfxAccelerationStructureUpdate(g_gfx, as);
    g_bvh.as        = as;
    g_bvh.primitive = primitive;
    g_scene.UpdateBVH();
}
void ReleaseGlobalState() {
    // SDL_CloseAudioDevice(g_audio_device);
}
void audio_callback(void *, Uint8 *, int);
class IAudioObject {
public:
    virtual f64  Next()       = 0;
    virtual bool IsFinished() = 0;
    virtual void Release()    = 0;
};
class AudioHelper {
private:
    std::vector<IAudioObject *> objects      = {};
    SDL_AudioDeviceID           audio_device = {};
    SDL_AudioSpec               device_spec  = {};
    f64                         cur_sample   = f32(0.0);
    f64                         cur_a        = f32(0.0);

public:
    f64 a_dump         = f64(0.3);
    f64 a_k            = f64(100.0);
    f64 a_kk           = f64(100.0);
    u64 sample_counter = u64(0);

    static const u32 AMPLITUDE = 28000;
    static const u32 FREQUENCY = u32(44100);
    void             Init() {
        SDL_Init(SDL_INIT_AUDIO);

        SDL_AudioSpec audio_spec = {};
        audio_spec.freq          = FREQUENCY;
        audio_spec.format        = AUDIO_S16SYS;
        audio_spec.channels      = 1;
        audio_spec.samples       = 1 << 12;
        audio_spec.callback      = audio_callback;
        audio_spec.userdata      = this;

        audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, &device_spec, 0);
        assert(audio_device);
        // SDL_PauseAudio(0);
        SDL_PauseAudioDevice(audio_device, 0);
    }
    void Release() {
        SDL_CloseAudioDevice(audio_device);
        // SDL_CloseAudio();
        SDL_Quit();
    }
    void Push(IAudioObject *obj) {
        assert(obj && obj->IsFinished() == false);

        SDL_LockAudio();
        objects.push_back(obj);
        SDL_UnlockAudio();
    }
    f64 GetNextSample(f32 v) {
        f64 dt   = f64(1.0) / f32(AudioHelper::FREQUENCY);
        f64 dump = std::exp(-dt * a_dump * f32(1.0e6) * std::abs(v - cur_sample));
        cur_sample += (v - cur_sample) * f64(0.3);
        // cur_a *dt *a_kk +
        // cur_a += -cur_sample * dt * a_k;
        // cur_a = cur_a * f32(0.999);
        if (std::isnan(cur_sample) || std::isinf(cur_sample)) {
            cur_sample = f64(0.0);
        }
        return glm::clamp(cur_sample, f64(-1.0), f64(1.0));
    }
    void generateSamples(i16 *dst, u32 num_samples) {
        SDL_LockAudio();
        u32 i = u32(0);
        /*if (objects.empty()) {
            while (i < num_samples) {
                dst[i] = i16(GetNextSample(f32(0.0)));
                i++;
            }
            return;
        }*/
        while (i < num_samples) {
            f64 sample_sum  = f64(0.0);
            f64 weight_sum  = f64(0.0);
            f64 sample_dump = f64(1.0);
            jfor(objects.size()) {
                IAudioObject *pObj = objects[j];
                if (pObj == NULL || pObj->IsFinished()) continue;

                f64 s      = pObj->Next();
                f64 weight = f64(1.0) * sample_dump * std::exp(std::abs(s));
                sample_sum += s * weight;
                weight_sum += weight;
                // sample_dump /= f64(2.0);
            }
            f64 sample = sample_sum / std::max(f64(1.0e-3), weight_sum);
            // f64 sample = std::sin(f64(1.0) * f64(sample_counter) / f64(FREQUENCY) * (M_PI * f64(2.0)));
            sample_counter++;
            dst[i] = i16(GetNextSample(sample) * f64(AMPLITUDE));
            i++;
        }
        SDL_UnlockAudio();
    }
    void GarbageCollect() {
        SDL_LockAudio();
        std::vector<IAudioObject *> new_objects = {};
        ifor(objects.size()) {
            IAudioObject *pObj = objects[i];
            if (pObj->IsFinished()) {
                pObj->Release();
                continue;
            }
            new_objects.push_back(objects[i]);
        }
        objects = new_objects;
        SDL_UnlockAudio();
    }
    void wait() {
        int size;
        do {
            SDL_Delay(20);
            SDL_LockAudio();
            size = objects.size();
            SDL_UnlockAudio();
        } while (size > 0);
    }
};
AudioHelper g_audio_helper = {};
void        audio_callback(void *pUserData, Uint8 *pDst, int _length) {
    Sint16 *stream      = (Sint16 *)pDst;
    int     num_samples = u32(_length) / u32(2);
    g_audio_helper.generateSamples(stream, num_samples);
}
class StringWave : public IAudioObject {
    f32 amplitude     = f32(0.0);
    f32 frequency0    = f32(0.0);
    f32 frequency1    = f32(0.0);
    f32 frequency_dt  = f32(0.0);
    f32 fade_in_gain  = f32(0.0);
    f32 fade_out_gain = f32(0.0);
    f32 frequency     = f32(0.0);

    f32 duration = f32(0.0);

    f32 v           = f32(0.0);
    f32 a           = f32(0.0);
    f32 s           = f32(0.0);
    i32 num_samples = i32(0);
    f32 t           = f32(0.0);
    f32 gain        = f32(0.0);

public:
    StringWave(f32 duration, f32 _amplitude, f32 _frequency0, f32 _frequency1, f32 _frequency_dt, f32 _fade_in_gain, f32 _fade_out_gain)
        : duration(std::abs(duration)), num_samples(i32(std::abs(duration * AudioHelper::FREQUENCY))), amplitude(std::abs(_amplitude)), frequency0(std::abs(_frequency0)),
          frequency1(std::abs(_frequency1)), frequency_dt(std::abs(_frequency_dt)), fade_in_gain(std::abs(_fade_in_gain)), fade_out_gain(std::abs(_fade_out_gain)) {
        frequency = frequency0;
        // amplitude = glm::clamp(amplitude, f32(0.0), f32(1.0));
        a = amplitude;
    }
    f64 Next() override {
        f32 dt = f32(1.0) / f32(AudioHelper::FREQUENCY);

        s += v * fade_in_gain * dt;
        v += a * frequency0 * dt;
        a -= s * frequency_dt * dt;
        v *= fade_out_gain;

        if (std::isnan(s) || std::isnan(a)) {
            s = f32(0.0);
            a = f32(0.0);
        }

        num_samples = std::max(i32(0), i32(num_samples - 1));

        return s;

        // f32 t0 = glm::clamp(t / duration, f32(0.0), f32(1.0));
        // f32 t1 = glm::clamp((duration - t) / duration, f32(0.0), f32(1.0));
        //// f32 freq = glm::mix(frequency0, frequency1, (f32)glm::clamp(t / frequancy_dt, f32(0.0), f32(1.0)));
        ////  f32 gain = glm::mix(f32(0.0), f32(1.0), (f32)glm::clamp(t / gain_dt, f32(0.0), f32(1.0)));
        // frequency = glm::mix(frequency1, frequency0, f32(std::exp(-frequency_dt * t0)));
        //// gain += (f32(1.0) - gain) * std::exp(-gain_dt * dt);
        // gain  = glm::clamp(t0 * fade_in_gain, f32(0.0), f32(1.0)) * (glm::clamp(t1 * fade_out_gain, f32(0.0), f32(1.0)));
        // f32 s = gain * amplitude * std::sin(f32(2.0 * M_PI) * frequency * t);
        // t += dt;
        //
        // return s;
    }
    bool IsFinished() override { return num_samples <= i32(0); }
    void Release() override { delete this; };
};
class SineWave : public IAudioObject {
    f64 amplitude     = f32(0.0);
    f64 frequency0    = f32(0.0);
    f64 frequency1    = f32(0.0);
    f64 frequency_dt  = f32(0.0);
    f64 fade_in_gain  = f32(0.0);
    f64 fade_out_gain = f32(0.0);
    f64 frequency     = f32(0.0);
    f64 gain          = f32(0.0);
    f64 duration      = f32(0.0);

    i32 num_samples = i32(0);
    f64 t           = f64(0.0);

public:
    SineWave(f32 duration, f32 _amplitude, f32 _frequency0, f32 _frequency1, f32 _frequency_dt, f32 _fade_in_gain, f32 _fade_out_gain)
        : duration(std::abs(duration)), num_samples(i32(std::abs(duration * AudioHelper::FREQUENCY))), amplitude(std::abs(_amplitude)), frequency0(std::abs(_frequency0)),
          frequency1(std::abs(_frequency1)), frequency_dt(std::abs(_frequency_dt)), fade_in_gain(std::abs(_fade_in_gain)), fade_out_gain(std::abs(_fade_out_gain)) {
        frequency = frequency0;
        amplitude = glm::clamp(amplitude, f64(0.0), f64(1.0));
    }
    f64 Next() override {
        f64 dt = f64(1.0) / f64(AudioHelper::FREQUENCY);
        f64 t0 = glm::clamp(t / duration, f64(0.0), f64(1.0));
        f64 t1 = glm::clamp(f64(1.0) - t0, f64(0.0), f64(1.0));
        // f32 freq = glm::mix(frequency0, frequency1, (f32)glm::clamp(t / frequancy_dt, f32(0.0), f32(1.0)));
        //  f32 gain = glm::mix(f32(0.0), f32(1.0), (f32)glm::clamp(t / gain_dt, f32(0.0), f32(1.0)));
        frequency = glm::mix(frequency1, frequency0, glm::clamp(t1 * frequency_dt, f64(0.0), f64(1.0))); // f32(std::exp(-frequency_dt * t0)));
        // gain += (f32(1.0) - gain) * std::exp(-gain_dt * dt);
        gain  = glm::clamp(t0 * fade_in_gain, f64(0.0), f64(1.0)) * (glm::clamp(t1 * fade_out_gain, f64(0.0), f64(1.0)));
        f64 s = gain * amplitude * std::sin(f64(2.0 * M_PI) * f64(frequency) * f64(t));
        t += dt;
        // fprintf(stdout, "%f\n", gain);
        num_samples = std::max(i32(0), i32(num_samples - 1));
        return s;
    }
    bool IsFinished() override { return num_samples <= i32(0); }
    void Release() override { delete this; }
};
void InitGlobalState() {
    Restore();

    g_audio_helper.Init();

    g_window                          = gfxCreateWindow(g_window_size.x, g_window_size.y);
    g_gfx                             = gfxCreateContext(g_window);
    g_write_texture_to_buffer_program = gfxCreateProgram(g_gfx, "write_texture_to_buffer", "src/shaders/");
    assert(g_write_texture_to_buffer_program);
    g_write_texture_to_buffer_kernel = gfxCreateComputeKernel(g_gfx, g_write_texture_to_buffer_program, "write_texture_to_buffer");
    assert(g_write_texture_to_buffer_kernel);

    g_camera.fov     = f32(1.4);
    g_camera.pos     = f32x3(1.0, 1.0, 1.0) * f32(256.0);
    g_camera.look_at = f32x3(0.0, 0.0, 0.0);
    g_camera.scale   = f32(32.0);
    g_camera.UpdateMatrices();

    g_scene.Init();
    {
        f32x3 colors[] = {
            f32x3(22, 66, 7) / f32(255.0),    //
            f32x3(35, 122, 6) / f32(255.0),   //
            f32x3(101, 209, 65) / f32(255.0), //
            f32x3(220, 224, 90) / f32(255.0), //
            f32x3(122, 82, 9) / f32(255.0),   //
            f32x3(184, 52, 22) / f32(255.0),  //
            f32x3(42, 13, 84) / f32(255.0),   //
            f32x3(22, 184, 135) / f32(255.0), //
            f32x3(41, 8, 27) / f32(255.0),    //
            f32x3(240, 226, 31) / f32(255.0), //
        };
        u32 num_colors = ARRAYSIZE(colors);

        u32 initial_grid_size = u32(32);
        zfor(initial_grid_size) {
            xfor(initial_grid_size) {
                ifor(16) {
                    i32x3 ipos = i32x3(x - initial_grid_size / 2, i32(-1), z - initial_grid_size / 2);
                    u32   rnd  = pcg(x + pcg(z + pcg(i)));
                    u32   rnd1 = pcg(z + pcg(i + pcg(x)));
                    if ((rnd1 & u32(1))) continue;

                    f32 xi  = f32(rnd & 0xffffu) / f32(0xffffu);
                    f32 xi2 = f32(rnd1 & 0xffffu) / f32(0xffffu);
                    ipos.y += i; // i32(xi * f32(8.0));

                    CubeCreateInfo cinfo          = {};
                    cinfo.ipos                    = ipos;
                    cinfo.material.primitive_type = PRIMITIVE_TYPE_CUBE;
                    // cinfo.material.primitive_type = (rnd1 & u32(1)) ? PrimitiveType_t::CUBE : PrimitiveType_t::SPHERE;

                    cinfo.material.albedo = colors[rnd1 % num_colors];
                    // cinfo.material.albedo = colors[0];
                    cinfo.material.metalic   = xi2 > f32(0.5) ? f32(1.0) : f32(0.0);
                    cinfo.material.roughness = f32(0.05);
                    f32x3 p                  = f32x3(ipos) + f32x3(0.5, 0.5, 0.5);
                    g_scene.UpdateBVH();
                    if (g_scene.cpu_bvh.root && g_scene.cpu_bvh.root->CheckAny(p)) continue;

                    g_scene.AddCube(cinfo);
                }
            }
        }
    }

    UpdateBVH();

    gfxImGuiInitialize(g_gfx);

    g_sobol_buffer           = gfxCreateBuffer(g_gfx, sizeof(sobol_256spp_256d), (void *)&sobol_256spp_256d[0]);
    g_ranking_tile_buffer    = gfxCreateBuffer(g_gfx, sizeof(rankingTile), (void *)&rankingTile[0]);
    g_scrambling_tile_buffer = gfxCreateBuffer(g_gfx, sizeof(scramblingTile), (void *)&scramblingTile[0]);
    g_radiance_hash_table    = gfxCreateBuffer(g_gfx, sizeof(RadianceHashItem) * RADIANCE_HASH_GRID_NUM_ITEMS);
    g_radiance_hash_table.setStride(sizeof(RadianceHashItem));
}
GfxBuffer write_texture_to_buffer(GfxTexture &input) {
    GfxBuffer dump_buffer = gfxCreateBuffer(g_gfx, sizeof(f32x4) * g_window_size.x * g_window_size.y);
    GfxBuffer cpu_buffer  = gfxCreateBuffer(g_gfx, sizeof(f32x4) * g_window_size.x * g_window_size.y, nullptr, kGfxCpuAccess_Read);
    defer(gfxDestroyBuffer(g_gfx, dump_buffer));
    // defer(gfxDestroyBuffer(g_gfx, cpu_buffer));

    gfxProgramSetParameter(g_gfx, g_write_texture_to_buffer_program, "g_input", input);
    gfxProgramSetParameter(g_gfx, g_write_texture_to_buffer_program, "g_output", dump_buffer);

    u32 const *num_threads  = gfxKernelGetNumThreads(g_gfx, g_write_texture_to_buffer_kernel);
    u32        num_groups_x = (input.getWidth() + num_threads[0] - 1) / num_threads[0];
    u32        num_groups_y = (input.getHeight() + num_threads[1] - 1) / num_threads[1];

    gfxCommandBindKernel(g_gfx, g_write_texture_to_buffer_kernel);
    gfxCommandDispatch(g_gfx, num_groups_x, num_groups_y, 1);

    gfxCommandCopyBuffer(g_gfx, cpu_buffer, dump_buffer);

    return cpu_buffer;
}
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

namespace {
//#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_STATIC
//#include <3rdparty/stb/stb_image.h>
#include <3rdparty/stb/stb_image_write.h>

void write_f32x4_png(char const *filename, void const *src_data, u64 width, u64 height, u64 pitch = u64(-1)) {
    if (pitch == u64(-1)) pitch = width * sizeof(f32x4);
    std::vector<u8> data = {};
    data.resize(width * height * 4);
    yfor(height) {
        xfor(width) {
            f32x4 src                                      = ((f32x4 const *)((u8 *)src_data + pitch * y))[x];
            data[x * u64(4) + y * width * u64(4) + u64(0)] = u8(glm::clamp(src.x, f32(0.0), f32(1.0)) * f32(255.0));
            data[x * u64(4) + y * width * u64(4) + u64(1)] = u8(glm::clamp(src.y, f32(0.0), f32(1.0)) * f32(255.0));
            data[x * u64(4) + y * width * u64(4) + u64(2)] = u8(glm::clamp(src.z, f32(0.0), f32(1.0)) * f32(255.0));
            data[x * u64(4) + y * width * u64(4) + u64(3)] = u8(glm::clamp(src.w, f32(0.0), f32(1.0)) * f32(255.0));
        }
    }
    stbi_write_png(filename, i32(width), i32(height), STBI_rgb_alpha, &data[0], i32(width) * i32(4));
}
} // namespace
void wait_idle() {
    // gfxFrame(g_gfx);
    GfxInternal *gfx = GfxInternal::GetGfx(g_gfx);
    gfx->finish();
}
uint64_t timeSinceEpochMillisec() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
#undef main
int main() {

    InitGlobalState();

    float vertices[] = {
        -1.0f, -1.0f, 0.0f, //
        3.0f,  -1.0f, 0.0f, //
        -1.0f, 3.0f,  0.0f,
    };
    auto vertex_buffer = gfxCreateBuffer(g_gfx, sizeof(vertices), vertices);

    auto program = gfxCreateProgram(g_gfx, "triangle", "src/shaders/");
    assert(program);

    auto fill_color_program = gfxCreateProgram(g_gfx, "cube", "src/shaders/");
    assert(fill_color_program);
    auto trace_primary_kernel     = gfxCreateComputeKernel(g_gfx, fill_color_program, "trace_primary");
    auto clear_counters_kernel    = gfxCreateComputeKernel(g_gfx, fill_color_program, "clear_counters");
    auto draw_lines_kernel        = gfxCreateComputeKernel(g_gfx, fill_color_program, "draw_lines");
    auto count_lines_kernel       = gfxCreateComputeKernel(g_gfx, fill_color_program, "count_lines");
    auto prepare_lines_arg_kernel = gfxCreateComputeKernel(g_gfx, fill_color_program, "prepare_lines_arg");
    auto bake_noise_kernel        = gfxCreateComputeKernel(g_gfx, fill_color_program, "bake_noise");
    auto taa_kernel               = gfxCreateComputeKernel(g_gfx, fill_color_program, "taa");
    auto update_water_kernel      = gfxCreateComputeKernel(g_gfx, fill_color_program, "update_water");
    auto simulate_water_kernel    = gfxCreateComputeKernel(g_gfx, fill_color_program, "simulate_water");
    // auto clear_hit_counters_kernel = gfxCreateComputeKernel(g_gfx, fill_color_program, "clear_hit_counters");
    // auto init_indirect_args_kernel = gfxCreateComputeKernel(g_gfx, fill_color_program, "init_indirect_args");
    // auto process_hits_kernel       = gfxCreateComputeKernel(g_gfx, fill_color_program, "process_hits");
    // auto process_spheres_kernel    = gfxCreateComputeKernel(g_gfx, fill_color_program, "process_spheres");
    // auto update_aabbs_kernel       = gfxCreateComputeKernel(g_gfx, fill_color_program, "update_aabbs");
    // auto compact_hits_kernel       = gfxCreateComputeKernel(g_gfx, fill_color_program, "compact_hits");

    auto kernel = gfxCreateGraphicsKernel(g_gfx, program);

    u32 water_buffer_size = u32(1 << 10);
    f32 water_plane_size  = f32(128.0);

    // GfxTexture back_buffer = gfxCreateTexture2D(g_gfx, g_window_size.x, g_window_size.y, DXGI_FORMAT_R32G32B32A32_FLOAT);
    GfxTexture back_buffer      = gfxCreateTexture2D(g_gfx, DXGI_FORMAT_R16G16B16A16_FLOAT);
    GfxTexture taa_buffer       = gfxCreateTexture2D(g_gfx, DXGI_FORMAT_R16G16B16A16_FLOAT);
    GfxTexture water_buffer_0   = gfxCreateTexture2D(g_gfx, water_buffer_size, water_buffer_size, DXGI_FORMAT_R16G16B16A16_FLOAT);
    GfxTexture water_buffer_1   = gfxCreateTexture2D(g_gfx, water_buffer_size, water_buffer_size, DXGI_FORMAT_R16G16B16A16_FLOAT);
    GfxTexture shadow_targets[] = {
        gfxCreateTexture2D(g_gfx, DXGI_FORMAT_R8_UNORM), //
        gfxCreateTexture2D(g_gfx, DXGI_FORMAT_R8_UNORM), //
    };
    GfxTexture gi_targets[] = {
        gfxCreateTexture2D(g_gfx, DXGI_FORMAT_R16G16B16A16_FLOAT), //
        gfxCreateTexture2D(g_gfx, DXGI_FORMAT_R16G16B16A16_FLOAT), //
    };
    GfxTexture noise_texture = gfxCreateTexture2D(g_gfx, u32(128), u32(128), DXGI_FORMAT_R8G8_UNORM);

    GfxSamplerState linear_sampler = gfxCreateSamplerState(g_gfx, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

    struct LineCmd {
        /*i32x2 o;
        i32x2 e;*/
        f32x3 o;
        u32   pad0;
        f32x3 e;
        u32   pad1;
        f32x3 c;
        u32   flags;
    };

    std::vector<LineCmd> gfx_lines = {};

    auto push_cube_lines = [&](f32x3 lo, f32x3 hi, f32x3 c, bool bold) {
        gfx_lines.push_back({f32x3(lo.x, lo.y, lo.z), u32(0), f32x3(hi.x, lo.y, lo.z), u32(0), c, u32(bold ? u32(1) : u32(0))});
        gfx_lines.push_back({f32x3(lo.x, hi.y, lo.z), u32(0), f32x3(hi.x, hi.y, lo.z), u32(0), c, u32(bold ? u32(1) : u32(0))});
        gfx_lines.push_back({f32x3(lo.x, lo.y, hi.z), u32(0), f32x3(hi.x, lo.y, hi.z), u32(0), c, u32(bold ? u32(1) : u32(0))});
        gfx_lines.push_back({f32x3(lo.x, hi.y, hi.z), u32(0), f32x3(hi.x, hi.y, hi.z), u32(0), c, u32(bold ? u32(1) : u32(0))});

        gfx_lines.push_back({f32x3(lo.x, lo.y, lo.z), u32(0), f32x3(lo.x, hi.y, lo.z), u32(0), c, u32(bold ? u32(1) : u32(0))});
        gfx_lines.push_back({f32x3(hi.x, lo.y, lo.z), u32(0), f32x3(hi.x, hi.y, lo.z), u32(0), c, u32(bold ? u32(1) : u32(0))});
        gfx_lines.push_back({f32x3(lo.x, lo.y, hi.z), u32(0), f32x3(lo.x, hi.y, hi.z), u32(0), c, u32(bold ? u32(1) : u32(0))});
        gfx_lines.push_back({f32x3(hi.x, lo.y, hi.z), u32(0), f32x3(hi.x, hi.y, hi.z), u32(0), c, u32(bold ? u32(1) : u32(0))});

        gfx_lines.push_back({f32x3(lo.x, lo.y, lo.z), u32(0), f32x3(lo.x, lo.y, hi.z), u32(0), c, u32(bold ? u32(1) : u32(0))});
        gfx_lines.push_back({f32x3(hi.x, lo.y, lo.z), u32(0), f32x3(hi.x, lo.y, hi.z), u32(0), c, u32(bold ? u32(1) : u32(0))});
        gfx_lines.push_back({f32x3(lo.x, hi.y, lo.z), u32(0), f32x3(lo.x, hi.y, hi.z), u32(0), c, u32(bold ? u32(1) : u32(0))});
        gfx_lines.push_back({f32x3(hi.x, hi.y, lo.z), u32(0), f32x3(hi.x, hi.y, hi.z), u32(0), c, u32(bold ? u32(1) : u32(0))});
    };

    // ifor(1000) gfx_lines.push_back(LineCmd{f32x3(0.0, 0.0, 0.0), f32x3(f32(100.0), f32(100.0) + f32(1.0) * i, f32(0.0))});

    /*render(g_frame_index++, f32(0.001));
    GfxBuffer dump_buffer = write_texture_to_buffer(back_buffer);
    wait_idle();

    f32x4 *host_rgba_f32x4 = gfxBufferGetData<f32x4>(g_gfx, dump_buffer);

    write_f32x4_png("out.png", host_rgba_f32x4, back_buffer.getWidth(), back_buffer.getHeight());*/

    double cur_time       = f64(timeSinceEpochMillisec());
    double cur_delta_time = 0.0;
    bool   show_imgui     = true;

    u32 g_frame_index = u32(0);

    f32 g_time = f32(0.0);

    ImVec2 prev_mpos = {};

    u32 picked_primitive = u32(-1);

    for (auto time = 0.0f; !gfxWindowIsCloseRequested(g_window); time += 0.1f) {
        gfxWindowPumpEvents(g_window);

        g_audio_helper.GarbageCollect();

        g_frame_index++;

        double this_time  = f64(timeSinceEpochMillisec());
        double delta_time = this_time - cur_time;
        cur_time          = this_time;

        g_time += f32(delta_time / 1000.0);

        cur_delta_time += 0.1 * (delta_time - cur_delta_time);

        u32 buffer_width  = gfxGetBackBufferWidth(g_gfx);
        u32 buffer_height = gfxGetBackBufferHeight(g_gfx);

        ImVec2 mpos      = ImGui::GetMousePos();
        f32x2  mouse_uv  = (f32x2(mpos.x, mpos.y) + f32x2(0.5, 0.5)) / f32x2(buffer_width, buffer_height);
        Ray    mouse_ray = g_camera.GenRay(mouse_uv);

        f32 scalex = f32(buffer_width) / g_camera.scale;
        f32 scaley = f32(buffer_height) / g_camera.scale;

        g_camera.yscale = f32(buffer_height) / g_camera.scale;
        g_camera.xscale = f32(buffer_width) / g_camera.scale;

        if (prev_mpos.x != mpos.x || prev_mpos.y != mpos.y) {
            f32 cur_t        = f32(1.0e6);
            picked_primitive = u32(-1);
            g_scene.cpu_bvh.root->AnyHit(mouse_ray, [&](cpubvh::Node *n) {
                if (n->IsLeaf()) {
                    cpubvh::LeafNode *ln          = (cpubvh::LeafNode *)n;
                    i32x3             ipos        = g_scene.ipos[ln->primitive_idx];
                    Material          material    = g_scene.materials[ln->primitive_idx];
                    AABB              aabb        = g_scene.aabbs[ln->primitive_idx];
                    f32x2             hit_min_max = AABB::hit_aabb(mouse_ray.o, mouse_ray.ird, f32x3(ipos), f32x3(ipos + i32x3(1, 1, 1)));
                    if (hit_min_max.x < cur_t) {
                        cur_t            = hit_min_max.x;
                        picked_primitive = ln->primitive_idx;
                    }
                }
                return false;
            });
        }
        prev_mpos = mpos;

        // gfxProgramSetParameter(g_gfx, fill_color_program, "g_light_config", g_light_config);
        gfx_lines.resize(0);

        bool dirty = false;

        ImGuiContext &g          = *GImGui;
        bool          ui_hovered = g.HoveredWindow != NULL;

        if (picked_primitive != u32(-1)) {
            // f32x3 p = cur_t * mouse_ray.d + mouse_ray.o;

            i32x3    ipos     = g_scene.ipos[picked_primitive];
            Material material = g_scene.materials[picked_primitive];
            AABB     aabb     = g_scene.aabbs[picked_primitive];
            f32x3    n        = f32x3(0.0, 1.0, 0.0); // pick_cube_normal(p - (f32x3(ipos) + f32x3(0.5, 0.5, 0.5)));

            /*        push_cube_lines(f32x3(ipos + i32x3(n * f32(1.1))), f32x3(ipos + i32x3(n * f32(1.1)) + i32x3(1, 1, 1)),
                                    f32x3(sin(cur_time / 1000.0) * f32(0.5) + f32(0.5), cos(cur_time / 1000.0) * f32(0.5) + f32(0.5), 0.0), true);*/
            push_cube_lines(f32x3(ipos + i32x3(n * f32(1.1))), f32x3(ipos + i32x3(n * f32(1.1)) + i32x3(1, 1, 1)), f32x3(g_block_color[0], g_block_color[1], g_block_color[2]),
                            true);

            if (ui_hovered == false) {
                if (ImGui::IsMouseDown(0) && ImGui::IsKeyDown('B') || ImGui::IsMouseClicked(0)) {
                    CubeCreateInfo cinfo = {};
                    cinfo.ipos           = ipos;
                    cinfo.ipos += i32x3(n * f32(1.1));
                    if (g_scene.cpu_bvh.root->CheckAny(f32x3(cinfo.ipos) + f32x3(0.5, 0.5, 0.5)) == false) {
                        cinfo.material          = material;
                        cinfo.material.albedo   = f32x3(g_block_color[0], g_block_color[1], g_block_color[2]);
                        cinfo.material.emission = f32x3(g_block_emissiveness[0], g_block_emissiveness[1], g_block_emissiveness[2]) * g_block_emission_power;
                        if (g_block_transparent) {
                            cinfo.material.transparency = f32(1.0);
                            cinfo.material.metalic      = f32(0.0);
                        } else {
                            cinfo.material.transparency = f32(0.0);
                            cinfo.material.metalic      = g_block_metalness ? f32(1.0) : f32(0.0);
                        }
                        cinfo.material.roughness = f32(0.05);
                        picked_primitive         = g_scene.AddCube(cinfo);
                        dirty                    = true;
                        u32 effect               = pcg(g_frame_index) % ARRAYSIZE(creation_sound_effect);
                        // ifor(ARRAYSIZE(creation_sound_effect))
                        g_audio_helper.Push(new SineWave(               //
                            creation_sound_effect[effect].duration,     //
                            creation_sound_effect[effect].amplitude,    //
                            creation_sound_effect[effect].frequency0,   //
                            creation_sound_effect[effect].frequency1,   //
                            creation_sound_effect[effect].frequency_dt, //
                            creation_sound_effect[effect].fade_in_gain, //
                            creation_sound_effect[effect].fade_out_gain));
                    }
                } else if (ImGui::IsMouseDown(1) && ImGui::IsKeyDown('B') || ImGui::IsMouseClicked(1)) {
                    g_scene.RemoveCube(picked_primitive);

                    g_audio_helper.Push(new SineWave(          //
                        destruction_sound_effect.duration,     //
                        destruction_sound_effect.amplitude,    //
                        destruction_sound_effect.frequency0,   //
                        destruction_sound_effect.frequency1,   //
                        destruction_sound_effect.frequency_dt, //
                        destruction_sound_effect.fade_in_gain, //
                        destruction_sound_effect.fade_out_gain));
                    picked_primitive = u32(-1);
                    dirty            = true;
                }
            }
        }
        if (dirty) UpdateBVH();

        gfxProgramSetParameter(g_gfx, fill_color_program, "g_delta_time", f32(delta_time / 1000.0));
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_color", f32x4(1.0, 1.0, 0.0, 1.0));
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_output", back_buffer);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_camera_pos", g_camera.pos);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_camera_look", g_camera.look);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_camera_up", g_camera.up);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_camera_right", g_camera.right);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_camera_fov", g_camera.fov);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_camera_yscale", f32(buffer_height) / g_camera.scale);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_camera_xscale", f32(buffer_width) / g_camera.scale);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_tlas", g_bvh.as);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_aabb_buffer", g_aabb_buffer);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_material_buffer", g_material_buffer);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_frame_index", g_frame_index);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_time", g_time);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_linear_sampler", linear_sampler);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_water_buffer_size", f32(water_buffer_size));
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_sobol_buffer", g_sobol_buffer);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_water_buffer", (g_frame_index & 1) ? water_buffer_0 : water_buffer_1);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_rw_water_buffer_prev", (g_frame_index & 1) ? water_buffer_1 : water_buffer_0);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_rw_water_buffer", (g_frame_index & 1) ? water_buffer_0 : water_buffer_1);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_water_plane_size", water_plane_size);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_ranking_tile_buffer", g_ranking_tile_buffer);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_scrambling_tile_buffer", g_scrambling_tile_buffer);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_radiance_hash_table", g_radiance_hash_table);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_noise_texture", noise_texture);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_temporal_accumulated_output", taa_buffer);
        gfxProgramSetParameter(g_gfx, fill_color_program, "g_env_color", f32x3(g_env_color[0], g_env_color[1], g_env_color[2]));

        {
            u32 const *num_threads  = gfxKernelGetNumThreads(g_gfx, bake_noise_kernel);
            u32        num_groups_x = (128 + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (128 + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(g_gfx, bake_noise_kernel);
            gfxCommandDispatch(g_gfx, num_groups_x, num_groups_y, 1);
        }

        {
            gfxCommandBindKernel(g_gfx, clear_counters_kernel);
            gfxCommandDispatch(g_gfx, 1, 1, 1);
        }

        {
            u32 const *num_threads  = gfxKernelGetNumThreads(g_gfx, update_water_kernel);
            u32        num_groups_x = (water_buffer_size + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (water_buffer_size + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(g_gfx, update_water_kernel);
            gfxCommandDispatch(g_gfx, num_groups_x, num_groups_y, 1);
        }
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(g_gfx, simulate_water_kernel);
            u32        num_groups_x = (water_buffer_size + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (water_buffer_size + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(g_gfx, simulate_water_kernel);
            gfxCommandDispatch(g_gfx, num_groups_x, num_groups_y, 1);
        }
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(g_gfx, trace_primary_kernel);
            u32        num_groups_x = (buffer_width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (buffer_height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(g_gfx, trace_primary_kernel);
            gfxCommandDispatch(g_gfx, num_groups_x, num_groups_y, 1);
        }
        if (gfx_lines.size())
        {

            GfxBuffer lines_buffer = gfxCreateBuffer(g_gfx, sizeof(gfx_lines[0]) * gfx_lines.size(), &gfx_lines[0]);
            lines_buffer.setStride(u32(sizeof(LineCmd)));
            GfxBuffer line_pixels_indirect_buffer = gfxCreateBuffer(g_gfx, sizeof(u32) * 4);
            GfxBuffer line_pixels_scan_buffer     = gfxCreateBuffer(g_gfx, sizeof(u32) * gfx_lines.size());
            GfxBuffer line_pixels_cnt_buffer      = gfxCreateBuffer(g_gfx, sizeof(u32) * gfx_lines.size());
            line_pixels_scan_buffer.setStride(u32(sizeof(u32)));
            line_pixels_cnt_buffer.setStride(u32(sizeof(u32)));
            line_pixels_indirect_buffer.setStride(u32(sizeof(u32)));
            defer(gfxDestroyBuffer(g_gfx, line_pixels_indirect_buffer));
            defer(gfxDestroyBuffer(g_gfx, line_pixels_scan_buffer));
            defer(gfxDestroyBuffer(g_gfx, line_pixels_cnt_buffer));

            gfxProgramSetParameter(g_gfx, fill_color_program, "g_lines_buffer", lines_buffer);
            gfxProgramSetParameter(g_gfx, fill_color_program, "g_line_pixels_cnt_buffer", line_pixels_cnt_buffer);
            gfxProgramSetParameter(g_gfx, fill_color_program, "g_line_pixels_scan_buffer", line_pixels_scan_buffer);
            gfxProgramSetParameter(g_gfx, fill_color_program, "g_line_pixels_indirect_buffer", line_pixels_indirect_buffer);
            gfxProgramSetParameter(g_gfx, fill_color_program, "g_num_lines", u32(gfx_lines.size()));

            u32 const *num_threads = gfxKernelGetNumThreads(g_gfx, draw_lines_kernel);
            // u32        num_groups_x = (total_line_pixels + num_threads[0] - 1) / num_threads[0];
            u32 num_groups_x = (u32(gfx_lines.size()) + num_threads[0] - u32(1)) / num_threads[0];

            gfxCommandBindKernel(g_gfx, count_lines_kernel);
            gfxCommandDispatch(g_gfx, num_groups_x, 1, 1);

            gfxCommandScanSum(g_gfx, GfxDataType::kGfxDataType_Uint, line_pixels_scan_buffer, line_pixels_cnt_buffer);

            gfxCommandBindKernel(g_gfx, prepare_lines_arg_kernel);
            gfxCommandDispatch(g_gfx, 1, 1, 1);

            gfxCommandBindKernel(g_gfx, draw_lines_kernel);
            gfxCommandDispatchIndirect(g_gfx, line_pixels_indirect_buffer);
        }

        {
            u32 const *num_threads  = gfxKernelGetNumThreads(g_gfx, taa_kernel);
            u32        num_groups_x = (buffer_width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (buffer_height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(g_gfx, taa_kernel);
            gfxCommandDispatch(g_gfx, num_groups_x, num_groups_y, 1);
        }

        gfxProgramSetParameter(g_gfx, program, "g_input", taa_buffer);

        gfxCommandBindKernel(g_gfx, kernel);
        gfxCommandBindVertexBuffer(g_gfx, vertex_buffer);

        gfxCommandDraw(g_gfx, 3);

        if (ImGui::IsKeyDown('W')) g_camera.pos += f32(6.0) / g_camera.scale * normalize(f32x3(g_camera.up.x, f32(0.0), g_camera.up.z));
        if (ImGui::IsKeyDown('S')) g_camera.pos -= f32(6.0) / g_camera.scale * normalize(f32x3(g_camera.up.x, f32(0.0), g_camera.up.z));
        if (ImGui::IsKeyDown('D')) g_camera.pos += f32(6.0) / g_camera.scale * normalize(f32x3(g_camera.right.x, f32(0.0), g_camera.right.z));
        if (ImGui::IsKeyDown('A')) g_camera.pos -= f32(6.0) / g_camera.scale * normalize(f32x3(g_camera.right.x, f32(0.0), g_camera.right.z));
        if (ImGui::IsKeyDown('E')) g_camera.scale += g_camera.scale * f32(0.01);
        if (ImGui::IsKeyDown('Q')) g_camera.scale -= g_camera.scale * f32(0.01);
        g_camera.scale = std::max(f32(0.01), std::min(f32(512.0), g_camera.scale));

        g_camera.pos.y = std::max(f32(1.0e-3), g_camera.pos.y);

        if (ImGui::IsKeyDown('X')) show_imgui = !show_imgui;
        // if (ImGui::IsKeyReleased('R')) {
        if (ImGui::IsKeyDown('R') || ImGui::IsKeyDown('T')) {
            f32 sign = ImGui::IsKeyDown('R') ? f32(1.0) : f32(-1.0);
            if (picked_primitive != u32(-1)) {

                f32x3 mp = g_scene.aabbs[picked_primitive].mid(); // mouse_ray.o + mouse_ray.d * cur_t;

                f32   t = abs(g_camera.pos.y - mp.y);
                f32   r = -t / g_camera.look.y;
                f32x3 d = g_camera.look * r;
                f32x3 p = g_camera.pos + g_camera.look * r;
                // g_camera.pos     = -f32x3(d.z, d.y, -d.x) + p;
                // g_camera.pos     = (glm::rotate(f32x4x4(1.0), f32(glm::pi<f32>() / f32(32.0)), f32x3(0.0, 1.0, 0.0)) * f32x4(-d, f32(1.0))).xyz + p;
                g_camera.pos = (glm::rotate(f32x4x4(1.0), sign * f32(delta_time / 1000.0) * f32(glm::pi<f32>() / f32(2.0)), f32x3(0.0, 1.0, 0.0)) * f32x4(-d, f32(1.0))).xyz + p;
                g_camera.look_at = p;
            } else {
                f32   t = g_camera.pos.y;
                f32   r = -t / g_camera.look.y;
                f32x3 d = g_camera.look * r;
                f32x3 p = g_camera.pos + g_camera.look * r;
                // g_camera.pos     = -f32x3(d.z, d.y, -d.x) + p;
                // g_camera.pos     = (glm::rotate(f32x4x4(1.0), f32(glm::pi<f32>() / f32(32.0)), f32x3(0.0, 1.0, 0.0)) * f32x4(-d, f32(1.0))).xyz + p;
                g_camera.pos = (glm::rotate(f32x4x4(1.0), sign * f32(delta_time / 1000.0) * f32(glm::pi<f32>() / f32(2.0)), f32x3(0.0, 1.0, 0.0)) * f32x4(-d, f32(1.0))).xyz + p;
                g_camera.look_at = p;
            }
            g_camera.UpdateMatrices();
        }

        if (show_imgui) {
            ImGui::Begin("Audio");

            // ImGui::DragFloat("a_dump", &g_audio_helper.a_dump, f32(0.0001));
            // ImGui::DragFloat("a_k", &g_audio_helper.a_k, f32(0.01));
            // ImGui::DragFloat("a_kk", &g_audio_helper.a_kk, f32(0.01));
            auto ui_se = [&](char const *name, SinSoundEffect &se) {
                if (ImGui::TreeNode(name)) {
                    ImGui::DragFloat("duration", &se.duration, f32(0.01));
                    ImGui::DragFloat("amplitude", &se.amplitude, f32(0.01));
                    ImGui::DragFloat("frequency0", &se.frequency0, f32(0.01));
                    ImGui::DragFloat("frequency1", &se.frequency1, f32(0.01));
                    ImGui::DragFloat("frequency_dt", &se.frequency_dt, f32(0.01));
                    ImGui::DragFloat("fade_in_gain", &se.fade_in_gain, f32(0.01));
                    ImGui::DragFloat("fade_out_gain", &se.fade_out_gain, f32(0.01));

                    if (ImGui::Button("Play")) {
                        g_audio_helper.Push(new SineWave(se.duration,     //
                                                         se.amplitude,    //
                                                         se.frequency0,   //
                                                         se.frequency1,   //
                                                         se.frequency_dt, //
                                                         se.fade_in_gain, //
                                                         se.fade_out_gain));
                    }
                    ImGui::TreePop();
                }
            };
            ifor(ARRAYSIZE(creation_sound_effect)) {
                char buf[100];
                snprintf(buf, sizeof(buf), "creation_sound_effect_%i", i);
                ui_se(buf, creation_sound_effect[i]);
            }
            ui_se("destruction_sound_effect", destruction_sound_effect);

            ImGui::End();

            ImGui::Begin("Config");
            ImGui::ColorEdit3("Env Color", g_env_color);
            ImGui::ColorEdit3("Block Color", g_block_color);
            ImGui::ColorEdit3("Block Emission", g_block_emissiveness);
            ImGui::DragFloat("Block Emission Power", &g_block_emission_power);
            ImGui::Checkbox("Block Metalness", &g_block_metalness);
            ImGui::Checkbox("Block Transparency", &g_block_transparent);
            // ImGui::Text("Frame Time ms %f", cur_delta_time);
            ImGui::End();
        }
        gfxImGuiRender();
        gfxFrame(g_gfx);
    }

    Store();

    gfxImGuiTerminate();
    gfxDestroyContext(g_gfx);
    gfxDestroyWindow(g_window);

    return 0;
}