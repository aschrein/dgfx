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

enum Material_t {
    MATERIAL_TYPE_UNKNOWN = 0, //
    MATERIAL_TYPE_GRASS        //
};

enum PrimitiveType_t {
    PRIMITIVE_TYPE_UNKNOWN = 0, //
    PRIMITIVE_TYPE_CUBE,        //
    PRIMITIVE_TYPE_SPHERE,      //
    PRIMITIVE_TYPE_WATER_PLANE,      //
};
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
STATIC_FUNCTION f32x3 pick_cube_normal(f32x3 p) {
    if (abs(p.x) > abs(p.y))
        if (abs(p.x) > abs(p.z))
            return f32x3(p.x > f32(0.0) ? f32(1.0) : f32(-1.0), f32(0.0), f32(0.0));
        else
            return f32x3(f32(0.0), f32(0.0), p.z > f32(0.0) ? f32(1.0) : f32(-1.0));
    else if (abs(p.y) > abs(p.z))
        return f32x3(f32(0.0), p.y > f32(0.0) ? f32(1.0) : f32(-1.0), f32(0.0));
    else
        return f32x3(f32(0.0), f32(0.0), p.z > f32(0.0) ? f32(1.0) : f32(-1.0));
}
struct Material {
    f32x3 albedo;
    u32   material_type;

    u32 primitive_type;
    f32 metalic;
    f32 transparency;
    f32 refraction;

    f32x3 emission;
    f32   roughness;

    void intersect(u32 primitive_idx, Ray ray, f32x3 aabb_lo, f32x3 aabb_hi, INTOUT_ARGUMENT(Hit, hit)) {

        if (primitive_type == PRIMITIVE_TYPE_SPHERE) {
            Sphere s;
            s.pos_radius = f32x4((aabb_lo + aabb_hi) / f32(2.0), f32(0.5));
            // s.r = f32(0.5);

            f32 t = s.intersect(ray);

            if (t > f32(0.0) && t < hit.t) {
                hit.primitive_idx = primitive_idx;
                hit.t             = t;
            }
        } else if (primitive_type == PRIMITIVE_TYPE_CUBE) {

            f32x2 hit_min_max = hit_aabb(ray.o, ray.ird, aabb_lo, aabb_hi);

            if (hit_min_max.x < hit.t) {
                hit.primitive_idx = primitive_idx;
                hit.t             = hit_min_max.x;
            }
        } else {
        }
    }
    f32x3 get_normal(AABB aabb, f32x3 p) {
        if (primitive_type == PRIMITIVE_TYPE_SPHERE) {
            return normalize(p - (aabb.lo + aabb.hi) / f32(2.0));
        } else if (primitive_type == PRIMITIVE_TYPE_CUBE) {
            return pick_cube_normal(p - (aabb.lo + aabb.hi) * f32(0.5));
        } else {
            return f32x3(0.0, 0.0, 0.0);
        }
    }
};
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

struct RadianceHashItem {
    u32   key;
    f32x3 radiance;
};

enum {
    RADIANCE_HASH_GRID_NUM_ITEMS = u32(16 << 20),
    RADIANCE_HASH_GRID_MASK      = u32(u32(16 << 20) - u32(1)),
};

#if defined(__HLSL_VERSION)
#else // #if defined(__HLSL_VERSION)
#    undef max
#    undef min
#endif // #if defined(__HLSL_VERSION)
