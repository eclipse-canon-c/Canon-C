// block: /* WITH Canon-C — failures are values, propagation is explicit */
// split: /* ... caller, 1000 lines away ... */
/*@DIRECTIVES@*/
#include "my_types.h"
typedef struct { int v[8]; usize len; } vec_int;   /* the program's own */
result_int_Error collect(Arena* arena, const char** inputs, usize count, vec_int* out);
result_int_Error validate(const vec_int* v);
int sum(const vec_int* v);
/*@FILE@*/
/* The caller's statements, which the block shows at file scope; its
   arena, inputs and count are its own. */
void readme_example_(Arena arena, const char** inputs, usize count);
void readme_example_(Arena arena, const char** inputs, usize count)
{
/*@BODY@*/
}
