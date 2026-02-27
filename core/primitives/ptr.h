#ifndef CANON_CORE_PRIMITIVES_PTR_H
#define CANON_CORE_PRIMITIVES_PTR_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/checked.h"
/**
 * @file core/primitives/ptr.h
 * @brief Explicit, named pointer arithmetic and alignment utilities
 *
 * Centralizes all pointer manipulation that other Canon-C modules perform
 * inline and inconsistently: arena bump allocation, pool block indexing,
 * vec offset calculation, slice construction, and buffer alignment checks.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - All pointer arithmetic is named — no raw `+` on void* or char*
 * - Alignment is always explicit — no silent padding assumptions
 * - Null is always checked — no silent UB on NULL input
 * - Overflow is caught — no wraparound on large offsets
 * - All operations work on void* or u8* — no type puns needed
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * ptr.h lives in core/primitives/ and may only include from core/primitives/.
 * No core/, data/, semantics/, or util/ headers may be included here.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Assumes flat address space (true for all hosted and most embedded targets)
 * - Pointer-to-integer conversion uses uintptr_t (via types.h)
 * - No platform-specific intrinsics
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are pure or operate on caller-supplied pointers.
 * No shared state — fully thread-safe.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are static inline — zero call overhead.
 * Compilers reduce every operation here to 1–3 instructions.
 *
 * Quick start:
 * ```c
 * #include "core/primitives/ptr.h"
 *
 * // Advance a buffer pointer by n bytes
 * void* next = ptr_offset(buf, 64);
 *
 * // Align a pointer up to 16-byte boundary
 * void* aligned = ptr_align_up(raw, 16);
 *
 * // Distance between two pointers in bytes
 * isize dist = ptr_diff(end, start);
 *
 * // Check if a pointer is within a buffer
 * bool ok = ptr_in_range(p, buf, buf_end);
 *
 * // Round a usize up to alignment
 * usize padded = align_up(size, alignof(double));
 * ```
 *
 * @sa core/slice.h — typed {ptr, len} views built on top of ptr.h
 * @sa core/arena.h — bump allocator that uses ptr_align_up and ptr_offset
 * @sa core/memory.h — safe memcpy/memmove wrappers
 */
/* ════════════════════════════════════════════════════════════════════════════
   Alignment — integer helpers
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Rounds n up to the nearest multiple of align
 *
 * align must be a power of two.
 *
 * @param n Value to round up
 * @param align Alignment boundary (must be a power of two, > 0)
 * @return Smallest value >= n that is a multiple of align
 *
 * @pre align > 0 && (align & (align - 1)) == 0
 *
 * Examples:
 * - align_up(13, 8) → 16
 * - align_up(16, 8) → 16 (already aligned)
 * - align_up(0, 16) → 0
 *
 * Performance:
 * - Time: O(1) — single bitwise expression
 * - Space: O(1)
 */
