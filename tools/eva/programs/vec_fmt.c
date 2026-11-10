/*
 * vec_fmt.h, instantiated for int as its proof instantiates it
 * (vmacros/vdrivers/vec_fmt_verify.h, which defines the vector type but
 * not its functions, so the vector is made from its fields): a vector
 * written into a string buffer with a format, with a callback, and into a
 * buffer too small for it.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/vec_fmt_verify.h"
#include "eva_program.h"

static bool hex(StringBuf *sb, int v)
{
    return stringbuf_append_fmt(sb, "%x", (unsigned)v);
}

int main(void)
{
    int fails = 0;
    int storage[3] = { 7, 10, 255 };
    vec_int v = { storage, 3u, 3u };
    char text[64];
    char tiny[4];
    StringBuf sb;
    StringBuf small;

    stringbuf_init_buffer(&sb, text, sizeof text);
    CHECK(vec_int_to_stringbuf(&v, &sb, "%d"));
    CHECK(stringbuf_len(&sb) == 6u && stringbuf_str(&sb)[0] == '7');   /* "710255" */
    stringbuf_clear(&sb);
    CHECK(vec_int_to_stringbuf_cb(&v, &sb, hex));
    CHECK(stringbuf_len(&sb) == 4u);                                    /* "7aff" */
    stringbuf_init_buffer(&small, tiny, sizeof tiny);
    CHECK(!vec_int_to_stringbuf(&v, &small, "%d"));
    stringbuf_close(&sb);
    stringbuf_close(&small);

    END(fails);
    return fails != 0;
}
