/* VERIFY-041 probe of the run-2 vec_range.h (F1 and F2 fixed); it compiles
 * against the committed header too, and shows the defects there. Build as
 * probe_committed.c, without arguments. */
#define CANON_CONTRACT_IMPL
#include "semantics/option/option.h"
#include "data/vec/vec.h"
#include "data/vec/vec_range.h"
#include <stdio.h>
CANON_OPTION(int)
DEFINE_VEC(static inline, int)
DEFINE_VEC_RANGE(static inline, int)
CANON_OPTION(u64)
DEFINE_VEC(static inline, u64)
DEFINE_VEC_RANGE(static inline, u64)
static void show(const char* t, result__Bool_Error r, vec_int* v){ printf("%-28s ok=%d err=%d len=%zu :", t, r.is_ok, r.is_ok?0:(int)r.val.err, (size_t)v->len); for(usize i=0;i<v->len;i++) printf(" %d", v->items[i]); printf("\n"); }
int main(void){
  int buf[8]; vec_int v = vec_int_init(buf, 8);
  show("f1 wrap up", vec_int_extend_from_range(&v, range_make((isize)2147483646, (isize)2147483649LL, 1)), &v);
  show("basic 0..3", vec_int_extend_from_range(&v, range_make(0, 3, 1)), &v);
  show("f1 wrap down", vec_int_extend_from_range(&v, range_make((isize)-2147483647LL, (isize)-2147483650LL, -1)), &v);
  range bad = { 0, -10, CANON_ISIZE_MIN }; show("f2 ISIZE_MIN step", vec_int_extend_from_range(&v, bad), &v);
  range z = { 0, 10, 0 }; show("step 0", vec_int_extend_from_range(&v, z), &v);
  show("desc 5..1 step -2", vec_int_extend_from_range(&v, range_make(5, 0, -2)), &v);
  u64 ub[4]; vec_u64 u = vec_u64_init(ub, 4);
  result__Bool_Error r = vec_u64_extend_from_range(&u, range_make(-1, 2, 1)); printf("u64 -1..1 ok=%d err=%d len=%zu\n", r.is_ok, r.is_ok?0:(int)r.val.err, (size_t)u.len);
  r = vec_u64_extend_from_range(&u, range_make(0, 3, 1)); printf("u64 0..2 ok=%d len=%zu %llu %llu %llu\n", r.is_ok, (size_t)u.len, (unsigned long long)ub[0],(unsigned long long)ub[1],(unsigned long long)ub[2]);
  printf("ERR_INVALID_ARG=%d ERR_OUT_OF_RANGE=%d\n", (int)ERR_INVALID_ARG, (int)ERR_OUT_OF_RANGE);
  return 0; }
