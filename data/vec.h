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
 * Canon-C vector is a **bounded**, **type-safe**, **explicit ownership** container
 * that provides deterministic memory usage and performance characteristics.
 *
 * Philosophy & goals:
 * ────────────────────────────────────────────────────────────────────────────
 * - Caller owns the buffer (stack, heap, arena, or static allocation)
 * - Fixed capacity - no automatic growth or hidden allocations
 * - Bounds-checked operations returning Result<T, Error>
 * - Zero-cost abstraction when bounds are statically verified
 * - Type-safe via macros (one concrete implementation per type)
 * - Deterministic memory and performance characteristics
 *
 * Supported features:
 * ────────────────────────────────────────────────────────────────────────────
 * - Stack, heap, or arena-backed buffers
 * - Typed vectors via DEFINE_VEC(type) macro
 * - Generic void* vector for heterogeneous collections
 * - Forward iterators with type safety
 * - Zero-copy slices and subvector views
 * - Range integration for numeric sequences
 * - StringBuf integration for debugging/display
 * - Optional safe access via Option<T>
 * - Result-based error handling (no exceptions)
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (for inline functions, stdbool.h, compound literals)
 * - All core functionality works in strict C99
 * - No dynamic dispatch or vtables
 * - Standard struct layout and alignment rules
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Each vector instance is independent - no shared state
 * - Concurrent reads are safe if no thread is modifying
 * - Concurrent modifications require external synchronization
 * - Iterator invalidation: any modification invalidates all active iterators
 * - All functions are thread-safe (no shared mutable state)
 *
 * Performance & Memory:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity:
 *   * Push/Pop: O(1)
 *   * Insert/Remove: O(n) - requires shifting elements
 *   * Extend: O(k) for k elements
 *   * Get/Set: O(1)
 *   * Iteration: O(1) per step
 *   * Slice/Subvector: O(1) - no copy, just pointer arithmetic
 * - Space complexity:
 *   * Generic void* vector: sizeof(void**) + 2*sizeof(size_t) + capacity*sizeof(void*)
 *   * Typed vectors: sizeof(T*) + 2*sizeof(size_t) + capacity*sizeof(T)
 *   * Iterators: 2*sizeof(size_t) - no heap allocation
 *   * Slices: sizeof(T*) + sizeof(size_t) - just a view
 * - Memory layout:
 *   * Contiguous buffer allocated by caller
 *   * No hidden allocations unless using vec_T_alloc()
 *   * Cache-friendly sequential access
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Dynamic arrays with predictable memory usage
 * - Temporary collections during parsing/processing
 * - Stack-based buffers for small, known-size collections
 * - Arena-backed collections for batch processing
 * - Building arrays with unknown final size (up to capacity)
 * - Type-safe collections without malloc overhead
 *
 * Usage examples:
 * ────────────────────────────────────────────────────────────────────────────
 * Stack-backed vector:
 *   int buffer[100];
 *   vec_int v = vec_int_init(buffer, 100);
 *   vec_int_push(&v, 42);
 *   vec_int_push(&v, 17);
 *   int val = vec_int_get_unchecked(&v, 0);  // val == 42
 *
 * Heap-backed vector:
 *   vec_int v = vec_int_alloc(100);
 *   vec_int_push(&v, 42);
 *   vec_int_free(&v);  // Must free when done
 *
 * Arena-backed vector:
 *   char arena_buf[1024];
 *   Arena arena = arena_init(arena_buf, sizeof(arena_buf));
 *   vec_int v = vec_int_arena_alloc(&arena, 100);
 *   vec_int_push(&v, 42);
 *   // No free needed - arena owns memory
 *
 * Safe iteration:
 *   vec_int_iter it = vec_int_iter_init(&v);
 *   int val;
 *   while (vec_int_iter_next(&it, &val)) {
 *       printf("%d\n", val);
 *   }
 *
 * Error handling with Result:
 *   result_bool_Error r = vec_int_push(&v, 42);
 *   if (result_bool_Error_is_err(r)) {
 *       Error e = result_bool_Error_unwrap_err(r);
 *       if (e == ERR_CAPACITY_EXCEEDED) {
 *           // Handle capacity error
 *       }
 *   }
 *
 * Safe access with Option:
 *   Option_int opt = vec_int_get_option(&v, 5);
 *   if (option_int_is_some(opt)) {
 *       int val = option_int_unwrap(opt);
 *       printf("Found: %d\n", val);
 *   }
 *
 * Recommended patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * ✓ Always check capacity before operations that can exceed it
 * ✓ Use get_option() for safe access when index might be out of bounds
 * ✓ Use iterators instead of manual indexing for full traversal
 * ✓ Prefer stack buffers for small, fixed-size collections
 * ✓ Use arena allocation for temporary collections that live together
 * ✓ Check Result return values from push/insert/extend operations
 * ✓ Use slices for zero-copy subrange access
 * ✓ Clear vectors for reuse instead of reallocating
 *
 * Anti-patterns to avoid:
 * ────────────────────────────────────────────────────────────────────────────
 * ✗ Don't use get_unchecked() without bounds verification (undefined behavior)
 * ✗ Don't ignore Result return values from mutating operations
 * ✗ Don't forget to free() heap-allocated vectors
 * ✗ Don't modify vector struct fields directly - use provided functions
 * ✗ Don't assume push() will succeed - always check capacity or Result
 * ✗ Don't use iterators after modifying the vector (iterator invalidation)
 * ✗ Don't mix different allocation strategies (heap vs arena vs stack)
 *
 * When to use vec vs other containers:
 * ────────────────────────────────────────────────────────────────────────────
 * - Use vec when you need dynamic sizing up to a known maximum
 * - Use fixed arrays when size is known at compile time
 * - Use linked lists when frequent insertions/deletions in middle
 * - Use hash tables when you need O(1) lookup by key
 * - Use vec_voidptr when you need heterogeneous types (or use unions)
 */

