// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Fikret Güney Ersezer
//
// This file is licensed under MIT (not the project-default MPL-2.0).
//
// Reason: bits.h provides portable bit manipulation backed by compiler
// builtins (GCC/Clang __builtin_popcountll, MSVC __popcnt64, etc.) with
// pure-C fallbacks. On targets with non-standard compilers, proprietary
// DSP toolchains, or hardware-specific bit instructions (e.g. ARM Cortex-M
// with dedicated bit-banding or RISC-V B-extension intrinsics), teams
// routinely replace the builtin detection blocks or fallback implementations
// with platform-optimized versions. Those modifications are integral to the
// platform port and should not carry a copyleft disclosure obligation.
// MIT keeps the porting path open without conditions.

/**
 * @file bits.h
 * @brief Portable bit manipulation operations for Canon-C
 *
 * Provides explicit, tested bit operations with compiler builtin support
 * for performance. Eliminates copy-paste bit tricks and provides portable
 * fallbacks for platforms without builtins.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicitness: Every bit operation is named and self-documenting
 * - Portability: Compiler builtins when available, fallbacks otherwise
 * - Correctness: No undefined behavior from shifts >= width
 * - Performance: Builtins compile to single instructions (popcnt, bsf, etc.)
 * - Type-safe: Separate functions for different integer sizes
 *
 * Formal verification:
 * ────────────────────────────────────────────────────────────────────────────
 * Every function in this header carries an ACSL contract suitable for
 * Frama-C WP. When Frama-C is running, `__FRAMAC__` is defined
 * automatically and the fallback path is forced (builtins cannot be
 * modeled by WP). The same fallback path can be forced in normal builds
 * via `-DCANON_BITS_FORCE_FALLBACK` for coverage measurement.
 *
 * All functions are pure (assigns \nothing) — they take values by copy
 * and return results without modifying any memory.
 *
 * ACSL precedence note: In ACSL (as in C), `==` binds tighter than `&`,
 * `^`, and `|`. All ensures clauses that use bitwise operators on the
 * RHS of `==` wrap the entire RHS in parentheses to avoid the classic
 * `\result == a & b` → `(\result == a) & b` mis-parse.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All operations: O(1) — constant time bitwise operations
 * - With builtins (GCC/Clang): 1 CPU instruction (bsf, bsr, popcnt, etc.)
 * - Without builtins: 3-20 instructions depending on operation
 * - All functions are static inline → zero call overhead
 * - Power-of-2 checks: 2-3 instructions (bitwise AND + comparison)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * All functions are fully thread-safe — no shared mutable state.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (inline functions, stdint.h)
 * - GCC/Clang builtins: detected via __GNUC__ (covers both compilers)
 * - MSVC builtins: detected via _MSC_VER, requires <intrin.h>
 * - Fallback implementations use only standard C bitwise operations
 * - No platform-specific assembly or intrinsics in fallbacks
 * - Undefined behavior avoided (no shifts >= type width)
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Flags and bitsets: bits_set(flags, FLAG_DIRTY)
 * - Allocator metadata: bits_popcount(free_blocks)
 * - Hash tables: bits_next_power_of_two(capacity)
 * - Alignment checks: bits_is_power_of_two(alignment)
 * - Bit packing: bits_extract(value, start, count)
 * - Rotation: bits_rotl(value, shift)
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Cryptographic operations (timing side-channels may exist)
 * - SIMD bit manipulation (use compiler intrinsics directly)
 * - Bit arrays larger than 64 bits (use dedicated bitset library)
 * - Endianness conversion (use dedicated byte-swap functions)
 *
 * @sa types.h
 */

#ifndef CANON_CORE_PRIMITIVES_BITS_H
#define CANON_CORE_PRIMITIVES_BITS_H

#include "types.h"

/* ============================================================================
 * Compiler Builtin Detection
 *
 * Two independent guards — one per compiler family.
 * Usage sites branch directly on these macros, so there is no combined
 * CANON_HAS_BUILTIN_BITS umbrella that would obscure which builtin is used.
 * ========================================================================= */

