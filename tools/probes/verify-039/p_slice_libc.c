/*
 * VERIFY-039 G17 probe: slice.h's calls into libc, read against the
 * repaired contracts.
 *
 * Frama-C's libc gives memcmp
 *     requires valid_s1: valid_read_or_empty(s1, n);
 *     requires valid_s2: valid_read_or_empty(s2, n);
 * with valid_read_or_empty(s, n) = (empty_block(s) || \valid_read((char *)s))
 * && \valid_read(((char *)s) + (1 .. n - 1)): even for n == 0 each pointer
 * must point into a block, readable or empty. It gives strlen
 *     requires valid_string_s: valid_read_string(s);
 *
 * Until G17 bytes_equal and str_equal reached memcmp with a zero length
 * when both views were empty, non-null and distinct, and their contracts
 * (bytes_valid_write, str_valid) constrain the pointers of an empty view
 * in nothing but that pair: memcmp's valid_s1 was invalid from zero_bytes
 * and zero_str below. str_from_cstr, with no precondition, called strlen on
 * any non-null argument: strlen's valid_string_s was invalid from
 * cstr_unterminated. Since the repair both equality functions return at a
 * zero length before memcmp, and str_from_cstr requires null_or_string,
 * cstr == \null || valid_read_string(cstr).
 *
 * Entry points, for Eva, one at a time (-main):
 *   zero_bytes, zero_str  two empty views into one array, one at its end
 *                         and one at its start: every precondition valid,
 *                         memcmp not reached
 *   zero_bytes_ok         a control, two empty views over readable bytes:
 *                         every precondition valid
 *   cstr_unterminated     an array with no null character: str_from_cstr's
 *                         null_or_string invalid at the call, which the
 *                         contract now excludes; strlen not reached
 *   cstr_ok               a control, a terminated array: both valid
 * No alarm is raised from any of them (VERIFY-039 G17).
 *
 * From the repository root, for one entry point:
 *     frama-c -c11 -eva -main zero_bytes \
 *       -cpp-extra-args="-I core/primitives -I core -I . \
 *                        -DCANON_NO_REQUIRE -DNDEBUG" \
 *       tools/probes/verify-039/p_slice_libc.c
 *
 * main() runs cstr_unterminated, a call outside str_from_cstr's contract
 * since G17. Built with -fsanitize=address at -O0, by gcc or clang, it
 * stops in strlen, called from str_from_cstr, on the read past the array.
 * At -O2 gcc warns of the call at compile time (-Wstringop-overread,
 * "argument missing terminating nul") and stops at the same read, and
 * clang folds the call to a constant. Without a sanitizer, or where the
 * call is folded, the output means nothing: the read is undefined.
 */
#define CANON_CONTRACT_IMPL
#include <stdbool.h>
#include <stdio.h>
#include "core/slice.h"

int zero_bytes(void)
{
    static u8 buf[4] = { 1u, 2u, 3u, 4u };
    return (int)bytes_equal(bytes_from(buf + 4, 0u), bytes_from(buf, 0u));
}

int zero_str(void)
{
    static const char buf[4] = { 'a', 'b', 'c', 'd' };
    return (int)str_equal(str_from(buf + 4, 0u), str_from(buf, 0u));
}

int zero_bytes_ok(void)
{
    static u8 x[1] = { 0u };
    static u8 y[1] = { 0u };
    return (int)bytes_equal(bytes_from(x, 0u), bytes_from(y, 0u));
}

usize cstr_unterminated(void)
{
    static const char buf[3] = { 'a', 'b', 'c' };
    return str_from_cstr(buf).len;
}

usize cstr_ok(void)
{
    static const char buf[4] = { 'a', 'b', 'c', '\0' };
    return str_from_cstr(buf).len;
}

int main(void)
{
    printf("%lu\n", (unsigned long)cstr_unterminated());
    return 0;
}
