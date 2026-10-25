# Overlap probe (VERIFY-008), retired

An experiment, not a gate, retired by the push that applied the fix to
`core/memory.h`: its workflow, `.github/workflows/overlap-probe.yml`, and
`overlap.patch` were removed there, and `overlap_probe.py` is kept as the
record of what the probe ran. Its one run is scored in `docs/deviations.md`,
VERIFY-008, "The overlap probe, scored".

- `overlap.patch` changed `mem_regions_overlap` in `core/memory.h` to
  compare the two addresses as `uintptr_t` values, without wrapping,
  in place of comparing the pointers with `<`, which C leaves undefined
  across objects.
- `overlap_probe.py run` proved the function with WP as `frama-c-memory`
  does, on the tree as committed and as patched, under the default
  machine and `gcc_x86_32`, and counted memory.h's goals with Qed alone.
- `overlap_probe.py report` scored the prediction committed in
  `docs/deviations.md`, VERIFY-008, "Committed prediction (2026-10-25,
  the overlap probe)", and posted the verdicts on the run page.
