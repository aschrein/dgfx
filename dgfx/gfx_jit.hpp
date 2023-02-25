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

#if !defined(GFX_JIT_HPP)
#    define GFX_JIT_HPP

#    include "gfx_imgui.h"
#    include "gfx_scene.h"
#    include "gfx_window.h"

#    include "bake_noise.hpp"
#    include "camera.hpp"
#    include "common.h"
#    include "file_io.hpp"
#    include "gfx_utils.hpp"
#    include "gizmo.hpp"
#    include "jit.hpp"

struct Mesh {
    uint32_t count;
    uint32_t first_index;
    uint32_t base_vertex;
    uint32_t material;
};

namespace {

struct Material {
    f32x4 albedo;
    f32x4 metallicity_roughness;
    f32x4 ao_normal_emissivity;
};

struct Instance {
    uint32_t mesh_id;
};

struct Vertex {
    f32x4     position;
    f32x4     normal;
    glm::vec2 uv;
};

} // namespace

struct GpuScene {
    GfxContext gfx;

    GfxScene          scene;
    std::vector<Mesh> meshes;

    GfxBuffer mesh_buffer;
    GfxBuffer index_buffer;
    GfxBuffer vertex_buffer;
    GfxBuffer instance_buffer;
    GfxBuffer material_buffer;
    GfxBuffer transform_buffer;
    GfxBuffer previous_transform_buffer;
    GfxBuffer upload_transform_buffers[kGfxConstant_BackBufferCount];

    std::vector<GfxTexture> textures;

    GfxSamplerState texture_sampler;

    std::vector<GfxRaytracingPrimitive> raytracing_primitives;
    GfxAccelerationStructure            acceleration_structure;

    std::vector<Instance> instances;
    std::vector<f32x4x4>  transforms;

    void BuildTLAS(bool _invalidate = false) {
        bool create_tlas = gfxIsRaytracingSupported(gfx);

        if (!create_tlas) return;

        if (acceleration_structure && _invalidate == false) return;

        for (auto &t : raytracing_primitives)
            if (t) {
                gfxDestroyRaytracingPrimitive(gfx, t);
                t = {};
            }

        if (acceleration_structure) gfxDestroyAccelerationStructure(gfx, acceleration_structure);

        acceleration_structure = gfxCreateAccelerationStructure(gfx);

        for (uint32_t i = 0; i < gfxSceneGetInstanceCount(scene); ++i) {
            GfxConstRef<GfxInstance> const instance_ref = gfxSceneGetInstanceHandle(scene, i);

            Instance             instance = {};
            GfxConstRef<GfxMesh> mesh_ref = gfxSceneGetMeshHandle(scene, i);
            instance.mesh_id              = (uint32_t)mesh_ref; // instance_ref->mesh;

            uint32_t const instance_id = (uint32_t)instance_ref;

            if (instance_id >= instances.size()) {
                instances.resize(instance_id + 1);
                transforms.resize(instance_id + 1);
                raytracing_primitives.resize(instance_id + 1);
            }

            instances[instance_id]  = instance;
            transforms[instance_id] = instance_ref->transform;

            GfxRaytracingPrimitive &rt_mesh = raytracing_primitives[instance_id];

            Mesh mesh = meshes[(u32)mesh_ref];
            rt_mesh   = gfxCreateRaytracingPrimitive(gfx, acceleration_structure);

            gfxRaytracingPrimitiveBuild(gfx, rt_mesh, index_buffer, mesh.first_index * u32(4), mesh.count, vertex_buffer, mesh.base_vertex * sizeof(Vertex), sizeof(Vertex),
                                        kGfxBuildRaytracingPrimitiveFlag_Opaque);

            f32x4x4 transform = glm::transpose(transforms[i]);

            // gfxRaytracingPrimitiveSetInstanceID(gfx, instance_id);
            gfxRaytracingPrimitiveSetTransform(gfx, rt_mesh, &transform[0][0]);
            gfxRaytracingPrimitiveSetInstanceID(gfx, rt_mesh, (u32)instance_ref);
        }
        gfxAccelerationStructureUpdate(gfx, acceleration_structure);
    }
};

static GpuScene UploadSceneToGpuMemory(GfxContext gfx, GfxScene scene);
static void     ReleaseGpuScene(GfxContext gfx, GpuScene const &gpu_scene);

static void UpdateGpuScene(GfxContext gfx, GfxScene scene, GpuScene &gpu_scene);
static void BindGpuScene(GfxContext gfx, GfxProgram program, GpuScene const &gpu_scene);

