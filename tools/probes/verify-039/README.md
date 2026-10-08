# VERIFY-039 probes

Executable checks of filings the record makes, against the code they describe.
Findings, predictions and scoring live in `docs/deviations.md`, VERIFY-039.
These files are recorded probes, not tests: nothing builds them (CMake uses
explicit targets) and CI ignores `tools/**`. Each becomes a regression test
when its fix lands.

Run from the repository root. Exact commands are in each file's header; both
probes use the bare `-CC` driver `vmacros/vdrivers/vec_cc_experiment.h`, the
same `vec<int>` instantiation CI verifies.

| Probe | Finding | Filing | Expected today |
|---|---|---|---|
| `p_pop_frame.c` | G1 | VERIFY-018 cat (d): `vec_int_pop_ok_ensures_4_part5` | `pop(&v, &v.items[0])` on [10, 20, 30] leaves [30, 20]: ensures 1–3 hold, the frame (ensures 4) is `VIOLATED`; exit 1, sanitizers clean |
| `p_remove_frame.c` | G1 | VERIFY-018 cat (d): `vec_int_remove_ok_ensures_4_part6`, `_4_part7`, `_5_part6` | `out` before `i` violates ensures 4, `out` after `i` violates ensures 5; exit 1, sanitizers clean |

Both build with any C99 compiler; add
`-fsanitize=address,undefined -fno-sanitize-recover=all` to confirm that the
violating calls are defined C, and `-DCANON_NO_REQUIRE -DNDEBUG` for the proof
configuration. After G1's fix the same calls fall outside the contract, as
`p_pool_null.c` did after VERIFY-036 F2: the run-time behaviour is unchanged,
and what changes is that the contract no longer admits the call.
