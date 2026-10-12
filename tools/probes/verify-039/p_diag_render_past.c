/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

/* VERIFY-039 G4, entry 4 -- docs/deviations.md, the proved-goal argument,
 * group B: a goal of diag_render the entry missed.
 *
 * diag_render's loop reads d->frames[i] while i < d->depth, depth read at
 * each head, after the earlier iterations' snprintf writes through
 * `char *`. WP's typed model takes depth to be unchanged by them. The
 * run-time check that frame i's message[0] is readable
 * (assert_rte_mem_access_4, proved in frama-c-diag, the first access to
 * the frame on the path) is false when the rendering raises depth past
 * DIAG_MAX_FRAMES: frames[8] would begin at depth, and its message lies
 * past the end of the Diag. Until the G4 diag repair, diag_render did not
 * separate buf from *d. Since the repair it does, so the call below is
 * outside its contract; its run-time behaviour is unchanged.
 *
 * Here buf is the 4 bytes before depth and depth's first two bytes
 * (buf_size 6), derived from (char *)&d. On LP64 the 4 bytes are frame 7's
 * tail padding; on ILP32 they end frame 7's message, whose first byte stays
 * '\0'. Frame 0's file name begins with a tab, 0x09, the fifth byte of its
 * line "[0] \t...". So snprintf, truncating at buf_size, writes 0x09 on
 * depth's low byte, on the project's little-endian targets, and its '\0' on
 * the next: depth goes from 2 to 9. Frames 1 to 7 render with nothing left
 * to write. At the head after frame 7, i is 8 and depth 9, so the body
 * would read frames[8].message[0], outside the Diag. The probe stops
 * before that: its snprintf, which replaces the library's, records each
 * call and, after frame 7's, returns to main through longjmp, so the
 * access is never made. Every access made is defined C: the Diag has
 * static storage, every write into it from buf is a character-type write,
 * and each read of depth is through its declared type.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_diag_render_past.c -o p && ./p
 * Expected: depth 2 before the call and 9 after frame 0; eight calls, for
 * frames 0 to 7; at the next head i = 8 < depth = 9, render_i_bounds
 * holding, and frames[8].message[0] past the Diag's end, so the check
 * VIOLATED; exit 1, sanitizers clean. The same without the two defines
 * (the default build).
 */
#define CANON_CONTRACT_IMPL
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* depth at each call of diag_render's snprintf, one call per frame. */
static const size_t* watched_depth;
static size_t depth_at[16];
static int calls;
static jmp_buf out;

/* depth is read before the write: the call comes after the loop head with
 * no write in between, so it is depth at that head. */
static int probe_snprintf(char* s, size_t n, const char* f, ...) {
    va_list ap;
    int r, watching = watched_depth != NULL && calls < 16;
    if (watching) { depth_at[calls++] = *watched_depth; }
    va_start(ap, f);
    r = vsnprintf(s, n, f, ap);
    va_end(ap);
    if (watching && calls == 8) { longjmp(out, 1); }   /* after frame 7's call */
    return r;
}
#undef snprintf                 /* a libc may define it as a macro */
#define snprintf probe_snprintf
#include "semantics/diag.h"
#undef snprintf

static Diag d;

static int say(const char* what, int holds) {
    printf("    %-62s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

int main(void) {
    size_t at       = offsetof(Diag, depth) - 4u;
    size_t buf_size = 6u;                     /* to depth's second byte */
    size_t past     = offsetof(Diag, frames) + DIAG_MAX_FRAMES * sizeof(DiagFrame)
                      + offsetof(DiagFrame, message);
    char*  buf;
    int    k, bad = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    d = diag_init();
    buf = (char*)&d + at;
    (void)diag_push(&d, "\tf.c", 1u, "f", (Error)1, "first");
    (void)diag_push(&d, "g.c", 2u, "g", (Error)2, "second");
    printf("buf = 4 bytes before depth, buf_size %zu; depth %zu; DIAG_MAX_FRAMES %zu\n",
           buf_size, (size_t)d.depth, (size_t)DIAG_MAX_FRAMES);

    watched_depth = &d.depth;
    if (setjmp(out) == 0) {
        (void)diag_render(&d, buf, buf_size);
        watched_depth = NULL;
        printf("diag_render returned before frame 7's call: NOT as placed\n");
        return 4;
    }
    watched_depth = NULL;
    for (k = 0; k < calls; k++) {
        printf("    head %d (frame %d's call): depth %zu\n", k, k, depth_at[k]);
    }
    if (calls != 8 || depth_at[0] != 2u || depth_at[1] != 9u || d.depth != 9u) {
        printf("NOT as placed\n");
        return 5;
    }
    printf("at the next head: i = 8, depth = %zu; frames[8].message[0] at offset %zu,"
           " the Diag %zu bytes\n", (size_t)d.depth, past, sizeof(Diag));
    printf("    the loop goes on there: 8 < depth is %s\n", 8u < d.depth ? "true" : "false");
    bad += say("loop invariant render_i_bounds at that head (8 <= depth)", 8u <= d.depth);
    bad += say("rte mem_access: frames[8].message[0] inside the Diag", past < sizeof(Diag));
    return bad ? 1 : 0;
}
