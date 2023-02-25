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

#if !defined(UTILS_HPP)
#    define UTILS_HPP

#    include "common.h"

#    include <map>
#    include <string>
#    include <unordered_map>
#    include <vector>

//#    undef min
//#    undef max

static u32 enable_fpe() {
    u32 fe_value  = ~(    //
        _EM_INVALID |    //
        _EM_DENORMAL |   //
        _EM_ZERODIVIDE | //
        _EM_OVERFLOW |   //
        _EM_UNDERFLOW |  //
        //_EM_INEXACT |    //
        0 //
    );
    u32 mask      = _MCW_EM;
    u32 old_state = 0;
    _clearfp();
    errno_t result = _controlfp_s(&old_state, fe_value, mask);
    (void)result;
    assert(result == 0);
    return old_state;
}

static u32 disable_fpe() {
    u32 fe_value  = ~(0);
    u32 mask      = _MCW_EM;
    u32 old_state = 0;
    _clearfp();
    errno_t result = _controlfp_s(&old_state, fe_value, mask);
    (void) result;
    assert(result == 0);
    return old_state;
}

static void restore_fpe(u32 new_mask) {
    u32 mask     = _MCW_EM;
    u32 old_mask = 0;
    _clearfp();
    errno_t result = _controlfp_s(&old_mask, new_mask, mask);
    (void)result;
    assert(result == 0);
}

class OffsetAllocator {
public:
    struct Allocation {
        u32 offset;
        u32 size;

