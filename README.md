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
- `contract.h` — explicit contracts and assertions (require_msg, ensure_msg, unreachable, panic)
- `ptr.h` — named pointer arithmetic and alignment (ptr_offset, ptr_diff, align_up, ptr_in_range, PTR_CONTAINER_OF)

### core/
- `memory.h` — low-level memory utilities (alignment, safe mem_copy/mem_move wrappers)
- `region.h` — explicit lifetime boundaries for borrowed values
- `arena.h` — explicit linear allocation (bump allocator)
- `pool.h` — fixed-size object pool allocator (arena-backed)
- `scope.h` — RAII-style deferred cleanup macros
- `slice.h` — non-owning views into contiguous memory (bytes_t, cbytes_t, str_t, DEFINE_SLICE)
- `ownership.h` — explicit ownership and borrowing annotations (owned, borrowed, moved, dropped, DEFINE_OWNED, CANON_DROP)

### data/
- **`vec/`** — bounded dynamic vector (caller-owned buffer, fixed capacity) — modular 7-file architecture
  - `vec.h` — user-facing API; header-only entry point
  - `vec_impl.h` — pure implementation logic (zero naming assumptions)
  - `vec_mangle.h` — name mangling conventions (individually overridable)
  - `vec_decl.h` — forward declarations for separate compilation
  - `vec_defn.h` — complete definitions (header-only or .c files)
  - `vec_range.h` — optional extension: `extend_from_range()` (depends on `range.h`)
  - `vec_fmt.h` — optional extension: `to_stringbuf()` (depends on `stringbuf.h`)
- **`deque/`** — bounded double-ended queue (ring buffer, fixed capacity) — modular 5-file architecture
  - `deque.h` — user-facing API; header-only entry point
  - `deque_impl.h` — pure implementation logic (zero naming assumptions)
  - `deque_mangle.h` — name mangling conventions (individually overridable)
  - `deque_decl.h` — forward declarations for separate compilation
  - `deque_defn.h` — complete definitions (header-only or .c files)
- `array.h` — fixed-size typed array with compile-time capacity (DEFINE_ARRAY, safe indexing, slice/bytes views, iteration macros)
- `queue.h` — FIFO queue wrapper over deque (fixed capacity); includes `DECLARE_QUEUE`
- `stack.h` — LIFO stack wrapper over vec (fixed capacity); includes `DECLARE_STACK`
- `range.h` — explicit integer range generator (ascending/descending, isize-based, overflow-safe)
- `stringbuf.h` — incremental string builder (arena- or buffer-backed, fixed capacity)

### data/convenience/
- `dynvec.h` — auto-growing typed vector (hidden heap allocation, 2x growth)
- `smallvec.h` — inline-first vector with at-most-one spill to heap or arena
- `dynstring.h` — auto-growing string builder (hidden heap allocation, 2x growth)

### semantics/
- **`option/`** — modular Option\<T\> implementation
  - `option.h` — user-facing API for explicit presence/absence of a value
  - `option_impl.h` — pure implementation logic (customizable)
  - `option_mangle.h` — name mangling conventions (overridable)
  - `option_decl.h` — declarations for separate compilation
  - `option_defn.h` — definitions for header-only or .c files
- **`result/`** — modular Result\<T, E\> implementation
  - `result.h` — user-facing API for explicit success/failure handling
  - `result_impl.h` — pure implementation logic (customizable)
  - `result_mangle.h` — name mangling conventions (overridable)
  - `result_decl.h` — declarations for separate compilation
  - `result_defn.h` — definitions for header-only or .c files
- `error.h` — common error codes and human-readable messages
- `borrow.h` — semantic non-owning view types (borrowed_ptr, borrowed_str, borrowed_bytes, DEFINE_BORROWED_SLICE)
- `diag.h` — structured diagnostic frames for error context chains (Diag, DiagFrame, DIAG_PUSH, DIAG_PROPAGATE; no allocation)

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

**Note:** Modules in `*/convenience/` subdirectories trade explicitness for ergonomics — hidden allocations, automatic growth, fewer required parameters. 
`core/` modules trade ergonomics for control — deterministic allocation, explicit lifetimes, no surprises. 
Use core for production, embedded, and real-time; use convenience for prototyping and tools.

