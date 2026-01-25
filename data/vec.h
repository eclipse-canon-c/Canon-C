// data/vec.h
#ifndef CANON_DATA_VEC_H
#define CANON_DATA_VEC_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "core/memory.h"
#include "core/arena.h"
#include "core/scope.h"
#include "data/range.h"
#include "data/stringbuf.h"
#include "semantics/result.h"
#include "semantics/error.h"
#include "semantics/option.h"

/**
 * @file vec.h
 * @brief Bounded dynamic vectors with explicit caller-owned buffer
 *
 * Canon-C vector is a **bounded**, **type-safe**, **explicit ownership** container.
 *
 * Supports:
 *  - Stack, heap, or arena-backed buffers
 *  - Typed vectors via macro
 *  - Generic `void*` vector
 *  - Iterators, slices, and range integration
 *  - Optional writing to `stringbuf`
 *  - Optional safe access via `Option<T>`
 *
 * Design Principles:
 *  - Caller owns the buffer (stack, heap, arena, static)
 *  - Fixed capacity (no automatic growth)
 *  - Bounds-checked operations returning Result<T, Error>
 *  - Deterministic memory and performance
 *
 * Performance & Memory:
 *  - Push/Pop: O(1), memory: +sizeof(T) per element
 *  - Insert/Remove: O(n), memory: no extra allocations
 *  - Extend: O(k), memory: contiguous buffer usage for k elements
 *  - Iterators: O(1) per step
 *  - Slice/Subvector: O(1), no copy
 *  - Generic `void*` vector: +sizeof(void*) per element
 *  - Typed vectors: +sizeof(T) per element
 *  - Heap allocation: +capacity*sizeof(T)
 *  - Arena allocation: +capacity*sizeof(T) via arena
 */

/* ────────────────────────────────────────────────────────────────
   Result type for vector operations
   ──────────────────────────────────────────────────────────────── */
CANON_C_DEFINE_RESULT(bool, Error)

/* ────────────────────────────────────────────────────────────────
   Generic void* vector
   ──────────────────────────────────────────────────────────────── */
typedef struct {
    void** items;   ///< Caller-owned buffer
    size_t len;     ///< Current number of elements
    size_t capacity;///< Maximum elements
} vec_voidptr;

static inline vec_voidptr vec_voidptr_init(void** buffer, size_t capacity) {
    assert(buffer != NULL || capacity == 0);
    return (vec_voidptr){ .items = buffer, .len = 0, .capacity = capacity };
}

static inline vec_voidptr vec_voidptr_empty(void) { return (vec_voidptr){0}; }

static inline bool vec_voidptr_is_empty(const vec_voidptr* v) { return !v || v->len == 0; }
static inline bool vec_voidptr_is_full(const vec_voidptr* v) { return v && v->len >= v->capacity; }
static inline size_t vec_voidptr_len(const vec_voidptr* v) { return v ? v->len : 0; }
static inline size_t vec_voidptr_capacity(const vec_voidptr* v) { return v ? v->capacity : 0; }
static inline size_t vec_voidptr_remaining(const vec_voidptr* v) { return v ? (v->capacity - v->len) : 0; }

static inline bool vec_voidptr_get(const vec_voidptr* v, size_t i, void** out) {
    if (!v || !out || i >= v->len) return false;
    *out = v->items[i];
    return true;
}

static inline Option_voidptr vec_voidptr_get_option(const vec_voidptr* v, size_t i) {
    if (!v || i >= v->len) return option_none_voidptr();
    return option_some_voidptr(v->items[i]);
}

static inline void* vec_voidptr_get_unchecked(const vec_voidptr* v, size_t i) {
    assert(v && v->items && i < v->len);
    return v->items[i];
}

static inline result_bool_Error vec_voidptr_push(vec_voidptr* v, void* item) {
    if (!v || !v->items) return result_bool_Error_err(ERR_INVALID_ARG);
    if (v->len >= v->capacity) return result_bool_Error_err(ERR_CAPACITY_EXCEEDED);
    v->items[v->len++] = item;
    return result_bool_Error_ok(true);
}

static inline result_bool_Error vec_voidptr_pop(vec_voidptr* v, void** out) {
    if (!v || !out || !v->items) return result_bool_Error_err(ERR_INVALID_ARG);
    if (v->len == 0) return result_bool_Error_err(ERR_INVALID_STATE);
    *out = v->items[--v->len];
    return result_bool_Error_ok(true);
}

