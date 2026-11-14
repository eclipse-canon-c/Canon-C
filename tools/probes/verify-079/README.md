# VERIFY-079's probe: vec_int_extend_from_range's two slow goals

`frama-c-vec-range` proves every goal of `vec_int_extend_from_range`, but
two only with Alt-Ergo: parts 3 and 4 of the preservation of the loop's
invariant 5, the values the loop has stored so far. Alt-Ergo alone took
89 s and 38 s on them in VERIFY-065's runs, and up to 76 s and 38 s in
`13 · Prover agreement` #6 to #11, against WP's 120 s limit. The proof job runs the three provers at once on each goal. A slower
runner could lose these two, and `frama-c-vec-range` would fail its pin
(VERIFY-065, "Left for later").

    loop invariant \forall integer k; 0 <= k < i
      ==> v->items[v->len + k] == \at(r.current, Pre) + k * r.step;

Under WP's typed model, the items, `v->len` and the range's fields are all
integers in one memory: the range `r` is in memory because the loop passes
`&r` to `range_next`. After the store `v->items[v->len + i] = elem`, each
read of `v->len` and of `r.step` in the invariant needs the provers to show
that the store did not write it, and the invariant asks it for every `k`.
The probe measures the function with four versions of
`data/vec/vec_range.h`, each a patch under `variants/`:

| version | patch | the loop's annotations |
|---------|-------|------------------------|
| P0 | none | as they are |
| A | `a_asserts.patch` | three assertions: after the round-trip check, that `value` is the range's `i`-th value (`ith_value`) and that `elem` equals it (`fits`); after the store, that the slot holds it (`ith_stored`) |
| B | `b_anchored.patch` | invariant 5 over values no store changes, `v->items[\at(v->len, Pre) + k] == \at(r.current, Pre) + k * \at(r.step, Pre)`, and after it a new invariant, `len_kept`: `v->len == \at(v->len, Pre)` |
| C | `c_both.patch` | A and B |

A states the new slot's value, the part of the step that is new at each
iteration. With B, of what the invariant reads only the items themselves
are integers a store can write, and `len_kept` proves once per iteration,
with no `k`, that `v->len` is kept. The new
invariant comes after invariant 5, so the five keep their numbers and their
goals' names.

`vecrange_probe.py` applies a version to a copy of the checkout and runs WP
on `vec_int_extend_from_range` alone (`-wp-fct`), otherwise as
`frama-c-vec-range` runs it (`tools/agreement/agreement.py`'s `UNITS`):
once with Alt-Ergo, Z3 and CVC5 together, as the proof job runs them, and
once with each alone, as `13 · Prover agreement` runs them.

| step | what |
|------|------|
| `P0`, `A`, `B`, `C` | that version's four runs: the goals proved and not; each goal's prover and time, together and alone; the goals one or two provers prove alone, as `tools/agreement/pins/` lists them; and the pin file's lines for the function that would go and come |
| `report` | the four compared: each version's counts, its slowest proof, and invariant 5's preserved goals, from what the other steps wrote |

The file `steps` names the steps that run; a step it does not name posts
that it did not run. Each step posts notices and prints each goal's results
in the log; its files are in `vecrange_out/` (the artifact
`vecrange-probe`).

It runs in `9 · Probes` (`probes.yml`, `vecrange-probe`), report-only: it
fails only if a WP run gives no summary or no report, or is stopped after
an hour. A version whose patch neither applies nor is found applied is
reported and not measured. Its predictions and their scores are
VERIFY-079's, in `docs/deviations.md`.
