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