static GpuScene UploadSceneToGpuMemory(GfxContext gfx, GfxScene scene) {
    GpuScene gpu_scene = {};

    gpu_scene.scene = scene;
    gpu_scene.gfx   = gfx;

    // Load our materials
    std::vector<Material> materials;

    for (uint32_t i = 0; i < gfxSceneGetMaterialCount(scene); ++i) {
        GfxConstRef<GfxMaterial> material_ref = gfxSceneGetMaterialHandle(scene, i);

        Material material              = {};
        material.albedo                = f32x4(f32x3(material_ref->albedo), glm::uintBitsToFloat((uint32_t)material_ref->albedo_map));
        material.metallicity_roughness = f32x4(material_ref->metallicity, glm::uintBitsToFloat((uint32_t)material_ref->metallicity_map), material_ref->roughness,
                                               glm::uintBitsToFloat((uint32_t)material_ref->roughness_map));
        material.ao_normal_emissivity  = f32x4(glm::uintBitsToFloat((uint32_t)material_ref->ao_map), glm::uintBitsToFloat((uint32_t)material_ref->normal_map),
                                               glm::uintBitsToFloat((uint32_t)material_ref->emissivity_map), 0.0f);

        uint32_t const material_id = (uint32_t)material_ref;

        if (material_id >= materials.size()) {
            materials.resize(material_id + 1);
        }

        materials[material_id] = material;
    }

    gpu_scene.material_buffer = gfxCreateBuffer<Material>(gfx, (uint32_t)materials.size(), materials.data());

    // Load our meshes
    uint32_t first_index = 0;
    uint32_t base_vertex = 0;

    for (uint32_t i = 0; i < gfxSceneGetMeshCount(scene); ++i) {
        GfxConstRef<GfxMesh> mesh_ref = gfxSceneGetMeshHandle(scene, i);

        Mesh mesh        = {};
        mesh.count       = (uint32_t)mesh_ref->indices.size();
        mesh.first_index = first_index;
        mesh.base_vertex = base_vertex;
        mesh.material    = (uint32_t)mesh_ref->material;
        // mesh.num_vertices = (uint32_t)mesh_ref->vertices.size();

        uint32_t const mesh_id = (uint32_t)mesh_ref;

        if (mesh_id >= gpu_scene.meshes.size()) {
            gpu_scene.meshes.resize(mesh_id + 1);
        }

        gpu_scene.meshes[mesh_id] = mesh;

        first_index += (uint32_t)mesh_ref->indices.size();
        base_vertex += (uint32_t)mesh_ref->vertices.size();
    }

    gpu_scene.mesh_buffer = gfxCreateBuffer<Mesh>(gfx, (uint32_t)gpu_scene.meshes.size(), gpu_scene.meshes.data());

    // Load our vertices
    std::vector<uint32_t> indices;
    std::vector<Vertex>   vertices;

    for (uint32_t i = 0; i < gfxSceneGetMeshCount(scene); ++i) {
        GfxConstRef<GfxMesh> mesh_ref = gfxSceneGetMeshHandle(scene, i);

        std::vector<uint32_t> const &index_buffer = mesh_ref->indices;

        for (uint32_t index : index_buffer) {
            indices.push_back(index);
        }

        std::vector<GfxVertex> const &vertex_buffer = mesh_ref->vertices;

        for (GfxVertex vertex : vertex_buffer) {
            Vertex gpu_vertex = {};

            gpu_vertex.position = f32x4(vertex.position, 1.0f);
            gpu_vertex.normal   = f32x4(vertex.normal, 0.0f);
            gpu_vertex.uv       = glm::vec2(vertex.uv);

            vertices.push_back(gpu_vertex);
        }
    }

    gpu_scene.index_buffer  = gfxCreateBuffer<uint32_t>(gfx, (uint32_t)indices.size(), indices.data());
    gpu_scene.vertex_buffer = gfxCreateBuffer<Vertex>(gfx, (uint32_t)vertices.size(), vertices.data());

    // Load our instances
    std::vector<Instance> instances;
    std::vector<f32x4x4>  transforms;

    for (uint32_t i = 0; i < gfxSceneGetInstanceCount(scene); ++i) {
        GfxConstRef<GfxInstance> const instance_ref = gfxSceneGetInstanceHandle(scene, i);

        Instance             instance = {};
        GfxConstRef<GfxMesh> mesh_ref = gfxSceneGetMeshHandle(scene, i);
        instance.mesh_id              = (uint32_t)mesh_ref; // instance_ref->mesh;

        uint32_t const instance_id = (uint32_t)instance_ref;

        if (instance_id >= instances.size()) {
            instances.resize(instance_id + 1);
            transforms.resize(instance_id + 1);
            gpu_scene.raytracing_primitives.resize(instance_id + 1);
        }

        instances[instance_id]  = instance;
        transforms[instance_id] = instance_ref->transform;
    }

    gpu_scene.instance_buffer           = gfxCreateBuffer<Instance>(gfx, (uint32_t)instances.size(), instances.data());
    gpu_scene.transform_buffer          = gfxCreateBuffer<f32x4x4>(gfx, (uint32_t)transforms.size(), transforms.data());
    gpu_scene.previous_transform_buffer = gfxCreateBuffer<f32x4x4>(gfx, (uint32_t)transforms.size(), transforms.data());

    for (GfxBuffer &upload_transform_buffer : gpu_scene.upload_transform_buffers) {
        upload_transform_buffer = gfxCreateBuffer<f32x4x4>(gfx, (uint32_t)transforms.size(), nullptr, kGfxCpuAccess_Write);
    }

    for (uint32_t i = 0; i < gfxSceneGetImageCount(scene); ++i) {
        GfxConstRef<GfxImage> const image_ref = gfxSceneGetImageHandle(scene, i);

        GfxTexture texture = gfxCreateTexture2D(gfx, image_ref->width, image_ref->height, image_ref->format, gfxCalculateMipCount(image_ref->width, image_ref->height));

        uint32_t const texture_size = image_ref->width * image_ref->height * image_ref->channel_count * image_ref->bytes_per_channel;

        GfxBuffer upload_texture_buffer = gfxCreateBuffer(gfx, texture_size, image_ref->data.data(), kGfxCpuAccess_Write);

        gfxCommandCopyBufferToTexture(gfx, texture, upload_texture_buffer);
        gfxDestroyBuffer(gfx, upload_texture_buffer);
        gfxCommandGenerateMips(gfx, texture);

        uint32_t const image_id = (uint32_t)image_ref;

        if (image_id >= gpu_scene.textures.size()) {
            gpu_scene.textures.resize(image_id + 1);
        }

        gpu_scene.textures[image_id] = texture;
    }

    gpu_scene.texture_sampler = gfxCreateSamplerState(gfx, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    return gpu_scene;
}

static void ReleaseGpuScene(GfxContext gfx, GpuScene const &gpu_scene) {
    gfxDestroyBuffer(gfx, gpu_scene.mesh_buffer);
    gfxDestroyBuffer(gfx, gpu_scene.index_buffer);
    gfxDestroyBuffer(gfx, gpu_scene.vertex_buffer);
    gfxDestroyBuffer(gfx, gpu_scene.instance_buffer);
    gfxDestroyBuffer(gfx, gpu_scene.material_buffer);
    gfxDestroyBuffer(gfx, gpu_scene.transform_buffer);
    gfxDestroyBuffer(gfx, gpu_scene.previous_transform_buffer);

    for (GfxBuffer upload_transform_buffer : gpu_scene.upload_transform_buffers) {
        gfxDestroyBuffer(gfx, upload_transform_buffer);
    }

    for (GfxTexture texture : gpu_scene.textures) {
        gfxDestroyTexture(gfx, texture);
    }

    gfxDestroySamplerState(gfx, gpu_scene.texture_sampler);
}

static void UpdateGpuScene(GfxContext gfx, GfxScene scene, GpuScene &gpu_scene) {
    GfxBuffer upload_transform_buffer = gpu_scene.upload_transform_buffers[gfxGetBackBufferIndex(gfx)];

    f32x4x4 *transforms = gfxBufferGetData<f32x4x4>(gfx, upload_transform_buffer);

    uint32_t const instance_count = gfxSceneGetInstanceCount(scene);

    for (uint32_t i = 0; i < instance_count; ++i) {
        GfxConstRef<GfxInstance> const instance_ref = gfxSceneGetInstanceHandle(scene, i);

        uint32_t const instance_id = (uint32_t)instance_ref;

        transforms[instance_id] = instance_ref->transform;
    }

    gfxCommandCopyBuffer(gfx, gpu_scene.previous_transform_buffer, gpu_scene.transform_buffer);

    gfxCommandCopyBuffer(gfx, gpu_scene.transform_buffer, upload_transform_buffer);

    gpu_scene.BuildTLAS();
}

static void BindGpuScene(GfxContext gfx, GfxProgram program, GpuScene const &gpu_scene) {
    gfxProgramSetParameter(gfx, program, "g_MeshBuffer", gpu_scene.mesh_buffer);
    gfxProgramSetParameter(gfx, program, "g_IndexBuffer", gpu_scene.index_buffer);
    gfxProgramSetParameter(gfx, program, "g_VertexBuffer", gpu_scene.vertex_buffer);
    gfxProgramSetParameter(gfx, program, "g_InstanceBuffer", gpu_scene.instance_buffer);
    gfxProgramSetParameter(gfx, program, "g_MaterialBuffer", gpu_scene.material_buffer);
    gfxProgramSetParameter(gfx, program, "g_TransformBuffer", gpu_scene.transform_buffer);
    gfxProgramSetParameter(gfx, program, "g_PreviousTransformBuffer", gpu_scene.previous_transform_buffer);
    gfxProgramSetParameter(gfx, program, "g_Textures", gpu_scene.textures.data(), (uint32_t)gpu_scene.textures.size());
    gfxProgramSetParameter(gfx, program, "g_TextureSampler", gpu_scene.texture_sampler);
}

namespace GfxJit {
using namespace SJIT;
using var = ValueExpr;

static SharedPtr<Type> Ray_Ty     = Type::Create("Ray", {
                                                        {"o", f32x3Ty},   //
                                                        {"d", f32x3Ty},   //
                                                        {"ird", f32x3Ty}, //
                                                    });
static SharedPtr<Type> RayDesc_Ty = Type::Create("RayDesc",
                                                 {
                                                     {"Direction", f32x3Ty}, //
                                                     {"Origin", f32x3Ty},    //
                                                     {"TMin", f32Ty},        //
                                                     {"TMax", f32Ty},        //
                                                 },
                                                 /* builtin */ true);
static ValueExpr       GenDiffuseRay(ValueExpr p, ValueExpr n, ValueExpr xi) {
    using var        = ValueExpr;
    var TBN          = GetTBN(n);
    var sint         = sqrt(xi["y"]);
    var cost         = sqrt(f32(1.0) - xi["y"]);
    var M_PI         = f32(3.14159265358979);
    var local_coords = make_f32x3(cost * cos(xi["x"] * M_PI * f32(2.0)), cost * sin(xi["x"] * M_PI * f32(2.0)), sint);
    var d            = normalize(TBN[u32(2)] * local_coords["z"] + TBN[u32(0)] * local_coords["x"] + TBN[u32(1)] * local_coords["y"]);
    var r            = Zero(Ray_Ty);
    r["o"]           = p + n * f32(1.0e-3);
    r["d"]           = d;
    r["ird"]         = f32x3(1.0, 1.0, 1.0) / r["d"];
    return r;
}

// https://www.shadertoy.com/view/4dsSzr

static ValueExpr hueGradient(ValueExpr t) {
    sjit_assert(t->InferType() == f32Ty);
    ValueExpr p = abs(frac(t["xxx"] + f32x3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * f32(6.0) - f32x3_splat(3.0));
    return clamp(p - f32x3_splat(1.0), f32x3_splat(0.0), f32x3_splat(1.0));
}

// https://www.shadertoy.com/view/ltB3zD

// Gold Noise �2015 dcerisano@standard3d.com
// - based on the Golden Ratio
// - uniform normalized distribution
// - fastest static noise generator function (also runs at low precision)
// - use with indicated fractional seeding method

static const f32 PHI = f32(1.61803398874989484820459); // Golden Ratio

static ValueExpr gold_noise(ValueExpr xy, ValueExpr seed) {
    sjit_assert(xy->InferType() == f32x2Ty);
    sjit_assert(seed->InferType() == f32Ty);
    return frac(tan(length(xy * PHI - xy) * seed) * xy.x());
}
static ValueExpr random_rgb(ValueExpr x) {
    sjit_assert(x->InferType() == f32Ty);
    return hueGradient(                                                                                                                                  //
        gold_noise(make_f32x2(frac(cos(abs(x) * f32(53.932) + f32(32.321))), frac(sin(-abs(x) * PHI * f32(37.254) + f32(17.354)))), f32(439753.1235389)) //
    );
}
static ValueExpr random_albedo(ValueExpr x) { return random_rgb(x) * f32(0.5) + f32x3_splat(0.5); }

enum ResourceType {
    RESOURCE_TYPE_UNKNOWN = 0,
    RESOURCE_TYPE_TEXTURE,
    RESOURCE_TYPE_BUFFER,
    RESOURCE_TYPE_SAMPLER,
    RESOURCE_TYPE_TLAS,
    RESOURCE_TYPE_U32,
    RESOURCE_TYPE_U32x2,
    RESOURCE_TYPE_U32x3,
    RESOURCE_TYPE_U32x4,
    RESOURCE_TYPE_I32,
    RESOURCE_TYPE_I32x2,
    RESOURCE_TYPE_I32x3,
    RESOURCE_TYPE_I32x4,
    RESOURCE_TYPE_F32,
    RESOURCE_TYPE_F32x2,
    RESOURCE_TYPE_F32x3,
    RESOURCE_TYPE_F32x4,
    RESOURCE_TYPE_F32x4x4,
};
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wreorder-ctor"
struct ResourceSlot {
    ResourceType             type          = RESOURCE_TYPE_UNKNOWN;
    GfxTexture               texture       = {};
    Array<GfxTexture>        textures      = {};
    GfxBuffer                buffer        = {};
    GfxAccelerationStructure tlas          = {};
    GfxSamplerState          sampler_state = {};
    union {
        u32     v_u32;
        u32x2   v_u32x2;
        u32x3   v_u32x3;
        u32x4   v_u32x4;
        i32     v_i32;
        i32x2   v_i32x2;
        i32x3   v_i32x3;
        i32x4   v_i32x4;
        f32     v_f32;
        f32x2   v_f32x2;
        f32x3   v_f32x3;
        f32x4   v_f32x4;
        f32x4x4 v_f32x4x4;
        struct {
            u8 m[16];
        } raw_data;
    };

    bool operator==(ResourceSlot const &that) const {
        if (type != that.type) return false;
        if (texture != that.texture) return false;
        if (buffer != that.buffer) return false;
        if (tlas != that.tlas) return false;
        if (sampler_state != that.sampler_state) return false;
        if (textures.size() != that.textures.size()) return false;
        ifor(textures.size()) if (textures[i] != that.textures[i]) return false;
        i32 cmp = memcmp(raw_data.m, that.raw_data.m, u32(16));
        return cmp == i32(0);
    }

    ResourceSlot() { memset(this, 0, sizeof(*this)); }
    ResourceSlot(GfxTexture _texture) : texture(_texture), type(RESOURCE_TYPE_TEXTURE) {}
    ResourceSlot(GfxTexture *_textures, u32 _num) : textures(_textures, _textures + _num), type(RESOURCE_TYPE_TEXTURE) {}
    ResourceSlot(GfxBuffer _buffer) : buffer(_buffer), type(RESOURCE_TYPE_BUFFER) {}
    ResourceSlot(GfxAccelerationStructure _tlas) : tlas(_tlas), type(RESOURCE_TYPE_TLAS) {}
    ResourceSlot(GfxSamplerState _sampler_state) : sampler_state(_sampler_state), type(RESOURCE_TYPE_SAMPLER) {}
    ResourceSlot(u32 _v) : v_u32(_v), type(RESOURCE_TYPE_U32) {}
    ResourceSlot(u32x2 _v) : v_u32x2(_v), type(RESOURCE_TYPE_U32x2) {}
    ResourceSlot(u32x3 _v) : v_u32x3(_v), type(RESOURCE_TYPE_U32x3) {}
    ResourceSlot(u32x4 _v) : v_u32x4(_v), type(RESOURCE_TYPE_U32x4) {}
    ResourceSlot(i32 _v) : v_i32(_v), type(RESOURCE_TYPE_I32) {}
    ResourceSlot(i32x2 _v) : v_i32x2(_v), type(RESOURCE_TYPE_I32x2) {}
    ResourceSlot(i32x3 _v) : v_i32x3(_v), type(RESOURCE_TYPE_I32x3) {}
    ResourceSlot(i32x4 _v) : v_i32x4(_v), type(RESOURCE_TYPE_I32x4) {}
    ResourceSlot(f32 _v) : v_f32(_v), type(RESOURCE_TYPE_F32) {}
    ResourceSlot(f32x2 _v) : v_f32x2(_v), type(RESOURCE_TYPE_F32x2) {}
    ResourceSlot(f32x3 _v) : v_f32x3(_v), type(RESOURCE_TYPE_F32x3) {}
    ResourceSlot(f32x4 _v) : v_f32x4(_v), type(RESOURCE_TYPE_F32x4) {}
    ResourceSlot(f32x4x4 _v) : v_f32x4x4(_v), type(RESOURCE_TYPE_F32x4x4) {}
};
#    pragma clang diagnostic pop

static SharedPtr<Type> Material_Ty = Type::Create("Material", {
                                                                  {"albedo", f32x4Ty},                //
                                                                  {"metallicity_roughness", f32x4Ty}, //
                                                                  {"ao_normal_emissivity", f32x4Ty},  //
                                                              });

static SharedPtr<Type> Mesh_Ty = Type::Create("Mesh", {
                                                          {"count", u32Ty},       //
                                                          {"first_index", u32Ty}, //
                                                          {"base_vertex", u32Ty}, //
                                                          {"material_id", u32Ty}, //
                                                      });

static SharedPtr<Type> GBuffer_Ty = Type::Create("GBuffer", {
                                                                {"P", f32x3Ty}, //
                                                                {"N", f32x3Ty}, //
                                                            });

static SharedPtr<Type> Instance_Ty = Type::Create("Instance", {
                                                                  {"mesh_id", u32Ty}, //
                                                              });

static SharedPtr<Type> Vertex_Ty = Type::Create("Vertex", {
                                                              {"position", f32x4Ty}, //
                                                              {"normal", f32x4Ty},   //
                                                              {"uv", f32x2Ty},       //
                                                          });

static HashMap<String, ResourceSlot> g_global_runtime_resource_registry = {};
template <typename T>
void set_global_resource(var access, T val) {
    g_global_runtime_resource_registry[access->GetResource()->GetName()] = val;
}
class IGfxResourceRegistryItem {
public:
    static constexpr u32 INVALID_ID = u32(0);

    u32 id = INVALID_ID;

    virtual void Update()                = 0;
    virtual var  Access(RWType _rw_type) = 0;
    virtual ~IGfxResourceRegistryItem() {}
};

template <typename T>
struct SlotManager {
    Array<T>   items      = {{}}; // item 0 is always an empty object
    Array<u32> free_items = {};

    u32 AddItem(T const &_item) {
        if (free_items.size()) {
            u32 id = free_items.back();
            sjit_debug_assert(id != IGfxResourceRegistryItem::INVALID_ID);
            free_items.pop_back();
            items[id] = _item;
            return id;
        } else {
            items.push_back(_item);
            return (u32)(items.size() - u64(1));
        }
    }
    void RemoveItem(u32 _id) {
        sjit_debug_assert(_id < u32(items.size()));
        sjit_debug_assert(_id != IGfxResourceRegistryItem::INVALID_ID);
        items[_id] = {};
        free_items.push_back(_id);
    }
};

class GfxResourceRegistry {
    SlotManager<IGfxResourceRegistryItem *>     items                     = {};
    HashMap<String, IGfxResourceRegistryItem *> runtime_resource_registry = {};

    static GfxResourceRegistry &Get() {
        static GfxResourceRegistry o = {};
        return o;
    }

    void _add_resource(IGfxResourceRegistryItem *_item) { _item->id = items.AddItem(_item); }
    void _remove_resource(IGfxResourceRegistryItem *_item) {
        items.RemoveItem(_item->id);
        delete _item;
    }
    void _update() {
        ifor(items.items.size()) {
            if (items.items[i]) {
                items.items[i]->Update();
            }
        }
    }
    void _release() {
        ifor(items.items.size()) {
            if (items.items[i]) {
                delete items.items[i];
            }
        }
        items = {};
    }

public:
    static void                                         AddResource(IGfxResourceRegistryItem *_item) { Get()._add_resource(_item); }
    static void                                         Update() { Get()._update(); }
    static HashMap<String, IGfxResourceRegistryItem *> &GetResources() { return Get().runtime_resource_registry; }
};
struct TimestampPool {
    static constexpr u32                    num_timesptams = u32(1 << 16);
    SlotManager<IGfxResourceRegistryItem *> items          = {};
    Array<GfxTimestampQuery>                timestamps     = {};
    GfxContext                              gfx            = {};

    void Init(GfxContext _gfx) {
        timestamps.resize(num_timesptams);
        // ifor(num_timesptams) { timestampst[i] = gfxCreateTimestampQuery(gfx); }
    }
    void Release() {}
};
struct GPUKernel {
    String                               name             = {};
    u32x3                                group_size       = {u32(8), u32(8), u32(1)};
    GfxProgram                           program          = {};
    GfxKernel                            kernel           = {};
    GfxContext                           gfx              = {};
    std::string                          isa              = {};
    u32                                  reg_pressure     = u32(0);
    HashMap<String, SharedPtr<Resource>> resources        = {};
    HashMap<String, ResourceSlot>        set_resources    = {};
    GfxTimestampQuery                    timestamps[3][2] = {};
    u32                                  timestamp_idx    = u32(0);
    f64                                  duration         = f64(0.0);

    void SetResource(char const *_name, ResourceSlot slot) {
        auto it = set_resources.find(_name);
        if (it != set_resources.end()) {
            if (it->second == slot) return; // no need
        }
        set_resources[_name] = slot;
        switch (slot.type) {
        case RESOURCE_TYPE_TEXTURE: {
            if (slot.textures.size())
                gfxProgramSetParameter(gfx, program, _name, slot.textures.data(), slot.textures.size());
            else
                gfxProgramSetParameter(gfx, program, _name, slot.texture);
        } break;
        case RESOURCE_TYPE_BUFFER: {
            gfxProgramSetParameter(gfx, program, _name, slot.buffer);
        } break;
        case RESOURCE_TYPE_SAMPLER: {
            gfxProgramSetParameter(gfx, program, _name, slot.sampler_state);
        } break;
        case RESOURCE_TYPE_TLAS: {
            gfxProgramSetParameter(gfx, program, _name, slot.tlas);
        } break;
        case RESOURCE_TYPE_U32: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_u32);
        } break;
        case RESOURCE_TYPE_U32x2: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_u32x2);
        } break;
        case RESOURCE_TYPE_U32x3: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_u32x3);
        } break;
        case RESOURCE_TYPE_U32x4: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_u32x4);
        } break;
        case RESOURCE_TYPE_I32: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_i32);
        } break;
        case RESOURCE_TYPE_I32x2: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_i32x2);
        } break;
        case RESOURCE_TYPE_I32x3: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_i32x3);
        } break;
        case RESOURCE_TYPE_I32x4: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_i32x4);
        } break;
        case RESOURCE_TYPE_F32: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_f32);
        } break;
        case RESOURCE_TYPE_F32x2: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_f32x2);
        } break;
        case RESOURCE_TYPE_F32x3: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_f32x3);
        } break;
        case RESOURCE_TYPE_F32x4: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_f32x4);
        } break;
        case RESOURCE_TYPE_F32x4x4: {
            gfxProgramSetParameter(gfx, program, _name, slot.v_f32x4x4);
        } break;
        default: {
            SJIT_TRAP;
        }
        };
    }
    template <typename T>
    void SetResource(ValueExpr res, T _v) {
        ResourceSlot slot = ResourceSlot(_v);
        auto         it   = set_resources.find(res->GetResource()->GetName().c_str());
        if (it != set_resources.end()) {
            if (it->second == slot) return; // no need
        }
        set_resources[res->GetResource()->GetName().c_str()] = slot;
        gfxProgramSetParameter(gfx, program, res->GetResource()->GetName().c_str(), _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v) {
        ResourceSlot slot = ResourceSlot(_v);
        auto         it   = set_resources.find(_name);
        if (it != set_resources.end()) {
            if (it->second == slot) return; // no need
        }
        set_resources[_name] = slot;
        gfxProgramSetParameter(gfx, program, _name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        ResourceSlot slot = ResourceSlot(_v, _num);
        auto         it   = set_resources.find(_name);
        if (it != set_resources.end()) {
            if (it->second == slot) return; // no need
        }
        set_resources[_name] = slot;
        gfxProgramSetParameter(gfx, program, _name, _v, _num);
    }
    void CheckResources() {
        for (auto &r : resources) {
            // sjit_assert(set_resources.find(r.first) != set_resources.end());
            if (set_resources.find(r.first) == set_resources.end()) {
                auto it = g_global_runtime_resource_registry.find(r.first);
                if (it != g_global_runtime_resource_registry.end()) {
                    SetResource(it->first.c_str(), it->second);
                } else {
                    SJIT_TRAP;
                }
            }
        }
    }
    void Begin() {
        ifor(3) jfor(2) {
            if (!timestamps[i][j]) timestamps[i][j] = gfxCreateTimestampQuery(gfx);
        }

        gfxCommandBeginEvent(gfx, name.c_str());
        gfxCommandBeginTimestampQuery(gfx, timestamps[timestamp_idx][0]);
    }
    void End() {
        gfxCommandEndTimestampQuery(gfx, timestamps[timestamp_idx][0]);
        gfxCommandEndEvent(gfx);

        duration = (f64)gfxTimestampQueryGetDuration(gfx, timestamps[timestamp_idx][0]);

        defer(timestamp_idx = (timestamp_idx + u32(1)) % u32(3));
    }
    void ResetTable() { set_resources.clear(); }
    void Destroy() {
        ifor(3) jfor(2) {
            if (timestamps[i][j]) gfxDestroyTimestampQuery(gfx, timestamps[i][j]);
        }
        if (kernel) gfxDestroyKernel(gfx, kernel);
        if (program) gfxDestroyProgram(gfx, program);
        *this = {};
    }
    bool IsValid() { return !!program && !!kernel; }
};

static HashMap<String, GPUKernel *> g_kernel_registry = {};
static HashMap<String, double>      g_pass_durations  = {};

// clang-format off
#define TRILINEAR_WEIGHTS(frac_rp) \
var trilinear_weights[2][2][2] = {                                                              \
    {                                                                                           \
                                                                                                \
        {                                                                                       \
            (f32(1.0) - frac_rp.x()) * (f32(1.0) - frac_rp.y()) * (f32(1.0) - frac_rp.z()),     \
            (frac_rp.x())            * (f32(1.0) - frac_rp.y()) * (f32(1.0) - frac_rp.z())      \
        },                                                                                      \
        {                                                                                       \
            (f32(1.0) - frac_rp.x()) * (frac_rp.y())             * (f32(1.0) - frac_rp.z()),    \
            (frac_rp.x())            * (frac_rp.y())             * (f32(1.0) - frac_rp.z())     \
        },                                                                                      \
    },                                                                                          \
    {                                                                                           \
                                                                                                \
        {                                                                                       \
            (f32(1.0) - frac_rp.x()) * (f32(1.0) - frac_rp.y()) * (frac_rp.z()),                \
            (frac_rp.x())            * (f32(1.0) - frac_rp.y()) * (frac_rp.z())                 \
        },                                                                                      \
        {                                                                                       \
            (f32(1.0) - frac_rp.x()) * (frac_rp.y())            * (frac_rp.z()),                \
            (frac_rp.x())            * (frac_rp.y())            * (frac_rp.z())                 \
        },                                                                                      \
    },                                                                                          \
};                                                                                                                                                               \
        // clang-format on

static GPUKernel CompileGlobalModule(GfxContext gfx, String _name) {
    using namespace SJIT;
    auto program = gfxCreateProgram(gfx, GfxProgramDesc::Compute(GetGlobalModule().Finalize()));
    if (!program) {
        fprintf(stdout, "%s", GetGlobalModule().Finalize());
        TRAP;
    }
    auto kernel = gfxCreateComputeKernel(gfx, program, "main");
    if (!kernel) {
        fprintf(stdout, "%s", GetGlobalModule().Finalize());
        TRAP;
    }

    GPUKernel k = {};
    k.name      = _name;
    k.gfx       = gfx;
    k.program   = program;
    k.kernel    = kernel;
    k.resources = GetGlobalModule().GetResources();
    k.isa       = gfxKernelGetIsa(gfx, k.kernel);
    if (k.isa.size() != size_t(0)) {
        char const *p = strstr(k.isa.c_str(), "vgpr_count(");
        p += strlen("vgpr_count(");
        if (p) {
            char buf[16] = {};
            int  l       = 0;
            while (true) {
                if (p[l] >= '0' && p[l] <= '9') {
                    buf[l] = p[l];
                    l++;
                } else {
                    break;
                }
            }
            sjit_assert(l);
            k.reg_pressure = std::atoi(buf);
        }
    }
    fprintf(stdout, "[REG PRESSURE] %s %i\n", _name.c_str(), k.reg_pressure);
    return k;
}

static void LaunchKernel(GfxContext gfx, u32x3 dispatch_size, std::function<void(void)> _func, bool _print = false) {
    HLSL_MODULE_SCOPE;
    _func();
    String str = String(GetGlobalModule().Finalize());
    if (g_kernel_registry.find(str) == g_kernel_registry.end()) {
        GPUKernel *n                  = new GPUKernel;
        *n                            = CompileGlobalModule(gfx, "anonymous");
        g_kernel_registry[str.Copy()] = n;
        if (_print) fprintf(stdout, "%s", str.c_str());
        if (_print && n->isa.c_str()) fprintf(stdout, "%s", n->isa.c_str());
    }
    GPUKernel *n = g_kernel_registry[str];
    n->CheckResources();
    gfxCommandBindKernel(gfx, n->kernel);
    gfxCommandDispatch(gfx, dispatch_size.x, dispatch_size.y, dispatch_size.z);

    n->ResetTable();
}

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
struct Octahedral {
    static var Sign(var v) { return MakeIfElse(v >= f32(0.0), f32(1.0), f32(-1.0)); }
    static var OctWrap(var v) {
        var tmp = make_f32x2(Sign(v.x()), Sign(v.y()));
        return (f32x2(1.0, 1.0) - abs(var(v.yx()))) * tmp;
    }
    static var Encode(var n) {
        n /= (abs(n.x()) + abs(n.y()) + abs(n.z()));
        n.xy() = MakeIfElse(n.z() >= f32(0.0), n.xy(), OctWrap(n.xy()));
        n.xy() = n.xy() * f32(0.5) + f32x2(0.5, 0.5);
        return n.xy();
    }
    static var Decode(var f) {
        f = f * f32(2.0) - f32x2(1.0, 1.0);

        // https://twitter.com/Stubbesaurus/status/937994790553227264
        var n = make_f32x3(f.x(), f.y(), f32(1.0) - abs(f.x()) - abs(f.y()));
        var t = saturate(-n.z());
        n.xy() += make_f32x2(Sign(n.x()), Sign(n.y())) * -t;
        return normalize(n);
    }
    static var EncodeNormalTo16Bits(var n) {
        var encoded = Encode(n);
        var ux      = (saturate(encoded.x()) * f32(255.0)).ToU32();
        var uy      = (saturate(encoded.y()) * f32(255.0)).ToU32();
        return ux | (uy << u32(8));
    }
    static var DecodeNormalFrom16Bits(var uxy) {
        var ux = uxy & u32(0xff);
        var uy = (uxy >> u32(8)) & u32(0xff);
        var x  = ux.ToF32() / f32(255.0);
        var y  = uy.ToF32() / f32(255.0);
        return Decode(make_f32x2(x, y));
    }
};

// https://google.github.io/filament/Filament.md.html#materialsystem/dielectricsandconductors
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// Don't mess up srgb roughness
struct GGXHelper {
    var NdotL;
    var NdotV;
    var LdotH;
    var VdotH;
    var NdotH;

    void Init(var L, var N, var V) {
        var H = normalize(L + V);
        LdotH = saturate(dot(L, H));
        VdotH = saturate(dot(V, H));
        NdotV = saturate(dot(N, V));
        NdotH = saturate(dot(N, H));
        NdotL = saturate(dot(N, L));
    }
    static var _GGX_G(var a2, var XdotY) {
        return //
            f32(2.0) * XdotY /
            // ------------------------ //
            (f32(1.0e-6) + XdotY + sqrt(a2 + (f32(1.0) - a2) * XdotY * XdotY)) //
            ;
    }
    var _GGX_GSchlick(var a, var XdotY) {
        var k = a / f32(2.0);

        return //
            XdotY /
            // ------------------------ //
            (XdotY * (f32(1.0) - k) + k) //
            ;
    }
    var DistributionGGX(var a2) {
        var NdotH2 = NdotH * NdotH;
        var denom  = (NdotH2 * (a2 - f32(1.0)) + f32(1.0));
        denom      = PI * denom * denom;
        return a2 / denom;
    }
    var ImportanceSampleGGX(var xi, var N, var roughness) {
        var a         = roughness * roughness;
        var phi       = f32(2.0) * PI * xi.x();
        var cos_theta = sqrt((f32(1.0) - xi.y()) / (f32(1.0) + (a * a - f32(1.0)) * xi.y()));
        var sin_theta = sqrt(f32(1.0) - cos_theta * cos_theta);
        var H         = Make(f32x3Ty);
        H.x()         = cos(phi) * sin_theta;
        H.y()         = sin(phi) * sin_theta;
        H.z()         = cos_theta;
        var TBN       = GetTBN(N);
        return normalize(TBN[u32(0)] * H.x() + TBN[u32(1)] * H.y() + TBN[u32(2)] * H.z());
    }
    static var G(var a, var NdotV, var NdotL) {
        // Smith
        // G(l,v,h)=G1(l)G1(v)
        var a2 = a * a;
        return _GGX_G(a2, NdotV) * _GGX_G(a2, NdotL);
    }
    var G(var r) { return G(r * r, NdotV, NdotL); }
    var D(var r) {
        // GGX (Trowbridge-Reitz)
        var a  = r * r;
        var a2 = a * a;
        var f  = NdotH * NdotH * (a2 - f32(1.0)) + f32(1.0);
        return a2 / (PI * f * f + f32(1.0e-6));
    }
    var fresnel(var f0 = f32x3_splat(0.04)) { return f0 + (f32x3_splat(1.0) - f0) * pow(saturate(f32(1.0) - VdotH), f32(5.0)); }
    var eval(var r) { return NdotL * G(r) * D(r); }

    // https://www.jcgt.org/published/0007/04/01/sampleGGXVNDF.h
    // Copyright (c) 2018 Eric Heitz (the Authors).
    //
    // Permission is hereby granted, free of charge, to any person obtaining a copy
    // of this software and associated documentation files (the "Software"), to deal
    // in the Software without restriction, including without limitation the rights
    // to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    // copies of the Software, and to permit persons to whom the Software is
    // furnished to do so, subject to the following conditions:
    //
    // The above copyright notice and this permission notice shall be included in
    // all copies or substantial portions of the Software.
    //
    // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    // IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    // FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    // AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    // LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    // OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    // THE SOFTWARE.
    static var SampleGGXVNDF(var Ve, var alpha_x, var alpha_y, var U1, var U2) {
        // Input Ve: view direction
        // Input alpha_x, alpha_y: roughness parameters
        // Input U1, U2: uniform random numbers
        // Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
        //
        //
        // Section 3.2: transforming the view direction to the hemisphere configuration
        var Vh = normalize(make_f32x3(alpha_x * Ve.x(), alpha_y * Ve.y(), Ve.z()));
        // Section 4.1: orthonormal basis (with special case if cross product is zero)
        var lensq = Vh.x() * Vh.x() + Vh.y() * Vh.y();
        var T1    = MakeIfElse(lensq > f32(0.0), make_f32x3(-Vh.y(), Vh.x(), 0) * rsqrt(lensq), var(f32x3(1.0, 0.0, 0.0)));
        var T2    = cross(Vh, T1);
        // Section 4.2: parameterization of the projected area
        var       r    = sqrt(U1);
        const f32 M_PI = f32(3.14159265358979);
        var       phi  = f32(2.0) * M_PI * U2;
        var       t1   = r * cos(phi);
        var       t2   = r * sin(phi);
        var       s    = f32(0.5) * (f32(1.0) + Vh.z());
        t2             = (f32(1.0) - s) * sqrt(f32(1.0) - t1 * t1) + s * t2;
        // Section 4.3: reprojection onto hemisphere
        var Nh = t1 * T1 + t2 * T2 + sqrt(max(f32(0.0), f32(1.0) - t1 * t1 - t2 * t2)) * Vh;
        // Section 3.4: transforming the normal back to the ellipsoid configuration
        var Ne = normalize(make_f32x3(alpha_x * Nh.x(), alpha_y * Nh.y(), max(f32(0.0), Nh.z())));
        return Ne;
    }
    static var SampleReflectionVector(var view_direction, var normal, var roughness, var xi) {
        var o = Make(f32x4Ty);
        EmitIfElse(
            roughness < f32(0.001), //
            [&] {
                o.xyz() = reflect(view_direction, normal);
                o.w()   = f32(1.0); // ? pdf of a nearly mirror like reflection
            },                      //
            [&] {
                var tbn_transform           = transpose(GetTBN(normal));
                var view_direction_tbn      = mul(-view_direction, tbn_transform);
                var a                       = roughness * roughness;
                var a2                      = a * a;
                var sampled_normal_tbn      = SampleGGXVNDF(view_direction_tbn, a, a, xi.x(), xi.y());
                var reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);

                var inv_tbn_transform = transpose(tbn_transform);
                o.xyz()               = mul(reflected_direction_tbn, inv_tbn_transform);

                // pdf
                var N        = normal;
                var V        = -view_direction;
                var H        = normalize(o.xyz() + V);
                var NdotH    = dot(H, N);
                var NdotH2   = NdotH * NdotH;
                var NdotV    = dot(H, V);
                var NdotL    = dot(N, o.xyz());
                var g        = G(a, NdotV, NdotL);
                var denom    = (NdotH2 * (a2 - f32(1.0)) + f32(1.0));
                denom        = PI * denom * denom;
                var D        = g * a2 / denom;
                var jacobian = f32(4.0) * dot(V, N);
                o.w()        = (D / jacobian); // pdf
            });
        return o;
    }
};

