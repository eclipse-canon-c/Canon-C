# Canon-C Cheatsheet

> Quick reference manual for all Canon-C modules.

---

## Table of Contents

### `core/primitives/`
- [`bits.h`](#bitsh) — Portable bit manipulation
- [`checked.h`](#checkedh) — Overflow-safe arithmetic
- [`compare.h`](#compareh) — Comparator and predicate types
- [`contract.h`](#contracth) — Contracts and assertions
- [`lifetime.h`](#lifetimeh) — Lifetime token types shared across owning modules
- [`limits.h`](#limitsh) — Numeric limits and constants
- [`ptr.h`](#ptrh) — Named pointer arithmetic
- [`types.h`](#typesh) — Type aliases

### `core/`
- [`arena.h`](#arenah) — Linear bump allocator
- [`memory.h`](#memoryh) — Safe low-level memory operations
- [`ownership.h`](#ownershiph) — Ownership and borrowing annotations
- [`pool.h`](#poolh) — Fixed-size object pool allocator
- [`region.h`](#regionh) — Explicit lifetime region tokens
- [`scope.h`](#scopeh) — RAII-style deferred cleanup
- [`slice.h`](#sliceh) — Non-owning views into contiguous memory

### `semantics/option/`
- [`option.h`](#optionh) — Rust-style Option\<T\> for C
- [`option_decl.h`](#option_declh) — Declaration macros (separate compilation)
- [`option_defn.h`](#option_defnh) — Definition macros (implementation generation)
- [`option_impl.h`](#option_implh) — Pure implementation logic
- [`option_mangle.h`](#option_mangleh) — Name mangling conventions

### `semantics/result/`
- [`result.h`](#resulth) — Rust-style Result\<T, E\> for C
- [`result_decl.h`](#result_declh) — Declaration macros (separate compilation)
- [`result_defn.h`](#result_defnh) — Definition macros (implementation generation)
- [`result_impl.h`](#result_implh) — Pure implementation logic
- [`result_mangle.h`](#result_mangleh) — Name mangling conventions

### `semantics/`
- [`borrow.h`](#borrowh) — Non-owning view types with explicit borrowing intent
- [`diag.h`](#diagh) — Structured diagnostic frames for error context chains
- [`error.h`](#errorh) — Common error codes and Result\<T, Error\> helpers

### `data/convenience/`
- [`dynstring.h`](#dynstringh) — Auto-growing heap string builder
- [`dynvec.h`](#dynvech) — Auto-growing typed heap vector
- [`smallvec.h`](#smallvech) — Inline-first vector with at-most-one heap/arena spill

### `data/deque/`
- [`deque.h`](#dequeh) — Bounded double-ended queue (ring buffer)
- [`deque_decl.h`](#deque_declh) — Declaration macros (separate compilation)
- [`deque_defn.h`](#deque_defnh) — Definition macros (implementation generation)
- [`deque_impl.h`](#deque_implh) — Pure implementation logic
- [`deque_mangle.h`](#deque_mangleh) — Name mangling conventions

### `data/hashmap/`
- [`hashmap.h`](#hashmaph) — Generic Robin Hood open-addressed hashmap
- [`hashmap_decl.h`](#hashmap_declh) — Declaration macros (separate compilation)
- [`hashmap_defn.h`](#hashmap_defnh) — Definition macros (implementation generation)
- [`hashmap_impl.h`](#hashmap_implh) — Pure implementation logic
- [`hashmap_mangle.h`](#hashmap_mangleh) — Name mangling conventions
- [`hashmap_fmt.h`](#hashmap_fmth) — Optional: diagnostic string formatting
- [`hashmap_range.h`](#hashmap_rangeh) — Optional: bulk key/value collection

### `data/vec/`
- [`vec.h`](#vech) — Bounded typed vector with caller-owned buffer
- [`vec_decl.h`](#vec_declh) — Declaration macros (separate compilation)
- [`vec_defn.h`](#vec_defnh) — Definition macros (implementation generation)
- [`vec_impl.h`](#vec_implh) — Pure implementation logic
- [`vec_mangle.h`](#vec_mangleh) — Name mangling conventions
- [`vec_fmt.h`](#vec_fmth) — Optional: StringBuf formatting
- [`vec_range.h`](#vec_rangeh) — Optional: range-fill extension

### `data/`
- [`array.h`](#arrayh) — Fixed-size typed array with compile-time capacity
- [`bitset.h`](#bitseth) — Fixed-capacity bitset with O(1) bit ops
- [`priority_queue.h`](#priority_queueh) — Fixed-capacity binary heap (min/max)
- [`queue.h`](#queueh) — Bounded FIFO queue (thin wrapper over deque)
- [`range.h`](#rangeh) — Explicit bounded integer range generator
- [`stack.h`](#stackh) — Bounded LIFO stack (thin wrapper over vec)
- [`stringbuf.h`](#stringbufh) — Fixed-capacity incremental string builder

### `algo/`
- [`any_all.h`](#any_allh) — Existential and universal predicate testing
- [`filter.h`](#filterh) — Select elements matching a predicate
- [`find.h`](#findh) — Locate first element matching a predicate
- [`fold.h`](#foldh) — Functional reduction of sequences to single values
- [`map.h`](#maph) — Element-wise transformations over sequences
- [`reverse.h`](#reverseh) — In-place sequence reversal
- [`search.h`](#searchh) — Binary search utilities for sorted sequences
- [`sort.h`](#sorth) — Stable in-place hybrid insertion/merge sort
- [`unique.h`](#uniqueh) — Remove consecutive duplicate elements


---


Note: 

This cheatsheet is a practical, at-a-glance companion to Canon-C — not a replacement for the headers. 
The header files remain the authoritative source for exact signatures, full ACSL contracts, and precise semantics; 
this document exists for the things headers convey poorly: how the modules are actually used, and what to watch out for.
For each module you'll find:

Usage examples — concrete, runnable snippets showing how each primitive is called and what it returns, the kind of practical guidance a bare signature can't give.
Known limitations — the sharp edges, gotchas, and caveats discovered in practice (undefined-behavior conditions, macro evaluation quirks, width assumptions, and so on), surfaced here so you meet them before they bite.
A link to the corresponding header — so you can jump straight to the authoritative source for full contracts, the complete function set, and the verification details.

Use the cheatsheet to orient quickly and start using a module; follow the header link whenever you need the exact, complete, verified specification.


---
