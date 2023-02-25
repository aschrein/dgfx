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

#if !defined(COMMON_H)
#    define COMMON_H

#    if defined(__HLSL_VERSION)

#        define f32 float
#        define f32x2 float2
#        define f32x3 float3
#        define f32x3x3 float3x3
#        define f32x4x3 float4x3
#        define f32x3x4 float3x4
#        define f32x4x4 float4x4
#        define f32x4 float4
#        define f16 half
#        define f16x2 half2
#        define f16x3 half3
#        define f16x4 half4
#        define u32 uint
#        define u32x2 uint2
#        define u32x3 uint3
#        define u32x4 uint4
#        define i32 int
#        define i32x2 int2
#        define i32x3 int3
#        define i32x4 int4
#        define asf32 asfloat
#        define asu32 asuint
#        define asi32 asint

#        define ifor(N) for (u32 i = u32(0); i < ((u32)(N)); ++i)
#        define jfor(N) for (u32 j = u32(0); j < ((u32)(N)); ++j)
#        define kfor(N) for (u32 k = u32(0); k < ((u32)(N)); ++k)
#        define xfor(N) for (u32 x = u32(0); x < ((u32)(N)); ++x)
#        define yfor(N) for (u32 y = u32(0); y < ((u32)(N)); ++y)
#        define zfor(N) for (u32 z = u32(0); z < ((u32)(N)); ++z)
#        define mfor(N) for (u32 m = u32(0); m < ((u32)(N)); ++m)

#        define math_max max
#        define math_min min

#    else // #if defined(HLSL_VERSION)

#        undef min
#        undef max

#        include "half.hpp"
#        include <glm/glm.hpp>

using f64     = double;
using f32     = float;
using f32x2   = glm::vec2;
using f32x3   = glm::vec3;
using f32x3x3 = glm::mat3x3;
using f32x4x3 = glm::mat4x3;
using f32x3x4 = glm::mat3x4;
using f32x4x4 = glm::mat4x4;
using f32x4   = glm::vec4;
using f16     = half_float::half;
using f16x2   = glm::vec<2, half_float::half, glm::lowp>;
using f16x3   = glm::vec<3, half_float::half, glm::lowp>;
using f16x4   = glm::vec<4, half_float::half, glm::lowp>;
using i16     = int16_t;
using u32     = glm::uint;
using u8      = unsigned char;
using u64     = uint64_t;
using u32x2   = glm::uvec2;
using u32x3   = glm::uvec3;
using u32x4   = glm::uvec4;
using i32     = int;
using i32x2   = glm::ivec2;
using i32x3   = glm::ivec3;
using i32x4   = glm::ivec4;

using namespace glm;

static u32 asu32(f32 a) {
    f32 *tmp  = &a;
    u32 *utmp = reinterpret_cast<u32 *>(tmp);
    return *utmp;
}
static i32 asi32(f32 a) {
    f32 *tmp  = &a;
    i32 *utmp = reinterpret_cast<i32 *>(tmp);
    return *utmp;
}
static f32 asf32(u32 a) {
    u32 *tmp  = &a;
    f32 *utmp = reinterpret_cast<f32 *>(tmp);
    return *utmp;
}

#        undef min
#        undef max

#        if !defined(ifor)
#            define ifor(N) for (u32 i = u32(0); i < ((u32)(N)); ++i)
#            define jfor(N) for (u32 j = u32(0); j < ((u32)(N)); ++j)
#            define kfor(N) for (u32 k = u32(0); k < ((u32)(N)); ++k)
#            define xfor(N) for (u32 x = u32(0); x < ((u32)(N)); ++x)
#            define yfor(N) for (u32 y = u32(0); y < ((u32)(N)); ++y)
#            define zfor(N) for (u32 z = u32(0); z < ((u32)(N)); ++z)
#            define mfor(N) for (u32 m = u32(0); m < ((u32)(N)); ++m)
#        endif // !defined(ifor)

#        if !defined(defer)
// SRC:
// https://grouse.github.io/posts/defer.html
// https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/
template <typename F>
struct privDefer {
    F f;
    privDefer(F f) : f(f) {}
    ~privDefer() { f(); }
};

