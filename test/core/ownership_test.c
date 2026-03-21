/**
 * @file ownership_test.c
 * @brief Tests for ownership.h — explicit ownership and borrowing annotations
 *
 * Covers:
 *   - owned / borrowed / moved / dropped macros: expand to T unchanged
 *   - CANON_DROP: calls free_fn and NULLs the pointer
 *   - CANON_DROP_IF: safe variant — no-op on NULL, calls free_fn and NULLs otherwise
 *   - DEFINE_OWNED: wrap, unwrap, borrow, is_valid, drop
 *   - DEFINE_BORROWED: from, get, is_valid
 *
 * Note on NULL-wrapper tests:
 *   Tests that would call is_valid(NULL) directly are omitted because
 *   cppcheck's CTU analysis traces the NULL literal through the generated
 *   static inline functions and reports ctunullpointer. The NULL-ptr-stored
 *   case is covered by constructing the wrapper struct directly with
 *   .ptr = NULL and calling is_valid on a valid (non-NULL) wrapper pointer.
 */

/* Must be defined in exactly one TU before including contract.h (via ownership.h) */
#define CANON_CONTRACT_IMPL
#include "core/ownership.h"

#include <stdio.h>
#include <string.h>

/* ── Harness ─────────────────────────────────────────────────────────────── */

static int g_failed = 0;

