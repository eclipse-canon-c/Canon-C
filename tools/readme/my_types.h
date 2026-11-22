/* tools/readme/my_types.h (VERIFY-109): the Option and Result types the
 * README's examples assume, made once in a header of the program's own, as
 * the README's Usage makes its own. */
#ifndef MY_TYPES_H
#define MY_TYPES_H
#include "semantics/error.h"
#include "semantics/option/option.h"
#include "semantics/result/result.h"

CANON_OPTION(int)
CANON_RESULT(int, Error)
CANON_RESULT(usize, Error)

#endif
