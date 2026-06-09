<p align="center">
  <img src="logo-github.png" alt="Canon-C" width="600"/>
</p>

# Canon-C 


<!-- Language -->
### Language
![C99](https://img.shields.io/badge/standard-C99-green.svg)

<!-- Platforms -->
### Platforms
![Windows](https://img.shields.io/badge/Platform-Windows-blue?logo=windows&logoColor=white)
![Linux](https://img.shields.io/badge/Platform-Linux-green?logo=linux&logoColor=white)
![macOS](https://img.shields.io/badge/Platform-macOS-black?logo=apple&logoColor=white)

<!-- License -->
### License
![License: MIT](https://img.shields.io/badge/License-MIT-blue)
![License: CC0-1.0](https://img.shields.io/badge/License-CC0--1.0-lightgrey)

<!-- Development / Testing -->
### Development & Testing
![GCC](https://img.shields.io/badge/GCC-Dev%2FTest-orange)
![Clang](https://img.shields.io/badge/Clang-Dev%2FTest-orange)
![MSVC](https://img.shields.io/badge/MSVC-Dev%2FTest-orange)
![ASan+UBSan](https://img.shields.io/badge/ASan%2BUBSan-Configurable-lightgrey)
![Valgrind](https://img.shields.io/badge/Valgrind-Configurable-lightgrey)
![Fuzzing](https://img.shields.io/badge/Fuzzing-Configurable-lightgrey)

<!-- Static Analysis -->
### Static Analysis
![clang-tidy](https://img.shields.io/badge/clang--tidy-Configurable-lightgrey)
![Cppcheck](https://img.shields.io/badge/Cppcheck-Configurable-lightgrey)
![Polyspace/LDRA](https://img.shields.io/badge/Polyspace%2FLDRA-Configurable-lightgrey)

<!-- Formal Verification & Timing Analysis -->
### Formal Verification & Timing Analysis
![Frama-C](https://img.shields.io/badge/Frama--C-Configurable-lightgrey)
![MC/DC](https://img.shields.io/badge/MC--DC-Configurable-lightgrey)
![aiT WCET](https://img.shields.io/badge/aiT%20WCET-Configurable-lightgrey)

<!-- Production / Certification -->
### Production & Certification
![CompCert](https://img.shields.io/badge/CompCert-Configurable-lightgrey)
![DO-178C Level C/D](https://img.shields.io/badge/DO--178C-Configurable-lightblue)
![ISO26262 ASIL C/B](https://img.shields.io/badge/ISO26262-Configurable-lightblue)
![IEC 62304](https://img.shields.io/badge/IEC62304-Configurable-lightblue)
![EN 50128](https://img.shields.io/badge/EN50128-Configurable-lightblue)
![IEC 61508 SIL2](https://img.shields.io/badge/IEC61508%20SIL2-Configurable-lightblue)
![ECSS-E-ST-40C](https://img.shields.io/badge/ECSS--E--ST--40C-Configurable-lightblue)

---

## Table of Contents

1. [Motivation](#motivation)
2. [IMPORTANT](#important)
3. [Dependency Rule (Strict)](#dependency-rule-strict)
4. [Design Philosophy](#design-philosophy)
5. [Getting Started](#getting-started)
    - [1. core/primitives/ — Foundations](#1-coreprimitives--foundations)
    - [2. core/ — Core memory & ownership](#2-core--core-memory--ownership)
    - [3. semantics/ — Explicit semantic types](#3-semantics--explicit-semantic-types)
    - [4. data/ — Fixed-capacity collections](#4-data--fixed-capacity-collections)
    - [5. data/convenience/ — Ergonomic collections](#5-dataconvenience--ergonomic-collections)
    - [6. algo/ — Algorithms on collections](#6-algo--algorithms-on-collections)
    - [7. util/ — Utility modules](#7-util--utility-modules)
6. [Scope](#scope)
7. [Examples](#examples)
8. [Usage](#usage)

---

## Motivation

Over time, I found myself repeatedly re-implementing the same low-level
patterns in C: arenas, error handling, vectors, parsing, file I/O, and
more. Existing libraries either hide allocation, impose heavy frameworks,
rely on implicit conventions, or lack consistency across modules. None
matched my preference for explicit ownership, predictable allocation,
header-only usage, and minimal hidden behavior.

In practice, this meant rewriting the same infrastructure across projects
and re-learning the same informal rules every time.

Canon-C is an attempt to unify these patterns into a small, disciplined,
and composable semantic standard library for C, governed by strict design
principles. The goal is **not to add new functionality**, but to make
program intent visible directly in APIs, so that ownership, lifetime,
failure, and data flow are immediately clear at call sites.

C is fast, portable, and predictable, but its native semantics are
low-level and mechanical. Writing non-trivial programs in C requires
memorizing conventions and boilerplate around memory management,
ownership rules, error handling, and iteration. These details obscure
intent and significantly increase cognitive load.

Modern languages embed these abstractions directly into the language.
While powerful, this often hides behavior, increases semantic complexity,
and reduces transparency. Canon-C takes a different approach: **C itself
remains unchanged. Meaning is added explicitly through libraries, not
syntax.**

The result is a set of semantic building blocks that improve readability,
preserve explicit control, maintain performance, and remain fully
transparent.

The long-term goal is for this taxonomy of C abstractions to become a
**shared standard vocabulary** — a common foundation that allows C
programs to communicate intent clearly, consistently, and safely.

### From shared vocabulary to compositional verification

Making intent explicit at call sites is the immediate benefit. The longer
arc is that the same discipline that makes Canon-C readable also makes it
verifiable, and that the verification effort *composes* — proofs done
once at the substrate are not re-done in every function that uses it.

Most safety-critical C is verified, when it is verified at all, function
by function. An engineer writes ACSL annotations, runs Frama-C,
discharges proof obligations, and moves on. The work scales linearly
with the codebase, which is why formal verification remains rare outside
the deepest specialization tier of the embedded industry.

Canon-C is built around a specific architectural bet: that a small,
formally verified primitive set provides a substrate against which the
calling code's verification burden is meaningfully reduced. Concretely,
when a function is written using verified primitives, three things
happen to its proof obligations:

1. **Runtime-safety obligations travel with the primitives.** Bounds
   checking, overflow detection, null-pointer validity, division-by-zero,
   and invalid-shift checks are discharged once at the substrate. Calling
   code does not re-prove them — it proves it meets the substrate's
   preconditions, which is a smaller and more local obligation.

2. **Functional-correctness obligations are partially reduced.** The
   primitive's postcondition becomes available to the caller as a
   verified fact, so the caller's proofs about its own logic can chain
   through the substrate rather than re-establishing low-level
   properties from scratch.

3. **A predictable, characterized residual remains.** Loop invariants,
   frame conditions over caller-defined state, and functional
   postconditions specific to the caller's logic still need proofs of
   their own. So does a small, named class of obligations that inherits
   WP integer-theory or memory-model limitations from the substrate
   (modular arithmetic, bitwise reasoning, uintptr_t round-trips). These
   replays appear at higher layers under different goal names but use
   the same manual arguments as the substrate, so the proof effort
   reuses across the codebase rather than being duplicated.

This is not a claim that calling code becomes verified for free. The
honest version of the bet is: verified primitives discharge the
runtime-safety obligations of calling code, reduce the
functional-correctness burden, and constrain the residual to a
predictable, documented class of obligations whose manual arguments are
written once and replayed under new goal names at each layer above.

### Verification status

The substrate is not hypothetical. Some of the headers are
formally verified using Frama-C WP with ACSL contracts, and CI enforces
the proof state on every push to master — any drift in the named
residual goals fails the build.

For current numbers, see:

- **`docs/verification.md`** — per-header proof obligation counts,
  prover breakdowns, manually discharged goals with their proof
  arguments, and reproduction commands.
- **`docs/deviations.md`** — every unproved goal documented by ID, with
  category, rationale, and mitigation.
- **`docs/traceability.md`** — coverage record (lines, branches, MC/DC),
  per-header coverage results, and history of measurements.

All currently unproved goals are demonstrated triple-prover-resistant
under Alt-Ergo + Z3 + CVC5 with a 120-second timeout, and fall into a
small number of named categories (modular arithmetic, bitwise complement
in specs, SWAR patterns, uintptr_t round-trips, alignment formulas, the
ACSL non-termination idiom for panic handlers) that are characteristic
of WP's encoding rather than weaknesses in the code.

The same code that WP proves is the code that runs in production. The
CI build uses `-DCANON_CHECKED_FORCE_FALLBACK` and
`-DCANON_BITS_FORCE_FALLBACK` to keep MC/DC coverage measurement aligned
with the verified path, and fuzzed binaries exercise the same fallback
paths under ASan + UBSan. Proof, coverage, and runtime instrumentation
measure one code path under three different lenses.

### Translation table

Canon-C's primitive set covers the constructs that appear in most C
functions:

| Raw C construct        | Canon-C replacement                                  |
| ---------------------- | ---------------------------------------------------- |
| Arithmetic operations  | `checked.h`                                          |
| Comparisons            | `compare.h`                                          |
| Bit operations         | `bits.h`                                             |
| Array indexing         | `slice_at` returning `Option`                        |
| Pointer arithmetic     | `ptr.h`                                              |
| Error propagation      | `Result` combinators (`TRY_UNWRAP`, `and_then`)      |
| Optional values        | `Option` combinators (`unwrap_or`, `map`, `filter`)  |
| Null checks            | `ptr_or`                                             |
| Multi-way dispatch     | function pointer tables with `Option` lookup         |
| Precondition guards    | `require_msg`                                        |
| Counted loops          | `range` + `RANGE_FOR`                                |
| Transform loops        | `algo_map`                                           |
| Filter loops           | `algo_filter`                                        |
| Reduce loops           | `algo_fold`                                          |
| Search loops           | `algo_find`, `algo_search`, `algo_any_all`           |
| Sort, unique, reverse  | corresponding algorithms                             |
| Shared mutable state   | explicit parameters, regions, functional composition |

When a function is written using only these replacements, its
verification reduces to discharging its specification-specific
obligations (loop invariants, caller-state frame conditions, functional
postconditions) over a verified substrate, plus a bounded set of
inherited residuals whose manual arguments are reused from the
substrate's deviations record.

### Three usage patterns

Canon-C is designed to be useful across a range of adoption depths.

**Verification-grade composition.** Code that will be formally verified
end-to-end — typical for the deepest certification tiers (DO-178C Level A
and B, ISO 26262 ASIL D, IEC 62304 Class C). These users adopt the strict
discipline and benefit most from the compositional verification approach.
The substrate's verified primitives, the deviations record, and the CI
enforcement of named residuals are directly reusable as certification
evidence.

**Disciplined embedded development.** Code that uses Canon-C's
conventions — `Result` for errors, ownership annotations, fixed-capacity
collections, contract-checked preconditions — without committing to
whole-program formal verification. Typical for ASIL B/C automotive,
IEC 62304 Class B medical, and industrial control work. These users get
runtime safety improvements and audit traceability without the full
proof discharge cost.

**Selective adoption.** Code that uses individual Canon-C modules for
specific benefits — `checked.h` for overflow safety, `slice.h` for bounds
checking, `arena.h` for predictable allocation — without adopting the
full convention set.

The same library serves all three. The strict discipline is an option
for code that needs it, not a requirement for using Canon-C.

### What about compile-time ownership enforcement?

A reasonable question after reading the ownership annotations
(`owned()`, `borrowed()`, `moved()`, `dropped()`) is whether Canon-C
provides a build flag that promotes them from documentation to
compiler-enforced types — something like `CANON_OWNERSHIP_STRICT`
that would catch owned-vs-borrowed mismatches at compile time across
the whole project.

It does not, and it deliberately does not. C99 lacks the language
machinery needed to make this work cleanly at the annotation level:
no move semantics to mark a variable invalid after `consume(x)`, no
way to forbid copies of an "owned" struct, no flow-sensitive type
checking. Any project-wide build flag attempting full enforcement
would either break the visible `Type*` notation, force every
declaration into a wrapper-and-unwrap dance, or silently allow the
exact mistakes it claims to prevent. None of those outcomes survive
Canon-C's design philosophy ("if behavior cannot be understood by
reading the header, it does not belong").

What Canon-C provides instead:

1. **`DEFINE_OWNED(T)` and `DEFINE_BORROWED(T)`** in `core/ownership.h`
   generate distinct wrapper struct types per type, opt-in. The
   compiler refuses to pass `owned_Arena` where `borrowed_Arena` is
   expected — real compile-time enforcement at API boundaries, with
   no build flag required. Use this for the types where the cost of
   the wrap/unwrap ceremony is worth catching mismatches at compile
   time.

2. **External static analyzers** for everything the C type system
   cannot express: use-after-move, double-drop, lifetime parameters
   across function calls, ownership flow through pointer aliases.
   Tools like Frama-C, Polyspace, and LDRA do dataflow analysis on
   the same source code Canon-C ships, reading the lowercase
   annotations (`owned()`, `borrowed()`, etc.) as ACSL contracts or
   tool-specific markers. This is the route certification users
   already take for the substrate verification described above; the
   same toolchain extends naturally to ownership flow analysis at the
   application layer.

The full rationale — the C99 limits, the design paths considered, why
each one breaks something — are recorded in `docs/design-decisions.md` 
entry. The short version is that `DEFINE_OWNED` covers the 
type-distinction case the C99 type system can enforce, and static analyzers
cover everything beyond it. The two together are the answer.

### Open empirical question

Canon-C's working hypothesis is that most safety-critical embedded code
above the substrate can be expressed compositionally — using only the
primitives in the translation table — with a residual proof effort that
scales sub-linearly in the codebase rather than linearly. Two things
remain to be measured against real industrial codebases:

1. **What fraction of typical safety-critical application-layer code is
   expressible without imperative escape hatches?** The current working
   estimate is 70-85%, with the remainder requiring escape hatches for
   state machines with non-trivial transitions, hardware register
   manipulation, and performance-critical inner loops. This estimate is
   provisional.

2. **How does the inherited residual scale across layers?** The
   substrate's unproved goals re-emit at higher layers under different
   goal names whenever calling code touches the same WP-resistant
   patterns. Early evidence from ptr.h verifying against checked.h
   (where some ptr.h residuals are direct replays of checked.h's
   residuals — see `docs/deviations.md`, VERIFY-006 category 1)
   suggests the inheritance is bounded and predictable, but the scaling
   behavior over a full application codebase is not yet measured.

Refining both estimates against real workloads is part of the project's
roadmap.

---

Canon-C is licensed under **MIT** for human-authored content, with
substantially AI-generated portions additionally dedicated to the public
domain under **CC0-1.0**. See `LICENSE` for the full text of both
licenses, `AI-USAGE.md` for details on AI tool usage, and individual
file headers for per-file licensing.

> Canon-C is header-only and permissively licensed. There is no copyleft
> obligation when using Canon-C in your project — commercial or
> non-commercial, modified or unmodified.

---

## IMPORTANT

**Feedback requested on:**

1. Does this categorization make sense?
2. Are things in the right layers?
3. What’s missing or wrong?

## Authorship and tool use

Canon-C is a single-author project developed as a human-directed
collaboration with AI coding assistants. AI assistance is extensive
across the codebase — it was used to produce text for the C
implementation, the ACSL annotations, the test code, deviation
arguments, and documentation.

What is human-authored is the judgment exercised throughout: the
architectural design and layering, the scope of the library, the
API surface and its contracts, the verification approach, the
deviation classifications, and decisions about which AI-produced
output is kept, modified, or rejected. AI-produced text is in the
repository only because I reviewed it, judged it correct, and chose
to include it. I am accountable for every line in the codebase
regardless of how it was first produced.

The implementations are tested end-to-end, the verification runs
on real CI with real Frama-C and SMT provers, and the proofs are
real.

For detailed disclosure, including the tools used and the licensing
model, see `AI-USAGE.md`. The codebase is licensed under MIT for
human-authored content (including the engineering and editorial
judgment expressed throughout) with AI-generated portions
additionally dedicated to the public domain under CC0-1.0, in
accordance with the Eclipse Foundation's generative AI guidelines.

---

## Dependency Rule (Strict)

Canonical layer order:

core/ → semantics/ → data/ → algo/ → util/

**Rules:**

- Modules are organized by **semantic depth**, not feature count. Lower layers define unavoidable mechanics; higher layers build meaning on top.
- Lower ( The lowest is "core" ) layers may be used by higher ( The highest is "util" ) layers, but **upward or circular dependencies are strictly forbidden**.
- Each module must be independently usable.
  
This ensures **explicitness** and prevents hidden behaviors or fragile dependencies.

---

## Design Philosophy

- Everything is optional
- Everything is explicit
- No forced hidden allocation
- No forced implicit ownership
- No forced global state
- No forced framework coupling
- No forced require runtime
- Works in **plain C99**
- Convenience macros requiring GNU C or C23 are optional (disable with `#define CANON_NO_GNU_EXTENSIONS`)
- No clever tricks

> If behavior cannot be understood by reading the header, it does not belong.
> Abstractions must **clarify behavior, not conceal it**.

---

## Getting Started

Canon-C is organized into **strict layers**, from low-level mechanics to high-level convenience modules.  
To get the most out of the library, explore layers **in order**.

---

### 1. `core/primitives/` — Foundations

This is the **bedrock of Canon-C**:

- Fixed-width types (`u8`, `usize`, `isize`, etc.)
- Compile-time numeric limits & alignment constants
- Pointer arithmetic and alignment utilities
- Overflow-checked arithmetic
- Lifetime token types shared across owning modules
- Min/max/clamp helpers
- Explicit contracts and assertions (`require_msg`, `ensure_msg`)

**Goal:** Understand primitive building blocks of memory, arithmetic, and pointer safety.

---

### 2. `core/` — Core memory & ownership

Builds on primitives for **fundamental memory management**:

- Linear bump allocator (`arena.h`)
- Fixed-size object pools (`pool.h`)
- Slice views (`slice.h`) and region-based lifetimes (`region.h`)
- Scope-bound defer for cleanup pairing (`scope.h`)
- Ownership and borrowing annotations (`ownership.h`)
- Low-level memory utilities (`memory.h`)

**Goal:** Learn to safely allocate, manage, and access memory with explicit ownership.

---

### 3. `semantics/` — Explicit semantic types

Higher-level abstractions for **safer programming**:

- `Option<T>` (`option/`) — presence/absence semantics
- `Result<T,E>` (`result/`) — success/failure handling
- Borrowed views (`borrow.h`) — non-owning references
- Structured diagnostics (`diag.h`) — contextual error reporting

**Goal:** Introduce **explicit semantic meaning** to data and operations.

---

### 4. `data/` — Fixed-capacity collections

Bounded, **caller-owned containers**:

- Typed vectors (`vec/`), double-ended queues (`deque/`)
- Fixed-size arrays, stacks, queues, priority queues
- Hashmaps, bitsets, ranges, string builders

**Goal:** Work with structured data safely and predictably **without hidden allocations**.

---

### 5. `data/convenience/` — Ergonomic collections

Optional modules for **ease-of-use**:

- Auto-growing vectors (`dynvec`), small-buffer vectors (`smallvec`)
- Auto-growing string builders (`dynstring`)

**Goal:** Simplify common tasks while understanding trade-offs (hidden allocations, growth logic).

---

### 6. `algo/` — Algorithms on collections

Generic and typed reusable algorithms, each implemented as a modular
5-file architecture (consistent with `data/` and `semantics/` layers):

- Transformation (`map/`), filtering (`filter/`), folding (`fold/`), searching (`search/`)
- Sorting (`sort/`), reversing (`reverse/`), uniqueness (`unique/`)
- Predicate checks (`any_all/`), element location (`find/`)

Eight of the nine modules provide three levels of use:
- **Generic** — `void*` + function pointer interface, works on any type
- **Typed macro** — compile-time type safety, wraps the generic level
- **Typed instantiation** — `DEFINE_ALGO_X(type)` stamps out fully typed
  slice variants with no `void*`, directly optimizable by the compiler

`fold/` is the exception: the accumulator type and element type are both
caller-determined and may differ, so a generic `void*`-based function
would require unsafe function pointer casts. Instead, fold provides
macro-based operations at Levels 1 and 2 (`ALGO_FOLD`, `ALGO_FOLD_RESULT`)
and typed slice functions at Level 3 (`DEFINE_ALGO_FOLD`). The deviation
is documented in `fold.h`.

`map/` supports cross-type transformation (different input and output types).
When multiple cross-type mappings share the same input type, use
`DEFINE_ALGO_MAP` for the first and `DEFINE_ALGO_MAP_CROSS` for subsequent
calls to avoid redefinition of the in-place variant.

**Goal:** Apply operations to collections **predictably and generically**,
with opt-in typed instantiation for cases where full type visibility matters.

---

### 7. `util/` — Utility modules

Convenience utilities for **Canon-C codebases** — not standalone replacements
for specialized libraries. These modules exist to prevent convention mismatch
at the boundary where `algo/` ends and application code begins. Every module
returns `Result`, accepts explicit arenas, and follows the same ownership and
lifetime conventions as the rest of Canon-C.

- Strings: `str.h`, `str_split.h`, `str_join.h`, `str_view.h`, `intern.h`
- Logging: `log.h`, `log_macros.h`
- File I/O, parsing, random number generation, timing

**Goal:** Extend Canon-C's conventions — explicit ownership, `Result`-based
errors, arena-backed allocation — into application-level utility code.
For production logging, use `zlog`. For everything else, `util/` removes
the seam between Canon-C's lower layers and the rest of your codebase.



## Scope

Canon-C covers exactly what is needed to write explicit, ownership-aware C programs
without reaching for a larger framework. Every module exists because the pattern
it encodes recurs across real programs and has no good idiomatic C equivalent.

Nothing was added speculatively. The cutoff rule is simple: if a module can be
built cleanly from existing layers using Canon-C's own primitives, it doesn't
need to live here — the user writes it. If the absence of a module forces every
downstream user to re-invent the same unsafe boilerplate, it belongs.

Canon-C stops at the boundary where general-purpose utility ends and
application-specific concerns begin. Concretely, all things that are not covered are listed below . These domains are large, platform-specific,
and opinionated in ways that don't belong in a vocabulary library — and they're
already covered by libraries that specialize in exactly that.

For what Canon-C intentionally omits, established C libraries exist:

---

**Networking / async I/O**
- `libuv` — cross-platform async I/O and event loop, the industry standard for
  non-blocking network programming in C. Battle-tested across Windows, Linux,
  and macOS. Used as the runtime underneath Node.js.
- `lwIP` — lightweight TCP/IP stack for embedded systems with constrained memory.
  Use when libuv's hosted runtime assumptions are too heavy for your target.

**GUI / graphics**
- `raylib` — immediate mode graphics, input, audio, and windowing in a single
  library. Zero external dependencies, builds on Windows, Linux, macOS, and
  compiles through Emscripten for WebAssembly. The closest in philosophy to
  Canon-C among GUI libraries — explicit, minimal, no hidden framework.
  Use for games, simulations, tools, and visualizers.
- `nuklear` — single-header immediate mode GUI in pure C99. No dependencies,
  no state machine, no heap allocation by default. Renders through a backend
  you provide (raylib, SDL2, OpenGL). Use when you need a lightweight debug
  or tool UI without pulling in a full GUI framework.
- `SDL2` — cross-platform window creation, input, audio, and 2D rendering.
  Lower level than raylib — you bring your own rendering pipeline. Use when
  you need fine-grained control over the graphics stack or are integrating
  with an existing renderer.
- `lvgl` — embedded GUI library designed for microcontrollers and displays
  with constrained memory. Runs on bare-metal, FreeRTOS, and Zephyr. No
  operating system required. Use for industrial HMIs, IoT devices, and any
  embedded target with a display.

**Threading / concurrency**
- `pthreads` — POSIX standard threading API. Available natively on Linux and macOS.
  No external dependency needed on Unix-like systems.
- `TinyCThread` — portable implementation of the C11 threads API in two files.
  No external dependencies. Use when you need cross-platform threading including
  Windows without pulling in a larger framework.
- `C11 <threads.h>` — if your compiler fully supports C11, the standard threading
  API is available directly with no library needed.
- `liblfds` — portable lock-free data structures (queues, stacks, hash tables,
  ring buffers, freelists) in C89 with no external dependencies. License is
  effectively whatever you want — the author grants any popular license on
  request and places the code in the public domain. Performs no allocations
  of its own, works with stack/heap/shared memory/NUMA, compiles on bare and
  freestanding C89 implementations including kernel mode. Used in production
  by AT&T, Red Hat, and Xen. Philosophically the closest match to Canon-C
  among lock-free libraries — header-only, no hidden allocation, no global
  state. Use for SPSC/MPSC queues between pinned threads where mutex-based
  synchronization adds unacceptable contention or jitter (real-time audio,
  telemetry pipelines, game loops, packet processing).
- `Concurrency Kit (ck)` — broader concurrency primitives including lock-free
  data structures, RCU, and epoch-based memory reclamation. BSD-licensed,
  used in production at companies running latency-sensitive infrastructure.
  Heavier than liblfds but more complete. Use when you need RCU or
  hazard-pointer semantics in addition to lock-free containers.

**High-resolution timing**
- Platform primitives are usually the right answer here — no library required.
  Use `clock_gettime(CLOCK_MONOTONIC_RAW)` on Linux, `mach_absolute_time()`
  on macOS, `QueryPerformanceCounter` on Windows, and `__rdtsc()` / `__rdtscp()`
  for cycle-level timing on x86. Beware TSC invariance, frequency scaling,
  CPU migration, and the cost of `rdtsc` itself. Useful for profiling,
  benchmarking, game loops, animation timing, and any workload where event
  timestamps drive correctness.

**Serialization**
- `cJSON` — ultralightweight JSON parser and emitter in ANSI C. Single file, MIT
  license. Widely used in embedded and systems projects.
- `yyjson` — fastest JSON library in C. Use when performance matters and cJSON
  is the bottleneck.
- `mpack` — MessagePack for C. Use when binary serialization is needed over JSON:
  smaller payloads, faster parsing, schema-driven.
- `miniz` — single-file deflate/inflate and ZIP compression. Use for compressing
  serialized data or reading and writing ZIP archives.

**Math / numerical computing**
- `cglm` — optimized graphics and general-purpose math (vectors, matrices,
  quaternions, transforms) in pure C99. Header-only, SIMD-accelerated where
  available, no dependencies. Fully cross-platform. Use for 3D transforms,
  physics, coordinate geometry, and any linear algebra that fits the graphics
  math model.
- `CMSIS-DSP` — ARM's official digital signal processing library for Cortex-M
  targets. Fixed-point and floating-point FFT, filters, matrix operations,
  statistics. Use on ARM embedded targets doing signal processing, sensor
  fusion, or control loops.
- `libfixmath` — portable fixed-point arithmetic (Q16.16) in pure C. No
  floating-point unit required, no dependencies. Fully cross-platform. Use
  on embedded targets without an FPU where `<math.h>` operations are
  prohibitively slow or unavailable.
- `libdecnumber` — IBM's reference implementation of IEEE 754-2008 decimal
  floating-point. Used internally by GCC. Portable C, permissive license.
  Use whenever exact decimal arithmetic matters — monetary values, billing,
  metering, accounting, or any domain where binary floating-point rounding
  errors are unacceptable.
- `Intel Decimal Floating-Point Math Library` — Intel's BID (Binary Integer
  Decimal) implementation. Faster than libdecnumber on x86, BSD-licensed.
  Use as a drop-in performance upgrade when libdecnumber is the bottleneck
  and the target is x86.
- `GSL (GNU Scientific Library)` — comprehensive scientific computing
  library: statistics, distributions, linear algebra, optimization, root
  finding, Monte Carlo, ODE solvers. GPL-licensed (a real constraint for
  proprietary codebases — verify license compatibility before adopting).
  Use for statistical analysis, numerical methods, and quantitative work
  where breadth of functions matters more than the last cycle of performance.
- `Cephes` — public domain library of special mathematical functions
  (gamma, error function, Bessel, elliptic integrals, statistical
  distributions). The numerical foundation many other libraries derive
  from. Use when GSL's license is a problem or when you need only special
  functions without the full GSL footprint.

**Hashing (non-cryptographic)**
- `xxHash` — extremely fast general-purpose hash. Use for checksums, data
  fingerprinting, or hash tables where security is not a concern.
- `SipHash` — hash-flooding resistant hash function. Recommended for string keys
  in Canon-C's hashmap — prevents algorithmic complexity attacks when keys are
  user-controlled input.

**Cryptography**
- `monocypher` — minimal cryptographic library, single C file, zero allocation,
  no global state. Covers ChaCha20, Poly1305, Blake2, Argon2, Ed25519. The
  closest in philosophy to Canon-C among crypto libraries.
- `libsodium` — higher-level cryptographic API, widely known and battle-tested.
  Heavier than monocypher and manages its own initialization, but more familiar
  to most developers.

**Testing**
- `Unity` — the most widely used unit testing framework in embedded and C99
  projects. Simple assertion macros, no dynamic allocation, minimal setup.
- `Criterion` — modern testing framework with automatic test discovery and no
  boilerplate. Better suited for desktop targets where a richer test runner
  is acceptable.
- `greatest` — single header, public domain. Use when you want zero friction
  and no framework overhead at all.

**Logging**
- `zlog` — for production systems requiring async, multi-target, or
  runtime-configurable logging. Canon-C's `log.h` covers the common case —
  reach for zlog only when you need to write to multiple sinks simultaneously,
  change log levels at runtime without recompiling, or need buffered async
  writes in high-throughput systems.

**Database / storage**
- `SQLite` — the universal embedded relational database. Zero configuration,
  single file, battle-tested. Use when your data has relational structure or
  you need SQL query capability.
- `LMDB` — memory-mapped key-value store. Extremely clean C API, no hidden
  allocation, ACID transactions. Use when you need fast persistent storage
  without the overhead of a full relational database.

**Embedded / bare-metal**
- `FreeRTOS` — the most widely adopted RTOS for constrained microcontrollers.
  Minimal footprint, simple task scheduler, direct hardware control. Use when
  your device has well-defined behavior and you want full architectural control
  with minimal overhead.
- `Zephyr` — full-featured, scalable RTOS managed by the Linux Foundation.
  Built-in drivers, networking, Bluetooth, and security. Use when your project
  needs to scale across hardware revisions, run multiple subsystems, or be
  maintained long-term. Higher learning curve than FreeRTOS but significantly
  more structure.

**WebAssembly**
- `Emscripten` — compiles C and C++ to WebAssembly using LLVM as a drop-in
  replacement for gcc/clang. Output runs in browsers, Node.js, and standalone
  Wasm runtimes. Canon-C's C99 code compiles cleanly through Emscripten
  without modification.

---

> **Integrating external libraries with Canon-C**
>
> These libraries use traditional C API conventions — raw pointers, integer
> error codes, implicit ownership, and occasional global state. They cannot
> be made fully Canon-C-idiomatic, but a thin adapter layer can keep the
> rest of your codebase consistent. How cleanly they integrate depends on
> the library:
>
> **Wrap cleanly** — monocypher, mpack, miniz, xxHash, SipHash, cglm,
> libfixmath, CMSIS-DSP, libdecnumber, Intel BID, Cephes, liblfds.
> Flat API, no global state, buffer-based. Maps directly to Canon-C
> conventions with a thin adapter.
>
> **Wrap partially** — SQLite, LMDB, cJSON, yyjson, libsodium,
> TinyCThread, pthreads, C11 `<threads.h>`, raylib, nuklear, SDL2, zlog,
> GSL, Concurrency Kit.
> Core operations wrap well into `Result<T, Error>` and `owned()`/`borrowed()`.
> Callbacks, domain-specific error codes, global initialization, stateful
> handle lifecycles, or thread entry points create contained mismatches
> that cannot be eliminated, only documented.
>
> **Isolate only** — libuv, FreeRTOS, Zephyr, lwIP, lvgl.
> Callback-driven, tick-driven, or RTOS task models are architecturally
> incompatible with Canon-C's explicit control flow. Contain the mismatch
> behind a single adapter file. Canon-C's lower layers (arena, slice,
> result) remain usable inside these environments — just not as wrappers
> around them.
>
> **Not applicable** — Unity, Criterion, greatest, Emscripten.
> Testing frameworks run at build time, not as runtime dependencies in
> your application. Emscripten is a compiler toolchain, not a linked
> library. These do not require integration adapters.
>
> Canon-C provides the following tools for integration boundaries:
>
> - `core/ownership.h` — annotate adapter functions with `owned()`,
>   `borrowed()`, and `DEFINE_OWNED` to make ownership explicit at the
>   integration point. Wrap external handles like `sqlite3*`, `uv_loop_t*`,
>   or `MDB_env*` with `CANON_DROP` to ensure cleanup is never missed.
>
> - `semantics/result/result.h` — convert integer return codes from SQLite,
>   libsodium, or libuv into `Result<T, Error>` at the adapter boundary.
>   Use `TRY` and `TRY_REMAP` to propagate failures up without boilerplate.
>   For libraries with many domain-specific codes (SQLite has 30+), define
>   a local error enum and instantiate `CANON_RESULT` with it.
>
> - `core/slice.h` / `semantics/borrow.h` — pass Canon-C `bytes_t`,
>   `cbytes_t`, and `borrowed_bytes` directly into buffer-based APIs like
>   mpack, miniz, and LMDB's `MDB_val`. The `{ptr, len}` layout is
>   compatible with most C buffer conventions without copying.
>
> - `core/scope.h` — use `DEFER(cleanup_expr) { body }` to pair acquisition
>   with release for external handles when the work runs to completion.
>   For adapter functions with error-return paths — the common case when
>   wrapping libraries like SQLite or libuv, where almost every API call
>   can fail — use the standard C99 `goto cleanup;` idiom instead. `DEFER`
>   does not fire on `return`, `break`, or outward `goto`, and adapter
>   functions almost always need cleanup on error exits. `DEFER` is
>   Canon-C's contribution (a C99-portable macro for run-to-completion
>   cleanup); `goto cleanup;` is plain C and needs no library support.
>   Canon-C recommends the combination because each tool fits a different
>   shape of function.
>
> - `semantics/diag.h` — attach context to external failures as they
>   propagate. "SQLite failed → while writing record → during sync" with
>   no allocation, using `DIAG_PUSH` and `DIAG_PROPAGATE`.
>
> - `core/arena.h` — some libraries accept custom allocator hooks
>   (SQLite's `sqlite3_config(SQLITE_CONFIG_MALLOC)`, lwIP's `mem_malloc`).
>   Canon-C's arena can back these if you write the adapter, giving you
>   explicit lifetime control over the library's internal allocations.
>
> The pattern is always the same: one thin adapter file per external library,
> using the tools above at the boundary. Everything above the adapter stays
> pure Canon-C. Callback-driven APIs (libuv, FreeRTOS tasks) and libraries
> requiring global initialization (libsodium's `sodium_init()`) cannot be
> fully wrapped — isolate them behind the adapter and contain the convention
> mismatch there.

---

> **Bare-metal and embedded use**
>
> Canon-C has two platform dependencies that require attention on
> bare-metal or RTOS targets: stdio and malloc.
>
> **stdio**
> stdio enters Canon-C in exactly two ways. The default contract panic
> handler in `core/primitives/contract.h` uses `fprintf` — replace it
> once at startup with `contract_set_handler()` pointing to your UART
> or fault handler, and stdio is never reached again from the entire
> core layer. The second entry point is rendering functions that use
> `FILE*`. `diag_print()` and `log.h` use `fprintf` — avoid these on
> bare-metal. Use `diag_render()` instead: it writes the same output
> into a caller-supplied `char[]` buffer with no `FILE*` dependency,
> and the caller sends that buffer to UART, CAN, flash, or whatever
> the platform provides. Everything else in Canon-C that includes
> `<stdio.h>` does so only for `snprintf` in optional formatting
> functions — skip those call sites and the dependency is inert on
> toolchains with stub stdio support (newlib, picolibc).
>
> **malloc / free**
> `core/memory.h` includes `<stdlib.h>` and provides `mem_alloc()` /
> `mem_free()` as explicit wrappers over `malloc` / `free`.
> `core/arena.h` and `core/pool.h` include `memory.h` but never call
> `malloc` — they operate entirely on caller-supplied buffers. The heap
> dependency is only active if your code calls `mem_alloc()`,
> `mem_alloc_type()`, or `mem_alloc_array()` directly. Everything in
> `data/convenience/` (dynvec, smallvec, dynstring) uses heap allocation
> internally.
>
> On bare-metal, if you avoid `mem_alloc()` and `data/convenience/`, no
> `malloc` is ever reached. If you do need heap allocation, `#define
> malloc`, `realloc`, and `free` to your RTOS allocator (e.g. FreeRTOS's
> `pvPortMalloc` / `pvPortReAlloc` / `vPortFree`) before including any
> Canon-C header. This single redefinition propagates through the entire
> dependency chain automatically.
>
> **What requires no mitigation at all**
> `core/primitives/types.h`, `core/primitives/limits.h`,
> `core/primitives/bits.h`, `core/primitives/checked.h`, and
> `core/primitives/compare.h` pull in only freestanding headers
> (`<stdint.h>`, `<stddef.h>`, `<limits.h>`, `<stdbool.h>`) and are
> safe on any target without modification. `core/scope.h` is the most
> portable header in Canon-C — it has zero header dependencies at all,
> not even freestanding ones, and is safe on any target including
> bare-metal and freestanding environments with no libc. All other
> `core/` headers include `contract.h` — they require the handler
> replacement described above.

---

## Examples

Canon-C is not about writing less code. It is about writing code that stays
readable, explicit, and safe as the codebase grows. The `...` below represents
real programs — the conventions you see in each snippet are the same ones
you will find everywhere in a Canon-C codebase, whether it is 300 lines or
30,000.

---

### Contracts — preconditions are visible at every function boundary
```c
/* WITHOUT Canon-C — preconditions are invisible or scattered */
int process(Arena* arena, Config* cfg, size_t count) {
    if (!arena) return -1;  /* what does -1 mean? */
    if (!cfg)   return -1;  /* same code, different cause — indistinguishable */
    if (!count) return -1;  /* caller has no idea which check failed */
    /* ... */
}
```
```c
/* WITH Canon-C — preconditions are visible, grep-able, self-documenting */
#define CANON_CONTRACT_IMPL
#include "core/primitives/contract.h"

result_int_Error process(
    borrowed(Arena*)  arena,
    borrowed(Config*) cfg,
    usize             count)
{
    require_msg(arena != NULL, "process: arena is NULL");
    require_msg(cfg   != NULL, "process: cfg is NULL");
    require_msg(count  > 0,    "process: count must be > 0");

    /* ... */
}
```

No silent failures. No NULL dereferences discovered at 3am.
Every precondition is visible, grep-able, and self-documenting.

---

### Ownership — intent is visible at every call site
```c
/* WITHOUT Canon-C — ownership is implicit, conventions drift */
Config* config_create(uint8_t* buffer, size_t size); /* owned? borrowed? */
char*   config_get_name(const Config* cfg);           /* heap copy? pointer into cfg? */
void    config_destroy(Config* cfg);                  /* can I use cfg after this? */

/* ... 2000 lines later ... */

Config* cfg  = config_create(buf, sizeof(buf));
char*   name = config_get_name(cfg);
config_destroy(cfg);
printf("%s\n", name); /* is name still valid? nobody knows */
```
```c
/* WITH Canon-C — intent is visible at every call site */
#include "core/ownership.h"

/* caller receives ownership — caller must free */
owned(Config*)  config_create(owned(u8*) buffer, usize size);

/* callee borrows — caller retains ownership */
str_t           config_get_name(borrowed(const Config*) cfg);

/* callee consumes — caller must not use after */
void            config_destroy(dropped(Config*) cfg);

/* ... 2000 lines later, still the same conventions ... */

owned(Config*) cfg  = config_create(buf, sizeof(buf));
str_t          name = config_get_name(cfg);
config_destroy(cfg); /* cfg is now invalid — dropped() makes this obvious */
```

No convention drift. A new contributor reading any function signature
immediately knows who owns what.

---

### Cleanup — pairing acquisition with release
```c
/* WITHOUT Canon-C — allocations are scattered, lifetimes implicit */
void process(void) {
    void* a = malloc(256);
    void* b = malloc(512);
    if (!validate(a)) {
        free(a);
        return; /* b leaked */
    }
    free(a);
    free(b);
    /* ... 500 lines of this ... */
}
```
```c
/* WITH Canon-C — cleanup is paired with acquisition, visible at the site */
#include "core/arena.h"
#include "core/scope.h"

u8    backing[4096];
Arena arena;
arena_init(&arena, backing, sizeof(backing));

ArenaMark mark = arena_mark(&arena);
DEFER(arena_reset_to(&arena, mark)) {
    /* allocations made in this block live until the block ends */
    void* tmp = arena_alloc(&arena, 256);

    /* ... work that runs to completion ... */
}
/* arena reset to `mark` here — no manual free */

/* ... 500 lines later, same arena, still predictable ... */
```

No hidden allocations. No malloc scattered across the codebase.
Lifetime boundaries are visible at the site where they are declared.

`DEFER` (from `core/scope.h`) handles run-to-completion blocks like
this cleanly — the body runs straight through, the cleanup expression
fires at the closing brace. But most real functions have error-return
paths *after* a resource has been acquired, and `DEFER` does not fire
on `return`, `break`, or outward `goto`. For those functions, the
standard C99 `goto cleanup;` idiom is what Canon-C recommends — not
as a library feature, but because it is already the right tool for
that shape of function. Here is what it looks like paired with
`require_msg` for the precondition check:

```c
/* WITHOUT Canon-C — cleanup scattered across every exit path */
int load_calibration(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return -1;

    void* buf = malloc(4096);
    if (!buf) {
        fclose(f);              /* must remember f */
        return -1;
    }

    if (fread(buf, 1, 4096, f) != 4096) {
        free(buf);              /* must remember buf */
        fclose(f);              /* must remember f */
        return -1;
    }

    if (!verify_crc(buf)) {
        free(buf);              /* must remember buf */
        fclose(f);              /* must remember f */
        return -1;
    }

    apply(buf);

    free(buf);
    fclose(f);
    return 0;
}
```
```c
/* WITH Canon-C — one cleanup block, every exit routed through it */
#define CANON_CONTRACT_IMPL
#include "core/ownership.h"
#include "core/primitives/contract.h"

int load_calibration(borrowed(const char*) path) {
    require_msg(path != NULL, "load_calibration: path is NULL");

    int   rc  = 0;
    FILE* f   = NULL;
    void* buf = NULL;

    f = fopen(path, "r");
    if (!f) { rc = ERR_OPEN; goto done; }

    buf = malloc(4096);
    if (!buf) { rc = ERR_ALLOC; goto done; }

    if (fread(buf, 1, 4096, f) != 4096) { rc = ERR_IO;  goto done; }
    if (!verify_crc(buf))                { rc = ERR_CRC; goto done; }

    apply(buf);

done:
    if (buf) free(buf);
    if (f)   fclose(f);
    return rc;
}
```

One cleanup block, listed in reverse acquisition order, reached by
every exit path through a single `goto done`. Adding a new error path
later means adding one line — the `goto done` automatically routes
through the existing cleanup. Nothing to remember, nothing to forget.

**`DEFER` and `goto cleanup;` are complementary, not alternatives.**
`DEFER` is Canon-C's contribution, supplied by `core/scope.h` — a
C99-portable macro that pairs cleanup with acquisition for
run-to-completion blocks where the body runs straight through without
error-return paths. `goto cleanup;` is plain C99 and needs no header,
no macro, no library support. Canon-C recommends it for error-handled
functions because it is already the right tool for that shape, proven
at scale in the Linux kernel, glibc, and every major C codebase that
handles errors carefully. Canon-C did not invent it and does not
reinvent it. A single function may use both — `goto cleanup;` for the
outer error-handled structure, `DEFER` for a straight-line inner block
such as a scratch arena checkpoint.

**Contracts versus error propagation — two tools, two categories of
failure.** Notice that the example above uses `require_msg(path != NULL, ...)`
at the top *and* `if (!f) { rc = ERR_OPEN; goto done; }` further down.
These are not redundant — they check different categories of failure.
`require_msg` (from `core/primitives/contract.h`) is for **programmer
errors**: NULL pointers where NULL is forbidden, preconditions the
caller promised to honor, invariants that must hold if the code is
correct. If a contract fires, the program has a bug and `require_msg`
panics to stop before damage spreads. Runtime failures are different:
`fopen` returning NULL, allocation failing, a hardware register not
responding, a checksum not matching. These are **not bugs** — they are
real conditions a correct program must handle. For those, propagate
the failure to the caller as a value — most Canon-C functions do this
with `Result<T, Error>` in the return type (shown in the next example),
pairing it with `goto cleanup;` inside the function body when resources
need to be released before the Result leaves. `Result` is how the
failure *leaves* the function; `goto cleanup;` is how the function
*tears down* its state before the Result leaves. The two roles are
orthogonal — one is about call signatures, the other is about internal
control flow — and a well-written function with both error paths and
resources to release will usually use both together.

Using `require_msg` for a recoverable runtime failure converts an
error into a crash; using `Result` for a contract violation hides a
bug behind an error value. The rule for picking between contracts and
error propagation is simple: **if the failure means your code has a
bug, use a contract. If the failure means reality didn't cooperate,
propagate the error through `Result`.**

---

### Error propagation — failures are values, not surprises
```c
/* WITHOUT Canon-C — errors are easy to ignore, origin is lost */
int parse_and_validate(Arena* arena, const char** inputs, size_t count) {
    int vec[MAX];
    if (collect(arena, inputs, count, vec) < 0) return -1; /* which error? */
    if (validate(vec)                      < 0) return -1; /* same -1, different cause */
    return sum(vec);
}

/* ... caller, 1000 lines away ... */

int result = parse_and_validate(&arena, inputs, count);
if (result < 0) {
    printf("something failed\n"); /* no idea what or where */
}
```
```c
/* WITH Canon-C — failures are values, propagation is explicit */
#include "semantics/result/result.h"
#include "semantics/error.h"

result_int_Error parse_and_validate(
    borrowed(Arena*)       arena,
    borrowed(const char**) inputs,
    usize                  count)
{
    require_msg(arena  != NULL, "arena is NULL");
    require_msg(inputs != NULL, "inputs is NULL");

    vec_int vec;
    TRY(int, Error, collect(arena, inputs, count, &vec));

    /* ... */

    TRY(int, Error, validate(&vec));

    return sum(&vec);
}

/* ... caller, 1000 lines away ... */

result_int_Error r = parse_and_validate(&arena, inputs, count);
if (!result_int_Error_is_ok(r)) {
    Error e = result_int_Error_unwrap_err(r);
    printf("Failed: %s\n", error_message(e));
}
```

No errno. No sentinel returns. No forgotten error checks.
Failures propagate explicitly through the entire call chain.

---

### Error context — the full chain, no allocation, no framework
```c
/* WITHOUT Canon-C — one error code reaches the surface, context is gone */
int load_config(const char* path, int* out_timeout) {
    const char* raw = read_field(path, "timeout");
    if (parse_int(raw, out_timeout) < 0)
        return -1; /* caller sees -1. was it a missing file? bad field? overflow? */
    return 0;
}

/* ... surface ... */

if (load_config("config.txt", &timeout) < 0) {
    printf("config failed\n"); /* the chain is gone — root cause is invisible */
}
```
```c
/* WITH Canon-C — full chain survives from root cause to surface */
#include "semantics/diag.h"
#include "semantics/result/result.h"
#include "semantics/error.h"

/* ... deep in the call chain ... */

static bool parse_timeout(
    borrowed(const char*) str,
    int*                  out,
    Diag*                 diag)
{
    require_msg(str != NULL, "parse_timeout: str is NULL");
    require_msg(out != NULL, "parse_timeout: out is NULL");

    result_i64_Error r = parse_i64(str, NULL);
    if (!result_i64_Error_is_ok(r)) {
        DIAG_PUSH(diag, ERR_PARSE_FAILED, "timeout value is not a valid integer");
        return false;
    }

    *out = (int)result_i64_Error_unwrap(r);
    return true;
}

/* ... one level up ... */

static bool load_config(
    borrowed(const char*) path,
    int*                  out_timeout,
    Diag*                 diag)
{
    require_msg(path        != NULL, "load_config: path is NULL");
    require_msg(out_timeout != NULL, "load_config: out_timeout is NULL");

    const char* raw_value = /* ... read from file ... */ "bad_value";

    DIAG_PROPAGATE(
        parse_timeout(raw_value, out_timeout, diag),
        diag,
        ERR_INVALID_FORMAT,
        "failed to parse timeout field in config",
        false
    );

    return true;
}

/* ... surface, 1000 lines away ... */

Diag diag = diag_init();
int  timeout;

if (!load_config("config.txt", &timeout, &diag)) {
    diag_print(&diag, stderr);
}

/*
 * Output:
 * [0] parse.c:24 in parse_timeout() — error 3: "timeout value is not a valid integer"
 * [1] config.c:51 in load_config()  — error 4: "failed to parse timeout field in config"
 *
 * No heap allocation. No logging framework. No lost context.
 * The full chain survives from root cause to surface — stack allocated.
 */
```

Without `diag.h`, the caller sees one error code and nothing else.
With `diag.h`, the full context chain propagates from root cause to
surface — allocation-free, framework-free, visible at every level.

---

### Borrow lifetime — know when a borrowed value is still valid
```c
/* WITHOUT Canon-C — stale borrows are silent undefined behavior */
const char* get_name(Arena* scratch) {
    char* name = arena_alloc(scratch, 64);
    strcpy(name, "Alice");
    arena_reset(scratch); /* name is now dangling — nothing warns you */
    return name;
}

/* ... 500 lines later ... */

const char* name = get_name(&scratch);
/* scratch was reset somewhere in between */
printf("%s\n", name); /* undefined behavior — crash or silent corruption */
```
```c
/* WITH Canon-C — borrow validity is assertable at any point */
#include "core/region.h"
#include "core/arena.h"
#include "semantics/borrow.h"

u8    backing[2048];
Arena scratch;
arena_init(&scratch, backing, sizeof(backing));

Region r;
region_begin(&r);
region_attach_arena(&r, &scratch); /* arena resets automatically on region_end */

/* stamp the borrow with this region's lifetime */
region_id_t  rid  = region_id(&r);
borrowed_str name = borrowed_str_from_cstr("Alice", &r);

/* ... pass name through several layers of the codebase ... */

/* assert the region is still alive before using name */
region_assert_borrow_valid(&r, rid);

/* ... work with name ... */

region_end(&r);
/* arena reset, cleanup hooks fired in reverse registration order */
/* name is now invalid — region is closed                         */

/* ... 500 lines later ... */

/* if someone tries to use name here: */
region_assert_borrow_valid(&r, rid); /* fires — region is closed */

/*
 * Plain C: stale borrows are silent — undefined behavior, data corruption,
 *          crashes that only appear in production under specific conditions.
 *
 * Canon-C: borrow validity is assertable at any point in the codebase.
 *          The region carries the lifetime. The assertion catches misuse early.
 */
```

Three enforcement levels — no code changes needed between them:

- **Default** — assertions are debug-only, compiled away with `NDEBUG`
- **`CANON_STRICT`** — assertions are always-on in every build including production
- **Frama-C + `CANON_NO_REQUIRE`** — borrows proved statically, zero runtime cost

The `...` represents real programs where borrowed values travel far
from their source — exactly where stale borrows become invisible bugs.

---

## Usage

Canon-C is **header-only**. To use:

```c
#include "Canon-c/core/arena.h"
#include "Canon-c/semantics/option/option.h"
``` 
Then you’re ready to go. No runtime or build system integration required.
