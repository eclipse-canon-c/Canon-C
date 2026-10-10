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

/* VERIFY-039 G4, listing entry 4 -- docs/deviations.md: diag_render into a
 * buffer that covers the Diag's depth.
 *
 * diag_render requires buf to be valid for buf_size bytes and the Diag to
 * be readable; until the G4 diag repair it did not separate the two. Since
 * the repair it requires buf + (0 .. buf_size - 1) to be separated from
 * *d, so the call below is outside its contract; its run-time behaviour is
 * unchanged. It writes the rendering
 * through `char *` (snprintf, under the trusted axiom whose assigns is
 * ((char *)buf)[0 .. size - 1]). In WP's typed model a `char` write cannot
 * reach d->depth, a usize, so the loop invariant render_i_bounds,
 * 0 <= i <= d->depth, proves preserved.
 *
 * Here the Diag holds two frames, and buf points into it at the last
 * frame's message, unused, and runs to the end of depth; it is derived from
 * (char *)&d, so it ranges over the Diag's bytes. Frame 0's message is
 * sized so that its rendered line is exactly as long as the distance from
 * buf to depth: snprintf's terminating '\0' lands on depth's first byte,
 * its low byte on the project's little-endian targets, and depth goes from
 * 2 to 0. The loop has run once, so at its next head i is 1 and depth is
 * 0: the invariant is false there, and the loop exits after one frame.
 * Every access is defined C: the Diag is a declared object, every write
 * into it from buf is a character-type write, and each later read of depth
 * is through its declared type.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG -I. \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_diag_render.c -o p && ./p
 * Expected: one frame rendered of two, depth 0 after the call, the
 * invariant VIOLATED at the loop's second head; diag_render's own
 * postcondition holds; exit 1, sanitizers clean. The same without the two
 * defines (the default build).
 */
#define CANON_CONTRACT_IMPL
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "semantics/diag.h"

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
    size_t buf_size = to_depth + sizeof d.depth;
    char   msg[DIAG_MAX_MSG_LEN];
    int    fixed = snprintf(NULL, 0, fmt, (size_t)0, "f.c", (size_t)1, "f", 1, "");
    size_t m;
    usize  total;
    int    bad = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (fixed < 0 || (size_t)fixed >= to_depth) { return 2; }
    m = to_depth - (size_t)fixed;            /* frame 0's line is to_depth long */
    if (m == 0u || m > DIAG_MAX_MSG_LEN - 1u) { return 3; }
    memset(msg, 'x', m);
    msg[m] = '\0';
    (void)diag_push(&d, "f.c", 1u, "f", (Error)1, msg);
    (void)diag_push(&d, "g.c", 2u, "g", (Error)2, "second");

    printf("buf = frames[%zu].message, %zu bytes to depth, buf_size %zu; depth %zu\n",
           (size_t)(DIAG_MAX_FRAMES - 1u), to_depth, buf_size, (size_t)d.depth);
    total = diag_render(&d, buf, buf_size);
    printf("diag_render returned %zu (frame 0's line is %zu: %s); depth is now %zu\n",
           (size_t)total, to_depth,
           (size_t)total == to_depth ? "one iteration ran" : "NOT as placed",
           (size_t)d.depth);
    if ((size_t)total != to_depth) { return 4; }
    bad += say("loop invariant render_i_bounds at the second head (i = 1)",
               1u <= d.depth);
    bad += say("ensures r_terminated: a '\\0' in buf[0 .. buf_size - 1]",
               memchr(buf, '\0', buf_size) != NULL);
    return bad ? 1 : 0;
}
