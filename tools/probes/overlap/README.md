# Overlap probe (VERIFY-008)

An experiment, not a gate. `.github/workflows/overlap-probe.yml` runs it
on a push to master that changes its files, and on demand.

- `overlap.patch` changes `mem_regions_overlap` in `core/memory.h` to
  compare the two addresses as `uintptr_t` values, without wrapping,
  in place of comparing the pointers with `<`, which C leaves undefined
  across objects.
- `overlap_probe.py run` proves the function with WP as `frama-c-memory`
  does, on the tree as committed and as patched, under the default
  machine and `gcc_x86_32`, and counts memory.h's goals with Qed alone.
- `overlap_probe.py report` scores the prediction committed in
  `docs/deviations.md`, VERIFY-008, "Committed prediction (2026-10-25,
  the overlap probe)", and posts the verdicts on the run page.

Run from the repository root with `frama-c` on PATH.

Retire the probe in the push that applies the fix: once `core/memory.h`
carries it, `overlap.patch` no longer applies and a run would fail.