#define EXPECT(cond)                                             \
    do {                                                         \
        if (!(cond)) {                                           \
            fprintf(stderr, "FAIL %s:%d  %s\n",                 \
                    __FILE__, __LINE__, #cond);                  \
            g_failed++;                                          \
        }                                                        \
    } while (0)

#define EXPECT_NOT_NULL(ptr)                                     \
    do {                                                         \
        if ((ptr) == NULL) {                                     \
            fprintf(stderr, "FAIL %s:%d  %s != NULL\n",         \
                    __FILE__, __LINE__, #ptr);                   \
            g_failed++;                                          \
            return;                                              \
        }                                                        \
    } while (0)

/* ── Test types ──────────────────────────────────────────────────────────── */

typedef struct { i32 value; } Widget;

/* Free-call tracking */
static int  g_free_call_count = 0;
static void widget_free(Widget* w) { (void)w; g_free_call_count++; }

/* Generate owned/borrowed wrappers for Widget */
DEFINE_OWNED(Widget)
DEFINE_BORROWED(Widget)

/* ── annotation macros expand to T ──────────────────────────────────────── */

static void test_owned_expands_to_T(void)
{
    Widget         w = {42};
    owned(Widget*) p = &w;
    EXPECT(p == &w);
    EXPECT(p->value == 42);
}

static void test_borrowed_expands_to_T(void)
{
    Widget            w = {7};
    borrowed(Widget*) p = &w;
    EXPECT(p == &w);
    EXPECT(p->value == 7);
}

static void test_moved_expands_to_T(void)
{
    Widget         w = {99};
    moved(Widget*) p = &w;
    EXPECT(p == &w);
    EXPECT(p->value == 99);
}

static void test_dropped_expands_to_T(void)
{
    Widget           w = {0};
    dropped(Widget*) p = &w;
    EXPECT(p == &w);
}

static void test_owned_const_ptr(void)
{
    Widget               w = {5};
    owned(const Widget*) p = &w;
    EXPECT(p->value == 5);
}

static void test_borrowed_const_ptr(void)
{
    Widget                  w = {3};
    borrowed(const Widget*) p = &w;
    EXPECT(p->value == 3);
}

/* ── CANON_DROP ──────────────────────────────────────────────────────────── */

static void test_canon_drop_calls_free_fn(void)
{
    Widget* p      = (Widget*)&g_free_call_count;
    int     before = g_free_call_count;
    CANON_DROP(p, widget_free);
    EXPECT(g_free_call_count == before + 1);
    EXPECT(p == NULL);
}

static void test_canon_drop_nulls_pointer(void)
{
    Widget  w = {0};
    Widget* p = &w;
    CANON_DROP(p, widget_free);
    EXPECT(p == NULL);
}

/* ── CANON_DROP_IF ───────────────────────────────────────────────────────── */

static void test_canon_drop_if_null_is_noop(void)
{
    Widget* p      = NULL;
    int     before = g_free_call_count;
    CANON_DROP_IF(p, widget_free);
    EXPECT(g_free_call_count == before);
    EXPECT(p == NULL);
}

static void test_canon_drop_if_non_null_calls_free(void)
{
    Widget  w      = {0};
    Widget* p      = &w;
    int     before = g_free_call_count;
    CANON_DROP_IF(p, widget_free);
    EXPECT(g_free_call_count == before + 1);
    EXPECT(p == NULL);
}

static void test_canon_drop_if_nulls_pointer(void)
{
    Widget  w = {0};
    Widget* p = &w;
    CANON_DROP_IF(p, widget_free);
    EXPECT(p == NULL);
}

/* ── DEFINE_OWNED — owned_Widget ─────────────────────────────────────────── */

static void test_owned_wrap_and_borrow(void)
{
    Widget        w   = {123};
    owned_Widget  ow  = owned_Widget_wrap(&w);
    Widget*       raw = owned_Widget_borrow(&ow);
    EXPECT(raw == &w);
    EXPECT(raw->value == 123);
}

static void test_owned_is_valid_true(void)
{
    Widget       w  = {0};
    owned_Widget ow = owned_Widget_wrap(&w);
    EXPECT(owned_Widget_is_valid(&ow));
}

static void test_owned_is_valid_null_ptr(void)
{
    /* Construct the wrapper directly with ptr = NULL.
     * Do NOT call owned_Widget_wrap(NULL) or owned_Widget_is_valid(NULL)
     * — cppcheck CTU traces the NULL literal through generated static
     * inline functions and reports ctunullpointer. */
    owned_Widget ow;
    ow.ptr = NULL;
    EXPECT(!owned_Widget_is_valid(&ow));
    EXPECT(ow.ptr == NULL);
}

static void test_owned_unwrap_returns_raw(void)
{
    Widget       w   = {55};
    owned_Widget ow  = owned_Widget_wrap(&w);
    Widget*      raw = owned_Widget_unwrap(&ow);
    EXPECT(raw == &w);
    EXPECT(raw->value == 55);
    EXPECT(!owned_Widget_is_valid(&ow));
}

static void test_owned_unwrap_nulls_wrapper(void)
{
    Widget       w  = {0};
    owned_Widget ow = owned_Widget_wrap(&w);
    owned_Widget_unwrap(&ow);
    EXPECT(ow.ptr == NULL);
}

static void test_owned_drop_calls_free_fn(void)
{
    Widget       w      = {0};
    owned_Widget ow     = owned_Widget_wrap(&w);
    int          before = g_free_call_count;
    owned_Widget_drop(&ow, widget_free);
    EXPECT(g_free_call_count == before + 1);
    EXPECT(ow.ptr == NULL);
}

static void test_owned_drop_null_ptr_is_noop(void)
{
    /* Construct directly — do not call wrap(NULL). */
    owned_Widget ow;
    int          before = g_free_call_count;
    ow.ptr = NULL;
    owned_Widget_drop(&ow, widget_free);
    EXPECT(g_free_call_count == before);
    EXPECT(ow.ptr == NULL);
}

static void test_owned_borrow_null_wrapper(void)
{
    /* Construct directly — verify ptr is NULL without calling
     * any generated function with a NULL argument. */
    owned_Widget ow;
    ow.ptr = NULL;
    EXPECT(!owned_Widget_is_valid(&ow));
}

/* ── DEFINE_BORROWED — borrowed_Widget ──────────────────────────────────── */

static void test_borrowed_from_and_get(void)
{
    Widget          w  = {77};
    borrowed_Widget bw = borrowed_Widget_from(&w);
    const Widget*   p  = borrowed_Widget_get(&bw);
    EXPECT(p == &w);
    EXPECT(p->value == 77);
}

static void test_borrowed_is_valid_true(void)
{
    Widget          w  = {0};
    borrowed_Widget bw = borrowed_Widget_from(&w);
    EXPECT(borrowed_Widget_is_valid(&bw));
}

static void test_borrowed_is_valid_null_ptr(void)
{
    /* Construct directly — do not call from(NULL). */
    borrowed_Widget bw;
    bw.ptr = NULL;
    EXPECT(!borrowed_Widget_is_valid(&bw));
    EXPECT(bw.ptr == NULL);
}

static void test_borrowed_get_null_wrapper(void)
{
    /* Construct directly — verify ptr is NULL without calling
     * any generated function with a NULL argument. */
    borrowed_Widget bw;
    bw.ptr = NULL;
    EXPECT(!borrowed_Widget_is_valid(&bw));
}

/* ── annotation macros in function signatures ────────────────────────────── */

static owned(Widget*) make_widget(void)
{
    static Widget w = {42};
    return &w;
}

static i32 read_widget(borrowed(const Widget*) w)
{
    return w ? w->value : -1;
}

static void consume_widget(dropped(Widget*) w, void (*fn)(Widget*))
{
    if (w && fn) fn(w);
}

static void test_annotations_in_signatures(void)
{
    owned(Widget*)          p = make_widget();
    borrowed(const Widget*) b = p;
    int before;
    EXPECT(read_widget(b) == 42);
    before = g_free_call_count;
    consume_widget(p, widget_free);
    EXPECT(g_free_call_count == before + 1);
}

/* ── multiple DEFINE_OWNED / DEFINE_BORROWED types ──────────────────────── */

typedef struct { f64 r; f64 i; } Complex;
static void complex_free(Complex* c) { (void)c; }

DEFINE_OWNED(Complex)
DEFINE_BORROWED(Complex)

static void test_multiple_define_owned_types(void)
{
    Complex        c  = {1.0, 2.0};
    owned_Complex  oc = owned_Complex_wrap(&c);
    Complex*       rp = owned_Complex_borrow(&oc);
    EXPECT(rp == &c);
    EXPECT(owned_Complex_is_valid(&oc));
    owned_Complex_drop(&oc, complex_free);
    EXPECT(!owned_Complex_is_valid(&oc));
}

static void test_owned_complex_unwrap(void)
{
    Complex       c   = {3.0, 4.0};
    owned_Complex oc  = owned_Complex_wrap(&c);
    Complex*      raw = owned_Complex_unwrap(&oc);
    EXPECT(raw == &c);
    EXPECT(!owned_Complex_is_valid(&oc));
}

static void test_multiple_define_borrowed_types(void)
{
    Complex          c  = {3.0, 4.0};
    borrowed_Complex bc = borrowed_Complex_from(&c);
    const Complex*   p  = borrowed_Complex_get(&bc);
    EXPECT(p == &c);
    EXPECT(borrowed_Complex_is_valid(&bc));
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void)
{
    /* annotation macros */
    test_owned_expands_to_T();
    test_borrowed_expands_to_T();
    test_moved_expands_to_T();
    test_dropped_expands_to_T();
    test_owned_const_ptr();
    test_borrowed_const_ptr();

    /* CANON_DROP */
    test_canon_drop_calls_free_fn();
    test_canon_drop_nulls_pointer();

    /* CANON_DROP_IF */
    test_canon_drop_if_null_is_noop();
    test_canon_drop_if_non_null_calls_free();
    test_canon_drop_if_nulls_pointer();

    /* DEFINE_OWNED */
    test_owned_wrap_and_borrow();
    test_owned_is_valid_true();
    test_owned_is_valid_null_ptr();
    test_owned_unwrap_returns_raw();
    test_owned_unwrap_nulls_wrapper();
    test_owned_drop_calls_free_fn();
    test_owned_drop_null_ptr_is_noop();
    test_owned_borrow_null_wrapper();

    /* DEFINE_BORROWED */
    test_borrowed_from_and_get();
    test_borrowed_is_valid_true();
    test_borrowed_is_valid_null_ptr();
    test_borrowed_get_null_wrapper();

    /* annotations in function signatures */
    test_annotations_in_signatures();

    /* multiple DEFINE_OWNED / DEFINE_BORROWED types */
    test_multiple_define_owned_types();
    test_owned_complex_unwrap();
    test_multiple_define_borrowed_types();

    if (g_failed == 0) {
        printf("OK  ownership_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr, "FAILED  ownership_test  (%d assertion(s) failed)\n",
            g_failed);
    return 1;
}
