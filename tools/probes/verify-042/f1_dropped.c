/* VERIFY-042 F1, against the header at 63f6ae3: elements dropped from the middle.
 * From the repository root: gcc -std=c99 -fsanitize=address,undefined -I core/primitives -I core -I semantics -I data -I . tools/probes/verify-042/f1_dropped.c -o p && ./p */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "semantics/option/option.h"
#include "data/vec/vec.h"
#include "data/vec/vec_fmt.h"
CANON_OPTION(int)
DEFINE_VEC(static inline, int)
DEFINE_VEC_FMT(static inline, int)
static void cb(StringBuf* sb, int x) { (void)stringbuf_printf(sb, "[%d]", x); }
int main(void) {
    int b[3] = { 1, 1000000, 2 };
    vec_int v = vec_int_init(b, 3); v.len = 3;
    char out[8]; StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    vec_int_to_stringbuf(&v, &sb, "%d ");
    printf("fmt, cap 8:  \"%s\" len=%zu\n", stringbuf_str(&sb), (size_t)stringbuf_len(&sb));
    stringbuf_init_buffer(&sb, out, sizeof out);
    vec_int_to_stringbuf_cb(&v, &sb, cb);
    printf("cb,  cap 8:  \"%s\" len=%zu\n", stringbuf_str(&sb), (size_t)stringbuf_len(&sb));
    return 0;
}