struct PingPong {
    u32 ping = u32(0);
    u32 pong = u32(0);

    void Next() {
        ping = u32(1) - ping;
        pong = u32(1) - ping;
    }
};

static BasicType GetBasicType(DXGI_FORMAT fmt) {
    switch (fmt) {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R32G32B32A32_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R32G32B32A32_SINT: return BASIC_TYPE_I32;
    case DXGI_FORMAT_R32G32B32_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R32G32B32_FLOAT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R32G32B32_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R32G32B32_SINT: return BASIC_TYPE_I32;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R16G16B16A16_UNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R16G16B16A16_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R16G16B16A16_SNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R16G16B16A16_SINT: return BASIC_TYPE_I32;
    case DXGI_FORMAT_R32G32_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R32G32_FLOAT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R32G32_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R32G32_SINT: return BASIC_TYPE_I32;
    case DXGI_FORMAT_R32G8X24_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: return BASIC_TYPE_F32;
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R10G10B10A2_UNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R10G10B10A2_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R11G11B10_FLOAT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R8G8B8A8_UNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R8G8B8A8_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R8G8B8A8_SNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R8G8B8A8_SINT: return BASIC_TYPE_I32;
    case DXGI_FORMAT_R16G16_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R16G16_FLOAT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R16G16_UNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R16G16_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R16G16_SNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R16G16_SINT: return BASIC_TYPE_I32;
    case DXGI_FORMAT_R32_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_D32_FLOAT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R32_FLOAT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R32_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R32_SINT: return BASIC_TYPE_I32;
    case DXGI_FORMAT_R24G8_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_D24_UNORM_S8_UINT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R8G8_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R8G8_UNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R8G8_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R8G8_SNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R8G8_SINT: return BASIC_TYPE_I32;
    case DXGI_FORMAT_R16_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R16_FLOAT: return BASIC_TYPE_F32;
    case DXGI_FORMAT_D16_UNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R16_UNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R16_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R16_SNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R16_SINT: return BASIC_TYPE_I32;
    case DXGI_FORMAT_R8_TYPELESS: return BASIC_TYPE_UNKNOWN;
    case DXGI_FORMAT_R8_UNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R8_UINT: return BASIC_TYPE_U32;
    case DXGI_FORMAT_R8_SNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R8_SINT: return BASIC_TYPE_I32;
    case DXGI_FORMAT_A8_UNORM: return BASIC_TYPE_F32;
    case DXGI_FORMAT_R1_UNORM: return BASIC_TYPE_F32;
    default: UNIMPLEMENTED;
    }
}
#    define GFX_JIT_MAKE_RESOURCE(name, type) var name = ResourceAccess(Resource::Create(type, #name))
#    define GFX_JIT_MAKE_GLOBAL_RESOURCE(name, type) static var name = ResourceAccess(Resource::Create(type, #name))
#    define GFX_JIT_MAKE_GLOBAL_RESOURCE_ARRAY(name, type) static var name = ResourceAccess(Resource::CreateArray(Resource::Create(type, "elem_" #name), #name))

