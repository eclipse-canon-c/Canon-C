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

Report-only until VERIFY-066's third run (`ENFORCE` in `eva.py`).

    python3 tools/eva/eva.py list
    python3 tools/eva/eva.py native [PROGRAM...]
    python3 tools/eva/eva.py run MODEL [PROGRAM...]
    python3 tools/eva/eva.py report [DIR]
    python3 tools/eva/eva.py check

Frama-C 25 needs `EVA_FRAMAC_ARGS=-c11`, which 29, the CI's, does not.
`8 · Records` runs `eva.py check`: every unit has a program and every program
includes its unit's file; the models are the proofs' and the workflow's.