#if defined(__GNUC__) || defined(__clang__)
    /* Clang also defines __GNUC__, so this covers both. */
    #define CANON_BITS_GNUC 1
#else
    #define CANON_BITS_GNUC 0
#endif

#if defined(_MSC_VER)
    #define CANON_BITS_MSVC 1
    #include <intrin.h>
#else
    #define CANON_BITS_MSVC 0
#endif

/* ============================================================================
 * Frama-C / Fallback Override
 *
 * Frama-C cannot model compiler builtins (__builtin_popcountll,
 * __builtin_clzll, __builtin_ctzll, __builtin_bswap*, etc.) — WP has
 * no semantics for them. When Frama-C is running (__FRAMAC__ is defined
 * automatically), force the fallback path so every function has real C
 * code to prove. This also applies when a downstream build explicitly
 * wants to verify or measure the fallback path via -DCANON_BITS_FORCE_FALLBACK.
 * The coverage CI job uses this flag to keep MC/DC measurement aligned
 * with the WP proof, same as CANON_CHECKED_FORCE_FALLBACK for checked.h.
 * ========================================================================= */

#if defined(__FRAMAC__) || defined(CANON_BITS_FORCE_FALLBACK)
    #undef  CANON_BITS_GNUC
    #define CANON_BITS_GNUC 0
    #undef  CANON_BITS_MSVC
    #define CANON_BITS_MSVC 0
#endif

/* ============================================================================
 * Basic Bit Operations (Single Bit)
 * ========================================================================= */

/**
 * @brief Test if a specific bit is set
 *
 * @param value Value to test
 * @param bit Bit position (0 = LSB, 63 = MSB for u64)
 *
 * @return true if bit is set (1), false if clear (0)
 *
 * @pre bit must be < 64
 *
 * @remark Compiles to: (value >> bit) & 1
 *
 * Example:
 * ```c
 * u32 flags = 0b1010;
 * bool dirty = bits_test(flags, 1);  // → true
 * bool clean = bits_test(flags, 2);  // → false
 * ```
 *
 * @sa bits_set(), bits_clear()
 */
/*@
    requires bit < 64;
    assigns  \nothing;
    ensures  \result <==> (((value >> bit) & 1) == 1);
 */
static inline bool bits_test(u64 value, u32 bit) {
    return (value >> bit) & 1;
}

/**
 * @brief Set a specific bit to 1
 *
 * @param value Original value
 * @param bit Bit position to set (0 = LSB, 63 = MSB for u64)
 *
 * @return Value with bit set
 *
 * @pre bit must be < 64
 *
 * @remark Compiles to: value | (1ULL << bit)
 *
 * Example:
 * ```c
 * u32 flags = 0b0000;
 * flags = bits_set(flags, 3);  // → 0b1000
 * ```
 *
 * @sa bits_clear(), bits_toggle()
 */
/*@
    requires bit < 64;
    assigns  \nothing;
    ensures  \result == (value | ((u64)1 << bit));
 */
static inline u64 bits_set(u64 value, u32 bit) {
    return value | (1ULL << bit);
}

/**
 * @brief Clear a specific bit to 0
 *
 * @param value Original value
 * @param bit Bit position to clear (0 = LSB, 63 = MSB for u64)
 *
 * @return Value with bit cleared
 *
 * @pre bit must be < 64
 *
 * @remark Compiles to: value & ~(1ULL << bit)
 *
 * Example:
 * ```c
 * u32 flags = 0b1111;
 * flags = bits_clear(flags, 2);  // → 0b1011
 * ```
 *
 * @sa bits_set(), bits_toggle()
 */
/*@
    requires bit < 64;
    assigns  \nothing;
    ensures  \result == (value & (0xFFFFFFFFFFFFFFFFULL ^ ((u64)1 << bit)));
 */