/* ────────────────────────────────────────────────────────────────────────────
   Result type for vector operations
   ──────────────────────────────────────────────────────────────────────────── */

CANON_C_DEFINE_RESULT(bool, Error)

/* ────────────────────────────────────────────────────────────────────────────
   Generic void* vector
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Generic void* vector - dynamically sized array with fixed capacity
 *
 * Stores pointers to any type. Useful for heterogeneous collections or
 * when type erasure is needed.
 *
 * Fields:
 * - items: Caller-owned buffer of void* pointers (must remain valid)
 * - len: Current number of valid elements (0 <= len <= capacity)
 * - capacity: Maximum elements the buffer can hold
 *
 * Memory layout:
 * - sizeof(vec_voidptr) = sizeof(void**) + 2*sizeof(size_t)
 * - Total memory: sizeof(vec_voidptr) + capacity*sizeof(void*)
 * - Each element stores a pointer, not the data itself
 *
 * ⚠️ WARNING: Do not modify fields directly - use provided functions.
 * Direct field access bypasses bounds checking and can cause undefined behavior.
 */
typedef struct {
    void** items;    ///< Caller-owned buffer of void* pointers
    size_t len;      ///< Current number of elements
    size_t capacity; ///< Maximum elements
} vec_voidptr;

/**
 * @brief Initializes a vector with caller-owned buffer
 *
 * The caller retains ownership of the buffer and must ensure it
 * remains valid for the vector's lifetime. The vector does not
 * allocate or free the buffer.
 *
 * @param buffer Caller-owned array of void* (NULL allowed if capacity==0)
 * @param capacity Maximum number of elements the buffer can hold
 * @return Initialized vector with len=0
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1) - no allocation, uses provided buffer
 */
static inline vec_voidptr vec_voidptr_init(void** buffer, size_t capacity) {
    assert(buffer != NULL || capacity == 0);
    return (vec_voidptr){ .items = buffer, .len = 0, .capacity = capacity };
}

/**
 * @brief Creates an empty vector with no buffer
 *
 * Useful as a placeholder or initial value. Cannot store elements
 * until reinitialized with a buffer.
 *
 * @return Empty vector with NULL buffer, len=0, capacity=0
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline vec_voidptr vec_voidptr_empty(void) {
    return (vec_voidptr){0};
}

/**
 * @brief Checks if vector is empty
 *
 * @param v Vector to check (NULL-safe)
 * @return true if v is NULL or has len==0, false otherwise
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool vec_voidptr_is_empty(const vec_voidptr* v) {
    return !v || v->len == 0;
}

/**
 * @brief Checks if vector is at capacity
 *
 * @param v Vector to check (NULL-safe)
 * @return true if len >= capacity, false otherwise
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool vec_voidptr_is_full(const vec_voidptr* v) {
    return v && v->len >= v->capacity;
}

/**
 * @brief Returns current number of elements
 *
 * @param v Vector to query (NULL-safe)
 * @return Number of elements (0 if v is NULL)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline size_t vec_voidptr_len(const vec_voidptr* v) {
    return v ? v->len : 0;
}

/**
 * @brief Returns maximum capacity
 *
 * @param v Vector to query (NULL-safe)
 * @return Maximum elements (0 if v is NULL)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline size_t vec_voidptr_capacity(const vec_voidptr* v) {
    return v ? v->capacity : 0;
}

/**
 * @brief Returns remaining space
 *
 * @param v Vector to query (NULL-safe)
 * @return capacity - len (0 if v is NULL)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline size_t vec_voidptr_remaining(const vec_voidptr* v) {
    return v ? (v->capacity - v->len) : 0;
}

/**
 * @brief Safely retrieves element at index
 *
 * This is the recommended way to access elements when the index
 * might be out of bounds.
 *
 * @param v   Vector to access (must not be NULL)
 * @param i   Index to retrieve
 * @param out Pointer to store result (must not be NULL)
 * @return true if element was retrieved, false if out of bounds or invalid args
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool vec_voidptr_get(const vec_voidptr* v, size_t i, void** out) {
    if (!v || !out || i >= v->len) return false;
    *out = v->items[i];
    return true;
}

/**
 * @brief Retrieves element at index as Option
 *
 * Functional-style safe access. Returns Some(value) if index is valid,
 * None if out of bounds.
 *
 * @param v Vector to access
 * @param i Index to retrieve
 * @return Some(element) if i < len, None otherwise
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline Option_voidptr vec_voidptr_get_option(const vec_voidptr* v, size_t i) {
    if (!v || i >= v->len) return option_none_voidptr();
    return option_some_voidptr(v->items[i]);
}

/**
 * @brief Retrieves element without bounds checking
 *
 * ⚠️ WARNING: Only use when you are certain the index is valid!
 * Accessing out of bounds is undefined behavior. In debug builds,
 * this asserts. In release builds, undefined if i >= len.
 *
 * Prefer: get(), get_option() over get_unchecked() in production.
 *
 * @param v Vector to access
 * @param i Index to retrieve
 * @return Element at index i
 *
 * Panics: If i >= len (assertion failure in debug builds)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void* vec_voidptr_get_unchecked(const vec_voidptr* v, size_t i) {
    assert(v && v->items && i < v->len);
    return v->items[i];
}

/**
 * @brief Appends an item to the end of the vector
 *
 * Adds item at index len and increments len. Fails if vector is full.
 *
 * @param v    Vector to push to (must not be NULL)
 * @param item Item to append (can be NULL pointer)
 * @return Ok(true) on success,
 *         Err(ERR_INVALID_ARG) if v or buffer is NULL,
 *         Err(ERR_CAPACITY_EXCEEDED) if vector is full
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1) - uses pre-allocated buffer
 */
