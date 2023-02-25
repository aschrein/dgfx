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

#if !defined(CAMERA_HPP)
#    define CAMERA_HPP

#    include "common.h"
#    include "glm/ext/matrix_transform.hpp"
#    include "imgui.h"

namespace {

static f32 CalculateHaltonNumber(uint32_t index, uint32_t base) {
    f32 f = f32(1.0), result = f32(0.0);
    for (u32 i = index; i > u32(0);) {
        f /= base;
        result = result + f * (i % base);
        i      = (u32)(i / (float)base);
    }
    return result;
}

} // namespace

static ImVec2 GetImGuiSize() {
    ImVec2 wsize = ImGui::GetWindowSize();
    // magic numbers
    f32 height_diff = f32(62.0);
    if (wsize.y < height_diff + i32(2)) {
        wsize.y = i32(2);
    } else {
        wsize.y = wsize.y - height_diff;
    }
    return wsize;
}

// https://github.com/aschrein/VulkII/blob/master/include/scene.hpp#L2101
struct Camera {
    static constexpr f32 PI       = f32(3.141592);
    f32                  phi      = PI / f32(2.0);
    f32                  theta    = PI / f32(2.0);
    f32                  distance = f32(3.0);
    f32x3                look_at  = f32x3(0.0, 0.0, 0.0);
    f32                  aspect   = f32(1.0);
    f32                  fov      = PI / f32(2.0);
    f32                  znear    = f32(1.0e-3);
    f32                  zfar     = f32(1.0e6);

    f32x3 pos = {};

    f32x4x4 view_to_proj_unjittered = {};

    f32x3 look      = {};
    f32x3 right     = {};
    f32x3 up        = {};
    i32x2 last_mpos = {};
    f32x2 mouse_uv  = {};

    f32x4x4 prev_proj          = {};
    f32x4x4 prev_inv_proj      = {};
    f32x4x4 prev_view          = {};
    f32x4x4 prev_inv_view      = {};
    f32x4x4 prev_view_proj     = {};
    f32x4x4 prev_inv_view_proj = {};

    f32x4x4 proj          = {};
    f32x4x4 inv_proj      = {};
    f32x4x4 view          = {};
    f32x4x4 inv_view      = {};
    f32x4x4 view_proj     = {};
    f32x4x4 inv_view_proj = {};

    void UpdateMatrices(f32x2 jitter = f32x2(0.0, 0.0)) {

        look  = normalize(look_at - pos);
        right = normalize(cross(look, f32x3(0.0, 1.0, 0.0)));
        up    = normalize(cross(right, look));

        // Swap the matrices from previous iteration
        prev_proj          = proj;
        prev_inv_proj      = inv_proj;
        prev_view          = view;
        prev_inv_view      = inv_view;
        prev_view_proj     = view_proj;
        prev_inv_view_proj = inv_view_proj;

        proj                    = f32x4x4(0.0);
        f32 halftanfov          = std::tan(fov * f32(0.5));
        proj[0][0]              = f32(1.0) / (aspect * halftanfov);
        proj[1][1]              = f32(1.0) / (halftanfov);
        proj[2][2]              = f32(0.0);
        proj[2][3]              = f32(-1.0);
        proj[3][2]              = znear;
        view_to_proj_unjittered = transpose(proj);

        proj[2][0] += jitter.x;
        proj[2][1] += jitter.y;
        proj = transpose(proj);
        view = transpose(lookAt(pos, look_at, f32x3(0.0, 1.0, 0.0)));

        inv_view      = inverse(view);
        inv_proj      = inverse(proj);
        view_proj     = view * proj;
        inv_view_proj = inverse(view_proj);
    }
    bool OnUI(f32 dt) {
        bool     dirty = false;
        ImGuiIO &io    = ImGui::GetIO();
        ImVec2   ires  = GetImGuiSize();
        f32x2    resolution;
        resolution = f32x2(ires.x, ires.y);

        f32 wheel = ImGui::GetIO().MouseWheel;

        if (wheel) {
            distance += distance * dt * f32(10.0) * wheel;
            distance = glm::clamp(distance, f32(1.0e-3), f32(1.0e3));
            dirty    = true;
        }
        f32 camera_speed = 2.0f * distance;
        if (ImGui::GetIO().KeysDown[ImGuiKey_LeftShift]) {
            camera_speed = 10.0f * distance;
            dirty        = true;
        }
        f32x3 camera_diff = f32x3(0.0, 0.0, 0.0);
        if (ImGui::IsKeyDown('W')) {
            camera_diff += look;
            dirty = true;
        }
        if (ImGui::IsKeyDown('E')) {
            camera_diff += f32x3(0.0, 1.0, 0.0);
            dirty = true;
        }
        if (ImGui::IsKeyDown('Q')) {
            camera_diff += f32x3(0.0, -1.0, 0.0);
            dirty = true;
        }
        if (ImGui::IsKeyDown('A')) {
            camera_diff -= right;
            dirty = true;
        }
        if (ImGui::IsKeyDown('S')) {
            camera_diff -= look;
            dirty = true;
        }

        if (ImGui::IsKeyDown('D')) {
            camera_diff += right;
            dirty = true;
        }
        if (dot(camera_diff, camera_diff) > f32(1.0e-6)) {
            look_at += glm::normalize(camera_diff) * camera_speed * dt;
        }
        ImVec2 impos = ImGui::GetMousePos();
        auto   wpos  = ImGui::GetCursorScreenPos();
        //auto   wsize = ImGui::GetWindowSize();
        impos.x -= wpos.x;
        impos.y -= wpos.y;
        i32x2 mpos = i32x2(impos.x, impos.y);
        f32x2 uv   = f32x2(mpos.x, mpos.y);
        uv /= resolution;
        uv       = f32(2.0) * uv - f32x2(1.0, 1.0);
        uv.y     = -uv.y;
        mouse_uv = uv;

        if (io.MouseDown[0] && (io.MouseDelta[0] != 0 || io.MouseDelta[1] != 0)) {
            i32 dx = mpos.x - last_mpos.x;
            i32 dy = mpos.y - last_mpos.y;

            phi += (float)dx * aspect * dt;
            theta -= (float)dy * dt;

            dirty = true;
        }
        last_mpos = mpos;

        pos = look_at + f32x3(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)) * distance;

        return dirty;
    }
    Ray GenRay(f32x2 uv) {
        uv    = uv * f32(2.0) - f32x2(1.0, 1.0);
        Ray r = {};
        r.o   = pos;
        r.d   = normalize(look + std::tan(fov * f32(0.5)) * (right * uv.x * aspect + up * uv.y));
        return r;
    }
};
#endif // CAMERA_HPP