/**
 * @file limits.h
 * @brief Common constants and limits for Canon-C
 *
 * Provides numeric limits, alignment constants, capacity thresholds,
 * and platform-specific values. Centralizes magic numbers to improve
 * readability and maintainability.
 *
 * Design:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit: named constants replace magic numbers
 * - Portable: works on 16/32/64-bit, C99 and later
 * - Overridable: define any CANON_* constant before including to override
 * - Zero overhead: all constants are compile-time, no code generated
 *
 * Override pattern (define BEFORE including this header):
 * ────────────────────────────────────────────────────────────────────────────
 *   #define CANON_PAGE_SIZE          16384   // Android 15+, some ARM64 servers
 *   #define CANON_CACHE_LINE         128     // Apple M-series L2+
 *   #define CANON_VEC_MAX_CAPACITY   (CANON_GB * 4)
 *   #include "core/primitives/limits.h"
 *
 * @sa types.h
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

/** @brief Max value of u8  — 255 */
#define CANON_U8_MAX   UINT8_MAX
/** @brief Max value of u16 — 65,535 */
#define CANON_U16_MAX  UINT16_MAX
/** @brief Max value of u32 — 4,294,967,295 */
#define CANON_U32_MAX  UINT32_MAX
/** @brief Max value of u64 — 18,446,744,073,709,551,615 */
#define CANON_U64_MAX  UINT64_MAX

/** @brief Min value of i8  — -128 */
#define CANON_I8_MIN   INT8_MIN
/** @brief Max value of i8  — 127 */
#define CANON_I8_MAX   INT8_MAX
/** @brief Min value of i16 — -32,768 */
#define CANON_I16_MIN  INT16_MIN
/** @brief Max value of i16 — 32,767 */
#define CANON_I16_MAX  INT16_MAX
/** @brief Min value of i32 — -2,147,483,648 */
#define CANON_I32_MIN  INT32_MIN
/** @brief Max value of i32 — 2,147,483,647 */
#define CANON_I32_MAX  INT32_MAX
/** @brief Min value of i64 — -9,223,372,036,854,775,808 */
#define CANON_I64_MIN  INT64_MIN
/** @brief Max value of i64 — 9,223,372,036,854,775,807 */
#define CANON_I64_MAX  INT64_MAX

/* ============================================================================
 * Size Type Limits (Platform-Dependent)
 * ========================================================================= */

/**
 * @brief Max value of usize — SIZE_MAX (2^32-1 or 2^64-1)
 *
 * Example — overflow guard:
 * ```c
 * if (size > CANON_USIZE_MAX - arena->offset) { return ERROR_OVERFLOW; }
 * ```
 */
#define CANON_USIZE_MAX  SIZE_MAX

/** @brief Max value of isize — PTRDIFF_MAX */
#define CANON_ISIZE_MAX  PTRDIFF_MAX

/** @brief Min value of isize — PTRDIFF_MIN */
#define CANON_ISIZE_MIN  PTRDIFF_MIN

/* ============================================================================
 * Size Literals
 * ========================================================================= */

/**
 * @brief 1 KiB — 1,024 bytes
 *
 * All size literals use usize casts on every factor to avoid signed integer
 * overflow on 32-bit platforms where plain int arithmetic would overflow before
 * the outer cast takes effect.
 */
#define CANON_KB ((usize)1024)

/** @brief 1 MiB — 1,048,576 bytes */
#define CANON_MB ((usize)1024 * (usize)1024)

/** @brief 1 GiB — 1,073,741,824 bytes */
#define CANON_GB ((usize)1024 * (usize)1024 * (usize)1024)

/* ============================================================================
 * Alignment Constants
 *
 * All alignment constants are wrapped in #ifndef so callers can override
 * any of them before including this header.
 * ========================================================================= */

/**
 * @brief Default alignment for general-purpose allocations.
 *
 * Uses _Alignof(max_align_t) on C11+; falls back to 16 on C99 — a
 * conservative value that satisfies SSE, double, and pointer alignment on
 * virtually all platforms. Override to 8 for memory-constrained 32-bit
 * embedded targets if needed.
 *
 * Frama-C handling: Frama-C reports __STDC_VERSION__ >= 201112L (C11) but
 * does NOT support _Alignof. The !defined(__FRAMAC__) guard forces the
 * C99 fallback ((usize)16) under Frama-C so WP can parse memory.h /
 * arena.h / pool.h functions that capture this constant. The fallback
 * value matches the pre-C11 behavior — it is sound, not approximate.
 * Same __FRAMAC__ discipline as checked.h (CANON_CHECKED_FORCE_FALLBACK)
 * and bits.h (CANON_BITS_FORCE_FALLBACK).
 */
