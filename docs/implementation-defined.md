# Implementation-defined behaviour

What Canon-C needs of the C implementation it is compiled with, beyond what
C guarantees. C leaves some behaviours to each implementation, which must
document its choice: how a pointer converts to an integer, what happens to a
value converted to a signed type too narrow for it, where `#include "..."`
looks. MISRA C:2012's Directive 1.1 asks that any such behaviour a program's
output depends on be documented and understood. For a library, that means
saying which ones it relies on, so that a project using it can check them
against its own compiler and C library (VERIFY-071).

## How the list was made

Every implementation-defined behaviour listed in C99's Annex J.3 was read
against the library's headers, `core/`, `semantics/`, `data/`, `algo/` and
`util/`, on 2026-11-12. Tests and tools are not part of the library. Each
behaviour turned out to be one of three kinds:

- **relied on**: Canon-C's results depend on it, and the section below says
  what Canon-C assumes, where, and what checks it;
- **not relied on**: the code does not use it, or does not depend on its
  outcome, with the evidence;
- **passed on**: Canon-C hands the implementation's result to the program
  unchanged, so the program depends on it, not the library.

`test/integration/impl_defined_test.c` checks each assumption a running
program can check. It runs wherever CI runs the tests: Linux on x86-64,
AArch64, i386 and big-endian s390x, with glibc and with musl; macOS; Windows
with MSVC, clang-cl and MinGW; with GCC, Clang and CompCert. The bare-metal
targets (Cortex-M0+, Cortex-M4, RV32, RV64) compile it and do not run it.

Clauses are C99's. "GCC says" quotes GCC's manual, "C Implementation-Defined
Behavior"; "MSVC says" quotes Microsoft's C language and C runtime
reference. For Clang, this page relies on the tests: on the targets CI
builds, Clang does what they check.

## Relied on

### 1. 8-bit bytes and the exact-width integer types

