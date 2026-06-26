# Canon-C Cheatsheet

> Quick reference for translating raw C constructs into their safe,
> verification-ready Canon-C forms.

---

## Table of C constructs

### Declarations
- [`Variable definition`](#c-var)
- [`Pointer definition`](#c-ptr)
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
a `for` loop, a `malloc`, a pointer dereference, a `switch`. Each entry gives
you, side by side, the raw C form you likely wrote and the Canon-C form that
replaces it, plus a one-line note on what the safe form buys you and what it
still leaves to you.

**Two kinds of replacement.** Some constructs translate to a *code shape* with
no function behind it — an initialized-at-declaration variable, a `require_msg`
precondition instead of a hand-rolled NULL branch, a single `goto cleanup;`
label. Those are written out in full, right here, ready to copy. Other constructs
translate to a *real Canon-C function* — overflow-safe arithmetic, bounds-checked
access, allocation — in which case the entry names the header that owns it
(`checked.h`, `slice.h`, `arena.h`, …) so you can open that header for the exact
signature, full example, and verification status.

**Some hazards live inside another construct's entry.** Not every C hazard gets
its own row. A `void*` cast-and-dereference, for instance, is covered under
[`Pointer definition`](#c-ptr) (NULL and alignment) and [`Cast`](#c-cast)
(casting back to a concrete type), because that is where a porter meets it. When
a row would only repeat another, it points there instead of duplicating it.

**Each entry lists the minimal include set.** An entry names only the
header(s) you actually need to `#include` for that construct — relying on
Canon-C's own transitive includes rather than listing the full chain. If
`ptr.h` already pulls in `types.h`, the entry shows `ptr.h` alone.

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

> Headers: `limits.h` (range constants), `contract.h` (`require_msg`).
> `limits.h` pulls in `types.h` (the aliases), so:
> ```c
> #include "core/primitives/limits.h"
> #include "core/primitives/contract.h"
> ```

A raw C variable definition has three independent failure modes. Find the one
your code matches.

<details>
<summary><b>1 — Read before assignment</b></summary>

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

</details>

<details>
<summary><b>2 — Platform-width type where the width matters</b></summary>

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

</details>

<details>
<summary><b>3 — Value that may not fit the type</b></summary>

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
> silently wrapping. Use `require_msg`, **not** `ensure_msg` — this guards an
> input and must survive release builds; `ensure_msg` is compiled out under
> `NDEBUG` unless `CANON_STRICT` is set. `require_msg` is itself compiled out
> only under `-DCANON_NO_REQUIRE`, once the range is formally proved, so the
> check is free in a verified build. Range constants per type are in `limits.h`
> (`CANON_I8_MIN/MAX` … `CANON_I64_MIN/MAX`, `CANON_U8_MAX` … `CANON_U64_MAX`,
> `CANON_USIZE_MAX`, `CANON_ISIZE_MIN/MAX`).
>
> If `level` only ever holds small constants, the real fix is simpler: pick a
> type wide enough that the value always fits, and case 3 disappears.

</details>

[↑ Back to constructs](#table-of-c-constructs)

---

<a name="c-ptr"></a>
## `Pointer definition`

[↑ Back to constructs](#table-of-c-constructs)

> Header: `ptr.h` (`ptr_is_aligned`, `ptr_is_valid`, `ptr_or`, `ALIGN_OF`).
> `ptr.h` pulls in `types.h` and `contract.h` (for `require_msg`), so:
> ```c
> #include "core/primitives/ptr.h"
> ```

A raw pointer definition has three failure modes. The first two are about the
pointer *value*; the third is about whether you may dereference it as the type
you cast to.

<details>
<summary><b>1 — Uninitialized pointer</b></summary>

```c
/* RAW — indeterminate; a stray dereference reads or writes a garbage address */
Config* cfg;
/* ... */
use(cfg);
```
```c
/* SAFE — initialize to NULL at definition; a NULL deref faults loudly
   instead of corrupting a random address silently */
Config* cfg = NULL;
```
> Buys you: a defined initial value. An uninitialized pointer dereference is
> undefined behavior with no diagnostic; a NULL one is a deterministic fault.
> This is the value half of the problem — see case 2 for the deref guard.

</details>

<details>
<summary><b>2 — Dereference without a NULL guard</b></summary>

```c
/* RAW — undefined behavior if p is NULL */
void f(Config* p) {
    use(p->field);
}
```
```c
/* SAFE — guard as a precondition before any dereference */
void f(Config* p) {
    require_msg(ptr_is_valid(p), "f: p is NULL");
    use(p->field);
}
```
> Buys you: the NULL case is caught at the boundary. `ptr_is_valid(p)` (ptr.h)
> is just an explicit `p != NULL` — the header recommends it inside contract
> macros so the intent reads unambiguously; plain `p != NULL` is equally fine.
>
> Use `require_msg`, **not** `ensure_msg` — this guards an input and must
> survive release; `ensure_msg` is compiled out under `NDEBUG` unless
> `CANON_STRICT`. `require_msg` is compiled out only under `-DCANON_NO_REQUIRE`,
> once the precondition is formally proved, so it is free in a verified build.
>
> **Which guard you need depends on NULL's meaning** (ptr.h's discipline):
> if NULL is a *bug* (a required pointer is missing), guard with `require_msg`.
> If NULL is a *valid input* with a sensible default, don't guard — fold it
> with `ptr_or(p, fallback)` instead, which returns `fallback` when `p` is NULL.

</details>

<details>
<summary><b>3 — Casting to a type the address may not be aligned for</b></summary>

```c
/* RAW — a NULL guard does NOT cover this. Dereferencing an int* whose
   address isn't int-aligned is undefined behavior on its own. */
void f(void* p) {
    require_msg(ptr_is_valid(p), "f: p is NULL");
    int* ip = (int*)p;
    use(*ip);                /* UB if p is not int-aligned */
}
```
```c
/* SAFE — guard alignment as well as null before the typed dereference */
void f(void* p) {
    require_msg(ptr_is_valid(p), "f: p is NULL");
    require_msg(ptr_is_aligned(p, ALIGN_OF(int)),
               "f: p is not int-aligned");
    int* ip = (int*)p;
    use(*ip);                /* now safe to dereference as int */
}
```
> Buys you: the second, easily-missed UB of a typed-pointer definition. A
> `void*` (or any under-aligned pointer) cast to `int*` and dereferenced is
> undefined behavior independent of whether it's NULL — the NULL guard alone
> leaves it open. `ptr_is_aligned(p, align)` (ptr.h, null-tolerant: returns
> `false` on NULL) tests it; `ALIGN_OF(type)` (ptr.h) gives the required
> alignment at compile time.
>
> This only applies when you *cast and dereference* as a wider type — a
> `char*`/`u8*` view needs only the NULL guard, since byte access has no
> alignment requirement.
>
> Note: `ALIGN_OF` uses C11 `_Alignof` where available and an `offsetof`
> fallback on C99. Under strict C99 (`CANON_NO_GNU_EXTENSIONS`) that fallback
> emits a compiler warning (`-Wgnu-offsetof-extensions` on Clang, C4116 on
> MSVC) — harmless, but expected. Compile as C11 to silence it.

</details>

[↑ Back to constructs](#table-of-c-constructs)

---
