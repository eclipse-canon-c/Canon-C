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
 * @brief Portable integer and size type aliases for Canon-C
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
 * core/primitives/limits.h enforces Tier 1 (size_t >= 32 bits) beside the
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
 * @sa limits.h — Tier 1 platform contract (size literals, capacity limits)
 */
#ifndef CANON_CORE_PRIMITIVES_TYPES_H
#define CANON_CORE_PRIMITIVES_TYPES_H
#include <stddef.h>   /* size_t, ptrdiff_t */
#include <stdint.h>   /* uint8_t … int64_t  */
#include <stdbool.h>  /* bool, true, false  */
#include <limits.h>   /* CHAR_BIT — platform contract below */

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
 * ──────────────────────────────────────────────────────────────────────── */
#if CHAR_BIT != 8
#  error "Canon-C platform contract (Tier 0): CHAR_BIT must be 8. \
Targets with non-8-bit bytes (e.g. TI C2000-class DSPs) cannot host \
Canon-C's byte-view semantics and are out of scope by design."
#endif

#if !defined(UINT8_MAX)  || !defined(UINT16_MAX) || \
    !defined(UINT32_MAX) || !defined(UINT64_MAX) || \
    !defined(INT8_MAX)   || !defined(INT16_MAX)  || \
    !defined(INT32_MAX)  || !defined(INT64_MAX)
#  error "Canon-C platform contract (Tier 0): the C99 exact-width integer \
types (uint8_t .. uint64_t, int8_t .. int64_t) are required. They are \
optional in strict C99 but provided by every supported toolchain."
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
 * ──────────────────────────────────────────────────────────────────────── */
typedef char canon_types_assert_f32_is_4_bytes_[(sizeof(f32) == 4) ? 1 : -1];
typedef char canon_types_assert_f64_is_8_bytes_[(sizeof(f64) == 8) ? 1 : -1];

/* ── Boolean ─────────────────────────────────────────────────────────────── */
/* bool / true / false provided by <stdbool.h> above. No aliases needed. */
#endif /* CANON_CORE_PRIMITIVES_TYPES_H */