#ifndef CANON_DEFAULT_ALIGN
#  if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__FRAMAC__)
#    define CANON_DEFAULT_ALIGN ((usize)_Alignof(max_align_t))
#  else
#    define CANON_DEFAULT_ALIGN ((usize)16)
#  endif
#endif

/**
 * @brief CPU cache line size for false-sharing prevention.
 *
 * 64 bytes on most x86-64 and ARM64 platforms.
 * Override to 128 for Apple M-series or IBM POWER.
 *
 * Example — pad a per-thread counter to one cache line:
 * ```c
 * typedef struct { u64 value; u8 _pad[CANON_CACHE_LINE - sizeof(u64)]; } AlignedCounter;
 * ```
 */
#ifndef CANON_CACHE_LINE
#  define CANON_CACHE_LINE ((usize)64)
#endif

/**
 * @brief SSE / NEON SIMD alignment — 16 bytes.
 * Override to 32 for AVX-only builds (see CANON_SIMD_ALIGN_AVX).
 */
#ifndef CANON_SIMD_ALIGN
#  define CANON_SIMD_ALIGN ((usize)16)
#endif

/** @brief AVX (256-bit) SIMD alignment — 32 bytes. */
#ifndef CANON_SIMD_ALIGN_AVX
#  define CANON_SIMD_ALIGN_AVX ((usize)32)
#endif

/** @brief AVX-512 (512-bit) SIMD alignment — 64 bytes. */
#ifndef CANON_SIMD_ALIGN_AVX512
#  define CANON_SIMD_ALIGN_AVX512 ((usize)64)
#endif

/**
 * @brief Typical memory page size — 4,096 bytes.
 *
 * Override to 16384 for Android 15+ (API 35+) and ARM64 cloud/server systems
 * that use 16 KiB pages. Mismatching this value causes alignment faults on
 * those platforms when using page-aligned allocations.
 *
 * Example:
 * ```c
 * void *page = aligned_alloc(CANON_PAGE_SIZE, CANON_PAGE_SIZE);
 * ```
 */
#ifndef CANON_PAGE_SIZE
#  define CANON_PAGE_SIZE ((usize)4096)
#endif

/**
 * @brief Recommended alignment for lock-free _Atomic objects.
 *
 * 16 bytes covers pointers and primitive atomics on all supported platforms.
 */
#ifndef CANON_ATOMIC_ALIGN
#  define CANON_ATOMIC_ALIGN ((usize)16)
#endif

/**
 * @brief Compile-time maximum of two alignment values.
 *
 * Both arguments must be constant expressions with no side effects;
 * macro arguments are evaluated twice.
 *
 * Example:
 * ```c
 * #define MY_ALIGN CANON_ALIGN_MAX(CANON_DEFAULT_ALIGN, CANON_SIMD_ALIGN)
 * ```
 */
#define CANON_ALIGN_MAX(a, b) ((usize)((a) > (b) ? (a) : (b)))

/* ============================================================================
 * Practical Capacity Limits
 * ========================================================================= */

/**
 * @brief Soft maximum byte capacity for dynamic vectors — 1 GiB.
 *
 * Element count limit = CANON_VEC_MAX_CAPACITY / sizeof(T).
 *
 * Example:
 * ```c
 * if (new_cap > CANON_VEC_MAX_CAPACITY / sizeof(Item)) {
 *     return ERROR_CAPACITY_EXCEEDED;
 * }
 * ```
 */
#ifndef CANON_VEC_MAX_CAPACITY
#  define CANON_VEC_MAX_CAPACITY CANON_GB
#endif

/**
 * @brief Soft maximum byte length for dynamic strings — 16 MiB.
 *
 * Does not include the null terminator byte.
 */
#ifndef CANON_STRING_MAX_SIZE
#  define CANON_STRING_MAX_SIZE ((usize)16 * CANON_MB)
#endif

/**
 * @brief Soft maximum byte size for arenas — 1 GiB.
 *
 * Arenas larger than this are almost certainly a bug in the caller.
 */
#ifndef CANON_ARENA_MAX_SIZE
#  define CANON_ARENA_MAX_SIZE CANON_GB
#endif

