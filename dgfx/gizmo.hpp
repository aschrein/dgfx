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

#if !defined(GIZMO_HPP)
#    define GIZMO_HPP

#    include <unordered_map>

#    include "font.hpp"
#    include "utils.hpp"

// https://github.com/aschrein/VulkII/blob/master/include/scene.hpp

struct SimpleTriangleMesh {
    std::vector<f32x3> positions = {};
    std::vector<u32x3> indices   = {};
};
static SimpleTriangleMesh MakeCylinder(u32 degree, f32 radius, f32 length) {
    SimpleTriangleMesh out  = {};
    f32                step = PI * f32(2.0) / degree;
    out.positions.resize(degree * u32(2));
    ifor(degree) {
        f32 angle                 = step * (f32)i;
        out.positions[i]          = {radius * std::cos(angle), radius * std::sin(angle), f32(0.0)};
        out.positions[i + degree] = {radius * std::cos(angle), radius * std::sin(angle), length};
    }
    ifor(degree) {
        out.indices.push_back(u32x3{(u16)i, (u16)(i + degree), (u16)((i + 1) % degree)});
        out.indices.push_back(u32x3{(u16)((i + 1) % degree), (u16)(i + degree), (u16)(((i + 1) % degree) + degree)});
    }
    return out;
}
static SimpleTriangleMesh MakeUVSphere(u32 degree) { TRAP; }
static SimpleTriangleMesh MakePyramid(u32 degree) { TRAP; }
static SimpleTriangleMesh MakeIcosahedron(u32 degree) {
    SimpleTriangleMesh out = {};

    static f32 const X = f32(0.5257311);
    static f32 const Z = f32(0.8506508);

    static f32x3 const g_icosahedron_positions[12] = {{-X, f32(0.0), Z},  //
                                                      {X, f32(0.0), Z},   //
                                                      {-X, f32(0.0), -Z}, //
                                                      {X, f32(0.0), -Z},  //
                                                      {f32(0.0), Z, X},   //
                                                      {f32(0.0), Z, -X},  //
                                                      {f32(0.0), -Z, X},  //
                                                      {f32(0.0), -Z, -X}, //
                                                      {Z, X, f32(0.0)},   //
                                                      {-Z, X, f32(0.0)},  //
                                                      {Z, -X, f32(0.0)},  //
                                                      {-Z, -X, f32(0.0)}};

    static u32x3 const g_icosahedron_indices[20] = {{1, 4, 0},  //
                                                    {4, 9, 0},  //
                                                    {4, 5, 9},  //
                                                    {8, 5, 4},  //
                                                    {1, 8, 4},  //
                                                    {1, 10, 8}, //
                                                    {10, 3, 8}, //
                                                    {8, 3, 5},  //
                                                    {3, 2, 5},  //
                                                    {3, 7, 2},  //
                                                    {3, 10, 7}, //
                                                    {10, 6, 7}, //
                                                    {6, 11, 7}, //
                                                    {6, 0, 11}, //
                                                    {6, 1, 0},  //
                                                    {10, 1, 6}, //
                                                    {11, 0, 9}, //
                                                    {2, 11, 9}, //
                                                    {5, 2, 9},  //
                                                    {11, 2, 7}};
    for (auto p : g_icosahedron_positions) {
        out.positions.push_back(p);
    }
    for (auto i : g_icosahedron_indices) {
        out.indices.push_back(i);
    }

    auto DoSubdivide = [&](SimpleTriangleMesh const &in) -> SimpleTriangleMesh {
        SimpleTriangleMesh out = {};

        auto add_vertex = [&](f32x3 p) {
            f32 length = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
            out.positions.push_back(f32x3{p.x / length, p.y / length, p.z / length});
        };

        std::unordered_map<std::pair<u16, u16>, u16> cache         = {};
        auto                                         get_or_insert = [&](u16 i0, u16 i1) {
            std::pair<u16, u16> key{i0, i1};
            if (key.first > key.second) std::swap(key.first, key.second);
            if (!contains(cache, key)) {
                cache[key] = (u32)out.positions.size();

                auto v0_x = out.positions[i0].x;
                auto v1_x = out.positions[i1].x;
                auto v0_y = out.positions[i0].y;
                auto v1_y = out.positions[i1].y;
                auto v0_z = out.positions[i0].z;
                auto v1_z = out.positions[i1].z;

                auto mid_point = f32x3{(v0_x + v1_x) / f32(2.0), (v0_y + v1_y) / f32(2.0), (v0_z + v1_z) / f32(2.0)};

                add_vertex(mid_point);
            }
            return cache.find(key)->second;
        };
        out.positions = in.positions;
        for (auto &face : in.indices) {
            u32x3 mid = {};
            for (u32 edge = u32(0); edge < u32(3); ++edge) {
                mid[edge] = get_or_insert(face[edge], face[(edge + u32(1)) % u32(3)]);
            }
            out.indices.push_back(u32x3{face[0], mid[0], mid[2]});
            out.indices.push_back(u32x3{face[1], mid[1], mid[0]});
            out.indices.push_back(u32x3{face[2], mid[2], mid[1]});
            out.indices.push_back(u32x3{mid[0], mid[1], mid[2]});
        }
        return out;
    };

    ifor(degree) { out = DoSubdivide(out); }
    return out;
}
static SimpleTriangleMesh MakeStrip(u32 degree) { TRAP; }
static SimpleTriangleMesh MakeCone(u32 degree, f32 radius, f32 length) {
    SimpleTriangleMesh out = {};
    degree += u32(4);
    f32 step = PI * f32(2.0) / degree;
    out.positions.resize(degree * 2 + 2);
    out.positions[0] = {f32(0.0), f32(0.0), f32(0.0)};
    out.positions[1] = {f32(0.0), f32(0.0), length};
    for (u32 i = 0; i < degree; i++) {
        f32 angle            = step * i;
        out.positions[i + 2] = {radius * std::cos(angle), radius * std::sin(angle), f32(0.0)};
    }
    for (u32 i = 0; i < degree; i++) {
        out.indices.push_back(u32x3{(u16)(i + 2), (u16)(2 + (i + 1) % degree), (u16)0});
        out.indices.push_back(u32x3{(u16)(i + 2), (u16)(2 + (i + 1) % degree), (u16)1});
    }
    return out;
}
enum class Gizmo_Index_Type { UNKNOWN, U32, U16 };
struct SimpleTriangleMesh_GfxWrapper {
    GfxBuffer vertex_buffer = {};
    GfxBuffer index_buffer  = {};
    u32       num_indices   = u32(0);
    u32       num_vertices  = u32(0);