static inline u64 bits_clear(u64 value, u32 bit) {
    return value & ~(1ULL << bit);
}

/**
 * @brief Toggle a specific bit (0→1, 1→0)
 *
 * @param value Original value
 * @param bit Bit position to toggle
 *
 * @return Value with bit flipped
 *
 * @pre bit must be < 64
 *
 * @remark Compiles to: value ^ (1ULL << bit)
 *
 * Example:
 * ```c
 * u32 flags = 0b1010;
 * flags = bits_toggle(flags, 0);  // → 0b1011
 * flags = bits_toggle(flags, 0);  // → 0b1010
 * ```
 */
/*@
    requires bit < 64;
    assigns  \nothing;
    ensures  \result == (value ^ ((u64)1 << bit));
 */
static inline u64 bits_toggle(u64 value, u32 bit) {
    return value ^ (1ULL << bit);
}

/* ============================================================================
 * Multi-Bit Operations
 * ========================================================================= */

/**
 * @brief Extract a range of bits
 *
 * Extracts `count` bits starting at position `start`, returns them
 * right-justified (shifted down to bit 0).
 *
 * @param value Value to extract from
 * @param start Starting bit position (0 = LSB)
 * @param count Number of bits to extract (1-64)
 *
 * @return Extracted bits, right-justified
 *
 * @pre start must be < 64
 *
 * Example:
 * ```c
 * u32 packed = 0b11010110;
 * u32 middle = bits_extract(packed, 2, 4);  // → 0b0101
 * ```
 *
 * @sa bits_insert()
 */
/*@
    requires start < 64;
    assigns  \nothing;
    behavior zero_count:
        assumes count == 0;
        ensures \result == 0;
    behavior full_width:
        assumes count >= 64;
        ensures \result == value >> start;
    behavior normal:
        assumes 0 < count < 64;
        ensures \result == ((value >> start) & (((u64)1 << count) - 1));
    complete behaviors;
    disjoint behaviors;
 */
static inline u64 bits_extract(u64 value, u32 start, u32 count) {
    if (count == 0) return 0;
    if (count >= 64) return value >> start;
    u64 mask = (1ULL << count) - 1;
    return (value >> start) & mask;
}

/**
 * @brief Insert bits into a value at a specific position
 *
 * Inserts the low `count` bits of `src` into `dst` at position `start`.
 * Other bits in `dst` are preserved.
 *
 * @param dst Destination value
 * @param src Source bits (only low `count` bits are used)
 * @param start Starting bit position in dst
 * @param count Number of bits to insert
 *
 * @return dst with the specified bits replaced by the low `count` bits of src
 *
 * @pre start must be < 64
 *
 * Example:
 * ```c
 * u32 dst = 0b11110000;
 * u32 src = 0b0101;
 * u32 result = bits_insert(dst, src, 2, 4);  // → 0b11010100
 * ```
 *
 * @sa bits_extract()
 */
/*@
    requires start < 64;
    assigns  \nothing;
    behavior zero_count:
        assumes count == 0;
        ensures \result == dst;
    behavior full_width:
        assumes count >= 64;
        ensures \result == (src << start);
    behavior normal:
        assumes 0 < count < 64;
        ensures \let m = ((u64)1 << count) - 1;
                \let shifted_mask = m << start;
                \result == ((dst & (0xFFFFFFFFFFFFFFFFULL ^ shifted_mask)) |
                           ((src << start) & shifted_mask));
    complete behaviors;
    disjoint behaviors;
 */
static inline u64 bits_insert(u64 dst, u64 src, u32 start, u32 count) {
    if (count == 0) return dst;
    if (count >= 64) {
        /* All 64 bits are replaced; shift src into position.
         * dst is fully overwritten — no bits of dst are preserved. */
        return src << start;
    }
    u64 mask = ((1ULL << count) - 1) << start;
    return (dst & ~mask) | ((src << start) & mask);
}