static inline result_bool_Error vec_voidptr_push(vec_voidptr* v, void* item) {
    if (!v || !v->items) return result_bool_Error_err(ERR_INVALID_ARG);
    if (v->len >= v->capacity) return result_bool_Error_err(ERR_CAPACITY_EXCEEDED);
    v->items[v->len++] = item;
    return result_bool_Error_ok(true);
}

/**
 * @brief Removes and returns the last element
 *
 * Decrements len and returns the element that was at index len-1.
 * Fails if vector is empty.
 *
 * @param v   Vector to pop from (must not be NULL)
 * @param out Pointer to store popped element (must not be NULL)
 * @return Ok(true) on success,
 *         Err(ERR_INVALID_ARG) if v, buffer, or out is NULL,
 *         Err(ERR_INVALID_STATE) if vector is empty
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline result_bool_Error vec_voidptr_pop(vec_voidptr* v, void** out) {
    if (!v || !out || !v->items) return result_bool_Error_err(ERR_INVALID_ARG);
    if (v->len == 0) return result_bool_Error_err(ERR_INVALID_STATE);
    *out = v->items[--v->len];
    return result_bool_Error_ok(true);
}

/**
 * @brief Removes and returns the last element as Option
 *
 * Functional-style pop operation. Returns Some(element) if vector is
 * not empty, None if empty.
 *
 * @param v Vector to pop from
 * @return Some(element) if len > 0, None if empty
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline Option_voidptr vec_voidptr_pop_option(vec_voidptr* v) {
    void* out;
    if (result_bool_Error_is_ok(vec_voidptr_pop(v, &out))) {
        return option_some_voidptr(out);
    }
    return option_none_voidptr();
}

/**
 * @brief Removes all elements from the vector
 *
 * Sets len to 0 but does not modify capacity or free buffer.
 * Does not modify the actual buffer contents - just makes them inaccessible.
 *
 * @param v Vector to clear (NULL-safe)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void vec_voidptr_clear(vec_voidptr* v) {
    if (v) v->len = 0;
}

/**
 * @brief Returns pointer to first element
 *
 * Useful for range-based operations or getting mutable access to
 * the first element.
 *
 * @param v Vector to access (NULL-safe)
 * @return Pointer to first element, or NULL if empty or v is NULL
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void** vec_voidptr_first(const vec_voidptr* v) {
    return (v && v->len > 0) ? &v->items[0] : NULL;
}

/**
 * @brief Returns pointer to last element
 *
 * Useful for range-based operations or getting mutable access to
 * the last element.
 *
 * @param v Vector to access (NULL-safe)
 * @return Pointer to last element, or NULL if empty or v is NULL
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void** vec_voidptr_last(const vec_voidptr* v) {
    return (v && v->len > 0) ? &v->items[v->len - 1] : NULL;
}

/* ────────────────────────────────────────────────────────────────────────────
   Heap allocation functions
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates a vector on the heap
 *
 * Caller is responsible for calling vec_voidptr_free() when done.
 * Returns empty vector if allocation fails or capacity is 0.
 *
 * @param capacity Maximum number of elements
 * @return Initialized vector with heap-allocated buffer, or empty vector on failure
 *
 * Performance:
 * - Time: O(1) - just malloc, no initialization
 * - Space: O(capacity) - allocates capacity*sizeof(void*) bytes
 */
