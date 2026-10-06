# VERIFY-036 probes

Executable checks of four in-force discharge arguments against the code they
describe. Findings, predictions and scoring live in `docs/deviations.md`,
VERIFY-036. These files are recorded probes, not tests: nothing builds them
(CMake uses explicit targets) and CI ignores `tools/**`. Each becomes a
regression test when its fix lands, except `p_arena_span.c`, which needs
ASan's pointer-pair checking.

Run from the repository root. Exact commands are in each file's header.

| Probe | Finding | Block | Expected today |
|---|---|---|---|
| `p_getalign.c` | F1 | VERIFY-008 Cat 2, goal 7 | fixed by F1 (f1ba2de): runs clean; before it, UBSan reported a signed negation overflow |
| `p_pool_null.c` | F2 | VERIFY-010 Cat 2b | unchanged at run time (segfault in the proof configuration, abort in the default build); since F2 (8163f06) the call is outside the contract, which now requires a live pool |
| `p_arena_span.c` | F3 | VERIFY-009 Cat 2a | ASan `invalid-pointer-pair` in `ptr_span`, called from `arena_alloc` |
| `p_arena_pad.c` | F4 | VERIFY-009 Cat 2b | three `VIOLATED` lines, in both build configurations |
| `p_zero_assigns.c` | F5 | VERIFY-009 Cat 2c, VERIFY-010 Cat 2d | the zeroed bytes change, as designed; since F5 (c5281dc) they lie inside the declared assigns sets |

`p_arena_span.c` needs a compiler whose ASan supports
`-fsanitize=pointer-subtract` (GCC or LLVM clang on Linux). The others build
with any C99 compiler.
