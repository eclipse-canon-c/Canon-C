/**
 * @file checked.h
 * @brief Overflow-safe arithmetic operations for Canon-C
 *
 * Provides explicit, checked arithmetic that detects overflow/underflow
 * instead of silently producing undefined behavior. Uses compiler builtins
 * when available (GCC, Clang), falls back to portable implementations.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicitness: All overflow is detected and reported explicitly
 * - Safety: No silent UB from integer overflow (C undefined behavior)
 * - Clarity: Boolean return + output parameter pattern is self-documenting
 * - Performance: Compiler builtins compile to single instructions when available
 * - Portability: Fallback implementations work everywhere
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All operations: O(1) — constant time arithmetic and bitwise operations
 * - With builtins (GCC/Clang): 1-2 CPU instructions per operation
 * - Without builtins: 3-10 instructions (comparison + branch)
 * - All functions are static inline → zero call overhead
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are fully thread-safe — no shared mutable state.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (inline functions, stdint.h, stdbool.h)
 * - Compiler builtins detected via __GNUC__ and __clang__
 * - Fallback implementations use only standard C arithmetic
 * - No platform-specific assembly or intrinsics
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Arena allocation: checked_add(arena->offset, size, &new_offset)
 * - Vector capacity: checked_mul(count, elem_size, &total_bytes)
 * - Pool initialization: checked_mul(num_blocks, block_size, &total)
 * - Index calculations: checked_add(base_index, offset, &final_index)
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Cryptographic constant-time operations (timing side-channels exist)
 * - Saturating arithmetic (use separate saturation functions)
 * - Modular arithmetic (use explicit modulo instead)
 * - Very tight loops where overflow is impossible (profile first)
 *
 * @note Alignment helpers (align_up, align_down, is_aligned, is_ptr_aligned)
 *       have been removed from this file. They live in core/primitives/ptr.h,
 *       which provides guarded versions with require_msg precondition checks.
 *       Include ptr.h if you need alignment utilities.
 *
 * @sa types.h, limits.h, contract.h, ptr.h
 */

#ifndef CANON_CORE_PRIMITIVES_CHECKED_H
#define CANON_CORE_PRIMITIVES_CHECKED_H

#include "types.h"
#include "limits.h"    /* CANON_ISIZE_MAX, CANON_ISIZE_MIN — needed by signed fallbacks */

/* ============================================================================
 * Compiler Builtin Detection
 * ========================================================================= */

#if defined(__GNUC__) || defined(__clang__)
    #define CANON_HAS_BUILTIN_OVERFLOW 1
#else
    #define CANON_HAS_BUILTIN_OVERFLOW 0
#endif

/* ============================================================================
 * Checked Addition (Unsigned)
 * ========================================================================= */

/**
 * @brief Add two unsigned values with overflow detection
 *
 * Performs `a + b` and stores the result in `*result` only if the addition
 * does not overflow. On overflow, `*result` is still written (with wrapped
 * value when using builtins), but function returns false.
 *
 * @param a First operand (any value)
 * @param b Second operand (any value)
 * @param result Output parameter for a + b (must not be NULL)
 *
 * @return true if a + b fits in usize, false if overflow occurred
 *
 * @pre result must be non-NULL (not checked — UB if violated)
 *
 * @remark Uses __builtin_add_overflow on GCC/Clang (1 instruction)
 * @remark Fallback checks: overflow iff (a + b) < a
 *
 * Example:
 * ```c
 * usize arena_offset = 1024;
 * usize alloc_size = 512;
 * usize new_offset;
 * if (!checked_add(arena_offset, alloc_size, &new_offset)) {
 *     return result_err(ERROR_OVERFLOW);
 * }
 * arena->offset = new_offset; // safe to use
 * ```
 *
 * @sa checked_sub(), checked_mul()
 */
static inline bool checked_add(usize a, usize b, usize* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_add_overflow(a, b, result);
#else
    *result = a + b;
    return *result >= a;  /* unsigned wraparound is well-defined; overflow iff result < a */
#endif
}

static inline bool checked_add_u8(u8 a, u8 b, u8* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_add_overflow(a, b, result);
#else
    u16 sum = (u16)a + (u16)b;
    *result = (u8)sum;
    return sum <= 0xFF;
#endif
}

static inline bool checked_add_u16(u16 a, u16 b, u16* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_add_overflow(a, b, result);
#else
    u32 sum = (u32)a + (u32)b;
    *result = (u16)sum;
    return sum <= 0xFFFF;
#endif
}

static inline bool checked_add_u32(u32 a, u32 b, u32* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_add_overflow(a, b, result);
#else
    u64 sum = (u64)a + (u64)b;
    *result = (u32)sum;
    return sum <= 0xFFFFFFFFU;
#endif
}

static inline bool checked_add_u64(u64 a, u64 b, u64* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_add_overflow(a, b, result);
#else
    *result = a + b;
    return *result >= a;  /* unsigned wraparound is well-defined */
#endif
}