static inline vec_voidptr vec_voidptr_alloc(size_t capacity) {
    if (capacity == 0) return vec_voidptr_empty();
    void** buf = (void**)malloc(capacity * sizeof(void*));
    if (!buf) return vec_voidptr_empty();
    return vec_voidptr_init(buf, capacity);
}

/**
 * @brief Frees a heap-allocated vector
 *
 * Only call this for vectors created with vec_voidptr_alloc().
 * Do NOT call for stack or arena-allocated vectors.
 * Sets all fields to 0 after freeing.
 *
 * ⚠️ WARNING: Does not free the pointed-to objects, only the buffer.
 * If you need deep cleanup, iterate and free each element first.
 *
 * @param v Vector to free (NULL-safe)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void vec_voidptr_free(vec_voidptr* v) {
    if (v && v->items) {
        free(v->items);
        v->items = NULL;
        v->len = 0;
        v->capacity = 0;
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Arena allocation functions
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates a vector from an arena
 *
 * The arena owns the memory - no need to free. Vector lifetime is
 * tied to arena lifetime. Returns empty vector if allocation fails.
 *
 * @param arena Arena to allocate from (must not be NULL)
 * @param capacity Maximum number of elements
 * @return Initialized vector with arena-allocated buffer, or empty vector on failure
 *
 * Performance:
 * - Time: O(1) - arena bump allocation
 * - Space: O(capacity) - allocates capacity*sizeof(void*) from arena
 */
static inline vec_voidptr vec_voidptr_arena_alloc(Arena* arena, size_t capacity) {
    if (!arena || capacity == 0) return vec_voidptr_empty();
    void** buf = arena_alloc_array(arena, void*, capacity);
    if (!buf) return vec_voidptr_empty();
    return vec_voidptr_init(buf, capacity);
}

/* ────────────────────────────────────────────────────────────────────────────
   Iterator support
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Forward iterator for vec_voidptr
 *
 * Iterators are invalidated by any modification to the vector
 * (push, pop, insert, remove, clear, etc.).
 *
 * Fields:
 * - vec: Pointer to the vector being iterated
 * - index: Current position in iteration
 */
typedef struct {
    vec_voidptr* vec; ///< Vector being iterated
    size_t index;     ///< Current iteration position
} vec_voidptr_iter;

/**
 * @brief Creates an iterator positioned at the start
 *
 * @param v Vector to iterate over
 * @return Iterator positioned at index 0
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline vec_voidptr_iter vec_voidptr_iter_init(vec_voidptr* v) {
    return (vec_voidptr_iter){ .vec = v, .index = 0 };
}

/**
 * @brief Advances iterator and retrieves next element
 *
 * @param it  Iterator to advance (must not be NULL)
 * @param out Pointer to store element (must not be NULL)
 * @return true if element was retrieved, false if end of vector or invalid args
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool vec_voidptr_iter_next(vec_voidptr_iter* it, void** out) {
    if (!it || !it->vec || !out) return false;
    if (it->index >= it->vec->len) return false;
    *out = it->vec->items[it->index++];
    return true;
}

/* ────────────────────────────────────────────────────────────────────────────
   Slice support (zero-copy subrange views)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Zero-copy slice/view into a vector
 *
 * A slice is just a pointer and length - it does not own the data.
 * The underlying vector must remain valid and unmodified while the
 * slice is in use.
 *
 * Fields:
 * - items: Pointer into the vector's buffer
 * - len: Number of elements in the slice
 *
 * ⚠️ WARNING: Modifying the underlying vector invalidates all slices.
 */
typedef struct {
    void** items; ///< Pointer into parent vector
    size_t len;   ///< Number of elements in slice
} vec_voidptr_slice;

/**
 * @brief Creates a slice of the vector from [start, end)
 *
 * Zero-copy operation - just pointer arithmetic. The slice is a view
 * into the vector's buffer.
 *
 * @param v     Vector to slice
 * @param start Start index (inclusive)
 * @param end   End index (exclusive)
 * @return Slice with items pointing to &v->items[start] and len=end-start,
 *         or empty slice if invalid range
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1) - no allocation
 */
static inline vec_voidptr_slice vec_voidptr_slice_init(
    vec_voidptr* v, size_t start, size_t end) {
    vec_voidptr_slice s = {0};
    if (!v || start > end || end > v->len) return s;
    s.items = &v->items[start];
    s.len = end - start;
    return s;
}