template <typename F>
privDefer<F> defer_func(F f) {
    return privDefer<F>(f);
}

#            define DEFER_1(x, y) x##y
#            define DEFER_2(x, y) DEFER_1(x, y)
#            define DEFER_3(x) DEFER_2(x, __COUNTER__)
#            define defer(code) auto DEFER_3(_defer_) = defer_func([&]() { code; })

#        endif // !defined(defer)

#        if !defined(UNIMPLEMENTED)
#            define UNIMPLEMENTED_(s)                                                                                                                                              \
                do {                                                                                                                                                               \
                    fprintf(stderr, "%s:%i UNIMPLEMENTED %s\n", __FILE__, __LINE__, s);                                                                                            \
                    abort();                                                                                                                                                       \
                } while (0)
#            define UNIMPLEMENTED UNIMPLEMENTED_("")
#            define TRAP                                                                                                                                                           \
                do {                                                                                                                                                               \
                    fprintf(stderr, "%s:%i TRAP\n", __FILE__, __LINE__);                                                                                                           \
                    abort();                                                                                                                                                       \
                } while (0)
#        endif // !defined(UNIMPLEMENTED)

#        if !defined(ARRAYSIZE)
#            define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#        endif // !defined(ARRAYSIZE)

#    endif // #if defined(HLSL_VERSION)

#    if defined(__HLSL_VERSION)
#        define STATIC_FUNCTION
#        define STATIC_GLOBAL static
#        define INTOUT_ARGUMENT(type, x) out type x
#    else // #if defined(__HLSL_VERSION)
#        define STATIC_FUNCTION static
#        define STATIC_GLOBAL static
#        define INTOUT_ARGUMENT(type, x) type &x
#        define math_max glm::max
#        define math_min glm::min

STATIC_FUNCTION f32     saturate(f32 x) { return x > f32(1.0) ? f32(1.0) : (x < f32(0.0) ? f32(0.0) : x); }
STATIC_FUNCTION f32x4   mul(f32x4x4 mat, f32x4 vec) { return mat * vec; }
STATIC_FUNCTION f32x3   mul(f32x3x3 mat, f32x3 vec) { return mat * vec; }
STATIC_FUNCTION f32x3   mul(f32x3 vec, f32x3x3 mat) { return vec * mat; }
STATIC_FUNCTION f32x3x3 mul(f32x3x3 a, f32x3x3 b) { return a * b; }
STATIC_FUNCTION f32x4   lerp(f32x4 a, f32x4 b, f32 t) { return mix(a, b, t); }
STATIC_FUNCTION f32x3   lerp(f32x3 a, f32x3 b, f32 t) { return mix(a, b, t); }
STATIC_FUNCTION f32x2   lerp(f32x2 a, f32x2 b, f32 t) { return mix(a, b, t); }
STATIC_FUNCTION f32     lerp(f32 a, f32 b, f32 t) { return mix(a, b, t); }
STATIC_FUNCTION f32     rsqrt(f32 a) { return f32(1.0) / sqrt(a); }
STATIC_FUNCTION f32     dot2(f32x3 a) { return dot(a, a); }

namespace std {
template <>
struct hash<std::pair<u16, u16>> {
    u64 operator()(std::pair<u16, u16> const &item) const { return hash<u64>()((u64)item.first + hash<u64>()((u64)item.second)); }
};
template <>
struct hash<std::pair<u32, u32>> {
    u64 operator()(std::pair<u32, u32> const &item) const { return hash<u64>()((u64)item.first + hash<u64>()((u64)item.second)); }
};
}; // namespace std

template <typename T, typename K>
static bool contains(T const &t, K const &k) {
    return t.find(k) != t.end();
}

#    endif // #if defined(__HLSL_VERSION)

STATIC_FUNCTION f32x4 f32x4_splat(f32 a) { return f32x4(a, a, a, a); }
STATIC_FUNCTION f32x3 f32x3_splat(f32 a) { return f32x3(a, a, a); }
STATIC_FUNCTION f32x2 f32x2_splat(f32 a) { return f32x2(a, a); }
STATIC_FUNCTION u32x4 u32x4_splat(u32 a) { return u32x4(a, a, a, a); }