/* ============================================================================
 * Bit Counting Operations
 * ========================================================================= */

/**
 * @brief Count the number of set bits (population count)
 *
 * Returns the number of 1 bits in the value.
 *
 * @param value Value to count bits in
 *
 * @return Number of set bits (0-64)
 *
 * @remark With GCC/Clang builtins: 1 instruction (popcnt)
 * @remark With MSVC builtins: 1 instruction (__popcnt64)
 * @remark Without builtins: ~20 instructions (parallel bit counting)
 *
 * @note The fallback uses the SWAR (SIMD Within A Register) parallel
 *       bit-counting algorithm. The full functional specification
 *       (popcount(v) == number of 1-bits in v) requires an axiomatic
 *       definition beyond current SMT solver capabilities. The contract
 *       specifies provable range and zero properties; full functional
 *       correctness is verified by testing and fuzzing.
 *
 * Example:
 * ```c
 * u32 bits = 0b10110101;
 * u32 count = bits_popcount(bits);  // → 5
 * ```
 *
 * @sa bits_clz(), bits_ctz()
 */
/*@
    assigns \nothing;
    ensures 0 <= \result <= 64;
 */
static inline u32 bits_popcount(u64 value) {
#if CANON_BITS_GNUC
    return (u32)__builtin_popcountll(value);
#elif CANON_BITS_MSVC
    return (u32)__popcnt64(value);
#else
    /* Parallel bit counting (SWAR algorithm) */
    value = value - ((value >> 1) & 0x5555555555555555ULL);
    value = (value & 0x3333333333333333ULL) + ((value >> 2) & 0x3333333333333333ULL);
    value = (value + (value >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    return (u32)((value * 0x0101010101010101ULL) >> 56);
#endif
}

/**
 * @brief Count leading zeros (CLZ)
 *
 * Returns the number of zero bits before the first set bit,
 * counting from the MSB.
 *
 * @param value Value to count leading zeros in
 *
 * @return Number of leading zeros (0-63), or 64 if value == 0
 *
 * @remark With GCC/Clang builtins: 1 instruction (lzcnt/bsr)
 * @remark With MSVC builtins: 1 instruction (_BitScanReverse64)
 * @remark Returns 64 for value == 0 (differs from raw builtin behavior,
 *         which is undefined for 0 on some platforms)
 *
 * @note The fallback uses a binary search algorithm. The full functional
 *       specification (value >= 2^(63-result) && value < 2^(64-result))
 *       generates 64 sub-goals per ensures clause that current SMT solvers
 *       cannot close through the cascading mask-and-shift logic. The
 *       contract specifies the zero behavior exactly and provides range
 *       bounds for nonzero inputs; full functional correctness is verified
 *       by testing (100% MC/DC) and fuzzing.
 *
 * Example:
 * ```c
 * u64 val = 0b00001010;
 * u32 lz = bits_clz(val);  // → 60
 * ```
 *
 * @sa bits_ctz(), bits_fls()
 */
/*@
    assigns \nothing;
    behavior zero:
        assumes value == 0;
        ensures \result == 64;
    behavior nonzero:
        assumes value != 0;
        ensures 0 <= \result <= 63;
    complete behaviors;
    disjoint behaviors;
 */
static inline u32 bits_clz(u64 value) {
    if (value == 0) return 64;
#if CANON_BITS_GNUC
    return (u32)__builtin_clzll(value);
#elif CANON_BITS_MSVC
    unsigned long index;
    _BitScanReverse64(&index, value);
    return 63 - (u32)index;
#else
    /* Binary search for highest set bit.
     * Each step halves the remaining search range. */
    u32 count = 0;
    if ((value & 0xFFFFFFFF00000000ULL) == 0) { count += 32; value <<= 32; }
    if ((value & 0xFFFF000000000000ULL) == 0) { count += 16; value <<= 16; }
    if ((value & 0xFF00000000000000ULL) == 0) { count +=  8; value <<=  8; }
    if ((value & 0xF000000000000000ULL) == 0) { count +=  4; value <<=  4; }
    if ((value & 0xC000000000000000ULL) == 0) { count +=  2; value <<=  2; }
    if ((value & 0x8000000000000000ULL) == 0) { count +=  1; }
    return count;
#endif
}

/**
 * @brief Count trailing zeros (CTZ)
 *
 * Returns the number of zero bits after the last set bit,
 * counting from the LSB.
 *
 * @param value Value to count trailing zeros in
 *
 * @return Number of trailing zeros (0-63), or 64 if value == 0
 *
 * @remark With GCC/Clang builtins: 1 instruction (tzcnt/bsf)
 * @remark With MSVC builtins: 1 instruction (_BitScanForward64)
 * @remark Returns 64 for value == 0 (differs from raw builtin behavior,
 *         which is undefined for 0 on some platforms)
 *
 * @note The fallback uses a binary search algorithm. See bits_clz for
 *       rationale on the contract strength. Range bounds are proved;
 *       full functional correctness is verified by testing and fuzzing.
 *
 * Example:
 * ```c
 * u64 val = 0b10100000;
 * u32 tz = bits_ctz(val);  // → 5
 * ```
 *
 * @sa bits_clz(), bits_ffs()
 */
/*@
    assigns \nothing;
    behavior zero:
        assumes value == 0;
        ensures \result == 64;
    behavior nonzero:
        assumes value != 0;
        ensures 0 <= \result <= 63;
    complete behaviors;
    disjoint behaviors;
 */
static inline u32 bits_ctz(u64 value) {
    if (value == 0) return 64;
#if CANON_BITS_GNUC
    return (u32)__builtin_ctzll(value);
#elif CANON_BITS_MSVC
    unsigned long index;
    _BitScanForward64(&index, value);
    return (u32)index;
#else
    /* Binary search for lowest set bit. */
    u32 count = 0;
    if ((value & 0x00000000FFFFFFFFULL) == 0) { count += 32; value >>= 32; }
    if ((value & 0x000000000000FFFFULL) == 0) { count += 16; value >>= 16; }
    if ((value & 0x00000000000000FFULL) == 0) { count +=  8; value >>=  8; }
    if ((value & 0x000000000000000FULL) == 0) { count +=  4; value >>=  4; }
    if ((value & 0x0000000000000003ULL) == 0) { count +=  2; value >>=  2; }
    if ((value & 0x0000000000000001ULL) == 0) { count +=  1; }
    return count;
#endif
}

/**
 * @brief Find first set bit (FFS) — 1-indexed
 *
 * Returns the position of the lowest set bit, 1-indexed.
 * Returns 0 if no bits are set.
 *
 * @param value Value to search
 *
 * @return Bit position (1-64), or 0 if value == 0
 *
 * Example:
 * ```c
 * u64 val = 0b10100000;
 * u32 first = bits_ffs(val);  // → 6  (bit 5 zero-indexed, +1)
 * ```
 *
 * @sa bits_ctz(), bits_fls()
 */
/*@
    assigns \nothing;
    behavior zero:
        assumes value == 0;
        ensures \result == 0;
    behavior nonzero:
        assumes value != 0;
        ensures 1 <= \result <= 64;
    complete behaviors;
    disjoint behaviors;
 */
static inline u32 bits_ffs(u64 value) {
    if (value == 0) return 0;
    return bits_ctz(value) + 1;
}

/**
 * @brief Find last set bit (FLS) — 1-indexed
 *
 * Returns the position of the highest set bit, 1-indexed.
 * Returns 0 if no bits are set.
 *
 * @param value Value to search
 *
 * @return Bit position (1-64), or 0 if value == 0
 *
 * Example:
 * ```c
 * u64 val = 0b10100000;
 * u32 last = bits_fls(val);  // → 8  (bit 7 zero-indexed, +1)
 * ```
 *
 * @sa bits_clz(), bits_ffs()
 */
/*@
    assigns \nothing;
    behavior zero:
        assumes value == 0;
        ensures \result == 0;
    behavior nonzero:
        assumes value != 0;
        ensures 1 <= \result <= 64;
    complete behaviors;
    disjoint behaviors;
 */
static inline u32 bits_fls(u64 value) {
    if (value == 0) return 0;
    return 64 - bits_clz(value);
}

/* ============================================================================
 * Rotation Operations
 * ========================================================================= */

/**
 * @brief Rotate left (circular shift) on a full u64 value
 *
 * Rotates all 64 bits to the left by `shift` positions. Bits that fall off
 * the left (MSB) end wrap around to the right (LSB) end.
 *
 * @param value u64 value to rotate
 * @param shift Number of positions to rotate (automatically masked to 0-63)
 *
 * @return Rotated u64 value
 *
 * @remark shift >= 64 is automatically masked: effective shift = shift & 63
 * @remark These functions always operate on the full 64-bit width.
 *         For narrower rotations (e.g. 8-bit), mask the result yourself:
 *         bits_rotl(val, 2) & 0xFF  — but note this is not a true 8-bit
 *         rotation (high bits from the upper 56 bits may bleed in).
 *         Use a dedicated narrow helper for correct narrow rotations.
 *
 * Example (full u64):
 * ```c
 * u64 val = 0x8000000000000001ULL;  // MSB and LSB set
 * u64 rot = bits_rotl(val, 1);
 * // → 0x0000000000000003ULL  (MSB wraps to bit 0, bit 0 shifts to bit 1)
 * ```
 *
 * Example (illustrative 8-bit behavior — mask result yourself):
 * ```c
 * u64 val = 0b10110001;
 * u64 rot = bits_rotl(val, 2) & 0xFF;  // → 0b11000110
 * //   high bits 10 wrap to low end → 0b11000110
 * ```
 *
 * @sa bits_rotr()
 */
/*@
    assigns \nothing;
    behavior zero_shift:
        assumes (shift & 63) == 0;
        ensures \result == value;
    behavior nonzero_shift:
        assumes (shift & 63) != 0;
        ensures \let s = shift & 63;
                \result == ((value << s) | (value >> (64 - s)));
    complete behaviors;
    disjoint behaviors;
 */
static inline u64 bits_rotl(u64 value, u32 shift) {
    shift &= 63;
    if (shift == 0) return value;
    return (value << shift) | (value >> (64 - shift));
}

/**
 * @brief Rotate right (circular shift) on a full u64 value
 *
 * Rotates all 64 bits to the right by `shift` positions. Bits that fall off
 * the right (LSB) end wrap around to the left (MSB) end.
 *
 * @param value u64 value to rotate
 * @param shift Number of positions to rotate (automatically masked to 0-63)
 *
 * @return Rotated u64 value
 *
 * @remark shift >= 64 is automatically masked: effective shift = shift & 63
 * @remark These functions always operate on the full 64-bit width.
 *         For a true narrow rotation (e.g. 8-bit rotr by 2):
 *         ((narrow >> 2) | (narrow << 6)) & 0xFF
 *
 * Example (full u64):
 * ```c
 * u64 val = 0x8000000000000001ULL;  // MSB and LSB set
 * u64 rot = bits_rotr(val, 1);
 * // → 0xC000000000000000ULL  (LSB wraps to MSB, MSB shifts to bit 62)
 * ```
 *
 * Example (illustrative 8-bit behavior — use narrow formula above for
 * correct results):
 * ```c
 * u8 narrow = 0b10110001;
 * u8 rot = ((narrow >> 2) | (narrow << 6)) & 0xFF;  // → 0b01101100
 * ```
 *
 * @sa bits_rotl()
 */
/*@
    assigns \nothing;
    behavior zero_shift:
        assumes (shift & 63) == 0;
        ensures \result == value;
    behavior nonzero_shift:
        assumes (shift & 63) != 0;
        ensures \let s = shift & 63;
                \result == ((value >> s) | (value << (64 - s)));
    complete behaviors;
    disjoint behaviors;
 */
static inline u64 bits_rotr(u64 value, u32 shift) {
    shift &= 63;
    if (shift == 0) return value;
    return (value >> shift) | (value << (64 - shift));
}

/* ============================================================================
 * Power-of-Two Operations
 * ========================================================================= */

/**
 * @brief Check if value is a power of two
 *
 * Returns true if value is 1, 2, 4, 8, 16, ..., false otherwise.
 *
 * @param value Value to check
 *
 * @return true if value is a power of 2, false otherwise
 *
 * @remark Returns false for 0
 * @remark Compiles to: (value != 0) && ((value & (value - 1)) == 0)
 *
 * Example:
 * ```c
 * bits_is_power_of_two(16)  // → true
 * bits_is_power_of_two(15)  // → false
 * bits_is_power_of_two(0)   // → false
 * ```
 *
 * @sa bits_next_power_of_two()
 */
/*@
    assigns \nothing;
    ensures \result <==> (value != 0 && (value & (value - 1)) == 0);
 */
static inline bool bits_is_power_of_two(u64 value) {
    return value != 0 && (value & (value - 1)) == 0;
}

/**
 * @brief Round up to next power of two
 *
 * Returns the smallest power of 2 that is >= value.
 * Returns 0 if value == 0, or on overflow (value > 2^63).
 *
 * @param value Value to round up
 *
 * @return Next power of 2 >= value, or 0 if value == 0 or would overflow u64
 *
 * @remark Useful for hash table sizing, buffer allocation
 * @remark Values that are already a power of two are returned unchanged
 *
 * Example:
 * ```c
 * bits_next_power_of_two(17)   // → 32
 * bits_next_power_of_two(32)   // → 32
 * bits_next_power_of_two(1000) // → 1024
 * bits_next_power_of_two(0)    // → 0
 * ```
 *
 * @sa bits_is_power_of_two()
 */
/*@
    assigns \nothing;
    behavior zero:
        assumes value == 0;
        ensures \result == 0;
    behavior overflow:
        assumes value > ((u64)1 << 63);
        ensures \result == 0;
    behavior normal:
        assumes 0 < value <= ((u64)1 << 63);
        ensures \result >= value;
        ensures \result != 0;
        ensures (\result & (\result - 1)) == 0;
        ensures \result / 2 < value;
    complete behaviors;
    disjoint behaviors;
 */
static inline u64 bits_next_power_of_two(u64 value) {
    if (value == 0) return 0;
    if (value > (1ULL << 63)) return 0;  /* Would overflow */

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return value + 1;
}

/* ============================================================================
 * Byte Reversal
 * ========================================================================= */

/**
 * @brief Reverse bytes in a 16-bit value
 *
 * @param value Value to byte-swap
 *
 * @return Value with bytes swapped
 *
 * @remark With GCC/Clang builtins: 1 instruction (__builtin_bswap16)
 * @remark With MSVC builtins: 1 instruction (_byteswap_ushort)
 *
 * Example:
 * ```c
 * u16 val = 0x1234;
 * u16 rev = bits_bswap16(val);  // → 0x3412
 * ```
 */
/*@
    assigns \nothing;
    ensures \result == (u16)((value >> 8) | (value << 8));
 */
static inline u16 bits_bswap16(u16 value) {
#if CANON_BITS_GNUC
    return __builtin_bswap16(value);
#elif CANON_BITS_MSVC
    return _byteswap_ushort(value);
#else
    return (u16)((value >> 8) | (value << 8));
#endif
}

/**
 * @brief Reverse bytes in a 32-bit value
 *
 * @param value Value to byte-swap
 *
 * @return Value with bytes reversed
 *
 * @remark With GCC/Clang builtins: 1 instruction (__builtin_bswap32)
 * @remark With MSVC builtins: 1 instruction (_byteswap_ulong)
 *
 * Example:
 * ```c
 * u32 val = 0x12345678;
 * u32 rev = bits_bswap32(val);  // → 0x78563412
 * ```
 */
/*@
    assigns \nothing;
    ensures \result == (((value & 0x000000FFu) << 24) |
                        ((value & 0x0000FF00u) <<  8) |
                        ((value & 0x00FF0000u) >>  8) |
                        ((value & 0xFF000000u) >> 24));
 */
static inline u32 bits_bswap32(u32 value) {
#if CANON_BITS_GNUC
    return __builtin_bswap32(value);
#elif CANON_BITS_MSVC
    return _byteswap_ulong(value);
#else
    return ((value & 0x000000FFU) << 24) |
           ((value & 0x0000FF00U) <<  8) |
           ((value & 0x00FF0000U) >>  8) |
           ((value & 0xFF000000U) >> 24);
#endif
}

/**
 * @brief Reverse bytes in a 64-bit value
 *
 * @param value Value to byte-swap
 *
 * @return Value with bytes reversed
 *
 * @remark With GCC/Clang builtins: 1 instruction (__builtin_bswap64)
 * @remark With MSVC builtins: 1 instruction (_byteswap_uint64)
 *
 * Example:
 * ```c
 * u64 val = 0x0102030405060708ULL;
 * u64 rev = bits_bswap64(val);  // → 0x0807060504030201ULL
 * ```
 */
/*@
    assigns \nothing;
    ensures \result == (((value & 0x00000000000000FFULL) << 56) |
                        ((value & 0x000000000000FF00ULL) << 40) |
                        ((value & 0x0000000000FF0000ULL) << 24) |
                        ((value & 0x00000000FF000000ULL) <<  8) |
                        ((value & 0x000000FF00000000ULL) >>  8) |
                        ((value & 0x0000FF0000000000ULL) >> 24) |
                        ((value & 0x00FF000000000000ULL) >> 40) |
                        ((value & 0xFF00000000000000ULL) >> 56));
 */
static inline u64 bits_bswap64(u64 value) {
#if CANON_BITS_GNUC
    return __builtin_bswap64(value);
#elif CANON_BITS_MSVC
    return _byteswap_uint64(value);
#else
    return ((value & 0x00000000000000FFULL) << 56) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x00000000FF000000ULL) <<  8) |
           ((value & 0x000000FF00000000ULL) >>  8) |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0xFF00000000000000ULL) >> 56);
