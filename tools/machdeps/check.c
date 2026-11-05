/* tools/machdeps/check.c — what Frama-C's front end sees at a machine model
 * (VERIFY-060, phase A). check.sh reads it at each model with -constfold
 * and prints every probe_ variable: the sizes and alignments the model
 * gives, and which compiler macros the preprocessing defines. */

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

enum check_small { CHECK_S0, CHECK_S1 };              /* a 1-byte short enum */
enum check_error { CHECK_E0 = 0, CHECK_E402 = 402 };  /* the range of Error  */
struct check_ll  { char c; long long x; };
struct check_dbl { char c; double x; };
struct check_ptr { char c; void *x; };

long probe_sizeof_short      = (long)sizeof(short);
long probe_sizeof_int        = (long)sizeof(int);
long probe_sizeof_long       = (long)sizeof(long);
long probe_sizeof_longlong   = (long)sizeof(long long);
long probe_sizeof_ptr        = (long)sizeof(void *);
long probe_sizeof_size_t     = (long)sizeof(size_t);
long probe_sizeof_uintptr    = (long)sizeof(uintptr_t);
long probe_sizeof_ldouble    = (long)sizeof(long double);
long probe_sizeof_enum_small = (long)sizeof(enum check_small);
long probe_sizeof_enum_error = (long)sizeof(enum check_error);
long probe_align_longlong    = (long)offsetof(struct check_ll, x);
long probe_align_double      = (long)offsetof(struct check_dbl, x);
long probe_align_ptr         = (long)offsetof(struct check_ptr, x);
long probe_char_min          = (long)CHAR_MIN;
int32_t   probe_type_int32   = 0;   /* check.sh prints the typedefs of these */
uint32_t  probe_type_uint32  = 0;
intptr_t  probe_type_intptr  = 0;
ptrdiff_t probe_type_ptrdiff = 0;

#if defined(__FRAMAC__)
int probe_macro_framac = 1;
#endif
#if defined(__x86_64__)
int probe_macro_x86_64 = 1;
#endif
#if defined(__i386__)
int probe_macro_i386 = 1;
#endif
#if defined(__arm__)
int probe_macro_arm = 1;
#endif
#if defined(__riscv)
int probe_macro_riscv = 1;
#endif
#if defined(__riscv_xlen)
int probe_macro_riscv_xlen = __riscv_xlen;
#endif
#if defined(__CHAR_UNSIGNED__)
int probe_macro_char_unsigned = 1;
#endif
#if defined(__SIZEOF_LONG__)
int probe_macro_sizeof_long = __SIZEOF_LONG__;
#endif
#if defined(__SIZEOF_POINTER__)
int probe_macro_sizeof_pointer = __SIZEOF_POINTER__;
#endif
#if defined(__GNUC__)
int probe_macro_gnuc = __GNUC__;
#endif
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
int probe_macro_big_endian = 1;
#endif
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
int probe_macro_little_endian = 1;
#endif
#if defined(__STDC_VERSION__)
long probe_macro_stdc_version = __STDC_VERSION__;
#endif
