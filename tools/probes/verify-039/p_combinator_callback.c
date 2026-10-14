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

/* VERIFY-039 G11 -- docs/deviations.md: the combinators' contracts claim,
 * for every callback, what only some callbacks give.
 *
 * option_int_map's contract (vmacros/vdrivers/option_verify.h, and the
 * same clauses in DEFINE_OPTION_MAP's macro body) stated `assigns \nothing`
 * and required nothing of f; it had no terminates or exits clause, so the
 * defaults claimed that every call terminates and none exits.
 * result_int_VErr_map's (vmacros/vdrivers/result_verify.h, and
 * DEFINE_RESULT_MAP's macro body) was the same. Each call below meets
 * every precondition those contracts stated, and each callback is
 * defined C:
 *
 *   frame       f adds 1 to a global; after the call the global has
 *               changed: `assigns \nothing` VIOLATED, for both maps;
 *   exit        f calls exit(3), in a child process; the child ends with
 *               status 3 inside the call: no exit VIOLATED;
 *   termination f loops on a volatile counter, in a child process that
 *               SIGALRM stops after 1 s; the call has not returned, and
 *               with this f it never does: termination VIOLATED;
 *   client      client() below calls option_int_map with the first f. WP
 *               proved its `ensures g == 0` from map's contract, and at
 *               run time g is 1: a client's proof made false by the
 *               callee's frame. WP 25, run from the repository root:
 *                 frama-c -c11 -wp -wp-rte -wp-fct client -wp-prover qed \
 *                   -cpp-extra-args="-I core/primitives -I core \
 *                     -I semantics -I . -DCANON_NO_REQUIRE -DNDEBUG" \
 *                   tools/probes/verify-039/p_combinator_callback.c
 *               proved all six of client's goals by Qed alone.
 *
 * Since G11's repair of option_verify.h and result_verify.h, and then of
 * DEFINE_OPTION_MAP's and DEFINE_RESULT_MAP's macro bodies, read under -CC,
 * their contracts make none of these claims: each requires the pointer
 * valid where it is called, claims termination and no exit only where
 * nothing is called, and states the empty frame only there. The run-time
 * behaviour is unchanged, and the same WP 25 command proves 4 of client's 8
 * goals: not its `ensures g == 0`, nor its frame at the call (normal and
 * exit path), nor the call's new precondition.
 *
 * The run-time check WP states at the call, \valid_function(f), has no
 * probe here: a call through a null pointer met every precondition until
 * G11's repair, and the call is itself the undefined step, as at G10.
 *
 * Exit status 1 when any claim is VIOLATED, which is the expected outcome
 * today.
 *
 * From the repository root:
 *   gcc -std=c99 -Wall -Wextra -DCANON_NO_REQUIRE -DNDEBUG \
 *       -I core/primitives -I core -I semantics -I . \
 *       -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       tools/probes/verify-039/p_combinator_callback.c -o p && ./p
 */

#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "vmacros/vdrivers/option_verify.h"
#include "vmacros/vdrivers/result_verify.h"

int g;
static volatile unsigned long spin;
static int failures;

static int bump(int x) { g = g + 1; return x; }
static int leave(int x) { exit(3); return x; }
static int loop_forever(int x) { for (;;) { spin = spin + 1u; } return x; }

/*@ requires g == 0;
    assigns g;
    ensures g == 0;
*/
void client(void) {
    option_int o = option_int_some(1);
    option_int r = option_int_map(o, bump);
    (void)r;
}

static void report(const char *claim, int violated) {
    printf("  %-58s %s\n", claim, violated ? "VIOLATED" : "holds");
    if (violated) { failures++; }
}

/* Runs option_int_map(some(1), f) in a child; returns its wait status. */
static int in_child(int (*f)(int), unsigned seconds) {
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(2); }
    if (pid == 0) {
        alarm(seconds);
        option_int r = option_int_map(option_int_some(1), f);
        _exit(r.has_value ? 0 : 1);   /* the call returned */
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); exit(2); }
    return status;
}

int main(void) {
    printf("G11: what the combinators' contracts claim for every callback\n");

    g = 0;
    (void)option_int_map(option_int_some(1), bump);
    report("option_int_map, f writes g: assigns \\nothing", g != 0);

    g = 0;
    (void)result_int_VErr_map(result_int_VErr_ok(1), bump);
    report("result_int_VErr_map, f writes g: assigns \\nothing", g != 0);

    int st = in_child(leave, 5);
    report("option_int_map, f calls exit(3): no exit",
           WIFEXITED(st) && WEXITSTATUS(st) == 3);

    st = in_child(loop_forever, 1);
    report("option_int_map, f loops: the call terminates",
           WIFSIGNALED(st) && WTERMSIG(st) == SIGALRM);

    g = 0;
    client();
    report("client(): ensures g == 0, proved by WP until G11", g != 0);

    return failures ? 1 : 0;
}
