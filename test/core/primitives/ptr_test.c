/**
 * @file ptr_test.c
 * @brief Tests for core/primitives/ptr.h
 *
 * Covers every public function and macro exported by ptr.h:
 *   - is_power_of_two
 *   - is_aligned / align_up / align_down / align_padding
 *   - ptr_align_up / ptr_align_down / ptr_is_aligned / ptr_align_padding
 *   - ptr_offset / ptr_offset_const / ptr_retreat
 *   - ptr_diff / ptr_span
 *   - ptr_in_range / ptr_range_in_range
 *   - ptr_elem / ptr_elem_const
 *   - ptr_or / ptr_or_const / ptr_is_valid
 *   - PTR_OFFSET_OF / PTR_CONTAINER_OF
 *   - ALIGN_OF / ALIGN_MAX
 *
 * Exit code: 0 on all pass, 1 on any failure.
 */

#include <stdio.h>
#include <stddef.h>

/* Define the contract handler in this translation unit.
 * contract.h uses the CANON_CONTRACT_IMPL pattern: exactly one TU must
 * define this before including the header so that canon_contract_handler
 * gets its definition (a contract_handler_fn variable, not a function).   */
#define CANON_CONTRACT_IMPL

/* On MSVC, ALIGN_OF falls back to the offsetof struct trick (C99 path),
 * which triggers warning C4116 (unnamed type definition in parentheses).
 * MSVC does support _Alignof as a C11 compiler, so force the C11 path by
 * predefining __STDC_VERSION__ before ptr.h is parsed. Only do this when
 * the compiler hasn't already set it high enough.                         */
#if defined(_MSC_VER) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L)
#   define __STDC_VERSION__ 201112L
#endif

#include "core/primitives/ptr.h"

/* ── Minimal test harness ─────────────────────────────────────────────────── */

static int s_pass = 0;
static int s_fail = 0;

#define CHECK(expr)                                                             \
    do {                                                                        \
        if (expr) {                                                             \
            ++s_pass;                                                           \
        } else {                                                                \
            ++s_fail;                                                           \
            fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__, #expr);   \
        }                                                                       \
    } while (0)

