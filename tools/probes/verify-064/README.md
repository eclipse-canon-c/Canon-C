# VERIFY-064: the prover-agreement probe

Every proof job gives Alt-Ergo, Z3 and CVC5 together each goal that Qed,
WP's simplifier, does not close. The first prover to succeed proves the
goal, and WP stops the other two. So a job does not say whether the other
two could have proved it, and no goal's proof needs two provers to agree.
`agreement_probe.py` measures that agreement. It runs each verified unit
three times, once with each prover alone and otherwise as its proof job runs
it, and keeps WP's report of each goal (`-wp-report-json`). Then it sorts
the goals:

- proved by WP itself, with no prover (by Qed, or by WP's own passes);
- proved by all three provers, each alone;
- proved by two;
- proved by one only;
- proved by none.

The workflow is `9 · Probes (manual): prover agreement`
(`.github/workflows/prover-agreement.yml`), started by hand from the Actions
tab. It runs 78 jobs, one per unit and prover, and holds up to 30 runners
for hours, so start it when no other run needs them. Its predictions and its
score are VERIFY-064 in `docs/deviations.md`. It gates nothing.

    python3 tools/probes/verify-064/agreement_probe.py run frama-c-arena z3
    python3 tools/probes/verify-064/agreement_probe.py unit frama-c-arena DIR
    python3 tools/probes/verify-064/agreement_probe.py total DIR

The units, and each one's file, memory model and preprocessing, are the
pointer probe's (`tools/probes/verify-057/pointer_probe.py`), copied from the
proof jobs. If a job's flags change, change them in both. WP's summary also
counts "Terminating" and "Unreachable" goals, which are not in its report.
The script reads them from the summary and adds them back, so that each
unit's count can be set beside its pin.

Results are posted as notices, which the GitHub API returns; the artifacts
hold WP's output and reports, and `agreement.tsv` lists every goal that WP
does not prove itself, with its class.
