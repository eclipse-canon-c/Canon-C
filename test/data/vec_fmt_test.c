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

/* test/data/vec_fmt_test.c — tests for data/vec/vec_fmt.h
 *
 * vec_fmt.h had no tests until 63f6ae3, written before its verification
 * and testing what its documentation stated and the code did with room in
 * the StringBuf. Since VERIFY-042's run 1 both functions return bool and
 * the callback returns bool; the tests below assert the results too:
 *   - to_stringbuf: each element through stringbuf_printf(sb, fmt, element),
 *     in order, appended after what the StringBuf already holds;
 *   - to_stringbuf_cb: each element passed to cb(sb, element), in order;
 *   - both: nothing happens for a NULL vector, StringBuf, format or
 *     callback, or for an empty vector;
 *   - both at a struct element type, through the callback.
 *   - VERIFY-042 F1: when the StringBuf fills up, both stop at the first
 *     element that does not fit, keep what was written and return false
 *     (before, the element was skipped and later ones appended);
 *   - VERIFY-042 F3: a callback that adds to the vector it is formatting
 *     sees only the elements present at the call.
 */

#define CANON_CONTRACT_IMPL
#include "semantics/error.h"
#include "semantics/result/result.h"
#include "data/vec/vec_defn.h"
#include "data/vec/vec_fmt.h"

#include <stdio.h>
#include <string.h>

/* vec_fmt.h calls no vec function, so only the structs are instantiated
 * and the vectors are built by their fields, as vec_range_cover.c does:
 * DEFINE_VEC would emit some sixty functions this test does not call. */
DEFINE_VEC_STRUCTS(int)
DEFINE_VEC_FMT(static inline, int)

typedef struct { int x; int y; } Pt;
DEFINE_VEC_STRUCTS(Pt)
DEFINE_VEC_FMT(static inline, Pt)

static int g_failed = 0;

#define EXPECT(cond)                                                  \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n",                      \
                    __FILE__, __LINE__, #cond);                       \
            g_failed++;                                               \
        }                                                             \
    } while (0)

/* The StringBuf holds exactly `want`, NUL-terminated. */
static int sb_is(const StringBuf* sb, const char* want)
{
    return stringbuf_len(sb) == (usize)strlen(want)
        && strcmp(stringbuf_str(sb), want) == 0;
}

static int g_calls = 0;

static bool fmt_bracket(StringBuf* sb, int x)
{
    g_calls++;
    return stringbuf_printf(sb, "[%d]", x);
}

static bool fmt_pt(StringBuf* sb, Pt p)
{
    g_calls++;
    return stringbuf_printf(sb, "(%d,%d)", p.x, p.y);
}

/* returns false at the second element it sees */
static bool fail_second(StringBuf* sb, int x)
{
    g_calls++;
    if (g_calls == 2) { return false; }
    return stringbuf_printf(sb, "[%d]", x);
}

static vec_int* g_grow;

/* formats x, then appends it to the vector being formatted (F3) */
static bool fmt_and_grow(StringBuf* sb, int x)
{
    g_calls++;
    if (g_grow->len < g_grow->capacity) { g_grow->items[g_grow->len++] = x; }
    return stringbuf_printf(sb, "%d,", x);
}

static vec_int make_123(int* buf)
{
    vec_int v = { .items = buf, .len = 3, .capacity = 3 };
    buf[0] = 1; buf[1] = 2; buf[2] = 3;
    return v;
}

static void test_to_stringbuf_basic(void)
{
    int buf[3];
    vec_int v = make_123(buf);
    char out[64];
    StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    EXPECT(vec_int_to_stringbuf(&v, &sb, "%d "));
    EXPECT(sb_is(&sb, "1 2 3 "));
}

static void test_to_stringbuf_appends_after_existing(void)
{
    int buf[3];
    vec_int v = make_123(buf);
    char out[64];
    StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    EXPECT(stringbuf_append(&sb, "v="));
    EXPECT(vec_int_to_stringbuf(&v, &sb, "<%d>"));
    EXPECT(sb_is(&sb, "v=<1><2><3>"));
}

static void test_to_stringbuf_null_and_empty(void)
{
    int buf[3];
    vec_int v = make_123(buf);
    vec_int e = { .items = buf, .len = 0, .capacity = 3 };
    char out[64];
    StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    EXPECT(stringbuf_append(&sb, "x"));
    EXPECT(!vec_int_to_stringbuf(NULL, &sb, "%d"));
    EXPECT(!vec_int_to_stringbuf(&v, NULL, "%d"));
    EXPECT(!vec_int_to_stringbuf(&v, &sb, NULL));
    EXPECT(vec_int_to_stringbuf(&e, &sb, "%d"));        /* empty: true */
    EXPECT(sb_is(&sb, "x"));
    {   /* a StringBuf in the zero state: false, nothing written */
        StringBuf z = { 0 };
        EXPECT(!vec_int_to_stringbuf(&v, &z, "%d"));
        EXPECT(z.len == 0u && z.data == NULL);
        EXPECT(vec_int_to_stringbuf(&e, &z, "%d"));     /* empty: true */
    }
}

