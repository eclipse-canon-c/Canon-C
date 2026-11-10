/*
 * option.h, instantiated for int as its proof instantiates it
 * (vmacros/vdrivers/option_verify.h): a lookup that may find nothing, and
 * every combinator on a value and on none.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/option_verify.h"
#include "eva_program.h"

static const int table[4] = { 10, 20, 30, 40 };

static option_int lookup(int i)
{
    return (i >= 0 && i < 4) ? option_int_some(table[i]) : option_int_none();
}
static int twice(int v)                { return v * 2; }
static option_int half(int v)          { return (v % 2 == 0) ? option_int_some(v / 2) : option_int_none(); }
static option_int fallback(void)       { return option_int_some(-1); }
static bool big(int v)                 { return v > 25; }
static int add(int a, int b)           { return a + b; }
static bool same(int a, int b)         { return a == b; }

int main(void)
{
    int fails = 0;
    option_int found = lookup(1);
    option_int missing = lookup(9);
    int v = 0;

    CHECK(option_int_is_some(found) && option_int_is_none(missing));
    CHECK(option_int_get(found, &v) && v == 20);
    CHECK(!option_int_get(missing, &v) && v == 20);
    CHECK(option_int_unwrap_or(missing, 5) == 5 && option_int_unwrap(found) == 20);
    CHECK(option_int_expect(found, "lookup(1) finds a value") == 20);
    CHECK(option_int_unwrap(option_int_map(found, twice)) == 40);
    CHECK(option_int_is_none(option_int_map(missing, twice)));
    CHECK(option_int_unwrap(option_int_and_then(found, half)) == 10);
    CHECK(option_int_is_none(option_int_and_then(missing, half)));
    CHECK(option_int_unwrap(option_int_or_else(missing, fallback)) == -1);
    CHECK(option_int_unwrap(option_int_or_else(found, fallback)) == 20);
    CHECK(option_int_is_none(option_int_filter(found, big)));
    CHECK(option_int_is_some(option_int_filter(lookup(2), big)));
    CHECK(option_int_unwrap(option_int_combine_with(found, lookup(2), add)) == 50);
    CHECK(option_int_is_none(option_int_combine_with(found, missing, add)));
    CHECK(option_int_eq(found, lookup(1), same) && !option_int_eq(found, missing, same));

    option_int slot = option_int_none();
    option_int old = option_int_replace(&slot, 7);
    CHECK(option_int_is_none(old) && option_int_unwrap(slot) == 7);
    option_int taken = option_int_take(&slot);
    CHECK(option_int_unwrap(taken) == 7 && option_int_is_none(slot));

    END(fails);
    return fails != 0;
}
