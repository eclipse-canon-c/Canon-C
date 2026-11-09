# Prover agreement (VERIFY-064, VERIFY-065)

Every proof job gives Alt-Ergo, Z3 and CVC5 together each goal that Qed,
WP's simplifier, does not close. The first prover to succeed proves the
goal, and WP stops the other two. Even with `-wp-run-all-provers`, WP's
per-goal report keeps only one prover per goal (Frama-C 29, `Stats.results`).
So a job does not say whether the other two could have proved a goal.

`agreement.py` measures it. It proves a unit three times, once with each
prover alone and otherwise as its proof job proves it, and keeps WP's report
of each goal (`-wp-report-json`). Then it sorts the goals:

- proved by WP itself, with no prover (by Qed, or by WP's own passes);
- proved by all three provers, each alone;
- proved by two;
- proved by one only;
- proved by none: these must be the names the unit's proof job pins.

VERIFY-064 measured it once for all 26 units, by hand. Since VERIFY-065 it
runs on every push that can change it, in `13 · Prover agreement`
(`.github/workflows/prover-agreement.yml`):

- **Which units.** The workflow's first job runs `agreement.py affected` on
  the files the push changed. The compiler lists the repository files each
  unit includes, with the unit's own flags, with and without Frama-C's
  macros, and the units that include a changed file run. A change to
  `agreement.py`, to the workflow or to the Frama-C setup, or to a header
  under `core/`, `semantics/`, `data/` or `vmacros/vdrivers/` that no unit
  includes, runs every unit. A change to a unit's pins runs that unit. A
  manual run runs every unit.
- **The pins.** `pins/UNIT.txt` lists the goals that one or two provers
  prove, one per line: `one PROVER NAME` or `two PROVER+PROVER NAME`. A goal
  not listed is proved by WP itself, by all three provers, or by none. A
  proof that takes a prover long could fall either side of the 120 s limit
  on another run: VERIFY-065 saw proofs that took over 60 s on one run time
  out on the next. So a goal is left out of the comparison when the run
  being compared takes over 30 s to prove it, or when its pins list it as
  `slow PROVER NAME`, a proof seen to take over 60 s when the pins were
  made. The run's notices give each slow proof's time.
- **The gate.** In each unit, the goals no prover proves alone must be the
  names its proof job pins, and the goals one or two provers prove must be
  those of its pin file. `ENFORCE` in `agreement.py` turns a difference into
  a failure; it is off until VERIFY-065's third run.
- **The check.** `agreement.py check`, in `8 · Records`, fails when a unit's
  file, memory model, preprocessing or extra options here differ from its
  proof job's, when a proof job has no unit here (but `frama-c-arena-32`
  and `frama-c-win64`, which prove these units at other machine models), or
  when a unit includes a file outside the workflow's push paths.

A new verified unit needs its entry in `UNITS` and `ORDER`, and its pin
file once its first run has posted its names.

    python3 tools/agreement/agreement.py run frama-c-arena z3
    python3 tools/agreement/agreement.py unit frama-c-arena DIR
    python3 tools/agreement/agreement.py total DIR
    python3 tools/agreement/agreement.py deps [UNIT]
    python3 tools/agreement/agreement.py affected FILE...
    python3 tools/agreement/agreement.py check

Results are posted as notices, which the GitHub API returns. The artifacts
hold WP's output and reports, and `agreement.tsv` lists every goal that WP
does not prove itself, with its class.
