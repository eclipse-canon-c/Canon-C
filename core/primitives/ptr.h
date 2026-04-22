#ifndef CANON_CORE_PRIMITIVES_PTR_H
#define CANON_CORE_PRIMITIVES_PTR_H

#include <stddef.h>          /* offsetof */

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/checked.h"

/**
 * @file core/primitives/ptr.h
 * @brief Explicit, named pointer arithmetic and alignment utilities
 *
 * Centralizes all pointer manipulation performed by Canon-C modules:
 * arena bump allocation, pool block indexing, vec offset calculation,
 * slice construction, and buffer alignment checks.
 *
 * Design rules:
 * - All pointer arithmetic is named — no raw `+` on void* or char*
 * - Alignment is always explicit — no silent padding assumptions
 * - NULL handling is always defined — see "NULL handling" below
 * - Overflow is caught before it happens — no wraparound on offsets
 * - All operations work on void* or u8* — no type puns needed
 *
 * NULL handling:
 * Canon-C categorizes functions by whether NULL has meaningful semantics:
 *
 *   Null-tolerant  — NULL is a valid input with a defined safe response.
 *     These functions preserve NULL or return a safe default. Use them
 *     freely in code paths where NULL propagation is expected.
 *     Examples: ptr_offset(NULL, n)     → NULL
 *               ptr_is_aligned(NULL, a) → false
 *               ptr_align_padding(NULL) → 0
 *               ptr_or(NULL, fb)        → fb
 *
 *   Null-intolerant — NULL is a contract violation (programmer error).
 *     These functions trigger require_msg() when given NULL because the
 *     operation has no meaningful answer for NULL input.
 *     Examples: ptr_elem requires base != NULL
 *               ptr_span requires ordered pointers
 *               ptr_diff requires same-allocation pointers
 *
 * Each function's behavior is documented explicitly in its @pre clause.
 *
 * Dependency rule:
 * ptr.h lives in core/primitives/ and may only include from core/primitives/
 * and <stddef.h> (for offsetof). No other standard headers are included.
 *
 * Portability:
 * - Requires C99 or later
 * - Assumes flat address space (true for all hosted and most embedded targets)
 * - Pointer-to-integer conversion uses uintptr_t (via types.h)
 * - No platform-specific intrinsics
 *
 * Thread-safety:
 * All functions are pure or operate on caller-supplied pointers.
 * No shared state — fully thread-safe.
 *
 * Performance:
 * All functions are static inline — zero call overhead.
 * Compilers reduce every operation here to 1-3 instructions.
 *
 * Quick start:
 * ```c
 * #include "core/primitives/ptr.h"
 *
 * void* next    = ptr_offset(buf, 64);          // advance by n bytes
 * void* aligned = ptr_align_up(raw, 16);        // align up to 16-byte boundary
 * isize dist    = ptr_diff(end, start);         // signed byte distance
 * bool  ok      = ptr_in_range(p, buf, end);    // bounds check
 * usize padded  = align_up(size, alignof(double)); // round size up
 * ```
 *
 * @sa core/slice.h  — typed {ptr, len} views built on top of ptr.h
 * @sa core/arena.h  — bump allocator that uses ptr_align_up and ptr_offset
 * @sa core/memory.h — safe memcpy/memmove wrappers
 */

/* Ensure limits.h provides CANON_USIZE_MAX consistent with SIZE_MAX.
 * Every overflow check in this file depends on CANON_USIZE_MAX being
 * the true maximum value representable in usize. If limits.h ever
 * drifts, catch it at compile time rather than at runtime. */
static_require(CANON_USIZE_MAX == SIZE_MAX,
               canon_usize_max_matches_size_max);

