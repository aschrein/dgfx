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

// clang-format off

#include "dgfx/jit.hpp"

#define GFX_IMPLEMENTATION_DEFINE

#include "gfx_imgui.h"
#include "gfx_scene.h"
#include "gfx_window.h"

#include "dgfx/common.h"
#include "dgfx/gfx_jit.hpp"

// clang-format on

int main(int argc, char **argv) {
    GfxWindow  window = gfxCreateWindow(1280, 720, "gfx - PBR");
    GfxContext gfx    = gfxCreateContext(window);
    gfxAddIncludePath(gfx, L"dgfx");

    BlueNoiseBaker blue_noise_baker = {};
    blue_noise_baker.Init(gfx);
    blue_noise_baker.Bake();

    {
        using namespace SJIT;
        using namespace GfxJit;
        using var = ValueExpr;

        u32 width  = u32(512);
        u32 height = u32(512);

        GFX_JIT_MAKE_GLOBAL_RESOURCE(g_cmd_list, Type::CreateStructuredBuffer(u32x4Ty));
        GFX_JIT_MAKE_GLOBAL_RESOURCE(g_output, RWTexture2D_f32x4_Ty);
        GfxTexture output = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT);
        defer(gfxDestroyTexture(gfx, output));
        g_global_runtime_resource_registry[g_output->GetResource()->GetName()] = output;

        enum {
            REG_UV_X = 30,         //
            REG_UV_Y = 31,         //
            REG_TIME = 29,         //
            REG_NIL  = 0xdeadbeef, //
        };

#define REG(x) u32(x)
#define IMMF32(x) u32(asu32(f32(x)))
#define IMMU32(x) u32(x)

        enum {
            CMD_UNKNOWON = 0,
            CMD_MOV,
            CMD_MOV_IMM,
            CMD_ADD,
            CMD_SUB,
            CMD_MUL,
            CMD_DIV,
            CMD_FRAC,
            CMD_SIN,
            CMD_COS,
            CMD_SQRT,
            CMD_SQR,
            CMD_RSQRT,
            CMD_POW,
            CMD_SET_OUTPUT,
            CMD_PCK,
            CMD_END,
        };

        struct InsrTy {
            u32 _type;
            u32 _dst;
            u32 _src0;
            u32 _src1;
        };

        Array<InsrTy> instructions = {
            {CMD_MOV_IMM, REG(20), IMMF32(0.5)},
            {CMD_MOV_IMM, REG(10), IMMF32(0.3333)},
            {CMD_MOV_IMM, REG(12), IMMF32(0.03)},
            {CMD_MOV_IMM, REG(11), IMMF32(1.0)},
            {CMD_MOV_IMM, REG(13), IMMF32(3.14159265358979323846264338327950288)},
            {CMD_MOV_IMM, REG(15), IMMF32(8.0)},

            {CMD_MUL, REG(16), REG_TIME, REG(20)},
            {CMD_SIN, REG(3), REG(16)},
            {CMD_COS, REG(4), REG(16)},

            {CMD_MUL, REG(5), REG_UV_X, REG(3)},
            {CMD_MUL, REG(6), REG_UV_Y, REG(4)},

            {CMD_ADD, REG(7), REG_UV_Y, REG_UV_X},

            {CMD_MUL, REG(5), REG(15), REG(5)},
            {CMD_MUL, REG(6), REG(15), REG(6)},
            {CMD_MUL, REG(7), REG(15), REG(7)},

            {CMD_SIN, REG(0), REG(5)},
            {CMD_COS, REG(1), REG(6)},
            {CMD_SIN, REG(2), REG(7)},

            {CMD_MUL, REG(3), REG(10), REG(0)},
            {CMD_MUL, REG(4), REG(10), REG(1)},
            {CMD_MUL, REG(5), REG(10), REG(2)},

            {CMD_MUL, REG(3), REG(20), REG(0)},
            {CMD_MUL, REG(4), REG(20), REG(1)},
            {CMD_MUL, REG(5), REG(20), REG(2)},

            {CMD_ADD, REG(3), REG(3), REG_UV_Y},
            {CMD_ADD, REG(4), REG(4), REG_UV_X},
            {CMD_ADD, REG(5), REG(5), REG_UV_Y},

            {CMD_ADD, REG(0), REG(0), REG(4)},
            {CMD_ADD, REG(1), REG(1), REG(5)},
            {CMD_ADD, REG(2), REG(2), REG(3)},

            {CMD_SIN, REG(0), REG(0)},
            {CMD_COS, REG(1), REG(1)},
            {CMD_SIN, REG(2), REG(2)},

            {CMD_PCK, REG(0), REG(0)},
            {CMD_PCK, REG(1), REG(1)},
            {CMD_PCK, REG(2), REG(2)},

            {CMD_MOV_IMM, REG(15), IMMF32(16.0)},

            {CMD_POW, REG(0), REG(0), REG(15)},
            {CMD_POW, REG(1), REG(1), REG(15)},
            {CMD_POW, REG(2), REG(2), REG(15)},

            {CMD_SET_OUTPUT, IMMU32(0), REG(0)},
            {CMD_SET_OUTPUT, IMMU32(1), REG(1)},
            {CMD_SET_OUTPUT, IMMU32(2), REG(2)},

            {CMD_END}, //
        };

        GfxBuffer cmd = gfxCreateBuffer<InsrTy>(gfx, instructions.size(), &instructions[0]);
        defer(gfxDestroyBuffer(gfx, cmd));
        g_global_runtime_resource_registry[g_cmd_list->GetResource()->GetName()] = cmd;

        LaunchKernel(
            gfx, {width / u32(8), height / u32(8), 1},
            [&] {
                u32 num_registers = u32(32);
                var registers     = EmitArray(f32Ty, u32(num_registers));
                var output_reg    = EmitArray(f32Ty, u32(4));

                output_reg[u32(0)] = f32(1.0);
                output_reg[u32(1)] = f32(1.0);
                output_reg[u32(2)] = f32(1.0);
                output_reg[u32(3)] = f32(1.0);

                var tid = Input(IN_TYPE_DISPATCH_THREAD_ID)["xy"];
                var dim = g_output.GetDimensions();
                var uv  = (tid.ToF32() + f32x2(0.5, 0.5)) / dim.ToF32();

                registers[REG_UV_X] = uv.x();
                registers[REG_UV_Y] = uv.y();
                registers[REG_TIME] = f32(6.0);

                var pc = var(u32(0)).Copy();
                EmitWhileLoop([&] {
                    var i = g_cmd_list.Load(pc);
                    pc += u32(1);
                    var src0 = registers[i.z() % num_registers];
                    EmitIfElse((i.x() == u32(CMD_UNKNOWON)) || (i.x() == u32(CMD_END)), [&] { EmitBreak(); });
                    EmitSwitchCase(i.x(), {
                                              //
                                              {CMD_UNKNOWON, [&] {}},
                                              {CMD_MOV, [&] { registers[i.y()] = src0; }},                                        //
                                              {CMD_MOV_IMM, [&] { registers[i.y()] = i.z().AsF32(); }},                           //
                                              {CMD_ADD, [&] { registers[i.y()] = src0 + registers[i.w() % num_registers]; }},     //
                                              {CMD_SUB, [&] { registers[i.y()] = src0 - registers[i.w() % num_registers]; }},     //
                                              {CMD_MUL, [&] { registers[i.y()] = src0 * registers[i.w() % num_registers]; }},     //
                                              {CMD_DIV, [&] { registers[i.y()] = src0 / registers[i.w() % num_registers]; }},     //
                                              {CMD_FRAC, [&] { registers[i.y()] = frac(src0); }},                                 //
                                              {CMD_SIN, [&] { registers[i.y()] = sin(src0); }},                                   //
                                              {CMD_COS, [&] { registers[i.y()] = cos(src0); }},                                   //
                                              {CMD_SQR, [&] { registers[i.y()] = pow(src0, f32(2.0)); }},                         //
                                              {CMD_POW, [&] { registers[i.y()] = pow(src0, registers[i.w() % num_registers]); }}, //
                                              {CMD_SQRT, [&] { registers[i.y()] = sqrt(src0); }},                                 //
                                              {CMD_RSQRT, [&] { registers[i.y()] = rsqrt(src0); }},                               //
                                              {CMD_PCK, [&] { registers[i.y()] = f32(0.5) * src0 + f32(0.5); }},                  //
                                              {CMD_SET_OUTPUT, [&] { output_reg[i.y()] = registers[i.z() % num_registers]; }},    //
                                              {CMD_END, [&] {}},                                                                  //

                                          });
                });

                g_output.Store(tid, make_f32x4(output_reg[u32(0)], output_reg[u32(1)], output_reg[u32(2)], output_reg[u32(3)]));
            },
            /*_print*/ true);

        write_texture_to_file(gfx, output, "build/test1.png");
    }

