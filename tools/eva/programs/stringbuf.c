/*
 * stringbuf.h: a string built in a fixed buffer (appended to, formatted
 * into, truncated, cleared, read as views and borrows) and one in an
 * arena; and appends that do not fit, which a StringBuf refuses: its
 * capacity is fixed.
 */
#define CANON_CONTRACT_IMPL
#include "data/stringbuf.h"
#include "eva_program.h"

static u64 backing[16];

int main(void)
{
    int fails = 0;
    char text[32];
    StringBuf sb;

    stringbuf_init_buffer(&sb, text, sizeof text);
    CHECK(stringbuf_is_empty(&sb) && !stringbuf_is_arena_backed(&sb));
    CHECK(stringbuf_capacity(&sb) > 0u);
    CHECK(stringbuf_append(&sb, "canon"));
    CHECK(stringbuf_append_char(&sb, '-'));
    CHECK(stringbuf_append_str(&sb, str_from("c99", 1u)));
    CHECK(stringbuf_append_n(&sb, "!!!", 2u));
    CHECK(stringbuf_append_fmt(&sb, " %d/%s", 42, "ok"));
    CHECK(stringbuf_len(&sb) == 15u && stringbuf_str(&sb)[5] == '-');
    CHECK(stringbuf_as_str(&sb).len == 15u && stringbuf_as_bytes(&sb).len == 15u);
    CHECK(stringbuf_as_cbytes(&sb).len == 15u);
    CHECK(stringbuf_buffer_bytes(&sb).len == sizeof text && stringbuf_buffer_cbytes(&sb).len == sizeof text);
    borrowed_str bs = stringbuf_as_borrowed_str(&sb);
    borrowed_bytes bb = stringbuf_as_borrowed_bytes(&sb);
    borrowed_bytes whole = stringbuf_buffer_as_borrowed_bytes(&sb);
    CHECK(borrowed_str_len(&bs) == 15u && borrowed_bytes_len(&bb) == 15u);
    CHECK(borrowed_bytes_len(&whole) == sizeof text);
    stringbuf_truncate(&sb, 5u);
    CHECK(stringbuf_len(&sb) == 5u && stringbuf_str(&sb)[5] == '\0');
    CHECK(!stringbuf_append(&sb, "this text is longer than the space left"));
    CHECK(stringbuf_remaining(&sb) + stringbuf_len(&sb) <= stringbuf_capacity(&sb));
    stringbuf_clear(&sb);
    CHECK(stringbuf_is_empty(&sb) && !stringbuf_is_full(&sb));
    stringbuf_close(&sb);

    Arena a;
    StringBuf in_arena;
    arena_init(&a, backing, sizeof backing);
    CHECK(stringbuf_init_arena(&in_arena, &a, 16u));
    CHECK(stringbuf_is_arena_backed(&in_arena) && stringbuf_append(&in_arena, "grows"));
    CHECK(!stringbuf_append(&in_arena, " is more than sixteen bytes"));
    stringbuf_close(&in_arena);

    END(fails);
    return fails != 0;
}