static void test_to_stringbuf_cb_basic(void)
{
    int buf[3];
    vec_int v = make_123(buf);
    char out[64];
    StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    g_calls = 0;
    EXPECT(vec_int_to_stringbuf_cb(&v, &sb, fmt_bracket));
    EXPECT(g_calls == 3);
    EXPECT(sb_is(&sb, "[1][2][3]"));
}

static void test_to_stringbuf_cb_null_and_empty(void)
{
    int buf[3];
    vec_int v = make_123(buf);
    vec_int e = { .items = buf, .len = 0, .capacity = 3 };
    char out[64];
    StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    g_calls = 0;
    EXPECT(!vec_int_to_stringbuf_cb(NULL, &sb, fmt_bracket));
    EXPECT(!vec_int_to_stringbuf_cb(&v, NULL, fmt_bracket));
    EXPECT(!vec_int_to_stringbuf_cb(&v, &sb, NULL));
    EXPECT(vec_int_to_stringbuf_cb(&e, &sb, fmt_bracket));
    EXPECT(g_calls == 0);
    EXPECT(sb_is(&sb, ""));
}

static void test_struct_element_through_cb(void)
{
    Pt pbuf[2] = { { 1, 2 }, { -3, 4 } };
    vec_Pt v = { .items = pbuf, .len = 2, .capacity = 2 };
    char out[64];
    StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    g_calls = 0;
    EXPECT(vec_Pt_to_stringbuf_cb(&v, &sb, fmt_pt));
    EXPECT(g_calls == 2);
    EXPECT(sb_is(&sb, "(1,2)(-3,4)"));
}

/* VERIFY-042 F1: {1, 1000000, 2} into 8 bytes. Before: "1 2 " (the
 * middle element skipped). Now: "1 " and false, the third not appended. */
static void test_to_stringbuf_stops_when_full_f1(void)
{
    int buf[3] = { 1, 1000000, 2 };
    vec_int v = { .items = buf, .len = 3, .capacity = 3 };
    char out[8];
    StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    EXPECT(!vec_int_to_stringbuf(&v, &sb, "%d "));
    EXPECT(sb_is(&sb, "1 "));
}

static void test_to_stringbuf_cb_stops_when_full_f1(void)
{
    int buf[3] = { 1, 1000000, 2 };
    vec_int v = { .items = buf, .len = 3, .capacity = 3 };
    char out[8];
    StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    g_calls = 0;
    EXPECT(!vec_int_to_stringbuf_cb(&v, &sb, fmt_bracket));
    EXPECT(g_calls == 2);                 /* the third is never passed */
    EXPECT(sb_is(&sb, "[1]"));
}

/* the callback's own false stops the loop, whatever the StringBuf holds */
static void test_to_stringbuf_cb_stops_at_false(void)
{
    int buf[3];
    vec_int v = make_123(buf);
    char out[64];
    StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    g_calls = 0;
    EXPECT(!vec_int_to_stringbuf_cb(&v, &sb, fail_second));
    EXPECT(g_calls == 2);
    EXPECT(sb_is(&sb, "[1]"));
}

/* VERIFY-042 F3: before, 8 calls and "1,2,1,2,1,2,1,2,". */
static void test_to_stringbuf_cb_growing_vector_f3(void)
{
    int buf[8] = { 1, 2 };
    vec_int v = { .items = buf, .len = 2, .capacity = 8 };
    char out[64];
    StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    g_grow = &v;
    g_calls = 0;
    EXPECT(vec_int_to_stringbuf_cb(&v, &sb, fmt_and_grow));
    EXPECT(g_calls == 2);
    EXPECT(sb_is(&sb, "1,2,"));
    EXPECT(v.len == 4u);                  /* the callback's additions stay */
}

/* Every generated function is referenced, so no build warns about an
 * unused one; vec_Pt_to_stringbuf has no format for a struct and is not
 * called. */
static void suppress_unused(void)
{
    (void)vec_Pt_to_stringbuf;
    (void)vec_int_lifetime_open_;
    (void)vec_int_lifetime_close_;
    (void)vec_Pt_lifetime_open_;
    (void)vec_Pt_lifetime_close_;
}

int main(void)
{
    (void)suppress_unused;
    test_to_stringbuf_basic();
    test_to_stringbuf_appends_after_existing();
    test_to_stringbuf_null_and_empty();
    test_to_stringbuf_cb_basic();
    test_to_stringbuf_cb_null_and_empty();
    test_struct_element_through_cb();
    test_to_stringbuf_stops_when_full_f1();
    test_to_stringbuf_cb_stops_when_full_f1();
    test_to_stringbuf_cb_stops_at_false();
    test_to_stringbuf_cb_growing_vector_f3();
    if (g_failed) {
        printf("FAILED  vec_fmt_test  (%d assertion(s) failed)\n", g_failed);
        return 1;
    }
    printf("OK  vec_fmt_test  (all assertions passed)\n");
    return 0;
}
