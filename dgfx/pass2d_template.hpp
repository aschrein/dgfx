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

class Pass2DTemplate {
protected:
    GfxContext gfx       = {};
    GPUKernel  kernel    = {};
    u32        width     = u32(0);
    u32        height    = u32(0);
    String     pass_name = ##Pass2DTemplate_NAME;

#if defined(Pass2DTemplate_BODY)
    Pass2DTemplate_BODY
#endif // defined(Pass2DTemplate_BODY)

#define TEXTURE_LIST TEXTURE(Dummy, DXGI_FORMAT_R32_UINT, u32, width, height, 1, 1)

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) GfxTexture _name = {};
                                                                             TEXTURE_LIST
#undef TEXTURE

        public :
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    static var g_rw_##_name() {                                                                                                                                                    \
        if (_depth == u32(1))                                                                                                                                                      \
            return ResourceAccess(Resource::Create(RWTexture2D_##_ty##_Ty, "g_" #_name));                                                                                          \
        else                                                                                                                                                                       \
            return ResourceAccess(Resource::Create(RWTexture3D_##_ty##_Ty, "g_" #_name));                                                                                          \
    }                                                                                                                                                                              \
    static var g_##_name() {                                                                                                                                                       \
        if (_depth == u32(1))                                                                                                                                                      \
            return ResourceAccess(Resource::Create(Texture2D_##_ty##_Ty, "g_" #_name));                                                                                            \
        else                                                                                                                                                                       \
            return ResourceAccess(Resource::Create(Texture3D_##_ty##_Ty, "g_" #_name));                                                                                            \
    }

        TEXTURE_LIST
#undef TEXTURE

            u32
            GetWidth() {
        return width;
    }
    u32 GetHeight() { return height; }

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    GfxTexture &Get##_name() { return _name; }
    TEXTURE_LIST
#undef TEXTURE

    SJIT_DONT_MOVE(PassTemplate);
    ~PassTemplate() {
        OnDestroy();
        kernel.Destroy();
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) gfxDestroyTexture(gfx, _name);
        TEXTURE_LIST
#undef TEXTURE
    }
    PassTemplate(GfxContext _gfx) {
        u32 _width  = gfxGetBackBufferWidth(_gfx);
        u32 _height = gfxGetBackBufferHeight(_gfx);
        gfx         = _gfx;
        width       = _width / 2;
        height      = _height / 2;

#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips)                                                                                                                  \
    {                                                                                                                                                                              \
        sjit_assert((_width) >= u32(1));                                                                                                                                           \
        sjit_assert((_height) >= u32(1));                                                                                                                                          \
        sjit_assert((_depth) >= u32(1));                                                                                                                                           \
        sjit_assert((_mips) >= u32(1));                                                                                                                                            \
        if (_depth == u32(1))                                                                                                                                                      \
            _name = gfxCreateTexture2D(gfx, (_width), (_height), (_fmt), (_mips));                                                                                                 \
        else                                                                                                                                                                       \
            _name = gfxCreateTexture3D(gfx, (_width), (_height), (_depth), (_fmt), (_mips));                                                                                       \
    }
        TEXTURE_LIST
#undef TEXTURE

        OnCreate();
    }
    void Execute() {
#define TEXTURE(_name, _fmt, _ty, _width, _height, _depth, _mips) kernel.SetResource(g_rw_##_name(), _name);
        TEXTURE_LIST
#undef TEXTURE

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
#undef TEXTURE_LIST
};