    void Release(GfxContext gfx) {
        if (vertex_buffer) gfxDestroyBuffer(gfx, vertex_buffer);
        if (index_buffer) gfxDestroyBuffer(gfx, index_buffer);
    }
    void Init(GfxContext   gfx,         //
              f32x3 const *positions,   //
              void const  *indices,     //
              u32          num_indices, //
              u32          num_vertices) {
        this->num_indices  = num_indices;
        this->num_vertices = num_vertices;
        vertex_buffer      = gfxCreateBuffer<f32x3>(gfx, num_vertices, positions);
        index_buffer       = gfxCreateBuffer<u32>(gfx, num_indices, indices);
    }
    void Init(GfxContext                gfx, //
              SimpleTriangleMesh const &model) {
        num_indices   = (u32)model.indices.size() * 3;
        num_vertices  = (u32)model.positions.size();
        vertex_buffer = gfxCreateBuffer<f32x3>(gfx, u32(model.positions.size()), &model.positions[0]);
        index_buffer  = gfxCreateBuffer<u32>(gfx, u32(3) * u32(model.indices.size()), &model.indices[0]);
    }
};
// https://github.com/aschrein/VulkII/blob/master/include/rendering_utils.hpp#L1335
class GfxGizmoManager {
public:
    struct SimpleVertex {
        f32x4 position;
    };
    struct LineVertex {
        f32x3 position;
        f32x3 color;
    };

