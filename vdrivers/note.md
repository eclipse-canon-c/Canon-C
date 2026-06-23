# Verification Drivers (`vdrivers/`)

## What this directory is

`vdrivers/` holds **verification drivers** — small headers that exist solely so
Frama-C/WP has concrete, annotatable functions to work on for Canon-C's
*macro-templated* modules.

> **Drivers serve WP. Coverage TUs serve MC/DC.** A driver is read as a header
> by `frama-c -wp` and is never compiled or executed. MC/DC condition coverage
> is a separate concern with a separate artifact — a per-module
> `coverage/<module>_cover.c` — for the subset of modules that need it. Both are
> fed by the **same one instantiation** of the real shipped macros. The rest of
> this file explains *why* that split exists and *which* modules get which
> artifact. [`NOTE.md`](./NOTE.md) carries the deeper derivation (the
> three-reasons-for-absence taxonomy and the attribution-confirmation
> experiment).

A verification driver is **not** shipped library code. It is a proof harness,
analogous to a test file, that lives outside the layered module tree
(`core/primitives/ → core/ → semantics/ → data/ → algo/ → util/`). Nothing in
the library includes anything from `vdrivers/`.

One driver exists per multi-file macro module. There are **14** such modules
across three layers, listed in the tables below.

---

## Why the split exists: Shape A vs Shape B

A macro module's *structural shape* — not its layer, not "macro vs non-macro" —
determines what each tool can see, and therefore which artifacts it needs. This
is the single fact that governs everything below.

**Shape A — written-out bodies in `_impl.h`.** The function body is real source
lines in the `_impl.h` file. The *name* may be mangled and the *types* may be
macro parameters, but the body is written out. Examples: `map_impl.h`'s
`algo_map` (`ALGO_MAP_LINKAGE void algo_map(...) { ... for (...) ... }`),
`hashmap_impl.h`'s `_HM_INSERT` (full body with its `&&`/`||`/loop conditions on
real lines).

**Shape B — expression-macro bodies.** The body exists only as a macro that
expands at the instantiation site. Example: `option_impl.h`'s
`IMPL_OPTION_FILTER` is `{ return ((_o).has_value && (_pred)(...)) ? ... }`.
There is no `option_int_filter` anywhere in `option_impl.h` — it comes into
being only when `DEFINE_OPTION_FUNCTIONS(static inline, int)` expands inside a
`.c` file.

This distinction is the whole story for both tools, because **a Shape-B body has
no analyzable lines until some TU instantiates it:**

- **WP** verifies concrete functions. Shape-A headers go straight to `frama-c
  -wp` (this is what the nine enforced per-header jobs do). Shape-B headers are
  only macros until instantiation, so WP needs a **driver** to instantiate a
  concrete type for it to prove.
- **MC/DC** (`gcc -fcondition-coverage` → `gcov-14 --conditions`) attributes
  each condition to the file the line resolves to **after preprocessing**. For
  Shape A the line is in `_impl.h`, so the condition is attributed there,
  survives the coverage pipeline's `/test/` exclusions, and **already appears**
  in the MC/DC table. For Shape B the line resolves to the **instantiation
  site** — today the module's `_test.c` — so the condition is attributed to a
  `/test/` path and is then deleted by `grep -v "/test/"` and
  `lcov --remove '*/test/*'`. The conditions are measured and then thrown away.

So Shape-A modules need nothing extra for either tool. Shape-B modules need a
driver (for WP) **and** a cover TU (for MC/DC).

---

## The three artifacts per Shape-B module

The instantiation a Shape-B module needs for WP is the **same** instantiation it
needs for MC/DC — one instantiation, two consumers — but the consumers take it
in two different forms, so a Shape-B module has **three** files with three
distinct jobs. Do not collapse them.

