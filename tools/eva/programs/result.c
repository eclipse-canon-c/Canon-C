/*
 * result.h, instantiated for int and an error enum as its proof
 * instantiates it (vmacros/vdrivers/result_verify.h): a parse that may
 * fail, and every combinator on a value and on an error.
 */
#define CANON_CONTRACT_IMPL
#include "vmacros/vdrivers/result_verify.h"
#include "eva_program.h"

static result_int_VErr parse_digit(char c)
{
    return (c >= '0' && c <= '9') ? result_int_VErr_ok(c - '0') : result_int_VErr_err(VERR_B);
}
static int twice(int v)                     { return v * 2; }
static VErr worse(VErr e)                   { return e == VERR_A ? VERR_B : VERR_C; }
static result_int_VErr nonzero(int v)       { return v != 0 ? result_int_VErr_ok(v) : result_int_VErr_err(VERR_A); }
static result_int_VErr recover(VErr e)      { return e == VERR_B ? result_int_VErr_ok(0) : result_int_VErr_err(e); }
static bool same_int(int a, int b)          { return a == b; }
static bool same_err(VErr a, VErr b)        { return a == b; }

int main(void)
{
    int fails = 0;
    result_int_VErr seven = parse_digit('7');
    result_int_VErr bad = parse_digit('x');
    int v = 0;
    VErr e = VERR_A;

    CHECK(result_int_VErr_is_ok(seven) && result_int_VErr_is_err(bad));
    CHECK(result_int_VErr_get_ok(seven, &v) && v == 7);
    CHECK(!result_int_VErr_get_ok(bad, &v) && v == 7);
    CHECK(result_int_VErr_get_err(bad, &e) && e == VERR_B);
    CHECK(!result_int_VErr_get_err(seven, &e));
    CHECK(result_int_VErr_unwrap_or(bad, -1) == -1 && result_int_VErr_unwrap(seven) == 7);
    CHECK(result_int_VErr_unwrap_err(bad) == VERR_B);
    CHECK(result_int_VErr_expect(seven, "'7' is a digit") == 7);
    CHECK(result_int_VErr_unwrap(result_int_VErr_map(seven, twice)) == 14);
    CHECK(result_int_VErr_unwrap_err(result_int_VErr_map_err(bad, worse)) == VERR_C);
    CHECK(result_int_VErr_unwrap(result_int_VErr_and_then(seven, nonzero)) == 7);
    CHECK(result_int_VErr_unwrap_err(result_int_VErr_and_then(parse_digit('0'), nonzero)) == VERR_A);
    CHECK(result_int_VErr_unwrap(result_int_VErr_or_else(bad, recover)) == 0);
    CHECK(result_int_VErr_is_err(result_int_VErr_and(bad, seven)));
    CHECK(result_int_VErr_unwrap(result_int_VErr_and(seven, parse_digit('3'))) == 3);
    CHECK(result_int_VErr_unwrap(result_int_VErr_or(bad, seven)) == 7);
    CHECK(result_int_VErr_eq(seven, parse_digit('7'), same_int, same_err));
    CHECK(!result_int_VErr_eq(seven, bad, same_int, same_err));

    END(fails);
    return fails != 0;
}
