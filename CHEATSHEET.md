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

<a name="c-arith"></a>
## `Arithmetic (+ - * / %)`

[↑ Back to constructs](#table-of-c-constructs)

> Header: `checked.h` (`checked_add`, `checked_sub`, `checked_mul`,
> `checked_div`, `checked_mod`, signed/typed variants). Pulls in `types.h`
> and `limits.h`, so:
> ```c
> #include "core/primitives/checked.h"
> ```

Raw integer arithmetic can overflow or divide by zero. For **signed** operands
that is undefined behavior — the compiler may assume it never happens and
delete the checks around it. For **unsigned** it wraps silently (defined, but
almost always a bug). Each checked op does the operation only if it is safe,
writes the result through an out-parameter, and returns `true`/`false`.

<details>
<summary><b>1 — Add / subtract / multiply (overflow)</b></summary>

```c
/* RAW — signed overflow is UB; unsigned silently wraps. Either way the
   result is wrong and nothing tells you. */
usize total = count * elem_size;     /* wraps small if it overflows */
isize sum   = a + b;                 /* UB if a + b exceeds isize range */
```
```c
/* SAFE — do the op only if it fits; branch on the boolean result */
usize total;
if (!checked_mul(count, elem_size, &total)) {
    return ERROR_OVERFLOW;           /* handle as a value, not a crash */
}

isize sum;
if (!checked_add_isize(a, b, &sum)) {
    return ERROR_OVERFLOW;
}
```
> Buys you: overflow becomes a handleable `false` instead of UB or a silent
> wrap. `checked_add` / `checked_sub` / `checked_mul` operate on `usize`;
> typed variants exist for the unsigned widths (`checked_add_u8` / `_u16` /
> `_u32` / `_u64`, and likewise for sub/mul) and for **signed pointer-width**
> (`checked_add_isize` / `_sub_isize` / `_mul_isize`).
>
> There is **no fixed-width signed** variant (`_i8` … `_i64`). To overflow-check
> narrow signed arithmetic, do the checked op in `isize`, then narrow the
> result with a range guard (see [`Variable definition`](#c-var) case 3). If you
> find yourself needing this often, it usually means the value wants to live in
> `isize`/`usize` in the first place.
>
> **Critical:** on overflow the function still writes `*result` (with the
> wrapped value, when compiled with builtins) and returns `false`. Always
> check the boolean **before** reading the result — never use `*result`
> blind. The pattern is `if (!checked_op(...)) handle; /* then use result */`.

</details>

<details>
<summary><b>2 — Divide / modulo (divide-by-zero, signed ISIZE_MIN / -1)</b></summary>

```c
/* RAW — division by zero is UB. For signed, ISIZE_MIN / -1 is ALSO UB
   (the true quotient is unrepresentable), even though it looks harmless. */
usize per = total / count;           /* UB if count == 0 */
isize q   = num / den;               /* UB if den == 0, or num==MIN & den==-1 */
```
```c
/* SAFE — both UB cases are detected and reported */
usize per;
if (!checked_div(total, count, &per)) {
    return ERROR_DIV_BY_ZERO;
}

isize q;
if (!checked_div_isize(num, den, &q)) {
    return ERROR_DIV_INVALID;        /* covers zero AND ISIZE_MIN / -1 */
}
```
> Buys you: the divide-by-zero guard, and — for the signed variant — the
> `ISIZE_MIN / -1` overflow case that a hand-written `if (den == 0)` check
> misses entirely. `checked_mod` / `checked_mod_isize` do the same for `%`.
> The unsigned `checked_div` / `checked_mod` only fail on a zero divisor; that
> is simple enough you could inline `if (b == 0)`, but they exist so you keep
> one idiom across all five operations.

</details>

<details>
<summary><b>3 — min / max / clamp (double-evaluation trap)</b></summary>

```c
/* RAW / NAIVE — a hand-rolled max macro evaluates its args twice */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
usize n = MAX(i++, limit);           /* i++ happens twice — bug */
```
```c
/* SAFE-ER — checked.h provides the macros, but the same caveat applies:
   pass values with no side effects. */
usize n = checked_max(i, limit);     /* fine: i has no side effect */
usize c = checked_clamp(x, lo, hi);  /* clamp x into [lo, hi] */
```
> Buys you: a named, tested `checked_min` / `checked_max` / `checked_clamp`
> instead of a per-file hand-rolled macro. These are **macros**, so the
> double-evaluation rule still holds — never pass an argument with a side
> effect (`i++`, a function call). Bind it to a variable first.

</details>

[↑ Back to constructs](#table-of-c-constructs)

---

<a name="c-bitwise"></a>
## `Bitwise (<< >> & | ^ ~)`

[↑ Back to constructs](#table-of-c-constructs)

> Header: `bits.h` (`bits_set`, `bits_clear`, `bits_test`, `bits_extract`,
> `bits_rotl`, `bits_clz`, `bits_popcount`, …). Pulls in `types.h`, so:
> ```c
> #include "core/primitives/bits.h"
> ```

Raw bit manipulation has one dominant UB source: **shifting by an amount ≥ the
type's width**, including the easy-to-miss case where the shifted `1` is a
plain `int`. The `bits_*` functions operate on `u64` and avoid the UB by
construction.

<details>
<summary><b>1 — Set / clear / test / toggle a bit</b></summary>

```c
/* RAW — `1` is an int; `1 << bit` is UB once bit >= 31 (and certainly at
   bit >= width). People write this meaning to touch a high bit and hit UB. */
flags |=  (1 << bit);                /* UB for bit >= 31 */
flags &= ~(1 << bit);
bool on = (flags >> bit) & 1;
```
```c
/* SAFE — named ops on u64; the shift constant is 1ULL internally */
flags   = bits_set(flags, bit);      /* set bit */
flags   = bits_clear(flags, bit);    /* clear bit */
bool on = bits_test(flags, bit);     /* test bit */
flags   = bits_toggle(flags, bit);   /* flip bit */
```
> Buys you: the high-bit UB disappears — `bits_*` shift `1ULL`, not `1`, and
> work on `u64`. Precondition: `bit < 64`. Note these return `u64`; assign
> back into a `u64`/`usize` flags word.

</details>

<details>
<summary><b>2 — Shift count and hand-rolled rotation</b></summary>

```c
/* RAW — variable shift >= width is UB; and the classic rotate idiom is UB
   when shift == 0, because `x >> (64 - 0)` shifts by the full width. */
u64 r = (x << s) | (x >> (64 - s));  /* UB if s == 0, or s >= 64 */
```
```c
/* SAFE — rotation that masks the count and special-cases 0 */
u64 r = bits_rotl(x, s);             /* left-rotate; shift auto-masked to 0..63 */
u64 l = bits_rotr(x, s);             /* right-rotate */
```
> Buys you: no shift-width UB. `bits_rotl` / `bits_rotr` mask the shift to
> `0..63` and handle `shift == 0` correctly. They always rotate the full 64
> bits — for a narrow (e.g. 8-bit) rotation, use the worked formula in the
> `bits.h` docblock rather than masking the result, which can let high bits
> bleed in.

</details>

<details>
<summary><b>3 — Count leading/trailing zeros, popcount</b></summary>

```c
/* RAW — the compiler builtins are UB for a zero input on many platforms,
   and hand-rolled bit-counting is easy to get subtly wrong. */
u32 lz = __builtin_clzll(value);     /* UB when value == 0 */
```
```c
/* SAFE — defined for every input, including zero */
u32 lz = bits_clz(value);            /* leading zeros; returns 64 if value == 0 */
u32 tz = bits_ctz(value);            /* trailing zeros; returns 64 if value == 0 */
u32 n  = bits_popcount(value);       /* set-bit count, 0..64 */
```
> Buys you: a defined answer for `value == 0` (the raw builtins are UB there),
> and tested popcount / clz / ctz that compile to a single instruction where
> the hardware has it and fall back to portable C otherwise. Related:
> `bits_ffs` / `bits_fls` (1-indexed first/last set bit, 0 if none),
> `bits_is_power_of_two`, `bits_next_power_of_two`.

</details>

[↑ Back to constructs](#table-of-c-constructs)

---

<a name="c-compare"></a>
## `Comparison (< > <= >= == !=)`

[↑ Back to constructs](#table-of-c-constructs)

> Header: `compare.h` (`algo_cmp_i32`, `algo_cmp_f64`, … and the `algo_cmp_fn`
> / `algo_pred_fn` typedefs). Pulls in `types.h`, so:
> ```c
> #include "core/primitives/compare.h"
> ```

**Plain comparison of two values is fine as-is.** `if (a < b)` on integers
needs no Canon-C form — there is nothing unsafe about it. This entry is about
the case that *does* go wrong: writing a **comparator** for sorting, searching,
or an ordered container. Two classic comparator bugs have safe replacements
here.

<details>
<summary><b>1 — Don't subtract to compare</b></summary>

```c
/* RAW — the "clever" comparator. a - b overflows for extreme values,
   which is UB for signed and gives a wrong sign for unsigned. */
int cmp(const void* a, const void* b) {
    return *(const i32*)a - *(const i32*)b;   /* overflows; wrong order */
}
```
```c
/* SAFE — use the built-in comparator, or the same branchless pattern */
/* built-in: */
algo_cmp_fn cmp = algo_cmp_i32;        /* correct for the full i32 range */

/* or, if you must hand-write one, the safe shape is: */
int my_cmp(const void* a, const void* b, void* ctx) {
    (void)ctx;
    i32 x = *(const i32*)a, y = *(const i32*)b;
    return (x > y) - (x < y);          /* always -1, 0, or +1; never overflows */
}
```
> Buys you: a correct ordering across the whole value range. The `a - b` trick
> overflows (UB for signed) and inverts for values far apart; the
> `(x > y) - (x < y)` pattern the `algo_cmp_*` functions use is always exactly
> -1/0/+1. Built-ins exist for every primitive width: `algo_cmp_u8` …
> `algo_cmp_u64`, `algo_cmp_i8` … `algo_cmp_i64`, `algo_cmp_usize` /
> `algo_cmp_isize`, each with an `_desc` descending variant. Note Canon-C's
> comparator signature takes a third `void* ctx` argument (`algo_cmp_fn`).

</details>

<details>
<summary><b>2 — Floating-point comparison with NaN</b></summary>

```c
/* RAW — raw < on floats is not a total order: every comparison with NaN is
   false, so a NaN in the data breaks sorting (intransitive, undefined order). */
int cmp(const void* a, const void* b) {
    f64 x = *(const f64*)a, y = *(const f64*)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;   /* NaN compares equal to all → broken */
}
```
```c
/* SAFE — built-in float comparator imposes a total order (NaN sorts last) */
algo_cmp_fn cmp = algo_cmp_f64;        /* f32 variant: algo_cmp_f32 */
```
> Buys you: a usable total order in the presence of NaN. With raw `<`, a NaN
> element makes every comparison against it false, so a sort can't place it and
> the result is undefined. `algo_cmp_f32` / `algo_cmp_f64` define a total order
> by sorting all NaNs to one end (last in ascending, and two NaNs compare
> equal); the `_desc` variants reverse the numeric order and move NaN to the
> front. If your float data can never contain NaN this doesn't bite — but a
> built-in costs nothing and removes the assumption.

</details>

[↑ Back to constructs](#table-of-c-constructs)

---

<a name="c-assert"></a>
## `assert`

[↑ Back to constructs](#table-of-c-constructs)

> Header: `contract.h` (`require` / `require_msg`, `ensure` / `ensure_msg`,
> `unreachable` / `unreachable_msg`, `panic`). Pulls in `types.h`, so:
> ```c
> #include "core/primitives/contract.h"
> ```

Plain `assert` is one tool for several different jobs, and it vanishes entirely
under `NDEBUG`. Canon-C splits it by *intent*: a check on the caller's input, a
check on your own logic, an impossible path, or an unconditional abort. Picking
the right one is what makes the check survive (or correctly disappear) in the
right build.

<details>
<summary><b>1 — Precondition on input → <code>require</code></b></summary>

```c
/* RAW — assert vanishes under NDEBUG, so an input check disappears in
   exactly the release build where you still want it. */
void process(Config* cfg, usize n) {
    assert(cfg != NULL);
    assert(n > 0);
    /* ... */
}
```
```c
/* SAFE — require is always-on (off only with -DCANON_NO_REQUIRE) */
void process(Config* cfg, usize n) {
    require(cfg != NULL);
    require_msg(n > 0, "process: n must be > 0");
    /* ... */
}
```
> Buys you: the input check survives into release. `require` / `require_msg`
> are on by default and in `NDEBUG` builds; they compile out only under the
> explicit `-DCANON_NO_REQUIRE` (for when formal proof has covered every call
> site). Use these for things that are a *bug* if they happen — a NULL where
> NULL is forbidden, a violated input range. Use `require_msg` when a message
> aids debugging.

</details>

<details>
<summary><b>2 — Check on your own logic → <code>ensure</code></b></summary>

```c
/* RAW — assert, no signal of whether this is an input check or a self-check */
usize allocated = do_work(pool);
assert(allocated <= pool->capacity);
```
```c
/* SAFE — ensure: debug-only by default, promotable for certified builds */
usize allocated = do_work(pool);
ensure(allocated <= pool->capacity);   /* internal invariant */
```
> Buys you: an intent signal and the right build behavior. `ensure` /
> `ensure_msg` check things your *own* code should guarantee (postconditions,
> invariants). They are on in debug, off under `NDEBUG`, and — importantly —
> promoted back to always-on by `-DCANON_STRICT` (for DO-178C / ISO 26262
> builds where debug checks must survive). Rule of thumb: `require` guards what
> comes *in*; `ensure` checks what your code *produced*.

</details>

<details>
<summary><b>3 — Impossible path → <code>unreachable</code></b></summary>

```c
/* RAW — a default case that "can't happen" but returns garbage if it does */
switch (state) {
    case S_A: return a();
    case S_B: return b();
    default:  return 0;          /* silently wrong if a new state appears */
}
```
```c
/* SAFE — mark it unreachable: panics in debug, optimization hint in release */
switch (state) {
    case S_A: return a();
    case S_B: return b();
    default:  unreachable_msg("invalid state");
}
```
> Buys you: a real signal instead of a fake return value. `unreachable` /
> `unreachable_msg` (contract.h) fire the panic handler in debug builds, act as
> a compiler optimization hint in release, and always panic under
> `CANON_STRICT`. Use only for paths that genuinely cannot occur through your
> API — for a recoverable "unexpected input", return an error value instead.

</details>

<details>
<summary><b>4 — Unconditional fatal error → <code>panic</code></b></summary>

```c
/* RAW — abort() with no message, or assert(0) which NDEBUG removes */
if (config_unparseable) {
    abort();
}
```
```c
/* SAFE — panic: always-on, carries a message, routes through the handler */
if (config_unparseable) {
    panic("failed to parse configuration");
}
```
> Buys you: an always-on fatal exit with a message, routed through the same
> contract handler as everything else (so a custom handler — e.g. an embedded
> `uart_print` + reset — applies here too). `panic` is never compiled out by
> any flag. Reserve it for genuinely unrecoverable situations; for anything the
> caller could handle, return an error value rather than aborting.

</details>

[↑ Back to constructs](#table-of-c-constructs)

---

<a name="c-staticassert"></a>
## `Compile-time assertion`

[↑ Back to constructs](#table-of-c-constructs)

> Header: `contract.h` (`static_require`). Pulls in `types.h`, so:
> ```c
> #include "core/primitives/contract.h"
> ```

Some assumptions should fail the *build*, not the program — a struct that must
be exactly one cache line, a buffer that must be large enough, a config value
within range. A runtime `assert` for these is both too late and removable;
`static_require` checks them at compile time and can never be disabled.

<details>
<summary><b>1 — Pin a compile-time invariant</b></summary>

```c
/* RAW — the assumption lives in a comment and a runtime check that NDEBUG
   removes; nothing actually enforces it at build time. */
/* Header must stay 64 bytes for the on-wire format! */
assert(sizeof(Header) == 64);          /* too late, and gone under NDEBUG */
```
```c
/* SAFE — checked at compile time; build fails if the assumption breaks */
static_require(sizeof(Header) == 64, header_must_be_64_bytes);
static_require(BUFFER_SIZE >= 1024,   buffer_too_small);
```
> Buys you: the assumption is enforced where it can still be fixed — at
> compile time — and can never be compiled out. `static_require(cond, msg)`
> expands to C11 `_Static_assert` where available and a negative-size-array
> trick on C99. The `msg` is an **identifier**, not a string: no spaces or
> punctuation (`header_must_be_64_bytes`, not `"must be 64 bytes"`) — it shows
> up verbatim in the compiler error. The condition must be a compile-time
> constant expression; for runtime values, use `require` (see
> [`assert`](#c-assert)).

</details>

[↑ Back to constructs](#table-of-c-constructs)

---
