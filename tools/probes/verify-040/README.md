# VERIFY-040 probes

Executable checks of what the record reads from the tools, against the code
they describe. Findings, predictions and scoring live in `docs/deviations.md`,
VERIFY-040. These files are recorded probes, not tests: nothing builds them
and the main CI ignores `tools/**`. Each probe is run by a workflow of its
own, the count probe by `.github/workflows/count-probe.yml` and the round 3
probe by `.github/workflows/round3-probe.yml`, through its script, which
runs WP and reads its output against the prediction.

Run from the repository root, with the repository's history. Exact commands
are in the file's header. The count probe runs WP, with Qed alone, on
`vmacros/vdrivers/vec_verify.h`, the driver `frama-c-vec` verifies, as
committed and as it was at bd9466e, which `git show` writes at the
repository root as `wp_B_vec_verify.h`. The round 3 probe runs WP on `fill`'s
function in vec's three units, as committed and in a copy of the tree with
`IMPL_VEC_FILL`'s loop annotated, which it writes as `r3_annotated/`, and on
`p_padding_lemmas.h`, which includes bitset's driver and states three lemmas
after it.

| Probe | Question | Predicted |
|---|---|---|
| `count_probe.py` | VERIFY-040 round 2: which clause of `vec_int_append_array` or `vec_int_extend` WP 29 splits otherwise than WP 25, so that each vec unit gained five goals at CI #1375 and CC #56 where the prediction said three | the two functions' goals five more than at bd9466e's driver; `append_array`'s fifth `ok` postcondition in six parts and `extend`'s two new ones one goal each, as WP 25 makes them; `extend`'s old `ok` frame in three goals where WP 25 makes five: one call effect on each path, where WP 25 makes two, and the call's result. Run #1 (d2ad2d6) scored as predicted in every part: 196 goals and 191, `extend`'s old `ok` frame three goals |
| `round3_probe.py`, `p_padding_lemmas.h` | VERIFY-040 round 3: what WP 29 makes of an invariant, a frame and a variant for `fill`'s loop, and whether the CI's provers prove them, under `-CC` and under `frama-c-vec`'s preprocessing; and whether they prove three lemmas for the bit-level steps of block 7's four goals, and the four goals with the lemmas as hypotheses | `bitset_shr_zero_bits` not proved (M), `bitset_bits_shr_zero` and `bitset_mask_bits` not proved (H); with the three, every goal of the four functions proved (M); `fill`'s goals under `-CC`, annotated, all proved (M), 27 more at each type (L); `frama-c-vec`'s the same, annotated or not (H) |