static int report(void) {
    printf("%d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}

/* ── Fixtures ─────────────────────────────────────────────────────────────── */

/* Flat buffer large enough for all pointer arithmetic tests. */
static u8 g_buf[256];

/* Struct for PTR_OFFSET_OF / PTR_CONTAINER_OF tests. */
typedef struct {
    u8   tag;
    u32  value;
    u8*  link;
} TestNode;

typedef struct {
    int      data;
    TestNode node;
} Container;

/* ══════════════════════════════════════════════════════════════════════════ */
/*  1. is_power_of_two                                                        */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_is_power_of_two(void) {
    /* true */
    CHECK(is_power_of_two(1));
    CHECK(is_power_of_two(2));
    CHECK(is_power_of_two(4));
    CHECK(is_power_of_two(8));
    CHECK(is_power_of_two(16));
    CHECK(is_power_of_two(64));
    CHECK(is_power_of_two(1024));
    CHECK(is_power_of_two(4096));
    CHECK(is_power_of_two((usize)1 << (sizeof(usize) * 8 - 1)));

    /* false */
    CHECK(!is_power_of_two(0));
    CHECK(!is_power_of_two(3));
    CHECK(!is_power_of_two(5));
    CHECK(!is_power_of_two(6));
    CHECK(!is_power_of_two(7));
    CHECK(!is_power_of_two(12));
    CHECK(!is_power_of_two(100));
    CHECK(!is_power_of_two(255));
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  2. is_aligned                                                              */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_is_aligned(void) {
    CHECK( is_aligned(0,   8));   /* zero is always aligned */
    CHECK( is_aligned(8,   8));
    CHECK( is_aligned(16,  8));
    CHECK( is_aligned(64,  8));
    CHECK(!is_aligned(1,   8));
    CHECK(!is_aligned(7,   8));
    CHECK(!is_aligned(9,   8));
    CHECK(!is_aligned(13,  8));

    CHECK( is_aligned(0,   1));   /* align=1: everything is aligned */
    CHECK( is_aligned(1,   1));
    CHECK( is_aligned(99,  1));

    CHECK( is_aligned(0,  16));
    CHECK( is_aligned(16, 16));
    CHECK(!is_aligned(15, 16));
    CHECK(!is_aligned(17, 16));
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  3. align_up                                                               */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_align_up(void) {
    CHECK(align_up(0,   8) == 0);
    CHECK(align_up(1,   8) == 8);
    CHECK(align_up(7,   8) == 8);
    CHECK(align_up(8,   8) == 8);   /* already aligned */
    CHECK(align_up(9,   8) == 16);
    CHECK(align_up(13,  8) == 16);
    CHECK(align_up(16,  8) == 16);  /* already aligned */

    CHECK(align_up(0,   1) == 0);
    CHECK(align_up(5,   1) == 5);   /* align=1: no-op */

    CHECK(align_up(0,  16) == 0);
    CHECK(align_up(1,  16) == 16);
    CHECK(align_up(15, 16) == 16);
    CHECK(align_up(16, 16) == 16);
    CHECK(align_up(17, 16) == 32);

    CHECK(align_up(100, 64) == 128);
    CHECK(align_up(128, 64) == 128);
    CHECK(align_up(129, 64) == 192);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  4. align_down                                                              */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_align_down(void) {
    CHECK(align_down(0,   8) == 0);
    CHECK(align_down(1,   8) == 0);
    CHECK(align_down(7,   8) == 0);
    CHECK(align_down(8,   8) == 8);
    CHECK(align_down(9,   8) == 8);
    CHECK(align_down(15,  8) == 8);
    CHECK(align_down(16,  8) == 16);

    CHECK(align_down(0,   1) == 0);
    CHECK(align_down(7,   1) == 7);  /* align=1: no-op */

    CHECK(align_down(0,  16) == 0);
    CHECK(align_down(15, 16) == 0);
    CHECK(align_down(16, 16) == 16);
    CHECK(align_down(31, 16) == 16);
    CHECK(align_down(32, 16) == 32);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  5. align_padding                                                           */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_align_padding(void) {
    CHECK(align_padding(0,   8) == 0);
    CHECK(align_padding(8,   8) == 0);
    CHECK(align_padding(1,   8) == 7);
    CHECK(align_padding(7,   8) == 1);
    CHECK(align_padding(9,   8) == 7);
    CHECK(align_padding(13,  8) == 3);

    CHECK(align_padding(0,   1) == 0);
    CHECK(align_padding(5,   1) == 0);  /* align=1: always 0 */

    CHECK(align_padding(1,  16) == 15);
    CHECK(align_padding(16, 16) == 0);
    CHECK(align_padding(17, 16) == 15);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  6. ptr_align_up / ptr_align_down                                          */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_ptr_align_up_down(void) {
    u8* base = g_buf;

    /* NULL passthrough */
    CHECK(ptr_align_up(NULL,   8) == NULL);
    CHECK(ptr_align_down(NULL, 8) == NULL);

    /* Already aligned pointer — should be unchanged */
    u8* aligned = (u8*)align_up((usize)(uintptr_t)base, 16);
    CHECK(ptr_align_up(aligned,   16) == (void*)aligned);
    CHECK(ptr_align_down(aligned, 16) == (void*)aligned);

    /* One byte past alignment boundary */
    u8* one_past = aligned + 1;
    CHECK(ptr_align_up(one_past,   16) == (void*)(aligned + 16));
    CHECK(ptr_align_down(one_past, 16) == (void*)aligned);

    /* align=1: always a no-op */
    CHECK(ptr_align_up(base + 3,   1) == (void*)(base + 3));
    CHECK(ptr_align_down(base + 3, 1) == (void*)(base + 3));
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  7. ptr_is_aligned / ptr_align_padding                                     */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_ptr_is_aligned_padding(void) {
    /* NULL: is_aligned → false, padding → 0 */
    CHECK(!ptr_is_aligned(NULL, 8));
    CHECK( ptr_align_padding(NULL, 8) == 0);

    u8* base    = g_buf;
    u8* aligned = (u8*)align_up((usize)(uintptr_t)base, 16);

    CHECK( ptr_is_aligned(aligned,      16));
    CHECK(!ptr_is_aligned(aligned + 1,  16));
    CHECK(!ptr_is_aligned(aligned + 7,  16));
    CHECK( ptr_is_aligned(aligned + 16, 16));

    CHECK(ptr_align_padding(aligned,      16) == 0);
    CHECK(ptr_align_padding(aligned + 1,  16) == 15);
    CHECK(ptr_align_padding(aligned + 15, 16) == 1);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  8. ptr_offset / ptr_offset_const                                          */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_ptr_offset(void) {
    u8* base = g_buf;

    /* NULL passthrough */
    CHECK(ptr_offset(NULL, 10)       == NULL);
    CHECK(ptr_offset_const(NULL, 10) == NULL);

    /* Zero offset — same pointer */
    CHECK(ptr_offset(base, 0)       == (void*)base);
    CHECK(ptr_offset_const(base, 0) == (const void*)base);

    /* Positive offsets */
    CHECK(ptr_offset(base, 1)   == (void*)(base + 1));
    CHECK(ptr_offset(base, 8)   == (void*)(base + 8));
    CHECK(ptr_offset(base, 128) == (void*)(base + 128));
    CHECK(ptr_offset(base, 255) == (void*)(base + 255));

    /* const variant */
    CHECK(ptr_offset_const(base, 32) == (const void*)(base + 32));
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  9. ptr_retreat                                                             */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_ptr_retreat(void) {
    u8* base = g_buf;
    u8* mid  = base + 128;

    /* NULL passthrough */
    CHECK(ptr_retreat(NULL, 10) == NULL);

    /* Zero retreat — same pointer */
    CHECK(ptr_retreat(mid, 0) == (void*)mid);

    /* Positive retreat */
    CHECK(ptr_retreat(mid,  1)  == (void*)(mid - 1));
    CHECK(ptr_retreat(mid, 64)  == (void*)(mid - 64));
    CHECK(ptr_retreat(mid, 128) == (void*)base);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  10. ptr_diff / ptr_span                                                   */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_ptr_diff_span(void) {
    u8* base = g_buf;
    u8* mid  = base + 64;
    u8* end  = base + 128;

    /* ptr_diff: signed, may be negative */
    CHECK(ptr_diff(base, base) == 0);
    CHECK(ptr_diff(mid,  base) == 64);
    CHECK(ptr_diff(end,  base) == 128);
    CHECK(ptr_diff(base, mid)  == -64);
    CHECK(ptr_diff(end,  mid)  == 64);

    /* ptr_span: unsigned, requires to >= from */
    CHECK(ptr_span(base, base) == 0);
    CHECK(ptr_span(mid,  base) == 64);
    CHECK(ptr_span(end,  base) == 128);
    CHECK(ptr_span(end,  mid)  == 64);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  11. ptr_in_range                                                           */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_ptr_in_range(void) {
    u8* start = g_buf;
    u8* end   = g_buf + 128;

    /* NULL handling */
    CHECK(!ptr_in_range(NULL,  start, end));
    CHECK(!ptr_in_range(start, NULL,  end));
    CHECK(!ptr_in_range(start, start, NULL));

    /* Inside [start, end) */
    CHECK( ptr_in_range(start,      start, end));
    CHECK( ptr_in_range(start + 1,  start, end));
    CHECK( ptr_in_range(start + 64, start, end));
    CHECK( ptr_in_range(end - 1,    start, end));

    /* Outside */
    CHECK(!ptr_in_range(end,       start, end));  /* one-past-end excluded */
    CHECK(!ptr_in_range(end + 1,   start, end));
    CHECK(!ptr_in_range(start - 1, start, end));
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  12. ptr_range_in_range                                                    */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_ptr_range_in_range(void) {
    u8* start = g_buf;
    u8* end   = g_buf + 128;

    /* NULL handling */
    CHECK(!ptr_range_in_range(NULL,  10, start, end));
    CHECK(!ptr_range_in_range(start, 10, NULL,  end));
    CHECK(!ptr_range_in_range(start, 10, start, NULL));

    /* Zero-length range */
    CHECK( ptr_range_in_range(start,      0, start, end));
    CHECK( ptr_range_in_range(start + 64, 0, start, end));
    CHECK( ptr_range_in_range(end,        0, start, end));

    /* Fully inside */
    CHECK( ptr_range_in_range(start,      1,   start, end));
    CHECK( ptr_range_in_range(start,      128, start, end));
    CHECK( ptr_range_in_range(start + 64, 64,  start, end));

    /* Overflows region */
    CHECK(!ptr_range_in_range(start,      129, start, end));
    CHECK(!ptr_range_in_range(start + 64, 65,  start, end));
    CHECK(!ptr_range_in_range(start + 1,  128, start, end));

    /* Starts before region */
    CHECK(!ptr_range_in_range(start - 1,  1,   start, end));
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  13. ptr_elem / ptr_elem_const                                             */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_ptr_elem(void) {
    u8* base = g_buf;

    /* elem_size = 1 */
    CHECK(ptr_elem(base, 0,   1) == (void*)(base + 0));
    CHECK(ptr_elem(base, 1,   1) == (void*)(base + 1));
    CHECK(ptr_elem(base, 100, 1) == (void*)(base + 100));

    /* elem_size = 4 (u32 array) */
    CHECK(ptr_elem(base, 0, 4) == (void*)(base + 0));
    CHECK(ptr_elem(base, 1, 4) == (void*)(base + 4));
    CHECK(ptr_elem(base, 2, 4) == (void*)(base + 8));
    CHECK(ptr_elem(base, 3, 4) == (void*)(base + 12));

    /* elem_size = 8 (u64 array) */
    CHECK(ptr_elem(base, 0, 8) == (void*)(base + 0));
    CHECK(ptr_elem(base, 4, 8) == (void*)(base + 32));

    /* const variant */
    CHECK(ptr_elem_const(base, 3, 4) == (const void*)(base + 12));
    CHECK(ptr_elem_const(base, 0, 8) == (const void*)(base + 0));
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  14. ptr_or / ptr_or_const / ptr_is_valid                                  */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_ptr_null_safety(void) {
    u8* base     = g_buf;
    u8* fallback = g_buf + 64;

    CHECK(ptr_or(base, fallback) == (void*)base);
    CHECK(ptr_or(NULL, fallback) == (void*)fallback);
    CHECK(ptr_or(base, NULL)     == (void*)base);
    CHECK(ptr_or(NULL, NULL)     == NULL);

    CHECK(ptr_or_const(base, fallback) == (const void*)base);
    CHECK(ptr_or_const(NULL, fallback) == (const void*)fallback);
    CHECK(ptr_or_const(NULL, NULL)     == NULL);

    CHECK( ptr_is_valid(base));
    CHECK( ptr_is_valid(fallback));
    CHECK(!ptr_is_valid(NULL));
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  15. PTR_OFFSET_OF / PTR_CONTAINER_OF                                      */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_struct_macros(void) {
    /* PTR_OFFSET_OF must agree with standard offsetof */
    CHECK(PTR_OFFSET_OF(TestNode, tag)   == (usize)offsetof(TestNode, tag));
    CHECK(PTR_OFFSET_OF(TestNode, value) == (usize)offsetof(TestNode, value));
    CHECK(PTR_OFFSET_OF(TestNode, link)  == (usize)offsetof(TestNode, link));

    CHECK(PTR_OFFSET_OF(Container, data) == (usize)offsetof(Container, data));
    CHECK(PTR_OFFSET_OF(Container, node) == (usize)offsetof(Container, node));

    /* PTR_CONTAINER_OF: round-trip from member pointer back to parent */
    Container c;
    c.data       = 42;
    c.node.tag   = 7;
    c.node.value = 99;

    Container* recovered = PTR_CONTAINER_OF(&c.node, Container, node);
    CHECK(recovered        == &c);
    CHECK(recovered->data       == 42);
    CHECK(recovered->node.tag   == 7);
    CHECK(recovered->node.value == 99);

    /* Works from the very first member too */
    TestNode n;
    n.tag = 3;
    CHECK(PTR_CONTAINER_OF(&n.tag, TestNode, tag) == &n);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  16. ALIGN_OF / ALIGN_MAX                                                  */
/* ══════════════════════════════════════════════════════════════════════════ */

static void test_align_macros(void) {
    /* Pre-compute ALIGN_OF values into named variables.
     * MSVC warns (C4116) about unnamed type definitions in parentheses when
     * the offsetof-trick form of ALIGN_OF appears directly inside an expression
     * macro like CHECK(). Hoisting to variables avoids that entirely.        */
    usize al_char   = ALIGN_OF(char);
    usize al_short  = ALIGN_OF(short);
    usize al_int    = ALIGN_OF(int);
    usize al_long   = ALIGN_OF(long);
    usize al_float  = ALIGN_OF(float);
    usize al_double = ALIGN_OF(double);
    usize al_ptr    = ALIGN_OF(void*);

    /* ALIGN_OF must be a positive power of two for fundamental types */
    CHECK(is_power_of_two(al_char));
    CHECK(is_power_of_two(al_short));
    CHECK(is_power_of_two(al_int));
    CHECK(is_power_of_two(al_long));
    CHECK(is_power_of_two(al_float));
    CHECK(is_power_of_two(al_double));
    CHECK(is_power_of_two(al_ptr));

    /* char always has alignment 1 */
    CHECK(al_char == 1);

    /* ALIGN_MAX: result is the larger of the two */
    CHECK(ALIGN_MAX(1,  4)  == 4);
    CHECK(ALIGN_MAX(4,  1)  == 4);
    CHECK(ALIGN_MAX(4,  4)  == 4);
    CHECK(ALIGN_MAX(8,  4)  == 8);
    CHECK(ALIGN_MAX(4,  8)  == 8);
    CHECK(ALIGN_MAX(16, 32) == 32);

    /* ALIGN_MAX of type alignments */
    CHECK(ALIGN_MAX(al_char,   al_double) == al_double);
    CHECK(ALIGN_MAX(al_int,    al_int)    == al_int);
    CHECK(ALIGN_MAX(al_double, al_char)   == al_double);
}

/* ══════════════════════════════════════════════════════════════════════════ */
/*  main                                                                       */
/* ══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    test_is_power_of_two();
    test_is_aligned();
    test_align_up();
    test_align_down();
    test_align_padding();
    test_ptr_align_up_down();
    test_ptr_is_aligned_padding();
    test_ptr_offset();
    test_ptr_retreat();
    test_ptr_diff_span();
    test_ptr_in_range();
    test_ptr_range_in_range();
    test_ptr_elem();
    test_ptr_null_safety();
    test_struct_macros();
    test_align_macros();

    return report();
}