/* ── Compiler attributes ─────────────────────────────────────────────────────
 *
 * PACKED        — removes all padding between struct members
 * CACHE_ALIGNED — aligns to cache line size (from limits.h)
 * ALIGNAS(n)    — aligns to exactly n bytes
 *
 * Disabled when CANON_NO_GNU_EXTENSIONS is defined (strict C99 mode).
 *
 * IMPORTANT — correctness risk in strict C99 mode:
 * When CANON_NO_GNU_EXTENSIONS is defined, these attributes expand to
 * nothing. If your code RELIES on packing (network protocols, hardware
 * register layouts, on-wire formats) or cache-line alignment (false
 * sharing avoidance, SIMD alignment requirements), the attribute's
 * absence silently changes struct layout and may cause correctness bugs.
 *
 * If you use PACKED for layout-critical structs, either:
 *   1. Commit to compiling with GNU extensions available, or
 *   2. Use explicit byte-level serialization (read/write individual
 *      fields with memcpy) instead of trusting struct layout.
 * ─────────────────────────────────────────────────────────────────────────── */
#if (defined(__GNUC__) || defined(__clang__)) && !defined(CANON_NO_GNU_EXTENSIONS)
#   define PACKED          __attribute__((packed))
#   define CACHE_ALIGNED   __attribute__((aligned(CANON_CACHE_LINE)))
#   define ALIGNAS(n)      __attribute__((aligned(n)))
#else
#   define PACKED          /* no packing in strict C99 mode — see comment above */
#   define CACHE_ALIGNED   /* no cache alignment in strict C99 mode */
#   define ALIGNAS(n)      /* no explicit alignment in strict C99 mode */
#endif

/* ── Compile-time alignment helpers ──────────────────────────────────────────
 *
 * ALIGN_OF(type)  — alignment requirement of a type, in bytes
 * ALIGN_MAX(a, b) — larger of two alignment values
 *
 * Both are pure compile-time expressions: zero runtime cost, zero code size.
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * @def ALIGN_OF(type)
 * @brief Compile-time alignment requirement of a type (in bytes)
 *
 * Uses C11 _Alignof when available; falls back to the offsetof trick on C99.
 * The fallback is well-defined: it computes the offset of the first member
 * after a char in an anonymous struct, which equals the member's alignment.
 *
 * Example:
 *   usize a = ALIGN_OF(double);  // typically 8
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#   define ALIGN_OF(type)    _Alignof(type)
#else
#   define ALIGN_OF(type)    offsetof(struct { char _c; type _m; }, _m)
#endif

/**
 * @def ALIGN_MAX(a, b)
 * @brief Larger of two alignment values (compile-time)
 *
 * Useful when computing alignment for unions or generic containers
 * whose members have different natural alignments.
 *
 * Note: evaluates arguments twice. Pass compile-time constants only.
 *
 * Example:
 *   usize worst = ALIGN_MAX(ALIGN_OF(int), ALIGN_OF(double));
 */
#define ALIGN_MAX(a, b)  ((a) > (b) ? (a) : (b))

/* ── Integer alignment ───────────────────────────────────────────────────────
 *
 * All functions require align to be a nonzero power of two.
 * Violations are caught by require_msg() in debug builds.
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns true if n is a nonzero power of two
 *
 * Used internally and exported for callers that want to validate alignment
 * values before passing them into other ptr.h functions.
 */
