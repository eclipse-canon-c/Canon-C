// block: /* WITH Canon-C — one cleanup block, every exit routed through it */
/*@DIRECTIVES@*/
#include "my_types.h"
bool verify_crc(const void* buf);   /* the program's own */
void apply(const void* buf);        /* the program's own */
/*@FILE@*/
