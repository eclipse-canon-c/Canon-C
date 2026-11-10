/* error.h: every error code's message, range and number. */
#define CANON_CONTRACT_IMPL
#include "semantics/error.h"
#include "eva_program.h"

int main(void)
{
    int fails = 0;
    static const Error codes[] = {
        ERR_OK, ERR_INVALID_ARG, ERR_OUT_OF_RANGE, ERR_PARSE_FAILED,
        ERR_INVALID_FORMAT, ERR_INVALID_STATE, ERR_OUT_OF_MEMORY,
        ERR_BUFFER_TOO_SMALL, ERR_CAPACITY_EXCEEDED, ERR_NOT_FOUND,
        ERR_IO_FAILED, ERR_PERMISSION, ERR_TIMEOUT, ERR_NOT_SUPPORTED,
        ERR_ALREADY_EXISTS, ERR_OVERFLOW, ERR_UNDERFLOW, ERR_DIVIDE_BY_ZERO,
        ERR_UNKNOWN,
    };
    bool all = true;
    for (usize i = 0u; i < sizeof codes / sizeof codes[0]; i++) {
        const char *m = error_message(codes[i]);
        all = all && m != NULL && m[0] != '\0';
        all = all && error_in_range(codes[i]);
        all = all && error_is_ok(codes[i]) == (i == 0u);
        all = all && error_code(codes[i]) == (int)codes[i];
    }
    CHECK(all);
    CHECK(error_code(ERR_TIMEOUT) == 202);

    END(fails);
    return fails != 0;
}
