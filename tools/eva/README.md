# Eva on programs that use Canon-C (VERIFY-066)

WP proves each of Canon-C's functions against its contract: given its
preconditions, the function has no runtime error and ensures its
postconditions. Whether a program that calls the library meets those
preconditions is the program's question, and WP answers it only for code it
is run on. Eva, Frama-C's abstract interpreter, answers it for a whole
program, from `main`: it computes the values the program can take, and
reports each place where a runtime error may happen (an alarm) and each call
whose precondition may not hold.

`programs/` holds one small program per verified unit, 26 in all, one for
each unit of `tools/agreement/agreement.py`. Each:

- includes its unit's file, and is preprocessed with the unit's own flags,
  so that Eva reads the code and the contracts WP proves (a program for a
  generic container includes the driver its proof instantiates, such as
  `vmacros/vdrivers/vec_verify.h` for `vec_int`);
- calls the unit's functions as a user would, with values the contracts
  allow, and with the values the functions are written to refuse (a full
  container, an index out of range, an overflow);
- checks what each call returns (`CHECK`) and marks its end (`END`), as
  `programs/eva_program.h` says.

`eva.py` runs them in `14 · Eva: programs that use Canon-C`
(`.github/workflows/eva.yml`):

- **Natively.** Each program is compiled with GCC (`-std=c11 -Wall -Wextra
  -Werror`, AddressSanitizer and UndefinedBehaviorSanitizer) and run. It must
  return 0: every check holds when the program runs.
- **Under Eva**, at the ten machine models the proofs use: x86_64, and the
  nine of `proofs-machdeps.yml` (Cortex-M4, RV32 and RV64 bare metal, 32-bit
  Arm, RV64 and AArch64 Linux, big-endian 32-bit PowerPC Linux, and
  Frama-C's own `ppc_32` and `msvc_x86_64`). For each program and model it
  reports:
  - whether the analysis reached the end of `main`, and the values Eva
    computed for the count of failed checks there (`{0}`: Eva found every
    check to hold);
  - the alarms;
  - the calls whose precondition Eva found invalid or could not decide,
    those the program makes and those the library's own code makes;
  - the program's checks Eva could not confirm, by line.

Eva is told three things beyond the defaults (`eva.py`, with the reasons):
`-eva-precision 3 -eva-split-return full` for every program;
`-eva-use-spec ptr_align_padding` for a program that includes
`core/primitives/ptr.h`, since Eva cannot compute an address modulo an
alignment (it does not know where an object lies in memory) and the
function's contract, which WP proves, bounds the result; and
`-warn-special-float none` for `compare.c`, which sorts a NaN.

What it shows, and what it does not:

- An alarm or a precondition Eva cannot decide is not a bug found: Eva
  over-approximates, and a property it cannot decide may hold. Each one is
  read, and is either fixed or argued.
- A precondition Eva finds invalid is broken on every path Eva considered
  to reach the call: a bug in the program, in the contract, or in Eva's
  reading of it.
- No alarm and every call valid holds for these programs, at these models,
  under Eva's assumptions (its machine model and Frama-C's specifications of
  the C library), not for every program that uses Canon-C.

## Pins

`pins/MODEL.txt` keeps, for each program, what Eva gives at that model: the
program's line, without the time, then each property Eva leaves open, as
`eva.py` prints them (VERIFY-066 run 2). `eva.py run` compares each program
with its block and says "as pinned", "not as pinned" (with the difference)
or "no pin". A pin changes only with a record that says why; `eva.py pin
DIR` writes the files from a run's summaries.

Open at x86_64: 3 alarms; 64 calls the programs make, and 13 the library
makes, whose precondition Eva cannot decide; 40 checks it cannot confirm.
None is invalid. The other nine models leave the same open, but for
`borrow.c`'s four calls, open at x86_64 only, and `ptr.c`'s two
preconditions, whose text names the model's largest `usize`. Each holds,
for the reason given below.

Two limits of Eva's account for most of them. Its domains keep a range for
each value, and do not relate one value to another: Eva cannot confirm
`mark <= arena->offset` when both are ranges, nor a property of every
element of an array. And it does not know where an object lies in memory,
so an address converted to an integer is unknown to it; `arena_alloc` pads
each allocation from the address it starts at, so an arena's offsets, and
the pointers made from them, are ranges.

