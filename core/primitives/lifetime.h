/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/

#ifndef CANON_CORE_PRIMITIVES_LIFETIME_H
#define CANON_CORE_PRIMITIVES_LIFETIME_H

#include <stdbool.h>
#include "core/primitives/types.h"

/**
 * @file core/primitives/lifetime.h
 * @brief Lifetime token types shared across owning modules
 *
 * This header is the canonical home of three small types used by every
 * Canon-C module that participates in lifetime tracking:
 *
 *   region_id_t       — 64-bit identifier for a lifetime
 *   REGION_ID_STATIC  — reserved ID for static / never-expiring borrows
 *   lifetime_t        — (id, open) pair embedded by owning modules
 *
 * Why this lives in primitives/:
 * ────────────────────────────────────────────────────────────────────────────
 * Multiple owning modules — core/arena.h, core/region.h, and (in later
 * phases) data/convenience/dynvec.h, data/convenience/dynstring.h,
 * data/convenience/smallvec.h, data/vec/, data/deque/, data/hashmap/,
 * data/stringbuf.h, data/priority_queue.h — all need to embed the same
 * (id, open) pair so that borrows can validate against any source via a
 * single const lifetime_t* pointer.
 *
 * The types must therefore have a single canonical home that all of
 * those modules can include without forming a cycle. core/primitives/
 * is the right home: it sits at the bottom of the dependency rule and
 * has no upward dependencies, so any module can include it freely.
 *
 * Why not in core/region.h:
 * ────────────────────────────────────────────────────────────────────────────
 * region.h includes core/arena.h (to reference Arena*). If arena.h had
 * to include region.h for the types, the two headers would form an
 * include cycle — region.h pulls in arena.h, arena.h pulls in region.h,
 * and whichever is parsed first sees the other half-defined. Putting
 * the shared types in primitives/ breaks the cycle: both arena.h and
 * region.h include this header without involving each other.
 *
 * What this header is NOT:
 * ────────────────────────────────────────────────────────────────────────────
 * - Not a lifetime-tracking runtime. The runtime check lives in
 *   core/region.h as lifetime_assert_valid() because that's where
 *   ensure_msg is available.
 * - Not gated by CANON_LIFETIME_DEBUG. The types are always defined,
 *   because Region uses region_id_t unconditionally (lifetime IDs are
 *   a release-build feature of Region, not a debug-only one). Modules
 *   that only need the types under CANON_LIFETIME_DEBUG can still
 *   include this header unconditionally — the types are tiny and cost
 *   nothing if unused.
 * - Not extensible. lifetime_t is exactly two fields. Anything richer
 *   (generation counters, per-thread tracking, dependency graphs)
 *   would belong in a different module, not by extending this one.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * This header is in core/primitives/ and includes only <stdbool.h> and
 * core/primitives/types.h. Nothing else may be included from here.
 *
 * @sa core/region.h         — Region type and lifetime_assert_valid()
 * @sa core/arena.h          — Arena embeds lifetime_t under CANON_LIFETIME_DEBUG
 * @sa semantics/borrow.h    — borrow types that capture and validate IDs
 */

/**
 * @brief Unique identifier for a lifetime
 *
 * For Region, derived from the Region's stack address in region_begin().
 * For Arena (and other modules under CANON_LIFETIME_DEBUG), derived from
 * the owner's address and re-stamped on every reset.
 *
 * Unique across all simultaneously live owners.
 * Not monotonic — addresses, not sequence numbers.
 *
 * 0 is reserved as REGION_ID_STATIC ("no owner" / static lifetime).
 * A valid owner will never have ID 0: no stack or heap object has
 * address 0 on any conforming C99 implementation.
 */
typedef u64 region_id_t;

/**
 * @brief Reserved ID — "no owner" / static lifetime.
 *
 * Borrows with this ID never expire and never fire lifetime assertions.
 * Used for borrows over static const data, string literals, and any
 * value whose lifetime exceeds every possible owner.
 */
#define REGION_ID_STATIC ((region_id_t)0)

/**
 * @brief The (id, open) pair embedded by ownership-bearing modules
 *
 * Modules embedding this:
 *   - core/arena.h         (under CANON_LIFETIME_DEBUG)
 *   - core/pool.h          (planned, Phase 3)
 *   - data/convenience     (planned, Phase 2)
 *   - data/vec, deque, hashmap, stringbuf, priority_queue (planned, Phase 3)
 *
 * Region does NOT embed lifetime_t — it has separate id/open fields by
 * historical accident. lifetime_assert_valid() takes (id, open) values
 * directly, so Region's separate fields and other modules' embedded
 * lifetime_t are interoperable at the assertion site.
 *
 * Borrows that need to validate against a source store a
 * const lifetime_t* pointing at the source's lt field plus a captured
 * id, then call lifetime_assert_valid() on read.
 *
 * The struct is a layout convention, not a clever abstraction. It exists
 * so borrows can hold a single pointer regardless of which owning struct
 * they came from.
 */
typedef struct {
    region_id_t id;
    bool        open;
} lifetime_t;

#endif /* CANON_CORE_PRIMITIVES_LIFETIME_H */