    struct StringRef {
        u32   cursor = u32(0);
        u32   len    = u32(0);
        f32   x      = f32(0.0);
        f32   y      = f32(0.0);
        f32   z      = f32(0.0);
        f32x3 color  = {};
    };

    struct InstanceInfo {
        f32x4x4 transform;
        f32x4   color;
    };
    struct GlyphInstance {
        f32 x, y, z;
        f32 u, v;
        f32 r, g, b;
    };

public:
    std::vector<InstanceInfo> cylinder_draw_cmds = {};
    std::vector<InstanceInfo> sphere_draw_cmds   = {};
    std::vector<InstanceInfo> cone_draw_cmds     = {};
    bool                      lines_locked       = false;
    std::vector<LineVertex>   line_segments      = {};
    std::vector<char>         char_storage       = {};
    std::vector<StringRef>    strings            = {};

    SimpleTriangleMesh_GfxWrapper icosahedron_wrapper    = {};
    SimpleTriangleMesh_GfxWrapper icosahedron_wrapper_x2 = {};
    SimpleTriangleMesh_GfxWrapper cylinder_wrapper       = {};
    SimpleTriangleMesh_GfxWrapper cone_wrapper           = {};
    SimpleTriangleMesh_GfxWrapper glyph_wrapper          = {};

    // Curve LODs
    // SimpleTriangleMesh_GfxWrapper curvex4_wrapper   = {};
    // SimpleTriangleMesh_GfxWrapper curvex8_wrapper   = {};
    // SimpleTriangleMesh_GfxWrapper curvex16_wrapper  = {};
    // SimpleTriangleMesh_GfxWrapper curvex32_wrapper  = {};
    // SimpleTriangleMesh_GfxWrapper curvex64_wrapper  = {};
    // SimpleTriangleMesh_GfxWrapper curvex128_wrapper = {};