#endif
}

/* ============================================================================
 * Usage Examples (documentation only)
 * ========================================================================= */

/*
// Flags management
typedef enum {
    FLAG_DIRTY   = 1 << 0,
    FLAG_LOADED  = 1 << 1,
    FLAG_SYNCED  = 1 << 2,
} CacheFlags;

void cache_mark_dirty(Cache* cache) {
    cache->flags = bits_set(cache->flags, 0);  // Set DIRTY bit
}

bool cache_is_dirty(Cache* cache) {
    return bits_test(cache->flags, 0);
}

// Allocator metadata
u64 free_blocks = 0xFFFFFFFF00000000;  // Upper 32 blocks free
usize num_free = bits_popcount(free_blocks);  // → 32

// Hash table sizing
usize capacity = 17;
capacity = bits_next_power_of_two(capacity);  // → 32

// Bit field extraction (network packet parsing)
u32 packet_header = 0b10110101001100110000111100001111;
u32 version = bits_extract(packet_header, 28, 4);    // Top 4 bits
u32 flags   = bits_extract(packet_header, 24, 4);    // Next 4 bits
u32 length  = bits_extract(packet_header, 0, 24);    // Bottom 24 bits

// Alignment verification
if (!bits_is_power_of_two(alignment)) {
    return ERROR_INVALID_ALIGNMENT;
}
*/

#endif /* CANON_CORE_PRIMITIVES_BITS_H */