static u32 LSB(u32 v) {
    static const int MultiplyDeBruijnBitPosition[32] = {0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};
    return u32(MultiplyDeBruijnBitPosition[((uint32_t)((v & -v) * 0x077CB531U)) >> 27]);
}

class Sun {
    f32                     width = f32(4.0);
    GfxContext              gfx;
    std::vector<GfxTexture> cascades;
    GfxBuffer               matrix_buffer;
    GfxProgram              shadow_program    = {};
    GfxKernel               shadow_kernels[4] = {};
    GfxDrawState            draw_states[4]    = {};
    u32                     frame_idx         = u32(0);
    u32                     cur_cascade_idx   = u32(0);
    u32                     num_cascades      = u32(4);

    f32x4x4 view[4] = {};
    f32x4x4 proj[4] = {};
    f32x3   pos     = {};
    f32x3   dir     = {};

public:
    void Init(GfxContext _gfx, char const *_shader_path) {
        gfx            = _gfx;
        shadow_program = gfxCreateProgram(gfx, "shadow", _shader_path);
        cascades.resize(num_cascades);
        ifor(num_cascades) {
            cascades[i] = gfxCreateTexture2D(gfx, u32(1 << 12), u32(1 << 12), DXGI_FORMAT_D32_FLOAT);
            gfxDrawStateSetDepthStencilTarget(draw_states[i], cascades[i]);
            gfxDrawStateSetDepthCmpOp(draw_states[i], D3D12_COMPARISON_FUNC_LESS);
            shadow_kernels[i] = gfxCreateGraphicsKernel(gfx, shadow_program, draw_states[i]);
        }
        matrix_buffer = gfxCreateBuffer<f32x4x4>(gfx, num_cascades);
    }
    std::vector<GfxTexture> GetTextures() { return cascades; }
    void                    Update(GfxUploadBuffer &upload_buffer) {
        frame_idx++;

        cur_cascade_idx = LSB(frame_idx & u32(0x7));

        sjit_assert(cur_cascade_idx < num_cascades);

        f32 theta = f32(3.141592 / 4.0);
        f32 phi   = f32(3.141592 / 4.0);

        dir   = {};
        dir.x = std::cos(theta) * std::cos(phi);
        dir.z = std::cos(theta) * std::sin(phi);
        dir.y = std::sin(theta);
        dir   = -dir;

        view[cur_cascade_idx] = {};
        proj[cur_cascade_idx] = {};

        f32 final_width             = width * std::pow(f32(2.0), float(cur_cascade_idx));
        f32 farz                    = final_width * f32(2.0);
        proj[cur_cascade_idx][0][0] = f32(1.0) / final_width;
        proj[cur_cascade_idx][1][1] = f32(1.0) / final_width;
        proj[cur_cascade_idx][2][2] = f32(-1.0) / farz;
        proj[cur_cascade_idx][3][3] = f32(1.0);
        view[cur_cascade_idx]       = glm::lookAt(pos - dir * f32(final_width), pos, f32x3(0.0, 1.0, 0.0));

        auto alloc = upload_buffer.Allocate(num_cascades * sizeof(f32x4x4));
        upload_buffer.DeferFree(alloc);
        ifor(num_cascades) {
            f32x4x4 m = transpose(transpose(view[i]) * transpose(proj[i]));
            memcpy(&((f32x4x4 *)alloc.host_dst)[i], &m, sizeof(f32x4x4));
        }
        gfxCommandCopyBuffer(gfx, matrix_buffer, u64(0), alloc.buffer, alloc.device_offset, num_cascades * sizeof(f32x4x4));
    }
    f32x4x4      GetViewProj() { return transpose(view[cur_cascade_idx]) * transpose(proj[cur_cascade_idx]); }
    f32x3        GetPos() { return pos; }
    f32x3        GetDir() { return dir; }
    f32          GetWidth() { return width; }
    GfxProgram   GetProgram() { return shadow_program; }
    GfxKernel    GetKernel() { return shadow_kernels[cur_cascade_idx]; }
    GfxDrawState GetDrawState() { return draw_states[cur_cascade_idx]; }
    GfxTexture   GetBuffer() { return cascades[cur_cascade_idx]; }
    GfxTexture   GetBuffer(u32 i) { return cascades[i]; }
    GfxBuffer    GetMatrixBuffer() { return matrix_buffer; }
    void         Release() {
        ifor(4) gfxDestroyTexture(gfx, cascades[i]);
        gfxDestroyBuffer(gfx, matrix_buffer);
    }
};

