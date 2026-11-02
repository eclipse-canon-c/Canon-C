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
 * @file hashmap_multi_test.c
 * @brief The hashmap paths that no test built before VERIFY-054
 *
 *   - two maps in one translation unit, configured as hashmap.h's
 *     "Multiple instantiations" section says (H3);
 *   - the per-map option and result names (H4);
 *   - hashmap_range.h included once per map (H6).
 *
 * hashmap_test.c tests the rest of VERIFY-054 with one map: range and fmt,
 * the alignment refusal and the load cap. This test has two instantiations
 * of every function, so the coverage job leaves it out: the MC/DC table
 * keeps, per file, the translation unit with the most outcomes, and this
 * one would displace hashmap_test.c's row for hashmap_impl.h.
 */

#define CANON_CONTRACT_IMPL

#include "core/primitives/types.h"
#include "semantics/option/option.h"
#include "data/vec/vec.h"

#include <stdio.h>
#include <string.h>


static u64 hash_u64(const u64* k, void* ctx)
{
    (void)ctx;
    u64 h = *k * 11400714819323198485ULL;
    return h ? h : 1u;
}
static bool eq_u64(const u64* a, const u64* b, void* ctx) { (void)ctx; return *a == *b; }

static u64 hash_u32(const u32* k, void* ctx)
{
    (void)ctx;
    u64 h = (u64)*k * 11400714819323198485ULL;
    return h ? h : 1u;
}
static bool eq_u32(const u32* a, const u32* b, void* ctx) { (void)ctx; return *a == *b; }

/* The vec types the range extension pushes into */
#include "test/support/hashmap_vecs.h"

/* First map: u64 -> int, the default names */
#define HASHMAP_KEY_TYPE u64
#define HASHMAP_VAL_TYPE int
#define HASHMAP_HASH_FN  hash_u64
#define HASHMAP_EQ_FN    eq_u64
#include "data/hashmap/hashmap.h"
#include "data/hashmap/hashmap_range.h"
#undef HASHMAP_KEY_TYPE
#undef HASHMAP_VAL_TYPE
#undef HASHMAP_HASH_FN
#undef HASHMAP_EQ_FN
#undef HASHMAP_TYPE_NAME
#undef HASHMAP_FN
#undef HASHMAP_SLOT_NAME

/* Second map: u32 -> u64, its own names */
#define HASHMAP_KEY_TYPE  u32
#define HASHMAP_VAL_TYPE  u64
#define HASHMAP_HASH_FN   hash_u32
#define HASHMAP_EQ_FN     eq_u32
#define HASHMAP_TYPE_NAME map_u32_u64
#define HASHMAP_FN(name)  map_u32_u64_##name
#include "data/hashmap/hashmap.h"
#include "data/hashmap/hashmap_range.h"

static int g_failed = 0;
#define EXPECT(cond)                                                  \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__,  \
                    #cond);                                           \
            g_failed++;                                               \
        }                                                             \
    } while (0)

static void test_two_maps(void)
{
    hashmap_slot     b1[8];
    map_u32_u64_slot b2[8];
    hashmap     m1;
    map_u32_u64 m2;
    /* Stop if either init fails: the maps are then not initialised (the
       analyzer follows that path otherwise). */
    if (!result__Bool_Error_is_ok(hashmap_init(&m1, bytes_from(b1, sizeof b1), 8, NULL)) ||
        !result__Bool_Error_is_ok(map_u32_u64_init(&m2, bytes_from(b2, sizeof b2), 8, NULL))) {
        EXPECT(false);   /* init failed */
        return;
    }

    u64 k1 = 7; int v1 = -3;
    u32 k2 = 7; u64 v2 = 900000000000ULL;
    EXPECT(result__Bool_Error_is_ok(hashmap_insert(&m1, &k1, &v1)));
    EXPECT(result__Bool_Error_is_ok(map_u32_u64_insert(&m2, &k2, &v2)));

    /* H4: the per-map option and result names */
    option_hashmap_val_t     o1 = hashmap_get(&m1, &k1);
    option_map_u32_u64_val_t o2 = map_u32_u64_get(&m2, &k2);
    EXPECT(option_hashmap_val_t_is_some(o1) && option_hashmap_val_t_unwrap(o1) == -3);
    EXPECT(option_map_u32_u64_val_t_is_some(o2) &&
           option_map_u32_u64_val_t_unwrap(o2) == 900000000000ULL);

    result_map_u32_u64_val_t_Error r2 = map_u32_u64_remove(&m2, &k2);
    EXPECT(result_map_u32_u64_val_t_Error_is_ok(r2));
    EXPECT(map_u32_u64_len(&m2) == 0 && hashmap_len(&m1) == 1);
}

static void test_second_map_range(void)
{
    map_u32_u64_slot b[8];
    map_u32_u64 m;
    if (!result__Bool_Error_is_ok(map_u32_u64_init(&m, bytes_from(b, sizeof b), 8, NULL))) {
        EXPECT(false);   /* init failed */
        return;
    }
    for (u32 k = 1; k <= 3; k++) {
        u64 v = (u64)k + 100u;
        EXPECT(result__Bool_Error_is_ok(map_u32_u64_insert(&m, &k, &v)));
    }
    u32 kb[4];
    vec_u32 keys = vec_u32_init(kb, 4);
    EXPECT(result__Bool_Error_is_ok(map_u32_u64_collect_keys(&m, &keys)));
    EXPECT(vec_u32_len(&keys) == 3);
}

int main(void)
{
    test_two_maps();
    test_second_map_range();
    if (g_failed == 0) {
        printf("OK  hashmap_multi_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  hashmap_multi_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}
