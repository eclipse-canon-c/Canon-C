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
 * - Compiler builtins detected via __GNUC__, __clang__, _MSC_VER
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
 * @sa types.h, checked.h
 */

#ifndef CANON_CORE_PRIMITIVES_BITS_H
#define CANON_CORE_PRIMITIVES_BITS_H

#include "types.h"

/* ============================================================================
 * Compiler Builtin Detection
 * ========================================================================= */

#if defined(__GNUC__) || defined(__clang__)
    #define CANON_HAS_BUILTIN_BITS 1
#elif defined(_MSC_VER)
    #define CANON_HAS_BUILTIN_BITS 1
    #include <intrin.h>
#else
    #define CANON_HAS_BUILTIN_BITS 0
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
 * @pre bit must be < 64 for u64 (not checked — UB if violated)
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
 * @pre bit must be < 64 for u64 (not checked — UB if violated)
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
 * @pre bit must be < 64 for u64 (not checked — UB if violated)
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
 * @remark Compiles to: value ^ (1ULL << bit)
 *
 * Example:
 * ```c
 * u32 flags = 0b1010;
 * flags = bits_toggle(flags, 0);  // → 0b1011
 * flags = bits_toggle(flags, 0);  // → 0b1010
 * ```
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
 * @pre start + count must be <= 64 (not checked — UB if violated)
 *
 * Example:
 * ```c
 * u32 packed = 0b11010110;
 * u32 middle = bits_extract(packed, 2, 4);  // → 0b0101
 * ```
 *
 * @sa bits_insert()
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
 * @return dst with inserted bits
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
static inline u64 bits_insert(u64 dst, u64 src, u32 start, u32 count) {
    if (count == 0) return dst;
    if (count >= 64) return (src << start);
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
 * @remark With builtins: 1 instruction (popcnt)
 * @remark Without builtins: ~20 instructions (parallel bit counting)
 *
 * Example:
 * ```c
 * u32 bits = 0b10110101;
 * u32 count = bits_popcount(bits);  // → 5
 * ```
 *
 * @sa bits_clz(), bits_ctz()
 */
static inline u32 bits_popcount(u64 value) {
#if CANON_HAS_BUILTIN_BITS && defined(__GNUC__)
    return (u32)__builtin_popcountll(value);
#elif CANON_HAS_BUILTIN_BITS && defined(_MSC_VER)
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
 * @return Number of leading zeros (0-64), or 64 if value == 0
 *
 * @remark With builtins: 1 instruction (bsr/lzcnt)
 * @remark Returns 64 for value == 0 (differs from some builtin behavior)
 *
 * Example:
 * ```c
 * u64 val = 0b00001010;
 * u32 lz = bits_clz(val);  // → 60 (on 64-bit)
 * ```
 *
 * @sa bits_ctz(), bits_fls()
 */
static inline u32 bits_clz(u64 value) {
    if (value == 0) return 64;
#if CANON_HAS_BUILTIN_BITS && defined(__GNUC__)
    return (u32)__builtin_clzll(value);
#elif CANON_HAS_BUILTIN_BITS && defined(_MSC_VER)
    unsigned long index;
    _BitScanReverse64(&index, value);
    return 63 - (u32)index;
#else
    /* Binary search for highest set bit */
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
 * @return Number of trailing zeros (0-64), or 64 if value == 0
 *
 * @remark With builtins: 1 instruction (bsf/tzcnt)
 * @remark Returns 64 for value == 0 (differs from some builtin behavior)
 *
 * Example:
 * ```c
 * u64 val = 0b10100000;
 * u32 tz = bits_ctz(val);  // → 5
 * ```
 *
 * @sa bits_clz(), bits_ffs()
 */
static inline u32 bits_ctz(u64 value) {
    if (value == 0) return 64;
#if CANON_HAS_BUILTIN_BITS && defined(__GNUC__)
    return (u32)__builtin_ctzll(value);
#elif CANON_HAS_BUILTIN_BITS && defined(_MSC_VER)
    unsigned long index;
    _BitScanForward64(&index, value);
    return (u32)index;
#else
    /* Binary search for lowest set bit */
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
 * @brief Find first set bit (FFS) - 1-indexed
 *
 * Returns the position of the lowest set bit, 1-indexed.
 * Returns 0 if no bits are set.
 *
 * @return Bit position (1-64), or 0 if value == 0
 *
 * Example:
 * ```c
 * u32 val = 0b10100000;
 * u32 first = bits_ffs(val);  // → 6 (1-indexed)
 * ```
 */
static inline u32 bits_ffs(u64 value) {
    if (value == 0) return 0;
    return bits_ctz(value) + 1;
}

/**
 * @brief Find last set bit (FLS) - 1-indexed
 *
 * Returns the position of the highest set bit, 1-indexed.
 * Returns 0 if no bits are set.
 *
 * @return Bit position (1-64), or 0 if value == 0
 *
 * Example:
 * ```c
 * u32 val = 0b10100000;
 * u32 last = bits_fls(val);  // → 8 (1-indexed)
 * ```
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
 * @remark shift >= 64 is automatically masked to shift % 64 (shift &= 63)
 * @remark These functions always operate on the full 64-bit width.
 *         If you need rotation on a narrower type (e.g. 8-bit), mask
 *         the result: bits_rotl(val, 2) & 0xFF
 *
 * Example (full u64):
 * ```c
 * u64 val = 0x00000000000000B1ULL;  // 0b10110001
 * u64 rot = bits_rotl(val, 2);
 * // → 0x00000000000002C4ULL  (bits shifted left by 2, nothing wraps from MSB
 * //                           because upper 62 bits were all zero)
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
static inline u64 bits_rotl(u64 value, u32 shift) {
    shift &= 63;  /* Ensure shift is in valid range */
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
 * @remark shift >= 64 is automatically masked to shift % 64 (shift &= 63)
 * @remark These functions always operate on the full 64-bit width.
 *         If you need rotation on a narrower type (e.g. 8-bit), mask
 *         the input first: bits_rotr(val & 0xFF | (val & 0xFF) << 8, 2) & 0xFF
 *         or use a dedicated narrow-rotation helper.
 *
 * Example (full u64):
 * ```c
 * u64 val = 0x00000000000000B1ULL;  // 0b10110001
 * u64 rot = bits_rotr(val, 2);
 * // → 0x40000000000000002CULL  (low 2 bits 01 wrap to the top of the u64)
 * ```
 *
 * Example (illustrative 8-bit behavior — mask input and result yourself):
 * ```c
 * u8 narrow = 0b10110001;
 * // For true 8-bit rotr: ((narrow >> 2) | (narrow << 6)) & 0xFF
 * // → 0b01101100
 * ```
 *
 * @sa bits_rotl()
 */
static inline u64 bits_rotr(u64 value, u32 shift) {
    shift &= 63;  /* Ensure shift is in valid range */
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
 * @return Next power of 2, or 0 on overflow
 *
 * @remark Useful for hash table sizing, buffer allocation
 *
 * Example:
 * ```c
 * bits_next_power_of_two(17)   // → 32
 * bits_next_power_of_two(32)   // → 32
 * bits_next_power_of_two(1000) // → 1024
 * ```
 *
 * @sa bits_is_power_of_two()
 */
static inline u64 bits_next_power_of_two(u64 value) {
    if (value == 0) return 0;
    if (value > (1ULL << 63)) return 0;  /* Overflow */
    
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
 * Byte Reversal (for endianness handling)
 * ========================================================================= */

/**
 * @brief Reverse bytes in a 16-bit value
 *
 * @param value Value to reverse
 * @return Value with bytes swapped
 *
 * Example:
 * ```c
 * u16 val = 0x1234;
 * u16 rev = bits_bswap16(val);  // → 0x3412
 * ```
 */
static inline u16 bits_bswap16(u16 value) {
    return (value >> 8) | (value << 8);
}

/**
 * @brief Reverse bytes in a 32-bit value
 */
static inline u32 bits_bswap32(u32 value) {
#if CANON_HAS_BUILTIN_BITS && defined(__GNUC__)
    return __builtin_bswap32(value);
#elif CANON_HAS_BUILTIN_BITS && defined(_MSC_VER)
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
 */
static inline u64 bits_bswap64(u64 value) {
#if CANON_HAS_BUILTIN_BITS && defined(__GNUC__)
    return __builtin_bswap64(value);
#elif CANON_HAS_BUILTIN_BITS && defined(_MSC_VER)
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
