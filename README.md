# Canon-C

A collection of small, explicit C libraries designed to improve readability,
semantic clarity, and development speed (via reduced cognitive load),
while preserving C’s performance and predictability.

This project treats **C as an execution backend**, not as a semantic authority.

---

## Motivation

C is fast, portable, and honest, but its native semantics are low-level and
mechanical. Writing non-trivial programs in C often requires memorizing
patterns, rules, and boilerplate such as manual memory management,
ownership conventions, error flags, and loop idioms.

These details obscure intent and slow development.

Many modern languages solve this by embedding abstractions directly into
the language. While powerful, this increases semantic complexity,
hides behavior, and raises the cost of mastery.

This project takes a different path.

C is left untouched.  
Meaning is added through **libraries**, not syntax.

The goal is to enable **literate, intention-revealing C code** while
preserving performance, portability, and transparency.

Readability is treated as a real engineering constraint.

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
- `option.h` — explicit presence/absence of a value (with combinators)
- `result.h` — explicit success/failure with value or error (with combinators)
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
All modules are **header-only** and require no runtime or build system integration.

---

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


core/ — memory, lifetime, scope

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
