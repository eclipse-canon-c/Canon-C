/* vmacros/vdrivers/vec_struct_cc_experiment.h — VERIFY-037.
 *
 * vec and option instantiated at a struct type, verified under cpp -CC so the
 * macro-body contracts (VERIFY-025) apply unchanged: the same contract text as
 * the int instantiation of vec_cc_experiment.h, at a record element. Element
 * equality in those contracts becomes structural equality, element assignment
 * becomes a struct copy, and the element size becomes 16 bytes.
 *
 * Purpose: extend the catalogue n(P) is measured against (tools/idioms), so a
 * program that stores records in a vec is not outside the substrate's idioms
 * merely because vec was only ever verified at int. The predictions are in
 * VERIFY-037 and in cc_pins/vec_struct.txt, committed before this file.
 */
#include "core/primitives/types.h"
typedef struct { u32 id; i64 due; } Rec;

#include "semantics/option/option_defn.h"
DEFINE_OPTION_STRUCT(Rec)
DEFINE_OPTION_FUNCTIONS(static inline, Rec)

#include "semantics/error.h"
#include "semantics/result/result_defn.h"
DEFINE_RESULT_TYPEDEF(bool, Error)
DEFINE_RESULT_STRUCT(bool, Error)
DEFINE_RESULT_FUNCTIONS(static inline, bool, Error)
#define CANON_RESULT_BOOL_ERROR_DEFINED

#include "data/vec/vec_defn.h"
DEFINE_VEC_STRUCTS(Rec)
DEFINE_VEC_FUNCTIONS(static inline, Rec)