    GfxDrawState    draw_state      = {};
    GfxDrawState    line_draw_state = {};
    u32             render_width    = u32(0);
    u32             render_height   = u32(0);
    GfxTexture      color_target    = {};
    GfxTexture      depth_target    = {};
    GfxContext      gfx             = {};
    GfxProgram      simple_program  = {};
    GfxKernel       simple_kernel   = {};
    GfxProgram      lines_program   = {};
    GfxKernel       lines_kernel    = {};
    GfxProgram      glyph_program   = {};
    GfxKernel       glyph_kernel    = {};
    GfxSamplerState font_sampler    = {};
    GfxTexture      font_texture    = {};

public:
    void Release(GfxContext _gfx) {
        icosahedron_wrapper.Release(_gfx);
        icosahedron_wrapper_x2.Release(_gfx);
        cylinder_wrapper.Release(_gfx);
        cone_wrapper.Release(_gfx);
        glyph_wrapper.Release(_gfx);

        if (simple_program) gfxDestroyProgram(_gfx, simple_program);
        if (simple_kernel) gfxDestroyKernel(_gfx, simple_kernel);
        if (lines_program) gfxDestroyProgram(_gfx, lines_program);
        if (lines_kernel) gfxDestroyKernel(_gfx, lines_kernel);
        if (glyph_program) gfxDestroyProgram(_gfx, glyph_program);
        if (glyph_kernel) gfxDestroyKernel(_gfx, glyph_kernel);
        if (font_sampler) gfxDestroySamplerState(_gfx, font_sampler);
        if (font_texture) gfxDestroyTexture(_gfx, font_texture);
        if (color_target) gfxDestroyTexture(_gfx, color_target);

        *this = {};
    }
    bool NeedsResize(u32 _width, u32 _height) { return render_width != _width || render_height != _height; }
    void Init(GfxContext gfx, u32 _width, u32 _height, GfxTexture _depth_target, char const *_shader_path) {
        this->gfx           = gfx;
        this->depth_target  = _depth_target;
        this->render_width  = _width;
        this->render_height = _height;

        color_target = gfxCreateTexture2D(gfx, render_width, render_height, DXGI_FORMAT_R16G16B16A16_FLOAT);

        // Those are the builtin primitives
        cone_wrapper.Init(gfx, MakeCone(u32(8), f32(1.0), f32(1.0)));
        icosahedron_wrapper.Init(gfx, MakeIcosahedron(u32(2)));
        icosahedron_wrapper_x2.Init(gfx, MakeIcosahedron(u32(4)));
        cylinder_wrapper.Init(gfx, MakeCylinder(u32(8), f32(1.0), f32(1.0)));

        { // Simple quad fro glyph rendering
            f32 pos[] = {
                f32(0.0), f32(0.0), f32(0.0), //
                f32(1.0), f32(0.0), f32(0.0), //
                f32(1.0), f32(1.0), f32(0.0), //
                f32(0.0), f32(0.0), f32(0.0), //
                f32(1.0), f32(1.0), f32(0.0), //
                f32(0.0), f32(1.0), f32(0.0), //
            };
            u32 indices[] = {u32(0), //
                             u32(1), //
                             u32(2), //
                             u32(3), //
                             u32(4), //
                             u32(5), //
                             u32(6)};
            glyph_wrapper.Init(gfx, (f32x3 *)pos, indices, u32(6), u32(6));
        }

        gfxDrawStateSetColorTarget(draw_state, 0, color_target);
        gfxDrawStateSetDepthStencilTarget(draw_state, depth_target);
        gfxDrawStateSetDepthCmpOp(draw_state, D3D12_COMPARISON_FUNC_GREATER);

        gfxDrawStateSetColorTarget(line_draw_state, 0, color_target);
        gfxDrawStateSetDepthStencilTarget(line_draw_state, depth_target);
        gfxDrawStateSetDepthCmpOp(line_draw_state, D3D12_COMPARISON_FUNC_GREATER);
        gfxDrawStateSetTopology(line_draw_state, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

        // gfxDrawStateSetCullMode(pbr_draw_state, D3D12_CULL_MODE_FRONT);

        char gizmo_shader_path[0x100];
        sprintf_s(gizmo_shader_path, "%s/gizmo/", _shader_path);

        simple_program = gfxCreateProgram(gfx, "simple", gizmo_shader_path);
        lines_program  = gfxCreateProgram(gfx, "line", gizmo_shader_path);
        glyph_program  = gfxCreateProgram(gfx, "glyph", gizmo_shader_path);

        simple_kernel = gfxCreateGraphicsKernel(gfx, simple_program, draw_state);
        lines_kernel  = gfxCreateGraphicsKernel(gfx, lines_program, line_draw_state);
        glyph_kernel  = gfxCreateGraphicsKernel(gfx, glyph_program, draw_state);

        {
            std::vector<u8> data;
            data.resize(simplefont_bitmap_width * simplefont_bitmap_height);
            u8 *ptr = (u8 *)&data[0];
            ifor(simplefont_bitmap_height) {
                jfor(simplefont_bitmap_width) { ptr[simplefont_bitmap_width * i + j] = simplefont_bitmap[i][j] == ' ' ? 0 : 0xffu; }
            }
            GfxBuffer upload_texture_buffer = gfxCreateBuffer(gfx, data.size(), data.data(), kGfxCpuAccess_Write);
            font_texture                    = gfxCreateTexture2D(gfx, simplefont_bitmap_width, simplefont_bitmap_height, DXGI_FORMAT_R8_UNORM);
            gfxCommandCopyBufferToTexture(gfx, font_texture, upload_texture_buffer);
            gfxDestroyBuffer(gfx, upload_texture_buffer);
        }

        font_sampler = gfxCreateSamplerState(gfx, D3D12_FILTER_MIN_MAG_MIP_POINT);
    }
    void AddCylinder(f32x3 start, f32x3 end, f32 radius, f32x3 color) {
        f32x3 dr      = end - start;
        f32   length  = glm::length(dr);
        f32x3 dir     = glm::normalize(dr);
        f32x3 tangent = glm::cross(dir, f32x3{f32(0.0), f32(1.0), f32(0.0)});
        if (dot2(tangent) < f32(1.0e-3)) tangent = glm::cross(dir, f32x3{f32(0.0), f32(0.0), f32(1.0)});
        tangent          = glm::normalize(tangent);
        f32x3   binormal = -glm::cross(dir, tangent);
        f32x4x4 tranform =                                    //
            f32x4x4(                                          //
                tangent.x, tangent.y, tangent.z, f32(0.0),    //
                binormal.x, binormal.y, binormal.z, f32(0.0), //
                dir.x, dir.y, dir.z, f32(0.0),                //
                start.x, start.y, start.z, f32(1.0));         //
        InstanceInfo cmd = {};
        cmd.color.xyz    = color;
        cmd.transform    = tranform * glm::scale(f32x4x4(f32(1.0)), f32x3(radius, radius, length));
        cylinder_draw_cmds.push_back(cmd);
    }
    void AddString(f32x3 position, f32x3 color, char const *fmt, ...) {
        char    buf[0x100];
        va_list args;
        va_start(args, fmt);
        i32 len = vsprintf_s(buf, fmt, args);
        (void)len;
        va_end(args);
        AddString(buf, position, color);
    }
    void AddString(std::string str, f32x3 position, f32x3 color) {
        if (str.size() == 0) return;
        u32 cursor = (u32)char_storage.size();
        char_storage.resize(cursor + str.size());
        char *dst = &char_storage[cursor];
        memcpy(dst, str.c_str(), str.size());
        StringRef internal_string = {};
        internal_string.color     = color;
        internal_string.cursor    = cursor;
        internal_string.len       = (u32)str.size();
        internal_string.x         = position.x;
        internal_string.y         = position.y;
        internal_string.z         = position.z;
        strings.push_back(internal_string);
    }
    void AddCircle(f32x3 const &o, f32x3 camera_up, f32x3 const &camera_right, f32 radius, f32x3 color, int N = 16) {
        f32x3 last_pos  = o + camera_right * radius;
        f32   delta_phi = f32(2.0) * PI / N;
        for (int i = 1; i <= N; i++) {
            f32   s       = sinf(delta_phi * i);
            f32   c       = cosf(delta_phi * i);
            f32x3 new_pos = o + (s * camera_up + c * camera_right) * radius;
            AddLine(last_pos, new_pos, color);
            last_pos = new_pos;
        }
    }
    void AddSphere(f32x3 const &start, f32 radius, f32x3 const &color) {
        InstanceInfo cmd = {};
        cmd.color.xyz    = color;
        f32x4x4 tranform = f32x4x4(               //
            radius, f32(0.0), f32(0.0), f32(0.0), //
            f32(0.0), radius, f32(0.0), f32(0.0), //
            f32(0.0), f32(0.0), radius, 0.0,      //
            start.x, start.y, start.z, f32(1.0)); //
        cmd.transform    = tranform;
        sphere_draw_cmds.push_back(cmd);
    }
    void AddCone(f32x3 const &start, f32x3 const &dir, f32 radius, f32x3 const &color) {
        f32x3        normal   = normalize(dir);
        f32x3        up       = fabs(normal.z) > f32(0.99) ? f32x3(f32(0.0), f32(1.0), f32(0.0)) : f32x3(f32(0.0), f32(0.0), f32(1.0));
        f32x3        tangent  = normalize(cross(normal, up));
        f32x3        binormal = -cross(normal, tangent);
        f32x4x4      tranform = f32x4x4(                       //
            tangent.x, tangent.y, tangent.z, f32(0.0),    //
            binormal.x, binormal.y, binormal.z, f32(0.0), //
            dir.x, dir.y, dir.z, f32(0.0),                //
            start.x, start.y, start.z, f32(1.0));         //
        InstanceInfo cmd      = {};
        cmd.color.xyz         = color;
        cmd.transform         = tranform * glm::scale(f32x4x4(f32(1.0)), f32x3(radius, radius, f32(1.0)));
        cone_draw_cmds.push_back(cmd);
    }
    void AddLine(f32x3 p0, f32x3 p1, f32x3 color) {
        assert(!lines_locked);
        line_segments.push_back({p0, color});
        line_segments.push_back({p1, color});
    }
    void LockLines() { lines_locked = true; }
    void UnLockLines() { lines_locked = false; }
    void ClearLines() {
        assert(!lines_locked);
        line_segments.clear();
    }
    void ReserveLines(size_t cnt) {
        assert(!lines_locked);
        line_segments.reserve(cnt);
    }
    void AddLineAABB(f32x3 lo, f32x3 hi, f32x3 color) {
        f32 coordsx[2] = {
            lo.x,
            hi.x,
        };
        f32 coordsy[2] = {
            lo.y,
            hi.y,
        };
        f32 coordsz[2] = {
            lo.z,
            hi.z,
        };
        ifor(8) {
            u32 x = (i >> u32(0)) & u32(1);
            u32 y = (i >> u32(1)) & u32(1);
            u32 z = (i >> u32(2)) & u32(1);
            if (x == 0) {
                AddLine(f32x3(coordsx[0], coordsy[y], coordsz[z]), f32x3(coordsx[1], coordsy[y], coordsz[z]), color);
            }
            if (y == 0) {
                AddLine(f32x3(coordsx[x], coordsy[0], coordsz[z]), f32x3(coordsx[x], coordsy[1], coordsz[z]), color);
            }
            if (z == 0) {
                AddLine(f32x3(coordsx[x], coordsy[y], coordsz[0]), f32x3(coordsx[x], coordsy[y], coordsz[1]), color);
            }
        }
    }
    void Reset() {
        cylinder_draw_cmds.resize(0);
        cone_draw_cmds.resize(0);
        sphere_draw_cmds.resize(0);
        // line_segments.resize(0);
    }
    void Render(GfxUploadBuffer &upload_buffer, f32x4x4 const &viewproj) {

        defer(char_storage.resize(0));
        if (strings.size() == 0 && cylinder_draw_cmds.size() == 0 && sphere_draw_cmds.size() == 0 && cone_draw_cmds.size() == 0 && line_segments.size() == 0) return;

        if (line_segments.size() != 0) {

            GfxUploadBuffer::Allocation device_memory = upload_buffer.Allocate(line_segments.size() * sizeof(line_segments[0]));
            upload_buffer.DeferFree(device_memory);
            assert(device_memory.IsValid());
            device_memory.CopyIn(line_segments);

            gfxCommandBindKernel(gfx, lines_kernel);
            gfxCommandBindVertexBuffer(gfx, upload_buffer.GetBuffer(), /* index */ 0, /* offset */ device_memory.device_offset, /* stride */ u64(24));
            gfxCommandBindVertexBuffer(gfx, upload_buffer.GetBuffer(), /* index */ 1, /* offset */ device_memory.device_offset + u64(12), /* stride */ u64(24));
            gfxProgramSetParameter(gfx, lines_program, "g_viewproj", transpose(viewproj));
            gfxCommandDraw(gfx, /* vertex_count */ u32(line_segments.size()), /* instance_count */ u32(1), /* base_vertex */ u32(0), /* base_instance */ u32(0));
        }
    }
};

#endif // GIZMO_HPP