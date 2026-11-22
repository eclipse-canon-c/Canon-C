// block: /* WITH Canon-C — borrow validity is assertable at any point */
/*@DIRECTIVES@*/
#include "my_types.h"
/* The block's statements, which it shows at file scope. */
void readme_example_(void);
void readme_example_(void)
{
/*@BODY@*/
    (void)name;                 /* its use is "..." */
}