### Calls the programs make

| program | calls (x86_64) | why the precondition holds |
|---------|----------------|----------------------------|
| `pool.c` | 24: `pool_invariant`, or `pool_invariant_read`, at every call after `pool_init` | `pool_init` ensures `pool_invariant` when it succeeds, which the check at line 19 confirms; each allocation and reset ensures it again, and every query assigns nothing. Part 4 of `pool_init`'s postcondition is a residual WP leaves, argued in VERIFY-010's Category 2a (`docs/verification.md`). |
| `hashmap.c` | 16: `hashmap_inv_`, or `hashmap_inv_read_`, after the first insertion | `hashmap_init` ensures it when it succeeds, and the program returns otherwise; `insert`, `remove` and `clear` ensure it; the queries and `iter_next` assign nothing of the map. The program's write at line 36 goes through `get_or_null`'s result, which points at a slot's `value`, a field the invariant does not mention. |
| `stringbuf.c` | 11: `stringbuf_ok`, or `stringbuf_wf`, after `stringbuf_append_fmt` (line 26), and on the arena-backed buffer | `stringbuf_init_buffer` and `stringbuf_init_arena` ensure `stringbuf_wf`; each append ensures it when it succeeds, and `stringbuf_unchanged`, which keeps it, when it fails; `stringbuf_truncate` and `stringbuf_clear` ensure it; the queries assign nothing. Eva reads `vsnprintf` by its specification, which does not give the bytes it writes, so it does not see the terminator `stringbuf_wf` asks for. |
| `vec_fmt.c` | 3: `stringbuf_ok`, or its read form, at lines 14, 29 and 30 | `vec_int_to_stringbuf` ensures `stringbuf_wf` when it formats (lines 29 and 30). At line 14, in the callback `hex`: `stringbuf_clear` (line 30) ensures it, and each `stringbuf_append_fmt` the callback makes keeps it; nothing else writes the buffer while `vec_int_to_stringbuf_cb` runs. |
| `diag.c` | 3: `frame_strings_ok` of a frame (line 41), or of every frame (lines 43 and 45) | Each frame's `file` and `func` are the string literals `DIAG_PUSH` and the program pass, and `diag_push` ensures the message it stores is terminated. Its contract says so of the frame it pushes only: of the frames before, it keeps their `code`, not their strings. Its body keeps them whole: a push writes the new frame only, and when the stack is full, shifts whole frames. Argued from the body (VERIFY-066, run 2). |
| `arena.c` | 1: `mark <= arena->offset`, at line 39 | `arena_mark` ensures its result is the offset (line 37); `arena_alloc` ensures the offset grows by the size when it succeeds and stays when it fails (line 38). |
| `region.c` | 1: `region_end`'s `region_hook_rule`, for each hook | `region_hook_rule` has no definition: its meaning is its comment in `core/region.h`, which no tool reads. The program's two hooks write `order` and `ran` only: they do not end the region, move its storage, or touch its arena or its slots. |
| `ptr.c` | 1: `ptr_align_up`'s `fits`, at line 36 | `odd` is one byte into `words`, 64 bytes long: its address plus 7 is an address inside `words`, so it does not pass the largest `usize`. |
| `borrow.c` | 4, at x86_64 only: `source_live`, `\object_pointer(b->ptr)`, at lines 19 and 22 | The pointers are `&value` and `NULL`, both object pointers. At Frama-C's own `x86_64` and `x86_32` models, Eva does not decide `\object_pointer` of a pointer read from a structure returned by value; the other nine models do, and so does x86_64 for the same pointer read directly (a 15-line program, with Frama-C 25 here). |

### Calls the library makes

