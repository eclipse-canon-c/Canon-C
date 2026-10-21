# VERIFY-040 probes

Executable checks of what the record reads from the tools, against the code
they describe. Findings, predictions and scoring live in `docs/deviations.md`,
VERIFY-040. These files are recorded probes, not tests: nothing builds them
and the main CI ignores `tools/**`. The count probe is run by its own
workflow, `.github/workflows/count-probe.yml`, through `count_probe.py`, which
runs WP and reads its output against the prediction.

Run from the repository root, with the repository's history. Exact commands
are in the file's header. The count probe runs WP, with Qed alone, on
`vmacros/vdrivers/vec_verify.h`, the driver `frama-c-vec` verifies, as
committed and as it was at bd9466e, which `git show` writes at the
repository root as `wp_B_vec_verify.h`.

| Probe | Question | Predicted |
|---|---|---|
| `count_probe.py` | VERIFY-040 round 2: which clause of `vec_int_append_array` or `vec_int_extend` WP 29 splits otherwise than WP 25, so that each vec unit gained five goals at CI #1375 and CC #56 where the prediction said three | the two functions' goals five more than at bd9466e's driver; `append_array`'s fifth `ok` postcondition in six parts and `extend`'s two new ones one goal each, as WP 25 makes them; `extend`'s old `ok` frame in three goals where WP 25 makes five: one call effect on each path, where WP 25 makes two, and the call's result |
