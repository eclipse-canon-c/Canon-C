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

/* The definitions for hashmap_separate_test (VERIFY-054). It configures the
 * same map as hashmap_separate_map.h and does not include that header: the
 * definitions file generates the types as well, and C99 forbids a repeated
 * typedef, so including both in one file does not compile (H5). */
#include "core/primitives/types.h"

u64 sep_hash_u64(const u64* k, void* ctx)
{
    (void)ctx;
    u64 h = *k * 11400714819323198485ULL;
    return h ? h : 1u;
}

bool sep_eq_u64(const u64* a, const u64* b, void* ctx)
{
    (void)ctx;
    return *a == *b;
}

#define HASHMAP_KEY_TYPE  u64
#define HASHMAP_VAL_TYPE  int
#define HASHMAP_HASH_FN   sep_hash_u64
#define HASHMAP_EQ_FN     sep_eq_u64
#define HASHMAP_TYPE_NAME map_u64_int
#define HASHMAP_FN(name)  map_u64_int_##name
#include "data/hashmap/hashmap_defn.h"
