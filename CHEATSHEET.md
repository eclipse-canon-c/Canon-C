# Canon-C Cheatsheet

> Quick reference for translating raw C constructs into their safe,
> verification-ready Canon-C forms.

---

## Table of C constructs

### Declarations
- [`Variable definition`](#c-var)
- [`Pointer definition`](#c-ptr)
- [`void*` pointer](#c-voidptr)
- [`Array`](#c-array)
- [`String literal` / `char[]`](#c-strlit)
- [`Integer types & widths`](#c-widths)
- [`Floating-point types`](#c-float)
- [`typedef`](#c-typedef)
- [`const` / `volatile` qualifiers](#c-qualifiers)
- [`Storage classes (static, extern, register, auto)`](#c-storage)
- [`Designated & compound initializers`](#c-init)

### Aggregates
- [`struct`](#c-struct)
- [`union`](#c-union)
- [`enum`](#c-enum)
- [`Bitfields`](#c-bitfield)
- [`Flexible array member`](#c-flexarray)

### Operators
- [`Arithmetic (+ - * / %)`](#c-arith)
- [`Increment / decrement (++ --)`](#c-incdec)
- [`Assignment & compound assignment (= += ...)`](#c-assign)
- [`Bitwise (<< >> & | ^ ~)`](#c-bitwise)
- [`Comparison (< > <= >= == !=)`](#c-compare)
- [`Logical (&& || !)`](#c-logical)
- [`Ternary (?:)`](#c-ternary)
- [`sizeof` / `_Alignof`](#c-sizeof)
- [`Cast`](#c-cast)
- [`Address-of & dereference (& *)`](#c-addrderef)
- [`Member access (. ->)`](#c-member)
- [`Subscript ([])`](#c-subscript)

### Control flow
- [`if` / `else`](#c-if)
- [`switch` / `case` / `default`](#c-switch)
- [`for`](#c-for)
- [`while`](#c-while)
- [`do` / `while`](#c-do)
- [`break` / `continue`](#c-breakcont)
- [`goto` / labels](#c-goto)
- [`return`](#c-return)

### Functions
- [`Function definition`](#c-fn-def)
- [`Function parameters`](#c-fn-params)
- [`Return value`](#c-fn-return)
- [`Function pointers`](#c-fn-ptr)
- [`Variadic functions`](#c-variadic)

### Memory
- [`malloc` / `calloc` / `realloc` / `free`](#c-heap)
- [`memcpy` / `memmove` / `memset` / `memcmp`](#c-mem)
- [`String functions (strcpy, strlen, ...)`](#c-str)

### Preprocessor
- [`#define` macros](#c-define)
- [`#include`](#c-include)
- [`Conditional compilation (#if / #ifdef)`](#c-ifdef)

### Diagnostics
- [`assert`](#c-assert)
- [`Compile-time assertion`](#c-staticassert)

---

## How to use this cheatsheet

This document is organized around the question a porter actually asks: *"I have
this piece of raw C — what is its safe Canon-C form?"* You read it
construct-first.

**Start from the Table of C constructs.** Find the construct your code uses —
a `for` loop, a `malloc`, a `void*` cast, a `switch`. Each entry gives you, side
by side, the raw C form you likely wrote and the Canon-C form that replaces it,
plus a one-line note on what the safe form buys you and what it still leaves to
you.

**Two kinds of replacement.** Some constructs translate to a *code shape* with
no function behind it — an initialized-at-declaration variable, a `require_msg`
precondition instead of a hand-rolled NULL branch, a single `goto cleanup;`
label. Those are written out in full, right here, ready to copy. Other constructs
translate to a *real Canon-C function* — overflow-safe arithmetic, bounds-checked
access, allocation — in which case the entry names the header that owns it
(`checked.h`, `slice.h`, `arena.h`, …) so you can open that header for the exact
signature, full example, and verification status.

**A word on what "safe" means here.** Copying a form from this cheatsheet makes
your code *verification-ready* — shaped so a verifier can reason about it and so
common undefined behavior is guarded — not automatically *verified*. Only the
Canon-C substrate headers themselves are formally proved, in CI; your code is
proved when you run the verifier over it. The forms here are written to be the
shape that does not fight the prover and does not strand a coverage outcome, so
that when you do verify, these constructs are not what blocks you.

**Not everything has a Canon-C form.** A few constructs in the table —
variadics, `volatile`, most of the preprocessor — exist in C but have no safe
Canon-C replacement by design; their entries say so plainly rather than inventing
one. When that happens, the honest move is the documented workaround, not a
forced wrapper.

**The headers are the authority.** This cheatsheet orients you and gets you
moving; for the exact signature, the full ACSL contract, and the precise
verification status of any function it names, open that header and read the
source. Where the two ever disagree, the header is correct and this document is
stale.

---


<a name="c-var"></a>
## `Variable definition`

[↑ Back to constructs](#table-of-c-constructs)

> Headers: `types.h` (aliases), `limits.h` (range constants), `contract.h`
> (`require_msg`). `limits.h` pulls in `types.h`, so:
> ```c
> #include "core/primitives/limits.h"
> #include "core/primitives/contract.h"
> ```

A raw C variable definition has three independent failure modes. Find the one
your code matches.

---

**1 — Read before assignment.**

```c
/* RAW — indeterminate value if a path reaches the use without assigning */
usize count;
/* ... */
return count;
```
```c
/* SAFE — define and initialize in one step */
usize count = 0;
return count;
```
> Buys you: no indeterminate-value read. A verifier rejects a read of a
> possibly-uninitialized variable outright, so this is what lets the proof
> proceed past the definition. Costs nothing at runtime.

---

**2 — Platform-width type where the width matters.**

```c
/* RAW — width and signedness left to the platform; intent unclear */
unsigned long flags = 0;
int           index = 0;
long          offset = 0;
```
```c
/* SAFE — explicit-width aliases (types.h); width is part of the name */
u32   flags  = 0;   /* exactly 32 bits, unsigned                       */
usize index  = 0;   /* unsigned, pointer-width — for sizes and indices */
isize offset = 0;   /* signed, pointer-width — for differences/offsets */
```
> Buys you: overflow and truncation reasoning is local to the use site,
> because the width is visible in the type name. Full set in `types.h`:
> `u8 u16 u32 u64`, `i8 i16 i32 i64`, `usize`, `isize`, `f32`, `f64`.

---

**3 — Value that may not fit the type.**

```c
/* RAW — a value past the type's max is silently truncated on assignment.
   i8 holds -128..127; 129 does not fit. */
i8 level = (i8)v;      /* if v == 129, level becomes -127 — no warning */
```
```c
/* SAFE — range-check against the type's named limit before narrowing */
require_msg(v >= CANON_I8_MIN && v <= CANON_I8_MAX, "level out of i8 range");
i8 level = (i8)v;      /* provably in range past this point */
```
> Buys you: an out-of-range narrowing is caught at the boundary instead of
> silently wrapping. `require_msg` (contract.h) is **compiled out** under
> `-DCANON_NO_REQUIRE` once the range is formally proved, so the check is
> free in a verified build. Range constants per type are in `limits.h`
> (`CANON_I8_MIN/MAX` … `CANON_I64_MIN/MAX`, `CANON_U8_MAX` … `CANON_U64_MAX`,
> `CANON_USIZE_MAX`, `CANON_ISIZE_MIN/MAX`).
>
> If `level` only ever holds small constants, the real fix is simpler: pick a
> type wide enough that the value always fits, and case 3 disappears.

[↑ Back to constructs](#table-of-c-constructs)

---
