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
| 3 | `analysis.yml` | 3 · Static analysis & coverage | clang-tidy and Cppcheck, MISRA C:2012, MC/DC and branch coverage, CompCert | static-analysis and coverage's threshold: yes. MISRA: advisory. CompCert: informational |
| 4 | `proofs-core.yml` | 4 · Proofs: core/ | Frama-C WP, one job per unit of core/ and core/primitives/, plus arena at 32-bit and memory/bits at LLP64 | yes, each job on its pins |
| 5 | `proofs-semantics.yml` | 5 · Proofs: semantics/ | WP for error, option, result, borrow, diag | yes |
| 6 | `proofs-data.yml` | 6 · Proofs: data/ | WP for vec, deque, bitset, priority_queue, range, vec_range, vec_fmt, stringbuf, stack | yes |
| 7 | `proofs-cc.yml` | 7 · Proofs: -CC | The macro-body contracts proved under `-CC` (VERIFY-025), per family | per family's gates |
| 8 | `records.yml` | 8 · Records | Record coverage (argument blocks against roll-calls) and the workflow-size guard | yes |
| 9 | `probes.yml` | 9 · Probes | The closed experiments of VERIFY-039/040 (G4, G11, G13, G19, Count, Round 3). Each runs only when its own paths change | a probe's MISS fails it |
| 9 | `f4-control.yml`, `vc-identity.yml` | 9 · Probes (manual): … | Two experiments run only by hand | n/a |

## Reading results

Every proof job ends with a step, **Result annotation**, that posts its
summary as an annotation, for example:

    frama-c-stack: proved 5714 / 5783 (pin 5714 / 5783); unproved 69: inherited 69, own 0; 0 difference(s)

`misra`, `coverage` (the MC/DC total and each cover file's row), `compcert`,
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

  Give it a "Result annotation" step like the others. Add an MC/DC cover
  file (`vmacros/coverage/`, registered in `CMakeLists.txt` and run by the
  coverage job), and a per-line debug step for it.
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
- `frama-c-vec` runs about three hours. The others run from minutes to about
  an hour.

## Things that stay fixed

- Job names (`build`, `frama-c-vec`, `misra`, …) are what branch protection
  and the records cite. Moving a job between files does not change its
  name.
- Every Linux job runs on `ubuntu-24.04`, never `ubuntu-latest` (see the
  note in `ci.yml`).
- A job that leaves `ci.yml` no longer waits for `build`: `needs:` cannot
  cross workflow files.
