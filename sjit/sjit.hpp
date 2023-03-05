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

#if !defined(JIT_HPP)
#    define JIT_HPP

#    pragma warning(disable : 4244)

#    undef min
#    undef max

#    if !defined(GLM_SETUP_INCLUDED)
#        include "3rdparty/glm/ext.hpp"
#        include "3rdparty/glm/glm.hpp"
#    endif // !defined (GLM_SETUP_INCLUDED)
#    include "3rdparty/half.hpp"
#    include "3rdparty/robin-map/include/tsl/robin_map.h"
#    include "3rdparty/robin-map/include/tsl/robin_set.h"
#    include <dxgiformat.h>
#    include <stdarg.h>

#    if !defined(ifor)
#        define ifor(N) for (u32 i = u32(0); i < ((u32)(N)); ++i)
#        define jfor(N) for (u32 j = u32(0); j < ((u32)(N)); ++j)
#        define kfor(N) for (u32 k = u32(0); k < ((u32)(N)); ++k)
#        define xfor(N) for (u32 x = u32(0); x < ((u32)(N)); ++x)
#        define yfor(N) for (u32 y = u32(0); y < ((u32)(N)); ++y)
#        define zfor(N) for (u32 z = u32(0); z < ((u32)(N)); ++z)
#        define mfor(N) for (u32 m = u32(0); m < ((u32)(N)); ++m)
#    endif // !defined(ifor)

#    if !defined(defer)
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

#        define DEFER_1(x, y) x##y
#        define DEFER_2(x, y) DEFER_1(x, y)
#        define DEFER_3(x) DEFER_2(x, __COUNTER__)
#        define defer(code) auto DEFER_3(_defer_) = defer_func([&]() { code; })

#    endif // !defined(defer)

#    if !defined(SJIT_UNIMPLEMENTED)
#        define SJIT_UNIMPLEMENTED_(s)                                                                                                                                             \
            do {                                                                                                                                                                   \
                fprintf(stderr, "%s:%i SJIT_UNIMPLEMENTED %s\n", __FILE__, __LINE__, s);                                                                                           \
                abort();                                                                                                                                                           \
            } while (0)
#        define SJIT_UNIMPLEMENTED SJIT_UNIMPLEMENTED_("")
#        define SJIT_TRAP                                                                                                                                                          \
            do {                                                                                                                                                                   \
                fprintf(stderr, "%s:%i SJIT_TRAP\n", __FILE__, __LINE__);                                                                                                          \
                abort();                                                                                                                                                           \
            } while (0)
#    endif // !defined(SJIT_UNIMPLEMENTED)

#    if !defined(SJIT_ARRAYSIZE)
#        define SJIT_ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#    endif // !defined(SJIT_ARRAYSIZE)