        bool IsValid() const { return offset != u32(-1); }
        bool operator<(Allocation const &that) const { return this->offset < that.offset; }
    };

private:
    u32 size = u32(0);
    // u32                     cursor      = u32(0);
    std::map<u32, u32> free_ranges = {};
    u32                free_space  = u32(0);

public:
    void Init(u32 _size) {
        size           = _size;
        free_space     = _size;
        free_ranges[0] = size;
    }
    Allocation Allocate(u32 needed_size, u32 alignment = u32(1)) {
        alignment = std::max(u32(1), alignment);
        assert((alignment & (alignment - u32(1))) == u32(0));
        assert(needed_size);
        /* {
             u32 aligned_cursor = ((cursor + alignment) & (~(alignment - u32(1))));
             if (size - aligned_cursor >= needed_size) {
                 if (aligned_cursor > cursor) {
                     free_ranges.push_back({cursor, aligned_cursor - cursor});
                 }
                 free_space -= needed_size;
                 cursor = aligned_cursor += needed_size;
                 return Allocation{aligned_cursor, needed_size};
             }
         }*/
        /*ifor(free_ranges.size()) {
            u32 aligned_offset = ((free_ranges[i].offset + alignment) & (~(alignment - u32(1))));
            u32 end            = free_ranges[i].offset + free_ranges[i].size;
            if (end > aligned_offset) {
                u32 resulting_size = end - aligned_offset;
                if (resulting_size >= needed_size) {
                    free_ranges[i].size = aligned_offset - free_ranges[i].offset;
                    if (resulting_size > needed_size) {
                        free_ranges.push_back({aligned_offset + needed_size, resulting_size - needed_size});
                    }
                    return Allocation{aligned_offset, needed_size};
                }
            }
        }*/
        if (free_ranges.size() == 0) return Allocation{u32(-1)};
        i32 offset = -1;
        for (auto iter = free_ranges.begin(); iter != free_ranges.end(); iter++) {
            i32 end            = iter->first + iter->second;
            i32 aligned_offset = (iter->first + alignment - 1) & (~(alignment - 1));
            if (aligned_offset >= end) continue;
            i32 usable_space = end - aligned_offset;
            if (usable_space >= (i32)needed_size) {
                offset         = aligned_offset;
                i32 key_offset = iter->first;

                if (aligned_offset != key_offset) {
                    i32 diff = aligned_offset - key_offset;
                    assert(diff < (i32)iter->second && diff > 0);
                    iter->second = diff;
                } else {
                    free_ranges.erase(iter);
                }
                u64 new_size = usable_space - needed_size;
                if (new_size) {
                    free_ranges[aligned_offset + needed_size] = u32(new_size);
                }
                free_space -= needed_size;
                break;
            }
        }
        return Allocation{u32(offset), needed_size};
    }
    bool CanAllocate(u32 needed_size, u32 alignment = u32(1)) {
        alignment = std::max(u32(1), alignment);
        assert((alignment & (alignment - u32(1))) == u32(0));
        assert(needed_size);
        if (free_ranges.size() == 0) return false;
        for (auto iter = free_ranges.begin(); iter != free_ranges.end(); iter++) {
            u64 end            = iter->first + iter->second;
            u64 aligned_offset = (iter->first + alignment - 1) & (~(alignment - 1));
            if (aligned_offset >= end) continue;
            u64 usable_space = end - aligned_offset;
            if (usable_space >= needed_size) {
                return true;
            }
        }
        return false;
    }
    u32 GetSpaceLeft() const { return free_space; }
    /*void Defrag() {
        if (free_ranges.size() == u64(0)) return;
        std::sort(free_ranges.begin(), free_ranges.end());
        std::vector<Allocation> new_free_ranges = {};
        Allocation              cur             = {u32(-1)};
        ifor(free_ranges.size()) {
            if (cur.offset == u32(-1)) {
                cur = free_ranges[i];
            } else if (cur.offset + cur.size == free_ranges[i].offset) {
                cur.size += free_ranges[i].size;
            } else {
                new_free_ranges.push_back(cur);
                cur = free_ranges[i];
            }
        }
        if (cur.offset != u32(-1)) {
            new_free_ranges.push_back(cur);
        }
        free_ranges = new_free_ranges;
    }*/
    void Free(Allocation const &allocation) {
        assert(contains(free_ranges, allocation.offset) == false);
        free_space += allocation.size;
        // free_ranges.push_back(allocation);

        if (free_ranges.size() == u64(0)) {
            free_ranges[allocation.offset] = size;
            return;
        }
        u32 new_offset = allocation.offset;
        u32 new_size   = allocation.size;

        {
            auto iter = free_ranges.upper_bound(new_offset);
            if (iter != free_ranges.end() && iter != free_ranges.begin()) {
                iter--;
                if ((u32)iter->first + (u32)iter->second == new_offset) {
                    new_offset = iter->first;
                    new_size   = iter->second + new_size;
                    free_ranges.erase(iter);
                }
            }
        }
        {
            auto iter = free_ranges.upper_bound(new_offset);
            if (iter != free_ranges.end()) {
                if (iter->first == (u32)new_offset + (u32)new_size) {
                    new_size = iter->second + new_size;
                    free_ranges.erase(iter);
                }
            }
        }

        free_ranges[new_offset] = new_size;
    }
    void Flush() {
        // cursor = u32(0);
        free_ranges.clear();
        free_space     = size;
        free_ranges[0] = size;
    }
    void Release() { *this = {}; }

    static void Test() {
        OffsetAllocator offset_allocator;
        offset_allocator.Init(u32(128 << 20));
        defer(offset_allocator.Release());

        {
            u32 N = 1 << 10;

            std::vector<OffsetAllocator::Allocation> allocations = {};

            allocations.resize(N);

            u32 total_allocated = u32(0);
            (void)total_allocated;
            ifor(N) {
                u32 size = u32(256) << u32(rand() % u32(4));
                total_allocated += size;
                OffsetAllocator::Allocation a = offset_allocator.Allocate(size, 256);
                allocations[i]                = a;
            }

            assert(offset_allocator.GetSpaceLeft() == u32((128 << 20) - total_allocated));

            ifor(N) { offset_allocator.Free(allocations[i]); }

            assert(offset_allocator.GetSpaceLeft() == u32(128 << 20));
        }
    }
};

