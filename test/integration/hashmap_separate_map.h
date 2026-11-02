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

/* The shared header of hashmap_separate_test (VERIFY-054): the recipe of
 * hashmap_decl.h, as corrected. Declarations only; the definitions are
 * generated in hashmap_separate_defs.c, which does not include this file. */
#ifndef CANON_TEST_HASHMAP_SEPARATE_MAP_H
#define CANON_TEST_HASHMAP_SEPARATE_MAP_H

#include "core/primitives/types.h"

u64  sep_hash_u64(const u64* k, void* ctx);
bool sep_eq_u64(const u64* a, const u64* b, void* ctx);

#define HASHMAP_KEY_TYPE  u64
#define HASHMAP_VAL_TYPE  int
#define HASHMAP_HASH_FN   sep_hash_u64
#define HASHMAP_EQ_FN     sep_eq_u64
#define HASHMAP_TYPE_NAME map_u64_int
#define HASHMAP_FN(name)  map_u64_int_##name
#include "data/hashmap/hashmap_decl.h"

#endif /* CANON_TEST_HASHMAP_SEPARATE_MAP_H */
