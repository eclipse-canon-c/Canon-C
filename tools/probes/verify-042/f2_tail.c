/* VERIFY-042 F2, against the header at 63f6ae3: the format in the StringBuf's unused tail.
 * Undefined behaviour; glibc and musl differ. Build as f1_dropped.c (musl: musl-gcc, no sanitizers). */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include <string.h>
#include "semantics/error.h"
#include "semantics/result/result.h"
#include "data/vec/vec_defn.h"
#include "data/vec/vec_fmt.h"
DEFINE_VEC_STRUCTS(int)
DEFINE_VEC_FMT(static inline, int)
int main(void) {
    int b[3] = { 1234567, 2, 3 };
    vec_int v = { .items = b, .len = 3, .capacity = 3 };
    char out[32]; StringBuf sb;
    stringbuf_init_buffer(&sb, out, sizeof out);
    /* the format lives in the StringBuf's own unused tail */
    strcpy(out + 4, "%d,");
    vec_int_to_stringbuf(&v, &sb, out + 4);
    printf("fmt in tail: \"%s\" len=%zu\n", stringbuf_str(&sb), (size_t)stringbuf_len(&sb));
    (void)vec_int_to_stringbuf_cb; (void)vec_int_lifetime_open_; (void)vec_int_lifetime_close_;
    return 0;
}
