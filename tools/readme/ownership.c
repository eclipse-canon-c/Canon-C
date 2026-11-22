// block: /* WITH Canon-C — intent is visible at every call site */
// split: /* ... 2000 lines later, still the same conventions ... */
/*@DIRECTIVES@*/
#include "my_types.h"
typedef struct Config Config;   /* the program's own */
/*@FILE@*/
/* The block's statements, which it shows at file scope. */
void readme_example_(void);
void readme_example_(void)
{
    u8 buf[64];                 /* the program's own */
/*@BODY@*/
    (void)name;                 /* its use is "..." */
}
