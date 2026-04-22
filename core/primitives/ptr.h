/**
 * @file ptr.h
 * @brief Explicit, named pointer arithmetic and alignment utilities for Canon-C
 *
 * Centralizes all pointer manipulation performed by Canon-C modules:
 * arena bump allocation, pool block indexing, vec offset calculation,
 * slice construction, and buffer alignment checks. Replaces raw pointer
 * arithmetic with named operations that document intent at the call site.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicitness: All pointer arithmetic is named — no raw `+` on void*/char*
 * - Alignment safety: Every alignment operation is explicit — no silent
 *   padding assumptions
 * - NULL discipline: Every function documents its NULL behavior — null-tolerant
 *   or null-intolerant (see "NULL handling" below)
 * - Overflow safety: Arithmetic is overflow-checked before it happens — no
 *   silent wraparound on offsets
 * - Type neutrality: All operations work on void* or u8* — no type puns needed
 *
 * NULL handling:
 * ────────────────────────────────────────────────────────────────────────────
 * Canon-C categorizes functions by whether NULL has meaningful semantics:
 *
 *   Null-tolerant — NULL is a valid input with a defined safe response.
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
 *     Examples: ptr_elem     requires base != NULL
 *               ptr_span     requires ordered pointers
 *               ptr_diff     requires same-allocation pointers
 *
 * Each function's behavior is documented explicitly in its @pre clause.
 *
 * Formal verification:
 * ────────────────────────────────────────────────────────────────────────────
 * Every function in this header carries an ACSL contract suitable for
 * Frama-C WP. The Typed+Cast memory model is required because ptr.h
 * performs void* → u8* casts extensively for byte-level pointer arithmetic
 * (same rationale as compare.h — see VERIFY-005).
 *
 * All functions are pure (assigns \nothing) — they take pointers and
 * integers by value and return results without modifying any memory.
 *
 * Compile-time invariant: static_require asserts CANON_USIZE_MAX equals
 * SIZE_MAX at compile time. Every overflow check in this file depends on
 * CANON_USIZE_MAX being the true maximum value representable in usize —
 * if limits.h ever drifts, the build fails immediately rather than
 * producing subtly-wrong overflow checks at runtime.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All operations: O(1) — constant time pointer/integer operations
 * - Every function is static inline → zero call overhead
 * - Compilers reduce every operation here to 1–3 machine instructions
 * - Alignment operations compile to a single AND (align_down) or
 *   ADD+AND (align_up)
 * - Pointer arithmetic compiles to a single LEA or ADD instruction
 * - No heap allocation, no hidden branches, no runtime dispatch
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are pure or operate on caller-supplied pointers.
 * No shared mutable state — fully thread-safe.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (inline functions, stdint.h via types.h)
 * - Assumes flat address space (true for all hosted and most embedded targets)
 * - Pointer-to-integer conversion uses uintptr_t (via types.h)
 * - No platform-specific intrinsics
 * - Only includes <stddef.h> from the standard library (for offsetof)
 * - PACKED, CACHE_ALIGNED, ALIGNAS use GCC/Clang attributes when available,
 *   degrade to no-ops in strict C99 mode (see attribute-specific docs)
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * ptr.h lives in core/primitives/ and may only include from core/primitives/
 * and <stddef.h> (for offsetof). No other standard headers are included.
 * This keeps ptr.h usable in freestanding environments and predictable as
 * a foundation for higher-layer modules.
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Arena bump allocation: void* next = ptr_align_up(arena->cur, align);
 *                          arena->cur = ptr_offset(next, size);
 * - Pool block indexing:   void* slot = ptr_elem(pool->buffer, i, block_size);
 * - Slice construction:    usize len = ptr_span(end, start);
 * - Bounds checking:       if (!ptr_in_range(p, buf, buf + sz)) return error;
 * - Alignment checks:      if (!ptr_is_aligned(ptr, 16)) ...
 * - Intrusive containers:  MyItem* item = PTR_CONTAINER_OF(node_ptr,
 *                                                         MyItem, node);
 * - Size rounding:         usize padded = align_up(size, ALIGN_OF(double));
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Pointer comparisons across different allocations (UB in C)
 * - Raw pointer arithmetic in performance-critical SIMD code (use intrinsics)
 * - Cross-segment addressing on segmented architectures (flat-address
 *   assumption doesn't hold)
 * - Security-sensitive bounds checks where a malicious caller supplies
 *   region_start/region_end (the comparison semantics assume same allocation)
 *
 * @sa types.h    — uintptr_t, usize, isize, u8 definitions
 * @sa limits.h   — CANON_USIZE_MAX, CANON_CACHE_LINE
 * @sa contract.h — require_msg(), ensure_msg(), static_require
 * @sa checked.h  — overflow-checked arithmetic used by ptr_elem
 */

#ifndef CANON_CORE_PRIMITIVES_PTR_H
#define CANON_CORE_PRIMITIVES_PTR_H

#include <stddef.h>          /* offsetof */

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/primitives/checked.h"

/* Compile-time invariant: CANON_USIZE_MAX must equal SIZE_MAX.
 * Every overflow check in this file depends on CANON_USIZE_MAX being
 * the true maximum value representable in usize. If limits.h drifts,
 * catch it at compile time rather than at runtime. */
static_require(CANON_USIZE_MAX == SIZE_MAX,
               canon_usize_max_matches_size_max);

/* ============================================================================
 * Compiler Attributes
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
 * ========================================================================= */

#if (defined(__GNUC__) || defined(__clang__)) && !defined(CANON_NO_GNU_EXTENSIONS)
#   define PACKED          __attribute__((packed))
#   define CACHE_ALIGNED   __attribute__((aligned(CANON_CACHE_LINE)))
#   define ALIGNAS(n)      __attribute__((aligned(n)))
#else
#   define PACKED          /* no packing in strict C99 mode — see comment above */
#   define CACHE_ALIGNED   /* no cache alignment in strict C99 mode */
#   define ALIGNAS(n)      /* no explicit alignment in strict C99 mode */
#endif

/* ============================================================================
 * Compile-Time Alignment Helpers
 * ========================================================================= */

/**
 * @def ALIGN_OF(type)
 * @brief Compile-time alignment requirement of a type (in bytes)
 *
 * Uses C11 `_Alignof` when available; falls back to the offsetof trick
 * on C99. The fallback is well-defined: it computes the offset of the
 * first member after a char in an anonymous struct, which equals the
 * member's alignment.
 *
 * @param type C type whose alignment is queried
 *
 * @return Alignment requirement in bytes (always a power of two)
 *
 * @remark Pure compile-time expression: zero runtime cost, zero code size
 * @remark The C99 fallback triggers -Wgnu-offsetof-extensions on Clang
 *         and C4116 on MSVC. Use only in .h/.c files that can tolerate
 *         the warning, or compile with C11.
 *
 * Example:
 * ```c
 * usize a = ALIGN_OF(double);    // typically 8
 * usize p = ALIGN_OF(void*);     // 4 on 32-bit, 8 on 64-bit
 * ```
 *
 * @sa ALIGN_MAX()
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
 * @param a First alignment value
 * @param b Second alignment value
 *
 * @return Larger of a and b
 *
 * @remark Evaluates arguments twice — pass compile-time constants only.
 *         Passing expressions with side effects produces those side
 *         effects twice.
 *
 * Example:
 * ```c
 * usize worst = ALIGN_MAX(ALIGN_OF(int), ALIGN_OF(double));  // 8 typically
 * ```
 *
 * @sa ALIGN_OF()
 */
#define ALIGN_MAX(a, b)  ((a) > (b) ? (a) : (b))

/* ============================================================================
 * Integer Alignment Operations
 *
 * Operate on usize values. All require align to be a nonzero power of two.
 * Violations are caught by require_msg() in debug builds.
 * ========================================================================= */

/**
 * @brief Test if value is a nonzero power of two
 *
 * @param n Value to test
 *
 * @return true if n is 1, 2, 4, 8, 16, ...; false otherwise (including 0)
 *
 * @remark Compiles to: (n != 0) && ((n & (n - 1)) == 0)
 * @remark Zero is explicitly not a power of two
 *
 * Example:
 * ```c
 * is_power_of_two(16)  // → true
 * is_power_of_two(15)  // → false
 * is_power_of_two(0)   // → false
 * ```
 *
 * @sa is_aligned(), align_up(), ptr_is_aligned()
 */
/*@
    assigns \nothing;
    ensures \result <==> (n > 0 && (n & (n - 1)) == 0);
 */
static inline bool is_power_of_two(usize n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/**
 * @brief Test if integer value is aligned to a power-of-two boundary
 *
 * @param n     Integer value to test
 * @param align Alignment requirement (must be a nonzero power of two)
 *
 * @return true if n is a multiple of align, false otherwise
 *
 * @pre is_power_of_two(align)
 *
 * @remark Compiles to: (n & (align - 1)) == 0
 *
 * Example:
 * ```c
 * is_aligned(16, 8)  // → true   (16 = 2 * 8)
 * is_aligned(17, 8)  // → false  (17 = 16 + 1)
 * is_aligned(0,  8)  // → true   (0 is aligned to anything)
 * ```
 *
 * @sa align_up(), align_down(), ptr_is_aligned()
 */
/*@
    requires align > 0;
    requires (align & (align - 1)) == 0;
    assigns  \nothing;
    ensures  \result <==> ((n & (align - 1)) == 0);
 */
static inline bool is_aligned(usize n, usize align) {
    require_msg(is_power_of_two(align), "is_aligned: align must be a nonzero power of two");
    return (n & (align - 1)) == 0;
}

/**
 * @brief Round n up to the nearest multiple of align
 *
 * @param n     Value to round
 * @param align Alignment requirement (must be a nonzero power of two)
 *
 * @return Smallest multiple of align that is >= n
 *
 * @pre is_power_of_two(align)
 * @pre n <= CANON_USIZE_MAX - (align - 1)  (prevents overflow)
 *
 * @remark Compiles to: (n + align - 1) & ~(align - 1)
 * @remark align_up(0, a) == 0 for any valid a
 * @remark Idempotent on already-aligned values
 *
 * Example:
 * ```c
 * align_up(13, 8)  // → 16
 * align_up(16, 8)  // → 16  (already aligned)
 * align_up( 0, 8)  // → 0
 * ```
 *
 * @sa align_down(), align_padding(), ptr_align_up()
 */
/*@
    requires align > 0;
    requires (align & (align - 1)) == 0;
    requires n <= CANON_USIZE_MAX - (align - 1);
    assigns  \nothing;
    ensures  \result == ((n + align - 1) & ~(align - 1));
    ensures  (\result & (align - 1)) == 0;
    ensures  \result >= n;
 */
static inline usize align_up(usize n, usize align) {
    require_msg(is_power_of_two(align), "align_up: align must be a nonzero power of two");
    return (n + align - 1) & ~(align - 1);
}

/**
 * @brief Round n down to the nearest multiple of align
 *
 * @param n     Value to round
 * @param align Alignment requirement (must be a nonzero power of two)
 *
 * @return Largest multiple of align that is <= n
 *
 * @pre is_power_of_two(align)
 *
 * @remark Compiles to: n & ~(align - 1)
 * @remark align_down(0, a) == 0 for any valid a
 * @remark Idempotent on already-aligned values
 * @remark Cannot overflow — always produces a value <= n
 *
 * Example:
 * ```c
 * align_down(13, 8)  // → 8
 * align_down(16, 8)  // → 16  (already aligned)
 * align_down( 7, 8)  // → 0
 * ```
 *
 * @sa align_up(), align_padding(), ptr_align_down()
 */
/*@
    requires align > 0;
    requires (align & (align - 1)) == 0;
    assigns  \nothing;
    ensures  \result == (n & ~(align - 1));
    ensures  (\result & (align - 1)) == 0;
    ensures  \result <= n;
 */
static inline usize align_down(usize n, usize align) {
    require_msg(is_power_of_two(align), "align_down: align must be a nonzero power of two");
    return n & ~(align - 1);
}

/**
 * @brief Compute padding bytes needed to align n to the next align boundary
 *
 * @param n     Current value (often a size or offset)
 * @param align Alignment requirement (must be a nonzero power of two)
 *
 * @return Padding bytes such that (n + result) is a multiple of align
 *
 * @pre is_power_of_two(align)
 * @pre n <= CANON_USIZE_MAX - (align - 1)  (inherited from align_up)
 *
 * @remark Always returns a value in [0, align - 1]
 * @remark Returns 0 when n is already aligned
 * @remark Equivalent to: align_up(n, align) - n
 *
 * Example:
 * ```c
 * align_padding(13, 8)  // → 3   (13 + 3 = 16)
 * align_padding(16, 8)  // → 0   (already aligned)
 * align_padding( 0, 8)  // → 0
 * ```
 *
 * @sa align_up(), align_down(), ptr_align_padding()
 */
/*@
    requires align > 0;
    requires (align & (align - 1)) == 0;
    requires n <= CANON_USIZE_MAX - (align - 1);
    assigns  \nothing;
    ensures  \result < align;
    ensures  \result == ((n + align - 1) & ~(align - 1)) - n;
 */
static inline usize align_padding(usize n, usize align) {
    return align_up(n, align) - n;
}

/* ============================================================================
 * Pointer Alignment Operations (Null-Tolerant)
 *
 * NULL is a valid input with a defined safe response. Use freely in code
 * paths where NULL propagation is expected.
 * ========================================================================= */

/**
 * @brief Round pointer p up to the nearest align-byte boundary
 *
 * @param p     Pointer to align (may be NULL)
 * @param align Alignment requirement (must be a nonzero power of two)
 *
 * @return Aligned pointer >= p, or NULL if p == NULL
 *
 * @pre is_power_of_two(align)
 *
 * @remark Null-tolerant: NULL in → NULL out
 * @remark Useful for arena bump allocation: align the cursor before stamping
 *         an object with nontrivial alignment
 *
 * Example:
 * ```c
 * void* raw     = arena->cur;
 * void* aligned = ptr_align_up(raw, 16);  // align for SIMD
 * arena->cur    = ptr_offset(aligned, sizeof(MyStruct));
 * ```
 *
 * @sa ptr_align_down(), ptr_is_aligned(), align_up()
 */
/*@
    requires align > 0;
    requires (align & (align - 1)) == 0;
    assigns  \nothing;
    behavior null:
        assumes p == \null;
        ensures \result == \null;
    behavior nonnull:
        assumes p != \null;
        ensures \result != \null;
    complete behaviors;
    disjoint behaviors;
 */
static inline void* ptr_align_up(void* p, usize align) {
    require_msg(is_power_of_two(align), "ptr_align_up: align must be a nonzero power of two");
    if (!p) return NULL;
    return (void*)(uintptr_t)align_up((usize)(uintptr_t)p, align);
}

/**
 * @brief Round pointer p down to the nearest align-byte boundary
 *
 * @param p     Pointer to align (may be NULL)
 * @param align Alignment requirement (must be a nonzero power of two)
 *
 * @return Aligned pointer <= p, or NULL if p == NULL
 *
 * @pre is_power_of_two(align)
 *
 * @remark Null-tolerant: NULL in → NULL out
 * @remark Cannot underflow past NULL — always stays within the original
 *         allocation if p was inside one
 *
 * Example:
 * ```c
 * void* p         = /* some pointer into a buffer */;
 * void* page_base = ptr_align_down(p, 4096);  // round to page boundary
 * ```
 *
 * @sa ptr_align_up(), ptr_is_aligned(), align_down()
 */
/*@
    requires align > 0;
    requires (align & (align - 1)) == 0;
    assigns  \nothing;
    behavior null:
        assumes p == \null;
        ensures \result == \null;
    behavior nonnull:
        assumes p != \null;
        ensures \result != \null;
    complete behaviors;
    disjoint behaviors;
 */
static inline void* ptr_align_down(void* p, usize align) {
    require_msg(is_power_of_two(align), "ptr_align_down: align must be a nonzero power of two");
    if (!p) return NULL;
    return (void*)(uintptr_t)align_down((usize)(uintptr_t)p, align);
}

/**
 * @brief Test if pointer p is aligned to align bytes
 *
 * @param p     Pointer to test (may be NULL)
 * @param align Alignment requirement (must be a nonzero power of two)
 *
 * @return true if p's address is a multiple of align, false otherwise
 *         (including when p == NULL)
 *
 * @pre is_power_of_two(align)
 *
 * @remark Null-tolerant: NULL in → false out
 * @remark Useful as a precondition check before SIMD loads, DMA transfers,
 *         or other operations with alignment requirements
 *
 * Example:
 * ```c
 * if (!ptr_is_aligned(buf, 16)) {
 *     return ERROR_UNALIGNED_BUFFER;  // required for SIMD load
 * }
 * ```
 *
 * @sa ptr_align_up(), ptr_align_down(), is_aligned()
 */
/*@
    requires align > 0;
    requires (align & (align - 1)) == 0;
    assigns  \nothing;
    behavior null:
        assumes p == \null;
        ensures \result == \false;
    behavior nonnull:
        assumes p != \null;
    complete behaviors;
    disjoint behaviors;
 */
static inline bool ptr_is_aligned(const void* p, usize align) {
    require_msg(is_power_of_two(align), "ptr_is_aligned: align must be a nonzero power of two");
    if (!p) return false;
    return is_aligned((usize)(uintptr_t)p, align);
}

/**
 * @brief Compute padding bytes needed to align pointer p to align
 *
 * @param p     Pointer (may be NULL)
 * @param align Alignment requirement (must be a nonzero power of two)
 *
 * @return Padding bytes in [0, align - 1], or 0 if p == NULL
 *
 * @pre is_power_of_two(align)
 *
 * @remark Null-tolerant: NULL in → 0 out
 * @remark Useful for computing arena fragmentation or reservation sizes
 *
 * Example:
 * ```c
 * usize pad    = ptr_align_padding(arena->cur, 16);
 * usize needed = pad + object_size;
 * if (arena_remaining(arena) < needed) return OUT_OF_MEMORY;
 * ```
 *
 * @sa ptr_align_up(), align_padding()
 */
/*@
    requires align > 0;
    requires (align & (align - 1)) == 0;
    assigns  \nothing;
    behavior null:
        assumes p == \null;
        ensures \result == 0;
    behavior nonnull:
        assumes p != \null;
        ensures \result < align;
    complete behaviors;
    disjoint behaviors;
 */
static inline usize ptr_align_padding(const void* p, usize align) {
    if (!p) return 0;
    return align_padding((usize)(uintptr_t)p, align);
}

/* ============================================================================
 * Pointer Arithmetic
 *
 * ptr_offset / ptr_offset_const : null-tolerant — NULL in → NULL out
 * ptr_retreat                   : null-tolerant — NULL in → NULL out
 * ptr_diff                      : null-intolerant — contract violation on NULL
 * ptr_span                      : null-intolerant — contract violation on NULL
 *
 * Overflow/underflow is caught by ensure_msg() in debug builds.
 * ========================================================================= */

/**
 * @brief Advance pointer p forward by n bytes
 *
 * @param p Pointer to advance (may be NULL)
 * @param n Number of bytes to advance
 *
 * @return p + n (byte-wise), or NULL if p == NULL
 *
 * @pre (uintptr_t)p + n does not overflow the address space
 *
 * @remark Null-tolerant: NULL in → NULL out
 * @remark Byte-wise advance regardless of pointed-to type — use ptr_elem
 *         for typed element access
 * @remark Overflow is caught by ensure_msg() in debug builds
 *
 * Example:
 * ```c
 * void* next = ptr_offset(base, 64);          // byte cursor
 * u8*   tail = ptr_offset(header, sizeof(Header)); // skip over fixed prefix
 * ```
 *
 * @sa ptr_offset_const(), ptr_retreat(), ptr_elem()
 */
/*@
    assigns  \nothing;
    behavior null:
        assumes p == \null;
        ensures \result == \null;
    behavior nonnull:
        assumes p != \null;
        ensures \result != \null;
    complete behaviors;
    disjoint behaviors;
 */
static inline void* ptr_offset(void* p, usize n) {
    if (!p) return NULL;
    ensure_msg((usize)(uintptr_t)p <= CANON_USIZE_MAX - n,
               "ptr_offset: offset would overflow address space");
    return (void*)((u8*)p + n);
}

/**
 * @brief Const-correct variant of ptr_offset
 *
 * @param p Pointer to advance (may be NULL)
 * @param n Number of bytes to advance
 *
 * @return p + n (byte-wise), or NULL if p == NULL
 *
 * @pre (uintptr_t)p + n does not overflow the address space
 *
 * @remark Null-tolerant: NULL in → NULL out
 * @remark Use on read-only buffers to preserve const-correctness
 *
 * Example:
 * ```c
 * const void* payload = ptr_offset_const(packet, sizeof(Header));
 * ```
 *
 * @sa ptr_offset()
 */
/*@
    assigns  \nothing;
    behavior null:
        assumes p == \null;
        ensures \result == \null;
    behavior nonnull:
        assumes p != \null;
        ensures \result != \null;
    complete behaviors;
    disjoint behaviors;
 */
static inline const void* ptr_offset_const(const void* p, usize n) {
    if (!p) return NULL;
    ensure_msg((usize)(uintptr_t)p <= CANON_USIZE_MAX - n,
               "ptr_offset_const: offset would overflow address space");
    return (const void*)((const u8*)p + n);
}

/**
 * @brief Retreat pointer p backward by n bytes
 *
 * @param p Pointer to retreat (may be NULL)
 * @param n Number of bytes to retreat
 *
 * @return p - n (byte-wise), or NULL if p == NULL
 *
 * @pre (uintptr_t)p >= n  (prevents address-space underflow)
 *
 * @warning This checks only that the raw address does not underflow zero.
 *          It does NOT verify that the result stays within the same
 *          allocation. The caller is responsible for that invariant.
 *
 * @remark Null-tolerant: NULL in → NULL out
 * @remark Underflow is caught by ensure_msg() in debug builds
 *
 * Example:
 * ```c
 * void* header = ptr_retreat(payload, sizeof(Header));
 * ```
 *
 * @sa ptr_offset(), ptr_diff()
 */
/*@
    assigns  \nothing;
    behavior null:
        assumes p == \null;
        ensures \result == \null;
    behavior nonnull:
        assumes p != \null;
        ensures \result != \null;
    complete behaviors;
    disjoint behaviors;
 */
static inline void* ptr_retreat(void* p, usize n) {
    if (!p) return NULL;
    ensure_msg((usize)(uintptr_t)p >= n,
               "ptr_retreat: retreat would underflow address space");
    return (void*)((u8*)p - n);
}

/**
 * @brief Compute signed byte distance from `from` to `to`  (to - from)
 *
 * @param to   End pointer
 * @param from Start pointer
 *
 * @return Signed byte distance. Positive if to > from, negative if
 *         to < from, zero if equal.
 *
 * @pre to   != NULL
 * @pre from != NULL
 * @pre Both pointers are in the same allocation (or one-past-end).
 *      Subtracting pointers from different allocations is undefined
 *      behavior in C — Canon-C cannot detect this at runtime but
 *      requires the caller to honor it.
 *
 * @remark Null-intolerant: passing NULL is a contract violation
 *
 * Example:
 * ```c
 * isize used      = ptr_diff(arena->cur, arena->start);
 * isize remaining = ptr_diff(arena->end, arena->cur);
 * ```
 *
 * @sa ptr_span(), ptr_offset()
 */
/*@
    requires \valid_read((char*)to);
    requires \valid_read((char*)from);
    requires \base_addr((char*)to) == \base_addr((char*)from);
    assigns  \nothing;
 */
static inline isize ptr_diff(const void* to, const void* from) {
    require_msg(to   != NULL, "ptr_diff: 'to' is NULL");
    require_msg(from != NULL, "ptr_diff: 'from' is NULL");
    return (isize)((const u8*)to - (const u8*)from);
}

/**
 * @brief Compute unsigned byte distance from `from` to `to`
 *
 * @param to   End pointer (must be >= from)
 * @param from Start pointer
 *
 * @return Unsigned byte distance
 *
 * @pre to   != NULL
 * @pre from != NULL
 * @pre to >= from  (checked by require_msg)
 * @pre Both pointers are in the same allocation (caller responsibility —
 *      pointer comparison across allocations is undefined behavior)
 *
 * @remark Null-intolerant: passing NULL is a contract violation
 * @remark Prefer ptr_span over ptr_diff when the ordering is known:
 *         it catches inverted arguments at the require_msg check
 *
 * Example:
 * ```c
 * usize remaining = ptr_span(arena->end, arena->cur);
 * usize used      = ptr_span(arena->cur, arena->start);
 * ```
 *
 * @sa ptr_diff(), ptr_in_range()
 */
/*@
    requires \valid_read((char*)to);
    requires \valid_read((char*)from);
    requires \base_addr((char*)to) == \base_addr((char*)from);
    requires (char*)to >= (char*)from;
    assigns  \nothing;
    ensures  \result == (usize)((char*)to - (char*)from);
 */
static inline usize ptr_span(const void* to, const void* from) {
    require_msg(to   != NULL, "ptr_span: 'to' is NULL");
    require_msg(from != NULL, "ptr_span: 'from' is NULL");
    require_msg((const u8*)to >= (const u8*)from,
                "ptr_span: 'to' must be >= 'from'");
    return (usize)((const u8*)to - (const u8*)from);
}

/* ============================================================================
 * Bounds Checking (Null-Tolerant)
 *
 * All functions return false on any NULL argument.
 * ========================================================================= */

/**
 * @brief Test if pointer p lies within [region_start, region_end)
 *
 * @param p            Pointer to test
 * @param region_start Region start (inclusive)
 * @param region_end   Region end (exclusive, one-past-last)
 *
 * @return true if p is inside the half-open range, false otherwise
 *         (including when any argument is NULL)
 *
 * @pre All three pointers are in the same allocation for defined behavior.
 *      Pointer comparisons across allocations are undefined in C — Canon-C
 *      cannot enforce this at runtime.
 *
 * @remark Null-tolerant: any NULL argument → false
 * @remark Half-open range: region_end itself is NOT included (standard
 *         C convention for iterator-like endpoints)
 *
 * Example:
 * ```c
 * if (!ptr_in_range(user_ptr, buf, buf + buf_size)) {
 *     return ERROR_POINTER_OUT_OF_RANGE;
 * }
 * ```
 *
 * @sa ptr_range_in_range(), ptr_span()
 */
/*@
    assigns  \nothing;
    behavior any_null:
        assumes p == \null || region_start == \null || region_end == \null;
        ensures \result == \false;
    behavior all_nonnull:
        assumes p != \null && region_start != \null && region_end != \null;
    complete behaviors;
    disjoint behaviors;
 */
static inline bool ptr_in_range(const void*  p,
                                 const void*  region_start,
                                 const void*  region_end) {
    if (!p || !region_start || !region_end) return false;
    return (const u8*)p >= (const u8*)region_start &&
           (const u8*)p <  (const u8*)region_end;
}

/**
 * @brief Test if the range [p, p+len) lies fully within [region_start, region_end)
 *
 * @param p            Start of range to check
 * @param len          Length of range in bytes
 * @param region_start Region start (inclusive)
 * @param region_end   Region end (exclusive)
 *
 * @return true if [p, p+len) is fully inside the region, false otherwise
 *         (including when any pointer is NULL)
 *
 * @pre All pointers are in the same allocation for defined behavior.
 *
 * @remark Null-tolerant: any NULL pointer argument → false
 * @remark Overflow-safe: never computes p + len directly. Uses
 *         `len > (region_end - p)` to avoid pointer overflow.
 * @remark A zero-length range at an in-range pointer returns true
 *         (consistent with half-open interval semantics)
 *
 * Example:
 * ```c
 * if (!ptr_range_in_range(user_ptr, user_len, buf, buf + buf_size)) {
 *     return ERROR_RANGE_OUT_OF_BOUNDS;
 * }
 * ```
 *
 * @sa ptr_in_range()
 */
/*@
    assigns  \nothing;
    behavior any_null:
        assumes p == \null || region_start == \null || region_end == \null;
        ensures \result == \false;
    behavior all_nonnull:
        assumes p != \null && region_start != \null && region_end != \null;
    complete behaviors;
    disjoint behaviors;
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

/* ============================================================================
 * Typed Element Access (Null-Intolerant)
 *
 * NULL base is a contract violation — indexing into a nonexistent array
 * has no meaningful answer, so this is a programmer error, not a case
 * where NULL should propagate.
 *
 * The index * elem_size multiply is overflow-checked before use.
 * ========================================================================= */

/**
 * @brief Compute pointer to element i in a flat array at base
 *
 * Equivalent to `&((T*)base)[i]` without requiring a typed pointer.
 *
 * @param base      Pointer to array start (must NOT be NULL)
 * @param index     Element index (zero-based)
 * @param elem_size Size of each element in bytes (must be > 0)
 *
 * @return Pointer to the i-th element
 *
 * @pre base      != NULL  (contract violation if NULL — indexing into a
 *                          nonexistent array has no meaningful answer)
 * @pre elem_size > 0
 * @pre index * elem_size does not overflow usize
 *
 * @remark Null-intolerant: NULL base is a contract violation
 * @remark Useful for iterating generic containers without type-punning
 *
 * Example:
 * ```c
 * void* slot = ptr_elem(pool->buffer, 3, sizeof(MyStruct));
 * MyStruct* item = (MyStruct*)slot;
 * ```
 *
 * @sa ptr_elem_const(), ptr_offset()
 */
/*@
    requires base      != \null;
    requires elem_size >  0;
    requires index     <= CANON_USIZE_MAX / elem_size;
    assigns  \nothing;
    ensures  \result != \null;
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
 * @param base      Pointer to array start (must NOT be NULL)
 * @param index     Element index (zero-based)
 * @param elem_size Size of each element in bytes (must be > 0)
 *
 * @return Const pointer to the i-th element
 *
 * @pre base      != NULL  (contract violation if NULL)
 * @pre elem_size > 0
 * @pre index * elem_size does not overflow usize
 *
 * @remark Null-intolerant: NULL base is a contract violation
 * @remark Use for read-only access to const arrays
 *
 * Example:
 * ```c
 * const void* slot = ptr_elem_const(data, 10, sizeof(Record));
 * ```
 *
 * @sa ptr_elem()
 */
/*@
    requires base      != \null;
    requires elem_size >  0;
    requires index     <= CANON_USIZE_MAX / elem_size;
    assigns  \nothing;
    ensures  \result != \null;
 */
static inline const void* ptr_elem_const(const void* base, usize index, usize elem_size) {
    require_msg(base      != NULL, "ptr_elem_const: base cannot be NULL");
    require_msg(elem_size >  0,    "ptr_elem_const: elem_size must be > 0");
    ensure_msg(index <= CANON_USIZE_MAX / elem_size,
               "ptr_elem_const: index * elem_size overflows usize");
    return ptr_offset_const(base, index * elem_size);
}

/* ============================================================================
 * Null Safety (Null-Tolerant Utilities)
 *
 * Utility functions where NULL has defined safe response. Use freely in
 * code paths where NULL propagation is expected.
 * ========================================================================= */

/**
 * @brief Return p if non-NULL, otherwise return fallback
 *
 * @param p        Primary pointer
 * @param fallback Pointer to return if p is NULL
 *
 * @return p if p != NULL, otherwise fallback
 *
 * @remark Null-tolerant: NULL primary → fallback returned
 * @remark Useful for replacing `x = p ? p : default` idioms at call sites
 *
 * Example:
 * ```c
 * void* buf = ptr_or(user_supplied, internal_default);
 * ```
 *
 * @sa ptr_or_const(), ptr_is_valid()
 */
/*@
    assigns \nothing;
    behavior null:
        assumes p == \null;
        ensures \result == fallback;
    behavior nonnull:
        assumes p != \null;
        ensures \result == p;
    complete behaviors;
    disjoint behaviors;
 */
static inline void* ptr_or(void* p, void* fallback) {
    return p ? p : fallback;
}

/**
 * @brief Const variant of ptr_or
 *
 * @param p        Primary pointer
 * @param fallback Pointer to return if p is NULL
 *
 * @return p if p != NULL, otherwise fallback
 *
 * @remark Null-tolerant: NULL primary → fallback returned
 *
 * Example:
 * ```c
 * const char* name = ptr_or_const(config->name, "default");
 * ```
 *
 * @sa ptr_or()
 */
/*@
    assigns \nothing;
    behavior null:
        assumes p == \null;
        ensures \result == fallback;
    behavior nonnull:
        assumes p != \null;
        ensures \result == p;
    complete behaviors;
    disjoint behaviors;
 */
static inline const void* ptr_or_const(const void* p, const void* fallback) {
    return p ? p : fallback;
}

/**
 * @brief Test if pointer is not NULL
 *
 * @param p Pointer to test
 *
 * @return true if p != NULL, false if p == NULL
 *
 * @remark Avoids implicit boolean conversion of pointers in contract macros
 * @remark Useful as an explicit validity check in require_msg/ensure_msg
 *         expressions to make the intent unambiguous
 *
 * Example:
 * ```c
 * require_msg(ptr_is_valid(ctx), "ptr_is_valid: ctx cannot be NULL");
 * ```
 *
 * @sa ptr_or()
 */
/*@
    assigns \nothing;
    ensures \result <==> (p != \null);
 */
static inline bool ptr_is_valid(const void* p) {
    return p != NULL;
}

/* ============================================================================
 * Struct Introspection Macros
 *
 * Both delegate to offsetof() from <stddef.h> (included at top of file),
 * avoiding the undefined null-pointer dereference of a hand-rolled version.
 * ========================================================================= */

/**
 * @def PTR_OFFSET_OF(type, member)
 * @brief Byte offset of member within type
 *
 * Thin, consistently-named wrapper over the standard offsetof macro.
 *
 * @param type   Struct type
 * @param member Member name within type
 *
 * @return Byte offset of member within type, as usize
 *
 * @remark Pure compile-time expression: zero runtime cost
 * @remark Cast to usize for type consistency with the rest of Canon-C
 *
 * Example:
 * ```c
 * usize off = PTR_OFFSET_OF(MyStruct, next);
 * ```
 *
 * @sa PTR_CONTAINER_OF()
 */
#define PTR_OFFSET_OF(type, member) \
    ((usize)offsetof(type, member))

/**
 * @def PTR_CONTAINER_OF(ptr, type, member)
 * @brief Recover the enclosing struct pointer from a member pointer
 *
 * Given a pointer to `member` inside a `type`, returns a pointer to
 * the containing `type` instance. Used for intrusive data structures
 * where nodes are embedded directly in user structs.
 *
 * @param ptr    Pointer to the member field
 * @param type   Type of the containing struct
 * @param member Name of the member field within type
 *
 * @return Pointer to the containing struct
 *
 * @warning The caller must ensure `ptr` actually points to a `member`
 *          inside a `type` instance. This macro performs no validation —
 *          passing an unrelated pointer produces undefined behavior.
 *
 * @remark Canonical intrusive-container pattern used by Linux kernel
 *         lists, ThreadX queues, and similar embedded systems code
 *
 * Example:
 * ```c
 * typedef struct { int x; Node node; } MyItem;
 * Node*   node_ptr = list_pop(&list);
 * MyItem* item     = PTR_CONTAINER_OF(node_ptr, MyItem, node);
 * printf("%d\n", item->x);
 * ```
 *
 * @sa PTR_OFFSET_OF()
 */
#define PTR_CONTAINER_OF(ptr, type, member) \
    ((type*)((u8*)(ptr) - PTR_OFFSET_OF(type, member)))

/* ============================================================================
 * Usage Examples (documentation only)
 * ========================================================================= */

/*
// Arena bump allocator
void* arena_alloc(Arena* a, usize size, usize align) {
    void* aligned = ptr_align_up(a->cur, align);
    void* next    = ptr_offset(aligned, size);
    if (!ptr_in_range(next, a->start, a->end)) return NULL;
    a->cur = next;
    return aligned;
}

// Pool block indexing
void* pool_block(Pool* p, usize i) {
    return ptr_elem(p->buffer, i, p->block_size);
}

// Slice length from pointers
usize slice_length(const void* start, const void* end) {
    return ptr_span(end, start);
}

// SIMD alignment precondition
void simd_add(float* dst, const float* a, const float* b, usize n) {
    require_msg(ptr_is_aligned(dst, 16), "simd_add: dst must be 16-aligned");
    require_msg(ptr_is_aligned(a,   16), "simd_add: a must be 16-aligned");
    require_msg(ptr_is_aligned(b,   16), "simd_add: b must be 16-aligned");
    // ... SIMD loads ...
}

// Intrusive linked list traversal
typedef struct Node { struct Node* next; } Node;
typedef struct { int value; Node link; } Item;

void print_items(Node* head) {
    for (Node* n = head; n != NULL; n = n->next) {
        Item* item = PTR_CONTAINER_OF(n, Item, link);
        printf("%d\n", item->value);
    }
}

// Safe default fallback
Config* get_config(Config* user) {
    static Config default_config = { /* ... */ };
    return ptr_or(user, &default_config);
}

// Page-aligned address for page table walk
void* page_base(void* addr) {
    return ptr_align_down(addr, 4096);
}
*/

#endif /* CANON_CORE_PRIMITIVES_PTR_H */
