# VERIFY-087's probe: WP's smoke tests on the proof units

A proof is of nothing where WP's hypotheses contradict each other: from
there on WP proves every goal, the false with the true. VERIFY-085 met
one, in a program that uses the library: `bytes_from`'s precondition,
asking for 512 bytes of a `u64[64]`, is false in WP's typed model, which
counts the array in cells of its type, 64; WP takes a call's precondition
as a hypothesis after the call, and proved every goal after it, in
milliseconds. WP's summary does not show such a point. This probe asks
whether any of the 27 proof units has one. WP runs only in CI, so it is
measured here, report-only, before anything is decided of it.

**Smoke tests.** With `-wp-smoke-tests`, WP adds goals of `\false`: at
each function's entry, under its requires and each behavior's assumes;
after each call; at each loop; and in code its own analysis does not find
reachable. It tries each with a short timeout, `-wp-smoke-timeout`, 2 s
by default. A smoke test whose `\false` a prover proves has failed: WP's
hypotheses at that point contradict each other, and it warns "Failed
smoke-test" at the place. One that no prover proves has passed, which is
a test, not a proof that the point is reachable.

| run | what |
|-----|------|
| `smoke-only` | `-wp-prop=smoke`: the smoke tests alone, as WP selects them by the name it gives them |
| `full` | every goal of the unit and the smoke tests: the smoke tests WP makes for the unit's proof; the unit's own goals with a 1 s timeout a prover, their results not read |

Each part is one unit of `tools/agreement/agreement.py`'s `UNITS`, one
job of the matrix, and runs both. WP runs with the unit's options, as
its proof job: its model, `-wp-rte`, `-wp-split`, Alt-Ergo, Z3 and CVC5,
and its extra options. Each run posts a notice: its smoke tests, how many
passed, those that failed by kind (a behavior's requires or assumes, a
call, a loop, dead code) and function, the places WP warns of, and WP's
summary; a third compares the two runs. The log lists the smoke tests
that failed or gave no result, and, from WP's JSON report, every smoke
test by kind and status. It never fails on what it measures: a part fails
only if a run is stopped at its limit (120 minutes), Frama-C exits with
an error, or the full run gives no summary.

The file `steps` names the units that run, or `all`; a unit it does not
name skips Frama-C's setup and posts that it did not run. Each unit's
files are its artifact, `smoke-probe-UNIT`.

    python3 tools/probes/verify-087/smoke_probe.py UNIT
    python3 tools/probes/verify-087/smoke_probe.py named UNIT
    python3 tools/probes/verify-087/smoke_probe.py check

**Counts.** WP prints a failed smoke test's line once for each prover
that proves it, and its summary's "Smoke Tests" counts it as often (run
1: `frama-c-arena`'s summary said 40 failed, for 14 tests). Since run 2
the notices count each smoke test once: the tests are those of WP's JSON
report, and the log keeps WP's own line.

**Runs.** Run 1: every unit (`steps`: `all`). Run 2: `frama-c-array` and
`frama-c-diag`, whose `memset`s run 1 found voiding WP's proofs past them,
with the code that replaces them: no smoke test fails in it, and the two
units' other failures are run 1's. Closed at run 3: `steps` names no
unit. The predictions and their scores are VERIFY-087's, in
`docs/deviations.md`.
