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
/* vmacros/vdrivers/hashmap_verify.h                              [VERIFY-055]
 *
 * WP driver for data/hashmap/, header-only, u64 -> int. hashmap's contracts
 * are in hashmap_impl.h's functions, not in macro bodies, but the option
 * and result types it instantiates carry theirs in macro bodies, so the unit
 * is preprocessed with -CC, as the other data/ units. The hash and equality
 * functions are the quick start's, with contracts: reading the key(s),
 * nothing written, a hash that is not 0, equality as ==. A caller's own
 * functions must meet hashmap's requirements on them in the same way; the
 * unit proves hashmap against these.
 */
#ifndef CANON_VDRIVER_HASHMAP_VERIFY_H
#define CANON_VDRIVER_HASHMAP_VERIFY_H

#include "core/primitives/types.h"

/*@ requires \valid_read(k);
    assigns \nothing;
    ensures \result >= 1; */
static u64 hash_u64(const u64* k, void* ctx) {
    (void)ctx;
    u64 h = *k * 11400714819323198485ULL;
    return h ? h : 1u;
}

/*@ requires \valid_read(a) && \valid_read(b);
    assigns \nothing;
    ensures \result <==> *a == *b; */
static bool eq_u64(const u64* a, const u64* b, void* ctx) {
    (void)ctx;
    return *a == *b;
}

#define HASHMAP_KEY_TYPE u64
#define HASHMAP_VAL_TYPE int
#define HASHMAP_HASH_FN  hash_u64
#define HASHMAP_EQ_FN    eq_u64
#include "data/hashmap/hashmap.h"

#endif /* CANON_VDRIVER_HASHMAP_VERIFY_H */
