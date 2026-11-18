# WP's smoke tests: `16 · Smoke tests`

A proof is of nothing where WP's hypotheses contradict each other: from
there on WP proves every goal, the false with the true. WP's summary does
not show such a point; its smoke tests look for them. With
`-wp-smoke-tests` WP adds goals of `\false`: at each function's entry,
under its requires and each behavior's assumes; after each call; at each
loop; and in code its own analysis does not find reachable. A smoke test
whose `\false` a prover proves has **failed**: WP's hypotheses there
contradict each other. One that no prover proves has passed; that is a
test, not a proof that the point is reachable.

VERIFY-087 ran them on the 27 proof units (its probe,
`tools/probes/verify-087/`) and found two kinds of failure. Branches no
execution reaches, an overflow branch after a `checked_mul` that cannot
overflow among them: there WP proves of nothing, as it should. And calls
whose precondition is false in WP's model, past which WP proved what
follows of nothing: three it fixed (`array_<T>_<N>_zero`,
`array_<T>_<N>_from_ptr`, `diag_init`, which called `memset` on a local
object), and nine call `memcmp`, as `docs/verification.md` says in "Past
`memcmp`'s call". VERIFY-088 made the smoke tests a check, so that a new
point of either kind is seen when it comes.

**The check.** `smoke.py UNIT` runs WP on one unit of
`tools/agreement/agreement.py`'s `UNITS` with the unit's options as its
proof job runs it (its model, `-wp-rte`, `-wp-split`, Alt-Ergo, Z3 and
CVC5, its extra options), every goal at 1 s a prover and the smoke tests
at WP's 2 s. The goals' results are not read: the proof jobs check them.
It reads the smoke tests from WP's JSON report and compares those that
failed with the unit's pins, `pins/UNIT.txt`: each line a goal's name as
WP gives it, but its statement number, and how many smoke tests of that
name fail. WP names a call's smoke test after the function called, so
that `typed_cast_memcmp_wp_smoke_dead_call 5` is five calls of `memcmp`.
A count other than the pin's, a name the pins do not hold, or a smoke
test with no result is a difference; with `ENFORCE` (in the script) a
difference fails the job. The notice gives the smoke tests, how many
failed by kind, the differences, and the slowest failure: how long the
fastest prover took to prove its `\false`, of the 2 s it had.

**Which units.** The workflow's first job chooses them as
`13 · Prover agreement` does (`smoke.py affected`): the units that include
a changed file, as the compiler lists them. A change to the script,
`agreement.py`, the workflow or the Frama-C setup, or to a header no unit
includes, runs every unit; a change to a unit's pin file runs that unit.

    python3 tools/smoke/smoke.py UNIT
    python3 tools/smoke/smoke.py affected --all | FILE...
    python3 tools/smoke/smoke.py check
    python3 tools/smoke/smoke.py pin UNIT

`check` is `8 · Records`': one pin file a unit, each line well formed,
and every file a unit includes under the workflow's push paths. `pin`
writes a unit's pins from its last run, in `smoke_out/UNIT/`. A new proof
unit needs its pin file; its lines come from the check's first run of it.

**Runs.** VERIFY-088's: run 1 report-only, its pins those VERIFY-087's
probe measured; the predictions and their scores are in
`docs/deviations.md`.