#    if __linux__
static inline size_t get_page_size() { return sysconf(_SC_PAGE_SIZE); }
#    elif WIN32
static inline size_t get_page_size() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
}
#    else
static inline size_t get_page_size() { return 1 << 12; }
#    endif
static inline size_t page_align_up(size_t n) { return (n + get_page_size() - 1) & (~(get_page_size() - 1)); }
static inline size_t page_align_down(size_t n) { return (n) & (~(get_page_size() - 1)); }
static inline size_t get_num_pages(size_t size) { return page_align_up(size) / get_page_size(); }
#    if __linux__
static inline void protect_pages(void *ptr, size_t num_pages) { mprotect(ptr, num_pages * get_page_size(), PROT_NONE); }
static inline void unprotect_pages(void *ptr, size_t num_pages, bool exec = false) { mprotect(ptr, num_pages * get_page_size(), PROT_WRITE | PROT_READ | (exec ? PROT_EXEC : 0)); }
static inline void unmap_pages(void *ptr, size_t num_pages) {
    int err = munmap(ptr, num_pages * get_page_size());
    ASSERT_ALWAYS(err == 0);
}
static inline void map_pages(void *ptr, size_t num_pages) {
    void *new_ptr = mmap(ptr, num_pages * get_page_size(), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    ASSERT_ALWAYS((size_t)new_ptr == (size_t)ptr);
}
#    elif WIN32
// TODO
static inline void protect_pages(void *ptr, size_t num_pages) {}
static inline void unprotect_pages(void *ptr, size_t num_pages, bool exec = false) {}
static inline void unmap_pages(void *ptr, size_t num_pages) {}
static inline void map_pages(void *ptr, size_t num_pages) {}
#    else
// Noops
static inline void protect_pages(void *ptr, size_t num_pages) {}
static inline void unprotect_pages(void *ptr, size_t num_pages, bool exec = false) {}
static inline void unmap_pages(void *ptr, size_t num_pages) {}
static inline void map_pages(void *ptr, size_t num_pages) {}
#    endif

static inline double time() { return ((double)clock()) / CLOCKS_PER_SEC; }

#    define ASSERT_ALWAYS(x)                                                                                                                                                       \
        do {                                                                                                                                                                       \
            if (!(x)) {                                                                                                                                                            \
                fprintf(stderr, "%s:%i [FAIL] at %s\n", __FILE__, __LINE__, #x);                                                                                                   \
                abort();                                                                                                                                                           \
            }                                                                                                                                                                      \
        } while (0)

#    define ASSERT_PANIC(x) ASSERT_ALWAYS(x)
#    define NOTNULL(x) ASSERT_ALWAYS((x) != NULL)

template <typename T = u8>
struct Pool {
    u8 *ptr            = NULL;
    u64 cursor         = u64(0);
    u64 capacity       = u64(0);
    u64 mem_length     = u64(0);
    u64 stack_capacity = u64(0);
    u64 stack_cursor   = u64(0);

    static Pool create(size_t capacity) {
        assert(capacity > 0);
        Pool   out;
        size_t STACK_CAPACITY = 0x20 * sizeof(size_t);
        out.mem_length        = get_num_pages(STACK_CAPACITY + capacity * sizeof(T)) * get_page_size();
#    if __linux__
        out.ptr = (uint8_t *)mmap(NULL, out.mem_length, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        ASSERT_ALWAYS(out.ptr != MAP_FAILED);
#    else
        out.ptr = (uint8_t *)malloc(out.mem_length);
        NOTNULL(out.ptr);
#    endif
        out.capacity       = capacity;
        out.cursor         = 0;
        out.stack_capacity = STACK_CAPACITY;
        out.stack_cursor   = 0;
        return out;
    }

    T *back() { return (T *)(this->ptr + this->stack_capacity + this->cursor * sizeof(T)); }

    void advance(size_t size) {
        this->cursor += size;
        assert(this->cursor < this->capacity);
    }

    void Release() {
#    if __linux__
        if (this->ptr) munmap(this->ptr, mem_length);
#    else
        if (this->ptr) free(this->ptr);
#    endif
        memset(this, 0, sizeof(Pool));
    }

    void push(T const &v) {
        T *ptr = alloc(1);
        memcpy(ptr, &v, sizeof(T));
    }

    bool has_items() { return this->cursor > 0; }

    T *at(uint32_t i) { return (T *)(this->ptr + this->stack_capacity + i * sizeof(T)); }

    T *alloc(size_t size) {
        assert(size != 0);
        T *ptr = (T *)(this->ptr + this->stack_capacity + this->cursor * sizeof(T));
        this->cursor += size;
        assert(this->cursor < this->capacity);
        return ptr;
    }

    T *try_alloc(size_t size) {
        assert(size != 0);
        if (this->cursor + size > this->capacity) return NULL;
        T *ptr = (T *)(this->ptr + this->stack_capacity + this->cursor * sizeof(T));
        this->cursor += size;
        assert(this->cursor < this->capacity);
        return ptr;
    }

    T *alloc_zero(size_t size) {
        T *mem = alloc(size);
        memset(mem, 0, size * sizeof(T));
        return mem;
    }

    T *alloc_align(size_t size, size_t alignment) {
        T *ptr = alloc(size + alignment);
        ptr    = (T *)(((size_t)ptr + alignment - 1) & (~(alignment - 1)));
        return ptr;
    }

    T *alloc_page_aligned(size_t size) {
        assert(size != 0);
        size           = page_align_up(size) + get_page_size();
        T *ptr         = (T *)(this->ptr + this->stack_capacity + this->cursor * sizeof(T));
        T *aligned_ptr = (T *)(void *)page_align_down((size_t)ptr + get_page_size());
        this->cursor += size;
        assert(this->cursor < this->capacity);
        return aligned_ptr;
    }

    void enter_scope() {
        // Save the cursor to the stack
        size_t *top = (size_t *)(this->ptr + this->stack_cursor);
        *top        = this->cursor;
        // Increment stack cursor
        this->stack_cursor += sizeof(size_t);
        assert(this->stack_cursor < this->stack_capacity);
    }

    void exit_scope() {
        // Decrement stack cursor
        assert(this->stack_cursor >= sizeof(size_t));
        this->stack_cursor -= sizeof(size_t);
        // Restore the cursor from the stack
        size_t *top  = (size_t *)(this->ptr + this->stack_cursor);
        this->cursor = *top;
    }

    void Reset() {
        this->cursor       = 0;
        this->stack_cursor = 0;
    }

    T *put(T const *old_ptr, size_t count) {
        T *new_ptr = alloc(count);
        memcpy(new_ptr, old_ptr, count * sizeof(T));
        return new_ptr;
    }
    void pop() {
        assert(cursor > 0);
        cursor -= 1;
    }
    bool has_space(size_t size) { return cursor + size <= capacity; }
};

template <typename T = u8>
using Temporary_Storage = Pool<T>;

#    include <string.h>

#    ifndef UTILS_TL_TMP_SIZE
#        define UTILS_TL_TMP_SIZE 1 << 26
#    endif

struct Thread_Local {
    Temporary_Storage<> temporary_storage;
    bool                initialized = false;
    ~Thread_Local() { temporary_storage.Release(); }
#    ifdef UTILS_TL_IMPL_DEBUG
    i64 allocated = 0;
#    endif

    static Thread_Local *get_tl() {
        // TODO(aschrein): Change to __thread?
        static thread_local Thread_Local g_tl{};
        if (g_tl.initialized == false) {
            g_tl.initialized       = true;
            g_tl.temporary_storage = Temporary_Storage<>::create(UTILS_TL_TMP_SIZE);
        }
        return &g_tl;
    }
};
template <typename T = u8>
T *tl_alloc_tmp(u64 num = u64(1)) {
    assert(num > u64(0));
    return (T *)Thread_Local::get_tl()->temporary_storage.alloc(num * sizeof(T));
}
template <typename T = u8>
T *tl_alloc_tmp_init(u64 num = u64(1)) {
    assert(num > u64(0));
    T *obj = (T *)Thread_Local::get_tl()->temporary_storage.alloc(num * sizeof(T));
    ifor(num) { new (&obj[i]) T(); }
    return obj;
}
void  tl_alloc_tmp_enter() { Thread_Local::get_tl()->temporary_storage.enter_scope(); }
void  tl_alloc_tmp_exit() { Thread_Local::get_tl()->temporary_storage.exit_scope(); }
void *tl_alloc(size_t size) {
#    ifdef UTILS_TL_IMPL_DEBUG
    Thread_Local::get_tl()->allocated += (i64)size;
    void *ptr          = malloc(size + sizeof(size_t));
    ((size_t *)ptr)[0] = size;
    return ((u8 *)ptr + 8);
#    elif UTILS_TL_IMPL_TRACY
    ZoneScopedS(16);
    void *ptr = malloc(size);
    TracyAllocS(ptr, size, 16);
    return ptr;
#    else
    return malloc(size);
#    endif
}

static inline void *_tl_realloc(void *ptr, size_t oldsize, size_t newsize) {
    return realloc(ptr, newsize);
#    if 0
  if (oldsize == newsize) return ptr;
  size_t min_size = oldsize < newsize ? oldsize : newsize;
  void * new_ptr  = NULL;
  if (newsize != 0) new_ptr = malloc(newsize);
  if (min_size != 0) {
    memcpy(new_ptr, ptr, min_size);
  }
  if (ptr != NULL) free(ptr);
  return new_ptr;
#    endif
}

#    define TMP_STORAGE_SCOPE                                                                                                                                                      \
        tl_alloc_tmp_enter();                                                                                                                                                      \
        defer(tl_alloc_tmp_exit(););

#    ifdef UTILS_TL_IMPL_DEBUG
static inline void assert_tl_alloc_zero() {
    ASSERT_ALWAYS(Thread_Local::get_tl()->allocated == 0);
    ASSERT_ALWAYS(Thread_Local::get_tl()->temporary_storage.cursor == 0);
    ASSERT_ALWAYS(Thread_Local::get_tl()->temporary_storage.stack_cursor == 0);
}
#    endif

void *tl_realloc(void *ptr, size_t oldsize, size_t newsize) {
#    ifdef UTILS_TL_IMPL_DEBUG
    if (ptr == NULL) {
        ASSERT_ALWAYS(oldsize == 0);
        return tl_alloc(newsize);
    }
    Thread_Local::get_tl()->allocated -= (i64)oldsize;
    Thread_Local::get_tl()->allocated += (i64)newsize;
    void *old_ptr = (u8 *)ptr - sizeof(size_t);
    ASSERT_ALWAYS(((size_t *)old_ptr)[0] == oldsize);
    void *new_ptr          = _tl_realloc(old_ptr, oldsize + sizeof(size_t), newsize + sizeof(size_t));
    ((size_t *)new_ptr)[0] = newsize;
    return ((u8 *)new_ptr + sizeof(size_t));
#    elif UTILS_TL_IMPL_TRACY
    ZoneScopedS(16);
    size_t minsize = MIN(oldsize, newsize);
    void  *new_ptr = NULL;
    if (newsize > 0) {
        new_ptr = malloc(newsize);
        TracyAllocS(new_ptr, newsize, 16);
        if (minsize > 0) {
            memcpy(new_ptr, ptr, minsize);
        }
    }
    TracyFreeS(ptr, 16);
    return new_ptr;
#    else
    return _tl_realloc(ptr, oldsize, newsize);
#    endif
}

void tl_free(void *ptr) {
#    ifdef UTILS_TL_IMPL_DEBUG
    size_t size = ((size_t *)((u8 *)ptr - sizeof(size_t)))[0];
    Thread_Local::get_tl()->allocated -= (i64)size;
    free(((u8 *)ptr - sizeof(size_t)));
    return;
#    elif UTILS_TL_IMPL_TRACY
    ZoneScopedS(16);
    TracyFreeS(ptr, 16);
    free(ptr);
#    else
    free(ptr);
#    endif
}

static inline char *read_file_tmp(char const *filename) {
    FILE *text_file = fopen(filename, "rb");
    if (text_file == NULL) return NULL;
    fseek(text_file, 0, SEEK_END);
    long fsize = ftell(text_file);
    fseek(text_file, 0, SEEK_SET);
    size_t size = (size_t)fsize;
    char  *data = (char *)tl_alloc_tmp((size_t)fsize + 1);
    fread(data, 1, (size_t)fsize, text_file);
    data[size] = '\0';
    fclose(text_file);
    return data;
}

static inline bool parse_decimal_int(char const *str, i32 len, int32_t *result) {
    i32 final = 0;
    i32 pow   = 1;
    i32 sign  = 1;
    i32 i     = 0;
    // parsing in reverse order
    for (; i < len; ++i) {
        switch (str[len - 1 - i]) {
        case '0': break;
        case '1': final += 1 * pow; break;
        case '2': final += 2 * pow; break;
        case '3': final += 3 * pow; break;
        case '4': final += 4 * pow; break;
        case '5': final += 5 * pow; break;
        case '6': final += 6 * pow; break;
        case '7': final += 7 * pow; break;
        case '8': final += 8 * pow; break;
        case '9': final += 9 * pow; break;
        // it's ok to have '-'/'+' as the first char in a string
        case '-': {
            if (i == len - 1)
                sign = -1;
            else
                return false;
            break;
        }
        case '+': {
            if (i == len - 1)
                sign = 1;
            else
                return false;
            break;
        }
        default: return false;
        }
        pow *= 10;
    }
    *result = sign * final;
    return true;
}

template <typename T>
static inline bool ParseFloat(char const *str, i32 len, T *result) {
    T   final = (T)0.0;
    i32 i     = 0;
    T   sign  = (T)1.0;
    if (str[0] == '-') {
        sign = (T)-1.0;
        i    = 1;
    }
    for (; i < len; ++i) {
        if (str[i] == '.') break;
        switch (str[i]) {
        case '\0': goto finish;
        case '0': final = final * (T)10.0; break;
        case '1': final = final * (T)10.0 + (T)1.0; break;
        case '2': final = final * (T)10.0 + (T)2.0; break;
        case '3': final = final * (T)10.0 + (T)3.0; break;
        case '4': final = final * (T)10.0 + (T)4.0; break;
        case '5': final = final * (T)10.0 + (T)5.0; break;
        case '6': final = final * (T)10.0 + (T)6.0; break;
        case '7': final = final * (T)10.0 + (T)7.0; break;
        case '8': final = final * (T)10.0 + (T)8.0; break;
        case '9': final = final * (T)10.0 + (T)9.0; break;
        default: return false;
        }
    }
    if (str[i] == '.') {
        i++;
        T pow = (T)1.0e-1;
        for (; i < len; ++i) {
            switch (str[i]) {
            case '\0': goto finish;
            case '0': break;
            case '1': final += (T)1.0 * pow; break;
            case '2': final += (T)2.0 * pow; break;
            case '3': final += (T)3.0 * pow; break;
            case '4': final += (T)4.0 * pow; break;
            case '5': final += (T)5.0 * pow; break;
            case '6': final += (T)6.0 * pow; break;
            case '7': final += (T)7.0 * pow; break;
            case '8': final += (T)8.0 * pow; break;
            case '9': final += (T)9.0 * pow; break;
            case 'f': goto finish;
            case 'F': goto finish;
            case 'e': goto parse_exponent;
            case 'E': goto parse_exponent;
            default: return false;
            }
            pow *= (T)1.0e-1;
        }

    parse_exponent:
        if (i < len) {
            if (str[i] != 'e' && str[i] != 'E') return false;
            i++;
            i32 exp = -1;
            i32 l   = 0;
            for (; l < (i32)len - i; l++)
                if (str[i + l] == 'f' || str[i + l] == 'F') break;
            if (!parse_decimal_int(str + i, l, &exp)) return false;
            final = final * std::pow((T)10.0, (T)exp);
        }
    }
finish:
    *result = sign * final;
    return true;
}

static double GetParseFloat(char const *str) {
    double out;
    ASSERT_ALWAYS(ParseFloat(str, (i32)strlen(str), &out));
    return out;
}

static int __test_float_parsing = [] {
    double EPS = 1.0e-6;
    ASSERT_ALWAYS(GetParseFloat("0.0") == 0.0);
    ASSERT_ALWAYS(GetParseFloat("0000000000.0000000000") == 0.0);
    ASSERT_ALWAYS(GetParseFloat("0.0000000000") == 0.0);
    ASSERT_ALWAYS(GetParseFloat("-0.0000000000") == 0.0);
    ASSERT_ALWAYS(int(GetParseFloat("1.12") * 100.0 * (1.0 + EPS)) == 112);
    ASSERT_ALWAYS(int(GetParseFloat("125.125") * 1000.0 * (1.0 + EPS)) == 125125);
    ASSERT_ALWAYS(int(GetParseFloat("-1.12e-1") * 1000.0 * (1.0 + EPS)) == -112);
    ASSERT_ALWAYS(int(GetParseFloat("-5") * (1.0 + EPS)) == -5);
    ASSERT_ALWAYS(int(GetParseFloat("-2") * (1.0 + EPS)) == -2);
    ASSERT_ALWAYS(int(GetParseFloat("1.12e+1") * 10.0 * (1.0 + EPS)) == 112);
    ASSERT_ALWAYS(int(GetParseFloat("1.12e+2") * 1.0 * (1.0 + EPS)) == 112);
    ASSERT_ALWAYS(int(GetParseFloat("1.12e+2f") * 1.0 * (1.0 + EPS)) == 112);
    ASSERT_ALWAYS(int(GetParseFloat("1.12e+2F") * 1.0 * (1.0 + EPS)) == 112);

    return 0;
}();

struct StringRef {
    const char *ptr = NULL;
    u64         len = u64(0);

    StringRef() = default;
    StringRef(char const *_ptr, u64 _len) : ptr(_ptr), len(_len) {}
    StringRef(char const *str) {
        ptr = str;
        len = strlen(str);
    }
    StringRef substr(size_t offset, size_t new_len) { return StringRef{ptr + offset, new_len}; }
    bool      eq(char const *str) const {
        size_t slen = strlen(str);
        if (ptr == NULL || str == NULL) return false;
        return len != slen ? false : strncmp(ptr, str, len) == 0 ? true : false;
    }
    std::string to_str() const { return std::string(ptr, ptr + len); }

    operator bool() const { return ptr && len; }
};

static inline uint64_t hash_of(u64 u) {
    u64 v = u * u64(3935559000370003845) + u64(2691343689449507681);
    v ^= v >> u64(21);
    v ^= v << u64(37);
    v ^= v >> u64(4);
    v *= u64(4768777513237032717);
    v ^= v << u64(20);
    v ^= v >> u64(41);
    v ^= v << u64(5);
    return v;
}

namespace std {
template <>
struct hash<StringRef> {
    u64 operator()(StringRef const &item) const {
        u64 hash = u64(5381);
        if (item.len == 0) return 0;
        ifor(item.len) {
            u64 v = u64(item.ptr[i]) * u64(3935559000370003845) + u64(2691343689449507681);
            hash  = hash ^ v;
        }
        return hash;
    }
};
}; // namespace std

static inline i32 str_match(char const *cur, char const *patt) {
    i32 i = i32(0);
    while (true) {
        if (cur[i] == '\0' || patt[i] == '\0') return i;
        if (cur[i] == patt[i]) {
            i++;
        } else {
            return i32(-1);
        }
    }
    return i32(-1);
}

static inline i32 str_find(char const *cur, size_t maxlen, char c) {
    size_t i = 0;
    while (true) {
        if (i == maxlen) return -1;
        if (cur[i] == '\0') return -1;
        if (cur[i] == c) {
            return (i32)i;
        }
        i++;
    }
}

// for printf
#    define STRF(str) (i32) str.len, str.ptr

struct String_Builder {
    Pool<char> tmp_buf = {};

    void      Init() { tmp_buf = Pool<char>::create(1 << 20); }
    void      Release() { tmp_buf.Release(); }
    void      Reset() { tmp_buf.Reset(); }
    StringRef GetStr() { return StringRef{(char const *)tmp_buf.at(0), tmp_buf.cursor}; }
    void      PutStr(StringRef str) { Putf("%.*s", STRF(str)); }
    i32       Putf(char const *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        i32 len = vsprintf(tmp_buf.back(), fmt, args);
        va_end(args);
        ASSERT_ALWAYS(len > 0);
        tmp_buf.advance(len);
        return len;
    }
    void PutChar(char c) { tmp_buf.put(&c, 1); }
};

/** String view of a static string
 */
static inline StringRef stref_s(char const *static_string, bool include_null = true) {
    if (static_string == NULL || static_string[0] == '\0') return StringRef{NULL, 0};
    assert(static_string != NULL);
    StringRef out;
    out.ptr = static_string;
    out.len = strlen(static_string);
    if (!include_null) out.len--;
    assert(out.len != 0);
    return out;
}

static inline bool operator==(StringRef a, StringRef b) {
    if (a.ptr == NULL || b.ptr == NULL) return false;
    return a.len != b.len ? false : strncmp(a.ptr, b.ptr, a.len) == 0 ? true : false;
}
#endif // UTILS_HPP