Each of the 13 is a call inside one of Canon-C's functions, and WP proves its
precondition when it proves that function: in `arena_free_bytes` and
`arena_free_cbytes` (of `bytes_from` and `cbytes_from`); `pool_alloc_zero`,
`pool_try_alloc` and `pool_try_alloc_zero` (of `pool_alloc` and
`pool_alloc_zero`); `pool_reset_secure` (of `pool_reset`);
`hashmap_contains_key` (of `hashmap_get`); `stringbuf_append` (of
`_stringbuf_append_bytes`); `stringbuf_append_fmt` (of
`stringbuf_append_fmt_va`); `stringbuf_append_fmt_va` (of `vsnprintf`);
`vec_int_to_stringbuf` (of `stringbuf_append_fmt`); `diag_render` (of
`snprintf`); `ptr_align_up` (of `align_up`). None is among the residuals the
proof jobs pin. Each holds when the calling function's own precondition
does, which Eva checks at the program's call: valid, or one of the calls
above. One reads differently in the two tools: WP proves `diag_render`'s
call of `snprintf` against `diag.h`'s own specification of it (VERIFY-017's
trusted stdio axioms), which asks `\valid(buf + (0 .. size - 1))`; Eva reads
Frama-C's, `snprintf_va_2`, whose precondition is that or another
condition.

### Alarms

| program | alarm | why no runtime error happens |
|---------|-------|------------------------------|
| `ptr.c` | 2, at line 38: `\pointer_comparable` of `up`, then of `down`, with `NULL` | `ptr_align_up` and `ptr_align_down` convert an integer to a pointer (`(void *)(uintptr_t)...`), which C leaves implementation-defined (C11 6.3.2.3, paragraph 5). On the flat address spaces Canon-C supports, the address of a byte of an object, converted back, points at that byte, and compares with `NULL` as any pointer does. Eva does not take a pointer made from an integer to point into an object. |
| `vec_fmt.c` | 1, at line 29: `\dangling` of the byte `stringbuf_str(&sb)[0]` | The byte is one `vsnprintf` wrote. Frama-C's specification of `vsnprintf` makes the bytes it writes depend on its `va_list`, which holds addresses of locals that end with the call, so Eva cannot rule out that a byte is such an address. A character written for `%d` is not an address; the program reads `'7'`. |

### Checks Eva cannot confirm

Each holds when the program runs (`native`, under AddressSanitizer and
UndefinedBehaviorSanitizer). Eva cannot confirm the 40 because:

| cause | checks |
|-------|--------|
| A result of a C library function Eva reads by its specification, which does not give it: `memcmp`'s, and `vsnprintf`'s or `snprintf`'s and the lengths that follow from them | `array.c` 29, 31, 33; `memory.c` 26, 27, 41; `slice.c` 22, 26, 47, 48, 50; `stringbuf.c` 26, 27, 28, 29, 34; `vec_fmt.c` 28, 29, 31, 32, 34; `diag.c` 42, 44 |
| An address: whether two objects overlap, an address's alignment, a pointer made from an integer, an id mixed with its owner's address (WP proves the id is not 0) | `memory.c` 20, 23; `ptr.c` 38, 39; `lifetime.c` 17 |
| A relation between values Eva keeps as ranges, an arena's offsets | `arena.c` 35, 40, 42, 43, 45, 46; `pool.c` 35 |
| Zeros written through a pointer whose offset is a range: Eva keeps the bytes' earlier values among the possible ones. WP leaves the zeroing postconditions too, argued in VERIFY-009's Category 2c (VERIFY-039, G2 and G3) | `arena.c` 27, 29; `pool.c` 26 |
| Zeros written through a `volatile` pointer, as secure zeroing does so that the compiler keeps the writes: Eva does not keep a value written through `volatile` | `memory.c` 35, 46 |

Enforced since VERIFY-066's third run (`ENFORCE` in `eva.py`): a program
that does not reach its end, or is not as pinned, fails its model's job.

    python3 tools/eva/eva.py list
    python3 tools/eva/eva.py native [PROGRAM...]
    python3 tools/eva/eva.py run MODEL [PROGRAM...]
    python3 tools/eva/eva.py report [DIR]
    python3 tools/eva/eva.py pin DIR [NOTE]
    python3 tools/eva/eva.py check

Frama-C 25 needs `EVA_FRAMAC_ARGS=-c11`, which 29, the CI's, does not.
`8 · Records` runs `eva.py check`: every unit has a program and every program
includes its unit's file; the models are the proofs' and the workflow's;
every model has a pin file, with a block for every program.