GFX_JIT_MAKE_GLOBAL_RESOURCE(g_MeshBuffer, Type::CreateStructuredBuffer(Mesh_Ty));
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_IndexBuffer, Type::CreateStructuredBuffer(u32Ty));
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_VertexBuffer, Type::CreateStructuredBuffer(Vertex_Ty));
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_InstanceBuffer, Type::CreateStructuredBuffer(Instance_Ty));
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_MaterialBuffer, Type::CreateStructuredBuffer(Material_Ty));
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_TransformBuffer, Type::CreateStructuredBuffer(f32x4x4Ty));
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_PreviousTransformBuffer, Type::CreateStructuredBuffer(f32x4x4Ty));
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_noise_texture, Texture2D_f32x2_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_color_buffer, RWTexture2D_f32x4_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE_ARRAY(g_Textures, Texture2D_f32x4_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_camera_pos, f32x3Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_camera_look, f32x3Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_camera_up, f32x3Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_camera_right, f32x3Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_camera_fov, f32Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_camera_aspect, f32Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE_ARRAY(g_sun_shadow_maps, Texture2D_f32_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_sun_shadow_matrices, Type::CreateStructuredBuffer(f32x4x4Ty));
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_sun_dir, f32x3Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_tlas, RaytracingAccelerationStructure_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_visibility_buffer, Texture2D_u32x4_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_velocity, Texture2D_f32x2_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_gbuffer_encoded, Texture2D_u32_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_gbuffer_roughness, Texture2D_f32_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_gbuffer_world_normals, Texture2D_f32x3_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_gbuffer_world_position, Texture2D_f32x3_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_prev_gbuffer_roughness, Texture2D_f32_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_prev_gbuffer_world_normals, Texture2D_f32x3_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_prev_gbuffer_world_position, Texture2D_f32x3_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_linear_sampler, SamplerState_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_nearest_sampler, SamplerState_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_frame_idx, u32Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_nearest_velocity, Texture2D_f32x2_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_disocclusion, Texture2D_f32_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_edges, Texture2D_f32_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_background, Texture2D_f32_Ty);
GFX_JIT_MAKE_GLOBAL_RESOURCE(g_ao, Texture2D_f32x4_Ty);

template <typename T>
using UniquePtr = std::unique_ptr<T>;

#    define GFX_JIT_MAKE_TEXTURE(_gfx, _name, _width_fn, _height_fn, _depth_fn, _mip_fn, _format_fn, _num_textures)                                                                \
        UniquePtr<GfxTextureResource>(GfxTextureResource::Create(                                                                                                                  \
            _gfx, _name, [=] { return (_width_fn); }, [=] { return (_height_fn); }, [=] { return (_depth_fn); }, [=] { return (_mip_fn); }, [=] { return (_format_fn); },          \
            _num_textures))

struct GfxTextureResource : public IGfxResourceRegistryItem {
    SharedPtr<Resource>          r_resource   = {};
    SharedPtr<Resource>          rw_resource  = {};
    String                       name         = {};
    std::function<u32()>         width_fn     = {};
    std::function<u32()>         height_fn    = {};
    std::function<u32()>         depth_fn     = {};
    std::function<u32()>         mip_fn       = {};
    std::function<DXGI_FORMAT()> format_fn    = {};
    u32                          num_textures = u32(1);
    GfxContext                   gfx          = {};
    std::vector<GfxTexture>      textures     = {};

    ~GfxTextureResource() override { ReleaseTextures(); }

    void Update() override {
        u32         _back_buffer_width  = gfxGetBackBufferWidth(gfx);
        u32         _back_buffer_height = gfxGetBackBufferHeight(gfx);
        u32         _width              = width_fn ? width_fn() : _back_buffer_width;
        u32         _height             = height_fn ? height_fn() : _back_buffer_height;
        u32         _depth              = depth_fn ? depth_fn() : u32(1);
        u32         _mip                = mip_fn ? mip_fn() : u32(1);
        DXGI_FORMAT _format             = format_fn ? format_fn() : DXGI_FORMAT_R16G16B16A16_FLOAT;

        sjit_assert(_width > u32(0));
        sjit_assert(_height > u32(0));
        sjit_assert(_depth > u32(0));
        sjit_assert(_mip > u32(0));
        sjit_assert(num_textures > u32(0));

        if (textures.size() != num_textures         //
            || (textures[0].getWidth() == _width)   //
            || (textures[0].getHeight() == _height) //
            || (textures[0].getDepth() == _depth)   //
            || (textures[0].getMipLevels() == _mip) //
            || (textures[0].getFormat() == _format) //
        ) {

            ReleaseTextures();
            ifor(num_textures) {
                if (_depth == u32(1))
                    textures.push_back(gfxCreateTexture2D(gfx, _width, _height, _format, _mip));
                else
                    textures.push_back(gfxCreateTexture3D(gfx, _width, _height, _depth, _format, _mip));
            }
            // GfxResourceRegistry::GetResources()[name] = ResourceSlot(&textures[0], u32(textures.size()));

            BasicType basic_type     = GetBasicType(_format);
            u32       num_components = GetNumComponents(_format);

            if (num_textures == u32(1)) {
                if (_depth == u32(1)) {
                    SharedPtr<Type> r_ty  = texture_2d_type_table[basic_type][num_components];
                    SharedPtr<Type> rw_ty = rw_texture_2d_type_table[basic_type][num_components];
                    sjit_assert(r_ty);
                    sjit_assert(rw_ty);
                    r_resource  = Resource::Create(r_ty, name);
                    rw_resource = Resource::Create(rw_ty, name);
                } else {
                    SharedPtr<Type> r_ty  = texture_3d_type_table[basic_type][num_components];
                    SharedPtr<Type> rw_ty = rw_texture_3d_type_table[basic_type][num_components];
                    sjit_assert(r_ty);
                    sjit_assert(rw_ty);
                    r_resource  = Resource::Create(r_ty, name);
                    rw_resource = Resource::Create(rw_ty, name);
                }
            } else {
                if (_depth == u32(1)) {
                    SharedPtr<Type> r_ty  = texture_2d_type_table[basic_type][num_components];
                    SharedPtr<Type> rw_ty = rw_texture_2d_type_table[basic_type][num_components];
                    sjit_assert(r_ty);
                    sjit_assert(rw_ty);
                    r_resource  = Resource::CreateArray(Resource::Create(r_ty, name), name);
                    rw_resource = Resource::CreateArray(Resource::Create(rw_ty, name), name);
                } else {
                    SharedPtr<Type> r_ty  = texture_3d_type_table[basic_type][num_components];
                    SharedPtr<Type> rw_ty = rw_texture_3d_type_table[basic_type][num_components];
                    sjit_assert(r_ty);
                    sjit_assert(rw_ty);
                    r_resource  = Resource::CreateArray(Resource::Create(r_ty, name), name);
                    rw_resource = Resource::CreateArray(Resource::Create(rw_ty, name), name);
                }
            }
        }
    }
    var Access(RWType _rw_type) override {
        if (_rw_type == RW_READ)
            return ResourceAccess(r_resource);
        else
            return ResourceAccess(rw_resource);
    }
    void ReleaseTextures() {
        GfxResourceRegistry::GetResources().erase(name);
        for (auto &t : textures) gfxDestroyTexture(gfx, t);
        textures.clear();
    }
    static GfxTextureResource *Create(GfxContext _gfx, String const &_name) {
        GfxTextureResource *o = new GfxTextureResource();
        o->gfx                = _gfx;
        o->name               = _name;
        GfxResourceRegistry::AddResource(o);
        return o;
    }
    static GfxTextureResource *Create(GfxContext                   _gfx,       //
                                      String                       _name,      //
                                      std::function<u32()>         _width_fn,  //
                                      std::function<u32()>         _height_fn, //
                                      std::function<u32()>         _depth_fn,  //
                                      std::function<u32()>         _mip_fn,    //
                                      std::function<DXGI_FORMAT()> _format_fn, //
                                      u32                          _num_textures) {
        GfxTextureResource *o = new GfxTextureResource();
        o->gfx                = _gfx;
        o->name               = _name;
        o->width_fn           = _width_fn;
        o->height_fn          = _height_fn;
        o->depth_fn           = _depth_fn;
        o->mip_fn             = _mip_fn;
        o->format_fn          = _format_fn;
        o->num_textures       = _num_textures;
        GfxResourceRegistry::AddResource(o);
        return o;
    }
};

static var GenCameraRay(var uv) {
    uv     = uv * f32x2(2.0, -2.0) - f32x2(1.0, -1.0);
    var r  = Zero(Ray_Ty);
    r["o"] = g_camera_pos;
    r["d"] = normalize(g_camera_look + tan(g_camera_fov * f32(0.5)) * (g_camera_right * uv.x() * g_camera_aspect + g_camera_up * uv.y()));
    return r;
}

