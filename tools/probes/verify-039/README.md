# VERIFY-039 probes

Executable checks of filings the record makes, against the code they describe.
Findings, predictions and scoring live in `docs/deviations.md`, VERIFY-039.
These files are recorded probes, not tests: nothing builds them (CMake uses
explicit targets) and the main CI ignores `tools/**`. Each becomes a
regression test when its fix lands. The G4 probe is also run under WP, by its
own workflow, `.github/workflows/g4-probe.yml`.

Run from the repository root. Exact commands are in each file's header. The
G1 probes use the bare `-CC` driver `vmacros/vdrivers/vec_cc_experiment.h`, the
same `vec<int>` instantiation CI verifies; the G2 probe includes `core/arena.h`,
the G4 probes `core/memory.h`, and for the listing `core/arena.h`,
`core/pool.h`, `data/stringbuf.h`, `data/bitset.h`, `semantics/diag.h` and the
bare `-CC` drivers for `vec<int>` and `vec<Rec>`; the G5 and G7 probes include
`core/arena.h` and `core/pool.h`.

| Probe | Finding | Filing | Expected today |
|---|---|---|---|
| `p_pop_frame.c` | G1 | VERIFY-018 cat (d): `vec_int_pop_ok_ensures_4_part5` | unchanged at run time: `pop(&v, &v.items[0])` on [10, 20, 30] leaves [30, 20], ensures 1–3 hold, the frame (ensures 4) is `VIOLATED`; exit 1, sanitizers clean. Since G1 (40cccd9) the call is outside the contract, which requires `out` to be separated from the buffer |
| `p_remove_frame.c` | G1 | VERIFY-018 cat (d): `vec_int_remove_ok_ensures_4_part6`, `_4_part7`, `_5_part6` | unchanged at run time: `out` before `i` violates ensures 4, `out` after `i` violates ensures 5; exit 1, sanitizers clean. Since G1 (40cccd9) both calls are outside the contract |
| `p_try_alloc_assigns.c` | G2 | VERIFY-009 Cat 2c: `arena_try_alloc_assigns_normal_part03`, `arena_try_alloc_aligned_assigns_normal_part03` | for both functions, only the caller's `p` changes: the pre-G2 default `assigns *arena` is `VIOLATED`, the current `assigns *arena, *out` holds on every watched location; exit 0, sanitizers clean (regression test since G2, f4a846a; both goals closed at CI #1343 and CC #30) |
| `p_frame_typed.c` | G4 | VERIFY-005's hypothesis: frames proved across writes stated through `char *` | at run time, every `_frame` claim (the object unchanged after `mem_zero`) is `VIOLATED` and every `_zero` claim holds, at `int`, `u8` and `char`; exit 0, sanitizers clean. Under WP (`g4-probe.yml`), predicted before it ran: the `int` and `u8` frames prove although false, the `char` zero proves, the other three do not (VERIFY-039 G4) |
| `p_arena_self.c` | G4, entry 1 | `arena_alloc_zero`, `arena_alloc_aligned_zero` (`arena_invariant`, the address `ensures`), `arena_reset_secure` (the `buffer` and `capacity` frames, `arena_invariant`), `stringbuf_init_arena` (`arena_kept`): proved in every unit that has them, false for an `Arena` inside its own buffer | the `Arena` placed inside the 256-byte heap block it manages, each call's write covering it: all eight `VIOLATED`; exit 1, sanitizers clean, at LP64 and ILP32. Since the G4 arena repair the calls are outside `arena_init`'s contract, which requires the `Arena` to lie outside its buffer; run-time behaviour unchanged |
| `p_pool_self.c` | G4, entries 2 and 1 | `pool_reset_secure`'s `pool_invariant`: proved in `frama-c-pool`, false when the zeroed slots cover the `Pool` (case B, entry 2) or the `Arena` (case A, entry 1) | both `VIOLATED` in the proof configuration; case B also in the default build, where case A is skipped (`ensure_msg` stops `pool_reset`); exit 1, sanitizers clean. Since the G4 arena repair case A is outside `arena_init`'s contract, and since the G4 pool repair case B is outside `pool_init`'s; run-time behaviour unchanged |
| `p_pool_arena.c` | G5 | pool Cat 2a: `pool_init_ensures_part4` (`pool_invariant`), unproved, filed under arithmetic | the `Arena` placed at the `Pool`'s `base_mark` field, so that the stores of `base_mark` and `end_mark` overwrite the `Arena`'s `buffer` and `capacity`: `pool_init` returns true, ensures 2 and 3 hold, ensures 1 `VIOLATED` on the overwritten bytes (read through `memcpy`); exit 1, sanitizers clean, both build configurations, LP64 and ILP32. Since the G5 repair the call is outside `pool_init`'s contract; run-time behaviour unchanged |
| `p_pool_reset_mark.c` | G7 | pool Cat 2d: `pool_reset_call_arena_reset_to_requires_2`, unproved, filed under arena delegation | `base_mark` 16, then `arena_reset`: `pool_invariant` as it stood before G7 holds, and `mark <= offset`, the call-site precondition and, since the G7 repair, `pool_invariant`'s last conjunct, is `VIOLATED`; in the proof configuration `pool_reset` returns with the offset moved forward, exit 1; in the default build `arena_reset_to`'s `require_msg` aborts (exit 134); sanitizers clean, LP64 and ILP32. So the `pool_reset` call is now outside its contract; checks and exit status unchanged |
| `p_bitset_self.c` | G4, entry 3; G6 | `bitset_clear_all` (`bitset_mut`), `bitset_set_all` (the call to `bitset_clear_padding`, `bitset_mut`): proved in `frama-c-bitset`, false for a `Bitset` whose fields lie in its own words; G6: `bitset_{set,clear,toggle}_live_ensures_4_part3`, residuals in VERIFY-020's single-bit family, and `bitset_assign`'s live `ensures bitset_mut(bs)`, proved: false on LP64 Linux for the same `Bitset` | the `Bitset` at the start of a heap block, its words starting at its `word_count`: `bitset_mut` holds before each call and is `VIOLATED` after it, three times, and on LP64 Linux five more times after `bitset_set`, `bitset_clear`, `bitset_toggle` and both `bitset_assign`s (G6); exit 1, sanitizers clean, both build configurations, LP64 and ILP32 |
| `p_diag_render.c` | G4, entry 4 | `diag_render`'s loop invariant `render_i_bounds`: proved in `frama-c-diag`, false when `buf` covers `d->depth` | `buf` inside the `Diag` from the last frame's message to the end of `depth`, frame 0's line sized so its terminating `'\0'` lands on `depth`: `depth` 2 becomes 0 after one iteration, the invariant `VIOLATED` at the second head, the postcondition holds; exit 1, sanitizers clean, both build configurations, LP64 and ILP32 |
| `p_vec_append_self.c` | G4, entry 5 | `append_array`'s ensures 3 (old elements unchanged): proved in `frama-c-vec` and `cc-vec`, false for a `vec_int` in its own free tail | the vec at `items[len]`, the appended elements the bytes of a pointer stored as an `int *`: ensures 1 and 2 hold, ensures 3 `VIOLATED`, read through the field's own type; exit 1, sanitizers clean, both build configurations, LP64 and ILP32 |
| `p_vec_rec_live.c` | G4, entry 5 | the frames of `insert`, `remove`, `push`, `try_push`, `push_unchecked`, `pop` and `append_array`: proved in `cc-vec_struct`, false for a `vec_Rec` whose `len` lies on `Rec[0].due` | the vec at `items[0]` on LP64: seven frames `VIOLATED`, `Rec[0].due` read as an `i64`, the counterpart of the `usize` stored there; exit 1, sanitizers clean, both build configurations. On ILP32 `len` covers only half of `due`: "not applicable", exit 0 |

All twelve build with gcc and clang at `-std=c99` (`p_try_alloc_assigns.c` uses C11's `_Alignas`, accepted there as an extension); add
`-fsanitize=address,undefined -fno-sanitize-recover=all` to confirm that the
violating calls are defined C, and `-DCANON_NO_REQUIRE -DNDEBUG` for the proof
configuration. Since G1's fix (40cccd9) the same calls fall outside the
contract, as `p_pool_null.c`'s did after VERIFY-036 F2: the run-time behaviour
is unchanged, and what changed is that the contract no longer admits the call.
The three frame fragments closed at CI #1341 and CC #28; the shift fragment,
`_5_part6`, is now true and stays residual under category (d)'s mechanism.
