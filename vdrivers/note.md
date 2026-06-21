# Verification Drivers (`vdrivers/`)

## What this directory is

`vdrivers/` holds **verification drivers** — small headers that exist solely so
Frama-C/WP and the MC/DC coverage tooling have concrete, annotatable functions
to work on for Canon-C's *macro-templated* modules.

A verification driver is **not** shipped library code. It is a proof and
coverage harness, analogous to a test file, that lives outside the layered
module tree (`core/primitives/ → core/ → semantics/ → data/ → algo/ → util/`).
Nothing in the library includes anything from `vdrivers/`.

One driver exists per multi-file macro module. The complete set of modules
that will be verified through this directory — every module in the tree built
on the multi-file decl/impl/mangle/defn macro pattern — is fixed and listed
below. There are **14** such modules across three layers.

### semantics/ (2)

| Driver            | Verifies module     |
|-------------------|---------------------|
| `option_verify.h` | `semantics/option/` |
| `result_verify.h` | `semantics/result/` |

### data/ (3)

| Driver             | Verifies module  |
|--------------------|------------------|
| `vec_verify.h`     | `data/vec/`      |
| `deque_verify.h`   | `data/deque/`    |
| `hashmap_verify.h` | `data/hashmap/`  |

### algo/ (9)

| Driver              | Verifies module   |
|---------------------|-------------------|
| `map_verify.h`      | `algo/map/`       |
| `filter_verify.h`   | `algo/filter/`    |
| `fold_verify.h`     | `algo/fold/`      |
| `find_verify.h`     | `algo/find/`      |
| `any_all_verify.h`  | `algo/any_all/`   |
| `sort_verify.h`     | `algo/sort/`      |
| `search_verify.h`   | `algo/search/`    |
| `unique_verify.h`   | `algo/unique/`    |
| `reverse_verify.h`  | `algo/reverse/`   |

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

These are listed here for completeness but are **out of scope for the initial
`vdrivers/` rollout**, which targets the 14 multi-file modules above. Their
disposition is tracked separately and this note should be updated when decided.

## Why this directory has to exist

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
of shipped code.

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
copy.

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

## Relationship to MC/DC coverage

MC/DC is likewise a property of **executed concrete code**, not of macros. It is
measured by the coverage build compiling each module's test (e.g.
`test/semantics/option_test.c`) with a concrete type instantiated. As with WP,
there is nothing to cover until instantiation. Coverage work therefore lives in
the test tree (where the type is instantiated and exercised), while the WP
contracts live here in `vdrivers/`. The two are complementary views of the same
instantiated functions: WP proves the contracts hold; MC/DC confirms the tests
exercise every condition outcome.

## File-placement conventions

- One driver per macro module, named `<module>_verify.h`.
- Drivers `#include` the **real** module headers by relative path; they never
  copy module logic.
- Each driver instantiates the **minimum** representative type set needed to
  reach the module's behaviour (commonly one primitive; add a struct type only
  where struct layout or by-value semantics must be exercised).
- Each verified module gets a matching CI job (`frama-c-<module>`) that runs WP
  over its driver with a zero-residual (or documented-residual) enforcement
  gate, mirroring the existing per-header WP jobs.

## Status

| Module   | Driver             | Layer      | WP status    | Notes                          |
|----------|--------------------|------------|--------------|--------------------------------|
| option   | `option_verify.h`  | semantics/ | In progress  | First driver; pattern baseline |
| result   | `result_verify.h`  | semantics/ | Not started  | Reuses the option pattern      |
| vec      | `vec_verify.h`     | data/      | Not started  |                                |
| deque    | `deque_verify.h`   | data/      | Not started  |                                |
| hashmap  | `hashmap_verify.h` | data/      | Not started  |                                |
| map      | `map_verify.h`     | algo/      | Not started  | Distinct in/out types          |
| filter   | `filter_verify.h`  | algo/      | Not started  |                                |
| fold     | `fold_verify.h`    | algo/      | Not started  | Infallible & fallible variants |
| find     | `find_verify.h`    | algo/      | Not started  |                                |
| any_all  | `any_all_verify.h` | algo/      | Not started  |                                |
| sort     | `sort_verify.h`    | algo/      | Not started  |                                |
| search   | `search_verify.h`  | algo/      | Not started  |                                |
| unique   | `unique_verify.h`  | algo/      | Not started  |                                |
| reverse  | `reverse_verify.h` | algo/      | Not started  |                                |

Update each row's WP status as its driver lands. The six single-file macro
families (see "Single-file macro families" above) are tracked separately and
are not part of this table until their disposition is decided.