**Primitives integration:** Canon-C replaces C's semantically weak primitives with named equivalents that carry explicit intent:

`usize`/`isize` instead of `size_t`/`ptrdiff_t`, 
`CANON_USIZE_MAX`/`CANON_ISIZE_MAX` instead of `SIZE_MAX`/`PTRDIFF_MAX`,
`require_msg()`/`ensure_msg()` instead of `assert()`, 
and `mem_copy()`/`mem_move()` instead of `memcpy()`/`memmove()`.

As a result, `<stddef.h>`, `<stdbool.h>`, and `<assert.h>` are never included by any Canon-C module.
The only exceptions are `<stdarg.h>`, `<stdio.h>`, and `<string.h>` in `data/stringbuf.h`,
where no Canon-C substitute exists at the `data/` layer:

`va_list` is a compiler ABI intrinsic; 
`vsnprintf` has no Canon-C equivalent; 
`strlen` is only needed because `stringbuf_append(const char*)` accepts unlengthed strings — `stringbuf_append_str(str_t)` avoids it entirely.

**Ownership and borrowing:** Ownership is expressed at three levels of explicitness.
`core/ownership.h` provides annotation macros (`owned`, `borrowed`, `moved`, `dropped`) that make intent visible at every call site with zero runtime cost. 
`core/slice.h` provides concrete non-owning view types (`bytes_t`, `cbytes_t`, `str_t`, `slice_##type`) that make non-ownership structural rather than conventional.
`semantics/borrow.h` adds source-tagged wrappers (`borrowed_str`, `borrowed_bytes`, `borrowed_slice_##type`) for APIs where the non-ownership claim must be part of the type itself, not just an annotation.

**Modular architecture:** `vec/`, `deque/`, `option/`, and `result/` separate implementation logic, naming conventions, declarations, and definitions into distinct files.
The single-file entry points (`vec.h`, `deque.h`, `option.h`, `result.h`) cover the common case. 
The underlying files exist for projects that need custom naming schemes (e.g. `Maybe<T>` instead of `Option<T>`, project-prefixed types), per-type specialization, or flexible compilation models (header-only or separate compilation via `DECLARE_`/`DEFINE_`).

**Result and Option variants:** All push/pop/enqueue/dequeue operations across `vec/`, `deque/`, `queue.h`, and `stack.h` return `result_bool_Error` instead of bare `bool`,
giving callers specific error codes (`ERR_CAPACITY_EXCEEDED`, `ERR_INVALID_STATE`, etc.). 
Option-returning variants (`pop_option`, `peek_option`, `dequeue_option`) are provided alongside the Result variants throughout, 
eliminating the need for an out-parameter in the common case.

**Error handling:** Three roles, one chain:

`error.h` names what failed (error codes),
`result.h` propagates it (`Result<T, E>`), 
and `semantics/diag.h` records where and why (stack-allocated context chain, no allocation). 

Start with `result.h`. Add `diag.h` when call sites need context. `error.h` is always present implicitly through both. `Diag` is always passed by pointer and always optional — `NULL` is valid everywhere. 
`DIAG_PUSH` captures file/line/func automatically; `DIAG_PROPAGATE` handles the check-annotate-return pattern in one line.

All modules are header-only by default and require no runtime or build system integration.

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

## Known Limitations

- **File I/O:** Some utilities in `util/file.h` use `fseek` + `ftell` to determine file size.
  - ISO C does **not guarantee** this works for very large files, non-seekable streams (pipes, sockets), or certain exotic filesystems.
  - Typical binary files on common platforms are fine.
  - A future fallback using `fread` + `realloc` may be added for full portability.

- **Integer sizes:** Canon-C uses fixed-width types (u8, usize, etc.). While portable across common platforms, extremely unusual architectures may require manual adjustment.

- **Concurrency:** Canon-C modules do **not** provide internal thread-safety. Any shared data must be synchronized externally.

- **Large allocations:** Convenience modules that allocate dynamically (`dynvec`, `dynstring`) do not guard against out-of-memory situations beyond returning errors. Embedded or real-time code should prefer core modules.

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
