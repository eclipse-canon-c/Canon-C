# VERIFY-079's probe: vec_int_extend_from_range's two slow goals

`frama-c-vec-range` proves every goal of `vec_int_extend_from_range`, but
two only with Alt-Ergo: parts 3 and 4 of the preservation of the loop's
invariant 5, the values the loop has stored so far. Alt-Ergo alone took
89 s and 38 s on them in VERIFY-065's runs, and up to 76 s and 38 s in
`13 · Prover agreement` #6 to #11, against WP's 120 s limit. The proof job
runs the three provers at once on each goal, which run 1 found slower than
one alone, 1.3 to 1.5 times. A slower runner could lose these two, and
`frama-c-vec-range` would fail its pin (VERIFY-065, "Left for later").

    loop invariant \forall integer k; 0 <= k < i
      ==> v->items[v->len + k] == \at(r.current, Pre) + k * r.step;

WP splits each goal of the loop's body on the paths through the round-trip
check (`-wp-split`): parts 1 and 2 are the two paths that return, which WP
proves itself, and parts 3 and 4 the two that reach the store.

The probe measures the function with versions of `data/vec/vec_range.h`,
each a patch under `variants/` or another WP model:

| version | change | the loop's annotations | run |
|---------|--------|------------------------|-----|
| P0 | none | as they are: the control | 1, 1b |
| A | `a_asserts.patch` | three assertions: after the round-trip check, that `value` is the range's `i`-th value (`ith_value`) and that `elem` equals it (`fits`); after the store, that the slot holds it (`ith_stored`) | 1 |
| B | `b_anchored.patch` | invariant 5 over values no store changes, `v->items[\at(v->len, Pre) + k] == \at(r.current, Pre) + k * \at(r.step, Pre)`, and after it a new invariant, `len_kept`: `v->len == \at(v->len, Pre)` | 1 |
| C | `c_both.patch` | A and B | 1 |
| D | `d_kept.patch` | after the store, `kept`: each earlier slot holds what it held at the iteration's start, `\at(v->items[v->len + k], LoopCurrent)` | 1b |
| E | `e_kept_call.patch` | D, and the same after the call to `range_next`, `kept_call`: the frame across the call and the frame across the store, each its own goal | 1b |
| R | WP's model `Typed+Cast+ref` | as they are, under WP's by-reference model, in which the range, passed to `range_next` by its address, may stay out of memory | 1b |

Run 1 found none of A to C easier, and B's premise wrong: WP itself proves
`len_kept`, so keeping `v->len` across the store was never the provers'
work, and the new slot's value, A's, is proved in under a second. What is
left is that the earlier slots keep their values across the call and the
store, or something in the hypotheses the provers get. D and E put that
frame in goals of its own; R takes the range out of memory, if WP's model
allows it.

`vecrange_probe.py` applies a version to a copy of the checkout and runs WP
on `vec_int_extend_from_range` alone (`-wp-fct`), otherwise as
`frama-c-vec-range` runs it (`tools/agreement/agreement.py`'s `UNITS`):
once with Alt-Ergo, Z3 and CVC5 together, as the proof job runs them, and
once with each alone, as `13 · Prover agreement` runs them. WP names a goal
after its model, so the probe names R's by `frama-c-vec-range`'s.

| step | what |
|------|------|
| `P0`, `A` to `E`, `R` | that version's four runs: the goals proved and not; each goal's prover and time, together and alone; the goals one or two provers prove alone, as `tools/agreement/pins/` lists them; and the pin file's lines for the function that would go and come |
| `goals` | P0's invariants as WP gives them to the provers: WP with Alt-Ergo, `-wp-prop @invariant` and `-wp-print`, its print in the log; and from `-wp-out`, the provers' files for invariant 5's preservation, parts 3 and 4, in the log |
| `report` | the versions `steps` names, compared: each one's counts, its slowest proof, and the watched goals, invariant 5's preservation, `kept` and `kept_call` |

The file `steps` names the steps that run, in order. The job in
`probes.yml` has eight slots; slot N runs the N-th step the file names
(`vecrange_probe.py --nth N`), so a new run needs no change to the
workflow. Each step posts notices and prints each goal's results in the
log; its files are in `vecrange_out/` (the artifact `vecrange-probe`).

It runs in `9 · Probes` (`probes.yml`, `vecrange-probe`), report-only: it
fails only if WP gives no summary or no report for P0, the checkout as it
is, or is stopped after an hour. Another version whose WP does so, or whose
patch neither applies nor is found applied, is reported and not measured.
Its predictions and their scores are VERIFY-079's, in `docs/deviations.md`.
