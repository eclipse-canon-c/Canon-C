/*
 * stringbuf_views.ya's rule, followed: views of a buffer read while it is
 * open; the buffer closed, initialised again, and new views taken and
 * read. Aorai with Eva: follows. Built with CANON_LIFETIME_DEBUG, which
 * checks each read as it runs, it returns 0.
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
    borrowed_bytes b = stringbuf_as_borrowed_bytes(&sb);
    CHECK(borrowed_str_get(&s).len == 5u && borrowed_bytes_get(&b).len == 5u);
    stringbuf_close(&sb);

    stringbuf_init_buffer(&sb, text, sizeof text);
    CHECK(stringbuf_append(&sb, "c99"));
    borrowed_bytes whole = stringbuf_buffer_as_borrowed_bytes(&sb);
    s = stringbuf_as_borrowed_str(&sb);
    CHECK(borrowed_str_get(&s).len == 3u && borrowed_bytes_get(&whole).len == sizeof text);
    stringbuf_close(&sb);

    END(fails);
    return fails != 0;
}
