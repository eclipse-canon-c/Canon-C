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
![License: MPL-2.0](https://img.shields.io/badge/License-MPL--2.0-yellow?logo=mozilla&logoColor=white)

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

<!-- Formal Verification -->
### Formal Verification
![Frama-C](https://img.shields.io/badge/Frama--C-Configurable-lightgrey)
![MC/DC](https://img.shields.io/badge/MC--DC-Configurable-lightgrey)

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
7. [Usage](#usage)

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

This project is an **architectural proposal and design foundation** built by a team. The taxonomy, dependency rules, and API designs are intentional and carefully considered. The implementations are **not yet fully tested** or compiled end-to-end. AI assistance was used for scaffolding, but architecture and philosophy are entirely human-designed.

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

- Strings: `string.h`, `str_split.h`, `str_join.h`, `str_view.h`, `intern.h`
- Logging: `log.h`, `log_macros.h`
- File I/O, parsing, random number generation, timing

**Goal:** Provide ergonomic helpers while relying on **lower layers**.

## Scope

Canon-C covers exactly what is needed to write explicit, ownership-aware C programs
without reaching for a larger framework. Every module exists because the pattern
it encodes recurs across real programs and has no good idiomatic C equivalent.

Nothing was added speculatively. The cutoff rule is simple: if a module can be
built cleanly from existing layers using Canon-C's own primitives, it doesn't
need to live here — the user writes it. If the absence of a module forces every
downstream user to re-invent the same unsafe boilerplate, it belongs.

Canon-C stops at the boundary where general-purpose utility ends and
application-specific concerns begin. Concretely: no networking, no threading,
no serialization, no OS abstractions. These domains are large, platform-specific,
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

**Threading / concurrency**
- `pthreads` — POSIX standard threading API. Available natively on Linux and macOS.
  No external dependency needed on Unix-like systems.
- `TinyCThread` — portable implementation of the C11 threads API in two files.
  No external dependencies. Use when you need cross-platform threading including
  Windows without pulling in a larger framework.
- `C11 <threads.h>` — if your compiler fully supports C11, the standard threading
  API is available directly with no library needed.

**Serialization**
- `cJSON` — ultralightweight JSON parser and emitter in ANSI C. Single file, MIT
  license. Widely used in embedded and systems projects.
- `yyjson` — fastest JSON library in C. Use when performance matters and cJSON
  is the bottleneck.
- `mpack` — MessagePack for C. Use when binary serialization is needed over JSON:
  smaller payloads, faster parsing, schema-driven.
- `miniz` — single-file deflate/inflate and ZIP compression. Use for compressing
  serialized data or reading and writing ZIP archives.

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
  with minimal overhead. Canon-C's lower layers (no stdio, no malloc) are
  designed to compose cleanly in this environment.
- `Zephyr` — full-featured, scalable RTOS managed by the Linux Foundation.
  Built-in drivers, networking, Bluetooth, and security. Use when your project
  needs to scale across hardware revisions, run multiple subsystems, or be
  maintained long-term. Higher learning curve than FreeRTOS but significantly
  more structure.

**WebAssembly**
- `Emscripten` — compiles C and C++ to WebAssembly using LLVM and Clang as a
  drop-in replacement for gcc/clang. Output runs in browsers, Node.js, and
  standalone Wasm runtimes. Canon-C's C99 code compiles cleanly through
  Emscripten without modification.

---

> Note: most of these libraries use traditional C API conventions — raw pointers,
> integer error codes, implicit ownership, and occasional global state. Wrapping
> their interfaces in Canon-C's Result, owned, and borrowed types at your
> integration boundary is recommended to keep the rest of your codebase consistent.


---

## Usage

Canon-C is **header-only**. To use:

```c
#include "Canon-c/core/arena.h"
#include "Canon-c/semantics/option/option.h"
``` 
Then you’re ready to go. No runtime or build system integration required.