| File                  | Path        | Consumer       | Role                                                                 |
|-----------------------|-------------|----------------|---------------------------------------------------------------------|
| `<module>_test.c`     | `test/...`  | ctest          | Functional test. Asserts correctness. Stays as-is; owns nothing new.|
| `<module>_verify.h`   | `vdrivers/` | `frama-c -wp`  | ACSL contracts on concrete prototypes + real-macro instantiation. Read as a header; never compiled. |
| `<module>_cover.c`    | `coverage/` | gcov           | `#include`s the driver, executes each generated function once. Compiled from a **non-`/test/`** path so its conditions survive the filter. |

Why three and not two: WP can consume a bare header (it preprocesses and proves
in place), but **gcov can only attribute conditions on lines a TU actually
compiles and runs** — a header is not a TU. The `_cover.c` is the executed `.c`
that gives the Shape-B conditions a home gcov keeps. Because it lives under
`coverage/` rather than `test/`, the macro-generated conditions are attributed
to `coverage/<module>_cover.c`, which the pipeline's exclusions do **not**
delete; and because it *runs* the functions, gcov records real outcomes instead
of `0/N`.

**This is not a second source of truth.** `<module>_cover.c` `#include`s the
driver, which instantiates the **real shipped macros**. It is a second *caller*
of the same generated functions, not a reimplementation. There remains exactly
one source of truth — the shipped macro — exercised by two callers (`_test.c`
and `_cover.c`). The integrity rule below is preserved.

### Minimal shape of a cover TU

```c
/* coverage/option_cover.c — NOT under test/, so not filter-excluded */
#define CANON_CONTRACT_IMPL
#include "vdrivers/option_verify.h"   /* real instantiation; ACSL is in comments, ignored by gcc */

int main(void) {
    /* Call each generated function once so gcov records its conditions.
     * Inputs only need to toggle each condition outcome; correctness is
     * already asserted by option_test.c — this TU is for attribution. */
    option_int s = option_int_some(1), n = option_int_none();
    (void)option_int_is_some(s); (void)option_int_is_none(n);
    /* ... one call per generated function, both outcomes of each condition ... */
    return 0;
}
```

(Often derivable by stripping the `EXPECT` harness out of the existing
`_test.c`.)

### Two hard requirements on the cover TU

1. **Same `-D` flags as the WP run.** The driver is written for the WP build,
   which compiles under `-DCANON_NO_REQUIRE -DNDEBUG`. Under that flag
   `require_msg` expands to `((void)0)`, so the NULL-guard branches disappear and
   are neither proved by WP nor counted by gcov. The coverage job already defines
   `-DCANON_NO_REQUIRE` for exactly this reason — to keep MC/DC measuring the
   same code WP proves. **A cover TU MUST be compiled with the same
   `-DCANON_NO_REQUIRE -DNDEBUG`.** Compiled without it, the `require_msg`
   branches reappear as conditions, the cover TU's condition count no longer
   matches the driver's WP goal set, and the two evidence streams silently
   diverge. State this in the cover TU's CMake target; do not leave it implicit.

2. **Build isolation.** `vdrivers/*_verify.h` is read **only** by `frama-c -wp`
   and must never be compiled into a test or coverage binary (its `static
   inline` definitions would collide with the same functions instantiated
   elsewhere in the link). `coverage/*_cover.c` is compiled **only** in the
   coverage job and must be excluded from the `build`, `valgrind`, `fuzzing`, and
   `lifetime-debug` targets. Neither `vdrivers/` nor `coverage/` may be swept
   into a globbed source root — a `.c` is far more likely than a `.h` to be
   picked up by an accidental `file(GLOB ...)`, which would add a duplicate
   definition of every generated function. Add them as explicit, job-gated CMake
   targets.

---

## Which modules get which artifact

Shape is **confirmed** for `option`/`result` (B) and `map`/`hashmap` (A) by
reading their `_impl.h`. For the rest it is **provisional** — inferred from
presence/absence in the MC/DC table — and is confirmed per-module by the
attribution debug run described in [`NOTE.md`](./NOTE.md) before its cover TU is
built.

