/* VERIFY-042 F2, found by the record's review: the format's first byte in the current
 * string, its terminator in the tail. It does NOT build against the header at 63f6ae3:
 * it needs the run-1 header, where the functions return bool. With that header, build
 * it with f1_dropped.c's command line. */
#define CANON_CONTRACT_IMPL
#include <stdio.h>
#include "semantics/error.h"
#include "semantics/result/result.h"
#include "data/vec/vec_defn.h"
#include "data/vec/vec_fmt.h"
DEFINE_VEC_STRUCTS(int)
DEFINE_VEC_FMT(static inline, int)
int main(void) {
    int b[3] = { 1, 2, 3 };
    vec_int v = { .items = b, .len = 3, .capacity = 3 };
    char out[32]; StringBuf sb; stringbuf_init_buffer(&sb, out, sizeof out);
    stringbuf_append(&sb, "x%d");          /* len 3: "x%d", NUL at out[3] */
    /* fmt = out+1 = "%d": fmt itself is outside [data+len, data+cap) (fmt_sep holds),
       but its terminator out[3] is the tail's first byte */
    bool ok = vec_int_to_stringbuf(&v, &sb, out + 1);
    printf("ok=%d \"%s\" len=%zu\n", ok, stringbuf_str(&sb), (size_t)stringbuf_len(&sb));
    (void)vec_int_to_stringbuf_cb; (void)vec_int_lifetime_open_; (void)vec_int_lifetime_close_;
    return 0;
}