static inline void vec_voidptr_clear(vec_voidptr* v) { if (v) v->len = 0; }
static inline void** vec_voidptr_first(const vec_voidptr* v) { return (v && v->len>0)? &v->items[0]:NULL; }
static inline void** vec_voidptr_last(const vec_voidptr* v) { return (v && v->len>0)? &v->items[v->len-1]:NULL; }

/* Heap allocation */
static inline vec_voidptr vec_voidptr_alloc(size_t capacity) {
    if (capacity == 0) return vec_voidptr_empty();
    void** buf = (void**)malloc(capacity * sizeof(void*));
    if (!buf) return vec_voidptr_empty();
    return vec_voidptr_init(buf, capacity);
}

static inline void vec_voidptr_free(vec_voidptr* v) {
    if (v && v->items) { free(v->items); v->items=NULL; v->len=0; v->capacity=0; }
}

/* Arena allocation */
static inline vec_voidptr vec_voidptr_arena_alloc(Arena* arena, size_t capacity) {
    if (!arena || capacity==0) return vec_voidptr_empty();
    void** buf = arena_alloc_array(arena, void*, capacity);
    return vec_voidptr_init(buf, capacity);
}

/* Iterators */
typedef struct { vec_voidptr* vec; size_t index; } vec_voidptr_iter;
static inline vec_voidptr_iter vec_voidptr_iter_init(vec_voidptr* v) { return (vec_voidptr_iter){ .vec=v, .index=0 }; }
static inline bool vec_voidptr_iter_next(vec_voidptr_iter* it, void** out) {
    if (!it||!it->vec||!out) return false;
    if (it->index >= it->vec->len) return false;
    *out = it->vec->items[it->index++];
    return true;
}

/* Slice */
typedef struct { void** items; size_t len; } vec_voidptr_slice;
static inline vec_voidptr_slice vec_voidptr_slice_init(vec_voidptr* v, size_t start, size_t end) {
    vec_voidptr_slice s = {0};
    if (!v || start>end || end>v->len) return s;
    s.items = &v->items[start];
    s.len = end-start;
    return s;
}
static inline void** vec_voidptr_slice_get(const vec_voidptr_slice* s, size_t i) { assert(s && i<s->len); return &s->items[i]; }

/* ────────────────────────────────────────────────────────────────
   Typed vector macro
   ──────────────────────────────────────────────────────────────── */