class GBufferFromVisibility {
private:
    GfxContext gfx                       = {};
    GPUKernel  kernel                    = {};
    GfxTexture gbuffer_world_normals[2]  = {};
    GfxTexture gbuffer_world_position[2] = {};
    GfxTexture gbuffer_roughness[2]      = {};
    u32        width                     = u32(0);
    u32        height                    = u32(0);
    PingPong   ping_pong                 = {};
    f32        global_roughnes           = f32(0.0);

public:
    SJIT_DONT_MOVE(GBufferFromVisibility);

    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetRoughness() { return gbuffer_roughness[ping_pong.ping]; }
    GfxTexture &GetPrevRoughness() { return gbuffer_roughness[ping_pong.ping]; }
    GfxTexture &GetNormals() { return gbuffer_world_normals[ping_pong.ping]; }
    GfxTexture &GetWorldPosition() { return gbuffer_world_position[ping_pong.ping]; }
    GfxTexture &GetPrevNormals() { return gbuffer_world_normals[ping_pong.pong]; }
    GfxTexture &GetPrevWorldPosition() { return gbuffer_world_position[ping_pong.pong]; }
    void        SetGlobalRoughness(f32 _global_roughness) { global_roughnes = _global_roughness; }
    GBufferFromVisibility(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;
        ifor(2) {
            gbuffer_roughness[i]      = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8_UNORM);
            gbuffer_world_normals[i]  = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT);
            gbuffer_world_position[i] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT);
        }
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid                         = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var g_global_roughnes           = ResourceAccess(Resource::Create(f32Ty, "g_global_roughnes"));
            var g_rw_roughnes               = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_roughnes"));
            var g_rw_gbuffer_world_normals  = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_rw_gbuffer_world_normals"));
            var g_rw_gbuffer_world_position = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_rw_gbuffer_world_position"));
            var dim                         = g_rw_gbuffer_world_normals.GetDimensions().Swizzle("xy");

            EmitIfElse((tid < dim).All(), [&] {
                var visibility = g_visibility_buffer.Read(tid);

                EmitIfElse((visibility == u32x4_splat(0)).All(), [&] {
                    g_rw_gbuffer_world_normals.Store(tid, f32x4_splat(0.0));
                    g_rw_gbuffer_world_position.Store(tid, f32x4_splat(0.0));
                    EmitReturn();
                });

                var barys         = visibility.xy().AsF32();
                var instance_idx  = visibility.z();
                var primitive_idx = visibility.w();

                var instance  = g_InstanceBuffer.Load(instance_idx);
                var mesh      = g_MeshBuffer.Load(instance["mesh_id"]);
                var transform = g_TransformBuffer.Load(instance_idx);

                var i0  = g_IndexBuffer.Load(mesh["first_index"] + primitive_idx * u32(3) + u32(0)) + mesh["base_vertex"];
                var i1  = g_IndexBuffer.Load(mesh["first_index"] + primitive_idx * u32(3) + u32(1)) + mesh["base_vertex"];
                var i2  = g_IndexBuffer.Load(mesh["first_index"] + primitive_idx * u32(3) + u32(2)) + mesh["base_vertex"];
                var v0  = g_VertexBuffer.Load(i0);
                var v1  = g_VertexBuffer.Load(i1);
                var v2  = g_VertexBuffer.Load(i2);
                var wv0 = mul(transform, make_f32x4(v0["position"]["xyz"], f32(1.0)))["xyz"];
                var wv1 = mul(transform, make_f32x4(v1["position"]["xyz"], f32(1.0)))["xyz"];
                var wv2 = mul(transform, make_f32x4(v2["position"]["xyz"], f32(1.0)))["xyz"];
                var wn0 = normalize(mul(transform, make_f32x4(v0["normal"]["xyz"], f32(0.0)))["xyz"]);
                var wn1 = normalize(mul(transform, make_f32x4(v1["normal"]["xyz"], f32(0.0)))["xyz"]);
                var wn2 = normalize(mul(transform, make_f32x4(v2["normal"]["xyz"], f32(0.0)))["xyz"]);

                var w = Interpolate(wv0, wv1, wv2, barys);
                var n = normalize(Interpolate(wn0, wn1, wn2, barys));

                g_rw_gbuffer_world_normals.Write(tid, make_f32x4(n, f32(1.0)));
                g_rw_gbuffer_world_position.Write(tid, make_f32x4(w, f32(1.0)));
                g_rw_roughnes.Write(tid, g_global_roughnes);
            });

            // fprintf(stdout, GetGlobalModule().Finalize());

            kernel = CompileGlobalModule(gfx, "GBufferFromVisibility");
        }
    }
    void Execute() {
        ping_pong.Next();
        kernel.SetResource("g_rw_gbuffer_world_normals", gbuffer_world_normals[ping_pong.ping]);
        kernel.SetResource("g_rw_gbuffer_world_position", gbuffer_world_position[ping_pong.ping]);
        kernel.SetResource("g_rw_roughnes", gbuffer_roughness[ping_pong.ping]);
        kernel.SetResource("g_global_roughnes", global_roughnes);
        kernel.CheckResources();
        kernel.Begin();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.ResetTable();
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
    }
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
    }
    ~GBufferFromVisibility() {
        kernel.Destroy();
        ifor(2) {
            gfxDestroyTexture(gfx, gbuffer_world_normals[i]);
            gfxDestroyTexture(gfx, gbuffer_world_position[i]);
        }
    }
};
static var GetNoise(var tid) { return g_noise_texture.Load(tid & var(u32x2(127, 127))); }
static var EncodeGBuffer32Bits(var N, var P, var xi) {
    var on_16_bits = Octahedral::EncodeNormalTo16Bits(N);
    var dist       = length(P - g_camera_pos);
    var idist      = f32(1.0) / (f32(1.0) + dist);
    idist += (xi * f32(2.0) - f32(1.0)) * f32(1.0e-4);
    var idist_16_bits = idist.ToF16().f16_to_u32();
    var pack          = on_16_bits | (idist_16_bits << u32(16));
    return pack;
}
static var DecodeGBuffer32Bits(var camera_ray, var pack, var xi) {
    var on_16_bits    = pack & u32(0xffff);
    var idist_16_bist = (pack >> u32(16)) & u32(0xffff);
    var N             = Octahedral::DecodeNormalFrom16Bits(on_16_bits);
    var idist         = idist_16_bist.u32_to_f16().ToF32();
    idist += (xi * f32(2.0) - f32(1.0)) * f32(1.0e-4);
    var dist     = f32(1.0) / idist - f32(1.0);
    var P        = camera_ray["o"] + camera_ray["d"] * dist;
    var gbuffer  = Zero(GBuffer_Ty);
    gbuffer["P"] = P;
    gbuffer["N"] = N;
    return gbuffer;
}
class NearestVelocity {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    GfxTexture result = {};
    u32        width  = u32(0);
    u32        height = u32(0);

    var g_rw_result = ResourceAccess(Resource::Create(RWTexture2D_f32x2_Ty, "g_rw_result"));

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return result; }

    SJIT_DONT_MOVE(NearestVelocity);
    ~NearestVelocity() {
        kernel.Destroy();
        gfxDestroyTexture(gfx, result);
    }
    NearestVelocity(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;
        result      = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32G32_FLOAT);
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid = Input(IN_TYPE_DISPATCH_THREAD_ID).xy();

            var dim = g_rw_result.GetDimensions().xy();

            EmitIfElse((tid < dim).All(), [&] {
                var N                = g_gbuffer_world_normals.Load(tid);
                var P                = g_gbuffer_world_position.Load(tid);
                var nearest_velocity = Zero(f32x2Ty).Copy();
                var nearest_pos      = Zero(f32x3Ty).Copy();
                var nearest_normal   = Zero(f32x3Ty).Copy();
                var nearest_depth    = var(f32(1.0e6)).Copy();

                for (i32 y = i32(-1); y <= i32(1); y++) {
                    for (i32 x = i32(-1); x <= i32(1); x++) {
                        var coord = tid + u32x2(x, y);
                        var P     = g_gbuffer_world_position.Load(coord);
                        var depth = length(P - g_camera_pos);
                        EmitIfElse(depth < nearest_depth, [&] {
                            nearest_depth    = depth;
                            nearest_velocity = g_velocity[coord];
                        });
                    }
                }

                g_rw_result.Store(tid, nearest_velocity);
            });

            kernel = CompileGlobalModule(gfx, "NearestVelocity");
        }
    }
    void Execute() {
        kernel.SetResource(g_rw_result, result);
        kernel.CheckResources();
        kernel.Begin();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.ResetTable();
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
    }
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
    }
};
class EncodeGBuffer {
private:
    GfxContext gfx             = {};
    GPUKernel  kernel          = {};
    GfxTexture gbuffer_encoded = {};
    GfxTexture background_mask = {};
    u32        width           = u32(0);
    u32        height          = u32(0);

public:
    SJIT_DONT_MOVE(EncodeGBuffer);

    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return gbuffer_encoded; }
    GfxTexture &GetBackground() { return background_mask; }

    ~EncodeGBuffer() {
        kernel.Destroy();
        gfxDestroyTexture(gfx, background_mask);
        gfxDestroyTexture(gfx, gbuffer_encoded);
    }
    EncodeGBuffer(GfxContext _gfx) {
        u32 _width      = gfxGetBackBufferWidth(_gfx);
        u32 _height     = gfxGetBackBufferHeight(_gfx);
        gfx             = _gfx;
        width           = _width;
        height          = _height;
        gbuffer_encoded = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32_UINT);
        background_mask = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8_UNORM);
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid                      = Input(IN_TYPE_DISPATCH_THREAD_ID).xy();
            var g_gbuffer_world_normals  = ResourceAccess(Resource::Create(RWTexture2D_f32x3_Ty, "g_gbuffer_world_normals"));
            var g_gbuffer_world_position = ResourceAccess(Resource::Create(RWTexture2D_f32x3_Ty, "g_gbuffer_world_position"));
            var g_rw_background          = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_background"));
            var g_rw_result              = ResourceAccess(Resource::Create(RWTexture2D_u32_Ty, "g_rw_result"));
            var dim                      = g_rw_result.GetDimensions().xy();

            EmitIfElse((tid < dim).All(), [&] {
                var N = g_gbuffer_world_normals.Load(tid);
                var P = g_gbuffer_world_position.Load(tid);

                EmitIfElse((N == f32x3_splat(0.0)).All(), [&] {
                    g_rw_result.Store(tid, u32(0));
                    g_rw_background.Store(tid, f32(1.0));
                    EmitReturn();
                });

                var xi = GetNoise(tid);

                var pack = EncodeGBuffer32Bits(N, P, xi.x());

                g_rw_result.Store(tid, pack);
                g_rw_background.Store(tid, f32(0.0));
            });

            // fprintf(stdout, GetGlobalModule().Finalize());

            kernel = CompileGlobalModule(gfx, "EncodeGBuffer");
        }
    }
    void Execute() {
        kernel.SetResource("g_rw_result", gbuffer_encoded);
        kernel.SetResource("g_rw_background", background_mask);
        kernel.CheckResources();
        kernel.Begin();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.ResetTable();
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
    }
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
    }
};
static var GetEps(var P) { return f32(1.0e-2) / (f32(1.0) + length(g_camera_pos - P)); }
static var GetWeight(var N, var P, var rN, var rP, var eps, f32 npow = f32(4.0), f32 ppow = f32(8.0)) { //
    return pow(max(dot(N, rN), f32(0.0)), npow) * exp(-eps * pow(length(P - rP), ppow));
}
class Discclusion {
private:
    GfxContext gfx          = {};
    GPUKernel  kernel       = {};
    GfxTexture disocclusion = {};
    u32        width        = u32(0);
    u32        height       = u32(0);

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetDisocclusion() { return disocclusion; }

    SJIT_DONT_MOVE(Discclusion);
    ~Discclusion() {
        kernel.Destroy();
        gfxDestroyTexture(gfx, disocclusion);
    }
    Discclusion(GfxContext _gfx) {
        u32 _width   = gfxGetBackBufferWidth(_gfx);
        u32 _height  = gfxGetBackBufferHeight(_gfx);
        gfx          = _gfx;
        width        = _width;
        height       = _height;
        disocclusion = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8_UNORM);
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];

            var g_rw_disocclusion = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_disocclusion"));
            var dim               = g_rw_disocclusion.GetDimensions().Swizzle("xy");

            EmitIfElse((tid < dim).All(), [&] {
                var N = g_gbuffer_world_normals.Load(tid);
                var P = g_gbuffer_world_position.Load(tid);

                var uv       = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
                var velocity = g_velocity.Load(tid);

                var tracked_uv = uv - velocity;

                EmitIfElse((tracked_uv < f32x2(0.0, 0.0)).Any() || (tracked_uv > f32x2(1.0, 1.0)).Any(), [&] {
                    g_rw_disocclusion.Store(tid, f32(0.0));
                    EmitReturn();
                });

                var rN     = g_prev_gbuffer_world_normals.Sample(g_linear_sampler, tracked_uv);
                var rP     = g_prev_gbuffer_world_position.Sample(g_linear_sampler, tracked_uv);
                var d      = var(f32(1.0)).Copy();
                var eps    = GetEps(P);
                var weight = GetWeight(N, P, rN, rP, eps);

                EmitIfElse(weight < f32(0.9), [&] { d = f32(0.0); });

                g_rw_disocclusion.Store(tid, d);
            });

            // fprintf(stdout, GetGlobalModule().Finalize());

            kernel = CompileGlobalModule(gfx, "Discclusion");
        }
    }
    void Execute() {
        kernel.SetResource("g_rw_disocclusion", disocclusion);
        kernel.CheckResources();
        kernel.Begin();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.ResetTable();
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
    }
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
    }
};
static var GetSunShadow(var p, var n) {
    var mat = g_sun_shadow_matrices.Load(u32(0));
    var pp  = mul(mat, make_f32x4(p, f32(1.0)));
    pp /= pp.w();
    pp.xy() = pp.xy() * f32(0.5) + f32x2(0.5, 0.5);
    pp.y()  = f32(1.0) - pp.y();
    var l   = var(saturate(-dot(g_sun_dir, n))).Copy();
    EmitIfElse((pp.xy() < f32x2(1.0, 1.0)).All() && (pp.xy() > f32x2(0.0, 0.0)).All(), //
               [&] {
                   var blocker = g_sun_shadow_maps[u32(0)].Sample(g_linear_sampler, pp.xy());
                   EmitIfElse(blocker < pp.z() - f32(1.0e-3), [&] { l = f32(0.0); });
                   // l = t * f32(10.0);//f32(0.0);
               });
    return l;
};
static SharedPtr<Type> Hit_Ty = Type::Create("Hit", {
                                                        {"W", f32x3Ty}, //
                                                        {"N", f32x3Ty}, //
                                                    });