#    define sjit_assert(x)                                                                                                                                                         \
        do {                                                                                                                                                                       \
            if (!(x)) {                                                                                                                                                            \
                fprintf(stderr, "%s:%i [FAIL] at %s\n", __FILE__, __LINE__, #x);                                                                                                   \
                abort();                                                                                                                                                           \
            }                                                                                                                                                                      \
        } while (0)

#    define sjit_debug_assert(x) sjit_assert(x)

#    define SJIT_REFERENCE_COUNTER_IMPL                                                                                                                                            \
        std::atomic<i32> _reference_counter = i32(0);                                                                                                                              \
        u32              IncrRef() { return u32(++_reference_counter); }                                                                                                           \
        u32              DecrRef() {                                                                                                                                               \
            i32 cnt = --_reference_counter;                                                                                                                           \
            if (cnt == i32(0)) {                                                                                                                                      \
                delete this;                                                                                                                                          \
            }                                                                                                                                                         \
            sjit_debug_assert(cnt >= i32(0));                                                                                                                         \
            return u32(cnt);                                                                                                                                          \
        }

namespace SJIT {
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

static constexpr float PI                  = float(3.1415926);
static constexpr float TWO_PI              = float(6.2831852);
static constexpr float FOUR_PI             = float(12.566370);
static constexpr float INV_PI              = float(0.3183099);
static constexpr float INV_TWO_PI          = float(0.1591549);
static constexpr float INV_FOUR_PI         = float(0.0795775);
static constexpr float DIELECTRIC_SPECULAR = float(0.04);

static f32x4 f32x4_splat(f32 a) { return f32x4(a, a, a, a); }
static f32x3 f32x3_splat(f32 a) { return f32x3(a, a, a); }
static f32x2 f32x2_splat(f32 a) { return f32x2(a, a); }
static u32x4 u32x4_splat(u32 a) { return u32x4(a, a, a, a); }
static u32x3 u32x3_splat(u32 a) { return u32x3(u32(a), u32(a), u32(a)); }
static f32x2 f32x2_splat(f64 a) { return f32x2(f32(a), f32(a)); }
static f32x3 f32x3_splat(f64 a) { return f32x3(f32(a), f32(a), f32(a)); }
static f32x4 f32x4_splat(f64 a) { return f32x4(f32(a), f32(a), f32(a), f32(a)); }

template <typename T>
class SharedPtr {
private:
    mutable T *ptr = NULL;

    void Release() {
        if (get()) get()->DecrRef();
        set(NULL);
    }

public:
    T   *get() const { return ptr; }
    void set(T *v) const { ptr = v; }

    SharedPtr(T *_ptr = NULL) {
        if (_ptr != NULL) {
            _ptr->IncrRef();
            set(_ptr);
        }
    }
    template <typename K>
    SharedPtr(SharedPtr<K> const &that) {
        if (that.get() == (T *)get()) return;
        Release();
        if (that.get() == NULL) return;
        that.get()->IncrRef();
        set((T *)that.get());
    }
    SharedPtr(SharedPtr<T> const &that) {
        if (that.get() == (T *)get()) return;
        Release();
        if (that.get() == NULL) return;
        that.get()->IncrRef();
        set((T *)that.get());
    }
    SharedPtr const &operator=(SharedPtr<T> const &that) {
        if (that.get() == (T *)get()) return *this;
        Release();
        if (that.get() == NULL) return *this;
        that.get()->IncrRef();
        set((T *)that.get());
        return *this;
    }
    SharedPtr(SharedPtr<T> &&that) {
        if (that.get() == get()) return;
        Release();
        if (that.get() == NULL) return;
        set((T *)that.get());
        that.set(NULL);
    }
    SharedPtr const &operator=(SharedPtr<T> &&that) {
        if (that.get() == get()) return *this;
        Release();
        if (that.get() == NULL) return *this;
        set((T *)that.get());
        that.set(NULL);
        return *this;
    }
#    if 0
				template <typename K>
    SharedPtr(SharedPtr<K> const &that) {
        if (that.get() == (T *)get()) return;
        Release();
        if (that.get() == NULL) return;
        that.get()->IncrRef();
        set((T *)that.get());
    }
    template <typename K>
    SharedPtr const &operator=(SharedPtr<K> const &that) {
        if (that.get() == (T *)get()) return *this;
        Release();
        if (that.get() == NULL) return *this;
        that.get()->IncrRef();
        set((T *)that.get());
        return *this;
    }
    template <typename K>
    SharedPtr(SharedPtr<K> &&that) {
        if (that.get() == get()) return;
        Release();
        if (that.get() == NULL) return;
        set((T *)that.get());
        that.set(NULL);
    }
    template <typename K>
    SharedPtr const &operator=(SharedPtr<K> &&that) {
        if (that.get() == get()) return *this;
        Release();
        if (that.get() == NULL) return *this;
        set((T *)that.get());
        that.set(NULL);
        return *this;
    }
#    endif // 0

    template <typename K>
    bool operator==(SharedPtr<K> const &that) const {
        return get() == (T *)that.get();
    }
    template <typename K>
    bool operator!=(SharedPtr<K> const &that) const {
        return get() != (T *)that.get();
    }
    T *operator->() const { return ptr; }
    ~SharedPtr() { Release(); }
    operator bool() const { return ptr != NULL; }
};
template <int N>
static constexpr u64 compute_hash(char const _c_str[N]) {
    u64 _hash = u64(5381);
    ifor(N) {
        char c = _c_str[i];
        u64  v = u64(c) * u64(3935559000370003845) + u64(2691343689449507681);
        _hash  = _hash ^ v;
    }
    return _hash;
}
static u64 compute_hash(char const *_c_str) {
    u64 _hash = u64(5381);
    if (_c_str == NULL) return u64(0);
    u64 cursor = u64(0);
    while (true) {
        char c = _c_str[cursor++];
        if (c == '\0') break;
        u64 v = u64(c) * u64(3935559000370003845) + u64(2691343689449507681);
        _hash = _hash ^ v;
    }
    return _hash;
}
struct c_str {
    char const *data;
    u64         hash;

    c_str() = default;
    c_str(char const *_c_str) : data(_c_str), hash(compute_hash(_c_str)) {}
};
class String {
private:
    char const *data = NULL;
    u32         hash = u32(0);
    u8          own  = u8(0);

    void Release() {
        if (own) {
            if (data) delete[] data;
        }
        data = NULL;
        hash = u32(0);
        own  = 0;
    }

    u32 UpdateHash() {
        hash = u32(compute_hash(data) & u32(0xffffffff));
        return hash;
    }

    void Init(char const *c_str = NULL, bool _copy = false) {
        if (c_str != NULL) {
            if (_copy) {
                u64   len   = strlen(c_str) + u64(1);
                char *_data = new char[len];
                memcpy(_data, c_str, len);
                data = _data;
                own  = u32(1);
            } else {
                data = c_str;
                own  = u32(0);
            }
            UpdateHash();
        }
    }
    void Init(char const *c_str, u32 _hash, bool _own = true) {
        if (c_str != NULL) {
            if (_own) {
                u64   len   = strlen(c_str) + u64(1);
                char *_data = new char[len];
                memcpy(_data, c_str, len);
                data = _data;
                own  = u8(1);
            } else {
                data = c_str;
                own  = u8(0);
            }
            hash = _hash;
        } else {
            data = NULL;
            own  = u32(0);
            hash = u64(0);
        }
    }

public:
    template <int N>
    constexpr String(char const c_str[N]) : data(c_str), hash(u32(compute_hash(c_str) & u32(0xffffffff))), own(0) {}
    String(char const *c_str = NULL) { Init(c_str); }
    u32    GetHash() const { return hash; }
    String Copy() const {
        String o = {};
        o.Init(c_str(), hash, /* own */ true);
        return o;
    }
    String(String const &that) { Init(that.c_str(), that.GetHash(), that.own); }
    String const &operator=(String const &that) {
        if (that.c_str() == c_str()) return *this;
        Release();
        if (that.c_str() == NULL) return *this;

        Init(that.c_str(), that.GetHash(), that.own);

        return *this;
    }
    String(String &&that) {
        if (that.c_str() == c_str()) return;
        Release();
        if (that.c_str() == NULL) return;

        data      = that.data;
        hash      = that.hash;
        own       = that.own;
        that.data = NULL;
        that.hash = u32(0);
        that.own  = u64(0);
    }
    String const &operator=(String &&that) {
        if (that.c_str() == c_str()) return *this;
        Release();
        if (that.c_str() == NULL) return *this;

        data      = that.data;
        hash      = that.hash;
        own       = that.own;
        that.data = NULL;
        that.hash = u32(0);
        that.own  = u64(0);

        return *this;
    }
    char const *c_str() const { return data; }
    ~String() { Release(); }
    bool operator==(String const &that) const {
        if (GetHash() != that.GetHash()) return false;
        if (c_str() == that.c_str() && c_str() == NULL) return true;
        if (c_str() == NULL && that.c_str() != NULL) return false;
        if (c_str() != NULL && that.c_str() == NULL) return false;

        u64 cursor = u64(0);
        while (true) {
            char a = c_str()[cursor];
            char b = that.c_str()[cursor];
            if (a != b) return false;
            if (a == '\0') break;
            cursor++;
        }
        return true;
    }
    operator char const *() { return data; }
};
// static constexpr size_t s = sizeof(String);
static_assert(sizeof(String) == u64(8 * 2));
} // namespace SJIT

namespace std {
template <>
struct hash<SJIT::String> {
    SJIT::u64 operator()(SJIT::String const &x) const {
        using namespace SJIT;
        return x.GetHash();
    }
};
}; // namespace std

#    if 0
				namespace ankerl::unordered_dense {
template <>
struct hash<SJIT::String> {

    using is_avalanching = void;

    [[nodiscard]] auto operator()(SJIT::String const &x) const noexcept -> SJIT::u64 {
        using namespace SJIT;
        u64 hash = u64(5381);
        if (x.c_str() == NULL) return u64(0);
        u64 cursor = u64(0);
        while (true) {
            char c = x.c_str()[cursor++];
            if (c == '\0') break;
            u64 v = u64(c) * u64(3935559000370003845) + u64(2691343689449507681);
            hash  = hash ^ v;
        }
        return hash;
    }
};
} // namespace ankerl::unordered_dense
#    endif // 0

namespace SJIT {

template <typename K, typename V>
using HashMap = tsl::robin_map<K, V>;
template <typename K>
using HashSet = tsl::robin_set<K>;
template <typename K>
using Array = std::vector<K>;

enum BasicType {
    BASIC_TYPE_UNKNOWN = 0,
    BASIC_TYPE_NUMBER,
    BASIC_TYPE_WILDCARD,
    BASIC_TYPE_VOID,
    BASIC_TYPE_F16,
    BASIC_TYPE_F32,
    BASIC_TYPE_U1,
    BASIC_TYPE_U8,
    BASIC_TYPE_I32,
    BASIC_TYPE_U32,
    BASIC_TYPE_STRUCTURE,
    BASIC_TYPE_RESOURCE,
    BASIC_TYPE_ARRAY,
};
static bool IsBasicTypeScalar(BasicType ty) {
    switch (ty) {
    case BASIC_TYPE_F16:
    case BASIC_TYPE_F32:
    case BASIC_TYPE_U1:
    case BASIC_TYPE_U8:
    case BASIC_TYPE_I32:
    case BASIC_TYPE_U32: return true;
    default: return false;
    }
}
enum EXPRESSION_TYPE {
    EXPRESSION_TYPE_UNKNOWN = 0,
    EXPRESSION_TYPE_OP,
    EXPRESSION_TYPE_FUNCTION,
    EXPRESSION_TYPE_RESOURCE,
    EXPRESSION_TYPE_LITERAL,
    EXPRESSION_TYPE_ARRAY,
    EXPRESSION_TYPE_INPUT,
    EXPRESSION_TYPE_SWIZZLE,
    EXPRESSION_TYPE_STRUCT_INIT,
    EXPRESSION_TYPE_FIELD,
    EXPRESSION_TYPE_INDEX,
    EXPRESSION_TYPE_REF,
    // EXPRESSION_TYPE_RETURN,
    EXPRESSION_TYPE_IF_ELSE,
    // EXPRESSION_TYPE_WHILE,
    // EXPRESSION_TYPE_ELSE,
    // EXPRESSION_TYPE_BREAK,
};
enum RWType { RW_UNKNOWN = 0, RW_READ, RW_READ_WRITE };
enum ResType { RES_UNKNOWN = 0, RES_TEXTURE, RES_BUFFER, RES_CONSTANT, RES_SAMPLER, RES_TLAS };
enum DimType { DIM_UNKNOWN = 0, DIM_1D, DIM_2D, DIM_3D, DIM_1D_ARRAY, DIM_2D_ARRAY };
static u32 GetNumDims(DimType dt) {
    switch (dt) {
    case DIM_1D: return u32(1);
    case DIM_2D: return u32(2);
    case DIM_3D: return u32(3);
    case DIM_1D_ARRAY: return u32(2);
    case DIM_2D_ARRAY: return u32(3);
    default: SJIT_UNIMPLEMENTED;
    }
}
enum OpType {
    OP_UNKNOWN = 0,
    OP_PLUS,
    OP_MINUS,
    OP_MUL,
    OP_DIV,
    OP_LESS,
    OP_LESS_OR_EQUAL,
    OP_GREATER,
    OP_GREATER_OR_EQUAL,
    OP_PLUS_ASSIGN,
    OP_MUL_ASSIGN,
    OP_DIV_ASSIGN,
    OP_MINUS_ASSIGN,
    OP_BIT_OR_ASSIGN,
    OP_BIT_AND_ASSIGN,
    OP_BIT_XOR_ASSIGN,
    OP_ASSIGN,
    OP_LOGICAL_AND,
    OP_BIT_AND,
    OP_BIT_OR,
    OP_BIT_XOR,
    OP_BIT_NEG,
    OP_SHIFT_LEFT,
    OP_SHIFT_RIGHT,
    OP_LOGICAL_OR,
    OP_LOGICAL_NOT,
    OP_EQUAL,
    OP_MODULO,
    OP_NOT_EQUAL,
};
enum InType {
    IN_TYPE_UNKNOWN = 0,
    IN_TYPE_DISPATCH_THREAD_ID,
    IN_TYPE_DISPATCH_GROUP_ID,
    IN_TYPE_GROUP_THREAD_ID,
    IN_TYPE_CUSTOM,
};
class Type;
class HLSLModule;
static void EmitType(Type *ty, HLSLModule &hlsl_module);
class Type {
private:
    String                                    name          = {};
    BasicType                                 basic_type    = BASIC_TYPE_UNKNOWN;
    Array<std::pair<String, SharedPtr<Type>>> fields        = {};
    u32                                       vector_size   = u32(1);
    u32                                       col_size      = u32(1);
    SharedPtr<Type>                           template_type = {};
    SharedPtr<Type>                           elem_type     = {};
    ResType                                   res_type      = RES_UNKNOWN;
    DimType                                   dim_type      = DIM_UNKNOWN;
    RWType                                    rw_type       = RW_UNKNOWN;
    u32                                       numeric_value = u32(0);
    bool                                      builtin       = true;
    u32                                       num_elems     = u32(1);

public:
    bool                                      IsBuiltin() { return builtin; }
    u32                                       GetNumElems() { return num_elems; }
    String                                    GetName() { return name; }
    BasicType                                 GetBasicTy() { return basic_type; }
    SharedPtr<Type>                           GetElemType() { return elem_type; }
    SharedPtr<Type>                           GetTemplateType() { return template_type; }
    ResType                                   GetResType() { return res_type; }
    DimType                                   GetDimType() { return dim_type; }
    u32                                       GetVectorSize() { return vector_size; }
    u32                                       GetColSize() { return col_size; }
    u32                                       GetNumericValue() { return numeric_value; }
    RWType                                    GetRWType() { return rw_type; }
    Array<std::pair<String, SharedPtr<Type>>> GetFields() { return fields; }

    SJIT_REFERENCE_COUNTER_IMPL;

    bool            IsArray() { return basic_type == BASIC_TYPE_ARRAY; }
    bool            IsStruct() { return basic_type == BASIC_TYPE_STRUCTURE; }
    bool            IsVector() { return IsBasicTypeScalar(basic_type) && vector_size >= u32(1) && vector_size <= u32(4) && col_size == u32(1); }
    bool            IsMatrix() { return IsBasicTypeScalar(basic_type) && vector_size >= u32(1) && vector_size <= u32(4) && col_size > u32(1); }
    SharedPtr<Type> FindField(String _name) {
        for (auto &f : fields)
            if (f.first == _name) return f.second;
        return {};
    }
    static SharedPtr<Type> CreateArray(String _name, SharedPtr<Type> _elem_type, u32 _num_elems) {
        SharedPtr<Type> o(new Type);
        o->name       = _name;
        o->basic_type = BASIC_TYPE_ARRAY;
        o->elem_type  = _elem_type;
        o->num_elems  = _num_elems;
        return o;
    }
    static SharedPtr<Type> Create(String _name, BasicType ty, u32 _vector_size = u32(1), u32 _col_size = u32(1)) {
        SharedPtr<Type> o(new Type);
        o->name        = _name;
        o->basic_type  = ty;
        o->vector_size = _vector_size;
        o->col_size    = _col_size;
        return o;
    }
    static SharedPtr<Type> Create(u32 num) {
        SharedPtr<Type> o(new Type);
        o->basic_type    = BASIC_TYPE_NUMBER;
        o->numeric_value = num;
        return o;
    }
    static SharedPtr<Type> Create(String _name, BasicType ty, SharedPtr<Type> _template_type, ResType _res_type, DimType _dim_type, RWType _rw_type) {
        SharedPtr<Type> o(new Type);
        o->name          = _name;
        o->basic_type    = ty;
        o->template_type = _template_type;
        o->res_type      = _res_type;
        o->dim_type      = _dim_type;
        o->rw_type       = _rw_type;
        return o;
    }
    static SharedPtr<Type> CreateStructuredBuffer(SharedPtr<Type> _template_type) {
        SharedPtr<Type> o(new Type);
        o->name          = "StructuredBuffer";
        o->basic_type    = BASIC_TYPE_RESOURCE;
        o->template_type = _template_type;
        o->res_type      = RES_BUFFER;
        o->dim_type      = DIM_UNKNOWN;
        o->rw_type       = RW_READ;
        return o;
    }
    static SharedPtr<Type> CreateRWStructuredBuffer(SharedPtr<Type> _template_type) {
        SharedPtr<Type> o(new Type);
        o->name          = "RWStructuredBuffer";
        o->basic_type    = BASIC_TYPE_RESOURCE;
        o->template_type = _template_type;
        o->res_type      = RES_BUFFER;
        o->dim_type      = DIM_UNKNOWN;
        o->rw_type       = RW_READ_WRITE;
        return o;
    }
    static SharedPtr<Type> Create(String _name, Array<std::pair<String, SharedPtr<Type>>> _fields, bool _builtin = false) {
        SharedPtr<Type> o(new Type);
        o->basic_type = BASIC_TYPE_STRUCTURE;
        o->name       = _name;
        o->fields     = _fields;
        o->builtin    = _builtin;
        return o;
    }
    void EmitHLSL(HLSLModule &hlsl_module) { EmitType(this, hlsl_module); }
};

static SharedPtr<Type> WildcardTy_0                       = Type::Create("Wildcard_0", BASIC_TYPE_WILDCARD);
static SharedPtr<Type> WildcardTy_1                       = Type::Create("Wildcard_1", BASIC_TYPE_WILDCARD);
static SharedPtr<Type> WildcardTy_2                       = Type::Create("Wildcard_2", BASIC_TYPE_WILDCARD);
static SharedPtr<Type> WildcardTy_3                       = Type::Create("Wildcard_3", BASIC_TYPE_WILDCARD);
static SharedPtr<Type> VoidTy                             = Type::Create("void", BASIC_TYPE_VOID);
static SharedPtr<Type> u1Ty                               = Type::Create("bool", BASIC_TYPE_U1);
static SharedPtr<Type> u1x2Ty                             = Type::Create("bool2", BASIC_TYPE_U1, u32(2));
static SharedPtr<Type> u1x3Ty                             = Type::Create("bool3", BASIC_TYPE_U1, u32(3));
static SharedPtr<Type> u1x4Ty                             = Type::Create("bool4", BASIC_TYPE_U1, u32(4));
static SharedPtr<Type> u8Ty                               = Type::Create("u8", BASIC_TYPE_U8);
static SharedPtr<Type> i32Ty                              = Type::Create("i32", BASIC_TYPE_I32);
static SharedPtr<Type> i32x2Ty                            = Type::Create("i32x2", BASIC_TYPE_I32, u32(2));
static SharedPtr<Type> i32x3Ty                            = Type::Create("i32x3", BASIC_TYPE_I32, u32(3));
static SharedPtr<Type> i32x4Ty                            = Type::Create("i32x4", BASIC_TYPE_I32, u32(4));
static SharedPtr<Type> u32Ty                              = Type::Create("u32", BASIC_TYPE_U32);
static SharedPtr<Type> u32x2Ty                            = Type::Create("u32x2", BASIC_TYPE_U32, u32(2));
static SharedPtr<Type> u32x3Ty                            = Type::Create("u32x3", BASIC_TYPE_U32, u32(3));
static SharedPtr<Type> u32x4Ty                            = Type::Create("u32x4", BASIC_TYPE_U32, u32(4));
static SharedPtr<Type> f32Ty                              = Type::Create("f32", BASIC_TYPE_F32);
static SharedPtr<Type> f32x2Ty                            = Type::Create("f32x2", BASIC_TYPE_F32, u32(2));
static SharedPtr<Type> f32x3Ty                            = Type::Create("f32x3", BASIC_TYPE_F32, u32(3));
static SharedPtr<Type> f32x4Ty                            = Type::Create("f32x4", BASIC_TYPE_F32, u32(4));
static SharedPtr<Type> f32x4x4Ty                          = Type::Create("f32x4x4", BASIC_TYPE_F32, u32(4), u32(4));
static SharedPtr<Type> f32x3x3Ty                          = Type::Create("f32x3x3", BASIC_TYPE_F32, u32(3), u32(3));
static SharedPtr<Type> f16Ty                              = Type::Create("f16", BASIC_TYPE_F16);
static SharedPtr<Type> f16x2Ty                            = Type::Create("f16x2", BASIC_TYPE_F16, u32(2));
static SharedPtr<Type> f16x3Ty                            = Type::Create("f16x3", BASIC_TYPE_F16, u32(3));
static SharedPtr<Type> f16x4Ty                            = Type::Create("f16x4", BASIC_TYPE_F16, u32(4));
static SharedPtr<Type> Texture2D_f16_Ty                   = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, f16Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_f16x2_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, f16x2Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_f16x3_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, f16x3Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_f16x4_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, f16x4Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_f32_Ty                   = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, f32Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_f32x2_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, f32x2Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_f32x3_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, f32x3Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_f32x4_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, f32x4Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_i32_Ty                   = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, i32Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_i32x2_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, i32x2Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_i32x3_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, i32x3Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_i32x4_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, i32x4Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_u32_Ty                   = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, u32Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_u32x2_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, u32x2Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_u32x3_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, u32x3Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> Texture2D_u32x4_Ty                 = Type::Create("Texture2D", BASIC_TYPE_RESOURCE, u32x4Ty, RES_TEXTURE, DIM_2D, RW_READ);
static SharedPtr<Type> RWTexture2D_f16_Ty                 = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, f16Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_f16x2_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, f16x2Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_f16x3_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, f16x3Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_f16x4_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, f16x4Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_f32_Ty                 = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, f32Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_f32x2_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, f32x2Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_f32x3_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, f32x3Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_f32x4_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, f32x4Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_i32_Ty                 = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, i32Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_i32x2_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, i32x2Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_i32x3_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, i32x3Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_i32x4_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, i32x4Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_u32_Ty                 = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, u32Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_u32x2_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, u32x2Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_u32x3_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, u32x3Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture2D_u32x4_Ty               = Type::Create("RWTexture2D", BASIC_TYPE_RESOURCE, u32x4Ty, RES_TEXTURE, DIM_2D, RW_READ_WRITE);
static SharedPtr<Type> Texture3D_f16_Ty                   = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, f16Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_f16x2_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, f16x2Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_f16x3_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, f16x3Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_f16x4_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, f16x4Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_f32_Ty                   = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, f32Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_f32x2_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, f32x2Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_f32x3_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, f32x3Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_f32x4_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, f32x4Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_i32_Ty                   = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, i32Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_i32x2_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, i32x2Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_i32x3_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, i32x3Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_i32x4_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, i32x4Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_u32_Ty                   = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, u32Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_u32x2_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, u32x2Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_u32x3_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, u32x3Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> Texture3D_u32x4_Ty                 = Type::Create("Texture3D", BASIC_TYPE_RESOURCE, u32x4Ty, RES_TEXTURE, DIM_3D, RW_READ);
static SharedPtr<Type> RWTexture3D_f16_Ty                 = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, f16Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_f16x2_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, f16x2Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_f16x3_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, f16x3Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_f16x4_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, f16x4Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_f32_Ty                 = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, f32Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_f32x2_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, f32x2Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_f32x3_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, f32x3Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_f32x4_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, f32x4Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_i32_Ty                 = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, i32Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_i32x2_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, i32x2Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_i32x3_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, i32x3Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_i32x4_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, i32x4Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_u32_Ty                 = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, u32Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_u32x2_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, u32x2Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_u32x3_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, u32x3Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWTexture3D_u32x4_Ty               = Type::Create("RWTexture3D", BASIC_TYPE_RESOURCE, u32x4Ty, RES_TEXTURE, DIM_3D, RW_READ_WRITE);
static SharedPtr<Type> RWStructuredBuffer_u32_Ty          = Type::Create("RWStructuredBuffer", BASIC_TYPE_RESOURCE, u32Ty, RES_BUFFER, DIM_UNKNOWN, RW_READ_WRITE);
static SharedPtr<Type> RaytracingAccelerationStructure_Ty = Type::Create("RaytracingAccelerationStructure", BASIC_TYPE_RESOURCE, {}, RES_TLAS, DIM_UNKNOWN, RW_UNKNOWN);
static SharedPtr<Type> SamplerState_Ty                    = Type::Create("SamplerState", BASIC_TYPE_RESOURCE, {}, RES_SAMPLER, DIM_UNKNOWN, RW_UNKNOWN);

static HashMap<BasicType, HashMap<u32, SharedPtr<Type>>> texture_2d_type_table = {
    {BASIC_TYPE_I32,
     {
         {u32(1), Texture2D_i32_Ty},   //
         {u32(2), Texture2D_i32x2_Ty}, //
         {u32(3), Texture2D_i32x3_Ty}, //
         {u32(4), Texture2D_i32x4_Ty}, //
     }},
    {BASIC_TYPE_U32,
     {
         {u32(1), Texture2D_u32_Ty},   //
         {u32(2), Texture2D_u32x2_Ty}, //
         {u32(3), Texture2D_u32x3_Ty}, //
         {u32(4), Texture2D_u32x4_Ty}, //
     }},
    {BASIC_TYPE_F32,
     {
         {u32(1), Texture2D_f32_Ty},   //
         {u32(2), Texture2D_f32x2_Ty}, //
         {u32(3), Texture2D_f32x3_Ty}, //
         {u32(4), Texture2D_f32x4_Ty}, //
     }},
    {BASIC_TYPE_F16,
     {
         {u32(1), Texture2D_f16_Ty},   //
         {u32(2), Texture2D_f16x2_Ty}, //
         {u32(3), Texture2D_f16x3_Ty}, //
         {u32(4), Texture2D_f16x4_Ty}, //
     }},
};
static HashMap<BasicType, HashMap<u32, SharedPtr<Type>>> rw_texture_2d_type_table = {
    {BASIC_TYPE_I32,
     {
         {u32(1), RWTexture2D_i32_Ty},   //
         {u32(2), RWTexture2D_i32x2_Ty}, //
         {u32(3), RWTexture2D_i32x3_Ty}, //
         {u32(4), RWTexture2D_i32x4_Ty}, //
     }},
    {BASIC_TYPE_U32,
     {
         {u32(1), RWTexture2D_u32_Ty},   //
         {u32(2), RWTexture2D_u32x2_Ty}, //
         {u32(3), RWTexture2D_u32x3_Ty}, //
         {u32(4), RWTexture2D_u32x4_Ty}, //
     }},
    {BASIC_TYPE_F32,
     {
         {u32(1), RWTexture2D_f32_Ty},   //
         {u32(2), RWTexture2D_f32x2_Ty}, //
         {u32(3), RWTexture2D_f32x3_Ty}, //
         {u32(4), RWTexture2D_f32x4_Ty}, //
     }},
    {BASIC_TYPE_F16,
     {
         {u32(1), RWTexture2D_f16_Ty},   //
         {u32(2), RWTexture2D_f16x2_Ty}, //
         {u32(3), RWTexture2D_f16x3_Ty}, //
         {u32(4), RWTexture2D_f16x4_Ty}, //
     }},
};
static HashMap<BasicType, HashMap<u32, SharedPtr<Type>>> texture_3d_type_table = {
    {BASIC_TYPE_I32,
     {
         {u32(1), Texture3D_i32_Ty},   //
         {u32(2), Texture3D_i32x2_Ty}, //
         {u32(3), Texture3D_i32x3_Ty}, //
         {u32(4), Texture3D_i32x4_Ty}, //
     }},
    {BASIC_TYPE_U32,
     {
         {u32(1), Texture3D_u32_Ty},   //
         {u32(2), Texture3D_u32x2_Ty}, //
         {u32(3), Texture3D_u32x3_Ty}, //
         {u32(4), Texture3D_u32x4_Ty}, //
     }},
    {BASIC_TYPE_F32,
     {
         {u32(1), Texture3D_f32_Ty},   //
         {u32(2), Texture3D_f32x2_Ty}, //
         {u32(3), Texture3D_f32x3_Ty}, //
         {u32(4), Texture3D_f32x4_Ty}, //
     }},
    {BASIC_TYPE_F16,
     {
         {u32(1), Texture3D_f16_Ty},   //
         {u32(2), Texture3D_f16x2_Ty}, //
         {u32(3), Texture3D_f16x3_Ty}, //
         {u32(4), Texture3D_f16x4_Ty}, //
     }},
};
static HashMap<BasicType, HashMap<u32, SharedPtr<Type>>> rw_texture_3d_type_table = {
    {BASIC_TYPE_I32,
     {
         {u32(1), RWTexture3D_i32_Ty},   //
         {u32(2), RWTexture3D_i32x2_Ty}, //
         {u32(3), RWTexture3D_i32x3_Ty}, //
         {u32(4), RWTexture3D_i32x4_Ty}, //
     }},
    {BASIC_TYPE_U32,
     {
         {u32(1), RWTexture3D_u32_Ty},   //
         {u32(2), RWTexture3D_u32x2_Ty}, //
         {u32(3), RWTexture3D_u32x3_Ty}, //
         {u32(4), RWTexture3D_u32x4_Ty}, //
     }},
    {BASIC_TYPE_F32,
     {
         {u32(1), RWTexture3D_f32_Ty},   //
         {u32(2), RWTexture3D_f32x2_Ty}, //
         {u32(3), RWTexture3D_f32x3_Ty}, //
         {u32(4), RWTexture3D_f32x4_Ty}, //
     }},
    {BASIC_TYPE_F16,
     {
         {u32(1), RWTexture3D_f16_Ty},   //
         {u32(2), RWTexture3D_f16x2_Ty}, //
         {u32(3), RWTexture3D_f16x3_Ty}, //
         {u32(4), RWTexture3D_f16x4_Ty}, //
     }},
};
static Array<SharedPtr<Type>> numeric_type_table = {
    Type::Create(0), //
    Type::Create(1), //
    Type::Create(2), //
    Type::Create(3), //
    Type::Create(4), //
    Type::Create(5), //
    Type::Create(6), //
    Type::Create(7), //
};
static HashMap<BasicType, HashMap<u32, SharedPtr<Type>>> vector_type_table = {
    {BASIC_TYPE_I32,
     {
         {u32(1), i32Ty},   //
         {u32(2), i32x2Ty}, //
         {u32(3), i32x3Ty}, //
         {u32(4), i32x4Ty}, //
     }},                    //
    {BASIC_TYPE_U32,
     {
         {u32(1), u32Ty},   //
         {u32(2), u32x2Ty}, //
         {u32(3), u32x3Ty}, //
         {u32(4), u32x4Ty}, //
     }},
    {BASIC_TYPE_F32,
     {
         {u32(1), f32Ty},   //
         {u32(2), f32x2Ty}, //
         {u32(3), f32x3Ty}, //
         {u32(4), f32x4Ty}, //
     }},
    {BASIC_TYPE_F16,
     {
         {u32(1), f16Ty},   //
         {u32(2), f16x2Ty}, //
         {u32(3), f16x3Ty}, //
         {u32(4), f16x4Ty}, //
     }},
    {BASIC_TYPE_U1,
     {
         {u32(1), u1Ty},   //
         {u32(2), u1x2Ty}, //
         {u32(3), u1x3Ty}, //
         {u32(4), u1x4Ty}, //
     }},
};

class Resource;
class Module {
private:
    HashMap<String, SharedPtr<Resource>> resources = {};

public:
    SJIT_REFERENCE_COUNTER_IMPL;

    static SharedPtr<Module> Create() {
        auto g = SharedPtr<Module>(new Module);
        return g;
    }

    void                                        AddResource(String const &name, SharedPtr<Resource> o) { resources[name] = o; }
    HashMap<String, SharedPtr<Resource>> const &GetResources() { return resources; }
};

// static SharedPtr<Module> g_current_module = {};

class Resource {
private:
    SharedPtr<Resource> elem_type  = {};
    SharedPtr<Type>     type       = {};
    String              name       = {};
    bool                is_array   = false;
    u32                 array_size = u32(-1);
    u32                 dxreg      = u32(-1);
    u32                 space      = u32(-1);
    char                letter     = char(0);

public:
    SJIT_REFERENCE_COUNTER_IMPL;

    static SharedPtr<Resource> Create( //
        SharedPtr<Type> _type,         //
        String          _name          //
    ) {
        SharedPtr<Resource> o(new Resource);
        o->name = _name;
        o->type = _type;
        return o;
    }
    static SharedPtr<Resource> CreateArray( //
        SharedPtr<Resource> _type,          //
        String              _name           //
    ) {
        SharedPtr<Resource> o(new Resource);
        o->name      = _name;
        o->type      = Type::CreateArray(_name, _type->GetType(), u32(-1));
        o->elem_type = _type;
        o->is_array  = true;
        return o;
    }
    u32                 GetArraySize() { return array_size; }
    u32                 GetDXReg() { return dxreg; };
    u32                 GetSpace() { return space; }
    char                GetLetter() { return letter; }
    bool                IsArray() { return is_array; }
    String const       &GetName() { return name; }
    SharedPtr<Type>     GetType() { return type; }
    SharedPtr<Resource> GetElemType() { return elem_type; }
};

class Expr;
class HLSLModule;

#    define SJIT_DONT_MOVE(class_name)                                                                                                                                             \
        class_name(class_name const &) = delete;                                                                                                                                   \
        class_name(class_name &&)      = delete;                                                                                                                                   \
        class_name const &operator=(class_name const &) = delete;                                                                                                                  \
        class_name const &operator=(class_name &&) = delete;

class SimpleWriter {
private:
    char *buf        = NULL;
    u64   buf_cursor = u64(0);

public:
    SJIT_DONT_MOVE(SimpleWriter);

    SimpleWriter() { buf = new char[1 << 20]; }
    ~SimpleWriter() { delete[] buf; }

    void EmitF(char const *fmt, ...) {
        va_list args;
        va_start(args, fmt);
#    define _CRT_SECURE_NO_WARNINGS
        buf_cursor += vsprintf(buf + buf_cursor, fmt, args);
#    undef _CRT_SECURE_NO_WARNINGS
        va_end(args);
    }
    void Reset() { buf_cursor = u64(0); }
    void Write(char const *str) {
        u64 len = strlen(str);
        memcpy(buf + buf_cursor, str, len);
        buf_cursor += len;
    }
    void        Putc(char const c) { buf[buf_cursor++] = c; }
    char const *Finalize() {
        buf[buf_cursor] = '\0';
        return buf;
    }
    SimpleWriter &operator<<(char const *_text) {
        Write(_text);
        return *this;
    }
    SimpleWriter &operator<<(f32 v) {
        EmitF("f32(%f)", v);
        return *this;
    }
    SimpleWriter &operator<<(f32x2 v) {
        EmitF("f32x2(%f, %f)", v.x, v.y);
        return *this;
    }
    SimpleWriter &operator<<(f32x3 v) {
        EmitF("f32x3(%f, %f, %f)", v.x, v.y, v.z);
        return *this;
    }
    SimpleWriter &operator<<(f32x4 v) {
        EmitF("f32x4(%f, %f, %f, %f)", v.x, v.y, v.z, v.w);
        return *this;
    }
    SimpleWriter &operator<<(u32 v) {
        EmitF("u32(%i)", v);
        return *this;
    }
    SimpleWriter &operator<<(u32x2 v) {
        EmitF("u32x2(%i, %i)", v.x, v.y);
        return *this;
        ;
    }
    SimpleWriter &operator<<(u32x4 v) {
        EmitF("u32x4(%i, %i, %i, %i)", v.x, v.y, v.z, v.w);
        return *this;
    }
    SimpleWriter &operator<<(i32 v) {
        EmitF("i32(%i)", v);
        return *this;
    }
    SimpleWriter &operator<<(i32x2 v) {
        EmitF("i32x2(%i, %i)", v.x, v.y);
        return *this;
    }
    SimpleWriter &operator<<(i32x3 v) {
        EmitF("i32x3(%i, %i, %i)", v.x, v.y, v.z);
        return *this;
    }
    SimpleWriter &operator<<(i32x4 v) {
        EmitF("i32x4(%i, %i, %i, %i)", v.x, v.y, v.z, v.w);
        return *this;
    }
};
class HLSLModule {
private:
    HashMap<String, SharedPtr<Resource>> resources = {};
    HashMap<String, SharedPtr<Type>>     types     = {};

    SimpleWriter header        = {};
    SimpleWriter function_body = {};
    SimpleWriter body          = {};
    SimpleWriter final_text    = {};

    bool is_finalized = false;

    u32x3 group_size = {u32(8), u32(8), u32(1)};

    Array<HashSet<u32>> emitted = {};

    Array<SimpleWriter *>  function_stack    = {};
    Array<SharedPtr<Expr>> wave32_mask_stack = {};
    Array<SharedPtr<Expr>> condition_stack   = {};

    HashMap<String, SharedPtr<Type>> lds = {};

    bool wave32_mask_mode = false;
    bool in_switch        = false;

public:
    SJIT_REFERENCE_COUNTER_IMPL;

    bool            IsWave32MaskMode() { return wave32_mask_mode; }
    void            SetWave32MaskMode(bool _mode = true) { wave32_mask_mode = _mode; }
    void            PushWave32Mask(SharedPtr<Expr> _mask) { wave32_mask_stack.push_back(_mask); }
    void            PopWave32Mask() { wave32_mask_stack.pop_back(); }
    SharedPtr<Expr> GetWave32Mask() {
        sjit_debug_assert(wave32_mask_stack.size() >= u64(0));
        return wave32_mask_stack.back();
    }
    HashMap<String, SharedPtr<Type>> const &GetLDS() { return lds; }
    void                                    AddLDS(String const &_name, SharedPtr<Type> _type) { lds[_name] = _type; }

    bool IsEmitted(u32 id) { return emitted.back().find(id) != emitted.back().end(); }
    void MarkEmitted(u32 id) { emitted.back().insert(id); }

    Array<SharedPtr<Expr>> const &GetConditionStack() { return condition_stack; }

    void EnterSwitchScope() { in_switch = true; }
    void ExitSwitchScope() { in_switch = false; }
    bool IsInSwitch() { return in_switch; }
    void EnterScope(SharedPtr<Expr> _cond = {}) {
        condition_stack.push_back(_cond);
        emitted.push_back(emitted.back());
    }
    void ExitScope() {
        condition_stack.pop_back();
        emitted.pop_back();
        sjit_assert(emitted.size());
    }
    void EnterFunction() { function_stack.push_back(new SimpleWriter); }
    void ExitFunction() {
        function_body.EmitF(function_stack.back()->Finalize());
        delete function_stack.back();
        function_stack.pop_back();
    }
    void AddType(SharedPtr<Type> o) {
        types[o->GetName()] = o;
        if (o->IsStruct()) {
            for (auto &f : o->GetFields()) {
                AddType(f.second);
            }
        } else if (o->GetTemplateType()) {
            AddType(o->GetTemplateType());
        }
    }
    void                                        AddResource(String const &name, SharedPtr<Resource> o) { resources[name] = o; }
    HashMap<String, SharedPtr<Resource>> const &GetResources() { return resources; }
    HashMap<String, SharedPtr<Type>> const     &GetTypes() { return types; }

    HLSLModule(HLSLModule const &) = delete;
    HLSLModule(HLSLModule &&)      = delete;
    HLSLModule const &operator=(HLSLModule const &) = delete;
    HLSLModule const &operator=(HLSLModule &&) = delete;
    HLSLModule() { emitted.push_back({}); }
    ~HLSLModule() { sjit_assert(function_stack.size() == u64(0)); }
    u32x3 GetGroupSize() { return group_size; }
    void  SetGroupSize(u32x3 const &_group_size) {
        group_size = _group_size;
        sjit_assert(group_size.x > u32(0) && group_size.y > u32(0) && group_size.z > u32(0));
        sjit_assert(((group_size.x * group_size.y * group_size.z) % u32(32)) == u32(0));
    }
    SimpleWriter &GetHeader() { return header; }
    SimpleWriter &GetBody() {
        if (function_stack.size()) return *function_stack.back();
        return body;
    }
    char const *Finalize(bool _emit_resources = true) {
        // if (is_finalized) return final_text.Finalize();
        final_text.Reset();
        final_text.Write(R"(
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

#define MAKE__get_dimensions(T)                                  \
u32x2 __get_dimensions(Texture2D<T> tex) {                       \
    u32x2 dims;                                                  \
    tex.GetDimensions(/* out */ dims.x, /* out */ dims.y);       \
    return dims;                                                 \
}                                                                \
u32x2 __get_dimensions(RWTexture2D<T> tex) {                     \
    u32x2 dims;                                                  \
    tex.GetDimensions(/* out */ dims.x, /* out */ dims.y);       \
    return dims;                                                 \
}                                                                \

MAKE__get_dimensions(f32);
MAKE__get_dimensions(f32x2);
MAKE__get_dimensions(f32x3);
MAKE__get_dimensions(f32x4);
MAKE__get_dimensions(f16);
MAKE__get_dimensions(f16x2);
MAKE__get_dimensions(f16x3);
MAKE__get_dimensions(f16x4);
MAKE__get_dimensions(u32);
MAKE__get_dimensions(u32x2);
MAKE__get_dimensions(u32x3);
MAKE__get_dimensions(u32x4);

u32 __get_lane_bit() {
    return u32(1) << u32(WaveGetLaneIndex());
}
bool __anyhit(RaytracingAccelerationStructure tlas, RayDesc ray_desc) {
    RayQuery<RAY_FLAG_CULL_NON_OPAQUE> ray_query;
    ray_query.TraceRayInline(tlas, RAY_FLAG_NONE, 0xffu, ray_desc);
    while (ray_query.Proceed()) {
        break;
    }
    if (ray_query.CommittedStatus() == COMMITTED_NOTHING) return false;
    return true;
}
struct RayQueryWrapper {
    bool hit;
    f32 ray_t;
    f32x2 bary;
    u32 primitive_idx;
    u32 instance_id;
};
RayQueryWrapper __ray_query(RaytracingAccelerationStructure tlas, RayDesc ray_desc) {
    RayQueryWrapper w = (RayQueryWrapper)0;
    RayQuery<RAY_FLAG_CULL_NON_OPAQUE> ray_query;
    ray_query.TraceRayInline(tlas, RAY_FLAG_NONE, 0xffu, ray_desc);
    while (ray_query.Proceed()) {
        break;
    }
    if (ray_query.CommittedStatus() == COMMITTED_NOTHING) return w;
    w.hit = true;
    w.bary = ray_query.CommittedTriangleBarycentrics();
    w.ray_t = ray_query.CommittedRayT(); 
    w.instance_id = ray_query.CommittedInstanceID();
    w.primitive_idx = ray_query.CommittedPrimitiveIndex();
    return w;
}

f32x2 __interpolate(f32x2 v0, f32x2 v1, f32x2 v2, f32x2 barys) { return v0 * (f32(1.0) - barys.x - barys.y) + v1 * barys.x + v2 * barys.y; }
f32x3 __interpolate(f32x3 v0, f32x3 v1, f32x3 v2, f32x2 barys) { return v0 * (f32(1.0) - barys.x - barys.y) + v1 * barys.x + v2 * barys.y; }
f32x4 __interpolate(f32x4 v0, f32x4 v1, f32x4 v2, f32x2 barys) { return v0 * (f32(1.0) - barys.x - barys.y) + v1 * barys.x + v2 * barys.y; }
f32x3x3 __get_tbn(f32x3 N) {
    f32x3 U = f32x3(0.0, 0.0, 0.0);
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
)");

        for (auto &t : GetTypes()) {
            if (t.second->IsStruct() && t.second->IsBuiltin() == false) {
                SharedPtr<Type> ty = t.second;
                final_text.EmitF("struct %s {\n", ty->GetName().c_str());
                for (auto &f : ty->GetFields()) {
                    final_text.EmitF("%s %s;\n", f.second->GetName().c_str(), f.first.c_str());
                }
                final_text.EmitF("};\n");
            }
        }
        for (auto &l : GetLDS()) {
            if (l.second->IsArray()) {
                final_text.EmitF("groupshared %s %s[%i];\n", l.second->GetElemType()->GetName().c_str(), l.first.c_str(), l.second->GetNumElems());
            } else {
                final_text.EmitF("groupshared %s %s;\n", l.second->GetName().c_str(), l.first.c_str());
            }
        }
        if (_emit_resources) {
            u32 array_space = u32(99);
            for (auto &r : GetResources()) {
                if (r.second->GetType()->GetBasicTy() == BASIC_TYPE_ARRAY) {
                    if (r.second->GetType()->GetElemType()->GetResType() == RES_TEXTURE) {
                        if (r.second->IsArray()) {
                            final_text.EmitF("%s<", r.second->GetType()->GetElemType()->GetName().c_str());
                            final_text.EmitF("%s> ", r.second->GetType()->GetElemType()->GetTemplateType()->GetName().c_str());
                            if (r.second->GetArraySize() == u32(-1))
                                final_text.EmitF("%s[] : register(space%i);\n", r.first.c_str(), array_space);
                            else {
                                final_text.EmitF("%s[%i] ", r.first.c_str(), r.second->GetArraySize());
                                if (r.second->GetSpace() != u32(-1) || (r.second->GetDXReg() != u32(-1) && r.second->GetLetter() != char(0))) {
                                    final_text.EmitF("register(");
                                    bool has_letter = false;
                                    if (r.second->GetDXReg() != u32(-1) && r.second->GetLetter() != char(0)) {
                                        final_text.EmitF("%c%i", r.second->GetLetter(), r.second->GetSpace());
                                        has_letter = true;
                                    }
                                    if (r.second->GetSpace() != u32(-1)) {
                                        if (has_letter) final_text.EmitF(", ");
                                        final_text.EmitF("space%i", r.second->GetSpace());
                                    }
                                }
                                final_text.EmitF("\n");
                            }
                            array_space++;
                        } else {
                            SJIT_UNIMPLEMENTED;
                        }
                    } else {
                        SJIT_UNIMPLEMENTED;
                    }
                } else if (r.second->GetType()->GetBasicTy() == BASIC_TYPE_RESOURCE) {
                    if (r.second->GetType()->GetResType() == RES_TEXTURE) {
                        if (r.second->IsArray()) {
                            SJIT_UNIMPLEMENTED;
                        } else {
                            final_text.EmitF("%s<", r.second->GetType()->GetName().c_str());
                            final_text.EmitF("%s> ", r.second->GetType()->GetTemplateType()->GetName().c_str());
                            final_text.EmitF("%s;\n", r.first.c_str());
                        }
                    } else if (r.second->GetType()->GetResType() == RES_SAMPLER) {
                        sjit_assert(r.second->IsArray() == false);
                        final_text.EmitF("%s ", r.second->GetType()->GetName().c_str());
                        final_text.EmitF("%s;\n", r.first.c_str());
                    } else if (r.second->GetType()->GetResType() == RES_BUFFER) {
                        sjit_assert(r.second->IsArray() == false);
                        final_text.EmitF("%s<", r.second->GetType()->GetName().c_str());
                        final_text.EmitF("%s> ", r.second->GetType()->GetTemplateType()->GetName().c_str());
                        final_text.EmitF("%s;\n", r.first.c_str());
                    } else if (r.second->GetType()->GetResType() == RES_TLAS) {
                        sjit_assert(r.second->IsArray() == false);
                        final_text.EmitF("%s ", r.second->GetType()->GetName().c_str());
                        final_text.EmitF("%s;\n", r.first.c_str());
                    } else {
                        SJIT_UNIMPLEMENTED;
                    }
                } else if (r.second->GetType()->GetBasicTy() == BASIC_TYPE_U32 || //
                           r.second->GetType()->GetBasicTy() == BASIC_TYPE_F32) {
                    sjit_assert(r.second->IsArray() == false);
                    final_text.EmitF("%s ", r.second->GetType()->GetName().c_str());
                    final_text.EmitF("%s;\n", r.first.c_str());
                } else {
                    SJIT_UNIMPLEMENTED;
                }
            }
        } else {
            final_text.EmitF("RESOURCE_STAB\n");
        }
        final_text.Write(header.Finalize());
        final_text.Write(function_body.Finalize());
        final_text.EmitF("[numthreads(%i, %i, %i)] void main(u32x3 __tid : SV_DispatchThreadID, u32x3 __gid : SV_GroupThreadID, u32x3 __group_id : SV_GroupID) \n", group_size.x,
                         group_size.y, group_size.z);
        final_text.Write("{\n");
        final_text.Write(body.Finalize());
        final_text.Write("}\n");

        is_finalized = true;
        return final_text.Finalize();
    }
    template <typename T, typename... V>
    void Emit(T first, V... rest) {
        first->EmitHLSL(*this);
        Emit(rest...);
    }
    template <typename T>
    void Emit(T first) {
        first->EmitHLSL(*this);
    }
};
class FnPrototype;
static void EmitFunctionCall(FnPrototype *fn, HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv);
static void EmitFunctionDefinition(FnPrototype *fn, HLSLModule &hlsl_module, Array<SharedPtr<Type>> const &argv = {});
enum FN_ARG_MODE {
    FN_ARG_IN,
    FN_ARG_INOUT,
};
struct FnPrototypeArg {
    String          name;
    SharedPtr<Type> type;
    FN_ARG_MODE     inout;
};
class FnPrototype {
private:
    String                                                            name              = {};
    SharedPtr<Type>                                                   ret_type          = {};
    Array<FnPrototypeArg>                                             argv              = {};
    std::function<SharedPtr<Type>(Array<SharedPtr<Type>> const &)>    ret_type_infer_fn = {};
    std::function<void(HLSLModule &, Array<SharedPtr<Expr>> const &)> emit_fn           = {};
    bool                                                              non_scalar        = false;

public:
    SJIT_REFERENCE_COUNTER_IMPL;

    bool            IsNonScalar() { return non_scalar; }
    String          GetName() { return name; }
    SharedPtr<Type> GetReturnTy(Array<SharedPtr<Type>> const &argv) {
        if (ret_type_infer_fn) return ret_type_infer_fn(argv);
        return ret_type;
    }
    Array<FnPrototypeArg> const  &GetArgv() { return argv; }
    static SharedPtr<FnPrototype> Create(String const &_name, SharedPtr<Type> _ret_type, Array<FnPrototypeArg> const &_argv = {},
                                         std::function<SharedPtr<Type>(Array<SharedPtr<Type>> const &)>    _ret_type_infer_fn = {},
                                         std::function<void(HLSLModule &, Array<SharedPtr<Expr>> const &)> _emit_fn = {}, bool _non_scalar = false) {
        FnPrototype *o       = new FnPrototype;
        o->name              = _name;
        o->ret_type          = _ret_type;
        o->argv              = _argv;
        o->ret_type_infer_fn = _ret_type_infer_fn;
        o->emit_fn           = _emit_fn;
        o->non_scalar        = _non_scalar;
        return SharedPtr<FnPrototype>(o);
    }
    void EmitDefinition(HLSLModule &hlsl_module, Array<SharedPtr<Type>> const &argv = {}) { EmitFunctionDefinition(this, hlsl_module, argv); }
    void EmitCall(HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
        if (emit_fn)
            emit_fn(hlsl_module, argv);
        else {
            EmitFunctionCall(this, hlsl_module, argv);
        }
    }
    void Dump() {
        fprintf(stdout, "%s %s(", ret_type->GetName().c_str(), name.c_str());
        ifor(argv.size()) {
            if (u64(i) == argv.size() - u64(1))
                fprintf(stdout, "%s %s %s", (argv[i].inout == FN_ARG_INOUT ? "inout" : "in"), argv[i].type->GetName().c_str(), argv[i].name.c_str());
            else
                fprintf(stdout, "%s %s %s, ", (argv[i].inout == FN_ARG_INOUT ? "inout" : "in"), argv[i].type->GetName().c_str(), argv[i].name.c_str());
        }
        fprintf(stdout, ")");
    }
};

class IEmittable {
public:
    SJIT_REFERENCE_COUNTER_IMPL;

    virtual void EmitHLSL(HLSLModule &) = 0;
    virtual ~IEmittable() {}
};

class Block : public IEmittable {
private:
    SharedPtr<Block>             parent = {};
    Array<SharedPtr<IEmittable>> list   = {};

    Block(SharedPtr<Block> _parent) : parent(_parent) {}

public:
    static SharedPtr<Block> Create(SharedPtr<Block> parent) {
        auto g = SharedPtr<Block>(new Block(parent));
        if (parent) parent->AddEmittalbe(g);
        return g;
    }
    SharedPtr<Block>                    GetParent() { return parent; }
    void                                AddEmittalbe(SharedPtr<IEmittable> e) { list.push_back(e); }
    Array<SharedPtr<IEmittable>> const &GetList() { return list; }
    void                                EmitHLSL(HLSLModule &hlsl_module) override {
        hlsl_module.GetBody().EmitF("{\n");
        for (auto &l : list) l->EmitHLSL(hlsl_module);
        hlsl_module.GetBody().EmitF("}\n");
    }
};

#    if 0
static SharedPtr<Block> g_current_block = {};

#        define DSL_BLOCK_SCOPE                                                                                                                                                    \
            g_current_block = Block::Create(g_current_block);                                                                                                                      \
            defer(g_current_block = g_current_block->GetParent());

#        define DSL_MODULE_SCOPE                                                                                                                                                   \
            auto tmp__##__LINE__ = g_current_module;                                                                                                                               \
            g_current_module     = Module::Create();                                                                                                                               \
            defer(g_current_module = tmp__##__LINE__);
#    endif // 0

enum ScalarMode {
    SCALAR_MODE_UNKNOWN = 0,
    SCALAR_MODE_SCALAR,
    SCALAR_MODE_NON_SCALAR,
};

class Expr : public IEmittable {
public:
    u32 id = u32(-1);
    struct Literal {
        union {
            f32   f;
            f32x2 fx2;
            f32x3 fx3;
            f32x4 fx4;
            f16   h;
            f16x2 hx2;
            f16x3 hx3;
            f16x4 hx4;
            i32   i;
            i32x2 ix2;
            i32x3 ix3;
            i32x4 ix4;
            u32   u;
            u32x2 ux2;
            u32x3 ux3;
            u32x4 ux4;
        };
    };
    Literal         lit      = {};
    SharedPtr<Type> lit_type = {};
    OpType          op_type  = OP_UNKNOWN;
    InType          in_type  = IN_TYPE_UNKNOWN;

    SharedPtr<Resource> resource = {};
    SharedPtr<Expr>     lhs      = NULL;
    SharedPtr<Expr>     rhs      = NULL;
    SharedPtr<Expr>     cond     = NULL;
    SharedPtr<Expr>     index    = NULL;
    // Array<SharedPtr<Expr>> elems         = {};
    u32             index_literal = u32(0);
    EXPRESSION_TYPE type          = EXPRESSION_TYPE_UNKNOWN;

    u32 array_size = u32(0);

    SharedPtr<FnPrototype> fn_prototype = {};

    SharedPtr<Type> inferred_type = {};

    Array<SharedPtr<Expr>> argv     = {};
    u32                    argv_num = u32(0);

    bool ref = false; // Emit an expression directly instead of a tmp variable

    char swizzle[5]   = {'\0', '\0', '\0', '\0', '\0'};
    u32  swizzle_size = u32(0);

    char name[0x100]       = {};
    char field_name[0x100] = {};
    char input_name[0x100] = {};

    ScalarMode scalar_mode = SCALAR_MODE_UNKNOWN;

public:
    SharedPtr<Resource> GetResource() { return resource; }
    SharedPtr<Expr>     GetLHS() { return lhs; }
    SharedPtr<Expr>     GetRHS() { return rhs; }
    SharedPtr<Expr>     GetCond() { return cond; }
    SharedPtr<Expr>     GetIndex() { return index; }

    Array<SharedPtr<Expr>> GetDeps() {
        Array<SharedPtr<Expr>> deps = argv;
        if (lhs) deps.push_back(lhs);
        if (rhs) deps.push_back(rhs);
        if (cond) deps.push_back(cond);
        return deps;
    }
    static SharedPtr<Expr> MakeArray(SharedPtr<Type> _elem_type, u32 _array_size) {
        SharedPtr<Expr> o = Create();
        // o->elems                  = _elems;
        o->array_size = _array_size;
        o->type       = EXPRESSION_TYPE_ARRAY;
        /*SharedPtr<Type> elem_type = _elems[0]->InferType();
        for (auto &e : _elems) {
            sjit_assert(_elems[i]->InferType() == elem_type);
        }*/
        o->inferred_type = Type::CreateArray(o->name, _elem_type, _array_size);
        return o;
    }
    static SharedPtr<Expr> MakeOp(SharedPtr<Expr> _lhs, SharedPtr<Expr> _rhs, OpType _op) {
        SharedPtr<Expr> o = Create();
        o->lhs            = _lhs;
        o->rhs            = _rhs;
        o->type           = EXPRESSION_TYPE_OP;
        o->op_type        = _op;
        o->InferType();
        return o;
    }
    static SharedPtr<Expr> MakeInput(InType _in_type) {
        SharedPtr<Expr> o = Create();
        o->type           = EXPRESSION_TYPE_INPUT;
        o->in_type        = _in_type;
        o->InferType();
        return o;
    }
    static SharedPtr<Expr> MakeInput(char const *_name, SharedPtr<Type> _type) {
        SharedPtr<Expr> o = Create();
        o->type           = EXPRESSION_TYPE_INPUT;
        o->in_type        = IN_TYPE_CUSTOM;
        sprintf_s(o->input_name, _name);
        o->inferred_type = _type;
        return o;
    }
    static SharedPtr<Expr> MakeLiteral(i32 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = i32Ty;
        expr->lit.i          = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(i32x2 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = i32x2Ty;
        expr->lit.ix2        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(i32x3 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = i32x3Ty;
        expr->lit.ix3        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(i32x4 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = i32x4Ty;
        expr->lit.ix4        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(u32 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = u32Ty;
        expr->lit.u          = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(u32x2 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = u32x2Ty;
        expr->lit.ux2        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(u32x3 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = u32x3Ty;
        expr->lit.ux3        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(u32x4 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = u32x4Ty;
        expr->lit.ux4        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(f32 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = f32Ty;
        expr->lit.f          = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(f32x2 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = f32x2Ty;
        expr->lit.fx2        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(f32x3 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = f32x3Ty;
        expr->lit.fx3        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(f32x4 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = f32x4Ty;
        expr->lit.fx4        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(f16 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = f16Ty;
        expr->lit.h          = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(f16x2 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = f16x2Ty;
        expr->lit.hx2        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(f16x3 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = f16x3Ty;
        expr->lit.hx3        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeLiteral(f16x4 v) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type           = EXPRESSION_TYPE_LITERAL;
        expr->lit_type       = f16x4Ty;
        expr->lit.hx4        = v;
        return expr;
    }
    static SharedPtr<Expr> MakeIfElse(SharedPtr<Expr> _cond, SharedPtr<Expr> _lhs, SharedPtr<Expr> _rhs) {
        SharedPtr<Expr> expr = Expr::Create();
        expr->cond           = _cond;
        expr->lhs            = _lhs;
        expr->rhs            = _rhs;
        expr->type           = EXPRESSION_TYPE_IF_ELSE;
        return expr;
    }

#    if 0
    static SharedPtr<Expr> MakeElse() {
        SharedPtr<Expr> expr = Expr::Create();
        expr->type = EXPRESSION_TYPE_ELSE;
        return expr;
    }
#    endif // 0

    static SharedPtr<Expr> Create(EXPRESSION_TYPE _ty = EXPRESSION_TYPE_UNKNOWN) {
        static u32      counter = u32(0);
        SharedPtr<Expr> e       = SharedPtr<Expr>(new Expr);
        e->type                 = _ty;
        e->id                   = counter++;
        sprintf_s(e->name, "tmp_%i", e->id);
        // sjit_assert(g_current_block);
        // g_current_block->AddEmittalbe(e);

        return e;
    }
    static SharedPtr<Expr> MakeRef(String const &_name, SharedPtr<Type> _type) {
        SharedPtr<Expr> expr = Create();
        expr->type           = EXPRESSION_TYPE_REF;
        expr->inferred_type  = _type;
        sprintf_s(expr->name, _name.c_str());
        return expr;
    }
    static SharedPtr<Expr> MakeResource(SharedPtr<Resource> _resource) {
        SharedPtr<Expr> expr = Create();
        expr->type           = EXPRESSION_TYPE_RESOURCE;
        expr->resource       = _resource;

        return expr;
    }
    static SharedPtr<Expr> MakeIndex(SharedPtr<Expr> src_expr, u32 index) {
        sjit_assert(src_expr->InferType()->GetBasicTy() == BASIC_TYPE_RESOURCE || src_expr->InferType()->IsVector() || src_expr->InferType()->IsMatrix() ||
                    src_expr->InferType()->IsArray());
        SharedPtr<Expr> expr = Create();
        expr->type           = EXPRESSION_TYPE_INDEX;
        expr->lhs            = src_expr;
        expr->index_literal  = index;
        expr->ref            = true;
        if (src_expr->InferType()->GetBasicTy() == BASIC_TYPE_RESOURCE) {
            if (src_expr->GetResource()->IsArray()) {
                expr->type          = EXPRESSION_TYPE_RESOURCE;
                expr->resource      = src_expr->GetResource()->GetElemType();
                expr->inferred_type = expr->resource->GetType();
                expr->ref           = true;
            } else {
                expr->ref           = false;
                expr->inferred_type = src_expr->InferType()->GetTemplateType();
            }
        } else if (src_expr->InferType()->IsArray())
            expr->inferred_type = src_expr->InferType()->GetElemType();
        else if (src_expr->InferType()->IsVector())
            expr->inferred_type = vector_type_table[src_expr->InferType()->GetBasicTy()][u32(1)];
        else if (src_expr->InferType()->IsMatrix())
            expr->inferred_type = vector_type_table[src_expr->InferType()->GetBasicTy()][src_expr->InferType()->GetVectorSize()];
        else {
            SJIT_UNIMPLEMENTED;
        }

        return expr;
    }
    static SharedPtr<Expr> MakeIndex(SharedPtr<Expr> src_expr, SharedPtr<Expr> index) {
        sjit_assert(src_expr->InferType()->GetBasicTy() == BASIC_TYPE_RESOURCE || src_expr->InferType()->GetBasicTy() == BASIC_TYPE_ARRAY);
        SharedPtr<Expr> expr = Create();
        expr->type           = EXPRESSION_TYPE_INDEX;
        expr->lhs            = src_expr;
        if (src_expr->InferType()->GetBasicTy() == BASIC_TYPE_RESOURCE) {
            if (src_expr->GetResource()->IsArray()) {
                expr->type     = EXPRESSION_TYPE_RESOURCE;
                expr->resource = src_expr->GetResource()->GetElemType();
                expr->ref      = true;
            } else {
                expr->ref           = false;
                expr->inferred_type = src_expr->InferType()->GetTemplateType();
            }
        } else {
            expr->ref           = true;
            expr->inferred_type = src_expr->InferType()->GetElemType();
        }
        expr->index = index;
        return expr;
    }
    static SharedPtr<Expr> MakeField(SharedPtr<Expr> src_expr, char const *_field) {
        sjit_assert(src_expr->InferType()->GetBasicTy() == BASIC_TYPE_STRUCTURE);
        SharedPtr<Type> field_ty = src_expr->InferType()->FindField(_field);
        sjit_assert(field_ty);
        SharedPtr<Expr> expr = Create();
        expr->type           = EXPRESSION_TYPE_FIELD;
        expr->lhs            = src_expr;
        expr->inferred_type  = field_ty;
        sprintf_s(expr->field_name, _field);
        return expr;
    }
    static SharedPtr<Expr> MakeSwizzle(SharedPtr<Expr> _expr, char const *_swizzle) {
        SharedPtr<Expr> expr = Create();
        expr->type           = EXPRESSION_TYPE_SWIZZLE;
        expr->lhs            = _expr;
        u32 max_component    = u32(0);
        ifor(4) {
            if (_swizzle[i] == '\0') break;
            sjit_assert(_swizzle[i] == 'x' || _swizzle[i] == 'y' || _swizzle[i] == 'z' || _swizzle[i] == 'w');
            expr->swizzle[i] = _swizzle[i];
            if (_swizzle[i] == 'x')
                max_component = std::max(max_component, u32(0));
            else if (_swizzle[i] == 'y')
                max_component = std::max(max_component, u32(1));
            else if (_swizzle[i] == 'z')
                max_component = std::max(max_component, u32(2));
            else if (_swizzle[i] == 'w')
                max_component = std::max(max_component, u32(3));
            else {
                SJIT_TRAP;
            }
            expr->swizzle_size++;
        }
        sjit_assert(max_component < _expr->InferType()->GetVectorSize());
        sprintf_s(expr->name, "%s.%s", expr->lhs->name, expr->swizzle);
        expr->InferType();
        return expr;
    }
    static SharedPtr<Expr> MakeFunction(SharedPtr<FnPrototype> _fn_prototype, SharedPtr<Expr> *_argv, u32 _argv_num) {
        SharedPtr<Expr> expr = Create();
        if (_argv_num > u32(0)) {
            expr->argv.resize(_argv_num);
            expr->argv_num = _argv_num;
            ifor(_argv_num) { expr->argv[i] = _argv[i]; }
        }
        expr->type         = EXPRESSION_TYPE_FUNCTION;
        expr->fn_prototype = _fn_prototype;
        expr->InferType();
        return expr;
    }
    Expr *SetName(char const *_name) {
        sprintf_s(name, _name);
        return this;
    }
    void EmitHLSLName(HLSLModule &hlsl_module) {
        if (type == EXPRESSION_TYPE_LITERAL) {
            hlsl_module.GetBody().EmitF("%s %s = ", InferType()->GetName().c_str(), name);
            if (lit_type == f32Ty) {
                hlsl_module.GetBody().EmitF("f32(%f)", lit.f);
            } else if (lit_type == f32x2Ty) {
                hlsl_module.GetBody().EmitF("f32x2(%f, %f)", lit.fx2.x, lit.fx2.y);
            } else if (lit_type == f32x3Ty) {
                hlsl_module.GetBody().EmitF("f32x3(%f, %f, %f)", lit.fx3.x, lit.fx3.y, lit.fx3.z);
            } else if (lit_type == f32x4Ty) {
                hlsl_module.GetBody().EmitF("f32x4(%f, %f, %f, %f)", lit.fx4.x, lit.fx4.y, lit.fx4.z, lit.fx4.w);
            } else if (lit_type == i32Ty) {
                hlsl_module.GetBody().EmitF("i32(%i)", lit.i);
            } else if (lit_type == i32x2Ty) {
                hlsl_module.GetBody().EmitF("i32x2(%i, %i)", lit.ix2.x, lit.ix2.y);
            } else if (lit_type == i32x3Ty) {
                hlsl_module.GetBody().EmitF("i32x3(%i, %i, %i)", lit.ix3.x, lit.ix3.y, lit.ix3.z);
            } else if (lit_type == i32x4Ty) {
                hlsl_module.GetBody().EmitF("i32x4(%i, %i, %i, %i)", lit.ix4.x, lit.ix4.y, lit.ix4.z, lit.ix4.w);
            } else if (lit_type == u32Ty) {
                hlsl_module.GetBody().EmitF("u32(%i)", lit.i);
            } else if (lit_type == u32x2Ty) {
                hlsl_module.GetBody().EmitF("u32x2(%i, %i)", lit.ix2.x, lit.ix2.y);
            } else if (lit_type == u32x3Ty) {
                hlsl_module.GetBody().EmitF("u32x3(%i, %i, %i)", lit.ix3.x, lit.ix3.y, lit.ix3.z);
            } else if (lit_type == u32x4Ty) {
                hlsl_module.GetBody().EmitF("u32x4(%i, %i, %i, %i)", lit.ix4.x, lit.ix4.y, lit.ix4.z, lit.ix4.w);
            } else {
                SJIT_UNIMPLEMENTED;
            }
        } else {
            hlsl_module.GetBody().EmitF("%s", name);
        }
    }
    void EmitHLSL(HLSLModule &hlsl_module) override {
        if (hlsl_module.IsEmitted(id)) return;
        hlsl_module.MarkEmitted(id);

        if (lhs) lhs->EmitHLSL(hlsl_module);
        if (rhs) rhs->EmitHLSL(hlsl_module);
        if (cond) cond->EmitHLSL(hlsl_module);
        if (index) index->EmitHLSL(hlsl_module);
        for (auto &a : argv) a->EmitHLSL(hlsl_module);

        auto &hlsl = hlsl_module.GetBody();

        if (type == EXPRESSION_TYPE_OP) {

            if (op_type == OP_PLUS_ASSIGN) {
                hlsl.EmitF("%s += %s;\n", lhs->name, rhs->name);
                sprintf_s(name, lhs->name);
            } else if (op_type == OP_MINUS_ASSIGN) {
                hlsl.EmitF("%s -= %s;\n", lhs->name, rhs->name);
                sprintf_s(name, lhs->name);
            } else if (op_type == OP_BIT_OR_ASSIGN) {
                hlsl.EmitF("%s |= %s;\n", lhs->name, rhs->name);
                sprintf_s(name, lhs->name);
            } else if (op_type == OP_BIT_XOR_ASSIGN) {
                hlsl.EmitF("%s ^= %s;\n", lhs->name, rhs->name);
                sprintf_s(name, lhs->name);
            } else if (op_type == OP_BIT_AND_ASSIGN) {
                hlsl.EmitF("%s &= %s;\n", lhs->name, rhs->name);
                sprintf_s(name, lhs->name);
            } else if (op_type == OP_MUL_ASSIGN) {
                hlsl.EmitF("%s *= %s;\n", lhs->name, rhs->name);
                sprintf_s(name, lhs->name);
            } else if (op_type == OP_DIV_ASSIGN) {
                hlsl.EmitF("%s /= %s;\n", lhs->name, rhs->name);
                sprintf_s(name, lhs->name);
            } else if (op_type == OP_ASSIGN) {
                if (lhs) {
                    hlsl.EmitF("%s = %s;\n", lhs->name, rhs->name);
                    sprintf_s(name, lhs->name);
                } else {
                    hlsl.EmitF("%s %s = %s;\n", InferType()->GetName().c_str(), name, rhs->name);
                }
            } else {

                hlsl.EmitF("%s %s = ", InferType()->GetName().c_str(), name);
                if (lhs) hlsl.EmitF("%s", lhs->name);
                switch (op_type) {
                case OP_DIV: hlsl.EmitF("/"); break;
                case OP_MUL: hlsl.EmitF("*"); break;
                case OP_PLUS: hlsl.EmitF("+"); break;
                case OP_MINUS: hlsl.EmitF("-"); break;
                case OP_LESS: hlsl.EmitF("<"); break;
                case OP_LESS_OR_EQUAL: hlsl.EmitF("<="); break;
                case OP_GREATER: hlsl.EmitF(">"); break;
                case OP_LOGICAL_AND: hlsl.EmitF("&&"); break;
                case OP_BIT_AND: hlsl.EmitF("&"); break;
                case OP_BIT_OR: hlsl.EmitF("|"); break;
                case OP_BIT_XOR: hlsl.EmitF("^"); break;
                case OP_BIT_NEG: hlsl.EmitF("~"); break;
                case OP_SHIFT_LEFT: hlsl.EmitF("<<"); break;
                case OP_SHIFT_RIGHT: hlsl.EmitF(">>"); break;
                case OP_LOGICAL_OR: hlsl.EmitF("||"); break;
                case OP_LOGICAL_NOT: hlsl.EmitF("!"); break;
                case OP_GREATER_OR_EQUAL: hlsl.EmitF(">="); break;
                case OP_EQUAL: hlsl.EmitF("=="); break;
                case OP_MODULO: hlsl.Putc('%'); break;
                case OP_NOT_EQUAL: hlsl.EmitF("!="); break;
                default: SJIT_UNIMPLEMENTED;
                }
                if (rhs) hlsl.EmitF("%s", rhs->name);
                hlsl.EmitF(";\n");
            }
        } else if (type == EXPRESSION_TYPE_LITERAL) {
            if (lit_type == f32Ty) {
                sprintf_s(name, "f32(%f)", lit.f);
            } else if (lit_type == f32x2Ty) {
                sprintf_s(name, "f32x2(%f, %f)", lit.fx2.x, lit.fx2.y);
            } else if (lit_type == f32x3Ty) {
                sprintf_s(name, "f32x3(%f, %f, %f)", lit.fx3.x, lit.fx3.y, lit.fx3.z);
            } else if (lit_type == f32x4Ty) {
                sprintf_s(name, "f32x4(%f, %f, %f, %f)", lit.fx4.x, lit.fx4.y, lit.fx4.z, lit.fx4.w);
            } else if (lit_type == f16Ty) {
                sprintf_s(name, "f16(%f)", f32(lit.h));
            } else if (lit_type == f16x2Ty) {
                sprintf_s(name, "f16x2(%f, %f)", f32(lit.hx2.x), f32(lit.hx2.y));
            } else if (lit_type == f16x3Ty) {
                sprintf_s(name, "f16x3(%f, %f, %f)", f32(lit.hx3.x), f32(lit.hx3.y), f32(lit.hx3.z));
            } else if (lit_type == f16x4Ty) {
                sprintf_s(name, "f16x4(%f, %f, %f, %f)", f32(lit.hx4.x), f32(lit.hx4.y), f32(lit.hx4.z), f32(lit.hx4.w));
            } else if (lit_type == i32Ty) {
                sprintf_s(name, "i32(%i)", lit.i);
            } else if (lit_type == i32x2Ty) {
                sprintf_s(name, "i32x2(%i, %i)", lit.ix2.x, lit.ix2.y);
            } else if (lit_type == i32x3Ty) {
                sprintf_s(name, "i32x3(%i, %i, %i)", lit.ix3.x, lit.ix3.y, lit.ix3.z);
            } else if (lit_type == i32x4Ty) {
                sprintf_s(name, "i32x4(%i, %i, %i, %i)", lit.ix4.x, lit.ix4.y, lit.ix4.z, lit.ix4.w);
            } else if (lit_type == u32Ty) {
                sprintf_s(name, "u32(%i)", lit.i);
            } else if (lit_type == u32x2Ty) {
                sprintf_s(name, "u32x2(%i, %i)", lit.ix2.x, lit.ix2.y);
            } else if (lit_type == u32x3Ty) {
                sprintf_s(name, "u32x3(%i, %i, %i)", lit.ix3.x, lit.ix3.y, lit.ix3.z);
            } else if (lit_type == u32x4Ty) {
                sprintf_s(name, "u32x4(%i, %i, %i, %i)", lit.ix4.x, lit.ix4.y, lit.ix4.z, lit.ix4.w);
            } else {
                SJIT_UNIMPLEMENTED;
            }

#    if 0
				hlsl.EmitF("%s %s = ", InferType()->GetName().c_str(), name);
            if (lit_type == f32Ty) {
                hlsl.EmitF("f32(%f)", lit.f);
            } else if (lit_type == f32x2Ty) {
                hlsl.EmitF("f32x2(%f, %f)", lit.fx2.x, lit.fx2.y);
            } else if (lit_type == f32x3Ty) {
                hlsl.EmitF("f32x3(%f, %f, %f)", lit.fx3.x, lit.fx3.y, lit.fx3.z);
            } else if (lit_type == f32x4Ty) {
                hlsl.EmitF("f32x4(%f, %f, %f, %f)", lit.fx4.x, lit.fx4.y, lit.fx4.z, lit.fx4.w);
            } else if (lit_type == i32Ty) {
                hlsl.EmitF("i32(%i)", lit.i);
            } else if (lit_type == i32x2Ty) {
                hlsl.EmitF("i32x2(%i, %i)", lit.ix2.x, lit.ix2.y);
            } else if (lit_type == i32x3Ty) {
                hlsl.EmitF("i32x3(%i, %i, %i)", lit.ix3.x, lit.ix3.y, lit.ix3.z);
            } else if (lit_type == i32x4Ty) {
                hlsl.EmitF("i32x4(%i, %i, %i, %i)", lit.ix4.x, lit.ix4.y, lit.ix4.z, lit.ix4.w);
            } else if (lit_type == u32Ty) {
                hlsl.EmitF("u32(%i)", lit.i);
            } else if (lit_type == u32x2Ty) {
                hlsl.EmitF("u32x2(%i, %i)", lit.ix2.x, lit.ix2.y);
            } else if (lit_type == u32x3Ty) {
                hlsl.EmitF("u32x3(%i, %i, %i)", lit.ix3.x, lit.ix3.y, lit.ix3.z);
            } else if (lit_type == u32x4Ty) {
                hlsl.EmitF("u32x4(%i, %i, %i, %i)", lit.ix4.x, lit.ix4.y, lit.ix4.z, lit.ix4.w);
            } else {
                SJIT_UNIMPLEMENTED;
            }
            hlsl.EmitF(";\n");
#    endif // 0

        } else if (type == EXPRESSION_TYPE_FUNCTION) {
            if (InferType() != VoidTy) hlsl.EmitF("%s %s = ", InferType()->GetName().c_str(), name);
            fn_prototype->EmitCall(hlsl_module, argv);
            hlsl.EmitF(";\n");
        } else if (type == EXPRESSION_TYPE_RESOURCE) {
            hlsl_module.AddResource(resource->GetName(), resource);
            hlsl_module.AddType(resource->GetType());
            sprintf_s(name, "%s", resource->GetName().c_str());
        } else if (type == EXPRESSION_TYPE_INPUT) {
            switch (in_type) {
            case IN_TYPE_GROUP_THREAD_ID: sprintf_s(name, "__gid"); break;
            case IN_TYPE_DISPATCH_GROUP_ID: sprintf_s(name, "__group_id"); break;
            case IN_TYPE_DISPATCH_THREAD_ID:
                sprintf_s(name, "__tid");
                /* hlsl.EmitF("%s %s = ", InferType()->GetName().c_str(), name);
                 hlsl.EmitF("__tid");*/
                break;
            case IN_TYPE_CUSTOM:
                SJIT_UNIMPLEMENTED;
                /*hlsl.EmitF("%s %s = __payload.", InferType()->GetName().c_str(), name);
                hlsl.EmitF(input_name);*/
                break;
            default: SJIT_UNIMPLEMENTED;
            }
            hlsl.EmitF(";\n");
        } else if (type == EXPRESSION_TYPE_SWIZZLE) {

            // ifor(swizzle_size) hlsl.Putc(swizzle[i]);
            /*lhs->EmitHLSL(hlsl_module);
            sjit_assert(lhs && swizzle_size);
            hlsl.EmitF("%s %s = %s.", InferType()->GetName().c_str(), name, lhs->name);
            ifor(swizzle_size) hlsl.Putc(swizzle[i]);
            hlsl.EmitF(";\n");*/
        } else if (type == EXPRESSION_TYPE_FIELD) {

            sprintf_s(name, "%s.%s", lhs->name, field_name);
        } else if (type == EXPRESSION_TYPE_INDEX) {

            if (index)
                sprintf_s(name, "%s[%s]", lhs->name, index->name);
            else
                sprintf_s(name, "%s[%i]", lhs->name, index_literal);

        } else if (type == EXPRESSION_TYPE_REF) {
        } else if (type == EXPRESSION_TYPE_IF_ELSE) {
            sjit_assert(lhs && rhs && cond);
            cond->EmitHLSL(hlsl_module);
            hlsl.EmitF("%s %s;\n", InferType()->GetName().c_str(), name);
            hlsl.EmitF("if (%s) {\n", cond->name);
            hlsl_module.EnterScope();
            lhs->EmitHLSL(hlsl_module);
            hlsl_module.ExitScope();
            hlsl.EmitF("%s = %s;\n", name, lhs->name);
            hlsl.EmitF("} else {\n");
            hlsl_module.EnterScope();
            rhs->EmitHLSL(hlsl_module);
            hlsl_module.ExitScope();
            hlsl.EmitF("%s = %s;\n", name, rhs->name);
            hlsl.EmitF("}\n");
        }

#    if 0
                else if (type == EXPRESSION_TYPE_WHILE) {
            SJIT_UNIMPLEMENTED;
            sjit_assert(lhs && cond);

            hlsl_module.EnterFunction();
            hlsl_module.EnterScope();
            hlsl.EmitF("struct Payload_%s {\n", name);
            hlsl.EmitF("};\n");
            hlsl.EmitF("bool tmp_%s(inout Payload_%s payload) {\n", name, name);

            hlsl.EmitF("}\n");
            hlsl_module.ExitScope();
            hlsl_module.ExitFunction();

            hlsl.EmitF("while (true) {\n");
            hlsl_module.EnterScope();
            hlsl.EmitF("bool do_break = tmp_%s(payload);\n", name);
            hlsl.EmitF("if (do_break) break;\n");
            hlsl_module.ExitScope();
            hlsl.EmitF("}\n");
              }
#    endif // 0

        else if (type == EXPRESSION_TYPE_STRUCT_INIT) {
            hlsl_module.AddType(InferType());
            hlsl.EmitF("%s %s = (%s)0;\n", InferType()->GetName().c_str(), name, InferType()->GetName().c_str());
        }
#    if 0
        else if (type == EXPRESSION_TYPE_RETURN) {
            hlsl .EmitF("return;\n");
        }
        else if (type == EXPRESSION_TYPE_IF) {
            hlsl .EmitF("if (%s)\n", lhs->name);
        }
        else if (type == EXPRESSION_TYPE_ELSE) {
            hlsl .EmitF("else\n", lhs->name);
        }
        else if (type == EXPRESSION_TYPE_BREAK) {
            hlsl .EmitF("break;\n", lhs->name);
        }
#    endif // 0
        else if (type == EXPRESSION_TYPE_ARRAY) {
        } else {
            SJIT_UNIMPLEMENTED;
        }
    }
    bool       IsScalar() { return GetScalarMode() == SCALAR_MODE_SCALAR; }
    ScalarMode GetScalarMode() {
        if (scalar_mode != SCALAR_MODE_UNKNOWN) return scalar_mode;
        if (type == EXPRESSION_TYPE_OP) {
            if ((lhs && lhs->GetScalarMode() == SCALAR_MODE_NON_SCALAR) || (rhs && rhs->GetScalarMode() == SCALAR_MODE_NON_SCALAR))
                scalar_mode = SCALAR_MODE_NON_SCALAR;
            else
                scalar_mode = SCALAR_MODE_SCALAR;
        } else if (type == EXPRESSION_TYPE_LITERAL) {
            scalar_mode = SCALAR_MODE_SCALAR;
        } else if (type == EXPRESSION_TYPE_RESOURCE) {
            scalar_mode = SCALAR_MODE_SCALAR;
        } else if (type == EXPRESSION_TYPE_INPUT) {
            scalar_mode = SCALAR_MODE_NON_SCALAR;
        } else if (type == EXPRESSION_TYPE_STRUCT_INIT) {
            scalar_mode = SCALAR_MODE_SCALAR;
        } else if (type == EXPRESSION_TYPE_STRUCT_INIT) {
            scalar_mode = SCALAR_MODE_SCALAR;
        } else if (type == EXPRESSION_TYPE_SWIZZLE) {
            scalar_mode = lhs->GetScalarMode();
        } else if (type == EXPRESSION_TYPE_FUNCTION) {
            if (fn_prototype->IsNonScalar())
                scalar_mode = SCALAR_MODE_NON_SCALAR;
            else {
                scalar_mode = SCALAR_MODE_SCALAR;
                for (auto &arg : argv) {
                    if (arg->GetScalarMode() == SCALAR_MODE_NON_SCALAR) {
                        scalar_mode = SCALAR_MODE_NON_SCALAR;
                        break;
                    }
                }
            }
        } else if (type == EXPRESSION_TYPE_IF_ELSE) {
            if (cond->GetScalarMode() == SCALAR_MODE_NON_SCALAR)
                scalar_mode = SCALAR_MODE_NON_SCALAR;
            else {
                if ((lhs && lhs->GetScalarMode() == SCALAR_MODE_NON_SCALAR) || (rhs && rhs->GetScalarMode() == SCALAR_MODE_NON_SCALAR))
                    scalar_mode = SCALAR_MODE_NON_SCALAR;
                else
                    scalar_mode = SCALAR_MODE_SCALAR;
            }
        } else if (type == EXPRESSION_TYPE_INDEX) {
            if (index) {
                if ((lhs && lhs->GetScalarMode() == SCALAR_MODE_NON_SCALAR) || (index && index->GetScalarMode() == SCALAR_MODE_NON_SCALAR)) scalar_mode = SCALAR_MODE_NON_SCALAR;
            } else
                scalar_mode = lhs->GetScalarMode();

        } else {
            SJIT_UNIMPLEMENTED;
        }
        assert(scalar_mode != SCALAR_MODE_UNKNOWN);
        return scalar_mode;
    }
    SharedPtr<Type> InferType() {
        if (inferred_type) return inferred_type;

        if (type == EXPRESSION_TYPE_OP || type == EXPRESSION_TYPE_IF_ELSE) {
            SharedPtr<Type> lhs_ty  = {};
            SharedPtr<Type> rhs_ty  = {};
            SharedPtr<Type> cond_ty = {};

            if (lhs) lhs_ty = lhs->InferType();
            if (rhs) rhs_ty = rhs->InferType();
            if (cond) cond_ty = cond->InferType();

            if (cond) {
                sjit_assert(cond_ty == u1Ty);
            }
            if (                         //
                op_type == OP_BIT_NEG || //
                false) {
                sjit_assert(rhs_ty == u32Ty);
                inferred_type = u32Ty;
            } else if (                      //
                op_type == OP_LOGICAL_NOT || //
                false) {
                sjit_assert(rhs_ty == u1Ty);
                inferred_type = u1Ty;
            } else if (                         //
                op_type == OP_BIT_AND ||        //
                op_type == OP_BIT_OR ||         //
                op_type == OP_BIT_XOR ||        //
                op_type == OP_BIT_OR_ASSIGN ||  //
                op_type == OP_BIT_XOR_ASSIGN || //
                op_type == OP_BIT_AND_ASSIGN || //
                op_type == OP_MODULO ||         //
                op_type == OP_SHIFT_LEFT ||     //
                op_type == OP_SHIFT_RIGHT ||    //
                false) {
                sjit_assert(lhs_ty == rhs_ty);
                sjit_assert(lhs_ty == u32Ty || lhs_ty == u32x2Ty || lhs_ty == u32x3Ty || lhs_ty == u32x4Ty);
                inferred_type = lhs_ty;
            } else if (                      //
                op_type == OP_LOGICAL_OR ||  //
                op_type == OP_LOGICAL_AND || //
                false) {
                sjit_assert(lhs_ty == rhs_ty);
                sjit_assert(lhs_ty == u1Ty);
                inferred_type = u1Ty;
            } else if (                           //
                op_type == OP_LESS ||             //
                op_type == OP_LESS_OR_EQUAL ||    //
                op_type == OP_GREATER ||          //
                op_type == OP_GREATER_OR_EQUAL || //
                op_type == OP_EQUAL ||            //
                op_type == OP_NOT_EQUAL ||        //
                false                             //
            ) {

                sjit_assert(lhs_ty && rhs_ty);
                sjit_assert(lhs_ty == rhs_ty);

                inferred_type = vector_type_table[BASIC_TYPE_U1][lhs_ty->GetVectorSize()];
            } else {
                if ((!lhs_ty && rhs_ty) || (lhs_ty && !rhs_ty)) {
                    if (lhs_ty) inferred_type = lhs_ty;
                    if (rhs_ty) inferred_type = rhs_ty;
                } else {
                    sjit_assert(lhs_ty && rhs_ty);
                    if (lhs_ty == rhs_ty) {
                        inferred_type = lhs_ty;
                    } else {

                        if (lhs_ty->GetBasicTy() == rhs_ty->GetBasicTy()) {
                            if (lhs_ty->GetVectorSize() == u32(1)) {
                                if (op_type == OP_MUL) {
                                    inferred_type = rhs_ty;
                                } else {
                                    SJIT_UNIMPLEMENTED;
                                }
                            } else if (rhs_ty->GetVectorSize() == u32(1)) {
                                if (op_type == OP_MUL || op_type == OP_DIV || op_type == OP_MUL_ASSIGN || op_type == OP_DIV_ASSIGN) {
                                    inferred_type = lhs_ty;
                                } else {
                                    SJIT_UNIMPLEMENTED;
                                }
                            } else {
                                SJIT_UNIMPLEMENTED;
                            }
                        } else {
                            SJIT_UNIMPLEMENTED;
                        }
                    }
                }
            }

        } else if (type == EXPRESSION_TYPE_LITERAL) {
            sjit_assert(lit_type);
            inferred_type = lit_type;
        } else if (type == EXPRESSION_TYPE_FUNCTION) {
            sjit_assert(fn_prototype);
            Array<SharedPtr<Type>> argv_ty = {};
            for (auto &e : argv) argv_ty.push_back(e->InferType());
            inferred_type = fn_prototype->GetReturnTy(argv_ty);
        } else if (type == EXPRESSION_TYPE_RESOURCE) {
            inferred_type = resource->GetType();
        } else if (type == EXPRESSION_TYPE_INPUT) {
            switch (in_type) {
            case IN_TYPE_DISPATCH_THREAD_ID: inferred_type = u32x3Ty; break;
            case IN_TYPE_DISPATCH_GROUP_ID: inferred_type = u32x3Ty; break;
            case IN_TYPE_GROUP_THREAD_ID: inferred_type = u32x3Ty; break;
            default: SJIT_UNIMPLEMENTED;
            }
        } else if (type == EXPRESSION_TYPE_SWIZZLE) {
            sjit_assert(lhs && swizzle_size);

            auto lhs_ty = lhs->InferType();
            u32  size   = swizzle_size;

            auto ty = vector_type_table[lhs_ty->GetBasicTy()][size];

            sjit_assert(ty);

            inferred_type = ty;
        } else if (type == EXPRESSION_TYPE_STRUCT_INIT) {
            inferred_type = lit_type;
        }
#    if 0
        else if (type == EXPRESSION_TYPE_RETURN) {
            inferred_type = VoidTy;
        }
        else if (type == EXPRESSION_TYPE_IF) {
            inferred_type = VoidTy;
        }
        else if (type == EXPRESSION_TYPE_ELSE) {
            inferred_type = VoidTy;
        }
        else if (type == EXPRESSION_TYPE_BREAK) {
            inferred_type = VoidTy;
        }
#    endif // 0

        else {
            SJIT_UNIMPLEMENTED;
        }

        sjit_assert(inferred_type);

        return inferred_type;
    }
};

static void EmitFunctionCall(FnPrototype *fn, HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
    hlsl_module.GetBody().EmitF("%s(", fn->GetName().c_str());
    ifor(argv.size()) {
        if (u64(i) == argv.size() - u64(1))
            hlsl_module.GetBody().EmitF("%s", argv[i]->name);
        else
            hlsl_module.GetBody().EmitF("%s, ", argv[i]->name);
    }
    hlsl_module.GetBody().EmitF(")");
}
static void EmitFunctionDefinition(FnPrototype *fn, HLSLModule &hlsl_module, Array<SharedPtr<Type>> const &argv) {
    hlsl_module.GetBody().EmitF("%s %s(", fn->GetReturnTy(argv)->GetName().c_str(), fn->GetName().c_str());
    ifor(fn->GetArgv().size()) {
        if (u64(i) == fn->GetArgv().size() - u64(1))
            hlsl_module.GetBody().EmitF("%s %s %s", (fn->GetArgv()[i].inout == FN_ARG_INOUT ? "inout" : "in"), fn->GetArgv()[i].type->GetName().c_str(),
                                        fn->GetArgv()[i].name.c_str());
        else
            hlsl_module.GetBody().EmitF("%s %s %s, ", (fn->GetArgv()[i].inout == FN_ARG_INOUT ? "inout" : "in"), fn->GetArgv()[i].type->GetName().c_str(),
                                        fn->GetArgv()[i].name.c_str());
    }
    hlsl_module.GetBody().EmitF(")");
}
static void EmitType(Type *ty, HLSLModule &hlsl_module) {
    if (ty->GetBasicTy() == BASIC_TYPE_STRUCTURE) {
        hlsl_module.GetBody().EmitF("struct %s {\n", ty->GetName().c_str());
        for (auto &f : ty->GetFields()) {
            hlsl_module.GetBody().EmitF("%s %s;\n", f.second->GetName().c_str(), f.first.c_str());
        }
        hlsl_module.GetBody().EmitF("};\n");
    } else {
        hlsl_module.GetBody().EmitF("%s", ty->GetName().c_str());
    }
}

static SharedPtr<FnPrototype> SampleTy = FnPrototype::Create(
    "Sample", WildcardTy_0, {{"texture", WildcardTy_1}, {"uv", WildcardTy_2}},
    [](Array<SharedPtr<Type>> const &argv) {
        sjit_assert(argv[0]->GetBasicTy() == BASIC_TYPE_RESOURCE);
        sjit_assert(argv[0]->GetResType() == RES_TEXTURE);
        sjit_assert(argv[1]->GetBasicTy() == BASIC_TYPE_RESOURCE);
        sjit_assert(argv[1]->GetResType() == RES_SAMPLER);
        sjit_assert(argv[2]->GetBasicTy() == BASIC_TYPE_F32);
        sjit_assert(argv[2]->GetVectorSize() == u32(2) || argv[2]->GetVectorSize() == u32(3));

        return argv[0]->GetTemplateType();
    },
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
        hlsl_module.GetBody().EmitF("%s.SampleLevel(%s, %s, f32(0.0))", argv[0]->name, argv[1]->name, argv[2]->name);
    });
static SharedPtr<FnPrototype> PowTy           = FnPrototype::Create("pow", WildcardTy_0, {{"a", WildcardTy_0}, {"b", WildcardTy_1}}, [](Array<SharedPtr<Type>> const &argv) {
    sjit_assert(argv[1] == f32Ty && argv[0]->GetBasicTy() == BASIC_TYPE_F32 || argv[1] == f16Ty && argv[0]->GetBasicTy() == BASIC_TYPE_F16);
    return argv[0];
          });
static SharedPtr<FnPrototype> ExpTy           = FnPrototype::Create("exp", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> DotTy           = FnPrototype::Create("dot", WildcardTy_1, {{"a", WildcardTy_0}, {"b", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) {
    sjit_assert(argv[1] == argv[0]);
    return vector_type_table[argv[0]->GetBasicTy()][1];
          });
static SharedPtr<FnPrototype> GetDimensionsTy = FnPrototype::Create(
    "GetDimensions", WildcardTy_1, {{"texture", WildcardTy_0}},
    [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[BASIC_TYPE_U32][GetNumDims(argv[0]->GetDimType())]; },
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) { hlsl_module.GetBody().EmitF("__get_dimensions(%s)", argv[0]->name); });
static SharedPtr<FnPrototype> ConvertToF32Ty = FnPrototype::Create(
    "ConvertToF32", WildcardTy_0, {{"a", WildcardTy_1}}, [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[BASIC_TYPE_F32][argv[0]->GetVectorSize()]; },
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
        auto ty = vector_type_table[BASIC_TYPE_F32][argv[0]->InferType()->GetVectorSize()];
        hlsl_module.GetBody().EmitF("%s(%s)", ty->GetName().c_str(), argv[0]->name);
    });
static SharedPtr<FnPrototype> ConvertToF16Ty = FnPrototype::Create(
    "ConvertToF32", WildcardTy_0, {{"a", WildcardTy_1}}, [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[BASIC_TYPE_F16][argv[0]->GetVectorSize()]; },
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
        auto ty = vector_type_table[BASIC_TYPE_F16][argv[0]->InferType()->GetVectorSize()];
        hlsl_module.GetBody().EmitF("%s(%s)", ty->GetName().c_str(), argv[0]->name);
    });
static SharedPtr<FnPrototype> BitcastToF32Ty = FnPrototype::Create(
    "BitcastToF32", WildcardTy_0, {{"a", WildcardTy_1}}, [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[BASIC_TYPE_F32][argv[0]->GetVectorSize()]; },
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) { hlsl_module.GetBody().EmitF("asf32(%s)", argv[0]->name); });
static SharedPtr<FnPrototype> u32_to_f16_FnTy = FnPrototype::Create("u32_to_f16_FnTy", f16Ty, {{"a", u32Ty}}, {}, [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
    hlsl_module.GetBody().EmitF("f16(f16tof32(%s))", argv[0]->name);
});
static SharedPtr<FnPrototype> f16_to_u32_FnTy = FnPrototype::Create("f16_to_u32_FnTy", u32Ty, {{"a", u32Ty}}, {}, [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
    hlsl_module.GetBody().EmitF("u32(f32tof16(%s))", argv[0]->name);
});
static SharedPtr<FnPrototype> ConvertToU32Ty  = FnPrototype::Create(
     "ConvertToU32", WildcardTy_0, {{"a", WildcardTy_1}}, [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[BASIC_TYPE_U32][argv[0]->GetVectorSize()]; },
     [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
        auto ty = vector_type_table[BASIC_TYPE_U32][argv[0]->InferType()->GetVectorSize()];
        hlsl_module.GetBody().EmitF("%s(%s)", ty->GetName().c_str(), argv[0]->name);
     });
static SharedPtr<FnPrototype> BitcastToU32Ty = FnPrototype::Create(
    "BitcastToU32", WildcardTy_0, {{"a", WildcardTy_1}}, [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[BASIC_TYPE_U32][argv[0]->GetVectorSize()]; },
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) { hlsl_module.GetBody().EmitF("asu32(%s)", argv[0]->name); });
static SharedPtr<FnPrototype> ConvertToI32Ty = FnPrototype::Create(
    "ConvertToI32", WildcardTy_0, {{"a", WildcardTy_1}}, [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[BASIC_TYPE_I32][argv[0]->GetVectorSize()]; },
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
        auto ty = vector_type_table[BASIC_TYPE_I32][argv[0]->InferType()->GetVectorSize()];
        hlsl_module.GetBody().EmitF("%s(%s)", ty->GetName().c_str(), argv[0]->name);
    });
static SharedPtr<FnPrototype> BitcastToI32Ty = FnPrototype::Create(
    "BitcastToI32", WildcardTy_0, {{"a", WildcardTy_1}}, [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[BASIC_TYPE_I32][argv[0]->GetVectorSize()]; },
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) { hlsl_module.GetBody().EmitF("asi32(%s)", argv[0]->name); });
static SharedPtr<FnPrototype> WriteFnTy = FnPrototype::Create(
    "Write", VoidTy, {{"index", WildcardTy_0}, {"value", WildcardTy_1}}, //
    {},                                                                  //
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) { hlsl_module.GetBody().EmitF("%s[%s] = %s", argv[0]->name, argv[1]->name, argv[2]->name); });
static SharedPtr<FnPrototype> ReadFnTy = FnPrototype::Create(
    "Read", VoidTy, {{"index", WildcardTy_0}},                                     //
    [](Array<SharedPtr<Type>> const &argv) { return argv[0]->GetTemplateType(); }, //
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) { hlsl_module.GetBody().EmitF("%s[%s]", argv[0]->name, argv[1]->name); });
static SharedPtr<FnPrototype> Splat2FnTy = FnPrototype::Create(
    "Splat", WildcardTy_0, {{"a", WildcardTy_1}},                                                   //
    [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[argv[0]->GetBasicTy()][2]; }, //
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
        hlsl_module.GetBody().EmitF("%s_splat(%s)", vector_type_table[argv[0]->InferType()->GetBasicTy()][2]->GetName().c_str(), argv[0]->name);
    });
static SharedPtr<FnPrototype> Splat3FnTy = FnPrototype::Create(
    "Splat", WildcardTy_0, {{"a", WildcardTy_1}},                                                   //
    [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[argv[0]->GetBasicTy()][2]; }, //
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
        hlsl_module.GetBody().EmitF("%s_splat(%s)", vector_type_table[argv[0]->InferType()->GetBasicTy()][2]->GetName().c_str(), argv[0]->name);
    });
static SharedPtr<FnPrototype> Splat4FnTy = FnPrototype::Create(
    "Splat", WildcardTy_0, {{"a", WildcardTy_1}},                                                   //
    [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[argv[0]->GetBasicTy()][2]; }, //
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) {
        hlsl_module.GetBody().EmitF("%s_splat(%s)", vector_type_table[argv[0]->InferType()->GetBasicTy()][2]->GetName().c_str(), argv[0]->name);
    });
static SharedPtr<FnPrototype> AllFnTy = FnPrototype::Create(
    "all", u1Ty, {{"a", WildcardTy_1}},
    [](Array<SharedPtr<Type>> const &argv) {
        sjit_assert(argv[0]->GetBasicTy() == BASIC_TYPE_U1);
        return u1Ty;
    },
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) { hlsl_module.GetBody().EmitF("all(%s)", argv[0]->name); });
static SharedPtr<FnPrototype> AnyFnTy = FnPrototype::Create(
    "any", u1Ty, {{"a", WildcardTy_1}},
    [](Array<SharedPtr<Type>> const &argv) {
        sjit_assert(argv[0]->GetBasicTy() == BASIC_TYPE_U1);
        return u1Ty;
    },
    [](HLSLModule &hlsl_module, Array<SharedPtr<Expr>> const &argv) { hlsl_module.GetBody().EmitF("any(%s)", argv[0]->name); });

static SharedPtr<Type>        RayQuery_Ty    = Type::Create("RayQueryWrapper",
                                                            {
                                                      {"hit", u1Ty},            //
                                                      {"bary", f32x2Ty},        //
                                                      {"ray_t", f32Ty},         //
                                                      {"primitive_idx", u32Ty}, //
                                                      {"instance_id", u32Ty},   //
                                                  },
                                                            /* builtin */ true);
static SharedPtr<FnPrototype> GetLaneIdxFnTy = FnPrototype::Create("WaveGetLaneIndex", u32Ty, {}, {}, {}, /*non_scalar*/ true);
static SharedPtr<FnPrototype> GetLaneBitFnTy = FnPrototype::Create("__get_lane_bit", u32Ty, {}, {}, {}, /*non_scalar*/ true);
static SharedPtr<FnPrototype> RayTestFnTy    = FnPrototype::Create("__anyhit", u1Ty, {}, {}, {});
static SharedPtr<FnPrototype> RayQueryFnTy   = FnPrototype::Create("__ray_query", RayQuery_Ty, {}, {}, {});
static SharedPtr<FnPrototype> PopCntFnTy     = FnPrototype::Create("countbits", u32Ty, {{"a", u32Ty}});
static SharedPtr<FnPrototype> NormalizeFnTy  = FnPrototype::Create("normalize", f32x3Ty, {{"a", f32x3Ty}});
static SharedPtr<FnPrototype> TransposeTy    = FnPrototype::Create("transpose", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) {
    sjit_assert(argv[0]->IsMatrix());
    return argv[0];
   });
static SharedPtr<FnPrototype> NonUniformFnTy = FnPrototype::Create("NonUniformResourceIndex", u32Ty, {{"a", u32Ty}});
static SharedPtr<FnPrototype> IsNanFnTy      = FnPrototype::Create("isnan", WildcardTy_0, {{"a", WildcardTy_1}},
                                                                   [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[BASIC_TYPE_U1][argv[0]->GetVectorSize()]; });
static SharedPtr<FnPrototype> IsInfFnTy      = FnPrototype::Create("isinf", WildcardTy_0, {{"a", WildcardTy_1}},
                                                                   [](Array<SharedPtr<Type>> const &argv) { return vector_type_table[BASIC_TYPE_U1][argv[0]->GetVectorSize()]; });
static SharedPtr<FnPrototype> CrossTy        = FnPrototype::Create("cross", f32x3Ty, {{"a", f32x3Ty}, {"b", f32x3Ty}});
static SharedPtr<FnPrototype> ReflectTy      = FnPrototype::Create("reflect", f32x3Ty, {{"a", f32x3Ty}, {"b", f32x3Ty}});
static SharedPtr<FnPrototype> TanFnTy        = FnPrototype::Create("tan", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> FracFnTy       = FnPrototype::Create("frac", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> SaturateFnTy   = FnPrototype::Create("saturate", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> LogFnTy        = FnPrototype::Create("log", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> FloorFnTy      = FnPrototype::Create("floor", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> SinFnTy        = FnPrototype::Create("sin", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> CosFnTy        = FnPrototype::Create("cos", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> SqrtFnTy       = FnPrototype::Create("sqrt", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> RsqrtFnTy      = FnPrototype::Create("rsqrt", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> AbsFnTy        = FnPrototype::Create("abs", WildcardTy_0, {{"a", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> MaxFnTy =
    FnPrototype::Create("max", WildcardTy_0, {{"a", WildcardTy_0}, {"b", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> MinFnTy =
    FnPrototype::Create("min", WildcardTy_0, {{"a", WildcardTy_0}, {"b", WildcardTy_0}}, [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> LerpFnTy               = FnPrototype::Create("lerp", WildcardTy_0, {{"a", WildcardTy_0}, {"b", WildcardTy_0}, {"c", f32Ty}}, //
                                                                           [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> ClampFnTy              = FnPrototype::Create("clamp", WildcardTy_0, {{"a", WildcardTy_0}, {"b", WildcardTy_0}, {"c", WildcardTy_0}}, //
                                                                           [](Array<SharedPtr<Type>> const &argv) { return argv[0]; });
static SharedPtr<FnPrototype> LengthFnTy             = FnPrototype::Create("length", f32Ty, {{"a", WildcardTy_0}});
static SharedPtr<FnPrototype> MakeF32X2_1_1_FnTy     = FnPrototype::Create("f32x2", f32x2Ty, {{"a", WildcardTy_0}, {"b", WildcardTy_1}});
static SharedPtr<FnPrototype> MakeF32X4_1_1_FnTy     = FnPrototype::Create("f32x4", f32x4Ty, {{"a", WildcardTy_0}, {"b", WildcardTy_1}});
static SharedPtr<FnPrototype> MakeF32X3_1_1_1_FnTy   = FnPrototype::Create("f32x3", f32x3Ty, {{"a", f32Ty}, {"b", f32Ty}, {"c", f32Ty}});
static SharedPtr<FnPrototype> MakeF32X3_1_1_FnTy     = FnPrototype::Create("f32x3", f32x3Ty, {{"a", WildcardTy_0}, {"b", WildcardTy_1}});
static SharedPtr<FnPrototype> MakeU32X3_1_1_1_FnTy   = FnPrototype::Create("u32x3", u32x3Ty, {{"a", u32Ty}, {"b", u32Ty}, {"c", u32Ty}});
static SharedPtr<FnPrototype> MakeU32X3_1_1_FnTy     = FnPrototype::Create("u32x3", u32x3Ty, {{"a", WildcardTy_0}, {"b", WildcardTy_1}});
static SharedPtr<FnPrototype> MakeF32X4_1_1_1_FnTy   = FnPrototype::Create("f32x4", f32x4Ty, {{"a", f32Ty}, {"b", f32Ty}});
static SharedPtr<FnPrototype> MakeF32X4_1_1_1_1_FnTy = FnPrototype::Create("f32x4", f32x4Ty, {{"a", f32Ty}, {"b", f32Ty}, {"c", f32Ty}, {"d", f32Ty}});
static SharedPtr<FnPrototype> MulFnTy                = FnPrototype::Create("mul", WildcardTy_1, {{"a", WildcardTy_0}, {"b", WildcardTy_1}}, //
                                                                           [](Array<SharedPtr<Type>> const &args) {
                                                                if (args[0] == f32x3x3Ty && args[1] == f32x3Ty) return f32x3Ty;
                                                                if (args[0] == f32x3Ty && args[1] == f32x3x3Ty) return f32x3Ty;
                                                                if (args[0] == f32x3x3Ty && args[1] == f32x3x3Ty) return f32x3x3Ty;

                                                                if (args[0] == f32x4x4Ty && args[1] == f32x4Ty) return f32x4Ty;
                                                                if (args[0] == f32x4Ty && args[1] == f32x4x4Ty) return f32x4Ty;
                                                                if (args[0] == f32x4x4Ty && args[1] == f32x4x4Ty) return f32x4x4Ty;

                                                                SJIT_TRAP;
                                                                           });
static SharedPtr<FnPrototype> GetTBNFnTy             = FnPrototype::Create("__get_tbn", f32x3x3Ty, {{"N", f32x3Ty}});
static SharedPtr<FnPrototype> InterpolateFnTy =
    FnPrototype::Create("__interpolate", WildcardTy_0, {{"a", WildcardTy_0}, {"b", WildcardTy_0}, {"c", WildcardTy_0}, {"bary", f32x2Ty}}, //
                        [](Array<SharedPtr<Type>> const &argv) {
                            sjit_assert(argv[0] == argv[1]);
                            sjit_assert(argv[1] == argv[2]);
                            return argv[0];
                        });

static Array<SharedPtr<FnPrototype>> splat_table = {
    NULL,       //
    Splat2FnTy, //
    Splat3FnTy, //
    Splat4FnTy, //
};

static Array<HLSLModule *> &GetGlobalModuleStack() {
    static thread_local Array<HLSLModule *> module_stack = {};
    return module_stack;
}
static bool        HasGlobalModule() { return GetGlobalModuleStack().size() != u64(0); }
static HLSLModule &GetGlobalModule() { return *GetGlobalModuleStack().back(); }
static void        PopModule() {
    delete GetGlobalModuleStack().back();
    GetGlobalModuleStack().pop_back();
}
static void PushModule() { GetGlobalModuleStack().push_back(new HLSLModule); }
static bool IsInScalarBlock() {
    if (HasGlobalModule()) { // Figure out if we're in a non-scalar condition block, then we're non-scalar also
        for (auto &s : GetGlobalModule().GetConditionStack()) {
            if (s && !s->IsScalar()) {
                return false;
            }
        }
    }
    return true;
}

#    define HLSL_MODULE_SCOPE                                                                                                                                                      \
        PushModule();                                                                                                                                                              \
        defer(PopModule());

class ValueExpr {
public:
    mutable SharedPtr<Expr> expr = NULL;

public:
    // ValueExpr(ValueExpr const &b) = default;
    // ValueExpr const &operator=(ValueExpr const &b) = default;
    /*ValueExpr operator=(ValueExpr &b) {
        expr = {Expr::MakeOp(expr, b.expr, OP_ASSIGN)};
        return *this;
    }*/
    SharedPtr<Expr> operator->() { return expr; }

    void EmitGlobalHLSL() {
        if (HasGlobalModule()) expr->EmitHLSL(GetGlobalModule());
        if (!IsInScalarBlock()) expr->scalar_mode = SCALAR_MODE_NON_SCALAR;
    }

    ValueExpr(SharedPtr<Expr> e) : expr(e) { EmitGlobalHLSL(); }
    template <typename T>
    ValueExpr(T v) {
        expr = Expr::MakeLiteral(v);
        EmitGlobalHLSL();
    }
#    if 0
    static ValueExpr Return() { return Expr::Create(EXPRESSION_TYPE_RETURN); }
    static ValueExpr If(ValueExpr v) { return Expr::MakeIf(v.expr); }
    static ValueExpr Else() { return Expr::MakeElse(); }
#    endif // 0

    // static ValueExpr IfElse(ValueExpr _cond, ValueExpr _lhs, ValueExpr _rhs) { return Expr::MakeIfElse(_cond.expr, _lhs.expr, _rhs.expr); }

    ValueExpr Sample(ValueExpr const &sampler, ValueExpr const &uv) {
        // sjit_assert(expr->type == EXPRESSION_TYPE_RESOURCE);
        // sjit_assert(expr->resource->GetType()->GetResType() == RES_TEXTURE);
        SharedPtr<Expr> argv[] = {
            expr,
            sampler.expr,
            uv.expr,
        };
        return Expr::MakeFunction(SampleTy, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr GetDimensions() {
        sjit_assert(expr->type == EXPRESSION_TYPE_RESOURCE);
        sjit_assert(expr->resource->GetType()->GetResType() == RES_TEXTURE);
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(GetDimensionsTy, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr ToF32() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(ConvertToF32Ty, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr ToF16() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(ConvertToF16Ty, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr AsF32() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(BitcastToF32Ty, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr u32_to_f16() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(u32_to_f16_FnTy, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr f16_to_u32() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(f16_to_u32_FnTy, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr ToU32() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(ConvertToU32Ty, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr AsU32() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(BitcastToU32Ty, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr ToI32() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(ConvertToI32Ty, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr AsI32() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(BitcastToI32Ty, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr Swizzle(char const *_swizzle) { return Expr::MakeSwizzle(expr, _swizzle); }
    ValueExpr All() {
        SharedPtr<Expr> argv[] = {
            expr,
        };
        return Expr::MakeFunction(AllFnTy, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr Any() {
        SharedPtr<Expr> argv[] = {
            expr,
        };
        return Expr::MakeFunction(AnyFnTy, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr Dot(ValueExpr const &b) {
        SharedPtr<Expr> argv[] = {expr, b.expr};
        return Expr::MakeFunction(DotTy, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr Splat(u32 num) {
        sjit_assert(expr->InferType() == f32Ty || expr->InferType() == i32Ty || expr->InferType() == u32Ty);
        sjit_assert(num > 1 && num <= 4);
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(splat_table[num], argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr operator|=(ValueExpr const &b) {
        ValueExpr e = (Expr::MakeOp(expr, b.expr, OP_BIT_OR_ASSIGN));
        if (!IsInScalarBlock()) {
            expr->scalar_mode = SCALAR_MODE_NON_SCALAR;
        }
        return *this;
    }
    ValueExpr operator^=(ValueExpr const &b) {
        ValueExpr e = (Expr::MakeOp(expr, b.expr, OP_BIT_XOR_ASSIGN));
        if (!IsInScalarBlock()) {
            expr->scalar_mode = SCALAR_MODE_NON_SCALAR;
        }
        return *this;
    }
    ValueExpr operator&=(ValueExpr const &b) {
        ValueExpr e = (Expr::MakeOp(expr, b.expr, OP_BIT_AND_ASSIGN));
        if (!IsInScalarBlock()) {
            expr->scalar_mode = SCALAR_MODE_NON_SCALAR;
        }
        return *this;
    }
    ValueExpr operator~() { return Expr::MakeOp(NULL, expr, OP_BIT_NEG); }
    ValueExpr operator!() { return Expr::MakeOp(NULL, expr, OP_LOGICAL_NOT); }
    ValueExpr operator+=(ValueExpr v) {
        ValueExpr e = (Expr::MakeOp(expr, v.expr, OP_PLUS_ASSIGN));
        if (!IsInScalarBlock()) {
            expr->scalar_mode = SCALAR_MODE_NON_SCALAR;
        }
        return *this;
    }
    ValueExpr operator*=(ValueExpr v) {
        ValueExpr e = (Expr::MakeOp(expr, v.expr, OP_MUL_ASSIGN));
        if (!IsInScalarBlock()) {
            expr->scalar_mode = SCALAR_MODE_NON_SCALAR;
        }
        return *this;
    }
    ValueExpr operator/=(ValueExpr v) {
        ValueExpr e = (Expr::MakeOp(expr, v.expr, OP_DIV_ASSIGN));
        if (!IsInScalarBlock()) {
            expr->scalar_mode = SCALAR_MODE_NON_SCALAR;
        }
        return *this;
    }
    // ValueExpr(ValueExpr const &v) : ValueExpr(Expr::MakeOp(expr, v.expr, OP_ASSIGN)) {}
    ValueExpr operator=(ValueExpr v) {
        ValueExpr e = ValueExpr(Expr::MakeOp(expr, v.expr, OP_ASSIGN));
        if (!IsInScalarBlock()) {
            expr->scalar_mode = SCALAR_MODE_NON_SCALAR;
        }
        return *this;
    }
    // ValueExpr(ValueExpr &&v) = delete;
    // ValueExpr operator=(ValueExpr &&v) = delete;
    ValueExpr SetName(char const *_name) {
        expr->SetName(_name);
        return *this;
    }
    ValueExpr Copy() { return ValueExpr(Expr::MakeOp(NULL, expr, OP_ASSIGN)); }
    ValueExpr PopCnt() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(PopCntFnTy, argv, u32(1));
    }
    void Set(char const *_field, ValueExpr _val) {
        ValueExpr e = ValueExpr(Expr::MakeOp(Expr::MakeField(expr, _field), _val.expr, OP_ASSIGN));
        if (!IsInScalarBlock()) {
            expr->scalar_mode = SCALAR_MODE_NON_SCALAR;
        }
    }
    ValueExpr Get(u32 index) { return Expr::MakeIndex(expr, index); }
    ValueExpr Load(ValueExpr e) { return ValueExpr(Expr::MakeIndex(expr, e.expr)).Copy(); }
    void      Store(ValueExpr e, ValueExpr v) { ValueExpr(Expr::MakeIndex(expr, e.expr)) = v; }
    ValueExpr Write(ValueExpr const &index, ValueExpr const &value) {
        SharedPtr<Expr> argv[] = {
            expr,
            index.expr,
            value.expr,
        };
        return Expr::MakeFunction(WriteFnTy, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr Read(ValueExpr const &index) {
        SharedPtr<Expr> argv[] = {expr, index.expr};
        return Expr::MakeFunction(ReadFnTy, argv, SJIT_ARRAYSIZE(argv));
    }
    ValueExpr Get(ValueExpr e) {
        ValueExpr o = ValueExpr(Expr::MakeIndex(expr, e.expr));
        if (expr->InferType()->GetBasicTy() == BASIC_TYPE_RESOURCE) // By default copy resource access
            return o.Copy();
        else
            return o;
    }
    ValueExpr Get(char const *_field) {
        if (expr->InferType()->IsVector())
            return Swizzle(_field);
        else
            return ValueExpr(Expr::MakeField(expr, _field));
    }
    ValueExpr x() { return (*this)["x"]; }
    ValueExpr y() { return (*this)["y"]; }
    ValueExpr z() { return (*this)["z"]; }
    ValueExpr w() { return (*this)["w"]; }
    ValueExpr xy() { return (*this)["xy"]; }
    ValueExpr xyz() { return (*this)["xyz"]; }
    ValueExpr zw() { return (*this)["zw"]; }
    ValueExpr yx() { return (*this)["yx"]; }
    ValueExpr operator[](u32 index) { return Get(index); }
    ValueExpr operator[](char const *_field) { return Get(_field); }
    ValueExpr operator[](ValueExpr e) { return Get(e); }
    ValueExpr NonUniform() {
        SharedPtr<Expr> argv[] = {expr};
        return Expr::MakeFunction(NonUniformFnTy, argv, SJIT_ARRAYSIZE(argv));
    }
};
#    if 0
static void IfElse(ValueExpr cond, std::function<void()> if_block, std::function<void()> else_block) {
    using var = ValueExpr;
    var::If(cond);
    {
        DSL_BLOCK_SCOPE;
        if_block();
    }
    var::Else();
    {
        DSL_BLOCK_SCOPE;
        else_block();
    }
}
#    endif // 0
static ValueExpr pow(ValueExpr const &a, ValueExpr const &b) {
    SharedPtr<Expr> argv[] = {
        a.expr,
        b.expr,
    };
    return Expr::MakeFunction(PowTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr exp(ValueExpr const &a) {
    SharedPtr<Expr> argv[] = {
        a.expr,
    };
    return Expr::MakeFunction(ExpTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr dot(ValueExpr const &a, ValueExpr const &b) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr};
    return Expr::MakeFunction(DotTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr reflect(ValueExpr const &a, ValueExpr const &b) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr};
    return Expr::MakeFunction(ReflectTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr transpose(ValueExpr const &a) {
    SharedPtr<Expr> argv[] = {a.expr};
    return Expr::MakeFunction(TransposeTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr cross(ValueExpr const &a, ValueExpr const &b) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr};
    return Expr::MakeFunction(CrossTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr ResourceAccess(SharedPtr<Resource> res) { return Expr::MakeResource(res); }
static ValueExpr Input(InType _in_type) { return Expr::MakeInput(_in_type); }
static ValueExpr Input(char const *_name, SharedPtr<Type> _type) { return Expr::MakeInput(_name, _type); }
static ValueExpr Zero(SharedPtr<Type> lit_type) {
    if (lit_type == f32Ty) {
        return ValueExpr(f32(0.0));
    } else if (lit_type == f32x2Ty) {
        return ValueExpr(f32x2(0.0, 0.0));
    } else if (lit_type == f32x3Ty) {
        return ValueExpr(f32x3(0.0, 0.0, 0.0));
    } else if (lit_type == f32x4Ty) {
        return ValueExpr(f32x4(0.0, 0.0, 0.0, 0.0));
    } else if (lit_type == f16Ty) {
        return ValueExpr(f16(0.0));
    } else if (lit_type == f16x2Ty) {
        return ValueExpr(f16x2(0.0, 0.0));
    } else if (lit_type == f16x3Ty) {
        return ValueExpr(f16x3(0.0, 0.0, 0.0));
    } else if (lit_type == f16x4Ty) {
        return ValueExpr(f16x4(0.0, 0.0, 0.0, 0.0));
    } else if (lit_type == i32Ty) {
        return ValueExpr(i32(0));
    } else if (lit_type == i32x2Ty) {
        return ValueExpr(i32x2(0, 0));
    } else if (lit_type == i32x3Ty) {
        return ValueExpr(i32x3(0, 0, 0));
    } else if (lit_type == i32x4Ty) {
        return ValueExpr(i32x4(0, 0, 0, 0));
    } else if (lit_type == u32Ty) {
        return ValueExpr(u32(0));
    } else if (lit_type == u32x2Ty) {
        return ValueExpr(u32x2(0, 0));
    } else if (lit_type == u32x3Ty) {
        return ValueExpr(u32x3(0, 0, 0));
    } else if (lit_type == u32x4Ty) {
        return ValueExpr(u32x4(0, 0, 0, 0));
    } else if (lit_type->GetBasicTy() == BASIC_TYPE_STRUCTURE) {
        SharedPtr<Expr> expr = Expr::Create(EXPRESSION_TYPE_STRUCT_INIT);
        expr->lit_type       = lit_type;
        return ValueExpr(expr);
    } else {
        SJIT_UNIMPLEMENTED;
    }
}
static ValueExpr Make(SharedPtr<Type> lit_type) { return Zero(lit_type).Copy(); }
static ValueExpr LaneIdx() { return Expr::MakeFunction(GetLaneIdxFnTy, {}, u32(0)); }
static ValueExpr LaneBit() { return Expr::MakeFunction(GetLaneBitFnTy, {}, u32(0)); }
static ValueExpr RayTest(ValueExpr tlas, ValueExpr ray_desc) {
    SharedPtr<Expr> argv[] = {
        tlas.expr,
        ray_desc.expr,
    };
    return Expr::MakeFunction(RayTestFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr RayQuery(ValueExpr tlas, ValueExpr ray_desc) {
    SharedPtr<Expr> argv[] = {
        tlas.expr,
        ray_desc.expr,
    };
    return Expr::MakeFunction(RayQueryFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr normalize(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(NormalizeFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr sqrt(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(SqrtFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr isnan(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(IsNanFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr isinf(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(IsInfFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr rsqrt(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(RsqrtFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr abs(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(AbsFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr lerp(ValueExpr a, ValueExpr b, ValueExpr c) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr, c.expr};
    return Expr::MakeFunction(LerpFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr clamp(ValueExpr a, ValueExpr b, ValueExpr c) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr, c.expr};
    return Expr::MakeFunction(ClampFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr max(ValueExpr a, ValueExpr b) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr};
    return Expr::MakeFunction(MaxFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr min(ValueExpr a, ValueExpr b) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr};
    return Expr::MakeFunction(MinFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr length(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(LengthFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr sin(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(SinFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr cos(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(CosFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr tan(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(TanFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr frac(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(FracFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr saturate(ValueExpr e) {
    SharedPtr<Expr> argv[] = {e.expr};
    return Expr::MakeFunction(SaturateFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr mul(ValueExpr a, ValueExpr b) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr};
    return Expr::MakeFunction(MulFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr log(ValueExpr a) {
    SharedPtr<Expr> argv[] = {a.expr};
    return Expr::MakeFunction(LogFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr floor(ValueExpr a) {
    SharedPtr<Expr> argv[] = {a.expr};
    return Expr::MakeFunction(FloorFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr make_f32x4(ValueExpr a, ValueExpr b) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr};
    return Expr::MakeFunction(MakeF32X4_1_1_FnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr make_f32x4(ValueExpr a, ValueExpr b, ValueExpr c, ValueExpr d) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr, c.expr, d.expr};
    return Expr::MakeFunction(MakeF32X4_1_1_1_1_FnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr make_f32x4(ValueExpr a, ValueExpr b, ValueExpr c) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr, c.expr};
    return Expr::MakeFunction(MakeF32X4_1_1_1_FnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr make_f32x2(ValueExpr a, ValueExpr b) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr};
    return Expr::MakeFunction(MakeF32X2_1_1_FnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr make_f32x3(ValueExpr x, ValueExpr y, ValueExpr z) {
    SharedPtr<Expr> argv[] = {x.expr, y.expr, z.expr};
    return Expr::MakeFunction(MakeF32X3_1_1_1_FnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr make_f32x3(ValueExpr x, ValueExpr y) {
    SharedPtr<Expr> argv[] = {x.expr, y.expr};
    return Expr::MakeFunction(MakeF32X3_1_1_FnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr make_u32x3(ValueExpr x, ValueExpr y, ValueExpr z) {
    SharedPtr<Expr> argv[] = {x.expr, y.expr, z.expr};
    return Expr::MakeFunction(MakeU32X3_1_1_1_FnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr make_u32x3(ValueExpr x, ValueExpr y) {
    SharedPtr<Expr> argv[] = {x.expr, y.expr};
    return Expr::MakeFunction(MakeU32X3_1_1_FnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr Interpolate(ValueExpr a, ValueExpr b, ValueExpr c, ValueExpr bary) {
    SharedPtr<Expr> argv[] = {a.expr, b.expr, c.expr, bary.expr};
    return Expr::MakeFunction(InterpolateFnTy, argv, SJIT_ARRAYSIZE(argv));
}
static ValueExpr GetTBN(ValueExpr N) {
    SharedPtr<Expr> argv[] = {N.expr};
    return Expr::MakeFunction(GetTBNFnTy, argv, SJIT_ARRAYSIZE(argv));
}
template <typename T>
static SharedPtr<Type> GetType() {
    SJIT_TRAP;
}
#    define DECLARE_TYPE_ENTRY(t)                                                                                                                                                  \
        template <>                                                                                                                                                                \
        SharedPtr<Type> GetType<t>() {                                                                                                                                             \
            return t##Ty;                                                                                                                                                          \
        }

DECLARE_TYPE_ENTRY(f32);
DECLARE_TYPE_ENTRY(f32x2);
DECLARE_TYPE_ENTRY(f32x3);
DECLARE_TYPE_ENTRY(f32x4);
DECLARE_TYPE_ENTRY(f32x4x4);
DECLARE_TYPE_ENTRY(u32);
DECLARE_TYPE_ENTRY(u32x2);
DECLARE_TYPE_ENTRY(u32x3);
DECLARE_TYPE_ENTRY(u32x4);
DECLARE_TYPE_ENTRY(i32);
DECLARE_TYPE_ENTRY(i32x2);
DECLARE_TYPE_ENTRY(i32x3);
DECLARE_TYPE_ENTRY(i32x4);

#    undef DECLARE_TYPE_ENTRY

template <typename T>
static ValueExpr MakeStaticArray(std::initializer_list<T> l) {
    SharedPtr<Expr> e = Expr::MakeArray(GetType<T>(), u32(l.size()));

    GetGlobalModule().GetBody().EmitF("%s %s[%i] = {\n", GetType<T>()->GetName().c_str(), e->name, u32(l.size()));
    for (auto t : l) {
        GetGlobalModule().GetBody() << t << ",\n";
    }
    GetGlobalModule().GetBody() << "};\n";
    return e;
}
template <typename T>
static ValueExpr MakeStaticArray(std::vector<T> l) {
    SharedPtr<Expr> e = Expr::MakeArray(GetType<T>(), u32(l.size()));

    GetGlobalModule().GetBody().EmitF("%s %s[%i] = {\n", GetType<T>()->GetName().c_str(), e->name, u32(l.size()));
    for (auto t : l) {
        GetGlobalModule().GetBody() << t << ",\n";
    }
    GetGlobalModule().GetBody() << "};\n";
    return e;
}
static ValueExpr EmitArray(SharedPtr<Type> _type, u32 _num) {
    SharedPtr<Expr> e = Expr::MakeArray(_type, _num);
    GetGlobalModule().GetBody().EmitF("%s %s[%i];\n", _type->GetName().c_str(), e->name, _num);
    return e;
}

static ValueExpr operator^(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_BIT_XOR)}; }
static ValueExpr operator+(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_PLUS)}; }
static ValueExpr operator-(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_MINUS)}; }
static ValueExpr operator-(ValueExpr const &a) { return {Expr::MakeOp(NULL, a.expr, OP_MINUS)}; }
static ValueExpr operator+(ValueExpr const &a) { return {Expr::MakeOp(NULL, a.expr, OP_PLUS)}; }
static ValueExpr operator*(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_MUL)}; }
static ValueExpr operator/(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_DIV)}; }
static ValueExpr operator<(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_LESS)}; }
static ValueExpr operator<=(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_LESS_OR_EQUAL)}; }
static ValueExpr operator>(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_GREATER)}; }
static ValueExpr operator>=(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_GREATER_OR_EQUAL)}; }
static ValueExpr operator&&(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_LOGICAL_AND)}; }
static ValueExpr operator||(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_LOGICAL_OR)}; }
static ValueExpr operator&(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_BIT_AND)}; }
static ValueExpr operator|(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_BIT_OR)}; }
static ValueExpr operator<<(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_SHIFT_LEFT)}; }
static ValueExpr operator>>(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_SHIFT_RIGHT)}; }
static ValueExpr operator==(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_EQUAL)}; }
static ValueExpr operator!=(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_NOT_EQUAL)}; }
static ValueExpr operator%(ValueExpr const &a, ValueExpr const &b) { return {Expr::MakeOp(a.expr, b.expr, OP_MODULO)}; }

static ValueExpr square(ValueExpr a) { return a * a; }
static ValueExpr var_f32x3_splat(ValueExpr p) { return make_f32x3(p, p, p); }
// clang-format off

//template<typename T> ValueExpr   operator+(ValueExpr a, T b) { return {Expr::MakeOp(a.expr,  ValueExpr(b).expr, OP_PLUS)}; }
//template<typename T> ValueExpr   operator-(ValueExpr a, T b) { return {Expr::MakeOp(a.expr,  ValueExpr(b).expr, OP_MINUS)}; }
//template<typename T> ValueExpr   operator*(ValueExpr a, T b) { return {Expr::MakeOp(a.expr,  ValueExpr(b).expr, OP_MUL)}; }
//template<typename T> ValueExpr   operator/(ValueExpr a, T b) { return {Expr::MakeOp(a.expr,  ValueExpr(b).expr, OP_DIV)}; }
//template<typename T> ValueExpr   operator<(ValueExpr a, T b) { return {Expr::MakeOp(a.expr,  ValueExpr(b).expr, OP_LESS)}; }
//template<typename T> ValueExpr   operator<=(ValueExpr a, T b) { return {Expr::MakeOp(a.expr, ValueExpr(b).expr, OP_LESS_OR_EQUAL)}; }
//template<typename T> ValueExpr   operator>(ValueExpr a, T b) { return {Expr::MakeOp(a.expr,  ValueExpr(b).expr, OP_GREATER)}; }
//template<typename T> ValueExpr   operator>=(ValueExpr a, T b) { return {Expr::MakeOp(a.expr, ValueExpr(b).expr, OP_GREATER_OR_EQUAL)}; }
//template<typename T> ValueExpr   operator&&(ValueExpr a, T b) { return {Expr::MakeOp(a.expr, ValueExpr(b).expr, OP_LOGICAL_AND)}; }
//template<typename T> ValueExpr   operator||(ValueExpr a, T b) { return {Expr::MakeOp(a.expr, ValueExpr(b).expr, OP_LOGICAL_OR)}; }
//template<typename T> ValueExpr   operator&(ValueExpr a, T b) { return {Expr::MakeOp(a.expr,  ValueExpr(b).expr, OP_BIT_AND)}; }
//template<typename T> ValueExpr   operator|(ValueExpr a, T b) { return {Expr::MakeOp(a.expr,  ValueExpr(b).expr, OP_BIT_OR)}; }
//template<typename T> ValueExpr   operator==(ValueExpr a, T b) { return {Expr::MakeOp(a.expr, ValueExpr(b).expr, OP_EQUAL)}; }
//template<typename T> ValueExpr   operator!=(ValueExpr a, T b) { return {Expr::MakeOp(a.expr, ValueExpr(b).expr, OP_NOT_EQUAL)}; }
//
//template<typename T> ValueExpr   operator+(T a, ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_PLUS)}; }
//template<typename T> ValueExpr   operator-(T a, ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_MINUS)}; }
//template<typename T> ValueExpr   operator*(T a, ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_MUL)}; }
//template<typename T> ValueExpr   operator/(T a, ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_DIV)}; }
//template<typename T> ValueExpr   operator<(T a, ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_LESS)}; }
//template<typename T> ValueExpr  operator<=(T a,ValueExpr b) { return  {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_LESS_OR_EQUAL)}; }
//template<typename T> ValueExpr   operator>(T a, ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_GREATER)}; }
//template<typename T> ValueExpr   operator>=(T  a,ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_GREATER_OR_EQUAL)}; }
//template<typename T> ValueExpr   operator&&(T  a,ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_LOGICAL_AND)}; }
//template<typename T> ValueExpr   operator||(T  a,ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_LOGICAL_OR)}; }
//template<typename T> ValueExpr   operator&(T a, ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_BIT_AND)}; }
//template<typename T> ValueExpr   operator|(T a, ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_BIT_OR)}; }
//template<typename T> ValueExpr   operator==(T  a,ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_EQUAL)}; }
//template<typename T> ValueExpr   operator!=(T  a,ValueExpr b) { return {Expr::MakeOp(ValueExpr(a).expr, b.expr, OP_NOT_EQUAL)}; }

// clang-format on

static HLSLModule &operator<<(HLSLModule &m, ValueExpr &a) {
    a->EmitHLSL(m);
    return m;
}
static void EmitForLoop(ValueExpr _begin, ValueExpr _end, std::function<void(ValueExpr)> _body) {
    auto     &body = GetGlobalModule().GetBody();
    ValueExpr iter = Zero(_begin->InferType()).Copy();
    body.EmitF("for (%s = %s; %s <= %s; %s++) {\n", iter->name, _begin->name, iter->name, _end->name, iter->name);
    // using var = ValueExpr;
    GetGlobalModule().EnterScope(Expr::MakeOp(iter.expr, _end.expr, OP_LESS_OR_EQUAL));
    _body(iter);
    GetGlobalModule().ExitScope();
    body.EmitF("}\n");
}
static void EmitWhileLoop(std::function<void()> _body) {
    auto &body = GetGlobalModule().GetBody();
    body.EmitF("while (true) {\n");
    // using var = ValueExpr;
    GetGlobalModule().EnterScope();
    _body();
    GetGlobalModule().ExitScope();
    body.EmitF("}\n");
}
static ValueExpr RayQueryTransparent(ValueExpr tlas, ValueExpr ray_desc, std::function<ValueExpr(ValueExpr)> _break) {
    auto     &body = GetGlobalModule().GetBody();
    ValueExpr w    = Make(RayQuery_Ty);
    tlas.EmitGlobalHLSL();
    ray_desc.EmitGlobalHLSL();
    body << "RayQuery<RAY_FLAG_NONE> ray_query;\n";
    body << "ray_query.TraceRayInline(" << tlas->name << ", RAY_FLAG_NONE, 0xffu, " << ray_desc->name << ");\n";
    body << "while (ray_query.Proceed()) {\n";
    GetGlobalModule().EnterScope();
    {
        body << "if (ray_query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE) {\n";
        GetGlobalModule().EnterScope();
        {
            ValueExpr tmp_w = Make(RayQuery_Ty);
            body << tmp_w->name << ".hit           = true;\n";
            body << tmp_w->name << ".bary          = ray_query.CandidateTriangleBarycentrics();\n";
            body << tmp_w->name << ".ray_t         = ray_query.CandidateTriangleRayT();\n";
            body << tmp_w->name << ".instance_id   = ray_query.CandidateInstanceID();\n";
            body << tmp_w->name << ".primitive_idx = ray_query.CandidatePrimitiveIndex();\n";
            ValueExpr do_break = _break(tmp_w);
            body << "if (" << do_break->name << ") { ray_query.CommitNonOpaqueTriangleHit(); }\n";
        }
        GetGlobalModule().ExitScope();
        body << "}\n";
    }
    GetGlobalModule().ExitScope();
    body << "}\n";

    body << "if (ray_query.CommittedStatus() != COMMITTED_NOTHING) {\n";
    body << w->name << ".hit           = true;\n";
    body << w->name << ".bary          = ray_query.CommittedTriangleBarycentrics();\n";
    body << w->name << ".ray_t         = ray_query.CommittedRayT();\n";
    body << w->name << ".instance_id   = ray_query.CommittedInstanceID();\n";
    body << w->name << ".primitive_idx = ray_query.CommittedPrimitiveIndex();\n";
    body << "}\n";

    return w;
}
static void EmitBreak() {
    sjit_assert(GetGlobalModule().IsInSwitch() == false && "That's just bad");
    auto &body = GetGlobalModule().GetBody();
    body.EmitF("break;\n");
}
static void EmitContinue() {
    auto &body = GetGlobalModule().GetBody();
    body.EmitF("continue;\n");
}
static void EmitReturn() {
    auto &body = GetGlobalModule().GetBody();
    body.EmitF("return;\n");
}
static void EmitReturn(ValueExpr e) {
    auto &body = GetGlobalModule().GetBody();
    body.EmitF("return %s;\n", e->name);
}
static void EmitGroupSync() { GetGlobalModule().GetBody().Write("GroupMemoryBarrierWithGroupSync();\n"); }
namespace wave32 {
static SharedPtr<Expr> GetInitialWave32MaskExpr() { return Expr::MakeLiteral(u32(0xffffffffu)); }
static ValueExpr       GetWave32Mask() { return GetGlobalModule().GetWave32Mask(); }
static void            EnableWave32MaskMode() {
    GetGlobalModule().SetWave32MaskMode();
    auto e = GetInitialWave32MaskExpr();
    e->EmitHLSL(GetGlobalModule());
    GetGlobalModule().PushWave32Mask(e);
}
static void EmitWhileLoop(std::function<void()> _body) {
    auto &body = GetGlobalModule().GetBody();

    using var = ValueExpr;
    GetGlobalModule().EnterScope();
    sjit_debug_assert(GetGlobalModule().IsWave32MaskMode());
    var cur_mask = GetWave32Mask().Copy();
    GetGlobalModule().PushWave32Mask(cur_mask.expr);
    body.EmitF("while (true) {\n");
    _body();
    body.EmitF("}\n");
    GetGlobalModule().PopWave32Mask();
    GetGlobalModule().ExitScope();
}
static void EmitIfLaneActive(std::function<void()> _if) {
    using var  = ValueExpr;
    auto &body = GetGlobalModule().GetBody();
    sjit_debug_assert(GetGlobalModule().IsWave32MaskMode());
    var cur_mask = GetWave32Mask().Copy();

    var lane_bit = LaneBit();

    var cond = Expr::MakeOp(Expr::MakeOp(cur_mask.expr, lane_bit.expr, OP_BIT_AND), Expr::MakeLiteral(u32(0)), OP_NOT_EQUAL);

    GetGlobalModule().EnterScope(cond.expr);

    body.EmitF("if (%s) {\n", cond->name);

    _if();

    body.EmitF("}\n");

    GetGlobalModule().ExitScope();
}
static void EmitIfElse(ValueExpr _cond, std::function<void()> _if, std::function<void()> _else = {}) {
    using var  = ValueExpr;
    auto &body = GetGlobalModule().GetBody();

    var if_mask   = var(u32(0));
    var else_mask = var(u32(0));
    var cur_mask  = GetWave32Mask().Copy();

    // sjit_assert(_cond->IsScalar());

    body.EmitF("%s = (WaveActiveBallot(%s).x) & %s;\n", if_mask->name, _cond->name, cur_mask->name);
    body.EmitF("%s = (~WaveActiveBallot(%s).x) & %s;\n", else_mask->name, _cond->name, cur_mask->name);

    GetGlobalModule().PushWave32Mask(if_mask.expr);
    GetGlobalModule().EnterScope();

    body.EmitF("if (%s != u32(0)) {\n", if_mask->name);

    _if();

    if (_else) {
        GetGlobalModule().PopWave32Mask();
        GetGlobalModule().PushWave32Mask(else_mask.expr);
        GetGlobalModule().ExitScope();
        GetGlobalModule().EnterScope();
        body.EmitF("} else if (%s != u32(0)) {\n", else_mask->name);
        _else();
    }

    body.EmitF("}\n");
    GetGlobalModule().ExitScope();
    GetGlobalModule().PopWave32Mask();
}
} // namespace wave32
static void EmitIfElse(ValueExpr _cond, std::function<void()> _if, std::function<void()> _else = {}) {
    // using var  = ValueExpr;
    auto &body = GetGlobalModule().GetBody();

    body.EmitF("if (%s) {\n", _cond->name);
    GetGlobalModule().EnterScope();
    _if();

    if (_else) {
        GetGlobalModule().ExitScope();
        GetGlobalModule().EnterScope();
        body.EmitF("} else {\n");
        _else();
    }

    body.EmitF("}\n");
    GetGlobalModule().ExitScope();
}
static void EmitSwitchCase(ValueExpr _val, Array<std::pair<u32, std::function<void()>>> const &_cases) {
    // using var  = ValueExpr;
    auto &body = GetGlobalModule().GetBody();

    body.EmitF("switch (%s) {\n", _val->name);
    GetGlobalModule().EnterScope();
    GetGlobalModule().EnterSwitchScope();

    for (auto const &_case : _cases) {
        body.EmitF("case %i: {\n", _case.first);
        GetGlobalModule().EnterScope();
        _case.second();
        GetGlobalModule().ExitScope();
        body.EmitF("break; }\n");
    }

    body.EmitF("}\n");
    GetGlobalModule().ExitSwitchScope();
    GetGlobalModule().ExitScope();
}
using var = ValueExpr;
static ValueExpr MakeIfElse(ValueExpr _cond, ValueExpr _if, ValueExpr _else) { return ValueExpr(Expr::MakeIfElse(_cond.expr, _if.expr, _else.expr)); }
static ValueExpr AllocateLDS(SharedPtr<Type> _type, u32 _num_elems, String const &_name) {
    SharedPtr<Type> lds_type = _type;
    if (_num_elems > u32(1)) {
        char buf[0x100];
        sprintf_s(buf, "%s[%i]", _type->GetName().c_str(), _num_elems);
        lds_type = Type::CreateArray(buf, _type, _num_elems);
    }
    GetGlobalModule().AddLDS(_name, lds_type);
    return ValueExpr(Expr::MakeRef(_name, lds_type));
}
static ValueExpr EmitBinarySearch(ValueExpr _buffer, ValueExpr _num_items, ValueExpr _offset) {
    // auto &body = GetGlobalModule().GetBody();
    var result = var(u32(0));
    GetGlobalModule().EnterScope();
    {
        var b = var(u32(0));
        var e = _num_items.Copy();

        EmitWhileLoop([&] {
            EmitIfElse((e.ToI32() - b.ToI32()) <= var(i32(1)), [] { EmitBreak(); });
            var m = (b + e) / var(u32(2));
            EmitIfElse(
                _buffer.Read(m) > _offset, [&] { e = m; }, [&] { b = m; });
        });
        result = b;
    }
    GetGlobalModule().ExitScope();

    return result;
}
static ValueExpr PackFp16x2ToU32(ValueExpr v) {
    ValueExpr x = v.x().f16_to_u32();
    ValueExpr y = v.y().f16_to_u32();
    return (x | (y << u32(16)));
}
static ValueExpr UnpackU32ToF16x2(ValueExpr _v) {
    ValueExpr x = (_v & u32(0xffff)).u32_to_f16();
    ValueExpr y = ((_v >> u32(16)) & u32(0xffff)).u32_to_f16();
    ValueExpr v = Make(f16x2Ty);
    v.x()       = x;
    v.y()       = y;
    return v;
}
static ValueExpr GetLuminance(ValueExpr v) { return max(f32(1.0e-3), dot(v, f32x3(0.299, 0.587, 0.114))); }
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
    static var SampleNormal(var view_direction, var normal, var roughness, var xi) {
        var o = Make(f32x4Ty);
        EmitIfElse(
            roughness < f32(0.001), //
            [&] {
                o.xyz() = normal;
                o.w()   = f32(1.0); // ? pdf of a nearly mirror like reflection
            },                      //
            [&] {
                var tbn_transform      = transpose(GetTBN(normal));
                var view_direction_tbn = mul(-view_direction, tbn_transform);
                var a                  = roughness * roughness;
                var a2                 = a * a;
                var sampled_normal_tbn = SampleGGXVNDF(view_direction_tbn, a, a, xi.x(), xi.y());
                var inv_tbn_transform  = transpose(tbn_transform);
                o.xyz()                = mul(sampled_normal_tbn, inv_tbn_transform);

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
#    if defined(__dxgiformat_h__)
static u32 GetBytesPerPixel(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM: //
        return u32(1);
    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT: //
        return u32(2);
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT: //
        return u32(4);
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT: //
        return u32(8);
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: //
        return u32(16);
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_TYPELESS: //
        return u32(12);
    default: break;
    }
    SJIT_TRAP;
}
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
    default: return BASIC_TYPE_UNKNOWN;
    }
}
#    endif // defined(__dxgiformat_h__)

#    define GFX_JIT_MAKE_RESOURCE(name, type) var name = ResourceAccess(Resource::Create(type, #    name))
#    define GFX_JIT_MAKE_GLOBAL_RESOURCE(name, type) static var name = ResourceAccess(Resource::Create(type, #    name))
#    define GFX_JIT_MAKE_GLOBAL_RESOURCE_ARRAY(name, type) static var name = ResourceAccess(Resource::CreateArray(Resource::Create(type, "elem_" #    name), #    name))

static u32 LSB(u32 v) {
    static const int MultiplyDeBruijnBitPosition[32] = {0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};
    return u32(MultiplyDeBruijnBitPosition[((uint32_t)((v & -v) * 0x077CB531U)) >> 27]);
}

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
};
        
#define BILINEAR_WEIGHTS(frac_uv)                                 \
    var bilinear_weights[2][2] =                                  \
    {                                                             \
            {(f32(1.0) - frac_uv.x()) * (f32(1.0) - frac_uv.y()), \
            (frac_uv.x()) * (f32(1.0) - frac_uv.y())},            \
            {(f32(1.0) - frac_uv.x()) * (frac_uv.y()),            \
            (frac_uv.x()) * (frac_uv.y())},                       \
    };

// clang-format on

// https://www.shadertoy.com/view/4dsSzr

static ValueExpr hueGradient(ValueExpr t) {
    sjit_assert(t->InferType() == f32Ty);
    ValueExpr p = abs(frac(t["xxx"] + f32x3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * f32(6.0) - f32x3_splat(3.0));
    return clamp(p - f32x3_splat(1.0), f32x3_splat(0.0), f32x3_splat(1.0));
}

// https://www.shadertoy.com/view/ltB3zD

// Gold Noise 2015 dcerisano@standard3d.com
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
static var EncodeGBuffer32Bits(var N, var P, var xi, var camera_pos) {
    var on_16_bits = Octahedral::EncodeNormalTo16Bits(N);
    var dist       = length(P - camera_pos);
    var idist      = f32(1.0) / (f32(1.0) + dist);
    idist += (xi * f32(2.0) - f32(1.0)) * f32(1.0e-4);
    var idist_16_bits = idist.ToF16().f16_to_u32();
    var pack          = on_16_bits | (idist_16_bits << u32(16));
    return pack;
}
static SharedPtr<Type> GBuffer_Ty = Type::Create("GBuffer", {
                                                                {"P", f32x3Ty}, //
                                                                {"N", f32x3Ty}, //
                                                            });
static var             DecodeGBuffer32Bits(var camera_ray, var pack, var xi) {
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
static SharedPtr<Type> Hit_Ty = Type::Create("Hit", {
                                                        {"W", f32x3Ty},  //
                                                        {"N", f32x3Ty},  //
                                                        {"UV", f32x2Ty}, //
                                                    });
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
} // namespace SJIT

#endif // JIT_HPP