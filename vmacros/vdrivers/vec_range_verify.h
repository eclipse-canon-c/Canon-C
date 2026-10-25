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

/* vmacros/vdrivers/vec_range_verify.h                             [VERIFY-041]
 * ============================================================================
 * WP driver for data/vec/vec_range.h at int, read by frama-c-vec-range under
 * -CC.
 *
 * vec_range.h's one function is contracted IN PLACE, in the macro body of
 * IMPL_VEC_EXTEND_FROM_RANGE, with its loop's annotation. Those annotations
 * are comments inside a macro, so they survive preprocessing under -CC only
 * (VERIFY-025); under -C, the main CI's default, they are stripped
 * (VERIFY-040 block 1). This unit is therefore preprocessed with -CC.
 *
 * The function calls no vec function, so vec's functions are not
 * instantiated: the unit is vec_cc_experiment.h's prefix, without option at
 * int, plus the extension. Its closure is arena.h's, result at (Bool, Error),
 * range.h with option at isize, and vec's structs at int (VERIFY-041, "The
 * unit").
 *
 * One clause must name the element type, and a macro-body comment cannot
 * (no parameter substitution inside comments). It is stated here, at int, on
 * a prototype; Frama-C merges it with the definition's contract ("found two
 * contracts ... Merging them").
 * ============================================================================ */

#ifndef CANON_VDRIVER_VEC_RANGE_VERIFY_H
#define CANON_VDRIVER_VEC_RANGE_VERIFY_H

#include "semantics/error.h"
#include "semantics/result/result_defn.h"

DEFINE_RESULT_TYPEDEF(bool, Error)
DEFINE_RESULT_STRUCT(bool, Error)
DEFINE_RESULT_FUNCTIONS(static inline, bool, Error)
#define CANON_RESULT_BOOL_ERROR_DEFINED

#include "data/vec/vec_defn.h"

DEFINE_VEC_STRUCTS(int)

#include "data/vec/vec_range.h"

/* When the vector has room and every value fits int, the call succeeds.
 * The macro's `room` behaviour says what success and failure mean; this
 * clause says when success is owed (VERIFY-041, design note 1). */
/*@ ensures fits_ok:
      (v != \null && v->items != \null && r.step != CANON_ISIZE_MIN
       && \old(v->len) + range_count(r) <= \old(v->capacity)
       && \forall integer k; 0 <= k < range_count(r)
            ==> (int)(r.current + k * r.step) == r.current + k * r.step)
      ==> \result.is_ok;
*/
static inline result__Bool_Error vec_int_extend_from_range(vec_int* v, range r);

DEFINE_VEC_RANGE(static inline, int)

#endif /* CANON_VDRIVER_VEC_RANGE_VERIFY_H */