static var             GetHit(var ray_query) {
    var barys         = ray_query["bary"];
    var instance_idx  = ray_query["instance_id"];
    var primitive_idx = ray_query["primitive_idx"];

    var instance  = g_InstanceBuffer.Load(instance_idx);
    var mesh      = g_MeshBuffer.Load(instance["mesh_id"]);
    var transform = g_TransformBuffer.Load(instance_idx);

    var i0  = g_IndexBuffer.Load(mesh["first_index"] + primitive_idx * u32(3) + u32(0)) + mesh["base_vertex"];
    var i1  = g_IndexBuffer.Load(mesh["first_index"] + primitive_idx * u32(3) + u32(1)) + mesh["base_vertex"];
    var i2  = g_IndexBuffer.Load(mesh["first_index"] + primitive_idx * u32(3) + u32(2)) + mesh["base_vertex"];
    var v0  = g_VertexBuffer.Load(i0);
    var v1  = g_VertexBuffer.Load(i1);
    var v2  = g_VertexBuffer.Load(i2);
    var wv0 = mul(transform, make_f32x4(v0["position"]["xyz"], f32(1.0)))["xyz"];
    var wv1 = mul(transform, make_f32x4(v1["position"]["xyz"], f32(1.0)))["xyz"];
    var wv2 = mul(transform, make_f32x4(v2["position"]["xyz"], f32(1.0)))["xyz"];
    var wn0 = normalize(mul(transform, make_f32x4(v0["normal"]["xyz"], f32(0.0)))["xyz"]);
    var wn1 = normalize(mul(transform, make_f32x4(v1["normal"]["xyz"], f32(0.0)))["xyz"]);
    var wn2 = normalize(mul(transform, make_f32x4(v2["normal"]["xyz"], f32(0.0)))["xyz"]);

    var w    = Interpolate(wv0, wv1, wv2, barys);
    var n    = normalize(Interpolate(wn0, wn1, wn2, barys));
    var hit  = Zero(Hit_Ty);
    hit["W"] = w;
    hit["N"] = n;
    return hit;
}
static var TraceGGX(var N, var P, var roughness, var xi) {
    var V                 = normalize(P - g_camera_pos);
    var ray               = GGXHelper::SampleReflectionVector(V, N, roughness, xi);
    var ray_desc          = Zero(RayDesc_Ty);
    ray_desc["Direction"] = ray;
    ray_desc["Origin"]    = P + N * f32(1.0e-3);
    ray_desc["TMin"]      = f32(1.0e-3);
    ray_desc["TMax"]      = f32(1.0e6);
    var ray_query         = RayQuery(g_tlas, ray_desc);
    return ray_query;
}
class PrimaryRays {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    GfxTexture result = {};
    u32        width  = u32(0);
    u32        height = u32(0);

    var g_output = ResourceAccess(Resource::Create(RWTexture2D_f32x4_Ty, "g_output"));

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return result; }

    SJIT_DONT_MOVE(PrimaryRays);
    ~PrimaryRays() {
        kernel.Destroy();
        gfxDestroyTexture(gfx, result);
    }
    PrimaryRays(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;
        result      = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
        HLSL_MODULE_SCOPE;

        GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

        var dim = u32x2(width, height);

        var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];

        EmitIfElse((tid < dim).All(), [&] {
            var uv                = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var ray               = GenCameraRay(uv);
            var ray_desc          = Zero(RayDesc_Ty);
            ray_desc["Direction"] = ray["d"];
            ray_desc["Origin"]    = ray["o"];
            ray_desc["TMin"]      = f32(1.0e-3);
            ray_desc["TMax"]      = f32(1.0e6);
            var ray_query         = RayQuery(g_tlas, ray_desc);

            EmitIfElse(
                ray_query["hit"],
                [&] {
                    var hit = GetHit(ray_query);
                    var w   = hit["W"];
                    var n   = hit["N"];
                    var l   = GetSunShadow(w, n);
                    var c   = random_albedo(ray_query["instance_id"].ToF32());
                    g_output.Store(tid, make_f32x4(c * l, f32(1.0)));
                },
                [&] { g_output.Store(tid, f32x4_splat(0.0)); });
        });

        kernel = CompileGlobalModule(gfx, "PrimaryRays");

        // fprintf(stdout, kernel.isa.c_str());
    }
    void Execute() {
        kernel.SetResource(g_output->resource->GetName().c_str(), result);
        kernel.CheckResources();
        kernel.Begin();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.ResetTable();
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
    }
    template <typename T>
    void SetResource(char const *_name, T _v) {
        kernel.SetResource(_name, _v);
    }
    template <typename T>
    void SetResource(char const *_name, T _v, u32 _num) {
        kernel.SetResource(_name, _v, _num);
    }
};
// Src: Hacker's Delight, Henry S. Warren, 2001
static var RadicalInverse_VdC(var bits) {
    bits = (bits << u32(16)) | (bits >> u32(16));
    bits = ((bits & u32(0x55555555)) << u32(1)) | ((bits & u32(0xAAAAAAAA)) >> u32(1));
    bits = ((bits & u32(0x33333333)) << u32(2)) | ((bits & u32(0xCCCCCCCC)) >> u32(2));
    bits = ((bits & u32(0x0F0F0F0F)) << u32(4)) | ((bits & u32(0xF0F0F0F0)) >> u32(4));
    bits = ((bits & u32(0x00FF00FF)) << u32(8)) | ((bits & u32(0xFF00FF00)) >> u32(8));
    return bits.ToF32() * f32(2.3283064365386963e-10); // / 0x100000000
}
static var Hammersley(var i, var N) { return make_f32x2(i.ToF32() / N.ToF32(), RadicalInverse_VdC(i)); }
var        pcg(var v) {
    var state = v * u32(747796405) + u32(2891336453);
    var word  = ((state >> ((state >> u32(28)) + u32(4))) ^ state) * u32(277803737);
    return (word >> u32(22)) ^ word;
}
// xxhash (https://github.com/Cyan4973/xxHash)
//   From https://www.shadertoy.com/view/Xt3cDn
static var xxhash32(var p) {
    const u32 PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
    const u32 PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
    var       h32 = p + PRIME32_5;
    h32           = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
    h32           = PRIME32_2 * (h32 ^ (h32 >> 15));
    h32           = PRIME32_3 * (h32 ^ (h32 >> 13));
    return h32 ^ (h32 >> 16);
}

static u32                halton_sample_count = u32(15);
static std::vector<i32x2> halton_samples      = {i32x2(0, 1),   //
                                                 i32x2(-2, 1),  //
                                                 i32x2(2, -3),  //
                                                 i32x2(-3, 0),  //
                                                 i32x2(1, 2),   //
                                                 i32x2(-1, -2), //
                                                 i32x2(3, 0),   //
                                                 i32x2(-3, 3),  //
                                                 i32x2(0, -3),  //
                                                 i32x2(-1, -1), //
                                                 i32x2(2, 1),   //
                                                 i32x2(-2, -2), //
                                                 i32x2(1, 0),   //
                                                 i32x2(0, 2),   //
                                                 i32x2(3, -1)};
static void               Init_LDS_16x16(var &lds, std::function<var(var)> init_fn) {
    var  tid        = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
    var  gid        = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];
    auto linear_idx = [](var xy) { return (xy.x().ToI32() + xy.y().ToI32() * i32(16)).ToU32(); };
    var  group_tid  = u32(8) * (tid / u32(8));
    xfor(2) {
        yfor(2) {
            var dst_lds_cood = gid.xy().ToI32() * i32(2) + i32x2(x, y);
            var src_coord    = group_tid.ToI32() - i32x2(4, 4) + gid.xy().ToI32() * i32(2) + i32x2(x, y);
            var val          = init_fn(src_coord);
            lds.Store(linear_idx(dst_lds_cood.ToU32()), val);
        }
    }
}
static var Gaussian(var x) { return exp(-x * x * f32(0.5)); }
class EdgeDetect {
private:
    GfxContext gfx    = {};
    GPUKernel  kernel = {};
    GfxTexture result = {};

    u32 width  = u32(0);
    u32 height = u32(0);

    var g_rw_result = ResourceAccess(Resource::Create(RWTexture2D_f32_Ty, "g_rw_result"));

public:
    u32         GetWidth() { return width; }
    u32         GetHeight() { return height; }
    GfxTexture &GetResult() { return result; }

    SJIT_DONT_MOVE(EdgeDetect);
    ~EdgeDetect() {
        kernel.Destroy();
        gfxDestroyTexture(gfx, result);
    }
    EdgeDetect(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width;
        height      = _height;
        result      = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8_UNORM);
        {
            HLSL_MODULE_SCOPE;

            GetGlobalModule().SetGroupSize({u32(8), u32(8), u32(1)});

            var  tid        = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
            var  gid        = Input(IN_TYPE_GROUP_THREAD_ID)["xy"];
            var  dim        = u32x2(width, height);
            var  lds        = AllocateLDS(u32Ty, u32(16 * 16), "lds_values");
            var  gid_center = gid.xy() + u32x2(4, 4);
            auto linear_idx = [](var xy) { return (xy.x().ToI32() + xy.y().ToI32() * i32(16)).ToU32(); };
            var  group_tid  = u32(8) * (tid / u32(8));

            Init_LDS_16x16(lds, [&](var src_coord) {
                var val         = Zero(u32Ty).Copy();
                var gbuffer_val = g_gbuffer_encoded.Load(src_coord);
                val.x()         = gbuffer_val;
                return val;
            });
            EmitGroupSync();

            var uv             = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();
            var l              = lds.Load(linear_idx(gid_center));
            var ray            = GenCameraRay(uv);
            var xi             = GetNoise(tid);
            var center_gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());
            var is_bg          = g_background.Load(tid) > f32(0.5);
            EmitIfElse(
                is_bg, [&] { g_rw_result.Store(tid, f32(0.0)); },
                [&] {
                    var eps = GetEps(center_gbuffer["P"]);

                    var acc = Make(f32Ty);

                    for (i32 y = i32(-1); y <= i32(1); y++) {
                        for (i32 x = i32(-1); x <= i32(1); x++) {
                            if (x == i32(0) && y == i32(0)) continue;
                            i32x2 soffset = i32x2(x, y);
                            var   l       = lds.Load(linear_idx(gid_center.ToI32() + soffset));
                            var   uv      = (tid.ToF32() + f32x2(soffset) + f32x2(0.5, 0.5)) / dim.ToF32();
                            var   ray     = GenCameraRay(uv);
                            var   xi      = GetNoise(tid);
                            var   gbuffer = DecodeGBuffer32Bits(ray, l.x(), xi.x());
                            var   weight  = GetWeight(center_gbuffer["N"], center_gbuffer["P"], gbuffer["N"], gbuffer["P"], eps);
                            acc += weight;
                        }
                    }

                    acc = f32(1.0) - acc / f32(3 * 3 - 1);

                    g_rw_result.Store(tid, acc);
                });
            kernel = CompileGlobalModule(gfx, "EdgeDetect");
        }
    }
    void Execute() {
        kernel.SetResource(g_rw_result, result);
        kernel.CheckResources();
        kernel.Begin();
        {
            u32 const *num_threads  = gfxKernelGetNumThreads(gfx, kernel.kernel);
            u32        num_groups_x = (width + num_threads[0] - 1) / num_threads[0];
            u32        num_groups_y = (height + num_threads[1] - 1) / num_threads[1];

            gfxCommandBindKernel(gfx, kernel.kernel);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }
        kernel.End();
        g_pass_durations[kernel.name] = kernel.duration;
        kernel.ResetTable();
    }
};
class ISceneTemplate {
protected:
    Camera     g_camera         = {};
    GfxWindow  window           = {};
    GfxContext gfx              = {};
    GfxScene   scene            = {};
    GpuScene   gpu_scene        = {};
    GfxProgram pbr_program      = {};
    GfxKernel  pbr_kernel       = {};
    GfxProgram program_triangle = {};
    GfxKernel  kernel_triangle  = {};

    GfxTexture color_buffer              = {};
    GfxTexture visibility_buffer         = {};
    GfxTexture history_visibility_buffer = {};
    GfxTexture resolve_buffer            = {};
    GfxTexture velocity_buffer           = {};
    GfxTexture depth_buffer              = {};
    GfxTexture back_buffer               = {};

    GfxBuffer vertex_buffer = {};

    GfxSamplerState linear_sampler  = {};
    GfxSamplerState nearest_sampler = {};

    GfxDrawState reproject_draw_state = {};

    GfxDrawState pbr_draw_state = {};

    GfxGizmoManager gizmo_manager = {};

    GfxUploadBuffer   upload_buffer   = {};
    GfxDownloadBuffer download_buffer = {};

    BlueNoiseBaker blue_noise_baker = {};

    u32 width  = u32(0);
    u32 height = u32(0);

    Sun sun = {};

    u32 frame_idx = u32(0);

    bool wiggle_camera = false;
    bool render_imgui  = true;

    char const *shader_path = NULL;

    virtual void       InitChild()    = 0;
    virtual void       ReleaseChild() = 0;
    virtual void       ResizeChild()  = 0;
    virtual void       Render()       = 0;
    virtual GfxTexture GetResult()    = 0;