**Cover TUs needed (Shape-B): five —** `option`, `result`, `vec`, `deque`,
`fold`. The nine Shape-A modules already surface in the MC/DC table via their
`_impl.h` bodies and get **no** cover TU. `fold` is the one algo module absent
from the table (its siblings all appear), which is why it is the only Shape-B
entry in the algo layer.

### semantics/ (2)

| Driver            | Verifies module     | Shape          | Cover TU (MC/DC)            |
|-------------------|---------------------|----------------|-----------------------------|
| `option_verify.h` | `semantics/option/` | B (confirmed)  | `coverage/option_cover.c`   |
| `result_verify.h` | `semantics/result/` | B (confirmed)  | `coverage/result_cover.c`   |

### data/ (3)

| Driver             | Verifies module  | Shape            | Cover TU (MC/DC)         |
|--------------------|------------------|------------------|--------------------------|
| `vec_verify.h`     | `data/vec/`      | B (provisional)  | `coverage/vec_cover.c`   |
| `deque_verify.h`   | `data/deque/`    | B (provisional)  | `coverage/deque_cover.c` |
| `hashmap_verify.h` | `data/hashmap/`  | A (confirmed)    | — already covered via `hashmap_impl.h` |

### algo/ (9)

| Driver              | Verifies module   | Shape            | Cover TU (MC/DC)          |
|---------------------|-------------------|------------------|---------------------------|
| `map_verify.h`      | `algo/map/`       | A (confirmed)    | — via `map_impl.h`        |
| `filter_verify.h`   | `algo/filter/`    | A (provisional)  | — via `filter_impl.h`     |
| `fold_verify.h`     | `algo/fold/`      | B (provisional)  | `coverage/fold_cover.c`   |
| `find_verify.h`     | `algo/find/`      | A (provisional)  | — via `find_impl.h`       |
| `any_all_verify.h`  | `algo/any_all/`   | A (provisional)  | — via `any_all_impl.h`    |
| `sort_verify.h`     | `algo/sort/`      | A (provisional)  | — via `sort_impl.h`       |
| `search_verify.h`   | `algo/search/`    | A (provisional)  | — via `search_impl.h`     |
| `unique_verify.h`   | `algo/unique/`    | A (provisional)  | — via `unique_impl.h`     |
| `reverse_verify.h`  | `algo/reverse/`   | A (provisional)  | — via `reverse_impl.h`    |

The `data/vec/` and `data/hashmap/` modules carry optional 6th/7th-file
extensions (`_range`, `_fmt`) that depend on other modules (range.h,
stringbuf.h, vec.h). Whether those extension functions are verified within the
same driver or deferred is decided per-module when that driver is built; the
core decl/impl/defn surface is verified first regardless.

### Single-file macro families — separate disposition (not yet decided)

The following are also macro-templated but live in a **single file** rather than
the multi-file split, so they are a distinct category and their handling is an
open decision (annotate in place where a non-macro surface exists, give them
their own driver, or document-only per the existing macro-disposition rule):

- `DEFINE_SLICE` in `core/slice.h`
- `DEFINE_OWNED` / `CANON_DROP` in `core/ownership.h`
- `DEFINE_ARRAY` in `data/array.h`
- `DECLARE_QUEUE` in `data/queue.h`
- `DECLARE_STACK` in `data/stack.h`
- `DEFINE_BORROWED_SLICE` in `semantics/borrow.h`

These are listed for completeness but are **out of scope for the initial
`vdrivers/` rollout**, which targets the 14 multi-file modules above. Their
disposition is tracked separately and this note should be updated when decided.

---

## Why this directory has to exist (WP detail)

Canon-C implements generics through **macro templates**. A module such as
Option is split across five files (`option_decl.h`, `option_impl.h`,
`option_defn.h`, `option_mangle.h`, plus the user-facing `option.h`), and the
actual functions are emitted by macros — `IMPL_OPTION_*`, `DEFINE_OPTION_*` —
only when a concrete type is instantiated with `CANON_OPTION(T)`.