#    if !defined(__HLSL_VERSION)

STATIC_FUNCTION f32x2 f32x2_splat(f64 a) { return f32x2(f32(a), f32(a)); }
STATIC_FUNCTION f32x3 f32x3_splat(f64 a) { return f32x3(f32(a), f32(a), f32(a)); }
STATIC_FUNCTION f32x4 f32x4_splat(f64 a) { return f32x4(f32(a), f32(a), f32(a), f32(a)); }

#    endif // !defined(__HLSL_VERSION)

struct Hit {
    f32 t;
    u32 primitive_idx;
};

struct Ray {
    f32x3 o;
    f32x3 d;
    f32x3 ird;
};

struct AABB {
    f32x3 lo;
    f32x3 hi;

    f32x3 mid() { return (lo + hi) * f32(0.5); }
    void  expand(AABB that) {
        lo.x = min(lo.x, that.lo.x);
        lo.y = min(lo.y, that.lo.y);
        lo.z = min(lo.z, that.lo.z);

        hi.x = max(hi.x, that.hi.x);
        hi.y = max(hi.y, that.hi.y);
        hi.z = max(hi.z, that.hi.z);
    }
    void expand(f32x3 p) {
        lo.x = min(lo.x, p.x);
        lo.y = min(lo.y, p.y);
        lo.z = min(lo.z, p.z);

        hi.x = max(hi.x, p.x);
        hi.y = max(hi.y, p.y);
        hi.z = max(hi.z, p.z);
    }
    f32 area() {
        f32x3 dim = hi - lo;
        return dim.x * dim.y * dim.z;
    }
    // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection.html
    STATIC_FUNCTION f32x2 hit_aabb(f32x3 ro,       //
                                   f32x3 rid,      //
                                   f32x3 aabb_min, //
                                   f32x3 aabb_max  //
    ) {
        f32x3 tb   = rid * (aabb_min - ro);
        f32x3 tt   = rid * (aabb_max - ro);
        f32x3 tmin = min(tt, tb);
        f32x3 tmax = max(tt, tb);
        f32x2 t    = max(f32x2(tmin.xx), f32x2(tmin.yz));
        f32   t0   = max(t.x, t.y);
        f32x2 t2   = min(f32x2(tmax.xx), f32x2(tmax.yz));
        f32   t1   = min(t2.x, t2.y);
        return f32x2(max(t0, f32(0.0)), max(t1, f32(0.0)));
    }
    bool ray_test(f32x3 ro, //
                  f32x3 rid //
    ) {
        f32x2 hit = hit_aabb(ro, rid, lo, hi);
        return hit.x < hit.y;
    }
    bool contains(f32x3 p) {
        return p.x > lo.x && //
               p.y > lo.y && //
               p.z > lo.z && //
               p.x < hi.x && //
               p.y < hi.y && //
               p.z < hi.z    //
            ;
    }
};
struct Sphere {
    f32x4 pos_radius;
    f32x4 color;

    f32 intersect(Ray r) {
        f32x3 dr       = pos_radius.xyz - r.o;
        f32   dr_dot_r = dot(r.d, dr);
        if (dr_dot_r < f32(0.0)) return f32(-1.0);
        f32 c = dot(dr, dr) - dr_dot_r * dr_dot_r;
        if (c < f32(0.0)) return f32(-1.0);
        f32 r2 = pos_radius.w * pos_radius.w;
        if (c > r2) return f32(-1.0);
        f32 d = r2 - c;
        if (d < f32(0.0)) return f32(-1.0);
        d = sqrt(d);
        return dr_dot_r - d;
    }
};

