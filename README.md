# Canon-C

Canon-C — Treating C as an execution backend, not a semantic authority.
A semantic standard library built from explicit, composable C modules.

---

## Motivation

Over time, I kept rewriting the same low-level patterns in C — arenas, error handling, vectors, parsing, file I/O, and iteration utilities.
Existing libraries either hid allocation, imposed frameworks, relied on implicit conventions, or lacked consistency across modules. None matched my preferences for 
explicit ownership, predictable allocation, header-only usage, and minimal hidden behavior.

In practice, this meant repeatedly reimplementing the same infrastructure and re-learning the same rules across projects.

Canon-C is an attempt to unify these patterns into a small, disciplined, and composable set of C modules with strict design rules. 
The goal is not to add new functionality, but to make program intent visible directly from APIs, so that ownership, lifetime, failure, and data flow are obvious at call sites.

C is fast, portable, and predictable, but its native semantics are low-level and mechanical.
Writing non-trivial programs in C often requires memorizing patterns, conventions, and boilerplate around memory management, ownership rules, error handling, and iteration. These details obscure intent and increase cognitive load.

Many modern languages embed abstractions directly into the language to address this. While powerful, 
this also increases semantic complexity and hides behavior. Canon-C takes a different approach: C itself is left untouched. Meaning is added through explicit libraries, not syntax.

The result is a set of semantic building blocks that improve readability, maintain explicit control, preserve performance, and remain fully transparent.

Canon-C is licensed under **GPL-3.0** to ensure the core remains open and shared.

**For library users**: You may use Canon-C in your projects under GPL-3.0 terms.

**Future plans**: Dual licensing (GPL + permissive commercial license) will be 
introduced to support broader adoption in proprietary codebases. Feedback welcome.

---

## Core Idea

Everything is a library.  
Everything is explicit.  

C is used as a **low-level target**, while higher-level semantics
(memory, lifetime, failure, data flow, transformation)
are expressed through small, composable modules.

You do not inherit semantics from the language.  
You **select** semantics by choosing which headers to include.

---

## Design Philosophy

- Everything is optional
- Everything is explicit
- No hidden allocation
- No implicit ownership
- No global state
- No framework coupling
- No required runtime
- No language extensions
- No clever tricks

If behavior cannot be understood by reading the header, it does not belong.

Abstractions must clarify behavior, not conceal it.

---

## Included Modules (Current)

### core/primitives/
- `types.h` — portable integer and size type aliases (u8, usize, isize, etc.)
- `limits.h` — common constants and limits (integer bounds, alignment, capacity limits)
- `bits.h` — portable bit manipulation (popcount, clz, rotate, power-of-2 checks)
- `checked.h` — overflow-safe arithmetic (checked add/sub/mul, alignment helpers)
- `contract.h` — explicit contracts and assertions (require, ensure, unreachable, panic)

### core/
- `memory.h` — low-level memory utilities (alignment, safe memcpy wrappers)
- `arena.h` — explicit linear allocation (bump allocator)
- `pool.h` — fixed-size object pool allocator (arena-backed)
- `scope.h` — RAII-style deferred cleanup macros

### data/
- `vec.h` — bounded dynamic vector (caller-owned buffer, fixed capacity)
- `deque.h` — bounded double-ended queue (ring buffer, fixed capacity)
- `queue.h` — FIFO queue wrapper (fixed capacity)
- `stack.h` — LIFO stack wrapper (fixed capacity)
- `range.h` — explicit integer range generator (ascending/descending, signed support)
- `stringbuf.h` — incremental string builder (arena- or buffer-backed, fixed capacity)

### data/convenience/
- `dynvec.h` — auto-growing vector (unlimited capacity, hidden allocations)
- `smallvec.h` — inline-first vector with one-time spill (stack-optimized)
- `dynstring.h` — auto-growing string builder (hidden allocation)
  
