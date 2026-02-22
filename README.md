# Canon-C

Canon-C — Treating C as an execution backend, not a semantic authority.
A semantic standard library built from explicit, composable C modules.

---

## Motivation

Over time, I kept rewriting the same low-level patterns in C — arenas, error handling, vectors, parsing, file I/O, and many others.
Existing libraries either hid allocation, imposed frameworks, relied on implicit conventions, or lacked consistency across modules. None matched my preferences for 
explicit ownership, predictable allocation, header-only usage, and minimal hidden behavior.

In practice, this meant repeatedly reimplementing the same infrastructure and re-learning the same rules across projects.

Canon-C is an attempt to unify these patterns into a small, disciplined, and composable set of C modules with strict design rules. 
The goal is not to add new functionality, but to make program intent visible directly from APIs, so that ownership, lifetime, failure, and data flow are obvious at call sites.

C is fast, portable, and predictable, but its native semantics are low-level and mechanical.
Writing non-trivial programs in C often requires memorizing patterns, conventions, and boilerplate around memory management, ownership rules, error handling, and iteration. These details obscure intent and increase cognitive load.

There is no standard taxonomy of most used libraries in C.

Many modern languages embed abstractions directly into the language to address this. While powerful, 
this also increases semantic complexity and hides behavior. Canon-C takes a different approach: C itself is left untouched. Meaning is added through explicit libraries, not syntax.

The result is a set of semantic building blocks that improve readability, maintain explicit control, preserve performance, and remain fully transparent.

I want this "taxonomy" to become a standard that everyone uses. 

Canon-C is licensed under **MPL 2.0** as the primary license. 

---

## IMPORTANT !!!

**I'm seeking feedback on:**
1. Does this categorization make sense?
2. Are things in the right layers?
3. What's missing or wrong?

**I'm NOT asking you to:**
- Debug the code
- Review implementation quality
- Test compilation (not yet)

If this taxonomy makes sense to you, help me:
1. Critique the structure
2. Suggest better categorization
3. Eventually: implement production-quality versions
   
---

### Dependency Rule (Strict)

core/ → semantics/ → data/ → algo/ → util/

Modules are organized by **semantic depth**, not by feature count. Lower layers
define unavoidable mechanics; higher layers build meaning on top of them.

- Lower (lowest is core here) layers may be used by higher (highest is util here) layers.  
- Upward or circular dependencies are strictly forbidden.  
- Each module must be independently usable.  

This rule ensures **explicitness** and prevents hidden behaviors or fragile dependencies.


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
- Works in plain C99 with no special compiler extensions
- No clever tricks

If behavior cannot be understood by reading the header, it does not belong.

Abstractions must clarify behavior, not conceal it.

---

## Included Modules (Current)

