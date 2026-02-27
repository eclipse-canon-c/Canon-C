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
 * - Alignment helpers: 2-5 instructions (bitwise operations only)
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
 * - Memory alignment: align_up(size, 16), is_aligned(ptr, 64)
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Cryptographic constant-time operations (timing side-channels exist)
 * - Saturating arithmetic (use separate saturation functions)
 * - Modular arithmetic (use explicit modulo instead)
 * - Very tight loops where overflow is impossible (profile first)
 *
 * @sa types.h, contract.h
 */

#ifndef CANON_CORE_PRIMITIVES_CHECKED_H
#define CANON_CORE_PRIMITIVES_CHECKED_H

#include "types.h"
#include "limits.h"    /* CANON_ISIZE_MAX, CANON_ISIZE_MIN — needed by checked_mul_isize fallback */

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
    return *result >= a;  /* overflow if result wrapped around */
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
    return sum <= 0xFFFFFFFF;
#endif
}

static inline bool checked_add_u64(u64 a, u64 b, u64* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_add_overflow(a, b, result);
#else
    *result = a + b;
    return *result >= a;
#endif
}

/* ============================================================================
 * Checked Subtraction (Unsigned)
 * ========================================================================= */

/**
 * Subtract two unsigned values with underflow detection.
 * 
 * @param a First operand (minuend)
 * @param b Second operand (subtrahend)
 * @param result Output parameter for a - b
 * @return true if operation succeeded, false if underflow occurred
 * 
 * Example:
 *   usize remaining;
 *   if (!checked_sub(capacity, used, &remaining)) {
 *       return ERROR_UNDERFLOW;
 *   }
 */
static inline bool checked_sub(usize a, usize b, usize* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_sub_overflow(a, b, result);
#else
    *result = a - b;
    return b <= a;  /* underflow if b > a */
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
 * @remark Fallback checks: overflow iff (a * b) / a != b (division recovers b)
 * @remark Special case: if a == 0 or b == 0, returns true with result = 0
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
    return *result / a == b;  /* overflow if division doesn't recover b */
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
    return prod <= 0xFFFFFFFF;
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
    return *result / a == b;
#endif
}

/* ============================================================================
 * Signed Arithmetic (for completeness)
 * ========================================================================= */

static inline bool checked_add_isize(isize a, isize b, isize* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_add_overflow(a, b, result);
#else
    *result = a + b;
    /* Overflow if signs match but result sign differs */
    if (a > 0 && b > 0) return *result > 0;
    if (a < 0 && b < 0) return *result < 0;
    return true;
#endif
}

static inline bool checked_sub_isize(isize a, isize b, isize* result) {
#if CANON_HAS_BUILTIN_OVERFLOW
    return !__builtin_sub_overflow(a, b, result);
#else
    *result = a - b;
    /* Overflow if signs differ and result sign doesn't match a */
    if (a >= 0 && b < 0) return *result >= 0;
    if (a < 0 && b > 0) return *result < 0;
    return true;
#endif
}

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
     *   (+, +): overflow if a > ISIZE_MAX / b
     *   (-, -): result is positive; same bound as (+,+) after negation
     *   (+, -): underflow if b < ISIZE_MIN / a
     *   (-, +): underflow if a < ISIZE_MIN / b
     *
     * Division is safe here because b != 0 and a != 0 (checked above).
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
 * Alignment Helpers
 * ========================================================================= */

/**
 * @brief Rounds value up to the next multiple of alignment
 *
 * Uses efficient bitwise operations (not division/modulo). Commonly used
 * in allocators to ensure proper alignment of allocated blocks.
 *
 * @param value Value to align (0 is allowed → returns 0)
 * @param alignment Power-of-2 alignment requirement (1, 2, 4, 8, 16, ...)
 *
 * @return Smallest multiple of alignment >= value
 *
 * @pre alignment must be a power of 2 (not checked — UB if violated)
 *
 * @remark Formula: (value + alignment - 1) & ~(alignment - 1)
 * @remark Compiles to 2-3 instructions on most platforms
 * @remark Does NOT check for overflow — use checked_add first if needed
 *
 * Example:
 * ```c
 * usize offset = 17;
 * usize aligned = align_up(offset, 16); // → 32
 * 
 * // In arena allocator:
 * arena->offset = align_up(arena->offset, 16);
 * ```
 *
 * @sa align_down(), is_aligned()
 */
static inline usize align_up(usize value, usize alignment) {
    /* alignment must be power of 2 */
    return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * Align a value down to the nearest multiple of alignment.
 * 
 * @param value Value to align
 * @param alignment Must be a power of 2
 * @return Aligned value
 * 
 * Example:
 *   usize aligned = align_down(17, 16);  // returns 16
 */
static inline usize align_down(usize value, usize alignment) {
    /* alignment must be power of 2 */
    return value & ~(alignment - 1);
}

/**
 * Check if a value is aligned to the specified alignment.
 * 
 * @param value Value to check
 * @param alignment Must be a power of 2
 * @return true if aligned, false otherwise
 */
static inline bool is_aligned(usize value, usize alignment) {
    /* alignment must be power of 2 */
    return (value & (alignment - 1)) == 0;
}

/**
 * Check if a pointer is aligned to the specified alignment.
 * 
 * @param ptr Pointer to check
 * @param alignment Must be a power of 2
 * @return true if aligned, false otherwise
 */
static inline bool is_ptr_aligned(const void* ptr, usize alignment) {
    return is_aligned((usize)ptr, alignment);
}

/* ============================================================================
 * Min/Max Utilities
 * ========================================================================= */

#define checked_min(a, b) ((a) < (b) ? (a) : (b))
#define checked_max(a, b) ((a) > (b) ? (a) : (b))
#define checked_clamp(x, lo, hi) (checked_max(lo, checked_min(x, hi)))

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
 * // Alignment in allocator
 * usize aligned_offset = align_up(arena->offset, 16);
 * if (!is_aligned(ptr, 8)) {
 *     return result_err(ERROR_ALIGNMENT);
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
 */

#endif /* CANON_CORE_PRIMITIVES_CHECKED_H */
