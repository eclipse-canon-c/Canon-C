/**
 * @file limits.h
 * @brief Common constants and limits for Canon-C
 *
 * Provides frequently-used numeric limits, alignment constants, and
 * platform-specific values. Centralizes magic numbers to improve
 * readability and maintainability.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicitness: Named constants replace magic numbers
 * - Readability: CANON_USIZE_MAX is clearer than ((usize)-1)
 * - Portability: Platform-specific values are computed correctly
 * - Convenience: Commonly-used values available in one place
 * - Type-safety: Constants have appropriate types for their use
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - O(1): All constants are compile-time (no runtime cost)
 * - Zero overhead: Constants fold into immediate values
 * - No code generation: Pure compile-time computation
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * N/A — pure compile-time constants, no runtime behavior.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (stdint.h, limits.h)
 * - All constants computed from standard C macros
 * - Works on all platforms (16-bit, 32-bit, 64-bit)
 * - Alignment constants adapt to platform max_align_t
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Overflow checks: if (offset > CANON_USIZE_MAX - size)
 * - Alignment: align_up(size, CANON_DEFAULT_ALIGN)
 * - Capacity limits: if (len > CANON_VEC_MAX_CAPACITY)
 * - Cache optimization: align_to(ptr, CANON_CACHE_LINE)
 * - Page alignment: align_to(size, CANON_PAGE_SIZE)
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Runtime configuration (use variables instead)
 * - User-tunable parameters (define separately)
 * - Platform detection (use proper feature macros)
 *
 * @sa types.h, checked.h
 */

#ifndef CANON_CORE_PRIMITIVES_LIMITS_H
#define CANON_CORE_PRIMITIVES_LIMITS_H

#include "types.h"
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * Integer Type Limits
 * ========================================================================= */

/**
 * @brief Maximum value for unsigned 8-bit integer
 *
 * @remark 255
 */
#define CANON_U8_MAX   UINT8_MAX

/**
 * @brief Maximum value for unsigned 16-bit integer
 *
 * @remark 65,535
 */
#define CANON_U16_MAX  UINT16_MAX

/**
 * @brief Maximum value for unsigned 32-bit integer
 *
 * @remark 4,294,967,295
 */
#define CANON_U32_MAX  UINT32_MAX

/**
 * @brief Maximum value for unsigned 64-bit integer
 *
 * @remark 18,446,744,073,709,551,615
 */
#define CANON_U64_MAX  UINT64_MAX

/**
 * @brief Minimum value for signed 8-bit integer
 *
 * @remark -128
 */
#define CANON_I8_MIN   INT8_MIN

/**
 * @brief Maximum value for signed 8-bit integer
 *
 * @remark 127
 */
#define CANON_I8_MAX   INT8_MAX

/**
 * @brief Minimum value for signed 16-bit integer
 *
 * @remark -32,768
 */
#define CANON_I16_MIN  INT16_MIN

/**
 * @brief Maximum value for signed 16-bit integer
 *
 * @remark 32,767
 */
#define CANON_I16_MAX  INT16_MAX

/**
 * @brief Minimum value for signed 32-bit integer
 *
 * @remark -2,147,483,648
 */
#define CANON_I32_MIN  INT32_MIN

/**
 * @brief Maximum value for signed 32-bit integer
 *
 * @remark 2,147,483,647
 */
#define CANON_I32_MAX  INT32_MAX

/**
 * @brief Minimum value for signed 64-bit integer
 *
 * @remark -9,223,372,036,854,775,808
 */
#define CANON_I64_MIN  INT64_MIN

/**
 * @brief Maximum value for signed 64-bit integer
 *
 * @remark 9,223,372,036,854,775,807
 */
#define CANON_I64_MAX  INT64_MAX

/* ============================================================================
 * Size Type Limits (Platform-Dependent)
 * ========================================================================= */

/**
 * @brief Maximum value for usize (platform-dependent)
 *
 * @remark 2^32-1 on 32-bit platforms, 2^64-1 on 64-bit platforms
 *
 * Example:
 * ```c
 * if (offset > CANON_USIZE_MAX - size) {
 *     return ERROR_OVERFLOW;
 * }
 * ```
 */
#define CANON_USIZE_MAX  SIZE_MAX

/**
 * @brief Maximum value for isize (platform-dependent)
 *
 * @remark 2^31-1 on 32-bit platforms, 2^63-1 on 64-bit platforms
 */
#define CANON_ISIZE_MAX  PTRDIFF_MAX

/**
 * @brief Minimum value for isize (platform-dependent)
 *
 * @remark -2^31 on 32-bit platforms, -2^63 on 64-bit platforms
 */