#if 0

    {
        using namespace SJIT;
        using var = ValueExpr;

#    if 0
                        {
                            char const* name = "MyFunc";
                            char        buf[0x100];
                            sprintf_s(buf, "Payload_%s", name);
                            std::shared_ptr<Type>        payload_ty = Type::Create(buf, { {"coord", u32x2Ty}, {"result", f32x4Ty} });
                            std::shared_ptr<FnPrototype> my_func_ty = FnPrototype::Create("MyFunc", u1Ty, { {"payload", payload_ty, FN_ARG_INOUT} });

                            hlsl_module.EnterFunction();
                            hlsl_module.EnterScope();

                            payload_ty->EmitHLSL(hlsl_module);

                            // hlsl_module.GetBody().EmitF("struct Payload_%s {\n", name);
                            // hlsl_module.GetBody().EmitF("u32x2 coord;\n");
                            // hlsl_module.GetBody().EmitF("f32x4 result;\n");
                            // hlsl_module.GetBody().EmitF("};\n");
                            auto& body = hlsl_module.GetBody();

#        if 0
                            hlsl_module.GetBody().EmitF("bool tmp_%s(inout Payload_%s __payload) {\n", name, name);
                            var coord = var::Input("coord", u32x2Ty);
                            var in = input.Read(coord);
                            in->EmitHLSL(hlsl_module);

                            hlsl_module.GetBody().EmitF("__payload.result = %s;\n", in->name);
                            hlsl_module.GetBody().EmitF("return true;\n");
                            hlsl_module.GetBody().EmitF("}\n");
#        endif // 0

                            my_func_ty->EmitDefinition(hlsl_module);
                            body.EmitF("\n");
                            body.EmitF("{\n");
                            body.EmitF("return true;\n");
                            body.EmitF("}\n");

                            hlsl_module.ExitScope();
                            hlsl_module.ExitFunction();

                            var payload = var::Zero(payload_ty);
                            payload->EmitHLSL(hlsl_module);
                        }
#    endif     // 0

   
        class GaussianBlur {
        private:
            GfxContext gfx     = {};
            GfxTexture texture = {};

        public:
            GaussianBlur(GfxContext _gfx, GfxTexture src_texture, i32x2 _dir, u32 radius = u32(16)) {
                gfx     = _gfx;
                texture = gfxCreateTexture2D(gfx, src_texture.getWidth(), src_texture.getHeight(), src_texture.getFormat());

                HLSL_MODULE_SCOPE;

                var tid            = var::Input(IN_TYPE_DISPATCH_THREAD_ID).Swizzle("xy");
                u32 num_components = GetNumComponents(src_texture.getFormat());
                var input          = var::Resource(Resource::Create(texture_2d_type_table[BASIC_TYPE_F32][num_components], "g_input"));
                var output         = var::Resource(Resource::Create(rw_texture_2d_type_table[BASIC_TYPE_F32][num_components], "g_output"));
                var val_sum        = var::Zero(input->resource->GetType()->GetTemplateType());
                var weight_sum     = var(f32(0.0));

                var dims = input.GetDimensions();

                var theta = f32(0.99);
                var dir   = var(_dir);

                EmitForLoop(/*begin*/ var(-i32(radius)), /*end*/ var(i32(radius)), [&](var iter) {
                    var weight = var::Pow(theta, (iter * iter).ToF32());
                    var dp     = var(dir * iter);
                    var offset = (tid.AsI32() + dp);
                    EmitIfElse((offset < dims.ToI32()).All() && (offset >= var(i32x2(0, 0))).All(), //
                               [&] {
                                   var in = input.Read(offset);
                                   if (num_components == u32(4)) {
                                       var total_weight = var(weight) * in.Swizzle("w");
                                       val_sum += in * total_weight;
                                       weight_sum += total_weight;
                                   } else {
                                       var total_weight = var(weight);
                                       val_sum += in * total_weight;
                                       weight_sum += total_weight;
                                   }
                               },     //
                               [&] {} //
                    );
                });
                var final_val = val_sum / weight_sum;
                output.Write(tid, final_val);

                auto program = gfxCreateProgram(gfx, GfxProgramDesc::Compute(GetGlobalModule().Finalize()));
                if (!program) {
                    fprintf(stdout, GetGlobalModule().Finalize());
                    TRAP;
                }
                auto kernel = gfxCreateComputeKernel(gfx, program, "main");
                if (!kernel) {
                    fprintf(stdout, GetGlobalModule().Finalize());
                    TRAP;
                }

                u32x3 group_size = GetGlobalModule().GetGroupSize();

                gfxProgramSetTexture(gfx, program, "g_output", texture);
                gfxProgramSetTexture(gfx, program, "g_input", src_texture);
                gfxCommandBindKernel(gfx, kernel);
                gfxCommandDispatch(gfx,                                                              //
                                   (src_texture.getWidth() + group_size.x - u32(1)) / group_size.x,  //
                                   (src_texture.getHeight() + group_size.y - u32(1)) / group_size.y, //
                                   u32(1));

                gfxDestroyKernel(gfx, kernel);
                gfxDestroyProgram(gfx, program);
            }
            GfxTexture GetTexture() { return texture; }
            ~GaussianBlur() { gfxDestroyTexture(gfx, texture); }
        };
        

        GaussianBlur blur_x(gfx, blue_noise_baker.GetTexture(), i32x2(1, 0));
        GaussianBlur blur_y(gfx, blur_x.GetTexture(), i32x2(0, 1));

        write_texture_to_file(gfx, blur_y.GetTexture(), "build/gaussian.png");

        {
            class PrefixSum {
                GfxContext gfx    = {};
                GfxBuffer  buffer = {};

            public:
                PrefixSum(GfxContext _gfx, GfxBuffer _src_buffer) {
                    gfx    = _gfx;
                    buffer = gfxCreateBuffer<u32>(gfx, _src_buffer.getSize() / u32(4));
                    gfxCommandScanSum(gfx, GfxDataType::kGfxDataType_Uint, buffer, _src_buffer);
                }
                GfxBuffer GetResult() { return buffer; }
                ~PrefixSum() { gfxDestroyBuffer(gfx, buffer); }
            };

            std::vector<u32> host_data = {};
            std::vector<u32> host_scan = {};
            u32              N         = u32(1 << 10);
            u32              sum       = u32(0);
            ifor(N) {
                u32 num = u32(rand()) % u32(256);
                host_data.push_back(num);
                host_scan.push_back(sum);
                sum += num;
            }

            GfxBuffer device_data = gfxCreateBuffer<u32>(gfx, N, &host_data[0]);
            defer(gfxDestroyBuffer(gfx, device_data));
            PrefixSum prefix_sum(gfx, device_data);
            GfxBuffer device_result = gfxCreateBuffer<u32>(gfx, sum);
            defer(gfxDestroyBuffer(gfx, device_result));

            {
                HLSL_MODULE_SCOPE;
                var tid       = var::Input(IN_TYPE_DISPATCH_THREAD_ID).Swizzle("x");
                var input     = var::Resource(Resource::Create(RWStructuredBuffer_u32_Ty, "g_input"));
                var output    = var::Resource(Resource::Create(RWStructuredBuffer_u32_Ty, "g_output"));
                var num_items = var::Resource(Resource::Create(u32Ty, "g_num_items"));
                var result    = EmitBinarySearch(input, num_items, tid);
                output.Write(tid, result);
                GetGlobalModule().SetGroupSize({u32(32), u32(1), u32(1)});
                fprintf(stdout, GetGlobalModule().Finalize());
                u32x3 group_size = GetGlobalModule().GetGroupSize();

                GPUKernel k = CompileGlobalModule(gfx);

                defer(k.Destroy(gfx));

                gfxProgramSetParameter(gfx, k.program, "g_num_items", N);
                gfxProgramSetParameter(gfx, k.program, "g_output", device_result);
                gfxProgramSetParameter(gfx, k.program, "g_input", prefix_sum.GetResult());
                gfxCommandBindKernel(gfx, k.kernel);
                gfxCommandDispatch(gfx,                                          //
                                   (sum + group_size.x - u32(1)) / group_size.x, //
                                   u32(1),                                       //
                                   u32(1));

                std::vector<u32> mapping = read_device_buffer<u32>(gfx, device_result);
                ifor(sum) {
                    assert(i >= host_scan[mapping[i]]);
                    if (mapping[i] + u32(1) < N) {
                        assert(i < host_scan[mapping[i] + u32(1)]);
                    }
                }
            }
        }
        {
            HLSL_MODULE_SCOPE;
            GetGlobalModule().SetGroupSize({u32(32), u32(1), u32(1)});

            var tid       = var::Input(IN_TYPE_DISPATCH_THREAD_ID).Swizzle("x");
            var input     = var::Resource(Resource::Create(RWStructuredBuffer_u32_Ty, "g_input"));
            var output    = var::Resource(Resource::Create(RWStructuredBuffer_u32_Ty, "g_output"));
            var num_items = var::Resource(Resource::Create(u32Ty, "g_num_items"));

            // var n = input.Read(tid);
            /*EmitWhileLoopMask32([&] {
                EmitIfElse(GetWave32Mask() == var(u32(0)), [] { EmitBreak(); });
                GetWave32Mask() = GetWave32Mask() & (~GetWave32Mask());
            });*/
            // output.Write(tid, val * val);

            wave32::EnableWave32MaskMode();
            wave32::EmitIfElse(
                tid < num_items,
                [&] {
                    var val = var::Zero(input->resource->GetType()->GetTemplateType());

                    assert(IsInScalarBlock());
                    assert(val->IsScalar());

                    wave32::EmitIfLaneActive([&] {
                        assert(!IsInScalarBlock());
                        val = input.Read(tid);
                    });

                    assert(wave32::GetWave32Mask()->IsScalar());
                    assert(!val->IsScalar());

                    wave32::EmitIfElse(val > tid, [&] { output.Write(tid, wave32::GetWave32Mask()); });
                },
                [&] {

                });

            GetGlobalModule().SetGroupSize({u32(32), u32(1), u32(1)});
            fprintf(stdout, GetGlobalModule().Finalize());
            u32x3 group_size = GetGlobalModule().GetGroupSize();

            GPUKernel k = CompileGlobalModule(gfx);
            defer(k.Destroy(gfx));

            std::vector<u32> host_data = {};
            std::vector<u32> host_scan = {};
            u32              N         = u32(1 << 10);
            u32              sum       = u32(0);
            ifor(N) {
                u32 num = u32(rand()) % u32(256);
                host_data.push_back(num);
                host_scan.push_back(sum);
                sum += num;
            }
            GfxBuffer device_data = gfxCreateBuffer<u32>(gfx, N, &host_data[0]);
            defer(gfxDestroyBuffer(gfx, device_data));
            GfxBuffer device_result = gfxCreateBuffer<u32>(gfx, N);
            defer(gfxDestroyBuffer(gfx, device_result));

            gfxProgramSetParameter(gfx, k.program, "g_num_items", N);
            gfxProgramSetParameter(gfx, k.program, "g_output", device_result);
            gfxProgramSetParameter(gfx, k.program, "g_input", device_data);
            gfxCommandBindKernel(gfx, k.kernel);
            gfxCommandDispatch(gfx,                                          //
                               (sum + group_size.x - u32(1)) / group_size.x, //
                               u32(1),                                       //
                               u32(1));

            std::vector<u32> mapping = read_device_buffer<u32>(gfx, device_result);
            ifor(N) {
                fprintf(stdout, "%i ", mapping[i]);
                // assert(i >= host_scan[mapping[i]]);
                // if (mapping[i] + u32(1) < N) {
                // assert(i < host_scan[mapping[i] + u32(1)]);
                //}
            }
        }
    }
#endif
    return 0;
}