#define DEFINE_VEC(type) \
typedef struct { type* items; size_t len; size_t capacity; } vec_##type; \
static inline vec_##type vec_##type##_init(type* buffer, size_t capacity){ assert(buffer||capacity==0); return (vec_##type){buffer,0,capacity}; } \
static inline vec_##type vec_##type##_empty(void){ return (vec_##type){0}; } \
static inline bool vec_##type##_is_empty(const vec_##type* v){ return !v||v->len==0; } \
static inline bool vec_##type##_is_full(const vec_##type* v){ return v&&v->len>=v->capacity; } \
static inline size_t vec_##type##_len(const vec_##type* v){ return v?v->len:0; } \
static inline size_t vec_##type##_capacity(const vec_##type* v){ return v?v->capacity:0; } \
static inline size_t vec_##type##_remaining(const vec_##type* v){ return v?(v->capacity-v->len):0; } \
static inline bool vec_##type##_get(const vec_##type* v, size_t i, type* out){ if(!v||!out||i>=v->len)return false; *out=v->items[i]; return true; } \
static inline Option_##type vec_##type##_get_option(const vec_##type* v, size_t i){ if(!v||i>=v->len)return option_none_##type(); return option_some_##type(v->items[i]); } \
static inline type vec_##type##_get_unchecked(const vec_##type* v, size_t i){ assert(v&&v->items&&i<v->len); return v->items[i]; } \
static inline bool vec_##type##_set(vec_##type* v, size_t i, type val){ if(!v||i>=v->len)return false; v->items[i]=val; return true; } \
static inline result_bool_Error vec_##type##_push(vec_##type* v, type item){ if(!v||!v->items) return result_bool_Error_err(ERR_INVALID_ARG); if(v->len>=v->capacity) return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); v->items[v->len++]=item; return result_bool_Error_ok(true); } \
static inline result_bool_Error vec_##type##_pop(vec_##type* v, type* out){ if(!v||!out||!v->items) return result_bool_Error_err(ERR_INVALID_ARG); if(v->len==0) return result_bool_Error_err(ERR_INVALID_STATE); *out=v->items[--v->len]; return result_bool_Error_ok(true); } \
static inline void vec_##type##_clear(vec_##type* v){ if(v)v->len=0; } \
static inline type* vec_##type##_first(const vec_##type* v){ return (v&&v->len>0)?&v->items[0]:NULL; } \
static inline type* vec_##type##_last(const vec_##type* v){ return (v&&v->len>0)?&v->items[v->len-1]:NULL; } \
static inline type* vec_##type##_data(const vec_##type* v){ return v?v->items:NULL; } \
static inline result_bool_Error vec_##type##_insert(vec_##type* v, size_t i, type item){ if(!v||!v->items)return result_bool_Error_err(ERR_INVALID_ARG); if(i>v->len)return result_bool_Error_err(ERR_OUT_OF_RANGE); if(v->len>=v->capacity)return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); for(size_t j=v->len;j>i;j--)v->items[j]=v->items[j-1]; v->items[i]=item; v->len++; return result_bool_Error_ok(true); } \
static inline result_bool_Error vec_##type##_remove(vec_##type* v, size_t i, type* out){ if(!v||!v->items||!out)return result_bool_Error_err(ERR_INVALID_ARG); if(v->len==0)return result_bool_Error_err(ERR_INVALID_STATE); if(i>=v->len)return result_bool_Error_err(ERR_OUT_OF_RANGE); *out=v->items[i]; for(size_t j=i;j<v->len-1;j++) v->items[j]=v->items[j+1]; v->len--; return result_bool_Error_ok(true); } \
static inline result_bool_Error vec_##type##_extend(vec_##type* v,const type* src,size_t count){ if(!v||!v->items||!src)return result_bool_Error_err(ERR_INVALID_ARG); if(v->len+count>v->capacity)return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); for(size_t j=0;j<count;j++)v->items[v->len+j]=src[j]; v->len+=count; return result_bool_Error_ok(true); } \
static inline vec_##type vec_##type##_alloc(size_t capacity){ if(capacity==0)return vec_##type##_empty(); type* buf=(type*)malloc(capacity*sizeof(type)); if(!buf)return vec_##type##_empty(); return vec_##type##_init(buf,capacity); } \
static inline void vec_##type##_free(vec_##type* v){ if(v&&v->items){ free(v->items); v->items=NULL; v->len=0; v->capacity=0; } } \
static inline vec_##type vec_##type##_arena_alloc(Arena* arena,size_t capacity){ if(!arena||capacity==0)return vec_##type##_empty(); type* buf=arena_alloc_array(arena,type,capacity); return vec_##type##_init(buf,capacity); } \
typedef struct{ vec_##type* vec; size_t index; } vec_##type##_iter; \
static inline vec_##type##_iter vec_##type##_iter_init(vec_##type* v){ return (vec_##type##_iter){.vec=v,.index=0}; } \
static inline bool vec_##type##_iter_next(vec_##type##_iter* it, type* out){ if(!it||!it->vec||!out)return false; if(it->index>=it->vec->len)return false; *out=it->vec->items[it->index++]; return true; } \
typedef struct{ type* items; size_t len; } vec_##type##_slice; \
static inline vec_##type##_slice vec_##type##_slice_init(vec_##type* v, size_t start, size_t end){ vec_##type##_slice s={0}; if(!v||start>end||end>v->len)return s; s.items=&v->items[start]; s.len=end-start; return s; } \
static inline type* vec_##type##_slice_get(const vec_##type##_slice* s,size_t i){ assert(s&&i<s->len); return &s->items[i]; } \
static inline result_bool_Error vec_##type##_extend_from_range(vec_##type* v, IntRange r){ size_t count=(r.end>r.start)?(r.end-r.start):0; if(v->len+count>v->capacity)return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); for(size_t i=0;i<count;i++)v->items[v->len+i]=r.start+i; v->len+=count; return result_bool_Error_ok(true); } \
static inline void vec_##type##_to_stringbuf(const vec_##type* v, StringBuf* sb, const char* fmt){ for(size_t i=0;i<v->len;i++){ stringbuf_printf(sb,fmt,v->items[i]); } }

#endif /* CANON_DATA_VEC_H */
