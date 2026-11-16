# VERIFY-084's probe: compare.h's comparators as total orders

Closed at VERIFY-084's run 2: its file `steps` names no part. WP proved
every goal of the driver, at x86_64 and at one model of each class, and
the two controls held (`9 · Probes` #29). The driver is now
`vmacros/vdrivers/compare_order_verify.h`, which the job
`frama-c-compare-order` (`proofs-core.yml`) proves; `13 · Prover
agreement` measures it, `11 · Proofs: machine models` proves it at the nine
other models, and `tools/eva/programs/compare_order.c` runs it under Eva
and E-ACSL. Below, the probe as run 1 ran it.

`algo_cmp_fn`'s documentation in `core/primitives/compare.h` asks every
comparator to be a total order:

- `cmp(a, a) == 0` (reflexive)
- `cmp(a, b) < 0` implies `cmp(b, a) > 0` (antisymmetric)
- `cmp(a, b) < 0 && cmp(b, c) < 0` implies `cmp(a, c) < 0` (transitive)

Sorting, binary search and `data/priority_queue.h` rely on that. The
contract of each of compare.h's 24 comparators states only that its result
is -1, 0 or 1. A contract speaks of one call, and each property relates two
or three, so WP cannot derive any of them from the contracts.
`frama-c-compare` proves the contracts, not the order.

## The drivers

`gen_order.py` writes two drivers, so that each function is the same text
with only the comparator and its type changed:

- **`compare_order_verify.h`**, in this directory in run 1 and in
  `vmacros/vdrivers/` since run 2: for each comparator `cmp`, three functions
  that call it and assert, for any `a`, `b` and `c` that meet its
  precondition and any `ctx`:

  | function | assertion | states |
  |----------|-----------|--------|
  | `order_T_reflexive` | `reflexive` | `cmp(a, a) == 0` |
  | `order_T_antisymmetric` | `antisymmetric` | `cmp(b, a) == -cmp(a, b)` |
  | `order_T_transitive` | `transitive_lt` | `cmp(a, b) < 0 && cmp(b, c) < 0 ==> cmp(a, c) < 0` |
  | | `transitive_eq` | `cmp(a, b) == 0 && cmp(b, c) == 0 ==> cmp(a, c) == 0` |

  72 functions, 96 assertions. Antisymmetry here is stronger than the
  documentation's, and with it the two transitive assertions give the rest
  of what a sort needs: `cmp(a, b) > 0 && cmp(b, c) > 0` implies
  `cmp(a, c) > 0`, and when `cmp(a, b) == 0`, `cmp(a, c)` and `cmp(b, c)`
  have the same sign.
- **`compare_order_false.h`**, the negative control, here: for each comparator,
  `order_false_T_symmetric` asserts `symmetric`, `cmp(b, a) == cmp(a, b)`,
  false whenever `a` and `b` compare unequal; and for `f32` and `f64`,
  `order_false_T_self_equal` asserts `self_equal`, that C's `x == x` holds,
  false for a NaN. 26 functions, 26 assertions.

    python3 tools/probes/verify-084/gen_order.py           write both
    python3 tools/probes/verify-084/gen_order.py --check   check both

## How WP proves them

WP proves the drivers as `frama-c-compare` proves `compare.h`
(`tools/agreement/agreement.py`'s `UNITS`: `Typed+Cast`, `-wp-rte`,
`-wp-split`, 120 s, its preprocessing), and with Frama-C's `-inline-calls`
and `-remove-inlined`, each given the 24 comparators' names. Each call is
replaced by the comparator's body, and so is a `_desc` comparator's call to
its ascending one. The assertions are then proved of the comparators' code,
not of their contracts. The kernel keeps each inlined body's contract as a
statement contract, which WP has not supported since Frama-C 23 (its
manual, sections 1.6 and 2.1): it neither proves nor uses them, and the
reads of the inlined bodies are checked by `-wp-rte`'s run-time checks
instead. The comparators themselves then leave the analysed unit;
`frama-c-compare` proves them. RPP, Frama-C's plug-in for such relational
properties, proves them by self-composition too, but needs Frama-C 33
(`docs/for-future.md`).

## The probe

`order_probe.py` runs in `9 · Probes` (`probes.yml`, the jobs `order-probe`
and `order-probe-report`), report-only. Each part is a job of the matrix
and one WP run; a part that `steps` does not name skips Frama-C's setup and
posts that it did not run:

| part | what |
|------|------|
| `together` | first the files: `gen_order.py --check`; with Frama-C but not WP, each driver as the inlining leaves it (`-print`), its functions, its assertions, and that no comparator's name is left; and whether `-inline-calls @all` leaves the functions the same. Then the driver, with Alt-Ergo, Z3 and CVC5 together, as a proof job runs them |
| `alt-ergo`, `z3`, `cvc5` | the driver with each prover alone, as `13 · Prover agreement` runs them |
| `contract` | the control without inlining: the driver's four kinds of assertion only (`-wp-prop`), each call proved from the comparator's contract |
| `false` | the control of false assertions: `compare_order_false.h` with the inlining, its `symmetric` and `self_equal` only |
| `cortex_m4`, `rv64_lp64`, `msvc_x86_64` | the driver with the three provers together at one model of each class of `11 · Proofs: machine models` (ilp32, lp64, llp64), with the model's arguments as `proofs-machdeps.yml` gives them |

`order-probe-report` downloads the parts' files and compares them: the
goals one or two provers prove alone, as a pin file of `13 · Prover
agreement` would list them (the lines are printed in its log); each model
against x86_64; the two controls.

Each part prints each goal's result in its log and posts notices: the
summary, the assertions' goals by property and by the comparators' type
(integer or floating point), every goal by kind, the goals not proved, and
the proofs over 10 s. It fails only if its WP gives no summary or no
report, or is stopped after 230 minutes. The report fails only if no part
wrote a summary. The file `steps` names the parts that run; a part it does
not name posts that it did not run. Each part's files, `order_out/PART/`,
are its artifact, `order-probe-PART`.

    python3 tools/probes/verify-084/order_probe.py PART
    python3 tools/probes/verify-084/order_probe.py report [DIR]

Its predictions and their scores are VERIFY-084's, in
`docs/deviations.md`.