static inline bool is_power_of_two(usize n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/**
 * @brief Returns true if n is a multiple of align
 *
 * @pre is_power_of_two(align)
 */
static inline bool is_aligned(usize n, usize align) {
    require_msg(is_power_of_two(align), "is_aligned: align must be a nonzero power of two");
    return (n & (align - 1)) == 0;
}

/**
 * @brief Rounds n up to the nearest multiple of align
 *
 * @pre is_power_of_two(align)
 *
 * Examples:
 *   align_up(13, 8) → 16
 *   align_up(16, 8) → 16   (already aligned)
 *   align_up( 0, 8) →  0
 */
static inline usize align_up(usize n, usize align) {
    require_msg(is_power_of_two(align), "align_up: align must be a nonzero power of two");
    return (n + align - 1) & ~(align - 1);
}

/**
 * @brief Rounds n down to the nearest multiple of align
 *
 * @pre is_power_of_two(align)
 *
 * Examples:
 *   align_down(13, 8) → 8
 *   align_down(16, 8) → 16  (already aligned)
 */
static inline usize align_down(usize n, usize align) {
    require_msg(is_power_of_two(align), "align_down: align must be a nonzero power of two");
    return n & ~(align - 1);
}

/**
 * @brief Returns the padding bytes needed to align n to align
 *
 * @pre is_power_of_two(align)
 *
 * Examples:
 *   align_padding(13, 8) → 3   (13 + 3 = 16)
 *   align_padding(16, 8) → 0
 */
static inline usize align_padding(usize n, usize align) {
    return align_up(n, align) - n;
}

/* ── Pointer alignment ───────────────────────────────────────────────────────
 *
 * Null-tolerant: NULL in → NULL (or false/0) out.
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Rounds pointer p up to the nearest align-byte boundary
 *
 * @pre is_power_of_two(align)
 * @return Aligned pointer >= p, or NULL if p == NULL
 */
static inline void* ptr_align_up(void* p, usize align) {
    require_msg(is_power_of_two(align), "ptr_align_up: align must be a nonzero power of two");
    if (!p) return NULL;
    return (void*)(uintptr_t)align_up((usize)(uintptr_t)p, align);
}

/**
 * @brief Rounds pointer p down to the nearest align-byte boundary
 *
 * @pre is_power_of_two(align)
 * @return Aligned pointer <= p, or NULL if p == NULL
 */
static inline void* ptr_align_down(void* p, usize align) {
    require_msg(is_power_of_two(align), "ptr_align_down: align must be a nonzero power of two");
    if (!p) return NULL;
    return (void*)(uintptr_t)align_down((usize)(uintptr_t)p, align);
}

/**
 * @brief Returns true if pointer p is aligned to align bytes
 *
 * @pre is_power_of_two(align)
 * @return false if p == NULL
 */
static inline bool ptr_is_aligned(const void* p, usize align) {
    require_msg(is_power_of_two(align), "ptr_is_aligned: align must be a nonzero power of two");
    if (!p) return false;
    return is_aligned((usize)(uintptr_t)p, align);
}

/**
 * @brief Returns padding bytes needed to align pointer p to align
 *
 * @pre is_power_of_two(align)
 * @return 0 if p == NULL or already aligned
 */
static inline usize ptr_align_padding(const void* p, usize align) {
    if (!p) return 0;
    return align_padding((usize)(uintptr_t)p, align);
}

/* ── Pointer arithmetic ──────────────────────────────────────────────────────
 *
 * ptr_offset / ptr_offset_const : null-tolerant — NULL in → NULL out
 * ptr_retreat                   : null-tolerant — NULL in → NULL out
 * ptr_diff                      : null-intolerant — contract violation on NULL
 * ptr_span                      : null-intolerant — contract violation on NULL
 *
 * Overflow/underflow is caught by ensure_msg() in debug builds.
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Advances pointer p forward by n bytes
 *
 * @pre (uintptr_t)p + n does not overflow
 * @return p + n, or NULL if p == NULL
 */
static inline void* ptr_offset(void* p, usize n) {
    if (!p) return NULL;
    ensure_msg((usize)(uintptr_t)p <= CANON_USIZE_MAX - n,
               "ptr_offset: offset would overflow address space");
    return (void*)((u8*)p + n);
}

/**
 * @brief Const-correct variant of ptr_offset
 */
static inline const void* ptr_offset_const(const void* p, usize n) {
    if (!p) return NULL;
    ensure_msg((usize)(uintptr_t)p <= CANON_USIZE_MAX - n,
               "ptr_offset_const: offset would overflow address space");
    return (const void*)((const u8*)p + n);
}

/**
 * @brief Retreats pointer p backward by n bytes
 *
 * @warning This checks only that the raw address does not underflow zero.
 *          It does NOT verify that the result stays within the same
 *          allocation. The caller is responsible for that invariant.
 *
 * @pre (uintptr_t)p >= n
 * @return p - n, or NULL if p == NULL
 */
static inline void* ptr_retreat(void* p, usize n) {
    if (!p) return NULL;
    ensure_msg((usize)(uintptr_t)p >= n,
               "ptr_retreat: retreat would underflow address space");
    return (void*)((u8*)p - n);
}

/**
 * @brief Signed byte distance from `from` to `to`  (to - from)
 *
 * Positive if to > from, negative if to < from, zero if equal.
 *
 * @pre to != NULL
 * @pre from != NULL
 * @pre Both pointers are in the same allocation (or one-past-end).
 *      Subtracting pointers from different allocations is undefined
 *      behavior in C — Canon-C cannot detect this at runtime but
 *      requires the caller to honor it.
 *
 * Example:
 *   isize used = ptr_diff(arena->cur, arena->start);
 */
static inline isize ptr_diff(const void* to, const void* from) {
    require_msg(to   != NULL, "ptr_diff: 'to' is NULL");
    require_msg(from != NULL, "ptr_diff: 'from' is NULL");
    return (isize)((const u8*)to - (const u8*)from);
}

/**
 * @brief Unsigned byte distance from `from` to `to`
 *
 * @pre to   != NULL
 * @pre from != NULL
 * @pre to >= from  (checked by require_msg)
 * @pre Both pointers are in the same allocation (caller responsibility —
 *      pointer comparison across allocations is undefined behavior)
 *
 * Example:
 *   usize remaining = ptr_span(arena->end, arena->cur);
 */
static inline usize ptr_span(const void* to, const void* from) {
    require_msg(to   != NULL, "ptr_span: 'to' is NULL");
    require_msg(from != NULL, "ptr_span: 'from' is NULL");
    require_msg((const u8*)to >= (const u8*)from,
                "ptr_span: 'to' must be >= 'from'");
    return (usize)((const u8*)to - (const u8*)from);
}

/* ── Bounds checking ─────────────────────────────────────────────────────────
 *
 * Null-tolerant: all functions return false on any NULL argument.
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns true if p lies within [region_start, region_end)
 *
 * @pre All three pointers are in the same allocation for defined behavior.
 * @return false if any argument is NULL
 *
 * Example:
 *   bool safe = ptr_in_range(user_ptr, buf, buf + buf_size);
 */
static inline bool ptr_in_range(const void*  p,
                                 const void*  region_start,
                                 const void*  region_end) {
    if (!p || !region_start || !region_end) return false;
    return (const u8*)p >= (const u8*)region_start &&
           (const u8*)p <  (const u8*)region_end;
}

/**
 * @brief Returns true if [p, p+len) lies fully within [region_start, region_end)
 *
 * Overflow-safe: never computes p + len directly.
 *
 * @return false if any pointer is NULL
 *
 * Example:
 *   bool safe = ptr_range_in_range(user_ptr, user_len, buf, buf + buf_size);
 */
static inline bool ptr_range_in_range(const void*  p,
                                       usize        len,
                                       const void*  region_start,
                                       const void*  region_end) {
    if (!p || !region_start || !region_end) return false;
    const u8* pb = (const u8*)p;
    const u8* rs = (const u8*)region_start;
    const u8* re = (const u8*)region_end;
    if (pb < rs)               return false;  /* starts before region */
    if (len > (usize)(re - pb)) return false;  /* overflow-safe: no pb+len */
    return true;
}

/* ── Typed element access ────────────────────────────────────────────────────
 *
 * Null-intolerant: NULL base is a contract violation. Indexing into a
 * nonexistent array has no meaningful answer — this is a programmer error,
 * not a case where NULL should propagate.
 *
 * The index * elem_size multiply is overflow-checked before use.
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns a pointer to element i in a flat array at base
 *
 * Equivalent to &((T*)base)[i] without requiring a typed pointer.
 *
 * @pre base      != NULL  (contract violation if NULL — indexing into a
 *                          nonexistent array has no meaningful answer)
 * @pre elem_size > 0
 * @pre index * elem_size does not overflow usize
 *
 * Example:
 *   void* slot = ptr_elem(pool->buffer, 3, sizeof(MyStruct));
 */
static inline void* ptr_elem(void* base, usize index, usize elem_size) {
    require_msg(base      != NULL, "ptr_elem: base cannot be NULL");
    require_msg(elem_size >  0,    "ptr_elem: elem_size must be > 0");
    ensure_msg(index <= CANON_USIZE_MAX / elem_size,
               "ptr_elem: index * elem_size overflows usize");
    return ptr_offset(base, index * elem_size);
}

/**
 * @brief Const variant of ptr_elem
 *
 * @pre base      != NULL  (contract violation if NULL)
 * @pre elem_size > 0
 * @pre index * elem_size does not overflow usize
 */
static inline const void* ptr_elem_const(const void* base, usize index, usize elem_size) {
    require_msg(base      != NULL, "ptr_elem_const: base cannot be NULL");
    require_msg(elem_size >  0,    "ptr_elem_const: elem_size must be > 0");
    ensure_msg(index <= CANON_USIZE_MAX / elem_size,
               "ptr_elem_const: index * elem_size overflows usize");
    return ptr_offset_const(base, index * elem_size);
}

/* ── Null safety ─────────────────────────────────────────────────────────────
 *
 * Null-tolerant utility functions — NULL is a valid input with a defined
 * safe response. Use freely in code paths where NULL propagation is expected.
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns p if non-NULL, otherwise fallback
 */
static inline void* ptr_or(void* p, void* fallback) {
    return p ? p : fallback;
}

/**
 * @brief Const variant of ptr_or
 */
static inline const void* ptr_or_const(const void* p, const void* fallback) {
    return p ? p : fallback;
}

/**
 * @brief Returns true if p is not NULL
 *
 * Avoids implicit boolean conversion of pointers in contract macros.
 */
static inline bool ptr_is_valid(const void* p) {
    return p != NULL;
}

/* ── Struct introspection macros ─────────────────────────────────────────────
 *
 * PTR_OFFSET_OF    — byte offset of a member within a struct
 * PTR_CONTAINER_OF — recover enclosing struct from a member pointer
 *
 * Both delegate to offsetof() from <stddef.h> (included at top of file),
 * avoiding the undefined null-pointer dereference of a hand-rolled version.
 * ─────────────────────────────────────────────────────────────────────────── */

/**
 * @def PTR_OFFSET_OF(type, member)
 * @brief Byte offset of member within type
 *
 * Thin, consistently-named wrapper over the standard offsetof macro.
 *
 * Example:
 *   usize off = PTR_OFFSET_OF(MyStruct, next);
 */
#define PTR_OFFSET_OF(type, member) \
    ((usize)offsetof(type, member))

/**
 * @def PTR_CONTAINER_OF(ptr, type, member)
 * @brief Recover the enclosing struct pointer from a member pointer
 *
 * Given a pointer to `member` inside a `type`, returns a pointer to
 * the containing `type` instance. Used for intrusive data structures.
 *
 * @param ptr    Pointer to the member field
 * @param type   Type of the containing struct
 * @param member Name of the member field within type
 *
 * Example:
 *   typedef struct { int x; Node node; } MyItem;
 *   MyItem* item = PTR_CONTAINER_OF(node_ptr, MyItem, node);
 */
#define PTR_CONTAINER_OF(ptr, type, member) \
    ((type*)((u8*)(ptr) - PTR_OFFSET_OF(type, member)))

#endif /* CANON_CORE_PRIMITIVES_PTR_H */
