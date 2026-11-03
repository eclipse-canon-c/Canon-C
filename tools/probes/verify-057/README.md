# VERIFY-057: the pointer probe

`pointer_probe.py` runs every verified unit as its proof job runs it, with
Frama-C's kernel option `-warn-invalid-pointer` added, and proves only the
`pointer_value` goals that option adds (`-wp-prop pointer_value`). C leaves a
pointer computed outside its object, or more than one past its end,
undefined; `-wp-rte` alone does not check it. The job is `pointer-probe` in
`.github/workflows/probes.yml`, one matrix entry per unit; its predictions
and its score are VERIFY-057 in `docs/deviations.md`. Its run on 9207897
(`9 · Probes` #3) is scored there, "Scored (2026-11-04)": every prediction
matched, and the 122 unproved goal-instances are read by kind.

    python3 tools/probes/verify-057/pointer_probe.py run frama-c-arena
    python3 tools/probes/verify-057/pointer_probe.py report frama-c-arena

Each unit's file, memory model and preprocessing are copied from its job;
if a job's flags change, change them here too.
