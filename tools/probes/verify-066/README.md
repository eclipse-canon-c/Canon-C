# VERIFY-066's probe: Frama-C 29's error in Eva

Frama-C 29's Eva stopped on entering `core/arena.h`'s `arena_alloc`, in
every program that called it, with an uncaught exception,
`Eva__Eval_terms.Reduce_to_bottom` (exit 125), raised while it decides which
of the function's behaviours is active. Five of the 26 programs of
`tools/eva/` stopped there, at every machine model. Frama-C 25 runs them to
their end. The behaviours `fits` and `does_not_fit` assume `arena_can_fit`,
which bound with `\let` the address `a->buffer + a->offset` converted to an
integer, and a pad computed from it; Frama-C 28 began to evaluate `\let`.
The probe's first run (`9 · Probes` #14, on 2b38ddb) found that pair to be
what raises the error: every one of its thirteen predictions matched
(VERIFY-066, "Scored: Eva's error probe"). Since VERIFY-067,
`core/arena.h` writes `arena_can_fit`'s terms out, with no `\let`; the
small programs keep watching the error in Frama-C, and `a1_let` on
Canon-C's own code.

`eva_error_probe.py` runs Eva on:

| variant | what | how |
|---------|------|-----|
| `r0_full` | `arena_alloc` standalone: the Arena, its invariant, `arena_can_fit` as written, the three behaviours, a body | `frama-c -eva r0_full.c` |
| `r1_min` | its smallest form: the address bound by `\let`, a pad, one conjunct, two complementary behaviours | `frama-c -eva r1_min.c` |
| `r2_nolet` | `r1_min` without `\let`, its terms written out | as `r1_min` |
| `r3_nocast` | `r1_min` with the pad computed from the offset: no address converted | as `r1_min` |
| `r4_onebhv` | `r1_min` with one behaviour: no negation, no `complete` or `disjoint` | as `r1_min` |
| `r5_nobody` | `r1_min` with the function declared, not defined | as `r1_min` |
| `a0_control` | `tools/eva/programs/arena.c` as it is (since VERIFY-067, `arena_can_fit` with no `\let`) | 14 · Eva's command at x86_64 (`tools/eva/eva.py`) |
| `a1_let` | the same, with `arena_can_fit`'s three `\let` as before VERIFY-067, in a copy of `core/arena.h`; until VERIFY-067 this variant was `a1_inline`, the `\let` written out in the copy | as `a0_control`, in the copy |
| `a2_usespec` | the same, with `arena_alloc` read by its contract | as `a0_control`, with `-eva-use-spec arena_alloc` |

For each it prints whether Eva finishes or stops, and for a run that stops,
the end of Frama-C's output (OCaml's backtrace) and the last call Eva names
(`-eva-show-progress`). It runs in `9 · Probes` (`probes.yml`,
`evaerror-probe`), report-only. Its predictions and their scores are
VERIFY-066's and VERIFY-067's, in `docs/deviations.md`.

    python3 tools/probes/verify-066/eva_error_probe.py

Each small program compiles with `gcc -std=c11 -Wall -Wextra -Werror` and,
but for `r5_nobody`, runs and returns 0.