/**
 * @brief Accesses element in slice without bounds checking
 *
 * ⚠️ WARNING: Only use when you are certain i < slice.len!
 * Undefined behavior if i >= len.
 *
 * @param s Slice to access
 * @param i Index within slice
 * @return Pointer to element at index i
 *
 * Panics: If i >= len (assertion failure in debug builds)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void** vec_voidptr_slice_get(const vec_voidptr_slice* s, size_t i) {
    assert(s && i < s->len);
    return &s->items[i];
}

/* ────────────────────────────────────────────────────────────────────────────
   Typed vector macro
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Defines a concrete typed vector for the given element type
 *
 * Generates a complete vector implementation including struct definition
 * and all associated functions. This is the primary way to create type-safe
 * vectors in Canon-C.
 *
 * Generated type: vec_##type
 *
 * Generated functions - Constructors:
 * ────────────────────────────────────────────────────────────────────────────
 * - vec_##type##_init(buffer, capacity)      - Initialize with caller buffer
 * - vec_##type##_empty()                     - Create empty vector
 * - vec_##type##_alloc(capacity)             - Heap allocate
 * - vec_##type##_arena_alloc(arena, cap)     - Arena allocate
 *
 * Generated functions - Queries:
 * ────────────────────────────────────────────────────────────────────────────
 * - vec_##type##_is_empty(v)                 - Check if empty
 * - vec_##type##_is_full(v)                  - Check if at capacity
 * - vec_##type##_len(v)                      - Get current length
 * - vec_##type##_capacity(v)                 - Get max capacity
 * - vec_##type##_remaining(v)                - Get remaining space
 *
 * Generated functions - Access:
 * ────────────────────────────────────────────────────────────────────────────
 * - vec_##type##_get(v, i, out)              - Safe bounds-checked get
 * - vec_##type##_get_option(v, i)            - Get as Option<T>
 * - vec_##type##_get_unchecked(v, i)         - Unchecked get (⚠️ unsafe)
 * - vec_##type##_at(v, i)                    - Get pointer to element
 * - vec_##type##_set(v, i, val)              - Safe bounds-checked set
 * - vec_##type##_first(v)                    - Pointer to first element
 * - vec_##type##_last(v)                     - Pointer to last element
 * - vec_##type##_data(v)                     - Raw buffer pointer
 *
 * Generated functions - Modification:
 * ────────────────────────────────────────────────────────────────────────────
 * - vec_##type##_push(v, item)               - Append to end
 * - vec_##type##_pop(v, out)                 - Remove from end
 * - vec_##type##_pop_option(v)               - Pop as Option<T>
 * - vec_##type##_insert(v, i, item)          - Insert at index
 * - vec_##type##_remove(v, i, out)           - Remove at index
 * - vec_##type##_remove_option(v, i)         - Remove as Option<T>
 * - vec_##type##_extend(v, src, count)       - Append multiple elements
 * - vec_##type##_extend_from_range(v, range) - Extend from IntRange
 * - vec_##type##_clear(v)                    - Remove all elements
 * - vec_##type##_free(v)                     - Free heap-allocated vector
 *
 * Generated functions - Iteration:
 * ────────────────────────────────────────────────────────────────────────────
 * - vec_##type##_iter_init(v)                - Create iterator
 * - vec_##type##_iter_next(it, out)          - Get next element
 *
 * Generated functions - Slicing:
 * ────────────────────────────────────────────────────────────────────────────
 * - vec_##type##_slice_init(v, start, end)   - Create zero-copy slice
 * - vec_##type##_slice_get(slice, i)         - Access slice element
 *
 * Generated functions - Display:
 * ────────────────────────────────────────────────────────────────────────────
 * - vec_##type##_to_stringbuf(v, sb, fmt)    - Format to StringBuf
 * - vec_##type##_to_stringbuf_cb(v, sb, cb)  - Custom formatter callback
 *
 * Type name convention:
 * ────────────────────────────────────────────────────────────────────────────
 * For pointer types, use a typedef first:
 *   typedef const char* constcharptr;
 *   DEFINE_VEC(constcharptr)
 * This produces: vec_constcharptr
 *
 * Usage:
 *   DEFINE_VEC(int)           // vec_int
 *   DEFINE_VEC(float)         // vec_float
 *   DEFINE_VEC(MyStruct)      // vec_MyStruct
 *
 * Note: This must be used at file or global scope, not inside functions.
 *       Use once per type in a header or source file.
 *
 * Requirements:
 * ────────────────────────────────────────────────────────────────────────────
 * - Option_##type must be defined (via DEFINE_OPTION)
 * - For extend_from_range: type must be assignable from IntRange values
 * - For to_stringbuf: format string must match type
 *
 * @param type The element type for the vector
 */
#define DEFINE_VEC(type) \
\
/** \
 * @brief Typed vector for elements of type 'type' \
 * \
 * Fields: \
 * - items: Caller-owned buffer of type* (must remain valid) \
 * - len: Current number of valid elements (0 <= len <= capacity) \
 * - capacity: Maximum elements the buffer can hold \
 * \
 * Memory layout: \
 * - sizeof(vec_##type) = sizeof(type*) + 2*sizeof(size_t) \
 * - Total memory: sizeof(vec_##type) + capacity*sizeof(type) \
 * \
 * ⚠️ WARNING: Do not modify fields directly - use provided functions. \
 */ \