static inline usize align_up(usize n, usize align) {
    require_msg(align > 0, "align_up: align must be > 0");
    require_msg((align & (align - 1)) == 0, "align_up: align must be a power of two");
    return (n + align - 1) & ~(align - 1);
}
/**
 * @brief Rounds n down to the nearest multiple of align
 *
 * align must be a power of two.
 *
 * @param n Value to round down
 * @param align Alignment boundary (must be a power of two, > 0)
 * @return Largest value <= n that is a multiple of align
 *
 * @pre align > 0 && (align & (align - 1)) == 0
 *
 * Examples:
 * - align_down(13, 8) → 8
 * - align_down(16, 8) → 16 (already aligned)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline usize align_down(usize n, usize align) {
    require_msg(align > 0, "align_down: align must be > 0");
    require_msg((align & (align - 1)) == 0, "align_down: align must be a power of two");
    return n & ~(align - 1);
}
/**
 * @brief Returns the number of padding bytes needed to align n to align
 *
 * align must be a power of two.
 *
 * @param n Current position or size
 * @param align Alignment boundary (must be a power of two, > 0)
 * @return Bytes of padding — 0 if already aligned
 *
 * Example:
 * - align_padding(13, 8) → 3 (13 + 3 = 16)
 * - align_padding(16, 8) → 0
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline usize align_padding(usize n, usize align) {
    usize up = align_up(n, align);
    return up - n;
}
/**
 * @brief Returns true if n is a multiple of align
 *
 * align must be a power of two.
 *
 * @param n Value to check
 * @param align Alignment boundary (must be a power of two, > 0)
 * @return true if n % align == 0
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool is_aligned(usize n, usize align) {
    require_msg(align > 0, "is_aligned: align must be > 0");
    require_msg((align & (align - 1)) == 0, "is_aligned: align must be a power of two");
    return (n & (align - 1)) == 0;
}
/**
 * @brief Returns true if n is a power of two
 *
 * @param n Value to check (must be > 0)
 * @return true if n is a power of two
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool is_power_of_two(usize n) {
    return n > 0 && (n & (n - 1)) == 0;
}
/* ════════════════════════════════════════════════════════════════════════════
   Alignment — pointer helpers
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Rounds a pointer up to the nearest align-byte boundary
 *
 * @param p Pointer to align (NULL-safe — returns NULL if p == NULL)
 * @param align Alignment boundary (must be a power of two, > 0)
 * @return Aligned pointer >= p
 *
 * @pre align > 0 && is_power_of_two(align)
 *
 * Example:
 * ```c
 * void* aligned = ptr_align_up(raw_ptr, 16);
 * ```
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void* ptr_align_up(void* p, usize align) {
    require_msg(align > 0, "ptr_align_up: align must be > 0");
    require_msg((align & (align - 1)) == 0, "ptr_align_up: align must be a power of two");
    if (!p) return NULL;
    usize addr = (usize)(uintptr_t)p;
    usize aligned = align_up(addr, align);
    return (void*)(uintptr_t)aligned;
}
/**
 * @brief Rounds a pointer down to the nearest align-byte boundary
 *
 * @param p Pointer to align (NULL-safe — returns NULL if p == NULL)
 * @param align Alignment boundary (must be a power of two, > 0)
 * @return Aligned pointer <= p
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void* ptr_align_down(void* p, usize align) {
    require_msg(align > 0, "ptr_align_down: align must be > 0");
    require_msg((align & (align - 1)) == 0, "ptr_align_down: align must be a power of two");
    if (!p) return NULL;
    usize addr = (usize)(uintptr_t)p;
    usize aligned = align_down(addr, align);
    return (void*)(uintptr_t)aligned;
}
/**
 * @brief Returns true if pointer p is aligned to align bytes
 *
 * @param p Pointer to check (NULL returns false)
 * @param align Alignment boundary (must be a power of two, > 0)
 * @return true if (uintptr_t)p % align == 0
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool ptr_is_aligned(const void* p, usize align) {
    require_msg(align > 0, "ptr_is_aligned: align must be > 0");
    require_msg((align & (align - 1)) == 0, "ptr_is_aligned: align must be a power of two");
    if (!p) return false;
    return is_aligned((usize)(uintptr_t)p, align);
}
/**
 * @brief Returns the number of padding bytes needed to align pointer p
 *
 * @param p Pointer to check (NULL returns 0)
 * @param align Alignment boundary (must be a power of two, > 0)
 * @return Bytes until next align-boundary, 0 if already aligned
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline usize ptr_align_padding(const void* p, usize align) {
    if (!p) return 0;
    return align_padding((usize)(uintptr_t)p, align);
}

/* ════════════════════════════════════════════════════════════════════════════
   NEW: Compile-time & structural alignment helpers
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def ALIGN_OF(type)
 * @brief Compile-time alignment requirement of a type (in bytes)
 *
 * Uses C11 _Alignof or portable C99 fallback via offsetof trick.
 *
 * Performance & memory:
 * - Time: 0 (pure compile-time constant)
 * - Space: 0 (no code or data generated)
 *
 * Example:
 * usize align = ALIGN_OF(double);  // usually 8
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define ALIGN_OF(type) _Alignof(type)
#else
    /* C99 fallback using offsetof trick */
    #define ALIGN_OF(type) offsetof(struct { char c; type member; }, member)
#endif

/**
 * @def ALIGN_MAX(a, b)
 * @brief Maximum of two alignment values
 *
 * Useful when computing alignment for unions, generic containers,
 * or structures with multiple members that have different alignments.
 *
 * Performance & memory:
 * - Time: 0 (compile-time ternary expression)
 * - Space: 0
 *
 * Example:
 * usize worst = ALIGN_MAX(ALIGN_OF(int), ALIGN_OF(double));
 */
