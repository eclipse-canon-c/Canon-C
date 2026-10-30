# Thread safety

Canon-C is a **single-threaded library**. It owns no threads, spawns none,
and contains no locks. This document states what that means for a caller
who has threads of their own, because "no locks" is a design fact and not
a contract — the contract is below.

Scope: this is a *documentation* record, not an evidence record. Nothing
here is machine-checked. Frama-C/WP has no concurrency model: every
obligation the project discharges assumes sequential semantics, so no
statement in this file is backed by the proof stream. Where a claim is
enforced by something, that is named explicitly.

---

## 1. The short version

| | contract |
|---|---|
| **Every object** (`Arena`, `Pool`, `Region`, `vec`, `deque`, `hashmap`, `StringBuf`, …) | **Single-owner.** One thread at a time. No internal locking. Handing an object to another thread requires a happens-before edge the library does not provide. |
| **Concurrent reads** of an object no thread is writing | Safe. Every accessor is `const`-correct and touches no shared state. |
| **`canon_contract_handler`** (the only global) | **Configure before threads.** `contract_set_handler()` is not thread-safe; call it once during init. The handler itself must be thread-safe and, if it can fire from a signal context, async-signal-safe. |
| **Lifetime tokens** under `CANON_LIFETIME_DEBUG` | **See §3 — the one place where a race is worse than UB.** |

Everything else in the library is caller-owned. There is no hidden
allocation, no thread-local state, no lazily-initialised singleton, and no
registry. A grep for file-scope mutable objects across `core`,
`semantics`, `data`, `algo` and `util` returns exactly one name:
`canon_contract_handler`.

---

## 2. The one global: the contract handler

`core/primitives/contract.h` declares

```c
extern contract_handler_fn canon_contract_handler;
```

with the definition emitted by whichever translation unit defines
`CANON_CONTRACT_IMPL`. It is a plain `extern` variable, deliberately: it is
read on every contract violation from every module, and an indirection or a
lock on that path would be paid by code that never fires.

The contract:

- **`contract_set_handler()` is not thread-safe.** Call it once during
  program initialisation, before spawning threads. Swapping the handler
  while other threads may be executing contract checks is a data race.
- **The handler must be thread-safe** if contract checks can fire on more
  than one thread. The default handler uses `fprintf`, which is not
  guaranteed thread-safe on all platforms.
- Reading the pointer on the check path is a benign read *provided* nobody
  writes it after threads start. That proviso is the whole contract.

This was already documented in `contract.h` (see its notes at the
`contract_set_handler` declaration); it is restated here so the library has
one place to look.

---

## 3. Lifetime tokens under `CANON_LIFETIME_DEBUG`

**This section exists because the failure mode is not "undefined
behaviour" in the abstract — it is a safety check that silently stops
working.**

### What the mode does

`CANON_LIFETIME=debug` defines `CANON_LIFETIME_DEBUG`, which embeds a
`lifetime_t {id, open}` in ownership-bearing types and stamps a token into
`id` on construction and re-stamps it on reset. Borrows capture the token;
`lifetime_assert_valid()` compares the captured token against the owner's
current one, so a borrow outliving its owner's reset is caught at runtime.
The mode is a **safety check**, and it is exercised by a dedicated CI job
across the platform/compiler matrix with ASan and UBSan enabled.

### The defect (fixed 2026-08)

Token generation is `counter ^ owner-address`, where `counter` has static
storage duration. Until 2026-08 each of the **eleven** ownership-bearing
modules carried its own byte-identical copy of the generator, each doing a
plain non-atomic read-modify-write:

```c
const region_id_t c_ = counter_;
counter_++;                       /* data race */
```

Two threads constructing owners concurrently can read the same `counter_`.
Because the token is `counter ^ address`, two owners then receive the
**same token** whenever the colliding counter values coincide with the
address term — and the token is exactly the value the borrow checks
compare. A stale borrow can therefore validate against a different live
owner: **the check passes when it should fail.** The mode fails silently,
and in the unsafe direction.

Measured on a single-core machine, 32 threads, 1.6 M tokens, fixed owner
address so the address term cannot mask a counter collision:

| build | atomics | duplicate tokens |
|---|---|---|
| C99 `-O0` | no | 0 of 1,600,000 |
| C99 `-O2` | no | **150,000 of 1,600,000 (9.4 %)** |
| C11 `-O2` | yes | 0 of 1,600,000 |

ThreadSanitizer reports the race directly on the non-atomic path. Note the
shape of the `-O0` row: the race does not reproduce at all without
optimisation, because the optimiser is what keeps the counter in a register
across the loop. A debug-only safety mechanism that is intact in a debug
build and 9 % corrupt in an optimised one is the worst available failure
profile, and it is why this is written up rather than quietly patched.