#define CANON_ISIZE_MIN  PTRDIFF_MIN

/* ============================================================================
 * Alignment Constants
 * ========================================================================= */

/**
 * @brief Default alignment for most types
 *
 * Uses the platform's natural alignment for max_align_t.
 * Typically 8 bytes on 64-bit systems, 4 bytes on 32-bit.
 *
 * @remark Use this for general-purpose allocations
 *
 * Example:
 * ```c
 * usize aligned = align_up(size, CANON_DEFAULT_ALIGN);
 * ```
 */
#define CANON_DEFAULT_ALIGN  _Alignof(max_align_t)

/**
 * @brief Cache line size for performance optimization
 *
 * Most modern CPUs use 64-byte cache lines. Aligning to this
 * prevents false sharing in multithreaded code.
 *
 * @remark 64 bytes on most x86/x86_64/ARM64 platforms
 *
 * Example:
 * ```c
 * // Prevent false sharing between threads
 * struct alignas(CANON_CACHE_LINE) PerThreadData {
 *     u64 counter;
 *     // ... pad to 64 bytes
 * };
 * ```
 */
#define CANON_CACHE_LINE  64

/**
 * @brief SIMD alignment for vectorized operations
 *
 * 16 bytes is sufficient for SSE (128-bit) operations.
 * AVX requires 32-byte alignment (use CANON_SIMD_ALIGN_AVX).
 *
 * @remark 16 bytes for SSE/NEON
 */
#define CANON_SIMD_ALIGN  16

/**
 * @brief AVX SIMD alignment
 *
 * Required for AVX (256-bit) operations on x86_64.
 *
 * @remark 32 bytes for AVX
 */
#define CANON_SIMD_ALIGN_AVX  32

/**
 * @brief AVX-512 SIMD alignment
 *
 * Required for AVX-512 (512-bit) operations.
 *
 * @remark 64 bytes for AVX-512
 */
#define CANON_SIMD_ALIGN_AVX512  64

/**
 * @brief Typical memory page size
 *
 * Most systems use 4KB pages. Some systems support larger pages
 * (2MB, 1GB) but 4KB is the common baseline.
 *
 * @remark 4096 bytes (4KB) on most systems
 *
 * Example:
 * ```c
 * // Allocate page-aligned memory
 * void* mem = aligned_alloc(CANON_PAGE_SIZE, CANON_PAGE_SIZE);
 * ```
 */
#define CANON_PAGE_SIZE  4096

/* ============================================================================
 * Common Size Constants
 * ========================================================================= */

/**
 * @brief One kilobyte (1024 bytes)
 */
#define CANON_KB  ((usize)1024)

/**
 * @brief One megabyte (1024 * 1024 bytes)
 */
#define CANON_MB  ((usize)1024 * 1024)

/**
 * @brief One gigabyte (1024 * 1024 * 1024 bytes)
 */
#define CANON_GB  ((usize)1024 * 1024 * 1024)

/* ============================================================================
 * Practical Capacity Limits
 * ========================================================================= */

/**
 * @brief Reasonable maximum capacity for dynamic vectors
 *
 * Prevents excessive memory allocation. Set to 1GB by default.
 * Applications can override this if needed.
 *
 * @remark 1GB / sizeof(element) maximum elements
 *
 * Example:
 * ```c
 * if (new_capacity > CANON_VEC_MAX_CAPACITY / sizeof(Item)) {
 *     return ERROR_CAPACITY_EXCEEDED;
 * }
 * ```
 */
#ifndef CANON_VEC_MAX_CAPACITY
    #define CANON_VEC_MAX_CAPACITY  CANON_GB
#endif

/**
 * @brief Reasonable maximum size for dynamic strings
 *
 * Prevents excessive memory allocation for strings. Set to 16MB by default.
 *
 * @remark 16MB maximum string length
 */
#ifndef CANON_STRING_MAX_SIZE
    #define CANON_STRING_MAX_SIZE  (16 * CANON_MB)
#endif

/**
 * @brief Maximum arena size (practical limit)
 *
 * Arenas larger than this are likely errors. Set to 1GB by default.
 *
 * @remark 1GB maximum arena capacity
 */
#ifndef CANON_ARENA_MAX_SIZE
    #define CANON_ARENA_MAX_SIZE  CANON_GB
#endif

/* ============================================================================
 * Small Buffer Optimization Thresholds
 * ========================================================================= */

