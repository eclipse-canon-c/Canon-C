# Metrics: each function's complexity, header by header

`metrics.py` runs Frama-C's Metrics plug-in on the units
`tools/size/size.py` measures: each public header alone, a generic module
instantiated for `u32`, a function counted for the header whose module
defines it, and `Result<_Bool, Error>`'s functions in a row of their own.
The code is as shipped (NDEBUG), at Frama-C's default machine model. The
counts are syntactic, on the code as Frama-C normalizes it: statements,
decision points, ifs, loops, gotos, assignments, exit points, calls,
pointer dereferences, and cyclomatic complexity. Nothing is proved.

`pins.txt` holds each function's cyclomatic complexity. The `metrics` job
in `2 · Platforms` measures them all and compares: a complexity that
changed, a function measured and not pinned, or pinned and not measured,
is a difference. `8 · Records` checks the file's form. A change that moves
a figure pins it again, with `metrics.py pin` from its run's
`metrics.csv`, in the same push, under its record.

    python3 tools/metrics/metrics.py run [OUT]   measure and compare; metrics.csv to OUT
    python3 tools/metrics/metrics.py pin DIR     pins.txt from DIR/metrics.csv
    python3 tools/metrics/metrics.py check       pins.txt is well formed

VERIFY-081 (`docs/deviations.md`) set it up. Its pins were measured with
Frama-C 25; in its run 1, report-only, CI's Frama-C 29 gave every function
the same complexity, and since its run 2 a difference fails the job.