/* ============================================================================
 * Checked Subtraction (Unsigned)
 * ========================================================================= */

/**
 * @brief Subtract two unsigned values with underflow detection
 *
 * @param a First operand (minuend)
 * @param b Second operand (subtrahend)
 * @param result Output parameter for a - b (must not be NULL)
 *
 * @return true if a >= b (no underflow), false if b > a
 *
 * @pre result must be non-NULL (not checked — UB if violated)
 *
 * Example:
 * ```c
 * usize remaining;
 * if (!checked_sub(capacity, used, &remaining)) {
 *     return ERROR_UNDERFLOW;
 * }
 * ```
 */
static inline bool checked_sub(usize a, usize b, usize* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_sub_overflow(a, b, result);
#else
    *result = a - b;
    return b <= a;  /* underflow iff b > a */
#endif
}

static inline bool checked_sub_u8(u8 a, u8 b, u8* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_sub_overflow(a, b, result);
#else
    *result = a - b;
    return b <= a;
#endif
}

static inline bool checked_sub_u16(u16 a, u16 b, u16* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_sub_overflow(a, b, result);
#else
    *result = a - b;
    return b <= a;
#endif
}

static inline bool checked_sub_u32(u32 a, u32 b, u32* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_sub_overflow(a, b, result);
#else
    *result = a - b;
    return b <= a;
#endif
}

static inline bool checked_sub_u64(u64 a, u64 b, u64* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_sub_overflow(a, b, result);
#else
    *result = a - b;
    return b <= a;
#endif
}

/* ============================================================================
 * Checked Multiplication (Unsigned)
 * ========================================================================= */

/**
 * @brief Multiply two unsigned values with overflow detection
 *
 * Performs `a * b` and stores the result in `*result` only if the
 * multiplication does not overflow. Critical for array/buffer size
 * calculations where `count * element_size` could wrap.
 *
 * @param a First operand (multiplicand)
 * @param b Second operand (multiplier)
 * @param result Output parameter for a * b (must not be NULL)
 *
 * @return true if a * b fits in usize, false if overflow occurred
 *
 * @pre result must be non-NULL (not checked — UB if violated)
 *
 * @remark Uses __builtin_mul_overflow on GCC/Clang (1-2 instructions)
 * @remark Fallback: if a == 0 or b == 0, returns true with result = 0;
 *         otherwise checks via division after multiply (unsigned — no UB)
 *
 * Example:
 * ```c
 * usize num_items = 1000;
 * usize item_size = sizeof(struct Item);
 * usize total_bytes;
 * if (!checked_mul(num_items, item_size, &total_bytes)) {
 *     return result_err(ERROR_ALLOCATION_TOO_LARGE);
 * }
 * void* buffer = malloc(total_bytes); // safe size
 * ```
 *
 * @sa checked_add(), checked_sub()
 */
static inline bool checked_mul(usize a, usize b, usize* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_mul_overflow(a, b, result);
#else
    if (a == 0 || b == 0) {
        *result = 0;
        return true;
    }
    *result = a * b;
    return *result / a == b;  /* unsigned — well-defined; overflow iff division doesn't recover b */
#endif
}

static inline bool checked_mul_u8(u8 a, u8 b, u8* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_mul_overflow(a, b, result);
#else
    u16 prod = (u16)a * (u16)b;
    *result = (u8)prod;
    return prod <= 0xFF;
#endif
}

static inline bool checked_mul_u16(u16 a, u16 b, u16* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_mul_overflow(a, b, result);
#else
    u32 prod = (u32)a * (u32)b;
    *result = (u16)prod;
    return prod <= 0xFFFF;
#endif
}

static inline bool checked_mul_u32(u32 a, u32 b, u32* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_mul_overflow(a, b, result);
#else
    u64 prod = (u64)a * (u64)b;
    *result = (u32)prod;
    return prod <= 0xFFFFFFFFU;
#endif
}

static inline bool checked_mul_u64(u64 a, u64 b, u64* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_mul_overflow(a, b, result);
#else
    if (a == 0 || b == 0) {
        *result = 0;
        return true;
    }
    *result = a * b;
    return *result / a == b;  /* unsigned — well-defined */
#endif
}

/* ============================================================================
 * Signed Arithmetic
 * ========================================================================= */

/**
 * @brief Add two signed isize values with overflow detection
 *
 * @param a First operand
 * @param b Second operand
 * @param result Output parameter for a + b (must not be NULL)
 *
 * @return true if a + b does not overflow isize, false otherwise
 *
 * @remark Fallback checks bounds BEFORE adding to avoid signed overflow UB.
 *         Signed integer overflow is undefined behavior in C.
 */
static inline bool checked_add_isize(isize a, isize b, isize* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_add_overflow(a, b, result);
#else
    /*
     * Check BEFORE adding — signed overflow is UB in C.
     * Overflow only possible when both operands share the same sign:
     *   (+, +): overflow if b > ISIZE_MAX - a  (both positive, sum too large)
     *   (-, -): underflow if b < ISIZE_MIN - a (both negative, sum too small)
     * Mixed signs can never overflow.
     */
    if (a > 0 && b > 0 && b > (CANON_ISIZE_MAX - a)) return false;
    if (a < 0 && b < 0 && b < (CANON_ISIZE_MIN - a)) return false;
    *result = a + b;
    return true;
#endif
}