### What was changed

1. The eleven private generators were replaced by **one** implementation,
   `canon_lifetime_next_id_()` in `core/primitives/lifetime.h`. The module
   helpers remain as named call sites, so no call site moved.

2. That implementation selects an atomic increment through a four-level
   ladder:

   | level | mechanism | `CANON_LIFETIME_ATOMIC_IDS` |
   |-------|-----------|------|
   | 1 | C11 `<stdatomic.h>`, `atomic_fetch_add_explicit(..., memory_order_relaxed)` | 1 |
   | 2 | GCC/Clang `__atomic_fetch_add(..., __ATOMIC_RELAXED)` — MISRA-DEV-018 | 1 |
   | 3 | MSVC `_InterlockedIncrement64()` — MISRA-DEV-018 | 1 |
   | 4 | plain increment — **not race-free** | 0 |

   **The ladder has four rungs rather than one for a reason worth stating.**
   A C11-only fix was written first and was wrong: Canon-C is C99
   (`CMAKE_C_STANDARD 99`, `REQUIRED ON`), so level 1 was unreachable in
   every CI job and for every caller following the project's own language
   stance. The fix would have been correct and never compiled. Levels 2 and
   3 are what make it reach the shipped configuration.

   The CI job for this property, `lifetime-token-concurrency`, runs the
   concurrency test at `-std=gnu11` as well as C99, so level 1 runs under
   concurrent load there. Since VERIFY-044 the `c-standards` job also
   builds the whole suite as C11, C17 and C23, so every header compiles
   with level 1 selected and `lifetime_test`, the sequential checks, runs
   on it. The other build jobs are C99: level 2 with GCC and clang,
   level 3 with MSVC, and level 4 in the CompCert job (below).

   Levels 2 and 3 are compiler extensions, so `CANON_NO_GNU_EXTENSIONS`
   drops to level 4 — which is how the CompCert job and any strict-C99 build
   reach the conforming path. `CANON_LIFETIME_NO_ATOMICS` forces level 4
   explicitly, which is how CI demonstrates that the fallback really does
   race.

   `memory_order_relaxed` (and its intrinsic equivalents) is deliberate and
   sufficient: the only property required is that concurrent increments
   return distinct values, which read-modify-write operations on a single
   object guarantee under any ordering. No happens-before edge is claimed —
   publishing an owner to another thread remains the caller's
   synchronisation, exactly as it is for every other Canon-C object.

3. Two of the eleven copies carried the ACSL contract `assigns \nothing`,
   which is **false** — the function writes the counter. No contract
   replaces it. The block is excluded from every verified configuration, so
   an annotation there would be unchecked decoration; that the falsehood
   survived unnoticed is the argument. See §5.

### The residual contract

On level 4 — a toolchain that is neither C11 nor GCC/Clang/MSVC, or a build
defining `CANON_NO_GNU_EXTENSIONS` or `CANON_LIFETIME_NO_ATOMICS` —
**concurrent construction of owners under `CANON_LIFETIME_DEBUG` is a data
race and can produce duplicate tokens.** Callers in that configuration must
serialise construction. `CANON_LIFETIME_ATOMIC_IDS` reports which path is in
effect.

Two further properties hold on **every** level and are not defects:

- The counter has internal linkage in a header, so there is **one instance
  per translation unit**, not one per program. Owners constructed in
  different TUs can draw the same counter value. Token distinctness rests
  principally on the address term, which is sound for simultaneously live
  owners because distinct live objects have distinct addresses. The counter
  is a diversifier, not the identity.
- Tokens are **not** unique across time. An address may be reused after an
  owner dies; the counter term makes reuse unlikely rather than impossible.

### What is machine-checked, and what stays prose (VERIFY-021)

As of 2026-09-02 **ladder level 4 is machine-checked**. `frama-c-lifetime`
runs WP with `CANON_LIFETIME_DEBUG` + `CANON_LIFETIME_NO_ATOMICS`, and
`canon_lifetime_next_id_` provably never returns `REGION_ID_STATIC` on that
path. Handing out 0 would mark the owner as static-lifetime, so borrows over
it would stop expiring: the instrument would fail OPEN. That is the property
worth proving about a generator, and it is now proved.

**Levels 1–3 carry no machine-checked claim, and are excluded permanently.**
WP has no concurrency model for `atomic_fetch_add_explicit`,
`__atomic_fetch_add` or `_InterlockedIncrement64`. The contract in
`lifetime.h` is `#if`-gated on `CANON_LIFETIME_ATOMIC_LEVEL_ == 4` so it
cannot silently acquire authority over the other three. Everything this
document says about levels 1–3 remains a prose argument, unchanged in status
by VERIFY-021.