typedef struct { \
    type* items;     \
    size_t len;      \
    size_t capacity; \
} vec_##type; \
\
/** \
 * @brief Initializes a vector with caller-owned buffer \
 * \
 * @param buffer Caller-owned array of type (NULL allowed if capacity==0) \
 * @param capacity Maximum number of elements \
 * @return Initialized vector with len=0 \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline vec_##type vec_##type##_init(type* buffer, size_t capacity) { \
    assert(buffer || capacity == 0); \
    return (vec_##type){buffer, 0, capacity}; \
} \
\
/** \
 * @brief Creates an empty vector \
 * \
 * @return Empty vector with NULL buffer \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline vec_##type vec_##type##_empty(void) { \
    return (vec_##type){0}; \
} \
\
/** \
 * @brief Checks if vector is empty \
 * \
 * @param v Vector to check (NULL-safe) \
 * @return true if empty \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline bool vec_##type##_is_empty(const vec_##type* v) { \
    return !v || v->len == 0; \
} \
\
/** \
 * @brief Checks if vector is full \
 * \
 * @param v Vector to check (NULL-safe) \
 * @return true if at capacity \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline bool vec_##type##_is_full(const vec_##type* v) { \
    return v && v->len >= v->capacity; \
} \
\
/** \
 * @brief Returns current length \
 * \
 * @param v Vector to query (NULL-safe) \
 * @return Number of elements \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline size_t vec_##type##_len(const vec_##type* v) { \
    return v ? v->len : 0; \
} \
\
/** \
 * @brief Returns capacity \
 * \
 * @param v Vector to query (NULL-safe) \
 * @return Maximum elements \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline size_t vec_##type##_capacity(const vec_##type* v) { \
    return v ? v->capacity : 0; \
} \
\
/** \
 * @brief Returns remaining space \
 * \
 * @param v Vector to query (NULL-safe) \
 * @return capacity - len \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline size_t vec_##type##_remaining(const vec_##type* v) { \
    return v ? (v->capacity - v->len) : 0; \
} \
\
/** \
 * @brief Safely retrieves element at index \
 * \
 * @param v   Vector to access \
 * @param i   Index to retrieve \
 * @param out Pointer to store result \
 * @return true if successful \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline bool vec_##type##_get(const vec_##type* v, size_t i, type* out) { \
    if (!v || !out || i >= v->len) return false; \
    *out = v->items[i]; \
    return true; \
} \
\
/** \
 * @brief Retrieves element as Option \
 * \
 * @param v Vector to access \
 * @param i Index to retrieve \
 * @return Some(element) or None \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline Option_##type vec_##type##_get_option(const vec_##type* v, size_t i) { \
    if (!v || i >= v->len) return option_none_##type(); \
    return option_some_##type(v->items[i]); \
} \
\
/** \
 * @brief Retrieves element without bounds checking \
 * \
 * ⚠️ WARNING: Undefined behavior if i >= len! \
 * \
 * @param v Vector to access \
 * @param i Index to retrieve \
 * @return Element at index i \
 * \
 * Panics: If i >= len (debug builds) \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline type vec_##type##_get_unchecked(const vec_##type* v, size_t i) { \
    assert(v && v->items && i < v->len); \
    return v->items[i]; \
} \
\
/** \
 * @brief Returns pointer to element \
 * \
 * @param v Vector to access \
 * @param i Index to access \
 * @return Pointer to element \
 * \
 * Panics: If i >= len (debug builds) \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline type* vec_##type##_at(const vec_##type* v, size_t i) { \
    assert(v && i < v->len); \
    return &v->items[i]; \
} \
\
/** \
 * @brief Sets element at index \
 * \
 * @param v   Vector to modify \
 * @param i   Index to set \
 * @param val Value to set \
 * @return true if successful \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline bool vec_##type##_set(vec_##type* v, size_t i, type val) { \
    if (!v || i >= v->len) return false; \
    v->items[i] = val; \
    return true; \
} \
\
/** \
 * @brief Appends element to end \
 * \
 * @param v    Vector to push to \
 * @param item Item to append \
 * @return Ok(true) or Err \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline result_bool_Error vec_##type##_push(vec_##type* v, type item) { \
    if (!v || !v->items) return result_bool_Error_err(ERR_INVALID_ARG); \
    if (v->len >= v->capacity) return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    v->items[v->len++] = item; \
    return result_bool_Error_ok(true); \
} \
\
/** \
 * @brief Removes last element \
 * \
 * @param v   Vector to pop from \
 * @param out Pointer to store result \
 * @return Ok(true) or Err \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline result_bool_Error vec_##type##_pop(vec_##type* v, type* out) { \
    if (!v || !out || !v->items) return result_bool_Error_err(ERR_INVALID_ARG); \
    if (v->len == 0) return result_bool_Error_err(ERR_INVALID_STATE); \
    *out = v->items[--v->len]; \
    return result_bool_Error_ok(true); \
} \
\
/** \
 * @brief Removes last element as Option \
 * \
 * @param v Vector to pop from \
 * @return Some(element) or None \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline Option_##type vec_##type##_pop_option(vec_##type* v) { \
    type out; \
    if (result_bool_Error_is_ok(vec_##type##_pop(v, &out))) \
        return option_some_##type(out); \
    return option_none_##type(); \
} \
\
/** \
 * @brief Clears all elements \
 * \
 * @param v Vector to clear \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline void vec_##type##_clear(vec_##type* v) { \
    if (v) v->len = 0; \
} \
\
/** \
 * @brief Returns pointer to first element \
 * \
 * @param v Vector to access (NULL-safe) \
 * @return Pointer to first or NULL \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline type* vec_##type##_first(const vec_##type* v) { \
    return (v && v->len > 0) ? &v->items[0] : NULL; \
} \
\
/** \
 * @brief Returns pointer to last element \
 * \
 * @param v Vector to access (NULL-safe) \
 * @return Pointer to last or NULL \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline type* vec_##type##_last(const vec_##type* v) { \
    return (v && v->len > 0) ? &v->items[v->len - 1] : NULL; \
} \
\
/** \
 * @brief Returns raw buffer pointer \
 * \
 * @param v Vector to access (NULL-safe) \
 * @return Buffer pointer or NULL \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline type* vec_##type##_data(const vec_##type* v) { \
    return v ? v->items : NULL; \
} \
\
/** \
 * @brief Inserts element at index \
 * \
 * Shifts elements at and after index to the right. \
 * \
 * @param v    Vector to modify \
 * @param i    Index to insert at (can be == len) \
 * @param item Item to insert \
 * @return Ok(true) or Err \
 * \
 * Performance: \
 * - Time: O(n) - shifts elements \
 * - Space: O(1) \
 */ \
