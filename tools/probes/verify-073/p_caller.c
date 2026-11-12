/*
 * p_caller.c -- VERIFY-073's caller: a program proved with WP alone that
 * pushes two frames and renders the chain, the use VERIFY-066 found that
 * diag_push's contract did not allow.
 *
 * WP proves caller() from diag.h's contracts, not from their bodies: each
 * push's preconditions, then diag_render's, of which r_strings asks every
 * frame renderable. The first frame's strings must survive the second
 * push. The strings are literals, as DIAG_PUSH's __FILE__ and __func__
 * are; WP knows a literal's bytes only with -wp-literals, and
 * diagpush_probe.py runs it both ways.
 */
#include "semantics/diag.h"

/*@
  requires caller_buf: \valid(buf + (0 .. 255));
  assigns buf[0 .. 255];
*/
usize caller(char *buf)
{
    Diag d = diag_init();
    (void)diag_push(&d, "a.c", 1u, "fa", ERR_IO_FAILED, "first");
    (void)diag_push(&d, "b.c", 2u, "fb", ERR_UNKNOWN, "second");
    return diag_render(&d, buf, 256u);
}
