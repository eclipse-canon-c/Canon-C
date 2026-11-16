# VERIFY-085's probe: the call-order rules under WP

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
| `main` | each case of `tools/aorai/aorai.py`'s `CASES`: Aorai, then WP on `main` alone (`-wp-fct main`), the callees' contracts taken as given |
| `lib-hashmap`, `lib-stringbuf`, `lib-bitset` | `hashmap_ok.c`, `stringbuf_ok.c` and `bitset_ok.c`, each with its automaton, and WP on every function |

WP runs with the options of the unit whose program the case is
preprocessed as (`tools/agreement/agreement.py`'s `UNITS` and `EXTRA`):
its model, `-wp-rte`, `-wp-split`, Alt-Ergo, Z3 and CVC5, 120 s.
Report's table, with `-report-untried`, then gives each property its
status; Aorai's are those that name its state (`aorai_`), as for
`aorai.py`. Each part posts WP's summary, Aorai's properties by status,
and, for a case that breaks its rule, the status of Aorai's precondition
at the call marked `/* refused */`. It never fails on what it measures: a
part fails only if a WP run gives no summary or no table, or is stopped
at its limit (30 minutes a case of `main`, 230 minutes a lib part).

The file `steps` names the parts that run; a part it does not name posts
that it did not run. Each part's files, `aorai_wp_out/PART/`, are its
artifact, `aorai-probe-PART`.

    python3 tools/probes/verify-085/aorai_wp_probe.py PART
    python3 tools/probes/verify-085/aorai_wp_probe.py named PART

Its predictions and their scores are VERIFY-085's, in
`docs/deviations.md`.
