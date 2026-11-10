/*
 * slice.h: byte views of a buffer (indexing, slicing, comparing), and
 * string views of a C string (prefix, suffix, slicing, comparing).
 */
#define CANON_CONTRACT_IMPL
#include "core/slice.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    u8 buf[16];
    u8 other[16];
    for (usize i = 0u; i < sizeof buf; i++) {
        buf[i] = (u8)i;
        other[i] = (u8)i;
    }

    bytes_t all = bytes_from(buf, sizeof buf);
    bytes_t copy = bytes_from(other, sizeof other);
    CHECK(!bytes_is_empty(all) && bytes_is_empty(bytes_empty()));
    CHECK(bytes_equal(all, copy));
    u8 *fifth = bytes_at(all, 5u);
    CHECK(fifth != NULL && *fifth == 5u && bytes_at(all, 16u) == NULL);
    *fifth = 50u;
    CHECK(!bytes_equal(all, copy));

    bytes_t mid = bytes_slice(all, 4u, 8u);
    CHECK(mid.len == 4u && mid.ptr == buf + 4);
    CHECK(bytes_slice(all, 8u, 4u).len == 0u);
    bytes_t head = bytes_take(all, 3u);
    bytes_t tail = bytes_skip(all, 13u);
    CHECK(head.len == 3u && tail.len == 3u && tail.ptr[0] == 13u);
    CHECK(bytes_take(all, 99u).len == 16u && bytes_skip(all, 99u).len == 0u);

    cbytes_t ro = bytes_as_const(mid);
    cbytes_t ro2 = cbytes_from(other + 4, 4u);
    CHECK(ro.len == ro2.len && ro.ptr[0] == ro2.ptr[0]);
    CHECK(cbytes_empty().len == 0u);

    static const char text[] = "canon-c: views";
    str_t s = str_from_cstr(text);
    CHECK(s.len == 14u && !str_is_empty(s));
    CHECK(str_is_empty(str_from_cstr(NULL)) && str_is_empty(str_empty()));
    str_t name = str_from(text, 7u);
    static const char again[] = "canon-c";
    CHECK(str_equal(name, str_from_cstr(again)));
    CHECK(str_starts_with(s, name) && !str_starts_with(name, s));
    static const char suffix[] = "views";
    CHECK(str_ends_with(s, str_from_cstr(suffix)));
    str_t word = str_slice(s, 9u, 14u);
    CHECK(word.len == 5u && word.ptr[0] == 'v');
    CHECK(str_take(s, 5u).len == 5u && str_skip(s, 9u).len == 5u);
    cbytes_t raw = str_as_bytes(word);
    CHECK(raw.len == 5u && raw.ptr[4] == 's');

    END(fails);
    return fails != 0;
}