static inline result_bool_Error vec_##type##_insert(vec_##type* v, size_t i, type item) { \
    if (!v || !v->items) return result_bool_Error_err(ERR_INVALID_ARG); \
    if (i > v->len) return result_bool_Error_err(ERR_OUT_OF_RANGE); \
    if (v->len >= v->capacity) return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    for (size_t j = v->len; j > i; j--) \
        v->items[j] = v->items[j - 1]; \
    v->items[i] = item; \
    v->len++; \
    return result_bool_Error_ok(true); \
} \
\
/** \
 * @brief Removes element at index \
 * \
 * Shifts elements after index to the left. \
 * \
 * @param v   Vector to modify \
 * @param i   Index to remove \
 * @param out Pointer to store removed element \
 * @return Ok(true) or Err \
 * \
 * Performance: \
 * - Time: O(n) - shifts elements \
 * - Space: O(1) \
 */ \
static inline result_bool_Error vec_##type##_remove(vec_##type* v, size_t i, type* out) { \
    if (!v || !v->items || !out) return result_bool_Error_err(ERR_INVALID_ARG); \
    if (v->len == 0) return result_bool_Error_err(ERR_INVALID_STATE); \
    if (i >= v->len) return result_bool_Error_err(ERR_OUT_OF_RANGE); \
    *out = v->items[i]; \
    for (size_t j = i; j < v->len - 1; j++) \
        v->items[j] = v->items[j + 1]; \
    v->len--; \
    return result_bool_Error_ok(true); \
} \
\
/** \
 * @brief Removes element at index as Option \
 * \
 * @param v Vector to modify \
 * @param i Index to remove \
 * @return Some(element) or None \
 * \
 * Performance: \
 * - Time: O(n) \
 * - Space: O(1) \
 */ \
static inline Option_##type vec_##type##_remove_option(vec_##type* v, size_t i) { \
    type out; \
    if (result_bool_Error_is_ok(vec_##type##_remove(v, i, &out))) \
        return option_some_##type(out); \
    return option_none_##type(); \
} \
\
/** \
 * @brief Extends vector with array of elements \
 * \
 * Appends count elements from src to the end. \
 * \
 * @param v     Vector to extend \
 * @param src   Source array \
 * @param count Number of elements to copy \
 * @return Ok(true) or Err \
 * \
 * Performance: \
 * - Time: O(count) \
 * - Space: O(1) \
 */ \
static inline result_bool_Error vec_##type##_extend( \
    vec_##type* v, const type* src, size_t count) { \
    if (!v || !v->items || !src) return result_bool_Error_err(ERR_INVALID_ARG); \
    if (v->len + count > v->capacity) \
        return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    for (size_t j = 0; j < count; j++) \
        v->items[v->len + j] = src[j]; \
    v->len += count; \
    return result_bool_Error_ok(true); \
} \
\
/** \
 * @brief Allocates vector on heap \
 * \
 * @param capacity Maximum elements \
 * @return Initialized vector or empty on failure \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(capacity) \
 */ \
static inline vec_##type vec_##type##_alloc(size_t capacity) { \
    if (capacity == 0) return vec_##type##_empty(); \
    type* buf = (type*)malloc(capacity * sizeof(type)); \
    if (!buf) return vec_##type##_empty(); \
    return vec_##type##_init(buf, capacity); \
} \
\
/** \
 * @brief Frees heap-allocated vector \
 * \
 * ⚠️ WARNING: Only for heap-allocated vectors! \
 * \
 * @param v Vector to free \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline void vec_##type##_free(vec_##type* v) { \
    if (v && v->items) { \
        free(v->items); \
        v->items = NULL; \
        v->len = 0; \
        v->capacity = 0; \
    } \
} \
\
/** \
 * @brief Allocates vector from arena \
 * \
 * @param arena Arena to allocate from \
 * @param capacity Maximum elements \
 * @return Initialized vector or empty on failure \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(capacity) \
 */ \
