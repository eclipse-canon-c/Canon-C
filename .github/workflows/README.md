# CI map

What runs on every push, where it lives, and where a new job goes. The
workflows are numbered in the order a newcomer should read them. GitHub's
Actions sidebar sorts by name, so the sidebar shows them in this order too.
This layout was set by VERIFY-050 (`docs/deviations.md`). Before it, almost
everything lived in one file, `cmake-multi-platform.yml`, which older records
cite.

| # | file | sidebar name | holds | fails the push? |
|---|------|--------------|-------|-----------------|
| 1 | `ci.yml` | 1 · Build & test | Build and test matrix (Linux, Windows MSVC and MinGW, macOS; Release, and Debug with sanitizers), i386, Valgrind, libFuzzer, the lifetime token under concurrency, lifetime-debug | yes |
| 2 | `platforms.yml` | 2 · Platforms | musl, Linux AArch64, C11 to C23, big-endian s390x and 32-bit PowerPC (qemu; PowerPC since VERIFY-095, with `CANON_LIFETIME=debug` too), embedded compile checks (Cortex-M, RISC-V, AVR tiers), code size and build cost by header (VERIFY-070), each function's complexity by Frama-C's Metrics, by header (VERIFY-081), the tests on Cortex-M0+ and Cortex-M4 under QEMU, with newlib and picolibc, with Ubuntu's toolchain and the Arm GNU Toolchain's (VERIFY-089, `tools/cortexm/`), the tests on MSP430X under the GDB simulator, at a 16-bit `int`, with TI's GCC (VERIFY-096, `tools/msp430/`) | yes; since VERIFY-070's third run, a header's size or tokens not as pinned (`tools/size/pins.txt`) fails the embedded job. Metrics: yes, since VERIFY-081's run 2: a function whose cyclomatic complexity is not as pinned (`tools/metrics/pins.txt`), or that is measured and not pinned, or pinned and not measured, fails the metrics job. Cortex-M: since VERIFY-089's run 2, with Ubuntu's toolchain, a test whose outcome is not its pin's (`tools/cortexm/pins.txt`) fails its leg; and since run 3 with the Arm GNU Toolchain's. MSP430 since VERIFY-097, VERIFY-096's run 1 report-only |
| 3 | `analysis.yml` | 3 · Static analysis & coverage | clang-tidy and Cppcheck, clang-tidy's CERT C checks on the library's code (VERIFY-077), MISRA C:2012, MC/DC and branch coverage, CompCert | static-analysis and coverage's threshold: yes; a CERT C finding in the library's code fails static-analysis (`tools/cert/cert_check.py`). MISRA: yes, since VERIFY-068's third run: a real finding not as pinned by file and rule (`tools/misra/pins.txt`, each line naming its record) fails the job, as does a canary not seen. CompCert: informational |
| 4 | `proofs-core.yml` | 4 · Proofs: core/ | Frama-C WP, one job per unit of core/ and core/primitives/, plus arena at 32-bit and memory/bits at LLP64, and `frama-c-compare-order`: compare.h's 24 comparators as total orders, through the driver `vmacros/vdrivers/compare_order_verify.h`, the comparators' calls inlined (VERIFY-084) | yes, each job on its pins |
| 5 | `proofs-semantics.yml` | 5 · Proofs: semantics/ | WP for error, option, result, borrow, diag | yes |
| 6 | `proofs-data.yml` | 6 · Proofs: data/ | WP for vec, deque, bitset, priority_queue, range, vec_range, vec_fmt, stringbuf, stack, array, hashmap | yes |
| 7 | `proofs-cc.yml` | 7 · Proofs: -CC | The macro-body contracts proved under `-CC` (VERIFY-025), per family, for the six families whose driver jobs above run under `-C` (slice, option, result, borrow, vec, deque). The units written since (vec_range, vec_fmt, stack, array, hashmap) are preprocessed with `-CC` in their own jobs in `proofs-data.yml`, so their macro-body contracts are proved there and they have no job here | per family's gates |
| 8 | `records.yml` | 8 · Records | Record coverage (argument blocks against roll-calls), the machine models' pins checked against x86_64's (VERIFY-060, `tools/machdeps/check_pins.py`), the prover agreement's units checked against the proof jobs (VERIFY-065, `tools/agreement/agreement.py check`), Eva's programs checked against the units and the models (VERIFY-066, `tools/eva/eva.py check`), the misra job's pins checked against their records (VERIFY-068, `tools/misra/misra.py check`), the size pins checked (VERIFY-070, `tools/size/size.py check`), the metrics pins checked (VERIFY-081, `tools/metrics/metrics.py check`), MetAcsl's units checked against the proof units (VERIFY-086, `tools/metacsl/metacsl.py check`), the smoke tests' pins checked against the units (VERIFY-088, `tools/smoke/smoke.py check`), the Cortex-M pins checked against the tests (VERIFY-089, `tools/cortexm/cortexm.py check`), the MSP430 pins checked against the tests (VERIFY-096, `tools/msp430/msp430.py check`), and the workflow-size guard | yes |
| 9 | `probes.yml` | 9 · Probes | The closed experiments of VERIFY-039/040 (G4, G11, G13, G19, Count, Round 3), VERIFY-057's pointer probe (every unit with `-warn-invalid-pointer`, report-only; since VERIFY-080 it runs on any change to `core/`, `semantics/` or `data/`), VERIFY-060's char probe (arena with `char`'s signedness flipped at x86_64 and AArch64), VERIFY-066's Eva error probe (Frama-C 29's error on entering `arena_alloc`: small programs that reproduce it, and `arena.c` three ways, report-only), VERIFY-073's diag_push probe (`diag_push`'s new contract and frame-by-frame shift, applied to a copy of the checkout: frama-c-diag's WP command, each prover alone, and Eva's `diag.c` at the ten models, report-only), and VERIFY-079's vec_range probe (`vec_int_extend_from_range` with its loop's annotations as they are and changed, or under another WP model, each applied to a copy of the checkout: WP on the function with the three provers together and with each alone, and its invariants as WP gives them to the provers, report-only; the job's eight slots run the steps its file `steps` names, so a new run of it needs no change here), and VERIFY-084's order probe (whether WP proves each of `compare.h`'s 24 comparators reflexive, antisymmetric and transitive, through functions that call it and assert the three, the calls inlined: with the three provers together and with each alone, at one model of each class of 11, and two controls, without the inlining and with false assertions; one job a part, and a report, report-only; closed at run 2, its parts skipping Frama-C's setup, since `frama-c-compare-order` proves the driver), and VERIFY-085's Aorai probe (WP on the programs Aorai instruments with `tools/aorai/`'s three call-order rules: on each case's `main`, and on every function of the three programs that keep them; one job a part, report-only; closed, its parts skipping Frama-C's setup, since `14 · Eva`'s job `aorai` checks the rules with Eva), and VERIFY-087's smoke-test probe (WP's smoke tests, which try to prove `\false` at each function's entry, after each call, at each loop and in code WP does not find reachable, on each of the 27 proof units: the smoke tests alone, and every goal of the unit with them; one job a unit, report-only; closed at run 3, its parts skipping Frama-C's setup). Each runs only when its own paths change | a probe's MISS fails it; the pointer probe fails only without a summary, the Eva error probe only if it cannot set up a variant, the diag_push probe only if its patch neither applies nor is found applied, or a step gives no data, the vec_range probe only if WP gives no summary or no report for the checkout as it is, or is stopped after an hour, the order probe only if a part's WP gives no summary or no report, or is stopped after 230 minutes, the Aorai probe only if a WP run gives no summary or no table, or is stopped at its limit, the smoke-test probe only if a run is stopped at its limit, Frama-C exits with an error, or the full run gives no summary |
| 9 | `f4-control.yml`, `vc-identity.yml` | 9 · Probes (manual): … | Two experiments run only by hand | n/a |
| 10 | `machdeps.yml` | 10 · Machine models | VERIFY-060's machine models for Arm, RISC-V and big-endian PowerPC: generated with Frama-C's make-machdep from the cross-compilers, compared with the committed copies in `tools/machdeps/`; what Frama-C sees at each, whether the core/ units parse, and whether GCC mode changes their code. Runs when its own paths change | the drift check: yes (since phase C). The reads: no |
| 11 | `proofs-machdeps.yml` | 11 · Proofs: machine models | VERIFY-060's proofs of the ten core/ units, and since VERIFY-084 of compare.h's comparators as total orders, at nine machine models besides x86_64: the seven of `tools/machdeps/` (Cortex-M4 with short enums, RV32 and RV64 bare metal, 32-bit Arm, RV64 and AArch64 Linux, big-endian 32-bit PowerPC Linux) and Frama-C's `ppc_32` and `msvc_x86_64`; 99 jobs, a matrix of model and unit, at most 30 at once, each on its class's pins (`tools/machdeps/pins/`). Runs when core/, the order driver, the models, its own file or the Frama-C setup change, and by hand | yes, each job on its pins (since phase C) |
| 12 | `coq.yml` | 12 · Coq: bits.h | VERIFY-061: bits.h's twelve residual goals, proved in Coq 8.18.0, installed into the Frama-C switch in the job. It replays the twelve proof scripts of `tools/coq/session`: each compiled by Coq as Why3 runs it, then `frama-c-bits`'s command with Coq added, in WP's batch mode, at x86_64 and the nine machine models. Runs when its own paths change, and by hand | yes: each script accepted, resting only on its own declarations, and 100 / 100 with Coq credited with twelve at each of the ten models |
| 13 | `prover-agreement.yml` | 13 · Prover agreement | VERIFY-064's measure, made a check by VERIFY-065: each unit a push can affect, proved once with each prover alone (`tools/agreement/`). The units are those that include a changed file, as the compiler lists them; a change to the script, the workflow, the Frama-C setup, or a header no unit includes runs all 27. Three jobs per unit, at most 30 at once (about an hour and 18 runner-hours for all of them), then one job per unit and a total | yes, since VERIFY-065's third run: the goals no prover proves alone are the job's pinned names, and those one or two prove are the unit's pins, proofs over 30 s left out |
| 14 | `eva.yml` | 14 · Eva: programs that use Canon-C | VERIFY-066: one small program per verified unit (`tools/eva/programs/`), which includes the unit's file with its own flags and calls its functions as a user would. Each is compiled with GCC and run under ASan and UBSan, and analysed by Eva at the ten machine models of the proofs: whether the analysis reaches the end of `main`, the alarms, and the calls whose preconditions Eva finds invalid or cannot decide. One job per model, then a total, which uploads the properties the pins hold open to code scanning, category `eva`: the alarms from MdR's SARIF, the assertion and the preconditions from Report's table (VERIFY-082, `tools/eva/mdr.py`). The job `e-acsl` instruments each program with E-ACSL, compiles it and runs it, so that it checks its annotations as it runs (VERIFY-083, `tools/eva/eacsl.py`). The job `aorai` checks three of the library's call-order rules, written as automata for Frama-C's Aorai (`tools/aorai/`): Aorai instruments each program that uses them, and each written to keep or break one, and Eva says whether it keeps the rule; and each of those programs is compiled and run, and again with `CANON_LIFETIME_DEBUG` (VERIFY-085, `tools/aorai/aorai.py`). Runs when core/, semantics/, data/, the drivers, `tools/eva/`, `tools/aorai/`, the models, its own file or the Frama-C setup change, and by hand | yes, since VERIFY-066's third run: a program that does not reach its end, or is not as pinned (`tools/eva/pins/`), fails its model's job. Since VERIFY-082's run 3, a program without its SARIF, or an alarm without MdR's open result, fails the total job, and nothing is uploaded. Since VERIFY-083's run 2, a program whose E-ACSL result is not as pinned (`tools/eva/eacsl_pins.txt`) fails the `e-acsl` job. Since VERIFY-085's run 2, a case not as `tools/aorai/aorai.py`'s `CASES` says, or a program not as its `LIFETIME_DEBUG` says, fails the `aorai` job |
| 15 | `metacsl.yml` | 15 · MetAcsl: the library's rules across its functions | VERIFY-086: two of the library's cross-cutting rules, written as meta-properties for Frama-C's MetAcsl (`tools/metacsl/rules/`): `core/primitives/contract.h`'s handler is written by `contract_set_handler` alone (at each write of every other function; and kept by every other function, at its return), and only documented functions call the heap (at each call: `malloc`, `calloc`, `realloc` and `free` only from `mem_alloc` and `mem_free`; `mem_alloc`, `mem_free` and `mem_alloc_array_checked` only from `mem_alloc_array_checked`, `vec_int_alloc` and `vec_int_free`). MetAcsl writes each as a check at each such point, and WP proves the checks on each of the 27 proof units, at the unit's model, one job for each of core, semantics and data (`tools/metacsl/metacsl.py`). Each job installs MetAcsl 0.7 in the Frama-C setup's switch (`--no-depexts`; failing that, with the system's packages), and removes it at its end. Runs when core/, semantics/, data/, the drivers, `tools/metacsl/`, agreement.py, its own file or the Frama-C setup change, and by hand. The functions whose checks WP does not prove are pinned in `tools/metacsl/pins.txt`: each calls through a pointer its caller passed, but `vec_int_fill` in the vec unit, whose loop loses its annotations there (no `-CC`) | yes, since VERIFY-086's run 2: a function not proved and not pinned, or one pinned and proved, fails its part, as does a run that gives no table or is stopped; report-only in runs 1 to 1c |
| 16 | `smoke.yml` | 16 · Smoke tests | VERIFY-087's probe, made a check by VERIFY-088: WP's smoke tests on each unit a push can affect (`tools/smoke/`), chosen as 13 chooses its units. WP adds a goal of `\false` at each function's entry, under its requires and each behavior's assumes, after each call, at each loop and in code it does not find reachable, and runs the unit with its proof job's options, every goal at 1 s and the smoke tests at 2 s. A smoke test a prover proves has failed: WP's hypotheses there contradict each other, and what it proves past them it proves of nothing. In each unit the failures must be its pins (`tools/smoke/pins/`), by name without the statement number and as many: the branches no execution reaches, and the nine calls of `memcmp` whose precondition WP compiles as false (`docs/verification.md`, "Past `memcmp`'s call"). One job a unit, the longest about 26 minutes; about 5 runner-hours when every unit runs | yes, since VERIFY-088's run 2: a unit whose failed smoke tests are not its pins, by name and count, or that has a smoke test with no result, fails its job, as does a run that is stopped, ends in an error or gives no report; report-only in run 1 |

## Reading results

Every proof job ends with a step, **Result annotation**, that posts its
summary as an annotation, for example:

    frama-c-stack: proved 5714 / 5783 (pin 5714 / 5783); unproved 69: inherited 69, own 0; 0 difference(s)

`misra` (its count, and since VERIFY-068 the count against its pins), `coverage` (the MC/DC total and each cover file's row), `compcert`,
`embedded`, the AVR check and, since VERIFY-070, the code size and build cost (one notice per target, one for the build, and since its run 2 one for the pins) do the same. Annotations are on the run's
summary page and readable through the GitHub API, so a run can be scored
without opening logs. The full log and the uploaded `wp-*` artifacts are
still there for detail.

## Where a new thing goes

- **A new test file**: add a `canon_c_test(...)` entry in `CMakeLists.txt`.
  For a test that needs a second translation unit, list both files under
  `SRC`, and name the second `X_defs.c` beside `X_test.c` so that CompCert
  links it too. Every build job then runs it, and the embedded check
  compiles it, with no workflow edit; the `cortex-m` job runs it on
  Cortex-M under QEMU, and a test that cannot pass there needs its line
  in `tools/cortexm/pins.txt`, with the reason (VERIFY-089); the
  `msp430` job builds it at a 16-bit `int`, where a test that includes a
  Tier 1 header is not built, and one that does not pass needs its line in
  `tools/msp430/pins.txt` (VERIFY-097). A test that
  includes from directories outside the library's, as
  `test/integrations/autosar/autosar_test.c` does (VERIFY-075), needs them
  on three more include paths: the embedded check's
  (`tools/embedded_compile_check.sh`), CompCert's and the static-analysis
  job's Cppcheck's (`analysis.yml`).
- **A newly verified unit**: a new job in the proof file of its layer
  (`proofs-core.yml`, `proofs-semantics.yml` or `proofs-data.yml`).
  `frama-c-stack` in `proofs-data.yml` is the shortest complete template:
  - report-only until two runs agree, then enforced;
  - the proved line and the unproved names pinned;
  - its own annotation.

  Give it a "Result annotation" step like the others. Give it an entry
  in `tools/agreement/agreement.py` (`UNITS` and `ORDER`), which
  `8 · Records` checks against its job, and its pin file in
  `tools/agreement/pins/` once `13 · Prover agreement` has posted its
  names, and a program in `tools/eva/programs/` with its entry in
  `tools/eva/eva.py` (`PROGRAMS`), which `8 · Records` also checks, and
  its pin file in `tools/smoke/pins/` (a comment alone until
  `16 · Smoke tests` has posted the smoke tests that fail), which it
  checks too.
  Add an MC/DC cover
  file (`vmacros/coverage/`, registered in `CMakeLists.txt` and run by the
  coverage job) when its conditions would otherwise land in a test file; a
  header with written-out bodies is measured in place and needs none. Either
  way, add a per-line debug step for it in the coverage job (VERIFY-055 left
  hashmap's out; VERIFY-056 added it).
- **A new platform or portability check**: a job in `platforms.yml`.
  Its Ubuntu packages go through `.github/actions/apt-install`, as
  `embedded`'s and `cortex-m`'s do (VERIFY-093): their files come from the
  Actions cache, or from Canonical's archive before Azure's mirror, each
  checked against Ubuntu's signed index; each list of packages with its
  own `cache-name`.
- **Something experimental**: a job in `probes.yml`, with its paths added to
  the `changes` job's filter. Workflows 1 to 6 ignore `probes.yml`, which
  none of them reads (VERIFY-080), so editing it starts 8 and 9 alone, and
  in 9 every probe, since each watches it. They ignore `smoke.yml` too
  (VERIFY-088): editing it starts 8 and 16 alone.
- **Any change**: `python3 tools/which_workflows.py --files FILE...` lists
  the workflows the push starts, then 13's units, 9's probes, 14's
  programs and 16's units that read the changed files. An annotation
  added inside a macro reaches the programs it marks as reading macro
  bodies (`-CC`): their Eva pins can change with it, as VERIFY-079's run 2
  found.

Keep every workflow file under GitHub's **500 KB** limit (512,000 bytes). A
file over it does not run at all, and GitHub reports only "a workflow file
issue" (VERIFY-047: CI #1394). The `workflow-size` job in `records.yml` fails
at the limit and warns from 480 KB.

## Special cases, kept as they are

- `frama-c-arena-32` gates arena's 32-bit run on equality with the 64-bit
  residual set, and checks that its embedded copy of that set matches
  `frama-c-arena`'s. `frama-c-priority-queue` reads `frama-c-memory`'s pins
  at run time, and the `-CC` workflow reads each driver job's pins. All
  three read `proofs-core.yml`, `proofs-semantics.yml` and `proofs-data.yml`
  joined into one text.
- `frama-c-win64` proves two units at two machine models in each leg, and
  gates on equal unproved sets.
- `frama-c-vec` runs about an hour and a half, `frama-c-bitset` and
  `frama-c-stack` about an hour, and the others from under a minute to 45
  minutes (at 06718b0).

## Things that stay fixed

- Job names (`build`, `frama-c-vec`, `misra`, …) are what branch protection
  and the records cite. Moving a job between files does not change its
  name.
- Every Linux job runs on `ubuntu-24.04`, never `ubuntu-latest` (see the
  note in `ci.yml`).
- A job that leaves `ci.yml` no longer waits for `build`: `needs:` cannot
  cross workflow files.
