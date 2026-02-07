/**
 * @file types.h
 * @brief Portable integer and size type aliases for Canon-C
 *
 * Provides short, readable type names that replace verbose stdint.h types.
 * This is the foundation header — every other module builds on these types.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicitness: Type names clearly indicate signedness and size
 * - Readability: Short names (u8, usize) reduce cognitive load vs uint8_t, size_t
 * - Portability: Guaranteed sizes across all platforms (via C99 stdint.h)
 * - Zero cost: Pure type aliases, no runtime overhead
 * - Consistency: Uniform naming convention across entire codebase
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - O(1): Pure compile-time type aliases (no runtime cost)
 * - Zero overhead: These are compile-time aliases
 * - No conversions, no hidden operations
 * - Identical codegen to using stdint.h types directly
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * N/A — pure type definitions, no runtime behavior
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (stdint.h, stddef.h, stdbool.h)
 * - Works on all platforms where stdint.h exists (essentially universal)
 * - usize/isize match pointer size (32-bit on 32-bit systems, 64-bit on 64-bit)
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Buffer handling: u8* buffer, usize capacity
 * - Array indexing: for (usize i = 0; i < len; i++)
 * - Offset calculations: isize offset = ptr2 - ptr1
 * - Binary data: byte* raw_data
 * - Fixed-width integers: u32 hash, i64 timestamp
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Bit-fields (use unsigned int per C standard)
 * - Atomic operations (use C11 _Atomic types directly)
 * - printf format specifiers (use PRIu32, PRId64 from inttypes.h)
 *
 * @sa checked.h, bits.h
 */

#ifndef CANON_CORE_PRIMITIVES_TYPES_H
#define CANON_CORE_PRIMITIVES_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * Unsigned Integer Types
 * ========================================================================= */

typedef uint8_t   u8;    /* 0 to 255 */
typedef uint16_t  u16;   /* 0 to 65,535 */
typedef uint32_t  u32;   /* 0 to 4,294,967,295 */
typedef uint64_t  u64;   /* 0 to 18,446,744,073,709,551,615 */

/* ============================================================================
 * Signed Integer Types
 * ========================================================================= */

typedef int8_t    i8;    /* -128 to 127 */
typedef int16_t   i16;   /* -32,768 to 32,767 */
typedef int32_t   i32;   /* -2,147,483,648 to 2,147,483,647 */
typedef int64_t   i64;   /* -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 */

/* ============================================================================
 * Size and Pointer Types
 * ========================================================================= */

typedef size_t    usize;  /* Platform-dependent unsigned size (32/64-bit) */
typedef ptrdiff_t isize;  /* Platform-dependent signed difference (32/64-bit) */

/* ============================================================================
 * Byte Type (Semantic Clarity)
 * ========================================================================= */

typedef u8 byte;  /* Raw byte - use when data is untyped/binary */

/* ============================================================================
 * Floating Point Types (Optional - uncomment if needed)
 * ========================================================================= */

typedef float  f32;   /* 32-bit IEEE 754 floating point */
typedef double f64;   /* 64-bit IEEE 754 floating point */

/* ============================================================================
 * Boolean Type (Explicit)
 * ========================================================================= */

/* Note: We use standard <stdbool.h> which provides:
 *   - bool type
 *   - true constant
 *   - false constant
 * 
 * This is preferred over custom typedefs for C99+ compatibility.
 */

/* ============================================================================
 * Usage Examples
 * ========================================================================= */

/*
 * // Buffer handling
 * u8* buffer = malloc(1024);
 * usize capacity = 1024;
 * usize used = 0;
 * 
 * // Offset calculations  
 * isize offset = ptr2 - ptr1;
 * 
 * // Raw byte manipulation
 * byte* raw_data = (byte*)&some_struct;
 * 
 * // Loop indices (prefer unsigned for array indexing)
 * for (usize i = 0; i < count; i++) {
 *     process(items[i]);
 * }
 * 
 * // Function signatures
 * bool copy_bytes(byte* dst, const byte* src, usize n);
 * isize find_offset(const u8* haystack, usize len, u8 needle);
 */

#endif /* CANON_CORE_PRIMITIVES_TYPES_H */