static inline vec_##type vec_##type##_arena_alloc(Arena* arena, size_t capacity) { \
    if (!arena || capacity == 0) return vec_##type##_empty(); \
    type* buf = arena_alloc_array(arena, type, capacity); \
    if (!buf) return vec_##type##_empty(); \
    return vec_##type##_init(buf, capacity); \
} \
\
/** \
 * @brief Forward iterator for typed vector \
 * \
 * ⚠️ WARNING: Invalidated by vector modifications! \
 */ \
typedef struct { \
    vec_##type* vec; \
    size_t index; \
} vec_##type##_iter; \
\
/** \
 * @brief Creates iterator at start \
 * \
 * @param v Vector to iterate \
 * @return Iterator at position 0 \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline vec_##type##_iter vec_##type##_iter_init(vec_##type* v) { \
    return (vec_##type##_iter){.vec = v, .index = 0}; \
} \
\
/** \
 * @brief Advances iterator \
 * \
 * @param it  Iterator to advance \
 * @param out Pointer to store element \
 * @return true if element retrieved \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline bool vec_##type##_iter_next(vec_##type##_iter* it, type* out) { \
    if (!it || !it->vec || !out) return false; \
    if (it->index >= it->vec->len) return false; \
    *out = it->vec->items[it->index++]; \
    return true; \
} \
\
/** \
 * @brief Zero-copy slice view \
 * \
 * ⚠️ WARNING: Invalidated by vector modifications! \
 */ \
typedef struct { \
    type* items; \
    size_t len; \
} vec_##type##_slice; \
\
/** \
 * @brief Creates slice from [start, end) \
 * \
 * @param v     Vector to slice \
 * @param start Start index (inclusive) \
 * @param end   End index (exclusive) \
 * @return Slice or empty on invalid range \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline vec_##type##_slice vec_##type##_slice_init( \
    vec_##type* v, size_t start, size_t end) { \
    vec_##type##_slice s = {0}; \
    if (!v || start > end || end > v->len) return s; \
    s.items = &v->items[start]; \
    s.len = end - start; \
    return s; \
} \
\
/** \
 * @brief Accesses slice element unchecked \
 * \
 * ⚠️ WARNING: Undefined behavior if i >= len! \
 * \
 * @param s Slice to access \
 * @param i Index within slice \
 * @return Pointer to element \
 * \
 * Panics: If i >= len (debug builds) \
 * \
 * Performance: \
 * - Time: O(1) \
 * - Space: O(1) \
 */ \
static inline type* vec_##type##_slice_get(const vec_##type##_slice* s, size_t i) { \
    assert(s && i < s->len); \
    return &s->items[i]; \
} \
\
/** \
 * @brief Extends vector from IntRange \
 * \
 * Adds numeric sequence from range to vector. \
 * Requires type to be assignable from range values. \
 * \
 * @param v Vector to extend \
 * @param r IntRange to extend from \
 * @return Ok(true) or Err \
 * \
 * Performance: \
 * - Time: O(|end - start|) \
 * - Space: O(1) \
 */ \
static inline result_bool_Error vec_##type##_extend_from_range( \
    vec_##type* v, IntRange r) { \
    size_t step = (r.end >= r.start) ? 1 : -1; \
    size_t count = (r.end >= r.start) ? (r.end - r.start) : (r.start - r.end); \
    if (v->len + count > v->capacity) \
        return result_bool_Error_err(ERR_CAPACITY_EXCEEDED); \
    for (size_t i = 0; i < count; i++) \
        v->items[v->len + i] = r.start + i * step; \
    v->len += count; \
    return result_bool_Error_ok(true); \
} \
\
/** \
 * @brief Formats vector to StringBuf with printf-style format \
 * \
 * @param v   Vector to format \
 * @param sb  StringBuf to write to \
 * @param fmt Printf format string for each element \
 * \
 * Performance: \
 * - Time: O(n) \
 * - Space: O(1) \
 */ \
static inline void vec_##type##_to_stringbuf( \
    const vec_##type* v, StringBuf* sb, const char* fmt) { \
    for (size_t i = 0; i < v->len; i++) { \
        stringbuf_printf(sb, fmt, v->items[i]); \
    } \
} \
\
/** \
 * @brief Formats vector to StringBuf with custom callback \
 * \
 * @param v  Vector to format \
 * @param sb StringBuf to write to \
 * @param cb Callback function(StringBuf*, element) for each element \
 * \
 * Performance: \
 * - Time: O(n) + O(cb) per element \
 * - Space: O(1) \
 */ \
static inline void vec_##type##_to_stringbuf_cb( \
    const vec_##type* v, StringBuf* sb, void(*cb)(StringBuf*, type)) { \
    for (size_t i = 0; i < v->len; i++) \
        cb(sb, v->items[i]); \
}

#endif /* CANON_DATA_VEC_H */