/**
 * @brief Subtract two signed isize values with overflow detection
 *
 * @param a First operand (minuend)
 * @param b Second operand (subtrahend)
 * @param result Output parameter for a - b (must not be NULL)
 *
 * @return true if a - b does not overflow isize, false otherwise
 *
 * @remark Fallback checks bounds BEFORE subtracting to avoid signed overflow UB.
 */
static inline bool checked_sub_isize(isize a, isize b, isize* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_sub_overflow(a, b, result);
#else
    /*
     * Check BEFORE subtracting — signed overflow is UB in C.
     * a - b overflows when b and a have opposite signs and a is near a limit:
     *   subtracting positive from near-ISIZE_MIN: underflow if a < ISIZE_MIN + b
     *   subtracting negative from near-ISIZE_MAX: overflow if a > ISIZE_MAX + b
     */
    if (b > 0 && a < (CANON_ISIZE_MIN + b)) return false;
    if (b < 0 && a > (CANON_ISIZE_MAX + b)) return false;
    *result = a - b;
    return true;
#endif
}

/**
 * @brief Multiply two signed isize values with overflow detection
 *
 * @param a First operand
 * @param b Second operand
 * @param result Output parameter for a * b (must not be NULL)
 *
 * @return true if a * b does not overflow isize, false otherwise
 *
 * @remark Fallback checks bounds BEFORE multiplying to avoid signed overflow UB.
 */
static inline bool checked_mul_isize(isize a, isize b, isize* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_mul_overflow(a, b, result);
#else
    if (a == 0 || b == 0) {
        *result = 0;
        return true;
    }
    /*
     * Check BEFORE multiplying to avoid signed overflow UB.
     * Signed integer overflow is undefined behavior in C —
     * `a * b` must never be evaluated if it would overflow.
     *
     * Four cases covering all sign combinations:
     *   (+, +): overflow  if a > ISIZE_MAX / b
     *   (-, -): overflow  if a < ISIZE_MAX / b  (both negative → positive result)
     *   (+, -): underflow if b < ISIZE_MIN / a
     *   (-, +): underflow if a < ISIZE_MIN / b
     *
     * Division is safe: a != 0 and b != 0 are both guaranteed above.
     */
    if (a > 0 && b > 0 && a > (CANON_ISIZE_MAX / b)) return false;
    if (a < 0 && b < 0 && a < (CANON_ISIZE_MAX / b)) return false;
    if (a > 0 && b < 0 && b < (CANON_ISIZE_MIN / a)) return false;
    if (a < 0 && b > 0 && a < (CANON_ISIZE_MIN / b)) return false;
    *result = a * b;
    return true;
#endif
}

/* ============================================================================
 * Min / Max / Clamp (macros)
 * ========================================================================= */

/**
 * @brief Returns the smaller of a and b
 * @warning Macro — arguments may be evaluated twice. Do not use with side effects.
 */
#define checked_min(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief Returns the larger of a and b
 * @warning Macro — arguments may be evaluated twice. Do not use with side effects.
 */
#define checked_max(a, b) ((a) > (b) ? (a) : (b))

/**
 * @brief Clamps x to the inclusive range [lo, hi]
 * @warning Macro — arguments may be evaluated twice. Do not use with side effects.
 */
#define checked_clamp(x, lo, hi) (checked_max((lo), checked_min((x), (hi))))

/* ============================================================================
 * Usage Examples
 * ========================================================================= */

/*
 * // Arena allocation with overflow check
 * usize new_offset;
 * if (!checked_add(arena->offset, size, &new_offset)) {
 *     return result_err(ERROR_OVERFLOW);
 * }
 * if (new_offset > arena->capacity) {
 *     return result_err(ERROR_OUT_OF_MEMORY);
 * }
 * arena->offset = new_offset;
 *
 * // Vector capacity calculation
 * usize total_bytes;
 * if (!checked_mul(count, elem_size, &total_bytes)) {
 *     return result_err(ERROR_OVERFLOW);
 * }
 *
 * // Safe index calculation
 * usize index;
 * if (!checked_add(base_index, offset, &index)) {
 *     return result_err(ERROR_OVERFLOW);
 * }
 * if (index >= vec->len) {
 *     return result_err(ERROR_OUT_OF_BOUNDS);
 * }
 *
 * // For alignment operations, use ptr.h:
 * #include "core/primitives/ptr.h"
 * usize aligned_offset = align_up(arena->offset, 16);
 * if (!is_aligned(ptr, 8)) {
 *     return result_err(ERROR_ALIGNMENT);
 * }
 */

#endif /* CANON_CORE_PRIMITIVES_CHECKED_H */
