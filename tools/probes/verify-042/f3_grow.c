/* VERIFY-042 F3, against the header at 63f6ae3: a callback that grows its vector.
 * Build as f1_dropped.c. */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "semantics/error.h"
#include "semantics/result/result.h"
#include "data/vec/vec_defn.h"
#include "data/vec/vec_fmt.h"
DEFINE_VEC_STRUCTS(int)
DEFINE_VEC_FMT(static inline, int)
static vec_int* g_v; static int g_calls;
/* a callback that also records each value it formats in the same vector */
static void cb(StringBuf* sb, int x) {
    g_calls++;
    (void)stringbuf_printf(sb, "%d,", x);
    if (g_v->len < g_v->capacity) { g_v->items[g_v->len++] = x; }
}
int main(void) {
    int b[8] = { 1, 2 };
    vec_int v = { .items = b, .len = 2, .capacity = 8 }; g_v = &v;
    char out[64]; StringBuf sb; stringbuf_init_buffer(&sb, out, sizeof out);
    vec_int_to_stringbuf_cb(&v, &sb, cb);
    printf("calls=%d out=\"%s\" (vector had 2 elements at the call)\n", g_calls, stringbuf_str(&sb));
    (void)vec_int_to_stringbuf; (void)vec_int_lifetime_open_; (void)vec_int_lifetime_close_;
    return 0;
}
