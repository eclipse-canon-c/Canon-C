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
| 2 | `platforms.yml` | 2 · Platforms | musl, Linux AArch64, C11 to C23, big-endian s390x (qemu), embedded compile checks (Cortex-M, RISC-V, AVR tiers) | yes |
| 3 | `analysis.yml` | 3 · Static analysis & coverage | clang-tidy and Cppcheck, MISRA C:2012, MC/DC and branch coverage, CompCert | static-analysis and coverage's threshold: yes. MISRA: yes, since VERIFY-068's third run: a real finding not as pinned by file and rule (`tools/misra/pins.txt`, each line naming its record) fails the job, as does a canary not seen. CompCert: informational |
| 4 | `proofs-core.yml` | 4 · Proofs: core/ | Frama-C WP, one job per unit of core/ and core/primitives/, plus arena at 32-bit and memory/bits at LLP64 | yes, each job on its pins |
| 5 | `proofs-semantics.yml` | 5 · Proofs: semantics/ | WP for error, option, result, borrow, diag | yes |
| 6 | `proofs-data.yml` | 6 · Proofs: data/ | WP for vec, deque, bitset, priority_queue, range, vec_range, vec_fmt, stringbuf, stack, array, hashmap | yes |
| 7 | `proofs-cc.yml` | 7 · Proofs: -CC | The macro-body contracts proved under `-CC` (VERIFY-025), per family, for the six families whose driver jobs above run under `-C` (slice, option, result, borrow, vec, deque). The units written since (vec_range, vec_fmt, stack, array, hashmap) are preprocessed with `-CC` in their own jobs in `proofs-data.yml`, so their macro-body contracts are proved there and they have no job here | per family's gates |
| 8 | `records.yml` | 8 · Records | Record coverage (argument blocks against roll-calls), the machine models' pins checked against x86_64's (VERIFY-060, `tools/machdeps/check_pins.py`), the prover agreement's units checked against the proof jobs (VERIFY-065, `tools/agreement/agreement.py check`), Eva's programs checked against the units and the models (VERIFY-066, `tools/eva/eva.py check`), the misra job's pins checked against their records (VERIFY-068, `tools/misra/misra.py check`), and the workflow-size guard | yes |
| 9 | `probes.yml` | 9 · Probes | The closed experiments of VERIFY-039/040 (G4, G11, G13, G19, Count, Round 3), VERIFY-057's pointer probe (every unit with `-warn-invalid-pointer`, report-only), VERIFY-060's char probe (arena with `char`'s signedness flipped at x86_64 and AArch64), and VERIFY-066's Eva error probe (Frama-C 29's error on entering `arena_alloc`: small programs that reproduce it, and `arena.c` three ways, report-only). Each runs only when its own paths change | a probe's MISS fails it; the pointer probe fails only without a summary, the Eva error probe only if it cannot set up a variant |
| 9 | `f4-control.yml`, `vc-identity.yml` | 9 · Probes (manual): … | Two experiments run only by hand | n/a |
| 10 | `machdeps.yml` | 10 · Machine models | VERIFY-060's machine models for Arm, RISC-V and big-endian PowerPC: generated with Frama-C's make-machdep from the cross-compilers, compared with the committed copies in `tools/machdeps/`; what Frama-C sees at each, whether the core/ units parse, and whether GCC mode changes their code. Runs when its own paths change | the drift check: yes (since phase C). The reads: no |
| 11 | `proofs-machdeps.yml` | 11 · Proofs: machine models | VERIFY-060's proofs of the ten core/ units at nine machine models besides x86_64: the seven of `tools/machdeps/` (Cortex-M4 with short enums, RV32 and RV64 bare metal, 32-bit Arm, RV64 and AArch64 Linux, big-endian 32-bit PowerPC Linux) and Frama-C's `ppc_32` and `msvc_x86_64`; ninety jobs, a matrix of model and unit, at most 30 at once, each on its class's pins (`tools/machdeps/pins/`). Runs when core/, the models, its own file or the Frama-C setup change, and by hand | yes, each job on its pins (since phase C) |
| 12 | `coq.yml` | 12 · Coq: bits.h | VERIFY-061: bits.h's twelve residual goals, proved in Coq 8.18.0, installed into the Frama-C switch in the job. It replays the twelve proof scripts of `tools/coq/session`: each compiled by Coq as Why3 runs it, then `frama-c-bits`'s command with Coq added, in WP's batch mode, at x86_64 and the nine machine models. Runs when its own paths change, and by hand | yes: each script accepted, resting only on its own declarations, and 100 / 100 with Coq credited with twelve at each of the ten models |
| 13 | `prover-agreement.yml` | 13 · Prover agreement | VERIFY-064's measure, made a check by VERIFY-065: each unit a push can affect, proved once with each prover alone (`tools/agreement/`). The units are those that include a changed file, as the compiler lists them; a change to the script, the workflow, the Frama-C setup, or a header no unit includes runs all 26. Three jobs per unit, at most 30 at once (about an hour and 18 runner-hours for all 26), then one job per unit and a total | yes, since VERIFY-065's third run: the goals no prover proves alone are the job's pinned names, and those one or two prove are the unit's pins, proofs over 30 s left out |
| 14 | `eva.yml` | 14 · Eva: programs that use Canon-C | VERIFY-066: one small program per verified unit (`tools/eva/programs/`), which includes the unit's file with its own flags and calls its functions as a user would. Each is compiled with GCC and run under ASan and UBSan, and analysed by Eva at the ten machine models of the proofs: whether the analysis reaches the end of `main`, the alarms, and the calls whose preconditions Eva finds invalid or cannot decide. One job per model, then a total. Runs when core/, semantics/, data/, the drivers, `tools/eva/`, the models, its own file or the Frama-C setup change, and by hand | yes, since VERIFY-066's third run: a program that does not reach its end, or is not as pinned (`tools/eva/pins/`), fails its model's job |

## Reading results

Every proof job ends with a step, **Result annotation**, that posts its
summary as an annotation, for example:

    frama-c-stack: proved 5714 / 5783 (pin 5714 / 5783); unproved 69: inherited 69, own 0; 0 difference(s)

`misra` (its count, and since VERIFY-068 the count against its pins), `coverage` (the MC/DC total and each cover file's row), `compcert`,
`embedded` and the AVR check do the same. Annotations are on the run's
summary page and readable through the GitHub API, so a run can be scored
without opening logs. The full log and the uploaded `wp-*` artifacts are
still there for detail.

## Where a new thing goes

- **A new test file**: add a `canon_c_test(...)` entry in `CMakeLists.txt`.
  For a test that needs a second translation unit, list both files under
  `SRC`, and name the second `X_defs.c` beside `X_test.c` so that CompCert
  links it too. Every build job then runs it, and the embedded check
  compiles it, with no workflow edit.
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
  `tools/eva/eva.py` (`PROGRAMS`), which `8 · Records` also checks.
  Add an MC/DC cover
  file (`vmacros/coverage/`, registered in `CMakeLists.txt` and run by the
  coverage job) when its conditions would otherwise land in a test file; a
  header with written-out bodies is measured in place and needs none. Either
  way, add a per-line debug step for it in the coverage job (VERIFY-055 left
  hashmap's out; VERIFY-056 added it).
- **A new platform or portability check**: a job in `platforms.yml`.
- **Something experimental**: a job in `probes.yml`, with its paths added to
  the `changes` job's filter.

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
