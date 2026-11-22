// block: /* WITH Canon-C — preconditions are visible, grep-able, self-documenting */
// flags: -Wno-return-type
//   process() leaves the rest of its body, and its return, to "...".
/*@DIRECTIVES@*/
#include "my_types.h"
typedef struct Config Config;   /* the program's own */
/*@FILE@*/