// Src: Hacker's Delight, Henry S. Warren, 2001
STATIC_FUNCTION f32 RadicalInverse_VdC(u32 bits) {
    bits = (bits << u32(16)) | (bits >> u32(16));
    bits = ((bits & u32(0x55555555)) << u32(1)) | ((bits & u32(0xAAAAAAAA)) >> u32(1));
    bits = ((bits & u32(0x33333333)) << u32(2)) | ((bits & u32(0xCCCCCCCC)) >> u32(2));
    bits = ((bits & u32(0x0F0F0F0F)) << u32(4)) | ((bits & u32(0xF0F0F0F0)) >> u32(4));
    bits = ((bits & u32(0x00FF00FF)) << u32(8)) | ((bits & u32(0xFF00FF00)) >> u32(8));
    return f32(bits) * f32(2.3283064365386963e-10); // / 0x100000000
}
STATIC_FUNCTION f32x2 Hammersley(u32 i, u32 N) { return f32x2(f32(i) / f32(N), RadicalInverse_VdC(i)); }

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
STATIC_FUNCTION f32x3 SampleGGXVNDF(f32x3 Ve, f32 alpha_x, f32 alpha_y, f32 U1, f32 U2) {
    // Input Ve: view direction
    // Input alpha_x, alpha_y: roughness parameters
    // Input U1, U2: uniform random numbers
    // Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
    //
    //
    // Section 3.2: transforming the view direction to the hemisphere configuration
    f32x3 Vh = normalize(f32x3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    f32   lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    f32x3 T1    = lensq > 0 ? f32x3(-Vh.y, Vh.x, 0) * rsqrt(lensq) : f32x3(1.0, 0.0, 0.0);
    f32x3 T2    = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    f32       r    = sqrt(U1);
    const f32 M_PI = f32(3.14159265358979);
    f32       phi  = f32(2.0) * M_PI * U2;
    f32       t1   = r * cos(phi);
    f32       t2   = r * sin(phi);
    f32       s    = f32(0.5) * (f32(1.0) + Vh.z);
    t2             = (f32(1.0) - s) * sqrt(f32(1.0) - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    f32x3 Nh = t1 * T1 + t2 * T2 + sqrt(max(f32(0.0), f32(1.0) - t1 * t1 - t2 * t2)) * Vh;
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    f32x3 Ne = normalize(f32x3(alpha_x * Nh.x, alpha_y * Nh.y, max(f32(0.0), Nh.z)));
    return Ne;
}
STATIC_FUNCTION f32x3x3 GetTBN(f32x3 N) {
    f32x3 U = f32x3_splat(0.0);
    // Simple 2d orthogonalization
    if (abs(N.z) > f32(1.e-6)) {
        U.x = f32(0.0);
        U.y = -N.z;
        U.z = N.y;
    } else {
        U.x = N.y;
        U.y = -N.x;
        U.z = f32(0.0);
    }
    U = normalize(U);

    f32x3x3 TBN;
    TBN[0] = U;
    TBN[1] = cross(N, U);
    TBN[2] = N;
    return TBN;
}
STATIC_FUNCTION f32x3 SampleReflectionVector(f32x3 view_direction, f32x3 normal, f32 roughness, f32x2 xi) {
    if (roughness < f32(0.001)) return reflect(view_direction, normal);
    f32x3x3 tbn_transform           = transpose(GetTBN(normal));
    f32x3   view_direction_tbn      = mul(-view_direction, tbn_transform);
    //f32     a                       = roughness * roughness;
    f32x3   sampled_normal_tbn      = SampleGGXVNDF(view_direction_tbn, roughness * roughness, roughness * roughness, xi.x, xi.y);
    f32x3   reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);
    // Transform reflected_direction back to the initial space.
    f32x3x3 inv_tbn_transform = transpose(tbn_transform);
    return mul(reflected_direction_tbn, inv_tbn_transform);
}

STATIC_GLOBAL const f32 PI           = f32(3.14159265358979);
STATIC_GLOBAL const f32 GOLDEN_RATIO = f32(1.61803398875);

// Helper macro to do in-shader bisect
#    define FIND_BISECT(offset, cnt)                                                                                                                                               \
        {                                                                                                                                                                          \
            u32 b = u32(0);                                                                                                                                                        \
            u32 e = cnt;                                                                                                                                                           \
            while (e - b > u32(1)) {                                                                                                                                               \
                u32 m = (b + e) / u32(2);                                                                                                                                          \
                if (FIND_BISECT_LOAD(m) > offset) {                                                                                                                                \
                    e = m;                                                                                                                                                         \
                } else {                                                                                                                                                           \
                    b = m;                                                                                                                                                         \
                }                                                                                                                                                                  \
            }                                                                                                                                                                      \
            FIND_BISECT_RESULT = b;                                                                                                                                                \
        }

STATIC_GLOBAL Ray GenDiffuseRay(f32x3 p, f32x3 n, f32x2 xi) {
    f32x3x3 TBN          = GetTBN(n);
    f32     sint         = sqrt(xi.y);
    f32     cost         = sqrt(f32(1.0) - xi.y);
    f32     M_PI         = f32(3.14159265358979);
    f32x3   local_coords = f32x3(cost * cos(xi.x * M_PI * f32(2.0)), cost * sin(xi.x * M_PI * f32(2.0)), sint);
    f32x3   d            = normalize(TBN[2] * local_coords.z + TBN[0] * local_coords.x + TBN[1] * local_coords.y);
    Ray     r;
    r.o   = p + n * f32(1.0e-3);
    r.d   = d;
    r.ird = f32(1.0) / r.d;

    return r;
}

STATIC_GLOBAL f32x3 Interpolate(f32x3 v0, f32x3 v1, f32x3 v2, f32x2 barys) { return v0 * (f32(1.0) - barys.x - barys.y) + v1 * barys.x + v2 * barys.y; }
STATIC_GLOBAL f32x4 Interpolate(f32x4 v0, f32x4 v1, f32x4 v2, f32x2 barys) { return v0 * (f32(1.0) - barys.x - barys.y) + v1 * barys.x + v2 * barys.y; }

// https://google.github.io/filament/Filament.md.html#materialsystem/dielectricsandconductors
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// Don't mess up srgb roughness
struct GGXHelper {
    f32 NdotL;
    f32 NdotV;
    f32 LdotH;
    f32 VdotH;
    f32 NdotH;

    void Init(f32x3 L, f32x3 N, f32x3 V) {
        f32x3 H = normalize(L + V);
        LdotH   = saturate(dot(L, H));
        VdotH   = saturate(dot(V, H));
        NdotV   = saturate(dot(N, V));
        NdotH   = saturate(dot(N, H));
        NdotL   = saturate(dot(N, L));
    }
    f32 _GGX_G(f32 a2, f32 XdotY) {
        return //
            f32(2.0) * XdotY /
            // ------------------------ //
            (f32(1.0e-6) + XdotY + sqrt(a2 + (f32(1.0) - a2) * XdotY * XdotY)) //
            ;
    }
    f32 _GGX_GSchlick(f32 a, f32 XdotY) {
        f32 k = a / f32(2.0);

        return //
            XdotY /
            // ------------------------ //
            (XdotY * (f32(1.0) - k) + k) //
            ;
    }
    f32 DistributionGGX(f32 a2) {
        f32 NdotH2 = NdotH * NdotH;
        f32 denom  = (NdotH2 * (a2 - f32(1.0)) + f32(1.0));
        denom      = PI * denom * denom;
        return a2 / denom;
    }
    f32x3 ImportanceSampleGGX(f32x2 xi, f32x3 N, f32 roughness) {
        f32   a         = roughness * roughness;
        f32   phi       = f32(2.0) * PI * xi.x;
        f32   cos_theta = sqrt((f32(1.0) - xi.y) / (f32(1.0) + (a * a - f32(1.0)) * xi.y));
        f32   sin_theta = sqrt(f32(1.0) - cos_theta * cos_theta);
        f32x3 H;
        H.x         = cos(phi) * sin_theta;
        H.y         = sin(phi) * sin_theta;
        H.z         = cos_theta;
        f32x3x3 TBN = GetTBN(N);
        return normalize(TBN[0] * H.x + TBN[1] * H.y + TBN[2] * H.z);
    }
    f32 G(f32 r) {
        // Smith
        // G(l,v,h)=G1(l)G1(v)
        f32 a  = r * r;
        f32 a2 = a * a;
        return _GGX_G(a2, NdotV) * _GGX_G(a2, NdotL);
    }
    f32 D(f32 r) {
        // GGX (Trowbridge-Reitz)
        f32 a  = r * r;
        f32 a2 = a * a;
        f32 f  = NdotH * NdotH * (a2 - f32(1.0)) + f32(1.0);
        return a2 / (PI * f * f + f32(1.0e-6));
    }
    f32x3 fresnel(f32x3 f0 = f32x3_splat(0.04)) { return f0 + (f32x3_splat(1.0) - f0) * pow(saturate(f32(1.0) - VdotH), f32(5.0)); }
    f32   eval(f32 r) { return NdotL * G(r) * D(r); }
};


// https://www.shadertoy.com/view/XtGGzG
STATIC_FUNCTION f32x3 viridis_quintic(f32 x) {
    x        = saturate(x);
    f32x4 x1 = f32x4(1.0, x, x * x, x * x * x); // 1 x x2 x3
    f32x4 x2 = x1 * x1.w * x;                   // x4 x5 x6 x7
    return f32x3(
        dot(f32x4(x1.xyzw), f32x4(f32(+0.280268003), f32(-0.143510503), f32(+2.225793877), f32(-14.815088879))) + dot(f32x2(x2.xy), f32x2(f32(+25.212752309), f32(-11.772589584))),
        dot(f32x4(x1.xyzw), f32x4(f32(-0.002117546), f32(+1.617109353), f32(-1.909305070), f32(+2.701152864))) + dot(f32x2(x2.xy), f32x2(f32(-1.685288385), f32(+0.178738871))),
        dot(f32x4(x1.xyzw), f32x4(f32(+0.300805501), f32(+2.614650302), f32(-12.019139090), f32(+28.933559110))) +
            dot(f32x2(x2.xy), f32x2(f32(-33.491294770), f32(+13.762053843))));
}
///////////////////////////////////////////////////////
// https://github.com/Shmaug/Stratum2/blob/main/src/Shaders/common/rng.hlsli

// xxhash (https://github.com/Cyan4973/xxHash)
//   From https://www.shadertoy.com/view/Xt3cDn
STATIC_FUNCTION u32 xxhash32(const u32 p) {
    const u32 PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
    const u32 PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
    u32       h32 = p + PRIME32_5;
    h32           = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
    h32           = PRIME32_2 * (h32 ^ (h32 >> 15));
    h32           = PRIME32_3 * (h32 ^ (h32 >> 13));
    return h32 ^ (h32 >> 16);
}

// https://www.pcg-random.org/
STATIC_FUNCTION u32 pcg(u32 v) {
    u32 state = v * u32(747796405) + u32(2891336453);
    u32 word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> u32(22)) ^ word;
}
// http://www.jcgt.org/published/0009/03/02/
STATIC_FUNCTION u32x4 pcg4d(u32x4 v) {
    v ^= v >> u32(16);
    v.x += v.y * v.w;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.w += v.y * v.z;
    return v;
}

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
struct Octahedral {
    static f32 Sign(f32 v) {
        if (v >= f32(0.0))
            return f32(1.0);
        else
            return f32(-1.0);
    }
    static f32x2 OctWrap(f32x2 v) {
        f32x2 tmp = f32x2(Sign(v.x), Sign(v.y));
        return (f32(1.0) - abs(f32x2(v.yx))) * tmp;
    }
    static f32x2 Encode(f32x3 n) {
        n /= (abs(n.x) + abs(n.y) + abs(n.z));
        if (n.z >= f32(0.0))
            n.xy = n.xy;
        else
            n.xy = OctWrap(n.xy);
        n.xy = n.xy * f32(0.5) + f32(0.5);
        return n.xy;
    }
    static f32x3 Decode(f32x2 f) {
        f = f * f32(2.0) - f32(1.0);

        // https://twitter.com/Stubbesaurus/status/937994790553227264
        f32x3 n = f32x3(f.x, f.y, f32(1.0) - abs(f.x) - abs(f.y));
        f32   t = saturate(-n.z);
        n.xy += f32x2(Sign(n.x), Sign(n.y)) * -t;
        return normalize(n);
    }
};

#endif // !deifned(COMMON_H)