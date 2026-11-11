# Using Canon-C with Eva

WP proves each of Canon-C's functions against its contract: given its
preconditions, the function has no runtime error and ensures its
postconditions. Whether your program calls each function with its
preconditions met is your program's question. Eva, Frama-C's abstract
interpreter, answers it for a whole program: from `main`, it computes the
values the program can take, and reports each place where a runtime error
may happen (an alarm) and each call whose precondition may not hold.

Canon-C runs Eva on a program for each of its units, at the ten machine
models of its proofs (`14 · Eva`, `tools/eva/`, VERIFY-066). This guide is
for running it on yours.

## What you need

- **Frama-C 29.0 (Copper)**, the version Canon-C's CI uses (from opam, on
  OCaml 4.14.2: `.github/actions/framac-setup`). Other versions read some
  contracts differently (below).
- **Your program**, with a `main`, and Canon-C's directories on its include
  path.
- **`CANON_CONTRACT_IMPL`** defined in exactly one of its files, before that
  file includes Canon-C (`core/primitives/contract.h` says why).

## The command

For a program that includes `core/arena.h`, at the Cortex-M4 model,
Canon-C's own runs use:

    frama-c -eva -eva-precision 3 -eva-split-return full \
      -machdep tools/machdeps/machdep_cortex_m4.yaml -enums gcc-short-enums \
      -eva-use-spec ptr_align_padding \
      -cpp-extra-args="-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG" \
      your_program.c

- **`-machdep`** is your target's machine model: its type sizes and
  alignments, the signedness of `char`, the byte order. Frama-C has models
  of its own (`x86_64`, `x86_32`, `ppc_32`, `msvc_x86_64` among them);
  `tools/machdeps/` has seven more, generated from cross-compilers, and its
  README says how to make one for your target. `-enums gcc-short-enums`
  goes with the Cortex-M4 model, whose compiler packs enums.
- **`-eva-precision 3 -eva-split-return full`** ask for more precision than
  Eva's defaults; higher values cost time.
- **`-eva-use-spec ptr_align_padding`**, when your program includes
  `core/primitives/ptr.h`, as `arena.h` does. Eva cannot compute an address
  modulo an alignment, since it does not know where an object lies in
  memory; the function's contract, which WP proves, bounds the result.
- **`-DCANON_NO_REQUIRE -DNDEBUG`** are the flags of the proofs, so that Eva
  reads the code WP proves. Without them, Canon-C's run-time checks
  (`require`) stay in the code, and Eva analyses them too.
- **`-warn-special-float none`**, if your program compares NaNs, as
  `tools/eva/programs/compare.c` does; Eva warns on each otherwise.

`tools/eva/eva.py` builds these commands for Canon-C's programs: `eva.py
run MODEL PROGRAM` runs one, and prints the command it runs.

## Reading what Eva says

- **An alarm** is a place where a runtime error may happen. **A
  precondition Eva finds invalid** is broken on every path Eva considered to
  reach the call: a bug in your program, in Canon-C's contract, or in Eva's
  reading of the contract. VERIFY-066 found 27 contracts of the last kind,
  which asked `\valid` of a pointer to `const`, now fixed
  (`docs/deviations.md`).
- **Unknown** means Eva could not decide. It over-approximates: a property
  it cannot decide may hold, and usually does. Canon-C's own programs leave
  open, at x86_64, 3 alarms, 77 calls whose precondition Eva does not
  decide and 40 checks it does not confirm, each argued in
  `tools/eva/README.md`, "Pins". The usual causes:
  - **a container's invariant**, which relates fields or quantifies over
    elements (`pool_invariant`, `hashmap_inv_`, `stringbuf_wf`): Eva keeps
    a range for each value, and does not relate them. Canon-C's functions
    ensure these invariants, which WP proves, so your program keeps them as
    long as it changes a container through its functions only;
  - **an address**: an arena pads each allocation from the address it
    starts at, which Eva does not know, so its offsets, and the pointers
    made from them, are ranges;
  - **the C library**: Eva reads `memcmp`, `vsnprintf` and `snprintf` by
    Frama-C's specifications, which do not give their results;
  - **`volatile`**: the secure zeroing functions write through it, so that
    the compiler keeps the writes, and Eva does not keep a value written so.
- **Calls inside Canon-C**, which Eva reports as the library's: WP proves
  their preconditions when it proves the calling function, so they hold
  when your call's precondition does.

## Known limits of Frama-C

- **Frama-C 29 stops with an internal error**,
  `Eva__Eval_terms.Reduce_to_bottom`, on a `\let` bound to an address
  converted to an integer, in a behaviour's `assumes`. Canon-C's contracts
  avoid it since VERIFY-067; avoid it in your own.
  `tools/probes/verify-066/r5_nobody.c` shows it in 47 lines.
- **At Frama-C's own `x86_64` and `x86_32` models**, Eva does not decide
  `\object_pointer` of a pointer read from a structure returned by value;
  the other models decide it. It leaves four calls of
  `tools/eva/programs/borrow.c` undecided at x86_64.
- **Frama-C 25** does not evaluate `\let` and cannot read the models of
  `tools/machdeps/`; at `ppc_32`, it has no `malloc` builtin, so memory from
  `malloc` reads as invalid there.

## What a clean result means

A program with no alarm and every call valid has no runtime error, and
meets the preconditions it calls with, at that machine model and under
Eva's assumptions: its model of the machine, and Frama-C's specifications
of the C library. That holds for that program, not for every program that
uses Canon-C. Eva is designed to be sound but is not itself proved
(`docs/verification.md`).
