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

/* vmacros/vdrivers/vec_fmt_verify.h                               [VERIFY-042]
 * ============================================================================
 * WP driver for data/vec/vec_fmt.h at int, read by frama-c-vec-fmt under -CC.
 *
 * vec_fmt.h's two functions are contracted IN PLACE, in the macro bodies,
 * with their loops' annotations; under -C those would be stripped
 * (VERIFY-040 block 1), so the unit is preprocessed with -CC, as
 * frama-c-vec-range is (VERIFY-041).
 *
 * The functions call no vec function, so only vec's structs are
 * instantiated. The closure is stringbuf.h's (arena.h's closure, borrow.h,
 * the vsnprintf axiom) and result at (Bool, Error), which vec_impl.h emits
 * through CANON_RESULT(bool, Error) since this driver does not define
 * CANON_RESULT_BOOL_ERROR_DEFINED (VERIFY-042, "The unit").
 * ============================================================================ */

#ifndef CANON_VDRIVER_VEC_FMT_VERIFY_H
#define CANON_VDRIVER_VEC_FMT_VERIFY_H

#include "data/vec/vec_defn.h"

DEFINE_VEC_STRUCTS(int)

#include "data/vec/vec_fmt.h"

DEFINE_VEC_FMT(static inline, int)

#endif /* CANON_VDRIVER_VEC_FMT_VERIFY_H */
