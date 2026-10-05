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
| `p_getalign.c` | F1 | VERIFY-008 Cat 2, goal 7 | UBSan: signed negation overflow in `mem_get_alignment` |
| `p_pool_null.c` | F2 | VERIFY-010 Cat 2b | segfault under `-DCANON_NO_REQUIRE -DNDEBUG`; contract abort in the default build |
| `p_arena_span.c` | F3 | VERIFY-009 Cat 2a | ASan `invalid-pointer-pair` in `ptr_span`, called from `arena_alloc` |
| `p_arena_pad.c` | F4 | VERIFY-009 Cat 2b | three `VIOLATED` lines, in both build configurations |

`p_arena_span.c` needs a compiler whose ASan supports
`-fsanitize=pointer-subtract` (GCC or LLVM clang on Linux). The others build
with any C99 compiler.
