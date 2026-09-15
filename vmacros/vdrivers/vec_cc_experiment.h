#include "semantics/option/option_defn.h"
DEFINE_OPTION_STRUCT(int)
DEFINE_OPTION_FUNCTIONS(static inline, int)

#include "semantics/error.h"
#include "semantics/result/result_defn.h"
DEFINE_RESULT_TYPEDEF(bool, Error)
DEFINE_RESULT_STRUCT(bool, Error)
DEFINE_RESULT_FUNCTIONS(static inline, bool, Error)
#define CANON_RESULT_BOOL_ERROR_DEFINED

#include "data/vec/vec_defn.h"
DEFINE_VEC_STRUCTS(int)
DEFINE_VEC_FUNCTIONS(static inline, int)