This creates a hard problem for formal verification:

1. **There is no function to annotate.** Before instantiation, the shipped
   headers contain only macros. WP verifies concrete functions; it has nothing
   to act on until a type is instantiated.

2. **You cannot annotate the macro bodies.** ACSL annotations are C comments.
   The C preprocessor removes comments in translation **phase 3**, *before* it
   expands macros in **phase 4**. Any `/*@ ... */` written inside an
   `IMPL_OPTION_*` or `DEFINE_OPTION_*` macro is therefore deleted before the
   macro ever becomes a function. WP never sees it. This is a property of the C
   standard's translation order, not a Frama-C limitation, and it is the same
   reason the project's macro-disposition rule (see the DEFINE_SLICE precedent
   in `verification.md`) records macro bodies as "documented but not directly
   WP-verified."

The verification driver resolves both problems without modifying a single line
of shipped code. The *same* "no analyzable lines until instantiation" property
is what blinds MC/DC for Shape-B modules — which is why the cover TU exists.

## How a verification driver works

Each driver does exactly three things, in order:

1. **Instantiate a concrete type from the real, unmodified module headers.**
   The driver `#include`s the actual shipped `_decl.h` and `_defn.h` and
   instantiates one representative type (e.g. `option_int`). The struct and
   function *bodies* are produced by the real macros — the same code a user
   compiles.

2. **Attach ACSL contracts to concrete prototypes.** Between the type
   instantiation and the body generation, the driver declares contracted
   prototypes for the generated functions:

   ```c
   /*@ assigns \nothing;
       ensures \result == o.has_value; */
   bool option_int_is_some(option_int o);
   ```

   The contract sits on a real declaration, outside any macro, so the
   preprocessor leaves it intact.

3. **Let the macros generate the bodies.** `DEFINE_OPTION_FUNCTIONS(static
   inline, int)` (etc.) then emits the real function bodies.

Frama-C's linking step **merges a contract on a declaration onto the matching
definition** — documented behaviour: *"if there are multiple ACSL
specifications for a given function, they are merged; in particular, if one
version of the function has an ACSL specification but not the other, the merged
version will have it."* So WP checks the **macro-generated body against the
contract** written on the prototype.

## The integrity property this preserves (read this before adding a driver)

A verification driver **instantiates the real shipped macros**. It must **never**
be a hand-written, separately-maintained "verified copy" of the module's logic.

This is the rule that keeps the verification honest:

- The proof must be about the code users actually compile. Because the driver
  instantiates the real `_impl.h`/`_defn.h` macros, there is exactly **one
  source of truth**: the shipped macro. Prove the instantiation, and you have
  proved the code that runs.
- A parallel "verified" reimplementation would create **two sources of truth**
  that can silently drift. A bug fixed in the shipped macro but not the copy
  (or vice versa) would leave WP certifying code that nobody executes — which is
  worse than no proof, because it looks like assurance while guaranteeing
  nothing. **Do not do this.**

If you cannot verify a property by instantiating the real macro, the correct
outcome is a documented residual or a deliberately weak spec — not a divergent
copy. The cover TU obeys the same rule: it `#include`s the driver and is a second
*caller*, never a reimplementation.

## What a driver proves, and what it does not

- **Proves:** the functional contracts of the module's operations, on the
  instantiated representative type, against the genuinely macro-generated
  bodies. This is the same strength of claim as a directly-annotated header
  (e.g. `error.h`), extended to macro-templated code.
- **Represents, does not exhaust:** verification is performed on a
  representative instantiation (typically a primitive type such as `int`, and
  where layout matters, a small struct). The claim is that the *code the macros
  generate* is correct; it is demonstrated on the chosen instantiation(s), not
  separately re-proved for every `T` a user might instantiate. This is the
  standard macro-family disposition already used in the project.

