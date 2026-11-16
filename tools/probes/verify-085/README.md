# VERIFY-085's probe: the call-order rules under WP

Closed at VERIFY-085's close: its file `steps` names no part. With the
buffer the hashmap programs pass declared as bytes, WP leaves unproved
the call `hashmap_stale` makes after the map changed, as Eva refutes it,
and proves Aorai's other properties of `hashmap_ok`'s `main` but one, a
call the rule allows in a loop whose invariants Aorai writes of the state
alone (`9 · Probes` #34). The check of the rules is Eva's, `14 · Eva`'s
job `aorai`; a WP check would be a record of its own. Below, the probe as
its runs ran it.

`tools/aorai/` writes three of the library's call-order rules as automata
for Frama-C's Aorai, and `14 · Eva`'s job `aorai` checks with Eva that the
programs keep them, and that the programs written to break them break them
(`tools/aorai/README.md`). This probe asks what WP makes of the same
instrumented programs. WP runs only in CI, so it is measured here,
report-only, before anything is decided of it.

Aorai instruments a program with an automaton: ghost state, the ghost
functions that update it at each call and each return, and contracts on
every function that name the states it is called and returns in. For a
program to keep the rule, every one of those contracts must hold: main's
obligations, among them each call's precondition on the state, and each
other function's, that it moves the state as its contract says.

| part | what |
|------|------|
| `main` | each case of the probe's `MAIN`: Aorai, then WP on `main` alone (`-wp-fct main`), the callees' contracts taken as given, with WP's smoke tests (since run 1b). Since run 2, cases of its own (below); in runs 1 and 1b, `tools/aorai/aorai.py`'s `CASES`, and in 1b `hashmap_stale_checks.c` after them |
| `lib-hashmap`, `lib-stringbuf`, `lib-bitset` | `hashmap_ok.c`, `stringbuf_ok.c` and `bitset_ok.c`, each with its automaton, and WP on every function |

WP runs with the options of the unit whose program the case is
preprocessed as (`tools/agreement/agreement.py`'s `UNITS` and `EXTRA`):
its model, `-wp-rte`, `-wp-split`, Alt-Ergo, Z3 and CVC5, 120 s (in the
part `main`, since run 2b, without `-wp-rte` and with 10 s). Report's table, with `-report-untried`, then gives each property its
status; Aorai's are those that name its state (`aorai_`), as for
`aorai.py`. Each part posts WP's summary, Aorai's properties by status,
and, for a case that breaks its rule, the status of Aorai's precondition
at the call marked `/* refused */`. It never fails on what it measures: a
part fails only if a WP run gives no summary or no table, or is stopped
at its limit (a case of `main` at 90 minutes since run 2b, 30 before;
230 minutes a lib part).

**Run 1b.** In run 1, WP proved the precondition of the call
`hashmap_stale.c` marks refused, which Eva finds invalid. So the part
`main` now runs WP's smoke tests, which try to prove that a call's
contract or a precondition leaves the code after it unreachable, and a
last case, `hashmap_stale_checks.c`: `hashmap_stale.c` with ACSL checks,
which WP proves or not but never assumes, of `\false` after the init,
after the loop, after the first `hashmap_iter_next` and after the insert,
and of `iter == 0` before the refused call. A check of `\false` proved
marks a point WP reaches with hypotheses that contradict each other;
`iter_zero` proved would mean WP takes `iter` to be 0 there. The part's
log listed, for each case that breaks its rule, every property of `main`
at the marked call and Aorai's of `main`, and for every case Aorai's
properties neither valid nor ignored.

**Run 2.** In run 1b WP proved every check of `\false` in
`hashmap_stale_checks.c`, from the first, after the init, and its smoke
tests found the code of every hashmap program unreachable from the init's
line on: WP's hypotheses there contradict each other, and what it proved
after them it proved of nothing. The likely cause, which the run did not
name: `backing` is a `u64[64]`, and `bytes_from` requires
`\valid((u8 *)ptr + (0 .. len - 1))`, 512 bytes; WP's typed model counts
an array in cells of its type, 64 here, so in its model that precondition
is false, and WP takes a call's precondition as a hypothesis after the
call. The part `main` now runs four cases:

| case | program | automaton |
|------|---------|-----------|
| `hashmap_stale_checks` | `hashmap_stale_checks.c`, as in run 1b | `hashmap_iteration` |
| `hashmap_stale_checks_plain` | the same | none: WP on the program alone |
| `hashmap_stale_bytes` | `hashmap_stale_bytes.c`: the same, its buffer a `u8[512]` aligned for the slots | `hashmap_iteration` |
| `hashmap_ok_bytes` | `hashmap_ok_bytes.c`: `tools/aorai/programs/hashmap_ok.c`, its buffer the same | `hashmap_iteration` |

Each case's notice also gives the lines WP's smoke tests find unreachable
(its warnings "Failed smoke-test"), the status of each check the program
names, and `main`'s other properties neither valid nor ignored, which its
log lists in full with `main`'s assertions and checks. Built with gcc, the
two programs of bytes return 0, and Eva with Aorai (Frama-C 25) gives them
the verdicts and counts of the programs they copy.

**Run 2b.** Run 2 named the goal: `bytes_from`'s precondition at line 19,
the only property of `main` WP left unproved, with the automaton and
without it. The two programs of bytes were stopped at 30 minutes: without
the contradiction, every goal WP does not prove, and every smoke test
that passes, takes the timeout, 120 s a prover. The part `main` now runs
`hashmap_stale_checks`, as a control, `hashmap_stale_bytes` and
`hashmap_ok_bytes`, with WP without `-wp-rte` (its overflow checks were
among the slow goals) and with a 10 s timeout (`MAIN_TIMEOUT`), each case
stopped at 90 minutes. A goal not proved in 10 s may be proved in 120 s;
a contradiction like run 1b's is found in milliseconds.

The file `steps` names the parts that run; a part it does not name posts
that it did not run. Each part's files, `aorai_wp_out/PART/`, are its
artifact, `aorai-probe-PART`.

    python3 tools/probes/verify-085/aorai_wp_probe.py PART
    python3 tools/probes/verify-085/aorai_wp_probe.py named PART

Its predictions and their scores are VERIFY-085's, in
`docs/deviations.md`.
