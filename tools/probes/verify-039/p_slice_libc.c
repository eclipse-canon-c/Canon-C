/*
 * VERIFY-039 G17 probe: two calls that meet every precondition slice.h
 * states and break one that the libc function it calls states.
 *
 * Frama-C's libc gives memcmp
 *     requires valid_s1: valid_read_or_empty(s1, n);
 *     requires valid_s2: valid_read_or_empty(s2, n);
 * with valid_read_or_empty(s, n) = (empty_block(s) || \valid_read((char *)s))
 * && \valid_read(((char *)s) + (1 .. n - 1)): even for n == 0 each pointer
 * must point into a block, readable or empty. bytes_equal and str_equal
 * reach memcmp with a zero length when both views are empty, non-null and
 * distinct, and their contracts (bytes_valid_write, str_valid) constrain
 * the pointers of an empty view in nothing but that pair. It gives strlen
 *     requires valid_string_s: valid_read_string(s);
 * and str_from_cstr, which calls strlen on any non-null argument, has no
 * precondition at all; its comment asks for a null-terminated string.
 *
 * Entry points, for Eva, one at a time (-main):
 *   zero_bytes, zero_str  two empty views into one array, one at its end
 *                         and one at its start: memcmp's valid_s1 invalid
 *   zero_bytes_ok         a control, two empty views over readable bytes
 *   cstr_unterminated     an array with no null character: strlen's
 *                         valid_string_s invalid
 *   cstr_ok               a control, a terminated array
 * Every other precondition Eva evaluates on these paths, slice.h's
 * included, is valid, and no alarm is raised (VERIFY-039 G17).
 *
 * From the repository root, for one entry point:
 *     frama-c -c11 -eva -main zero_bytes \
 *       -cpp-extra-args="-I core/primitives -I core -I . \
 *                        -DCANON_NO_REQUIRE -DNDEBUG" \
 *       tools/probes/verify-039/p_slice_libc.c
 *
 * main() runs cstr_unterminated. Built with -fsanitize=address at -O0, by
 * gcc or clang, it stops in strlen, called from str_from_cstr, on the read
 * past the array. At -O2 gcc warns of the call at compile time
 * (-Wstringop-overread, "argument missing terminating nul") and stops at
 * the same read, and clang folds the call to a constant. Without a
 * sanitizer, or where the call is folded, the output means nothing: the
 * read is undefined. No run-time check applies to the zero-length memcmp,
 * which reads nothing.
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