    f64 time = f64(0.0);

public:
    void Init(char const *_scene_path, char const *_shader_path, char const *_shader_include_path) {
        shader_path = _shader_path;
        {
            g_camera         = {};
            g_camera.pos     = f32x3(1.0, 1.0, 1.0) * f32(5.0);
            g_camera.look_at = f32x3(0.0, 0.0, 0.0);
            g_camera.UpdateMatrices();
        }
        window = gfxCreateWindow(1920, 1080, "gfx - PBR");
        gfx    = gfxCreateContext(window);
        gfxAddIncludePath(gfx, _shader_include_path);
        scene = gfxCreateScene();
        gfxImGuiInitialize(gfx);

        // Import the scene data
        gfxSceneImport(scene, _scene_path);
        gpu_scene = UploadSceneToGpuMemory(gfx, scene);

        sun.Init(gfx, _shader_path);

        // Create our PBR programs and kernels

        color_buffer              = gfxCreateTexture2D(gfx, DXGI_FORMAT_R16G16B16A16_FLOAT);
        visibility_buffer         = gfxCreateTexture2D(gfx, DXGI_FORMAT_R32G32B32A32_UINT);
        history_visibility_buffer = gfxCreateTexture2D(gfx, DXGI_FORMAT_R32G32B32A32_UINT);
        resolve_buffer            = gfxCreateTexture2D(gfx, DXGI_FORMAT_R32G32B32A32_UINT);
        velocity_buffer           = gfxCreateTexture2D(gfx, DXGI_FORMAT_R32G32_FLOAT);
        depth_buffer              = gfxCreateTexture2D(gfx, DXGI_FORMAT_D32_FLOAT);

        pbr_program = gfxCreateProgram(gfx, "pbr", _shader_path);
        gfxDrawStateSetColorTarget(pbr_draw_state, 0, visibility_buffer);
        gfxDrawStateSetColorTarget(pbr_draw_state, 1, velocity_buffer);
        gfxDrawStateSetDepthStencilTarget(pbr_draw_state, depth_buffer);
        gfxDrawStateSetDepthCmpOp(pbr_draw_state, D3D12_COMPARISON_FUNC_GREATER);
        // gfxDrawStateSetCullMode(pbr_draw_state, D3D12_CULL_MODE_FRONT);
        pbr_kernel = gfxCreateGraphicsKernel(gfx, pbr_program, pbr_draw_state);

        gfxProgramSetParameter(gfx, pbr_program, "g_LinearSampler", linear_sampler);

        gfxDrawStateSetColorTarget(reproject_draw_state, 0, resolve_buffer);

        // Create our sampler states
        linear_sampler  = gfxCreateSamplerState(gfx, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
        nearest_sampler = gfxCreateSamplerState(gfx, D3D12_FILTER_MIN_MAG_MIP_POINT);

        program_triangle = gfxCreateProgram(gfx, "triangle", _shader_path);
        assert(program_triangle);
        kernel_triangle  = gfxCreateGraphicsKernel(gfx, program_triangle);
        float vertices[] = {
            -1.0f, -1.0f, 0.0f, //
            3.0f,  -1.0f, 0.0f, //
            -1.0f, 3.0f,  0.0f,
        };
        vertex_buffer = gfxCreateBuffer<f32x3>(gfx, 3, vertices);
        // auto       debug_trace_primary = gfxCreateComputeKernel(gfx, debug_rt_program, "trace_primary");
        back_buffer = gfxCreateTexture2D(gfx, DXGI_FORMAT_R32G32B32A32_FLOAT);

        upload_buffer.Init(gfx);
        download_buffer.Init(gfx);

        blue_noise_baker.Init(gfx, _shader_path);

        InitChild();
    }
    uint64_t timeSinceEpochMillisec() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }
    void WindowLoop() {

        f64 cur_time       = f64(timeSinceEpochMillisec());
        f64 cur_delta_time = f64(0.0);
        while (!gfxWindowIsCloseRequested(window)) {
            frame_idx++;

            f64 this_time  = f64(timeSinceEpochMillisec());
            f64 delta_time = this_time - cur_time;
            cur_time       = this_time;

            time += f32(delta_time / f64(1000.0));

            cur_delta_time += f64(0.1) * (delta_time - cur_delta_time);

            gfxWindowPumpEvents(window);

            upload_buffer.FlushDeferredFreeQueue();
            download_buffer.FlushDeferredFreeQueue();

            sun.Update(upload_buffer);

            if (width != gfxGetBackBufferWidth(gfx) || height != gfxGetBackBufferHeight(gfx)) {
                gizmo_manager.Release(gfx);
                gizmo_manager.Init(gfx, gfxGetBackBufferWidth(gfx), gfxGetBackBufferHeight(gfx), depth_buffer, shader_path);
                width  = gfxGetBackBufferWidth(gfx);
                height = gfxGetBackBufferHeight(gfx);
                ResizeChild();
            }

            gizmo_manager.ClearLines();

            if (wiggle_camera) g_camera.phi += f32(std::sin(time * f64(3.0)) * f64(0.01));

            ImGuiContext &g          = *GImGui;
            bool          ui_hovered = g.HoveredWindow != NULL || g.MovingWindow != NULL || g.DragDropActive;
            if (!ui_hovered) g_camera.OnUI(f32(16.0 / 1000.0));

            g_camera.aspect = f32(gfxGetBackBufferWidth(gfx)) / f32(gfxGetBackBufferHeight(gfx));

            g_camera.UpdateMatrices();

            f32 gizmo_size = g_camera.distance / f32(8.0);
            gizmo_manager.AddLine(g_camera.look_at, g_camera.look_at + gizmo_size * f32x3(1.0, 0.0, 0.0), f32x3(1.0, 0.0, 0.0));
            gizmo_manager.AddLine(g_camera.look_at, g_camera.look_at + gizmo_size * f32x3(0.0, 1.0, 0.0), f32x3(0.0, 1.0, 0.0));
            gizmo_manager.AddLine(g_camera.look_at, g_camera.look_at + gizmo_size * f32x3(0.0, 0.0, 1.0), f32x3(0.0, 0.0, 1.0));

            gfxCommandClearTexture(gfx, gizmo_manager.color_target);

            // Update our GPU scene and camera
            UpdateGpuScene(gfx, scene, gpu_scene);

            blue_noise_baker.Bake();

            // Render primary
            {
                // Bind a bunch of shader parameters
                BindGpuScene(gfx, pbr_program, gpu_scene);
                gfxProgramSetParameter(gfx, pbr_program, "g_Eye", g_camera.pos);
                gfxProgramSetParameter(gfx, pbr_program, "g_ViewProjection", transpose(g_camera.view_proj));
                gfxProgramSetParameter(gfx, pbr_program, "g_PreviousViewProjection", transpose(g_camera.prev_view_proj));

                // Update texel size (can change if window is resized)
                // float const texel_size[] = {1.0f / gfxGetBackBufferWidth(gfx), 1.0f / gfxGetBackBufferHeight(gfx)};

                // Clear our render targets
                gfxCommandClearTexture(gfx, visibility_buffer);
                gfxCommandClearTexture(gfx, velocity_buffer);
                gfxCommandClearTexture(gfx, depth_buffer, /* clear_depth_value */ f32(0.0));

                // Draw all the meshes in the scene
                uint32_t const instance_count = gfxSceneGetInstanceCount(scene);

                gfxCommandBindKernel(gfx, pbr_kernel);
                gfxCommandBindIndexBuffer(gfx, gpu_scene.index_buffer);
                gfxCommandBindVertexBuffer(gfx, gpu_scene.vertex_buffer, /* index */ u32(0), /* byte_offset */ u64(0));
                gfxCommandBindVertexBuffer(gfx, gpu_scene.vertex_buffer, /* index */ u32(1), /* byte_offset */ u64(16));
                gfxCommandBindVertexBuffer(gfx, gpu_scene.vertex_buffer, /* index */ u32(2), /* byte_offset */ u64(32));

                for (uint32_t i = 0; i < instance_count; ++i) {
                    GfxConstRef<GfxInstance> const instance_ref = gfxSceneGetInstanceHandle(scene, i);

                    uint32_t const instance_id = (uint32_t)instance_ref;
                    uint32_t const mesh_id     = (uint32_t)instance_ref->mesh;

                    Mesh const mesh = gpu_scene.meshes[mesh_id];

                    gfxProgramSetParameter(gfx, pbr_program, "g_InstanceId", instance_id);

                    gfxCommandDrawIndexed(gfx, mesh.count, 1, mesh.first_index, mesh.base_vertex);
                }
            }

            // Render sun shadow
            {
                // Bind a bunch of shader parameters
                BindGpuScene(gfx, sun.GetProgram(), gpu_scene);
                f32x4x4 viewproj = sun.GetViewProj();

                gfxProgramSetParameter(gfx, sun.GetProgram(), "g_ViewProjection", transpose(viewproj));

                // Clear our render targets
                gfxCommandClearTexture(gfx, sun.GetBuffer(), /* clear_depth_value */ f32(1.0));

                // Draw all the meshes in the scene
                uint32_t const instance_count = gfxSceneGetInstanceCount(scene);

                gfxCommandBindKernel(gfx, sun.GetKernel());
                gfxCommandBindIndexBuffer(gfx, gpu_scene.index_buffer);
                gfxCommandBindVertexBuffer(gfx, gpu_scene.vertex_buffer, /* index */ u32(0), /* byte_offset */ u64(0));
                gfxCommandBindVertexBuffer(gfx, gpu_scene.vertex_buffer, /* index */ u32(1), /* byte_offset */ u64(16));
                gfxCommandBindVertexBuffer(gfx, gpu_scene.vertex_buffer, /* index */ u32(2), /* byte_offset */ u64(32));

                for (uint32_t i = 0; i < instance_count; ++i) {
                    GfxConstRef<GfxInstance> const instance_ref = gfxSceneGetInstanceHandle(scene, i);

                    uint32_t const instance_id = (uint32_t)instance_ref;
                    uint32_t const mesh_id     = (uint32_t)instance_ref->mesh;

                    Mesh const mesh = gpu_scene.meshes[mesh_id];

                    gfxProgramSetParameter(gfx, sun.GetProgram(), "g_InstanceId", instance_id);

                    gfxCommandDrawIndexed(gfx, mesh.count, 1, mesh.first_index, mesh.base_vertex);
                }
            }

            Render();

            gizmo_manager.Render(upload_buffer, g_camera.view_proj);

            // gfxProgramSetParameter(gfx, program, "g_input", color_buffer);
            gfxProgramSetParameter(gfx, program_triangle, "g_ui", gizmo_manager.color_target);
            // gfxProgramSetParameter(gfx, program, "g_input", disocclusion->GetDisocclusion());
            // gfxProgramSetParameter(gfx, program, "g_input", velocity_buffer);
            gfxProgramSetParameter(gfx, program_triangle, "g_input", GetResult());
            // gfxProgramSetParameter(gfx, program_triangle, "g_input", sun.GetBuffer(0));
            //   gfxProgramSetParameter(gfx, program, "g_input", spatial_filter->GetResult());
            //   gfxProgramSetParameter(gfx, program, "g_input", gbuffer_normal_splitter->GetResult());
            //          gfxProgramSetParameter(gfx, program, "g_input", gizmo_manager.font_texture);

            gfxCommandBindKernel(gfx, kernel_triangle);
            gfxCommandBindVertexBuffer(gfx, vertex_buffer);

            gfxCommandDraw(gfx, 3);

            if (ImGui::IsKeyPressed('R')) {
                wiggle_camera = !wiggle_camera;
            }

            // And submit the frame
            gfxImGuiRender();
            gfxFrame(gfx);
        }
    }
    void Release() {
        ReleaseChild();

        sun.Release();
        gfxDestroyTexture(gfx, visibility_buffer);
        gfxDestroyTexture(gfx, color_buffer);
        gfxDestroyTexture(gfx, depth_buffer);
        gfxDestroyTexture(gfx, history_visibility_buffer);
        gfxDestroyTexture(gfx, resolve_buffer);
        gfxDestroyTexture(gfx, velocity_buffer);

        gfxDestroySamplerState(gfx, linear_sampler);
        gfxDestroySamplerState(gfx, nearest_sampler);

        gfxDestroyKernel(gfx, pbr_kernel);
        gfxDestroyProgram(gfx, pbr_program);
        upload_buffer.Release(gfx);
        download_buffer.Release(gfx);
        blue_noise_baker.Release();
        gfxImGuiTerminate();
        gfxDestroyScene(scene);
        ReleaseGpuScene(gfx, gpu_scene);
        gfxDestroyContext(gfx);
        gfxDestroyWindow(window);
    }
};

} // namespace GfxJit

#endif // GFX_JIT_HPP