#define ALIGN_MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * @def PACKED
 * @def CACHE_ALIGNED
 * @def ALIGNAS(n)
 * @brief Opportunistic compiler attributes for struct packing and alignment
 *
 * - PACKED: removes all padding between struct members
 * - CACHE_ALIGNED: aligns struct to cache line size (from limits.h)
 * - ALIGNAS(n): aligns struct or variable to exactly n bytes
 *
 * Disabled if CANON_NO_GNU_EXTENSIONS is defined (strict C99 mode).
 *
 * Performance & memory:
 * - Time: 0 (affects only layout, no runtime cost)
 * - Space: PACKED reduces struct size (eliminates padding bytes);
 *          CACHE_ALIGNED and ALIGNAS may increase size for better
 *          cache performance or hardware requirements (e.g. SIMD)
 *
 * Example:
 * typedef struct PACKED { u32 id; u8 flag; } PackedHeader;
 * typedef struct CACHE_ALIGNED { u64 counters[8]; } ThreadLocalData;
 */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(CANON_NO_GNU_EXTENSIONS)
    #define PACKED               __attribute__((packed))
    #define CACHE_ALIGNED        __attribute__((aligned(CANON_CACHE_LINE)))
    #define ALIGNAS(n)           __attribute__((aligned(n)))