/* ============================================================================
 * Small Buffer Optimization Thresholds
 * ========================================================================= */

/**
 * @brief Small String Optimization (SSO) inline capacity — 23 bytes.
 *
 * Strings up to this many bytes (excluding null terminator) can be stored
 * inline in a 24-byte struct: 23 bytes of data + 1 byte of length/tag.
 */
#ifndef CANON_SSO_THRESHOLD
#  define CANON_SSO_THRESHOLD ((usize)23)
#endif

/**
 * @brief Small Vector Optimization (SVO) inline element count — 8 elements.
 *
 * Vectors with at most this many elements can use inline stack storage,
 * avoiding a heap allocation for small collections.
 */
#ifndef CANON_SVO_THRESHOLD
#  define CANON_SVO_THRESHOLD ((usize)8)
#endif

/* ============================================================================
 * Growth Factor Constants
 * ========================================================================= */

/**
 * @brief Numerator of the default 1.5× capacity growth factor.
 *
 * New capacity = old_capacity * CANON_GROWTH_FACTOR_NUM
 *                             / CANON_GROWTH_FACTOR_DENOM
 *
 * 1.5× is more memory-efficient than 2× while still achieving amortised O(1)
 * push. Override both NUM and DENOM together to change the ratio.
 *
 * Example:
 * ```c
 * usize new_cap = old_cap * CANON_GROWTH_FACTOR_NUM / CANON_GROWTH_FACTOR_DENOM;
 * ```
 */
#ifndef CANON_GROWTH_FACTOR_NUM
#  define CANON_GROWTH_FACTOR_NUM   ((usize)3)
#endif

/** @brief Denominator of the default 1.5× growth factor. */
#ifndef CANON_GROWTH_FACTOR_DENOM
#  define CANON_GROWTH_FACTOR_DENOM ((usize)2)
#endif

/**
 * @brief Minimum initial allocation for dynamic containers — 32 bytes.
 *
 * Avoids a flood of tiny reallocs when pushing into a freshly created
 * container. Holds 4–8 pointers or 8–32 small elements.
 */
#ifndef CANON_MIN_ALLOCATION
#  define CANON_MIN_ALLOCATION ((usize)32)
#endif

/* ============================================================================
 * Pointer Tagging Constants
 * ========================================================================= */

/**
 * @brief Number of low-order bits available for pointer tagging.
 *
 * Aligned pointers have low bits that are always zero and can carry metadata.
 * This value reflects minimum pointer alignment on each platform width.
 *
 * - 64-bit: 8-byte alignment → 3 tag bits
 * - 32-bit: 4-byte alignment → 2 tag bits
 * - other:  conservative 1 tag bit
 */
#if UINTPTR_MAX == UINT64_MAX
#  define CANON_PTR_TAG_BITS 3
#elif UINTPTR_MAX == UINT32_MAX
#  define CANON_PTR_TAG_BITS 2
#else
#  define CANON_PTR_TAG_BITS 1
#endif

/**
 * @brief Mask for extracting the tag stored in the low bits of a pointer.
 *
 * Always typed as uintptr_t to match pointer arithmetic.
 *
 * Example:
 * ```c
 * usize      tag     = (uintptr_t)ptr & CANON_PTR_TAG_MASK;
 * void      *untagged = (void*)((uintptr_t)ptr & CANON_PTR_ADDR_MASK);
 * uintptr_t  tagged   = ((uintptr_t)untagged & CANON_PTR_ADDR_MASK) | tag;
 * ```
 */
#define CANON_PTR_TAG_MASK  ((uintptr_t)((uintptr_t)1 << CANON_PTR_TAG_BITS) - (uintptr_t)1)

/** @brief Mask for stripping tag bits and recovering the original address. */
#define CANON_PTR_ADDR_MASK ((uintptr_t)(~CANON_PTR_TAG_MASK))

/* ============================================================================
 * Platform Read-Only Information
 * ========================================================================= */

/** @brief Size of a pointer in bytes — 4 on 32-bit, 8 on 64-bit. */
#define CANON_POINTER_SIZE  ((usize)sizeof(void*))

/** @brief Number of bits in a byte — always 8 on supported platforms. */
#define CANON_BITS_PER_BYTE ((usize)8)

/** @brief Number of bits in a pointer — 32 or 64. */
#define CANON_POINTER_BITS  (CANON_POINTER_SIZE * CANON_BITS_PER_BYTE)

#endif /* CANON_CORE_PRIMITIVES_LIMITS_H */
