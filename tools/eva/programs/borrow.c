/*
 * borrow.h: borrows of a pointer, a string and bytes from their owners:
 * made, read, checked, compared and sliced, with and without a lifetime.
 */
#define CANON_CONTRACT_IMPL
#include "semantics/borrow.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    static int owner;
    static int value = 7;
    static const char text[] = "borrowed text";
    static u8 bytes[8] = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };

    borrowed_ptr p = borrowed_ptr_from(&value, &owner);
    borrowed_ptr q = borrowed_ptr_from_lifetime(&value, NULL, &owner);
    CHECK(borrowed_ptr_is_valid(&p) && borrowed_ptr_get(&p) == &value);
    CHECK(borrowed_ptr_eq(p, q));
    borrowed_ptr none = borrowed_ptr_null();
    CHECK(!borrowed_ptr_is_valid(&none) && borrowed_ptr_get(&none) == NULL);

    borrowed_str s = borrowed_str_from_cstr(text, &owner);
    borrowed_str t = borrowed_str_from(str_from(text, 8u), &owner);
    borrowed_str u = borrowed_str_from_lifetime(str_from(text, 8u), NULL, &owner);
    CHECK(borrowed_str_is_valid(&s) && borrowed_str_len(&s) == 13u);
    CHECK(borrowed_str_eq(t, u) && !borrowed_str_eq(s, t));
    borrowed_str word = borrowed_str_slice(s, 9u, 13u);
    CHECK(borrowed_str_len(&word) == 4u && borrowed_str_get(&word).ptr[0] == 't');
    borrowed_str empty = borrowed_str_empty();
    CHECK(borrowed_str_len(&empty) == 0u);

    borrowed_bytes b = borrowed_bytes_from(bytes, sizeof bytes, &owner);
    borrowed_bytes c = borrowed_bytes_from_cbytes(cbytes_from(bytes, sizeof bytes), &owner);
    borrowed_bytes d = borrowed_bytes_from_lifetime(bytes, 4u, NULL, &owner);
    CHECK(borrowed_bytes_is_valid(&b) && borrowed_bytes_len(&b) == 8u);
    CHECK(borrowed_bytes_eq(b, c) && !borrowed_bytes_eq(b, d));
    borrowed_bytes mid = borrowed_bytes_slice(b, 2u, 6u);
    CHECK(borrowed_bytes_len(&mid) == 4u && borrowed_bytes_get(&mid).ptr[0] == 3u);
    borrowed_bytes nob = borrowed_bytes_empty();
    CHECK(borrowed_bytes_len(&nob) == 0u);

    END(fails);
    return fails != 0;
}