Note the asymmetry plainly, because it is easy to quote the wrong way round:
**the verified path is the only one that is NOT race-free.** Level 4 is the
fallback with the documented data race above. Verification followed the
tool's reach, not the risk. A reader who takes "lifetime.h is verified" to
mean the threaded paths are covered has it exactly backwards.

Two things VERIFY-021 does NOT claim:

- **No frame.** The contract has no `assigns` clause, because one cannot be
  written: `counter_` is a function-local static and is not in scope where a
  function contract is parsed. `assigns \nothing` would be FALSE — it is the
  historical defect, carried by two of the eleven superseded copies — and
  hoisting the variable would verify a different program than the one that
  ships. WP therefore assumes `\everything` and emits a
  `[wp:pedantic-assigns]` warning, which is retained rather than suppressed.
- **Not distinctness.** Token distinctness is a property over a SEQUENCE of
  calls; a WP contract speaks about one. It is not merely unproved but false
  in the strict sense — the `REGION_ID_STATIC` guard maps the xor-cancelling
  case onto 1, which is also produced directly, so two distinct owners can
  receive id 1. The rate is negligible and this is not a defect, but the two
  bullets above about per-TU counters and reuse across time are the operative
  statement, not an aspiration toward uniqueness.

### What CI checks, and what it deliberately does not

`test/concurrency/lifetime_token_test.c` is the library's only concurrent
test — necessarily, since the library owns no threads elsewhere. The
`lifetime-token-concurrency` job runs it across the ladder.

**Gated**: on an atomic path, zero duplicate tokens. Atomicity either holds
or it does not, so the direction is deterministic and safe to fail a build
on. Also gated: TSan reports **zero** races on the atomic path.

**Reported, never gated**: the duplicate count on the fallback path. A data
race is undefined behaviour, not a scheduled event. The same counter
produced 50000, 50000, 0, 50000, 50000 duplicates across five consecutive
runs on one machine; a job requiring the race to appear would fail about one
run in five and would in effect be asserting that undefined behaviour is
reliable. ThreadSanitizer instruments accesses rather than sampling
outcomes, so it gives a deterministic two-way signal (0 races on the atomic
path, 2 on the fallback) and is what the job uses for that direction.

---

## 4. Per-module notes

- **`arena.h`, `pool.h`, `region.h`** — no internal state beyond the caller's
  object. `region_end()` invokes registered hook functions; if a hook
  touches state shared with other threads, that obligation is the *hook
  author's*, not the library's.
- **Containers** (`vec`, `deque`, `hashmap`, `priority_queue`, `bitset`,
  `stringbuf`, `array`, `stack`, `queue`, `range`) — caller-owned buffers,
  no internal locking, single-owner.
- **`diag.h`** — the `Diag` context is caller-owned, not global. A single
  `Diag` pushed and popped from two threads is a race; give each thread its
  own.
- **`option`, `result`, `borrow`** — by-value types with no shared state.
  Their panic paths route through the contract handler (§2).
- **`util/log`, `util/parse`, `util/file`, `util/time`, `util/random`** —
  not verified, and not audited for thread safety here. Treat as
  single-owner and read the source before sharing across threads. `random.h`
  in particular carries caller-owned state that is not safe to share.

---

## 5. What is *not* claimed

- **No proof covers any statement in this document.** WP has no concurrency
  model. The sequential library's residual sets, roll-calls and pins say
  nothing about concurrent execution, and no gate in CI checks any claim
  here.
- **TSan covers exactly one thing.** The `lifetime-token-concurrency` job
  runs ThreadSanitizer over `test/concurrency/lifetime_token_test.c` — token
  generation, and nothing else. The rest of the suite is sequential, so TSan
  over it would instrument no concurrent accesses and prove nothing. That
  job runs on its own build, since TSan and ASan cannot share a binary.
  Broader TSan coverage arrives with the concurrency tier, when there is
  something for it to observe.
- **This is a single-threaded library.** The forthcoming concurrency work
  is a separate tier with its own repository, its own test suite and its
  own evidence stream, scoped so that the deductive story stays honest:
  runtime-verified first, with documented invariants, pinned baselines and
  an explicit record of what is and is not formally covered.

---

## 6. Change record

| date | change |
|---|---|
| 2026-08 | Eleven private lifetime-token generators unified into `canon_lifetime_next_id_()`; false `assigns \nothing` contracts removed; race measured and documented; this file created. |
| 2026-08 | First fix was C11-only and therefore unreachable in a C99 project — corrected to the four-level ladder (MISRA-DEV-018) so the atomic path reaches the shipped configuration. Added `test/concurrency/lifetime_token_test.c` and the `lifetime-token-concurrency` CI job. |
