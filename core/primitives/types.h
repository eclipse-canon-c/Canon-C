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
 * @file types.h
 * @brief Portable integer and size type aliases, and their limits, for Canon-C
 *
 * Short, explicit type names over verbose stdint.h identifiers.
 * Every other module depends on this header.
 *
 * Design:
 *   - Explicit: signedness and width are visible at every use site
 *   - Minimal: one name per semantic role, no redundant aliases
 *   - Zero cost: pure compile-time aliases, identical codegen to stdint.h
 *   - Portable: guaranteed sizes via the C99 exact-width types, with the
 *     platform contract below enforced at compile time — unsupported
 *     targets fail loudly with a named error, never silently
 *
 * Requires C99 or later.
 *
 * Platform contract (Tier 0):
 * ────────────────────────────────────────────────────────────────────────────
 * Canon-C requires:
 *   - 8-bit bytes (CHAR_BIT == 8)
 *   - The C99 exact-width integer types uint8_t .. uint64_t and
 *     int8_t .. int64_t
 *
 * These are optional in strict C99 (ISO C99 7.18.1.1) but present on every
 * supported platform — Windows, Linux, macOS, 32/64-bit ARM, RISC-V, and
 * every mainstream 8/16-bit MCU toolchain (avr-gcc, msp430-gcc, SDCC).
 * Targets that fail these checks (e.g. DSPs with CHAR_BIT == 16 such as
 * TI C2000) cannot host Canon-C's byte-view semantics and are out of
 * scope by design. The #error block below converts that boundary from a
 * silent assumption into a compile-time contract.
 *
 * This header is Tier 0 of Canon-C's platform tiers. Headers that
 * introduce further platform requirements guard them at their own origin:
 * core/primitives/sizes.h enforces Tier 1 (size_t >= 32 bits) beside the
 * size literals that need it. See the README's bare-metal section for the
 * tier model.
 *
 * NOT suitable for:
 *   - Bit-fields          — use 'unsigned int' per C standard
 *   - Atomic operations   — use C11 _Atomic types directly
 *   - printf formatters   — use PRIu32, PRId64 from <inttypes.h>
 *
 * @sa <inttypes.h> for printf/scanf format macros (PRIu32, PRId64, etc.)
 * @sa checked.h, bits.h
 * @sa sizes.h — Tier 1 platform contract (size literals, capacity limits)
 */
#ifndef CANON_CORE_PRIMITIVES_TYPES_H
#define CANON_CORE_PRIMITIVES_TYPES_H
#include <stddef.h>   /* size_t, ptrdiff_t */
#include <stdint.h>   /* uint8_t … int64_t  */
#include <stdbool.h>  /* bool, true, false  */
/* NOTE: <limits.h> is not included here. Until VERIFY-140 the project's
 * own core/primitives/limits.h shadowed the system header under the
 * build's -I core/primitives flag, and CHAR_BIT was never defined; that
 * header's limits are this one's now (below), the rest sizes.h's and
 * ptr.h's. The 8-bit-byte requirement is enforced through the exact-
 * width type check below, which implies it — see the platform contract
 * comment — and Canon-C's limits are <stdint.h>'s, which suffice. */

/* ── Platform contract (Tier 0) ──────────────────────────────────────────────
 * Enforced here, once, at the bottom of the dependency rule. Every other
 * Canon-C header includes types.h (directly or transitively) and inherits
 * this contract — no header repeats these checks, and no header may rely
 * on a platform property these checks do not establish without guarding
 * it at its own origin.
 *
 * Per ISO C99 7.18.2.1, the UINTn_MAX / INTn_MAX macros are defined if
 * and only if the corresponding exact-width types exist, so testing the
 * macros is the standard-blessed detection for the types themselves.
 *
 * The 8-bit-byte requirement (CHAR_BIT == 8) is enforced by the same
 * check, by implication rather than by reading CHAR_BIT directly:
 * per C99 7.18.1.1, uint8_t has exactly 8 bits and no padding, and the
 * smallest addressable object is one byte of CHAR_BIT bits — so
 * 8 == CHAR_BIT * sizeof(uint8_t) with CHAR_BIT >= 8 forces
 * CHAR_BIT == 8 and sizeof(uint8_t) == 1. A target where uint8_t exists
 * is a target with 8-bit bytes; a target with non-8-bit bytes (e.g. a
 * TI C2000-class DSP with CHAR_BIT == 16) cannot define UINT8_MAX and
 * fails this check. CHAR_BIT itself is not consulted: until VERIFY-140
 * the system <limits.h> was shadowed by the project's own limits.h under
 * the build's include flags — see the include-section note above.
 * ──────────────────────────────────────────────────────────────────────── */
#if !defined(UINT8_MAX)  || !defined(UINT16_MAX) || \
    !defined(UINT32_MAX) || !defined(UINT64_MAX) || \
    !defined(INT8_MAX)   || !defined(INT16_MAX)  || \
    !defined(INT32_MAX)  || !defined(INT64_MAX)
