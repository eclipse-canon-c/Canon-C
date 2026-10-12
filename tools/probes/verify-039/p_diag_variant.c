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
 * group A: a goal of diag_render the entry missed.
 *
 * Entry 4 named diag_render's loop invariant render_i_bounds,
 * 0 <= i <= d->depth (p_diag_render.c, where the rendering lowers depth).
 * The loop variant, d->depth - i, reads depth after the same `char` writes,
 * and its decrease (loop_variant_decrease, proved in frama-c-diag) is false
 * when the rendering raises depth instead. Until the G4 diag repair,
 * diag_render did not separate buf from *d. Since the repair it does, so
 * the call below is outside its contract; its run-time behaviour is
 * unchanged.
 *
 * Here buf runs from the last frame's message, unused, to the second byte
 * of depth. It is derived from (char *)&d, so it ranges over the Diag's
 * bytes. Frame 0's message ends in the byte 0x05, placed so that snprintf,
 * truncating the line at buf_size, writes it on depth's low byte, on the
 * project's little-endian targets, and its terminating '\0' on the next.
 * So depth goes from 2 to 5, and the variant from 2 at the first head to
 * 4 at the second. The loop then renders frames 1 to 4 with nothing left
 * to write (rem 0) and exits: frames 2 to 4 are diag_init's zeroed frames,
 * rendered as "?" and "(no message)". render_i_bounds holds throughout.
 * Every access is defined C: the Diag is a declared object, every write
 * into it from buf is a character-type write, and each later read of depth
 * is through its declared type.
 *
 * The probe sees depth at each loop head through snprintf, which it
 * replaces before including diag.h: diag_render calls it after the head
 * and before any write.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_diag_variant.c -o p && ./p
 * Expected: five iterations, depth 2 at the first head and 5 after; the
 * variant 2 then 4, its decrease VIOLATED at the first back edge;
 * render_i_bounds holds at every head; diag_render's own postcondition
 * holds; exit 1, sanitizers clean. The same without the two defines (the
 * default build).
 */
#define CANON_CONTRACT_IMPL
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* depth at each loop head, as diag_render's snprintf sees it. */
static const size_t* watched_depth;
static size_t depth_at[16];
static int heads;

static int probe_snprintf(char* s, size_t n, const char* f, ...) {
    va_list ap;
    int r;
    if (watched_depth != NULL && heads < 16) { depth_at[heads++] = *watched_depth; }
    va_start(ap, f);
    r = vsnprintf(s, n, f, ap);
    va_end(ap);
    return r;
}
#undef snprintf                 /* a libc may define it as a macro */
#define snprintf probe_snprintf
#include "semantics/diag.h"
#undef snprintf

static int say(const char* what, int holds) {
    printf("    %-58s %s\n", what, holds ? "holds" : "VIOLATED");
    return holds ? 0 : 1;
}

int main(void) {
    static const char fmt[] = "[%zu] %s:%zu in %s() -- error %d: \"%s\"\n";
    Diag   d = diag_init();
    /* A pointer to the Diag's bytes (C11 6.3.2.3p7), at the last frame's
     * message; one derived from the message member could not go past it. */
    size_t at = offsetof(Diag, frames) +
                (DIAG_MAX_FRAMES - 1u) * sizeof(DiagFrame) +
                offsetof(DiagFrame, message);
    char*  buf = (char*)&d + at;
    size_t to_depth = offsetof(Diag, depth) - at;
    size_t buf_size = to_depth + 2u;          /* to depth's second byte */
    char   msg[DIAG_MAX_MSG_LEN];
    int    fixed = snprintf(NULL, 0, fmt, (size_t)0, "f.c", (size_t)1, "f", 1, "");
    size_t p, m;
    int    k, bad = 0, ok = 1;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (fixed < 2) { return 2; }
    p = (size_t)fixed - 2u;                   /* where frame 0's message starts */
    if (to_depth < p) { return 3; }
    m = to_depth - p + 1u;                    /* its last byte on depth's low byte */
    if (m > DIAG_MAX_MSG_LEN - 1u) { return 4; }
    memset(msg, 'x', m - 1u);
    msg[m - 1u] = '\x05';
    msg[m] = '\0';
    (void)diag_push(&d, "f.c", 1u, "f", (Error)1, msg);
    (void)diag_push(&d, "g.c", 2u, "g", (Error)2, "second");

    printf("buf = frames[%zu].message, %zu bytes to depth, buf_size %zu; depth %zu\n",
           (size_t)(DIAG_MAX_FRAMES - 1u), to_depth, buf_size, (size_t)d.depth);
    watched_depth = &d.depth;
    (void)diag_render(&d, buf, buf_size);
    watched_depth = NULL;
    printf("%d iterations; depth is now %zu\n", heads, (size_t)d.depth);
    for (k = 0; k < heads; k++) {
        printf("    head %d: depth %zu, variant depth - i = %ld\n", k,
               depth_at[k], (long)depth_at[k] - k);
    }
    if (heads != 5 || depth_at[0] != 2u || depth_at[1] != 5u) {
        printf("NOT as placed\n");
        return 5;
    }
    bad += say("loop variant decrease at the first back edge (2 then 4)",
               (long)depth_at[1] - 1 < (long)depth_at[0]);
    for (k = 0; k < heads; k++) { ok = ok && ((size_t)k <= depth_at[k]); }
    bad += say("loop invariant render_i_bounds at every head", ok);
    bad += say("ensures r_terminated: a '\\0' in buf[0 .. buf_size - 1]",
               memchr(buf, '\0', buf_size) != NULL);
    return bad ? 1 : 0;
}
