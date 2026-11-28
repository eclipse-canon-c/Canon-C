/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

/**
 * @file sizes.h
 * @brief Size literals, alignment, capacity and growth constants for Canon-C
 *
 * The constants that the allocators and the collections size themselves
 * by: the size literals, the alignments, the capacity limits, the small
 * buffer thresholds and the growth factor. Centralizes magic numbers to
 * improve readability and maintainability.
 *
 * Until VERIFY-140 these were core/primitives/limits.h's, with the integer
 * type limits, the platform information and the pointer-tagging constants.
 * The limits and the platform information are types.h's now (Tier 0), and
 * the tagging constants ptr.h's. limits.h, whose name under the build's
 * -I core/primitives was the C library's <limits.h>, is gone.
 *
 * Design:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit: named constants replace magic numbers
 * - Overridable: define any CANON_* tuning constant before including to
 *   override (tuning constants only — the tier contract is not
 *   overridable; see "Platform tier")
 * - Zero overhead: all constants are compile-time, no code generated
 *
 * Platform tier:
 * ────────────────────────────────────────────────────────────────────────────
 * This header is Tier 1: it requires size_t >= 32 bits. On a 16-bit size_t
 * target, CANON_MB and CANON_GB would wrap to 0 under well-defined unsigned
 * arithmetic, silently zeroing every capacity constant derived from them
 * (CANON_ARENA_MAX_SIZE, CANON_VEC_MAX_CAPACITY, ...) and rendering the
 * allocator and collection layers unusable with no diagnostic. The #error
 * guard below converts that silent breakage into a named compile-time
 * refusal. The guard is deliberately not overridable — an escape hatch
 * into silently-zero constants is a footgun, not a configuration.
 *
 * 16-bit size_t targets retain use of the Tier-0 subset: types.h, bits.h,
 * compare.h, scope.h, and contract.h. (checked.h is conceptually Tier 0,
 * its limits being types.h's, but it still includes this header and so
 * rides Tier 1 until it is measured at 16 bits.) See the README's
 * bare-metal section for the tier model.
 *
 * Override pattern (define BEFORE including this header):
 * ────────────────────────────────────────────────────────────────────────────
 *   #define CANON_PAGE_SIZE          16384   // Android 15+, some ARM64 servers
 *   #define CANON_CACHE_LINE         128     // Apple M-series L2+
 *   #define CANON_VEC_MAX_CAPACITY   (CANON_GB * 4)
 *   #include "core/primitives/sizes.h"
 *
 * @sa types.h — Tier 0 platform contract (8-bit bytes, exact-width types)
 *     and the integer type limits (CANON_U8_MAX .. CANON_ISIZE_MIN)
 */

#ifndef CANON_CORE_PRIMITIVES_SIZES_H
#define CANON_CORE_PRIMITIVES_SIZES_H

#include "types.h"
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * Platform contract (Tier 1) — size_t >= 32 bits
 * ============================================================================
 *
 * Everything below this guard assumes a size_t of at least 32 bits. On a
 * 16-bit size_t target the size literals would wrap to 0 (well-defined
 * unsigned arithmetic — no warning, no UB, just wrong values), which would
 * propagate zeros into every capacity constant and make arena_init,
 * vector construction, and string building fail at runtime with no
 * indication of why. This guard makes the boundary loud and names it.
 *
 * The requirement is enforced here — beside the constants that introduce
 * it — rather than in types.h, so that types.h, with the integer type
 * limits, and the freestanding primitives remain fully usable on 16-bit
 * targets.
 *
 * This contract is NOT overridable. If a concrete 16-bit target ever
 * needs the capacity-managed layers, that is a recorded design decision
 * with its own width-conditional constants and validation story, not a
 * macro escape hatch. See the README's bare-metal section.
 *
 * The defined(SIZE_MAX) gate exists for static analyzers, not compilers.
 * Tools like Cppcheck do not expand system headers, so SIZE_MAX is
 * undefined in their view and a bare `#if SIZE_MAX < 0xFFFFFFFF` evaluates
 * the unknown identifier as 0 — firing this #error in every configuration
 * the tool constructs and failing the analysis run on healthy 64-bit
 * hosts. Gating on defined() gives such tools a valid error-free
 * configuration (the same mechanism that lets them past types.h's
 * defined()-based Tier 0 check). Real-compiler enforcement is unchanged:
 * C99 7.18.3 makes SIZE_MAX MANDATORY in <stdint.h> — unlike the optional
 * exact-width types — so every conforming toolchain, including every
 * 16-bit one, defines it and the guard still fires exactly where it must.
 *
 * The message below is limits.h's, byte for byte (VERIFY-140): the tier
 * checks find a refusal by its first words, and cppcheck's rule 4.1 reads
 * its line splices by their offsets (MISRA-TOOL-001).
 * ========================================================================= */
#if defined(SIZE_MAX) && (SIZE_MAX < 0xFFFFFFFF)
#  error "Canon-C platform contract (Tier 1): the size literals and \
capacity constants below (and every header that uses them: arena, pool, \
slice, collections) require size_t >= 32 bits. On this target, use the \
Tier 0 subset only: types.h, bits.h, compare.h, scope.h, contract.h \
(exact-width limit macros come from <stdint.h>). See the README's \
bare-metal section."
#endif

/* ============================================================================
 * Size Literals
 * ========================================================================= */

/**
 * @brief 1 KiB — 1,024 bytes
 *
 * All size literals use usize casts on every factor to avoid signed integer
 * overflow on 32-bit platforms where plain int arithmetic would overflow before
 * the outer cast takes effect. (On sub-32-bit size_t the casts would prevent
 * the UB but still produce wrapped values — which is why the Tier 1 guard
 * above refuses such targets outright.)
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
 *
 * Note: this constant is load-bearing for verification evidence — the
 * MCDC-003 unreachability argument for arena.h's overflow-guard
 * subconditions reasons from CANON_ARENA_MAX_SIZE's value. Overriding it
 * to a value near CANON_USIZE_MAX makes those guards reachable; see
 * MCDC-003 in docs/deviations.md before raising it.
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

#endif /* CANON_CORE_PRIMITIVES_SIZES_H */