## core/primitives/
- `types.h` — portable integer and size type aliases (u8, usize, isize, etc.)
- `compare.h` — Three-way comparator type and built-in comparators
- `limits.h` — common constants and limits (integer bounds, alignment, capacity limits)
- `bits.h` — Portable bit manipulation (popcount, clz/ctz, rotate, power-of-2, extract/insert, byte swap
- `checked.h` — Overflow-safe arithmetic (checked add/sub/mul), alignment helpers, and min/max/clamp utilities
- `contract.h` — explicit contracts and assertions (require_msg, ensure_msg, unreachable, panic)
- `ptr.h` — named pointer arithmetic and alignment (ptr_offset, ptr_diff, align_up, ptr_in_range, PTR_CONTAINER_OF)

## core/
- `memory.h` — low-level memory utilities (alignment, safe mem_copy/mem_move wrappers)
- `region.h` — explicit lifetime boundaries for borrowed values
- `arena.h` — Linear bump allocator with explicit lifetime control
- `pool.h` — fixed-size object pool allocator (arena-backed)
- `scope.h` — RAII-style deferred cleanup macros
- `slice.h` — non-owning views into contiguous memory (bytes_t, cbytes_t, str_t, DEFINE_SLICE)
- `ownership.h` — explicit ownership and borrowing annotations (owned, borrowed, moved, dropped, DEFINE_OWNED, CANON_DROP)

## data/
- **`vec/`** — bounded typed vectors with explicit caller-owned buffers (caller-owned buffer, fixed capacity) — modular 7-file architecture
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
- **`hashmap/`** — generic Robin Hood open-addressed hashmap (caller-owned buffer, fixed capacity) — modular 7-file architecture
  - `hashmap.h` — user-facing API; header-only entry point
  - `hashmap_impl.h` — pure implementation logic (zero naming assumptions)
  - `hashmap_mangle.h` — name mangling conventions (individually overridable)
  - `hashmap_decl.h` — forward declarations for separate compilation
  - `hashmap_defn.h` — complete definitions (header-only or .c files)
  - `hashmap_range.h` — optional extension: `collect_keys()`, `collect_values()`, `HASHMAP_FOR_EACH` (depends on `vec.h`)
  - `hashmap_fmt.h` — optional extension: `to_stringbuf()` (depends on `stringbuf.h`)
- `array.h` — fixed-size typed array with compile-time capacity (DEFINE_ARRAY, safe indexing, slice/bytes views, iteration macros)
- `priority_queue.h` — fixed-capacity binary heap (priority queue) with caller-owned buffer
- `queue.h` — FIFO queue wrapper over deque (fixed capacity); includes `DECLARE_QUEUE`
- `stack.h` — LIFO stack wrapper over vec (fixed capacity); includes `DECLARE_STACK`
- `range.h` — explicit integer range generator (ascending/descending, isize-based, overflow-safe)
- `stringbuf.h` — incremental string builder (arena- or buffer-backed, fixed capacity)
- `bitset.h` — fixed-capacity bitset with O(1) set/clear/test and O(n/64) bulk ops

## data/convenience/
- `dynvec.h` — auto-growing typed vector (hidden heap allocation, 2x growth)
- `smallvec.h` — inline-first vector with at-most-one spill to heap or arena
- `dynstring.h` — auto-growing string builder (hidden heap allocation, 2x growth)

## semantics/
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

## algo/
- `map.h` — element-wise transformation (supports different input/output types)
- `filter.h` — select elements matching predicate
- `fold.h` — reduce sequence to single value (infallible & fallible variants)
- `find.h` — locate first matching element
- `any_all.h` — predicate checks (any / all)
- `sort.h` — generic in-place sorting (with comparator)
- `search.h` — binary search utilities (lower_bound, exact match)
- `unique.h` — remove consecutive duplicates (in-place)
- `reverse.h` — reverse sequence in-place

## util/str/
- `string.h` — safe string operations (copy, concat, predicates)
- `split.h` — non-mutating string splitting (borrowed views)
- `join.h` — safe string joining (buffer-based & allocating)
- `view.h` — minimal immutable borrowed string view (pointer + length)
- `intern.h` — minimal string interning — returns shared immutable str_view_t

## util/log/
- `log.h` — minimal, explicit logging with Result-based error handling
- `log_macros.h` — ergonomic, safe logging macros layered on log.h

## util/
- `file.h` — safe file I/O (read/write whole files, arena-backed preferred)
- `parse.h` — robust parsing of integers, unsigned, and floating-point values
- `time.h` — high-resolution stopwatch (monotonic timing)
- `random.h` — fast, explicit PRNG (PCG32, no global state)

---

**Note:** Modules in `*/convenience/` subdirectories trade explicitness for ergonomics — hidden allocations, automatic growth, fewer required parameters. 

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

- **Integer sizes:** Canon-C uses fixed-width types (u8, usize, etc.). While portable across common platforms, extremely unusual architectures may require manual adjustment.

- **Concurrency:** Canon-C modules do **not** provide internal thread-safety. Any shared data must be synchronized externally.

- **Large allocations:** Convenience modules that allocate dynamically (`dynvec`, `dynstring`) do not guard against out-of-memory situations beyond returning errors. Embedded or real-time code should prefer core modules.

---