### semantics/
- **`option/`** — modular Option<T> implementation
  - `option.h` — user-facing API for explicit presence/absence of a value
  - `option_impl.h` — pure implementation logic (customizable)
  - `option_mangle.h` — name mangling conventions (overridable)
  - `option_decl.h` — declarations for separate compilation
  - `option_defn.h` — definitions for header-only or .c files
- **`result/`** — modular Result<T, E> implementation
  - `result.h` — user-facing API for explicit success/failure handling
  - `result_impl.h` — pure implementation logic (customizable)
  - `result_mangle.h` — name mangling conventions (overridable)
  - `result_decl.h` — declarations for separate compilation
  - `result_defn.h` — definitions for header-only or .c files
- `error.h` — common error codes and human-readable messages

### algo/
- `map.h` — element-wise transformation (supports different input/output types)
- `filter.h` — select elements matching predicate
- `fold.h` — reduce sequence to single value (infallible & fallible variants)
- `find.h` — locate first matching element
- `any_all.h` — predicate checks (any / all)
- `sort.h` — generic in-place sorting (with comparator)
- `search.h` — binary search utilities (lower_bound, exact match)
- `unique.h` — remove consecutive duplicates (in-place)
- `reverse.h` — reverse sequence in-place

### util/
- `string.h` — safe string operations (copy, concat, predicates)
- `str_split.h` — non-mutating string splitting (borrowed views)
- `str_join.h` — safe string joining (buffer-based & allocating)
- `log.h` — minimal, explicit logging with Result-based error handling
- `file.h` — safe file I/O (read/write whole files, arena-backed preferred)
- `parse.h` — robust parsing of integers, unsigned, and floating-point values
- `time.h` — high-resolution stopwatch (monotonic timing)
- `random.h` — fast, explicit PRNG (PCG32, no global state)

---

**Note:** Modules in `*/convenience/` subdirectories trade explicitness for ergonomics.
The `core/primitives/` subdirectory contains foundational types and operations that all
other modules depend on. Core modules prioritize determinism and explicit control; 
convenience modules prioritize ease of use and may include hidden allocations or 
automatic growth. Choose based on your constraints: use core for production/embedded/
real-time, convenience for prototyping.

**Modular architecture:** The `option/` and `result/` modules now use a modular design
that separates implementation logic, naming conventions, declarations, and definitions.
Most users only need to include `option.h` or `result.h`, but advanced users can customize
behavior by overriding individual components. This architecture enables:
- Custom naming schemes (e.g., `Maybe<T>` instead of `Option<T>`)
- Function specialization for specific types
- Flexible compilation models (header-only or separate compilation)
- Easier maintenance and testing

All modules are **header-only** by default and require no runtime or build system integration.

---

## Usage

Canon-C is **header-only**. To use:

1. Clone or download the repository
2. Add the Canon-C directory to your include path
3. Include the modules you need:
```c
   #include "Canon-c/core/arena.h"
   #include "Canon-c/semantics/option/option.h"
```

---

No build system integration required. No linking step. Just include and use.

**Compiler support**: C99 or later, works with GCC, Clang, MSVC.

## What This Is

- A curated set of **foundational C modules**
- A shared structure and discipline for semantic libraries
- A functional-style semantic vocabulary built *on top of* C
- Usable from both **C and C++**
- Optimized for readability, predictability, and control

---

## What This Is Not

- Not a replacement for C++
- Not a new programming language
- Not object-oriented
- Not macro-heavy metaprogramming
- Not a framework
- Not opinionated about application architecture

This project does not compete with languages.  
It avoids them.

---

## Semantic Layers

Modules are organized by **semantic depth**, not by feature count. Lower layers
define unavoidable mechanics; higher layers build meaning on top of them.


core/ — memory, lifetime, scope, primitives

semantics/ — meaning 

data/ — data shapes 

algo/ — transformations 

util/ — optional helpers



### Dependency Rule (Strict)

core → semantics → data → algo → util


- Lower (lowest is core here) layers may be used by higher (highest is util here) layers.  
- Upward or circular dependencies are strictly forbidden.  
- Each module must be independently usable.  

This rule ensures **explicitness** and prevents hidden behaviors or fragile dependencies.
