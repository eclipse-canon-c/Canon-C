#include "semantics/result/result_defn.h"
typedef enum { VERR_A = 0, VERR_B = 1, VERR_C = 2 } VErr;
DEFINE_RESULT_TYPEDEF(int, VErr)
DEFINE_RESULT_STRUCT(int, VErr)
DEFINE_RESULT_FUNCTIONS(static inline, int, VErr)
