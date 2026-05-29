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
 *   - Portable: guaranteed sizes via C99 stdint.h (universally available)
 *
 * Requires C99 or later.
 *
 * NOT suitable for:
 *   - Bit-fields          — use 'unsigned int' per C standard
 *   - Atomic operations   — use C11 _Atomic types directly
 *   - printf formatters   — use PRIu32, PRId64 from <inttypes.h>
 *
 * @sa <inttypes.h> for printf/scanf format macros (PRIu32, PRId64, etc.)
 * @sa checked.h, bits.h
 */

#ifndef CANON_CORE_PRIMITIVES_TYPES_H
#define CANON_CORE_PRIMITIVES_TYPES_H

#include <stddef.h>   /* size_t, ptrdiff_t */
#include <stdint.h>   /* uint8_t … int64_t  */
#include <stdbool.h>  /* bool, true, false  */

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

typedef float     f32;   /* 32-bit IEEE 754 */
typedef double    f64;   /* 64-bit IEEE 754 */

/* ── Boolean ─────────────────────────────────────────────────────────────── */

/* bool / true / false provided by <stdbool.h> above. No aliases needed. */

#endif /* CANON_CORE_PRIMITIVES_TYPES_H */
