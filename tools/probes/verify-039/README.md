# VERIFY-039 probes

Executable checks of filings the record makes, against the code they describe.
Findings, predictions and scoring live in `docs/deviations.md`, VERIFY-039.
These files are recorded probes, not tests: nothing builds them (CMake uses
explicit targets) and CI ignores `tools/**`. Each becomes a regression test
when its fix lands.

Run from the repository root. Exact commands are in each file's header. The
G1 probes use the bare `-CC` driver `vmacros/vdrivers/vec_cc_experiment.h`, the
same `vec<int>` instantiation CI verifies; the G2 probe includes `core/arena.h`.

| Probe | Finding | Filing | Expected today |
|---|---|---|---|
| `p_pop_frame.c` | G1 | VERIFY-018 cat (d): `vec_int_pop_ok_ensures_4_part5` | unchanged at run time: `pop(&v, &v.items[0])` on [10, 20, 30] leaves [30, 20], ensures 1–3 hold, the frame (ensures 4) is `VIOLATED`; exit 1, sanitizers clean. Since G1 (40cccd9) the call is outside the contract, which requires `out` to be separated from the buffer |
| `p_remove_frame.c` | G1 | VERIFY-018 cat (d): `vec_int_remove_ok_ensures_4_part6`, `_4_part7`, `_5_part6` | unchanged at run time: `out` before `i` violates ensures 4, `out` after `i` violates ensures 5; exit 1, sanitizers clean. Since G1 (40cccd9) both calls are outside the contract |
| `p_try_alloc_assigns.c` | G2 | VERIFY-009 Cat 2c: `arena_try_alloc_assigns_normal_part03`, `arena_try_alloc_aligned_assigns_normal_part03` | `arena_try_alloc(&a, 8, &p)` and the aligned variant change the caller's `p`, which lies outside `*arena`: the default `assigns *arena` is `VIOLATED`; exit 1, sanitizers clean |

All three build with any C99 compiler; add
`-fsanitize=address,undefined -fno-sanitize-recover=all` to confirm that the
violating calls are defined C, and `-DCANON_NO_REQUIRE -DNDEBUG` for the proof
configuration. Since G1's fix (40cccd9) the same calls fall outside the
contract, as `p_pool_null.c`'s did after VERIFY-036 F2: the run-time behaviour
is unchanged, and what changed is that the contract no longer admits the call.
The three frame fragments closed at CI #1341 and CC #28; the shift fragment,
`_5_part6`, is now true and stays residual under category (d)'s mechanism.