#  error "Canon-C platform contract (Tier 0): the C99 exact-width integer \
types (uint8_t .. uint64_t, int8_t .. int64_t) are required. Their \
existence also implies 8-bit bytes (C99 7.18.1.1: uint8_t is exactly 8 \
bits, no padding, so CHAR_BIT == 8). Targets without them (e.g. \
TI C2000-class DSPs with 16-bit bytes) cannot host Canon-C's byte-view \
semantics and are out of scope by design."
#endif

/* ── Unsigned integers ───────────────────────────────────────────────────── */
typedef uint8_t   u8;    /*         0 .. 255                          */
typedef uint16_t  u16;   /*         0 .. 65 535                       */
typedef uint32_t  u32;   /*         0 .. 4 294 967 295                */
typedef uint64_t  u64;   /*         0 .. 18 446 744 073 709 551 615   */
/* ── Signed integers ─────────────────────────────────────────────────────── */
typedef int8_t    i8;    /*      -128 .. 127                          */
typedef int16_t   i16;   /*   -32 768 .. 32 767                       */
typedef int32_t   i32;   /*    -2^31 .. 2^31 - 1                      */
typedef int64_t   i64;   /*    -2^63 .. 2^63 - 1                      */
/* ── Platform-width types ────────────────────────────────────────────────── */
typedef size_t    usize;  /* Unsigned: array index, object size       */
typedef ptrdiff_t isize;  /* Signed:   pointer difference, offset     */
/* ── Floating point ──────────────────────────────────────────────────────── */
typedef float     f32;   /* 32-bit floating point (IEEE 754 on all supported platforms) */
typedef double    f64;   /* 64-bit floating point (IEEE 754 on all supported platforms) */

/* ── Floating-point width pins ───────────────────────────────────────────────
 * C99 does not guarantee that float/double are IEEE 754 (__STDC_IEC_559__
 * signals it but is unreliably defined across toolchains). Every supported
 * platform provides IEEE binary32/binary64; these compile-time pins catch
 * the realistic failure mode (a toolchain with non-4/8-byte float/double)
 * without claiming to verify full IEEE semantics. The negative-size-array
 * pattern is used instead of static_require because contract.h includes
 * this header — using it here would form an include cycle.
 *
 * That failure mode is real: AVR's GCC makes double 4 bytes by default
 * (non-conforming: C requires DBL_DIG >= 10), and the f64 pin refuses it.
 * Build for AVR with -mdouble=64 (GCC 10 and later). CI checks the
 * refusal with avr-gcc 7 (VERIFY-048, tools/avr_tier_check.sh).
 * ──────────────────────────────────────────────────────────────────────── */
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
typedef char canon_types_assert_f32_is_4_bytes_[(sizeof(f32) == 4) ? 1 : -1];
/* cppcheck-suppress misra-c2012-2.3 ; MISRA-DEV-017 */
typedef char canon_types_assert_f64_is_8_bytes_[(sizeof(f64) == 8) ? 1 : -1];

/* ── Boolean ─────────────────────────────────────────────────────────────── */
/* bool / true / false provided by <stdbool.h> above. No aliases needed. */

/* ── Integer type limits (Tier 0) ────────────────────────────────────────────
 * The limits of the aliases above, from <stdint.h> (C99 7.18.2, 7.18.3),
 * which is never shadowed: width-correct on any target this header
 * admits. Until VERIFY-140 these, and the platform information below,
 * were core/primitives/limits.h's; VERIFY-140 split that header into
 * this one, sizes.h and ptr.h, and removed it.
 * ──────────────────────────────────────────────────────────────────────── */
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

/* ── Size type limits (Tier 0) ───────────────────────────────────────────────
 * These track the platform's own size_t and ptrdiff_t at any width.
 * ──────────────────────────────────────────────────────────────────────── */
/**
 * @brief Max value of usize — SIZE_MAX: at least 2^16-1 (C99 7.18.3);
 *        2^32-1 or 2^64-1 at Tier 1
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

/* ── Platform information (Tier 0) ───────────────────────────────────────── */

/** @brief Size of a pointer in bytes — sizeof(void *): 4 at 32 bits, 8 at
 *         64; 2 or 4 at 16. */
#define CANON_POINTER_SIZE  ((usize)sizeof(void*))

/** @brief Number of bits in a byte — always 8, per the Tier 0 contract above. */
#define CANON_BITS_PER_BYTE ((usize)8)

/** @brief Number of bits of a pointer's storage — CANON_POINTER_SIZE times 8. */
#define CANON_POINTER_BITS  (CANON_POINTER_SIZE * CANON_BITS_PER_BYTE)
#endif /* CANON_CORE_PRIMITIVES_TYPES_H */