## File-placement conventions

- One driver per macro module, named `<module>_verify.h`, in `vdrivers/`.
- Drivers `#include` the **real** module headers by relative path; they never
  copy module logic.
- Each driver instantiates the **minimum** representative type set needed to
  reach the module's behaviour (commonly one primitive; add a struct type only
  where struct layout or by-value semantics must be exercised).
- Each verified module gets a matching CI job (`frama-c-<module>`) that runs WP
  over its driver with a zero-residual (or documented-residual) enforcement
  gate, mirroring the existing per-header WP jobs.
- **Shape-B modules only:** a cover TU `coverage/<module>_cover.c` that
  `#include`s the driver and drives every generated function once — compiled
  **only** in the coverage job, with the same `-DCANON_NO_REQUIRE -DNDEBUG` the
  WP job uses, and kept out of every other build target and out of any globbed
  source root. See the two hard requirements above.

## Before building the cover TUs: confirm the attribution

The cover-TU rollout rests on the claim that GCC attributes Shape-B conditions to
the instantiation TU (`_test.c`) rather than to `_impl.h`. That is strongly
supported by reading `option_impl.h` (Shape B) against `map_impl.h` /
`hashmap_impl.h` (Shape A), but **confirm it on one real run before building five
cover TUs.** The confirming experiment — a report-only debug step dumping
`option_test`'s `gcov-14 --conditions` output next to `map_test`'s as a Shape-A
control — and the decision it drives (cover TUs vs. a single narrower filter
change that could close the whole Shape-B bucket far more cheaply) are described
in [`NOTE.md`](./NOTE.md). Do not build the cover TUs before that run.

## Status

| Module   | Driver             | Layer      | WP status    | Cover TU (MC/DC)            | Notes                          |
|----------|--------------------|------------|--------------|-----------------------------|--------------------------------|
| option   | `option_verify.h`  | semantics/ | In progress  | `option_cover.c` (pending)  | First driver; pattern baseline; Shape B |
| result   | `result_verify.h`  | semantics/ | Not started  | `result_cover.c` (pending)  | Reuses the option pattern; Shape B |
| vec      | `vec_verify.h`     | data/      | Not started  | `vec_cover.c` (pending)     | Shape B (provisional)          |
| deque    | `deque_verify.h`   | data/      | Not started  | `deque_cover.c` (pending)   | Shape B (provisional)          |
| hashmap  | `hashmap_verify.h` | data/      | Not started  | — (Shape A; via `_impl.h`)  | Already in MC/DC table         |
| map      | `map_verify.h`     | algo/      | Not started  | — (Shape A; via `_impl.h`)  | Distinct in/out types          |
| filter   | `filter_verify.h`  | algo/      | Not started  | — (Shape A; via `_impl.h`)  |                                |
| fold     | `fold_verify.h`    | algo/      | Not started  | `fold_cover.c` (pending)    | Infallible & fallible variants; Shape B (absent from MC/DC table) |
| find     | `find_verify.h`    | algo/      | Not started  | — (Shape A; via `_impl.h`)  |                                |
| any_all  | `any_all_verify.h` | algo/      | Not started  | — (Shape A; via `_impl.h`)  |                                |
| sort     | `sort_verify.h`    | algo/      | Not started  | — (Shape A; via `_impl.h`)  |                                |
| search   | `search_verify.h`  | algo/      | Not started  | — (Shape A; via `_impl.h`)  |                                |
| unique   | `unique_verify.h`  | algo/      | Not started  | — (Shape A; via `_impl.h`)  |                                |
| reverse  | `reverse_verify.h` | algo/      | Not started  | — (Shape A; via `_impl.h`)  |                                |

Update each row's WP status as its driver lands, and the Cover-TU column as each
Shape-B cover TU lands (and as provisional Shape classifications are confirmed by
the attribution debug run). The six single-file macro families are tracked
separately and are not part of this table until their disposition is decided.
