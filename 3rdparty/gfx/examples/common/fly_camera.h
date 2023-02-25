/****************************************************************************
MIT License

Copyright (c) 2023 Guillaume Boissé

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
****************************************************************************/
#pragma once

#include "gfx_scene.h"
#include "gfx_window.h"

struct FlyCamera
{
    glm::vec3 eye;
    glm::vec3 center;
    glm::vec3 up;

    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 view_proj;

    glm::mat4 prev_view;
    glm::mat4 prev_proj;
    glm::mat4 prev_view_proj;
};

FlyCamera CreateFlyCamera(GfxContext gfx, glm::vec3 const &eye, glm::vec3 const &center);
void UpdateFlyCamera(GfxContext gfx, GfxWindow window, FlyCamera &fly_camera);
