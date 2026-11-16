/*
 * stringbuf_views.ya's rule, broken: a view taken before its buffer was
 * closed, read after the buffer was initialised again and before any new
 * view was taken. Aorai with Eva: breaks, at the call marked "refused".
 * Built with CANON_LIFETIME_DEBUG, the read fails its check as it runs: the
 * new init drew a new lifetime.
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
    borrowed_bytes b = stringbuf_as_borrowed_bytes(&sb);
    stringbuf_close(&sb);
    stringbuf_init_buffer(&sb, text, sizeof text);
    cbytes_t old = borrowed_bytes_get(&b);   /* refused */
    CHECK(old.len == 5u);

    END(fails);
    return fails != 0;
}
