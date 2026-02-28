# Canon-C

Canon-C — Treating C as an execution backend, not a semantic authority.  
A semantic standard library built from explicit, composable C modules.

---

## Table of Contents

1. [Motivation](#motivation)
2. [IMPORTANT](#important)
3. [Dependency Rule (Strict)](#dependency-rule-strict)
4. [Design Philosophy](#design-philosophy)
5. [Getting Started](#getting-started)
    - [1. core/primitives/ — Foundations](#1-coreprimitives--foundations)
    - [2. core/ — Core memory & ownership](#2-core-—-core-memory--ownership)
    - [3. semantics/ — Explicit semantic types](#3-semantics-—-explicit-semantic-types)
    - [4. data/ — Fixed-capacity collections](#4-data-—-fixed-capacity-collections)
    - [5. data/convenience/ — Ergonomic collections](#5-dataconvenience-—-ergonomic-collections)
    - [6. algo/ — Algorithms on collections](#6-algo-—-algorithms-on-collections)
    - [7. util/ — Utility modules](#7-util-—-utility-modules)
6. [Usage](#usage)

---

## Motivation

Over time, I found myself repeatedly re-implementing the same low-level patterns in C: arenas, error handling, vectors, parsing, file I/O, and more. Existing libraries either hide allocation, impose heavy frameworks, rely on implicit conventions, or lack consistency across modules. None matched my preference for explicit ownership, predictable allocation, header-only usage, and minimal hidden behavior.

In practice, this meant rewriting the same infrastructure across projects and re-learning the same informal rules every time.

Canon-C is an attempt to unify these patterns into a small, disciplined, and composable semantic standard library for C, governed by strict design principles. The goal is **not to add new functionality**, but to make program intent visible directly in APIs, so that ownership, lifetime, failure, and data flow are immediately clear at call sites.

C is fast, portable, and predictable, but its native semantics are low-level and mechanical. Writing non-trivial programs in C requires memorizing conventions and boilerplate around memory management, ownership rules, error handling, and iteration. These details obscure intent and significantly increase cognitive load.

Modern languages embed these abstractions directly into the language. While powerful, this often hides behavior, increases semantic complexity, and reduces transparency. Canon-C takes a different approach: **C itself remains unchanged. Meaning is added explicitly through libraries, not syntax.**

The result is a set of semantic building blocks that improve readability, preserve explicit control, maintain performance, and remain fully transparent.

The long-term goal is for this taxonomy of C abstractions to become a **shared standard vocabulary** — a common foundation that allows C programs to communicate intent clearly, consistently, and safely.

Canon-C is licensed under **MPL 2.0**.  

> Using Canon-C headers unmodified in your project (commercial or non-commercial) does not trigger MPL requirements — only modifications to Canon-C files themselves do.

---

## IMPORTANT

**Feedback requested on:**

1. Does this categorization make sense?
2. Are things in the right layers?
3. What’s missing or wrong?

**Not requested:**

- Debugging code
- Reviewing implementation quality
- Testing compilation (not yet)

**Note:**  

This project is an **architectural proposal and design foundation** built by one person. The taxonomy, dependency rules, and API designs are intentional and carefully considered. The implementations are **not yet fully tested** or compiled end-to-end. AI assistance was used for scaffolding, but architecture and philosophy are entirely human-designed.

---

## Dependency Rule (Strict)

Canonical layer order:

core/ → semantics/ → data/ → algo/ → util/

**Rules:**

- Modules are organized by **semantic depth**, not feature count. Lower layers define unavoidable mechanics; higher layers build meaning on top.
- Lower layers may be used by higher layers, but **upward or circular dependencies are strictly forbidden**.
- Each module must be independently usable.
  
This ensures **explicitness** and prevents hidden behaviors or fragile dependencies.

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
- Works in **plain C99**, with optional GNU C extensions for macros (disable with `#define CANON_NO_GNU_EXTENSIONS`)
- No clever tricks

> If behavior cannot be understood by reading the header, it does not belong.

Abstractions must **clarify behavior, not conceal it**.

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
- Min/max/clamp helpers
- Explicit contracts and assertions (`require_msg`, `ensure_msg`)

**Goal:** Understand primitive building blocks of memory, arithmetic, and pointer safety.

---

### 2. `core/` — Core memory & ownership

Builds on primitives for **fundamental memory management**:

- Linear bump allocator (`arena.h`)
- Fixed-size object pools (`pool.h`)
- Slice views (`slice.h`) and region-based lifetimes (`region.h`)
- RAII-style deferred cleanup (`scope.h`)
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

Generic, reusable algorithms:

- Transformation (`map.h`), filtering, folding, searching
- Sorting, reversing, uniqueness
- Predicate checks (`any_all.h`)

**Goal:** Apply operations to collections **predictably and generically**.

---

### 7. `util/` — Utility modules

Convenience utilities for **common tasks**:

- Strings: `string.h`, `split.h`, `join.h`, `view.h`, `intern.h`
- Logging: `log.h`, `log_macros.h`
- File I/O, parsing, random number generation, timing

**Goal:** Provide ergonomic helpers while relying on **lower layers**.

---

## Usage

Canon-C is **header-only**. To use:

```c
#include "Canon-c/core/arena.h"
#include "Canon-c/semantics/option/option.h"
``` 
Then you’re ready to go. No runtime or build system integration required.
