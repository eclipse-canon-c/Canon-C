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
 * @file hashmap_separate_test.c
 * @brief hashmap's separate-compilation path, which no test built before
 *        VERIFY-054 (H5)
 *
 * The map is declared in hashmap_separate_map.h (hashmap_decl.h) and defined
 * in hashmap_separate_defs.c (hashmap_defn.h). This file uses it through the
 * declarations alone, the option and result types among them.
 */
#define CANON_CONTRACT_IMPL

#include "hashmap_separate_map.h"

#include <stdio.h>

static int g_failed = 0;
#define EXPECT(cond)                                                  \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__,  \
                    #cond);                                           \
            g_failed++;                                               \
        }                                                             \
    } while (0)

int main(void)
{
    map_u64_int_slot buf[8];
    map_u64_int m;
    if (!result__Bool_Error_is_ok(map_u64_int_init(&m, bytes_from(buf, sizeof buf), 8, NULL))) {
        EXPECT(false);   /* init failed */
        return 1;
    }
    for (u64 k = 1; k <= 4; k++) {
        int v = (int)k * 2;
        EXPECT(result__Bool_Error_is_ok(map_u64_int_insert(&m, &k, &v)));
    }
    u64 k3 = 3;
    option_map_u64_int_val_t o = map_u64_int_get(&m, &k3);
    EXPECT(option_map_u64_int_val_t_is_some(o) && option_map_u64_int_val_t_unwrap(o) == 6);
    result_map_u64_int_val_t_Error r = map_u64_int_remove(&m, &k3);
    EXPECT(result_map_u64_int_val_t_Error_is_ok(r) && result_map_u64_int_val_t_Error_unwrap(r) == 6);
    EXPECT(map_u64_int_len(&m) == 3 && !map_u64_int_contains_key(&m, &k3));
    /* the rest of the declared API, through the declarations */
    EXPECT(result_map_u64_int_val_t_Error_is_err(map_u64_int_remove(&m, &k3)));
    EXPECT(map_u64_int_capacity(&m) == 8 && !map_u64_int_is_empty(&m));
    EXPECT(map_u64_int_load_factor(&m) > 0.0);
    u64 k1 = 1;
    int* p = map_u64_int_get_or_null(&m, &k1);
    EXPECT(p != NULL && *p == 2);
    usize it = 0, seen = 0;
    const u64* kp; const int* vp;
    while (map_u64_int_iter_next(&m, &it, &kp, &vp)) { seen++; }
    EXPECT(seen == 3);
    EXPECT(map_u64_int_buffer_size(8) == sizeof buf);
    map_u64_int_clear(&m);
    EXPECT(map_u64_int_is_empty(&m));
    if (g_failed == 0) {
        printf("OK  hashmap_separate_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  hashmap_separate_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}