/**
 * @brief Threshold for small string optimization
 *
 * Strings smaller than this can be stored inline without allocation.
 * Typically 15-23 bytes (leaving room for length + null terminator).
 *
 * @remark 23 bytes (fits in 24-byte struct with 1-byte length)
 */
#define CANON_SSO_THRESHOLD  23

/**
 * @brief Threshold for small vector optimization
 *
 * Vectors with capacity below this can use inline storage.
 * Typically 4-16 elements depending on element size.
 *
 * @remark 8 elements (reasonable default for small types)
 */
#define CANON_SVO_THRESHOLD  8

/* ============================================================================
 * Growth Factor Constants
 * ========================================================================= */

/**
 * @brief Default growth factor for dynamic containers (multiplicative)
 *
 * When growing capacity, multiply by this factor. 2.0 is aggressive
 * (doubles each time), 1.5 is more memory-efficient.
 *
 * @remark 1.5x growth (balanced between speed and memory)
 */
#define CANON_GROWTH_FACTOR_NUM    3
#define CANON_GROWTH_FACTOR_DENOM  2

/**
 * @brief Minimum allocation size for dynamic containers
 *
 * Start with at least this many bytes to avoid many small allocations.
 *
 * @remark 32 bytes (holds 4-8 pointers or 8-32 small elements)
 */
#define CANON_MIN_ALLOCATION  32

/* ============================================================================
 * Pointer Tagging Constants (Advanced Use)
 * ========================================================================= */

/**
 * @brief Number of low bits available for pointer tagging
 *
 * On most platforms, pointers to aligned data have low bits that are
 * always zero. These can be used to store metadata.
 *
 * @remark 3 bits on 64-bit (8-byte alignment = 2^3)
 * @remark 2 bits on 32-bit (4-byte alignment = 2^2)
 */
#if UINTPTR_MAX == UINT64_MAX
    #define CANON_PTR_TAG_BITS  3
#elif UINTPTR_MAX == UINT32_MAX
    #define CANON_PTR_TAG_BITS  2
#else
    #define CANON_PTR_TAG_BITS  1  /* Conservative fallback */
#endif

/**
 * @brief Mask for extracting pointer tag bits
 */
#define CANON_PTR_TAG_MASK  ((1ULL << CANON_PTR_TAG_BITS) - 1)

/**
 * @brief Mask for extracting pointer address (removing tag bits)
 */
#define CANON_PTR_ADDR_MASK  (~CANON_PTR_TAG_MASK)

/* ============================================================================
 * Platform Detection Helpers (Read-Only Information)
 * ========================================================================= */

/**
 * @brief Size of a pointer in bytes (4 on 32-bit, 8 on 64-bit)
 */
#define CANON_POINTER_SIZE  sizeof(void*)

/**
 * @brief Number of bits in a byte (always 8 on modern systems)
 */
#define CANON_BITS_PER_BYTE  CHAR_BIT

/**
 * @brief Number of bits in a pointer (32 or 64)
 */
#define CANON_POINTER_BITS  (CANON_POINTER_SIZE * CANON_BITS_PER_BYTE)

/* ============================================================================
 * Usage Examples (documentation only)
 * ========================================================================= */

/*
// Overflow detection using limits
bool arena_can_alloc(Arena* arena, usize size) {
    if (size > CANON_USIZE_MAX - arena->offset) {
        return false;  // Would overflow
    }
    return arena->offset + size <= arena->capacity;
}

// Alignment for performance
typedef struct alignas(CANON_CACHE_LINE) PerCpuData {
    u64 counter;
    u64 timestamp;
    // Padded to 64 bytes to prevent false sharing
} PerCpuData;

// Capacity limits
Result_Vec vec_grow(Vec* vec) {
    if (vec->capacity > CANON_VEC_MAX_CAPACITY / 2) {
        return result_err(ERROR_CAPACITY_EXCEEDED);
    }
    usize new_cap = vec->capacity * CANON_GROWTH_FACTOR_NUM / CANON_GROWTH_FACTOR_DENOM;
    // ... grow logic
}

// Size constants
Arena arena = arena_create(malloc(2 * CANON_MB), 2 * CANON_MB);

// SIMD alignment
void* simd_buffer = aligned_alloc(CANON_SIMD_ALIGN, 1024);

// Pointer tagging (advanced)
uintptr_t tagged = ((uintptr_t)ptr & CANON_PTR_ADDR_MASK) | tag;
void* untagged = (void*)(tagged & CANON_PTR_ADDR_MASK);
usize tag = tagged & CANON_PTR_TAG_MASK;
*/

#endif /* CANON_CORE_PRIMITIVES_LIMITS_H */