**C**: the number of bits in a byte (J.3.4); `uint8_t` to `int64_t` are
optional (7.18.1.1). **Canon-C** uses `u8` to `i64` throughout. **Checked**:
`core/primitives/types.h` refuses to compile without the eight types
(`#error`, the platform's Tier 0). `uint8_t` existing means 8-bit bytes.

### 2. `size_t` of at least 32 bits

**C**: `SIZE_MAX` is at least 65535 (7.18.3). **Canon-C**'s size constants
and capacity limits need 32 bits. **Checked**: `core/primitives/limits.h`
refuses a smaller `size_t` (`#error`, Tier 1), and `2 · Platforms` checks
the refusal with AVR's 16-bit `size_t` (`tools/avr_tier_check.sh`). Tier 0's
headers stay usable there.

### 3. Pointers as flat byte addresses

**C**: a pointer converted to an integer, and an integer converted to a
pointer, give implementation-defined results (6.3.2.3, paragraphs 5 and 6;
J.3.7).

**Canon-C** converts pointers to `uintptr_t`, and on to `usize`, to:

- test alignment and compute padding: `ptr_is_aligned`, `ptr_align_padding`
  (which `arena_alloc` uses), `mem_is_aligned`, `mem_get_alignment`, and
  hashmap's init, which tests its buffer's alignment;
- compare addresses across objects, defined where comparing the pointers
  would not be: `ptr_in_range`, `ptr_range_in_range`, `mem_regions_overlap`,
  and dynvec's extend, which tests whether its source lies in its own
  buffer;
- check address arithmetic for overflow: `ptr_offset`, `ptr_offset_const`
  and `ptr_retreat`;
- name a region by its address: `region_begin`, and under
  `CANON_LIFETIME_DEBUG`, the lifetime token's mixing in
  `core/primitives/lifetime.h`.

It converts integers back to pointers in `ptr_align_up` and
`ptr_align_down`, which round an address and return the pointer to it.

It assumes that the integer is the byte's address in one flat address
space: the bytes of an object have consecutive addresses; an address inside
an object converts back to the pointer to that byte; two objects' address
ranges do not overlap; and an object of type `T` sits at a multiple of
`T`'s alignment. Naming a region needs less: distinct pointers give distinct
integers, which C guarantees wherever `uintptr_t` exists (7.18.1.4).

**GCC says**: "A cast from pointer to integer discards most-significant bits
if the pointer representation is larger than the integer type,
sign-extends if the pointer representation is smaller than the integer
type, otherwise the bits are unchanged", and the same of the reverse cast;
and "When casting from pointer to integer and back again, the resulting
pointer must reference the same object as the original pointer, otherwise
the behavior is undefined." So the pointer `ptr_align_up` or
`ptr_align_down` returns is defined when it stays in its argument's object,
as an arena's allocation does. A caller that rounds out of its object, to
the start of a page below its buffer, say, holds a pointer GCC does not
define.

**MSVC says**: if the integral type is the same size as the pointer,
"the conversion simply causes the integral value to be treated as a
pointer (an unsigned integer)", and a pointer converted to an integral type
as wide "behaves like an unsigned value in the conversion".

**Not supported**: segmented and capability-based targets (CHERI, for
one), where a pointer is not an address.

**Checked**: `impl_defined_test`'s `flat_addresses`, `address_to_pointer`,
`alignment_divides` and `objects_disjoint`. **Not proved**: the contracts
state these functions' results in the converted addresses, and take the
conversion as this section describes it. WP cannot prove that the round
trip through `uintptr_t` keeps an address's bounds, and leaves those goals
unproved (VERIFY-006's third category, `docs/verification.md`).

### 4. `uintptr_t` exists, as wide as `size_t`

**C**: `uintptr_t` is optional (7.18.1.4). **Canon-C** uses it without a
fallback, so a target without it fails to compile; and it converts addresses
to `usize`, which needs the two to have one width. Measured: 8 and 8 bytes
on x86-64 and RV64, 4 and 4 on Cortex-M and RV32. **Checked**:
`uintptr_is_usize`, on every platform the tests run on.

### 5. A value converted to a signed type too narrow for it

**C**: "either the result is implementation-defined or an
implementation-defined signal is raised" (6.3.1.3p3, J.3.5).
**Canon-C**: `vec_T_extend_from_range` converts each value of the range to
`T` and back, and refuses one that changes. That needs no particular result,
only that no signal is raised. **GCC says**: "the value is reduced modulo
2^N to be within range of the type; no signal is raised." **MSVC says**:
"When converted to an integer of smaller size, the high-order bits are
truncated", and an unsigned value converted to the signed type of its size
keeps its bit pattern. **Checked**: `signed_conversion`.

### 6. `long long` is 64 bits

**C**: `long long` has at least 64 bits (5.2.4.2.1). **Canon-C**:
`parse_i64` reads with `strtoll` and `parse_u64` with `strtoull`, and return
`i64` and `u64`. A wider `long long` would let a value outside those types
through without `ERANGE`. **Checked**: `strtoll_is_i64`, which reads the
largest `i64` and `u64` and the values one past their limits.

### 7. IEEE 754 floating point

**C**: the floating types' characteristics (5.2.4.2.2, J.3.6); IEC 60559
support is optional (Annex F). **Canon-C**'s floating-point code is small:
`algo_cmp_f32` and `algo_cmp_f64` order NaN after every number and detect
it as `x != x`; `random_f64` divides a 32-bit value by 2^32, and the float
ranges scale it; hashmap and intern compute a load factor, and the
stopwatch its seconds. It needs binary32 and binary64, whose NaN is
unequal to itself. **Checked**: `core/primitives/types.h` refuses a `float`
not 4 bytes or a `double` not 8 (AVR's GCC makes `double` 4 bytes unless
given `-mdouble=64`); `ieee_floats` checks the radix, precision and
exponent range, and the NaN. **Not supported**: `-ffast-math` and
`-ffinite-math-only`, with which GCC and Clang assume there are no NaNs and
may fold `x != x` to false.

### 8. `#include "..."` looks beside the including file first

**C**: where a quoted `#include` looks is implementation-defined (6.10.2p3,
J.3.11). **Canon-C**'s headers include their siblings by name:
`core/primitives/`'s headers include `"types.h"`, `checked.h` includes
`"limits.h"`, and the generic modules include their own fragments
(`_mangle.h`, `_impl.h`, `_decl.h`, `_defn.h`). Everything else is included
from the root, as in `"core/arena.h"`, so the root must be on the include
path (README, "Usage").
**GCC says** that the quote form looks "first relative to the directory of
the current file", then in the `-I` directories. **MSVC says** it looks "In
the same directory as the file that contains the #include statement", then
in those of the files that included it, then along `/I`. **Checked**: every
build.

One consequence is worth knowing. The project's own builds also put
`core/primitives/` on the include path, and there `<limits.h>` names
Canon-C's `limits.h`, not the C library's: Canon-C's headers use no macro
of the system's `<limits.h>` (`core/primitives/limits.h`, its header
comment). A program that adds only the root, as the README says, is not
affected.

### 9. `volatile` stores are kept

**C**: what constitutes an access to a `volatile` object is
implementation-defined (6.7.3p6, J.3.10). **Canon-C**: where C11's
`memset_s` is not available, `mem_secure_zero` writes zeros through a
`volatile u8 *`, so that the compiler keeps the stores even when nothing
reads the buffer again. Under `CANON_LIFETIME_DEBUG` with MSVC, the
lifetime token's counter is a `volatile long long` incremented by
`_InterlockedIncrement64`. **GCC**'s manual takes a store through a
`volatile` pointer, as in `*dst = *src;`, as a write, and settles the less
clear case, a read whose value is unused. **Not checked**:
a test that reads the buffer back makes the stores necessary, so it cannot
tell whether they would have been kept.

### 10. Pragmas

**C**: a pragma other than `STDC` does what the implementation says, and
one it does not recognise is ignored (6.10.6, J.3.11). **Canon-C**:
`core/primitives/contract.h` prints a `#pragma message` under GCC and Clang
when `CANON_NO_REQUIRE` and `CANON_STRICT` are both set, and
`semantics/diag.h` turns off GCC's `-Wformat-truncation` around its two
`snprintf` calls with `#pragma GCC diagnostic`. Both change what the
compiler reports, never the code it generates.

## Not relied on

| behaviour (C99 Annex J.3) | why Canon-C does not depend on it |
|---------------------------|-----------------------------------|
| right shift of a negative value, and bitwise operators on signed values (J.3.5; 6.5.7p5) | Every shift in the library acts on an unsigned operand (read for this page), and Cppcheck's MISRA rule 10.1 check, which flags a bitwise operator on a signed operand, reports none (`tools/misra/pins.txt`). Cppcheck shows one finding per line, and none of the 57 lines with another finding has a bitwise operator. |
| how signed integers are represented (J.3.5) | The exact-width types are two's complement by definition (7.18.1.1), and nothing depends on how `int`, `long` or `ptrdiff_t` are represented. |
| whether plain `char` is signed (J.3.4) | Characters become integers only through `unsigned char`: intern's hash and `str_compare`. Measured: signed on x86-64, unsigned on Arm and RISC-V. |
| the execution character set's values (J.3.4) | Characters are compared for equality only, with `'\0'`, the whitespace characters and `'-'`, all of the basic character set. `str_compare` orders by byte value, as `strcmp` does. |
| the integer type of an enumeration (J.3.9; 6.7.2.2p4) | `Error` is `unsigned int`, 4 bytes, on x86-64 and RISC-V, and `unsigned short`, 2 bytes, on arm-none-eabi, whose ABI makes enumerations short ("On some targets, -fshort-enums is the default", GCC says). `error_in_range` compares through `int`, and no code reads an enumeration's bytes. |
| bit-fields (J.3.9) | There are none. |
| padding and the alignment of members (J.3.9) | hashmap's alignment test uses the offset of a member after one `char`, a multiple of the alignment whatever the padding. `mem_equal_type` and the arrays' `_equal` compare objects byte for byte: on a type with padding, whose bytes C leaves unspecified (6.2.6.1p6), they can find equal values unequal, as `data/array.h` says. |
| significant characters in identifiers (J.3.3; 5.2.4.1) | C guarantees 63 significant characters in an internal name and 31 in an external one. No function in the tests' objects has a name longer than 54. The separate-compilation headers make names external: the tests' 170 differ within their first 31 characters. A program's own type names lengthen them; GCC says that for internal names "all characters are significant". |
| `malloc(0)` and `realloc(p, 0)` (J.3.12; 7.20.3) | Every call asks for at least one byte, and `mem_alloc(0)` returns `NULL` without calling `malloc`. |
| whether `abort` flushes open streams (J.3.12; 7.20.4.1) | The default contract handler flushes `stderr` before it calls `abort`. |
| byte order (J.3.13; 6.2.6.1) | No code reads an object's bytes as another type; bits.h's byte swaps are arithmetic. `2 · Platforms` runs every test on big-endian s390x. |
| `inline` and `register` (J.3.8) | `inline` is a hint and is taken as one; `register` is not used. |

## Passed on

These results come from the implementation, and Canon-C returns or prints
them as they are.

- **`strtod`'s rounding and underflow** (J.3.6, J.3.12; 7.20.1.3):
  `parse_f64` returns `strtod`'s value, and leaves `errno` as `strtod` sets
  it, which on underflow may or may not be `ERANGE`.
- **`printf`'s `%f`**: hashmap's formatting prints the load factor to three
  decimals with the C library's `vsnprintf`, whose rounding C recommends and
  does not require. newlib-nano formats no floating point unless the
  program links in `_printf_float`.
- **An object's bytes** (J.3.9, J.3.13): the `_as_bytes` and `_as_cbytes`
  views of slices, arrays, vectors, bitsets and priority queues show their
  elements in the platform's byte order, with its padding.
- **`__FILE__`** in contract and diagnostic messages: as the compiler was
  given the file name.
- **`rename` onto a file that exists** (J.3.12; 7.19.4.2):
  `file_write_all_atomic` writes a temporary file and renames it over the
  target. POSIX replaces the target, atomically. MSVC says "The new name
  must not be the name of an existing file or directory", so on Windows
  `file_write_all_atomic` moves the file with `MoveFileExA` and
  `MOVEFILE_REPLACE_EXISTING` instead, which replaces it (VERIFY-074);
  Microsoft does not document that move as atomic. **Checked**:
  `rename_existing`, which expects `rename` to replace the file on POSIX
  systems and to refuse on Windows, and `file_write_all_atomic` to replace
  it on each. See "Found by the survey".
- **Binary streams** (7.19.2p3, 7.19.9.2p3): a binary stream need not
  support `fseek` to `SEEK_END`, and `file_read_all_arena` reads in chunks
  when it fails; it may carry null characters appended at its end, which
  POSIX and Windows do not add.
- **`util/time.h`'s clocks**: `QueryPerformanceCounter` on Windows,
  `clock_gettime(CLOCK_MONOTONIC)` elsewhere, with the system's resolution
  and origin.
- **How floating expressions are evaluated** (`FLT_EVAL_METHOD`, J.3.6):
  where GCC computes in the x87 unit's extended precision, as on i386
  without SSE, a load factor can differ in its last bit from x86-64's.

## Found by the survey

1. **`random_i32_range` was wrong for spans above 2^31.** It converted
   the draw, a `u32` up to the span, to `i32`, which for draws above
   `INT32_MAX` is the out-of-range conversion of section 5, and added it to
   `min`, which then overflowed `int`: undefined behaviour. UBSan reported
   it at `util/random.h:232` for `random_i32_range(r, INT32_MIN + 1,
   INT32_MAX)`. The full range, `INT32_MIN` to `INT32_MAX`, made the span
   2^32, which `u32` reduced to 0: every call returned `INT32_MIN`.
   **Fixed** by VERIFY-072: the draw is added in `i64`, and the full range
   takes every `u32`.
2. **`random_f32_range` and `random_f64_range` could return `max`**,
   which they document as excluded. `random_f64`'s largest value,
   1 − 2^−32, rounds to 1.0f as an `f32` (6.3.1.5 leaves the direction to
   the implementation; IEEE 754's default rounds to nearest): for 128 of
   the 2^32 values of `random_u32`, `random_f32_range(r, 0.0f, 1.0f)`
   returned 1.0f. And in `f64`, `min + x * (max - min)` rounds to `max`
   where `max - min` is small beside `min`: from 2^60 to 2^60 + 1024, an
   eighth of the draws. **Fixed** by VERIFY-072: such a draw is drawn
   again. `!(min < max)` and an infinite bound now return `min`, and a
   span that overflows is added in two halves, so the value stays finite.
3. **`file_write_all_atomic` could not replace a file on Windows**, as
   above, and `util/file.h` did not say so. Decided by the author
   (2026-11-12): on Windows it replaces the file with `MoveFileExA` and
   `MOVEFILE_REPLACE_EXISTING`. **Fixed** by VERIFY-074, and `util/file.h`
   says how it moves the file on each system, and what it does not promise.
4. **`Error`'s size with short enumerations was misstated.**
   `semantics/error.h`'s comment in `error_in_range`, and VERIFY-046's
   record, say that with `-fshort-enums` `Error` is `unsigned char`. Its
   values reach 402, so it is `unsigned short`, 2 bytes (measured with
   arm-none-eabi-gcc 13.2.1). VERIFY-046's record now carries the
   correction. **Corrected** in the comment by VERIFY-073, the next change
   to `semantics/`.
5. **`semantics/option/option.h`'s `parse_int` example** converts
   `strtol`'s `long` to `int` without checking its range, the conversion of
   section 5; `result.h`'s version of the example checks it.
   **Corrected** by VERIFY-073, the next change to `semantics/`: the
   example returns none for a value outside `int`'s range.

## Keeping it current

A change that adds a conversion between pointers and integers, a
conversion to a signed type, floating-point code, or a use of a C library
function named here updates this page, and the test where it can check
the behaviour.

Sources: GCC's
[Arrays and pointers](https://gcc.gnu.org/onlinedocs/gcc/Arrays-and-pointers-implementation.html),
[Integers](https://gcc.gnu.org/onlinedocs/gcc/Integers-implementation.html),
[Structures, unions, enumerations, and bit-fields](https://gcc.gnu.org/onlinedocs/gcc/Structures-unions-enumerations-and-bit-fields-implementation.html),
[Qualifiers](https://gcc.gnu.org/onlinedocs/gcc/Qualifiers-implementation.html),
[Identifiers](https://gcc.gnu.org/onlinedocs/gcc/Identifiers-implementation.html)
and [the preprocessor's search path](https://gcc.gnu.org/onlinedocs/cpp/Search-Path.html);
Microsoft's
[Conversions to and from pointer types](https://learn.microsoft.com/en-us/cpp/c-language/conversions-to-and-from-pointer-types),
[Conversions from unsigned integral types](https://learn.microsoft.com/en-us/cpp/c-language/conversions-from-unsigned-integral-types),
[#include directive](https://learn.microsoft.com/en-us/cpp/preprocessor/hash-include-directive-c-cpp)
and [rename](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/rename-wrename).