#else
    #define PACKED               /* fallback: no packing */
    #define CACHE_ALIGNED        /* fallback: no cache alignment */
    #define ALIGNAS(n)           /* fallback: no explicit alignment */
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Pointer arithmetic
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Advances pointer p forward by n bytes
 *
 * @param p Pointer to advance (NULL-safe — returns NULL if p == NULL)
 * @param n Number of bytes to advance (overflow-checked in debug builds)
 * @return p + n as void*
 *
 * @pre n <= CANON_USIZE_MAX - (usize)(uintptr_t)p (overflow-safe)
 *
 * Example:
 * ```c
 * void* next_slot = ptr_offset(buffer, slot_size);
 * ```
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void* ptr_offset(void* p, usize n) {
    if (!p) return NULL;
    ensure_msg((usize)(uintptr_t)p <= CANON_USIZE_MAX - n,
        "ptr_offset: byte offset would overflow address space");
    return (void*)((u8*)p + n);
}
/**
 * @brief Retreats pointer p backward by n bytes
 *
 * @param p Pointer to retreat (NULL-safe — returns NULL if p == NULL)
 * @param n Number of bytes to retreat (underflow-checked in debug builds)
 * @return p - n as void*
 *
 * @pre (usize)(uintptr_t)p >= n (underflow-safe)
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void* ptr_retreat(void* p, usize n) {
    if (!p) return NULL;
    ensure_msg((usize)(uintptr_t)p >= n,
        "ptr_retreat: byte retreat would underflow address space");
    return (void*)((u8*)p - n);
}
/**
 * @brief Advances a const pointer forward by n bytes
 *
 * Const-correct variant of ptr_offset.
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline const void* ptr_offset_const(const void* p, usize n) {
    if (!p) return NULL;
    ensure_msg((usize)(uintptr_t)p <= CANON_USIZE_MAX - n,
        "ptr_offset_const: byte offset would overflow address space");
    return (const void*)((const u8*)p + n);
}
/**
 * @brief Returns the signed byte distance from `from` to `to` (to - from)
 *
 * Positive if to > from, negative if to < from, zero if equal.
 *
 * @param to End pointer
 * @param from Start pointer
 * @return Signed byte distance (isize)
 *
 * @pre Both pointers point into the same allocation or one past its end
 *
 * Example:
 * ```c
 * isize used = ptr_diff(arena->current, arena->start);
 * ```
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline isize ptr_diff(const void* to, const void* from) {
    return (isize)((const u8*)to - (const u8*)from);
}
/**
 * @brief Returns the unsigned byte distance from `from` to `to`
 *
 * @param to End pointer (must be >= from)
 * @param from Start pointer
 * @return Unsigned byte distance (usize)
 *
 * @pre to >= from — checked via require_msg()
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline usize ptr_span(const void* to, const void* from) {
    require_msg((const u8*)to >= (const u8*)from,
        "ptr_span: 'to' must be >= 'from'");
    return (usize)((const u8*)to - (const u8*)from);
}
/* ════════════════════════════════════════════════════════════════════════════
   Bounds checking
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Returns true if p lies within [region_start, region_end)
 *
 * All three pointers must be in the same allocation for defined behavior.
 *
 * @param p Pointer to test
 * @param region_start Inclusive lower bound
 * @param region_end Exclusive upper bound
 * @return true if region_start <= p < region_end
 *
 * @note Returns false if any argument is NULL.
 *
 * Example:
 * ```c
 * bool safe = ptr_in_range(user_ptr, buf, buf + buf_size);
 * ```
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool ptr_in_range(const void* p,
                                 const void* region_start,
                                 const void* region_end) {
    if (!p || !region_start || !region_end) return false;
    return (const u8*)p >= (const u8*)region_start &&
           (const u8*)p < (const u8*)region_end;
}
/**
 * @brief Returns true if [p, p+len) lies fully within [region_start, region_end)
 *
 * Overflow-safe — handles the case where p + len would wrap.
 *
 * @param p Start of range to test
 * @param len Length of range in bytes
 * @param region_start Inclusive lower bound of containing region
 * @param region_end Exclusive upper bound of containing region
 * @return true if the entire range [p, p+len) is inside [region_start, region_end)
 *
 * @note Returns false if any pointer is NULL or len == 0 with p == NULL.
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool ptr_range_in_range(const void* p,
                                       usize len,
                                       const void* region_start,
                                       const void* region_end) {
    if (!p || !region_start || !region_end) return false;
    const u8* pb = (const u8*)p;
    const u8* rs = (const u8*)region_start;
    const u8* re = (const u8*)region_end;
    /* overflow-safe: check pb + len doesn't wrap before comparing */
    if (len > (usize)(re - rs)) return false;
    return pb >= rs && pb + len <= re;
}
/* ════════════════════════════════════════════════════════════════════════════
   Typed element access helpers
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Returns a void* pointer to element i in a typed array at base
 *
 * Equivalent to &((type*)base)[i] but without requiring a typed pointer.
 * Used by arena, pool, and vec internals.
 *
 * @param base Base pointer of the array
 * @param index Element index
 * @param elem_size sizeof(element type) in bytes
 * @return Pointer to element at index
 *
 * @pre base != NULL
 * @pre elem_size > 0
 *
 * Example:
 * ```c
 * void* slot = ptr_elem(pool->buffer, 3, sizeof(MyStruct));
 * ```
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void* ptr_elem(void* base, usize index, usize elem_size) {
    require_msg(base != NULL, "ptr_elem: base cannot be NULL");
    require_msg(elem_size > 0, "ptr_elem: elem_size must be > 0");
    return ptr_offset(base, index * elem_size);
}
/**
 * @brief Const variant of ptr_elem
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline const void* ptr_elem_const(const void* base, usize index, usize elem_size) {
    require_msg(base != NULL, "ptr_elem_const: base cannot be NULL");
    require_msg(elem_size > 0, "ptr_elem_const: elem_size must be > 0");
    return ptr_offset_const(base, index * elem_size);
}
/* ════════════════════════════════════════════════════════════════════════════
   Null safety
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Returns p if non-NULL, otherwise returns fallback
 *
 * @param p Pointer to test
 * @param fallback Value to return if p == NULL
 * @return p if p != NULL, else fallback
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void* ptr_or(void* p, void* fallback) {
    return p ? p : fallback;
}
/**
 * @brief Returns true if p is not NULL
 *
 * Explicit null check for use in require_msg and ensure_msg conditions.
 * Avoids implicit boolean conversion of pointers.
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool ptr_is_valid(const void* p) {
    return p != NULL;
}
/* ════════════════════════════════════════════════════════════════════════════
   Convenience macros
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @def PTR_OFFSET_OF(type, member)
 * @brief Byte offset of member within type (portable offsetof replacement)
 *
 * Uses the same technique as the standard offsetof macro.
 * Prefer this over bare offsetof for consistency with Canon-C naming.
 */
#define PTR_OFFSET_OF(type, member) \
    ((usize)((const u8*)&((type*)0)->member - (const u8*)0))
/**
 * @def PTR_CONTAINER_OF(ptr, type, member)
 * @brief Recover the enclosing struct pointer from a member pointer
 *
 * Given a pointer to `member` inside a `type`, returns a pointer to
 * the containing `type` instance. Commonly used for intrusive data
 * structures (intrusive lists, pool nodes, etc.)
 *
 * @param ptr Pointer to the member field
 * @param type Type of the containing struct
 * @param member Name of the member field within type
 *
 * Example:
 * ```c
 * typedef struct { int x; Node node; } MyItem;
 * Node* n = get_node();
 * MyItem* item = PTR_CONTAINER_OF(n, MyItem, node);
 * ```
 */
#define PTR_CONTAINER_OF(ptr, type, member) \
    ((type*)((u8*)(ptr) - PTR_OFFSET_OF(type, member)))
#endif /* CANON_CORE_PRIMITIVES_PTR_H */
