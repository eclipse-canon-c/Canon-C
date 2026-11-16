/*
 * stringbuf_views.ya's rule, broken: a view read after its buffer was
 * closed. Aorai with Eva: breaks, at the call marked "refused". Built with
 * CANON_LIFETIME_DEBUG, the read fails its check as it runs.
 */
#define CANON_CONTRACT_IMPL
#include "data/stringbuf.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    char text[16];
    StringBuf sb;

    stringbuf_init_buffer(&sb, text, sizeof text);
    CHECK(stringbuf_append(&sb, "canon"));
    borrowed_str s = stringbuf_as_borrowed_str(&sb);
    stringbuf_close(&sb);
    str_t gone = borrowed_str_get(&s);   /* refused */
    CHECK(gone.len == 5u);

    END(fails);
    return fails != 0;
}
