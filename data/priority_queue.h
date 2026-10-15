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

#ifndef CANON_DATA_PRIORITY_QUEUE_H
#define CANON_DATA_PRIORITY_QUEUE_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/ptr.h"
#include "core/primitives/compare.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"
#include "semantics/error.h"
#include "semantics/option/option.h"
#include "semantics/result/result.h"

#ifdef CANON_LIFETIME_DEBUG
    #include <stdint.h>                       /* uintptr_t */
    #include "core/primitives/lifetime.h"     /* region_id_t, lifetime_t */
#endif

/*
 * Instantiate the Result type used by fallible operations.
 *
 * NOTE: CANON_RESULT(bool, Error) token-pastes to result__Bool_Error
 * (not result_bool_Error) because bool expands to _Bool before ## in C99.
 * All function signatures and call sites use result__Bool_Error accordingly.
 *
 * Guarded against multi-TU collision when vec/deque/priority_queue/hashmap
 * are all included in the same translation unit (see test/semantics/borrow_test.c).
 * Matches the pattern already used in vec_impl.h and deque_impl.h.
 */
#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
    #define CANON_RESULT_BOOL_ERROR_DEFINED
    #ifdef __FRAMAC__
        /* VERIFY-022 commit 5. Under WP the three constructors this header
         * calls carry contracts, interposed exactly as vmacros/vdrivers/
         * vec_verify.h does: type first, contracted prototypes, then the
         * real function definitions. Without this, result__Bool_Error_ok /
         * _err had NO assigns clause, WP took them as assigning everything,
         * and every push_result / remove_at_result postcondition died at the
         * `return` -- 6 goals at CI #1285 that commit 4 had misattributed to
         * struct/buffer aliasing. Shapes copied from vec_verify.h; nothing
         * invented. The shipped path (#else) is byte-identical to before. */
        DEFINE_RESULT_TYPEDEF(bool, Error)
        /* cppcheck-suppress misra-c2012-19.2 ; MISRA-DEV-014 */
        DEFINE_RESULT_STRUCT(bool, Error)
        /*@ assigns \nothing;
            ensures \result.is_ok == \true;
            ensures \result.val.ok == v;
        */
        static inline result__Bool_Error result__Bool_Error_ok(bool v);
        /*@ assigns \nothing;
            ensures \result.is_ok == \false;
            ensures \result.val.err == err;
        */
        static inline result__Bool_Error result__Bool_Error_err(Error err);
        /*@ assigns \nothing;
            ensures \result <==> r.is_ok;
        */
        static inline bool result__Bool_Error_is_ok(result__Bool_Error r);
        /* VERIFY-039 G12. get_ok and get_err store through out, and without
         * a contract nothing required out valid: their two run-time checks
         * were false for a null out with the matching tag, the pair
         * VERIFY-018 F4 closed at vec. Shapes copied from
         * vmacros/vdrivers/vec_verify.h. */
        /*@ requires \valid(out);
            assigns *out;
            behavior ok:
              assumes r.is_ok;
              assigns *out;
              ensures \result == \true;
              ensures *out == r.val.ok;
            behavior err:
              assumes !r.is_ok;
              assigns \nothing;
              ensures \result == \false;
              ensures *out == \old(*out);
            complete behaviors;
            disjoint behaviors;
        */
        static inline bool result__Bool_Error_get_ok(result__Bool_Error r, bool *out);
        /*@ requires \valid(out);
            assigns *out;
            behavior err:
              assumes !r.is_ok;
              assigns *out;
              ensures \result == \true;
              ensures *out == r.val.err;
            behavior ok:
              assumes r.is_ok;
              assigns \nothing;
              ensures \result == \false;
              ensures *out == \old(*out);
            complete behaviors;
            disjoint behaviors;
        */
        static inline bool result__Bool_Error_get_err(result__Bool_Error r, Error *out);
        /* VERIFY-039 G11. The five combinators had no contract. With C
         * the condition on which the body calls the pointer, each now
         * requires `C ==> \valid_function(f)`, claims `terminates !C;
         * exits C;`, and states the empty frame only in the behaviours
         * that call nothing. The queue calls none of them. WP 29 does not
         * implement `\valid_function`, so the run-time check on each
         * pointer stays unproved. Shapes copied from
         * vmacros/vdrivers/result_verify.h. */
        /*@ requires r.is_ok ==> \valid_function(f);
            terminates !r.is_ok;
            exits r.is_ok;
            behavior err:
              assumes !r.is_ok;
              assigns \nothing;
              ensures \result.is_ok == \false;
              ensures \result.val.err == r.val.err;
            behavior ok:
              assumes r.is_ok;
              ensures \result.is_ok == \true;
              // \result.val.ok == f(r.val.ok): not stated, the callback's value
            complete behaviors;
            disjoint behaviors;
        */
        static inline result__Bool_Error result__Bool_Error_map(result__Bool_Error r,
                                                                bool (*f)(bool));
        /*@ requires !r.is_ok ==> \valid_function(f);
            terminates r.is_ok;
            exits !r.is_ok;
            behavior ok:
              assumes r.is_ok;
              assigns \nothing;
              ensures \result.is_ok == \true;
              ensures \result.val.ok == r.val.ok;
            behavior err:
              assumes !r.is_ok;
              ensures \result.is_ok == \false;
              // \result.val.err == f(r.val.err): not stated, the callback's value
            complete behaviors;
            disjoint behaviors;
        */
        static inline result__Bool_Error result__Bool_Error_map_err(
            result__Bool_Error r, Error (*f)(Error));
        /*@ requires r.is_ok ==> \valid_function(f);
            terminates !r.is_ok;
            exits r.is_ok;
            behavior err:
              assumes !r.is_ok;
              assigns \nothing;
              ensures \result.is_ok == \false;
              ensures \result.val.err == r.val.err;
            behavior ok:
              assumes r.is_ok;
              // \result == f(r.val.ok): not stated, the callback's value
            complete behaviors;
            disjoint behaviors;
        */
        static inline result__Bool_Error result__Bool_Error_and_then(
            result__Bool_Error r, result__Bool_Error (*f)(bool));
        /*@ requires !r.is_ok ==> \valid_function(f);
            terminates r.is_ok;
            exits !r.is_ok;
            behavior ok:
              assumes r.is_ok;
              assigns \nothing;
              ensures \result.is_ok == \true;
              ensures \result.val.ok == r.val.ok;
            behavior err:
              assumes !r.is_ok;
              // \result == f(r.val.err): not stated, the callback's value
            complete behaviors;
            disjoint behaviors;
        */
        static inline result__Bool_Error result__Bool_Error_or_else(
            result__Bool_Error r, result__Bool_Error (*f)(Error));
        /*@ requires (r1.is_ok && r2.is_ok) ==> \valid_function(eq_ok);
            requires (!r1.is_ok && !r2.is_ok) ==> \valid_function(eq_err);
            terminates r1.is_ok != r2.is_ok;
            exits r1.is_ok == r2.is_ok;
            behavior mismatch:
              assumes r1.is_ok != r2.is_ok;
              assigns \nothing;
              ensures \result == \false;
            behavior both_ok:
              assumes r1.is_ok && r2.is_ok;
              // \result == eq_ok(r1.val.ok, r2.val.ok): not stated
            behavior both_err:
              assumes !r1.is_ok && !r2.is_ok;
              // \result == eq_err(r1.val.err, r2.val.err): not stated
            complete behaviors;
            disjoint behaviors;
        */
        static inline bool result__Bool_Error_eq(result__Bool_Error r1,
                                                 result__Bool_Error r2,
                                                 bool (*eq_ok)(bool, bool),
                                                 bool (*eq_err)(Error, Error));
        DEFINE_RESULT_FUNCTIONS(static inline, bool, Error)
    #else
        /* cppcheck-suppress misra-c2012-19.2 ; MISRA-DEV-014 */
        CANON_RESULT(bool, Error)
    #endif
#endif

/**
 * @file data/priority_queue.h
 * @brief Fixed-capacity binary min-heap (priority queue) with caller-owned buffer
 *
 * A PriorityQueue is a binary min-heap backed by a flat contiguous buffer.
 * The element with the lowest comparator value is always at the top.
 * Use a descending comparator for max-heap behavior.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Fixed capacity — no dynamic growth, no hidden allocation
 * - Caller owns the backing buffer (stack, arena, static, etc.)
 * - Min-heap by default; pass a descending comparator for max-heap
 * - O(log n) push/pop, O(1) peek
 * - Fallible operations return result__Bool_Error
 * - Typed peek/pop return option_T via DEFINE_PRIORITY_QUEUE(T)
 * - bytes_t view of current contents via pq_as_bytes()
 *
 * NULL contract (uniform across all functions):
 * ────────────────────────────────────────────────────────────────────────────
 * - A NULL PriorityQueue* is a silent no-op for void functions
 * - Query functions return 0, false, or a safe sentinel for NULL input
 * - Invalid arguments on a valid non-NULL PriorityQueue* fire require_msg —
 *   these are programming errors, not recoverable conditions
 *
 * Min-heap vs max-heap:
 * ────────────────────────────────────────────────────────────────────────────
 * - Min-heap (default): pass algo_cmp_i32 or any ascending comparator
 * - Max-heap: pass algo_cmp_i32_desc or any descending comparator
 *
 * Typed usage (recommended):
 * ────────────────────────────────────────────────────────────────────────────
 * Use DEFINE_PRIORITY_QUEUE(T) to get a fully type-safe wrapper that
 * returns option_T from pop/peek instead of raw void* out-params.
 * Requires CANON_OPTION(T) to be instantiated first.
 *
 * Lifetime tracking (define CANON_LIFETIME_DEBUG before including):
 * ────────────────────────────────────────────────────────────────────────────
 *   - Embeds a lifetime_t lt field on the PriorityQueue struct. The field
 *     is inherited transparently by every DEFINE_PRIORITY_QUEUE(T) typed
 *     wrapper — typed wrappers just contain `PriorityQueue _pq` and read
 *     `h->_pq.lt` exactly like any other field.
 *   - pq_init opens a fresh lifetime. The ID is derived from a per-TU
 *     monotonic counter XOR'd with the constructor's address (same pattern
 *     as vec/deque — defensive against any future shift to a value-return
 *     constructor shape).
 *   - PriorityQueue has NO destructor. The buffer is caller-owned and the
 *     struct is caller-allocated; there is no hook on which to call close.
 *     The lifetime stays open for the life of the struct. Use-of-PQ-
 *     after-the-struct-goes-out-of-scope is the caller's responsibility,
 *     not the substrate's. (Same contract as deque.)
 *   - Restamp-on-mutation: every operation that can change the element
 *     at index 0 — push_result, pop_raw, remove_at_result, heapify — re-
 *     derives lt.id at the end of the operation (on the success path).
 *     A borrow that captured the previous id (e.g. against the address
 *     returned by pq_peek_raw before the mutation) reads the new id at
 *     the same &pq->lt address and mismatches — invalidation by id-bump,
 *     analogous to vec's swap-via-struct-copy semantics.
 *   - Operations that do NOT restamp: peek_raw/peek, queries (len,
 *     capacity, remaining, is_empty, is_full, as_bytes). The bool legacy
 *     wrappers (pq_push, pq_pop, pq_peek, pq_remove_at) inherit the
 *     restamp transitively through the result variants they delegate to.
 *   - Internal helpers (pq_swap_, pq_sift_up_, pq_sift_down_) deliberately
 *     do NOT restamp. The restamp belongs at the public-operation
 *     boundary so a single push (which may call sift_up many times)
 *     produces exactly one id bump, not one per internal swap.
 *   Zero cost in release builds — struct layout is identical without
 *   the flag.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * data/priority_queue.h is in data/ — depends on core/ and semantics/.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * NOT thread-safe. Caller must synchronize if shared across threads.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - pq_push / pq_push_result:           O(log n)
 * - pq_pop_raw / pq_pop:                O(log n)
 * - pq_peek_raw / pq_peek:              O(1)
 * - pq_remove_at / pq_remove_at_result: O(log n)
 * - pq_heapify:                         O(n) — Floyd's algorithm
 * - pq_as_bytes:                        O(1)
 *
 * Quick start:
 * ```c
 * CANON_OPTION(int)
 * DEFINE_PRIORITY_QUEUE(int)
 *
 * int buf[64];
 * pq_int h;
 * pq_int_init(&h, buf, 64, algo_cmp_i32, NULL);
 * pq_int_push_result(&h, 42);
 * pq_int_push_result(&h, 7);
 *
 * option_int top = pq_int_pop_option(&h);  // Some(7) — min-heap
 * ```
 *
 * @sa core/primitives/compare.h — algo_cmp_fn and built-in comparators
 * @sa core/primitives/lifetime.h — canonical home of region_id_t and lifetime_t
 * @sa semantics/option/option.h — option_T for typed peek/pop
 * @sa semantics/result/result.h — result__Bool_Error for fallible operations
 * @sa semantics/error.h         — Error enum (ERR_INVALID_ARG, etc.)
 * @sa core/ownership.h          — borrowed() annotation
 * @sa core/slice.h              — bytes_t view of heap contents
 * @sa core/memory.h             — mem_copy, mem_swap, CANON_MEM_SWAP_MAX
 */

/* ════════════════════════════════════════════════════════════════════════════
   Internal: lifetime field and helpers
   ════════════════════════════════════════════════════════════════════════════
   Field is conditionally injected via PQ_LIFETIME_FIELD_. Helpers are
   conditionally defined; in release builds they are empty no-ops (and the
   field doesn't exist on the struct).

   PriorityQueue is a single base struct shared across all typed wrappers —
   so a single pair of file-scoped helpers (open + restamp) suffices for
   every DEFINE_PRIORITY_QUEUE(T) instantiation. Unlike vec/deque, no
   per-instantiation helper is needed because nothing in this module
   token-pastes against the lifetime helper name.

   Restamp uses the same counter-mixed derivation as open. The result is
   a fresh id distinct from the previous one (the counter monotonically
   increments) — borrows that captured the old id now mismatch.
   ════════════════════════════════════════════════════════════════════════════ */

#ifdef CANON_LIFETIME_DEBUG
    #define PQ_LIFETIME_FIELD_ \
        lifetime_t lt; /**< [debug] Lifetime token: id + open */

    /* Per-TU counter used to derive unique lifetime ids.
     *
     * The counter ensures successive id derivations within a TU produce
     * distinct values. The XOR with the struct address adds cross-TU
     * diversity. Used by both open (on pq_init) and restamp (on every
     * mutating operation).
     *
     * No thread-safety guarantee — concurrent construction or concurrent
     * mutation requires external synchronization, the same constraint
     * that applies to every Canon-C container.
     *
     * REGION_ID_STATIC (0) is reserved; the counter starts at 1 and the
     * derivation is defensively guarded against producing 0.
     */
    /* Delegates to the single shared generator in
     * core/primitives/lifetime.h — see docs/thread-safety.md for the
     * concurrency contract on token generation. */
    static inline region_id_t pq_lifetime_next_id_(void* pqp) {
        return canon_lifetime_next_id_(pqp);
    }
#else
    #define PQ_LIFETIME_FIELD_  /* empty */
#endif

/* ════════════════════════════════════════════════════════════════════════════
   PriorityQueue struct
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Fixed-capacity binary min-heap
 *
 * Do not access fields directly — use the provided functions.
 * Always initialize with pq_init() before use.
 *
 * Under CANON_LIFETIME_DEBUG, an additional lifetime_t lt field is appended
 * for borrow-invalidation tracking. Typed wrappers (DEFINE_PRIORITY_QUEUE)
 * read this field transparently as `h->_pq.lt`.
 */
typedef struct {
    void*       data;      ///< Caller-owned element buffer
    usize       len;       ///< Current number of elements
    usize       capacity;  ///< Fixed maximum number of elements
    usize       elem_size; ///< Size of each element in bytes
    algo_cmp_fn cmp;       ///< Three-way comparator (< 0 means parent first)
    void*       ctx;       ///< Optional context passed to cmp (may be NULL)
    PQ_LIFETIME_FIELD_
} PriorityQueue;

/* ════════════════════════════════════════════════════════════════════════════
   Internal: lifetime open/restamp helpers (file-scoped, after struct defn)
   ════════════════════════════════════════════════════════════════════════════ */

#ifdef CANON_LIFETIME_DEBUG
    /** @brief Opens a fresh lifetime token on the queue (called by pq_init). */
    static inline void pq_lifetime_open_(PriorityQueue* pq) {
        pq->lt.id   = pq_lifetime_next_id_(pq);
        pq->lt.open = true;
    }
    /** @brief Derives a new id, invalidating any prior borrows. */
    static inline void pq_lifetime_restamp_(PriorityQueue* pq) {
        pq->lt.id = pq_lifetime_next_id_(pq);
        /* lt.open stays true — restamp is not destruction */
    }
#else
    /* VERIFY-022 commit 2. These stubs had no frame, so WP assumed they
     * assign everything and every postcondition downstream of a call died:
     * all 8 of pq_init's ensures at run 1, plus a large share of push /
     * pop / remove / heapify. The deque run-1 F1 shape — missing default
     * assigns on a callee — recurring. Contract covers the STUBS only; the
     * CANON_LIFETIME_DEBUG bodies above are not in any verified
     * configuration. */
    #ifdef __FRAMAC__
    /*@ assigns \nothing; */
    #endif
    static inline void pq_lifetime_open_(PriorityQueue* pq)    { (void)pq; }
    #ifdef __FRAMAC__
    /*@ assigns \nothing; */
    #endif
    static inline void pq_lifetime_restamp_(PriorityQueue* pq) { (void)pq; }
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Internal helpers
   ════════════════════════════════════════════════════════════════════════════ */


/** @brief Returns index of parent node — @pre i > 0 */
/* ════════════════════════════════════════════════════════════════════════════
 * ACSL: structural invariant and scope (VERIFY-022, commit 7 — runs 1-6 at CI #1282-#1288 scored, see the job)
 *
 * WHAT IS CLAIMED: the STRUCTURAL invariant only. A queue is well-formed when
 * its buffer is valid for capacity*elem_size bytes, len <= capacity, and
 * elem_size > 0. Plus exact results for the accessors, and the len arithmetic
 * of push/pop/remove.
 *
 * WHAT IS NOT CLAIMED, AND CANNOT BE: HEAP ORDER. pq_sift_up_ and
 * pq_sift_down_ decide every swap by calling pq->cmp(a, b, ctx) — an indirect
 * call through a CALLER-SUPPLIED function pointer. WP reasons about an
 * indirect call only through a `calls` clause that enumerates its possible
 * targets (it says so in the run-0 log: "no 'calls' specification ...
 * Assuming that they can call 'pq_sift_down_'"), and `\valid_function` is
 * unimplemented. The base layer's comparator is any function the caller
 * chooses, so no finite target list exists to write; without one WP treats
 * the call as an unknown callee — possibly non-terminating, assigning
 * everything, possibly recursive. "parent <= child" is therefore a statement
 * about an uninterpreted function, and asserting it would be asserting
 * something the prover cannot connect to the code. (The DEFINE_PRIORITY_QUEUE
 * typed wrappers DO have an enumerable comparator set — the algo_cmp_* family
 * — so a `calls` clause is writable for them; that is a separate, later pass
 * and does not lift the ceiling on the base layer.)
 *
 * This is not a timeout and no budget touches it. It is the same ceiling that
 * leaves the run-time checks on option's and result's combinator pointers
 * unproved (6 per instantiation at their home units since VERIFY-039 G11;
 * before it their dispatch residuals were 32 and 28, the rest coming from
 * contracts that claimed, where the pointer is called, what an unknown
 * callee need not satisfy), met here for the first time in a container
 * whose CORRECTNESS ARGUMENT depends on it — vec and deque are ordered by
 * insertion, so their specs never needed the comparator.
 * A priority queue's whole point is the ordering, and the ordering is the part
 * that cannot be specified.
 *
 * Recorded as a specification-strength ceiling, VERIFY-020 F4 family, declared
 * before the run rather than discovered in the residual count.
 *
 * COMMIT 5 UPDATE. The FRAME half of this ceiling -- "WP treats the call as
 * an unknown callee, assigning everything" -- is closed for the verified
 * configuration by pq_cmp_ (see its banner): a `calls` clause over the 24
 * proved built-in comparators makes the call's purity a theorem. The ORDER
 * half stands: the built-ins ensure only -1 <= \result <= 1, so "parent <=
 * child" is still not statable. What changed is that WP now knows the queue
 * is intact after a comparison; it still knows nothing about which way the
 * comparison went.
 *
 * The heap order is covered instead by the runtime suite: every pop sequence
 * in priority_queue_test.c asserts ascending order, including through the
 * byte-wise swap path for elements over CANON_MEM_SWAP_MAX.
 * ════════════════════════════════════════════════════════════════════════════ */
#ifdef __FRAMAC__
/*@
  // pq_wf_buf: everything that holds of an INITIALISED queue regardless of
  // len. Introduced in commit 5 because heapify -- which SETS len and so
  // cannot require len <= capacity -- had its own copy of these conjuncts
  // in its requires, and that copy lagged pq_wf in commits 3 AND 4 (the
  // overflow bound, then \separated), regressing heapify 0 -> 4 -> 5. One
  // definition, two names; heapify requires pq_wf_buf and ensures pq_wf.
  // VERIFIED CONFIGURATION: the comparator is one of compare.h's 24 built-ins.
  // Every one of them is proved (VERIFY-005, 208/208, zero residuals) with
  // `assigns \nothing`, byte-form validity requires, and -1 <= \result <= 1.
  // Restricting the proof to them lets pq_cmp_'s frame be a THEOREM rather
  // than a trusted axiom. A caller-supplied comparator is OUTSIDE this
  // configuration: the C code accepts it, the proof does not cover it. That
  // is the lifetime.h shape -- "verified at level 4 only" -- not the diag.h
  // trusted-axiom shape, and it is the stronger of the two.
  predicate pq_cmp_builtin(PriorityQueue* pq) =
       pq->cmp == algo_cmp_u8
    || pq->cmp == algo_cmp_u8_desc
    || pq->cmp == algo_cmp_u16
    || pq->cmp == algo_cmp_u16_desc
    || pq->cmp == algo_cmp_u32
    || pq->cmp == algo_cmp_u32_desc
    || pq->cmp == algo_cmp_u64
    || pq->cmp == algo_cmp_u64_desc
    || pq->cmp == algo_cmp_i8
    || pq->cmp == algo_cmp_i8_desc
    || pq->cmp == algo_cmp_i16
    || pq->cmp == algo_cmp_i16_desc
    || pq->cmp == algo_cmp_i32
    || pq->cmp == algo_cmp_i32_desc
    || pq->cmp == algo_cmp_i64
    || pq->cmp == algo_cmp_i64_desc
    || pq->cmp == algo_cmp_usize
    || pq->cmp == algo_cmp_usize_desc
    || pq->cmp == algo_cmp_isize
    || pq->cmp == algo_cmp_isize_desc
    || pq->cmp == algo_cmp_f32
    || pq->cmp == algo_cmp_f32_desc
    || pq->cmp == algo_cmp_f64
    || pq->cmp == algo_cmp_f64_desc;

  // Each built-in reads sizeof(T) bytes through a and b; the element must be
  // at least that wide, or the comparator's own requires cannot be met.
  predicate pq_cmp_fits(PriorityQueue* pq) =
       ((pq->cmp == algo_cmp_u8 || pq->cmp == algo_cmp_u8_desc) ==> pq->elem_size >= sizeof(u8))
    && ((pq->cmp == algo_cmp_u16 || pq->cmp == algo_cmp_u16_desc) ==> pq->elem_size >= sizeof(u16))
    && ((pq->cmp == algo_cmp_u32 || pq->cmp == algo_cmp_u32_desc) ==> pq->elem_size >= sizeof(u32))
    && ((pq->cmp == algo_cmp_u64 || pq->cmp == algo_cmp_u64_desc) ==> pq->elem_size >= sizeof(u64))
    && ((pq->cmp == algo_cmp_i8 || pq->cmp == algo_cmp_i8_desc) ==> pq->elem_size >= sizeof(i8))
    && ((pq->cmp == algo_cmp_i16 || pq->cmp == algo_cmp_i16_desc) ==> pq->elem_size >= sizeof(i16))
    && ((pq->cmp == algo_cmp_i32 || pq->cmp == algo_cmp_i32_desc) ==> pq->elem_size >= sizeof(i32))
    && ((pq->cmp == algo_cmp_i64 || pq->cmp == algo_cmp_i64_desc) ==> pq->elem_size >= sizeof(i64))
    && ((pq->cmp == algo_cmp_usize || pq->cmp == algo_cmp_usize_desc) ==> pq->elem_size >= sizeof(usize))
    && ((pq->cmp == algo_cmp_isize || pq->cmp == algo_cmp_isize_desc) ==> pq->elem_size >= sizeof(isize))
    && ((pq->cmp == algo_cmp_f32 || pq->cmp == algo_cmp_f32_desc) ==> pq->elem_size >= sizeof(f32))
    && ((pq->cmp == algo_cmp_f64 || pq->cmp == algo_cmp_f64_desc) ==> pq->elem_size >= sizeof(f64));

  predicate pq_wf_buf(PriorityQueue* pq) =
       \valid(pq)
    && pq->elem_size > 0
    && pq->capacity  > 0
    && pq->cmp != \null
    && pq_cmp_builtin(pq)
    && pq_cmp_fits(pq)
    // commit 6, FINDING F-WRAP: pq_left_child_/pq_right_child_ compute 2*i+1
    // and 2*i+2 in usize with NO guard. For elem_size == 1 a queue with
    // capacity > 2^63 - 1 makes 2*idx+1 wrap, and `left < pq->len` then reads
    // the WRONG slot silently. Unreachable on any real machine (no such
    // buffer exists), so not a runtime bug; but WP found it at CI #1287 as
    // two unprovable child-index requires, and the proof must state the
    // bound rather than assume it. Same shape as arena's CANON_ARENA_MAX_SIZE
    // argument in MCDC-003.
    && pq->capacity <= (CANON_USIZE_MAX - 2) / 2
    // commit 3: ptr_elem requires index <= CANON_USIZE_MAX / elem_size, and
    // run 2 showed ~20 residuals were exactly that obligation, unprovable
    // because the invariant did not carry the bound. With it, any index
    // below capacity satisfies ptr_elem by transitivity -- no nonlinear
    // arithmetic needed, the division is one fixed term.
    && pq->capacity <= CANON_USIZE_MAX / pq->elem_size
    && \valid((char*)pq->data + (0 .. pq->capacity * pq->elem_size - 1))
    // commit 4: the struct and its element buffer are disjoint. Without
    // this, every callee frame of the form
    //   assigns ((char*)pq->data)[0 .. capacity*elem_size - 1]
    // can be read by WP as possibly writing *pq itself -- a char* range with
    // no separation fact could alias anything -- so pq->len, pq->capacity
    // and pq->elem_size were unknown after any call to pq_swap_ / sift_up_ /
    // sift_down_, and the caller's own ensures and frames failed even where
    // the callee's contract was fully proved. Registered for run 4 as the
    // cause of push_result's accepted_ensures pair, pop_raw's nonempty
    // assigns pair, and the sift assigns fragments.
    && \separated(pq, (char*)pq->data + (0 .. pq->capacity * pq->elem_size - 1));

  predicate pq_wf(PriorityQueue* pq) = pq_wf_buf(pq) && pq->len <= pq->capacity;

  // The slot index i is inside the live prefix.
  predicate pq_in_use(PriorityQueue* pq, integer i) = 0 <= i < pq->len;
*/
#endif

/* ════════════════════════════════════════════════════════════════════════════
 * pq_cmp_ — THE ONE PLACE THE QUEUE CALLS THE COMPARATOR.
 *
 * VERIFY-022 commit 5. Until now pq_sift_up_ and pq_sift_down_ called
 * pq->cmp(...) directly at three sites. WP has no contract for an unknown
 * callee, so from each such call onward it assumed EVERYTHING was assigned:
 * pq->data, pq->len, pq->elem_size all unknown, and every later obligation
 * in the function -- ptr_elem requires, pq_swap_ requires, loop invariant
 * preservation, frames, termination -- failed regardless of its own merit.
 * At CI #1285 that cascade was ~99 of 144 own residuals. The comparator call
 * was an information horizon.
 *
 * TWO WAYS TO CLOSE IT WERE CONSIDERED, and the second was chosen:
 *
 *   (a) A TRUSTED AXIOM: contract this wrapper `assigns \nothing` and leave
 *       its own goal unprovable as the permanent marker -- diag.h's stdio
 *       axiom shape (VERIFY-017). Covers any comparator; proves nothing
 *       about it.
 *
 *   (b) A VERIFIED CONFIGURATION: a `calls` clause naming compare.h's 24
 *       built-in comparators, and a requires that pq->cmp is one of them.
 *       All 24 are proved -- VERIFY-005, 208/208, zero residuals -- with
 *       `assigns \nothing`, byte-form validity requires, and a bounded
 *       result. This wrapper's frame and termination then follow from THEIR
 *       contracts: a theorem, not an assumption. lifetime.h's shape,
 *       "verified at level 4 only". The claim is narrower -- a caller-
 *       supplied comparator is outside the verified configuration, though
 *       the C code accepts it exactly as before -- and nothing is trusted.
 *
 * (b) is what is below. The one goal expected to remain unprovable is
 * \valid_function(pq->cmp), unimplemented in Frama-C 29 -- the same single
 * goal every function-pointer call in the project carries.
 *
 * NOT claimed: anything about the comparator's RESULT beyond -1..1. Heap
 * order still cannot be stated here, and the runtime suite remains the
 * evidence for ordering. This wrapper changes what WP knows about the queue
 * AFTER a comparison, not what it knows about the comparison.
 *
 * Shipped code: three call sites change from pq->cmp(a, b, pq->ctx) to
 * pq_cmp_(pq, a, b). static inline; identical machine code; no conditions
 * added, so MC/DC's denominator does not move.
 * ════════════════════════════════════════════════════════════════════════════ */
#ifdef __FRAMAC__
/*@
  requires \valid_read(pq);
  requires pq->cmp != \null;
  requires pq_cmp_builtin(pq);
  requires pq_cmp_fits(pq);
  requires \valid_read((char*)a + (0 .. pq->elem_size - 1));
  requires \valid_read((char*)b + (0 .. pq->elem_size - 1));
  assigns \nothing;
  ensures -1 <= \result <= 1;
*/
#endif
static inline int pq_cmp_(const PriorityQueue* pq, const void* a, const void* b) {
#ifdef __FRAMAC__
    /*@ calls algo_cmp_u8, algo_cmp_u8_desc, algo_cmp_u16, algo_cmp_u16_desc, algo_cmp_u32, algo_cmp_u32_desc, algo_cmp_u64, algo_cmp_u64_desc, algo_cmp_i8, algo_cmp_i8_desc, algo_cmp_i16, algo_cmp_i16_desc, algo_cmp_i32, algo_cmp_i32_desc, algo_cmp_i64, algo_cmp_i64_desc, algo_cmp_usize, algo_cmp_usize_desc, algo_cmp_isize, algo_cmp_isize_desc, algo_cmp_f32, algo_cmp_f32_desc, algo_cmp_f64, algo_cmp_f64_desc; */
#endif
    return pq->cmp(a, b, pq->ctx);
}

#ifdef __FRAMAC__
/*@
  requires i > 0;
  requires i <= (usize)(-1);
  assigns \nothing;
  ensures \result == (i - 1) / 2;
  ensures \result < i;
*/
#endif
static inline usize pq_parent_(usize i) {
    require_msg(i > 0u, "pq_parent_: i must be > 0 (root has no parent)");
    return (i - 1u) / 2u;
}
/** @brief Returns index of left child */
#ifdef __FRAMAC__
/*@
  requires 2 * i + 1 <= (usize)(-1);   // no unsigned wrap
  assigns \nothing;
  ensures \result == 2 * i + 1;
  ensures \result > i;
*/
#endif
static inline usize pq_left_child_(usize i)  { return (2u * i) + 1u; }
/** @brief Returns index of right child */
#ifdef __FRAMAC__
/*@
  requires 2 * i + 2 <= (usize)(-1);   // no unsigned wrap
  assigns \nothing;
  ensures \result == 2 * i + 2;
  ensures \result > i;
*/
#endif
static inline usize pq_right_child_(usize i) { return (2u * i) + 2u; }

/**
 * @brief Swaps two elements in the heap. INTERNAL.
 *
 * Trailing underscore as of 2026-09: this and pq_sift_up_, pq_sift_down_,
 * pq_parent_, pq_left_child_ and pq_right_child_ were plain public symbols
 * until then, while this same header already marked pq_lifetime_next_id_,
 * pq_lifetime_open_ and pq_lifetime_restamp_ internal by that convention.
 * The rule existed here and had not been applied to the heap internals.
 *
 * The exposure was not cosmetic. Demonstrated on the pre-rename header:
 * a client calling pq_swap(&q, 0, 4) on a queue holding 1..5 leaves
 * pq_peek returning 3 where the minimum is 1 - the heap invariant broken
 * silently, no error, no diagnostic. And because this function
 * deliberately does not restamp (see below), a client call mutates
 * contents WITHOUT bumping the lifetime id, so an outstanding borrow keeps
 * validating against a queue that changed underneath it. The instrument
 * fails OPEN, the same mode VERIFY-021 was written to prevent in the token
 * generator.
 *
 * Verified before renaming: one caller outside this header — the test
 * test_self_swap_is_a_noop in test/, which called pq_swap directly and was
 * deleted rather than adapted (see MCDC-014 in docs/deviations.md). None in
 * other headers. The rename is therefore contained, and no deprecation shim
 * is provided.
 *
 * Consequence for coverage: the `a == b` early return is now dead by
 * construction. pq_sift_down_ only calls this when smallest != idx, and
 * pq_sift_up_ swaps a parent with a child at idx > 0, so a self-swap
 * cannot arise from the heap's own operations. It takes an MC/DC
 * justification row rather than a test.
 *
 * @pre pq != NULL
 * @pre a < pq->len && b < pq->len
 *
 * Lifetime: does NOT restamp. The restamp belongs at the public-operation
 * boundary so a single push/pop produces exactly one id bump, not one per
 * internal swap.
 */
#ifdef __FRAMAC__
/*@
  requires pq_wf(pq);
  requires pq_in_use(pq, a);
  requires pq_in_use(pq, b);
  assigns ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
  ensures pq_wf(pq);
  // NOT claimed: that the two elements exchanged contents. Stating it needs
  // a ghost model of the byte ranges and is out of scope for this pass;
  // the runtime suite checks it, including the >256-byte path where the
  // whole element (pad[0] AND pad[511]) is verified to have moved.
*/
#endif
static inline void pq_swap_(borrowed(PriorityQueue*) pq, usize a, usize b) {
    require_msg(pq != NULL, "pq_swap_: pq cannot be NULL");
    if (a == b) { return; }
    usize es = pq->elem_size;
    void* pa = ptr_elem(pq->data, a, es);
    void* pb = ptr_elem(pq->data, b, es);
    if (es <= CANON_MEM_SWAP_MAX) {
        mem_swap(pa, pb, es);
    } else {
        /* Fallback for elements larger than mem_swap's stack buffer */
        unsigned char* ba = (unsigned char*)pa;
        unsigned char* bb = (unsigned char*)pb;
        /*@
          loop invariant 0 <= k <= es;
          loop assigns k, ba[0 .. es - 1], bb[0 .. es - 1];
          loop variant es - k;
        */
        for (usize k = 0; k < es; k++) {
            unsigned char t = ba[k]; ba[k] = bb[k]; bb[k] = t;
        }
    }
}

/**
 * @brief Restores heap invariant upward from index i
 *
 * @pre pq != NULL
 *
 * Lifetime: does NOT restamp (internal helper).
 */
#ifdef __FRAMAC__
/*@
  requires pq_wf(pq);
  requires i < pq->len;
  assigns ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
  ensures pq_wf(pq);
  // NOT claimed: heap order. Every swap decision here is pq->cmp(...), an
  // indirect call through a caller-supplied pointer that WP cannot
  // interpret. See the scope block above pq_parent_.
*/
#endif
static inline void pq_sift_up_(borrowed(PriorityQueue*) pq, usize i) {
    require_msg(pq != NULL, "pq_sift_up_: pq cannot be NULL");
    usize idx = i;
    /*@
      loop invariant idx <= i;
      loop invariant idx < pq->len;
      loop invariant pq_wf(pq);
      loop assigns idx, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
      loop variant idx;
    */
    while (idx > 0u) {
        usize p  = pq_parent_(idx);
        void* pe = ptr_elem(pq->data, p, pq->elem_size);
        void* ie = ptr_elem(pq->data, idx, pq->elem_size);
        if (pq_cmp_(pq, pe, ie) <= 0) { break; }
        pq_swap_(pq, p, idx);
        idx = p;
    }
}

/**
 * @brief Restores heap invariant downward from index i
 *
 * @pre pq != NULL
 *
 * Lifetime: does NOT restamp (internal helper).
 */
#ifdef __FRAMAC__
/*@
  requires pq_wf(pq);
  requires i < pq->len;                  // commit 2: needed to establish the
                                         // loop invariant; true at all three
                                         // call sites (heapify, pop, remove)
  assigns ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
  ensures pq_wf(pq);
  // Termination: idx strictly increases (pq_left_child_ and pq_right_child_
  // both return > i) and is bounded by pq->len, so the while(true) loop
  // terminates. Whether WP discharges that without an explicit loop variant
  // is a question for run 0.
  // NOT claimed: heap order, same reason as pq_sift_up_.
*/
#endif
static inline void pq_sift_down_(borrowed(PriorityQueue*) pq, usize i) {
    require_msg(pq != NULL, "pq_sift_down_: pq cannot be NULL");
    usize idx = i;
    /*@
      loop invariant i <= idx;
      loop invariant idx < pq->len;
      loop invariant pq_wf(pq);
      loop assigns idx, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
      loop variant pq->len - idx;
    */
    while (true) {
        usize smallest = idx;
        usize left     = pq_left_child_(idx);
        usize right    = pq_right_child_(idx);
        if (left < pq->len) {
            void* sl = ptr_elem(pq->data, smallest, pq->elem_size);
            void* le = ptr_elem(pq->data, left,     pq->elem_size);
            if (pq_cmp_(pq, le, sl) < 0) { smallest = left; }
        }
        if (right < pq->len) {
            void* sl = ptr_elem(pq->data, smallest, pq->elem_size);
            void* re = ptr_elem(pq->data, right,    pq->elem_size);
            if (pq_cmp_(pq, re, sl) < 0) { smallest = right; }
        }
        if (smallest == idx) { break; }
        pq_swap_(pq, idx, smallest);
        idx = smallest;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Initialization
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes a PriorityQueue over a caller-owned buffer
 *
 * @param pq        Pointer to uninitialized PriorityQueue
 * @param buffer    Caller-owned element buffer — must hold at least
 *                  capacity * elem_size bytes and remain valid for the
 *                  lifetime of the queue
 * @param capacity  Maximum number of elements (> 0)
 * @param elem_size Size of each element in bytes (> 0)
 * @param cmp       Three-way comparator — must not be NULL
 * @param ctx       Optional context forwarded to cmp (may be NULL)
 *
 * @pre pq != NULL
 * @pre buffer != NULL
 * @pre capacity > 0
 * @pre elem_size > 0
 * @pre cmp != NULL
 *
 * @post pq->len == 0
 * @post pq->capacity == capacity
 *
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token. The ID is
 * derived from a per-TU counter XOR'd with pq's address — borrows constructed
 * against this queue carry this ID.
 *
 * Performance: O(1)
 */
#ifdef __FRAMAC__
/*@
  requires \valid(pq);
  requires buffer != \null;
  requires capacity  > 0;
  requires elem_size > 0;
  requires cmp != \null;
  requires cmp == algo_cmp_u8
    || cmp == algo_cmp_u8_desc
    || cmp == algo_cmp_u16
    || cmp == algo_cmp_u16_desc
    || cmp == algo_cmp_u32
    || cmp == algo_cmp_u32_desc
    || cmp == algo_cmp_u64
    || cmp == algo_cmp_u64_desc
    || cmp == algo_cmp_i8
    || cmp == algo_cmp_i8_desc
    || cmp == algo_cmp_i16
    || cmp == algo_cmp_i16_desc
    || cmp == algo_cmp_i32
    || cmp == algo_cmp_i32_desc
    || cmp == algo_cmp_i64
    || cmp == algo_cmp_i64_desc
    || cmp == algo_cmp_usize
    || cmp == algo_cmp_usize_desc
    || cmp == algo_cmp_isize
    || cmp == algo_cmp_isize_desc
    || cmp == algo_cmp_f32
    || cmp == algo_cmp_f32_desc
    || cmp == algo_cmp_f64
    || cmp == algo_cmp_f64_desc;
  requires ((cmp == algo_cmp_u8 || cmp == algo_cmp_u8_desc) ==> elem_size >= sizeof(u8))
    && ((cmp == algo_cmp_u16 || cmp == algo_cmp_u16_desc) ==> elem_size >= sizeof(u16))
    && ((cmp == algo_cmp_u32 || cmp == algo_cmp_u32_desc) ==> elem_size >= sizeof(u32))
    && ((cmp == algo_cmp_u64 || cmp == algo_cmp_u64_desc) ==> elem_size >= sizeof(u64))
    && ((cmp == algo_cmp_i8 || cmp == algo_cmp_i8_desc) ==> elem_size >= sizeof(i8))
    && ((cmp == algo_cmp_i16 || cmp == algo_cmp_i16_desc) ==> elem_size >= sizeof(i16))
    && ((cmp == algo_cmp_i32 || cmp == algo_cmp_i32_desc) ==> elem_size >= sizeof(i32))
    && ((cmp == algo_cmp_i64 || cmp == algo_cmp_i64_desc) ==> elem_size >= sizeof(i64))
    && ((cmp == algo_cmp_usize || cmp == algo_cmp_usize_desc) ==> elem_size >= sizeof(usize))
    && ((cmp == algo_cmp_isize || cmp == algo_cmp_isize_desc) ==> elem_size >= sizeof(isize))
    && ((cmp == algo_cmp_f32 || cmp == algo_cmp_f32_desc) ==> elem_size >= sizeof(f32))
    && ((cmp == algo_cmp_f64 || cmp == algo_cmp_f64_desc) ==> elem_size >= sizeof(f64));
  requires capacity <= (CANON_USIZE_MAX - 2) / 2;     // F-WRAP, see pq_wf_buf
  requires capacity <= CANON_USIZE_MAX / elem_size; // the product must not wrap;
                                                    // pq_init does NOT check this
                                                    // and a caller can overflow it.
                                                    // Stated in ptr_elem's form so
                                                    // pq_wf's bound is established
                                                    // directly, not via a product
  requires \valid((char*)buffer + (0 .. capacity * elem_size - 1));
  requires \separated(pq, (char*)buffer + (0 .. capacity * elem_size - 1));
  assigns *pq;
  ensures pq->data      == buffer;
  ensures pq->len       == 0;
  ensures pq->capacity  == capacity;
  ensures pq->elem_size == elem_size;
  ensures pq->cmp       == cmp;
  ensures pq->ctx       == ctx;
  ensures pq_wf(pq);
*/
#endif
static inline void pq_init(
    borrowed(PriorityQueue*) pq,
    borrowed(void*)          buffer,
    usize                    capacity,
    usize                    elem_size,
    algo_cmp_fn              cmp,
    void*                    ctx)
{
    require_msg(pq        != NULL, "pq_init: pq cannot be NULL");
    require_msg(buffer    != NULL, "pq_init: buffer cannot be NULL");
    require_msg(capacity   > 0u,    "pq_init: capacity must be > 0");
    require_msg(elem_size  > 0u,    "pq_init: elem_size must be > 0");
    require_msg(cmp       != NULL, "pq_init: cmp cannot be NULL");

    pq->data      = buffer;
    pq->len       = 0;
    pq->capacity  = capacity;
    pq->elem_size = elem_size;
    pq->cmp       = cmp;
    pq->ctx       = ctx;
    pq_lifetime_open_(pq);
}

/**
 * @brief Builds a valid heap in-place from len pre-existing elements
 *
 * Uses Floyd's algorithm — O(n), not O(n log n). Prefer this over pushing
 * elements one at a time when bulk-initializing from an existing array.
 *
 * The first `len` elements of pq->data must already be populated by the
 * caller. pq_heapify rearranges them in-place to satisfy the heap invariant.
 * If len > pq->capacity it is silently clamped to pq->capacity.
 *
 * NULL pq is a no-op.
 *
 * @pre pq->data contains len valid elements of size pq->elem_size
 * @post pq->len == min(len, pq->capacity)
 * @post Heap invariant holds for all nodes
 *
 * Lifetime (CANON_LIFETIME_DEBUG): RESTAMPS. Floyd's algorithm reshuffles
 * the whole heap; any borrow against a prior element position is invalid.
 *
 * Performance: O(n) — Floyd's algorithm
 */
#ifdef __FRAMAC__
/*@
  behavior null:
    assumes pq == \null;
    assigns \nothing;
  behavior live:
    assumes pq != \null;
    requires pq_wf_buf(pq);            // commit 5: was a hand-copied subset of
                                       // pq_wf that lagged it twice (CI #1284,
                                       // #1285). Now the same predicate.
    assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
    ensures pq->len == (len > pq->capacity ? pq->capacity : len);
    ensures pq_wf(pq);
  complete behaviors;
  disjoint behaviors;
  // NOT claimed: that the result is a heap. Floyd's algorithm is correct
  // only if pq->cmp is a strict weak order, and WP cannot interpret
  // pq->cmp at all. Structural claims only, as everywhere in this header.
*/
#endif
static inline void pq_heapify(borrowed(PriorityQueue*) pq, usize len) {
    if (!pq) { return; }
    require_msg(pq->data    != NULL, "pq_heapify: pq not initialized (data is NULL)");
    require_msg(pq->cmp     != NULL, "pq_heapify: pq not initialized (cmp is NULL)");
    require_msg(pq->capacity > 0u,    "pq_heapify: pq not initialized (capacity is 0)");
    if (len == 0u) { pq->len = 0u; pq_lifetime_restamp_(pq); return; }
    const usize n = (len > pq->capacity) ? pq->capacity : len;
    pq->len = n;
    if (n >= 2u) {
        usize i = pq_parent_(n - 1u) + 1u;
        /*@
          loop invariant 0 <= i <= n;
          loop invariant pq->len == n;
          loop invariant pq_wf(pq);
          loop assigns i, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
          loop variant i;
        */
        while (i-- > 0) {
            pq_sift_down_(pq, i);
        }
    }
    pq_lifetime_restamp_(pq);
}

/* ════════════════════════════════════════════════════════════════════════════
   Core operations — result__Bool_Error / raw out-param (untyped base layer)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Inserts an element into the heap (fallible, preferred)
 *
 * NULL pq or elem returns Err(ERR_INVALID_ARG).
 * Full queue returns Err(ERR_CAPACITY_EXCEEDED).
 *
 * @return result__Bool_Error — Ok(true) on success, Err on failure
 *
 * Lifetime (CANON_LIFETIME_DEBUG): RESTAMPS on the success path. Sift-up
 * may move the new element up to position 0, displacing whatever was there.
 * A borrow against the previous index-0 element is invalid after push.
 * The error paths (NULL args, capacity exceeded) do NOT restamp — nothing
 * changed.
 *
 * Performance: O(log n)
 */
#ifdef __FRAMAC__
/*@
  behavior rejected:
    assumes pq == \null || elem == \null;
    assigns \nothing;
    ensures \result.is_ok == \false;
  behavior full:
    assumes pq != \null && elem != \null && pq->len >= pq->capacity;
    requires \valid_read(pq);
    assigns \nothing;
    ensures \result.is_ok == \false;
  behavior accepted:
    assumes pq != \null && elem != \null && pq->len < pq->capacity;
    requires pq_wf(pq);
    requires \valid_read((char*)elem + (0 .. pq->elem_size - 1));
    requires \separated((char*)elem + (0 .. pq->elem_size - 1), (char*)pq->data + (0 .. pq->capacity * pq->elem_size - 1));
                                       // commit 5: mem_copy requires the copied
                                       // element not to overlap the slot it
                                       // lands in. A caller passing a pointer
                                       // INTO the queue's own buffer is outside
                                       // the API.
    assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
    ensures pq->len == \old(pq->len) + 1;
    ensures pq_wf(pq);
    ensures \result.is_ok == \true;
  complete behaviors;
  disjoint behaviors;
  // Behavior-local `assigns` because the three exits differ: two guards
  // return before touching anything, only the third writes. A single
  // function-level frame would have to over-approximate to the write path
  // and would then be true but useless on the guard paths; `assigns
  // \nothing` at function level would be FALSE, which is the exact defect
  // VERIFY-021 F1 corrected in lifetime.h. Whether WP accepts per-behavior
  // frames cleanly here is a question for run 0.
  // NOT claimed: that the pushed element equals *elem afterwards, nor that
  // heap order is restored. The first needs a byte-range ghost model; the
  // second needs pq->cmp, which WP cannot interpret.
*/
#endif
static inline result__Bool_Error pq_push_result(
    borrowed(PriorityQueue*)  pq,
    borrowed(const void*)     elem)
{
    if (!pq || !elem) { return result__Bool_Error_err(ERR_INVALID_ARG); }
    if (pq->len >= pq->capacity) { return result__Bool_Error_err(ERR_CAPACITY_EXCEEDED); }
    mem_copy(ptr_elem(pq->data, pq->len, pq->elem_size), elem, pq->elem_size);
    pq->len++;
    pq_sift_up_(pq, pq->len - 1u);
    pq_lifetime_restamp_(pq);
    return result__Bool_Error_ok(true);
}

/**
 * @brief Removes the top element and copies it into out
 *
 * NULL pq or empty queue returns false. NULL out is permitted — the element
 * is removed from the heap even if out is NULL (discard semantics).
 *
 * For a type-safe option-returning variant use pq_##T##_pop_option()
 * from DEFINE_PRIORITY_QUEUE(T).
 *
 * @return true if an element was removed, false if empty or pq is NULL
 *
 * Lifetime (CANON_LIFETIME_DEBUG): RESTAMPS on the success path. The
 * element at index 0 is removed; the last element is moved to position 0
 * and sift-down rearranges. A borrow against the previous index-0 is
 * invalid after pop.
 *
 * Performance: O(log n)
 */
#ifdef __FRAMAC__
/*@
  requires pq == \null || \valid_read(pq);   // commit 6: the code reads pq->len
                                              // whenever pq != NULL (CI #1287,
                                              // pop_raw_assert_rte_mem_access)
  behavior empty:
    assumes pq == \null || pq->len == 0;
    assigns \nothing;
    ensures \result == \false;
  behavior nonempty_discard:
    assumes pq != \null && pq->len > 0 && out == \null;
    requires pq_wf(pq);
    assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
    ensures \result == \true;
    ensures pq->len == \old(pq->len) - 1;
    ensures pq_wf(pq);
  behavior nonempty_out:
    assumes pq != \null && pq->len > 0 && out != \null;
    requires pq_wf(pq);
    requires \valid((char*)out + (0 .. pq->elem_size - 1));
    requires \separated((char*)out + (0 .. pq->elem_size - 1), pq, (char*)pq->data + (0 .. pq->capacity * pq->elem_size - 1));
    assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1], ((char*)out)[0 .. pq->elem_size - 1];
    ensures \result == \true;
    ensures pq->len == \old(pq->len) - 1;
    ensures pq_wf(pq);
  complete behaviors;
  disjoint behaviors;
  // commit 5: the class-(h) `out` frame, stated. Split on out != \null so
  // the frame is true on both exits (the run-1 note said to do exactly this).
*/
#endif
static inline bool pq_pop_raw(borrowed(PriorityQueue*) pq, void* out) {
    if (!pq || (pq->len == 0u)) { return false; }
    if (out != NULL) { mem_copy(out, ptr_elem(pq->data, 0, pq->elem_size), pq->elem_size); }
    pq->len--;
    if (pq->len > 0u) {
        mem_copy(ptr_elem(pq->data, 0,       pq->elem_size),
                 ptr_elem(pq->data, pq->len, pq->elem_size),
                 pq->elem_size);
        pq_sift_down_(pq, 0);
    }
    pq_lifetime_restamp_(pq);
    return true;
}

/**
 * @brief Returns a pointer to the top element without removing it
 *
 * NULL pq or empty queue returns NULL.
 * The returned pointer is valid until the next mutating operation.
 *
 * For a type-safe option-returning variant use pq_##T##_peek_option()
 * from DEFINE_PRIORITY_QUEUE(T).
 *
 * @return Pointer to the top element, or NULL if empty or pq is NULL
 *
 * Lifetime: does NOT restamp. Peek is non-mutating.
 *
 * Performance: O(1)
 */
#ifdef __FRAMAC__
/*@
  requires pq == \null || \valid_read(pq);   // commit 5: the code reads
                                              // pq->len whenever pq != NULL,
                                              // including on the empty path
  assigns \nothing;
  behavior empty:
    assumes pq == \null || pq->len == 0;
    ensures \result == \null;
  behavior nonempty:
    assumes pq != \null && pq->len > 0;
    requires \valid_read(pq);
    requires pq->len <= pq->capacity && pq->elem_size > 0;
    requires \valid_read((char*)pq->data + (0 .. pq->capacity * pq->elem_size - 1));
    ensures \result == (char*)pq->data;    // slot 0 is the top
  complete behaviors;
  disjoint behaviors;
*/
#endif
static inline const void* pq_peek_raw(borrowed(const PriorityQueue*) pq) {
    if (!pq || (pq->len == 0u)) { return NULL; }
    return ptr_elem_const(pq->data, 0, pq->elem_size);
}

/**
 * @brief Removes the element at heap index i (fallible, preferred)
 *
 * Replaces the removed slot with the last element, then runs both
 * pq_sift_up_ and pq_sift_down_ to restore the heap invariant regardless
 * of whether the replacement is smaller or larger than its neighbors.
 *
 * NULL pq or out-of-range i returns Err(ERR_OUT_OF_RANGE).
 *
 * @return result__Bool_Error — Ok(true) on success, Err on failure
 *
 * Lifetime (CANON_LIFETIME_DEBUG): RESTAMPS on the success path. The
 * removed slot is filled with the last element and reorganized — any
 * borrow against a prior position is invalid.
 *
 * Performance: O(log n)
 */
#ifdef __FRAMAC__
/*@
  requires pq == \null || \valid_read(pq);
  behavior rejected:
    assumes pq == \null;
    assigns \nothing;
    ensures \result.is_ok == \false;
  behavior out_of_range:
    assumes pq != \null && i >= pq->len;
    assigns \nothing;
    ensures \result.is_ok == \false;
  behavior removed:
    assumes pq != \null && i < pq->len;
    requires pq_wf(pq);
    assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
    ensures pq->len == \old(pq->len) - 1;
    ensures pq_wf(pq);
    ensures \result.is_ok == \true;
  complete behaviors;
  disjoint behaviors;
  // commit 5. The i == len early return and the copy-then-resift path are
  // one behaviour here: both shrink len by one and preserve pq_wf. The
  // mem_copy from slot len to slot i needs the two slots not to overlap,
  // which is i != len -- true on that path -- lifted through elem_size.
  // Whether the provers do that multiplication is a question for the run.
*/
#endif
static inline result__Bool_Error pq_remove_at_result(
    borrowed(PriorityQueue*) pq,
    usize                    i)
{
    if (!pq)          { return result__Bool_Error_err(ERR_INVALID_ARG); }
    if (i >= pq->len) { return result__Bool_Error_err(ERR_OUT_OF_RANGE); }
    pq->len--;
    if (i == pq->len) {
        /* removed last element — no rearrangement, but the heap composition
         * changed (one element gone), so prior borrows are still invalidated */
        pq_lifetime_restamp_(pq);
        return result__Bool_Error_ok(true);
    }
    mem_copy(ptr_elem(pq->data, i,       pq->elem_size),
             ptr_elem(pq->data, pq->len, pq->elem_size),
             pq->elem_size);
    pq_sift_up_(pq, i);
    pq_sift_down_(pq, i);
    pq_lifetime_restamp_(pq);
    return result__Bool_Error_ok(true);
}

/* ════════════════════════════════════════════════════════════════════════════
   Legacy bool wrappers — kept for compatibility, result variants preferred
   ════════════════════════════════════════════════════════════════════════════
   These delegate to the result/raw variants above. Restamp happens
   transitively through the inner call — no additional bookkeeping here.
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Inserts elem — returns true on success. Prefer pq_push_result(). */
#ifdef __FRAMAC__
/*@
  assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];  // commit 7
  behavior rejected:
    assumes pq == \null || elem == \null;
    ensures \result == \false;
  behavior full:
    assumes pq != \null && elem != \null && pq->len >= pq->capacity;
    requires \valid_read(pq);
    ensures \result == \false;
  behavior accepted:
    assumes pq != \null && elem != \null && pq->len < pq->capacity;
    requires pq_wf(pq);
    requires \valid_read((char*)elem + (0 .. pq->elem_size - 1));
    requires \separated((char*)elem + (0 .. pq->elem_size - 1), (char*)pq->data + (0 .. pq->capacity * pq->elem_size - 1));
    assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
    ensures pq->len == \old(pq->len) + 1;
    ensures pq_wf(pq);
    ensures \result == \true;
  complete behaviors;
  disjoint behaviors;
  // commit 6: no behaviour-local `assigns \\nothing` on the reject paths. WP
  // frames a CALL by the callee's default assigns -- the union over its
  // behaviours -- so a wrapper cannot claim a tighter frame than the callee
  // it delegates to (CI #1287, 7 goals across the three wrappers). deque's
  // run-1 fix was the same shape. True but weaker; recorded as such.
*/
#endif
static inline bool pq_push(borrowed(PriorityQueue*) pq, borrowed(const void*) elem) {
    return result__Bool_Error_is_ok(pq_push_result(pq, elem));
}

/** @brief Removes and copies the top element into out. Prefer pq_pop_raw(). */
#ifdef __FRAMAC__
/*@
  requires pq == \null || \valid_read(pq);   // commit 7: pop_raw requires this
                                              // (added in commit 6) and pq_pop
                                              // delegates to it -- the one goal
                                              // outside both classes at CI #1288
  assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1], ((char*)out)[0 .. pq->elem_size - 1];
  behavior empty:
    assumes pq == \null || pq->len == 0;
    ensures \result == \false;
  behavior nonempty_discard:
    assumes pq != \null && pq->len > 0 && out == \null;
    requires pq_wf(pq);
    assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
    ensures \result == \true;
    ensures pq->len == \old(pq->len) - 1;
    ensures pq_wf(pq);
  behavior nonempty_out:
    assumes pq != \null && pq->len > 0 && out != \null;
    requires pq_wf(pq);
    requires \valid((char*)out + (0 .. pq->elem_size - 1));
    requires \separated((char*)out + (0 .. pq->elem_size - 1), pq, (char*)pq->data + (0 .. pq->capacity * pq->elem_size - 1));
    assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1], ((char*)out)[0 .. pq->elem_size - 1];
    ensures \result == \true;
    ensures pq->len == \old(pq->len) - 1;
    ensures pq_wf(pq);
  complete behaviors;
  disjoint behaviors;
  // commit 6: no behaviour-local `assigns \\nothing` on the reject paths. WP
  // frames a CALL by the callee's default assigns -- the union over its
  // behaviours -- so a wrapper cannot claim a tighter frame than the callee
  // it delegates to (CI #1287, 7 goals across the three wrappers). deque's
  // run-1 fix was the same shape. True but weaker; recorded as such.
*/
#endif
static inline bool pq_pop(borrowed(PriorityQueue*) pq, void* out) {
    return pq_pop_raw(pq, out);
}

/** @brief Copies the top element into out without removing it. Prefer pq_peek_raw(). */
#ifdef __FRAMAC__
/*@
  requires pq == \null || \valid_read(pq);
  requires pq != \null && pq->len > 0 ==> pq_wf(pq);  // commit 6: when out == NULL
                                              // and the queue is non-empty, the
                                              // `none` branch still calls
                                              // pq_peek_raw's NONEMPTY behaviour,
                                              // whose requires need pq_wf
                                              // (CI #1287, 2 goals)
  behavior none:
    assumes pq == \null || pq->len == 0 || out == \null;
    assigns \nothing;
    ensures \result == \false;
  behavior some:
    assumes pq != \null && pq->len > 0 && out != \null;
    requires pq_wf(pq);
    requires \valid((char*)out + (0 .. pq->elem_size - 1));
    requires \separated((char*)out + (0 .. pq->elem_size - 1), (char*)pq->data + (0 .. pq->capacity * pq->elem_size - 1));
    assigns ((char*)out)[0 .. pq->elem_size - 1];
    ensures \result == \true;
  complete behaviors;
  disjoint behaviors;
*/
#endif
static inline bool pq_peek(borrowed(const PriorityQueue*) pq, void* out) {
    const void* top = pq_peek_raw(pq);
    if (!top || !out) { return false; }
    mem_copy(out, top, pq->elem_size);
    return true;
}

/** @brief Removes element at index i — returns true on success. Prefer pq_remove_at_result(). */
#ifdef __FRAMAC__
/*@
  requires pq == \null || \valid_read(pq);
  assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];  // commit 7
  behavior rejected:
    assumes pq == \null || i >= pq->len;
    ensures \result == \false;
  behavior removed:
    assumes pq != \null && i < pq->len;
    requires pq_wf(pq);
    assigns pq->len, ((char*)pq->data)[0 .. pq->capacity * pq->elem_size - 1];
    ensures pq->len == \old(pq->len) - 1;
    ensures pq_wf(pq);
    ensures \result == \true;
  complete behaviors;
  disjoint behaviors;
  // commit 6: no behaviour-local `assigns \\nothing` on the reject paths. WP
  // frames a CALL by the callee's default assigns -- the union over its
  // behaviours -- so a wrapper cannot claim a tighter frame than the callee
  // it delegates to (CI #1287, 7 goals across the three wrappers). deque's
  // run-1 fix was the same shape. True but weaker; recorded as such.
*/
#endif
static inline bool pq_remove_at(borrowed(PriorityQueue*) pq, usize i) {
    return result__Bool_Error_is_ok(pq_remove_at_result(pq, i));
}

/* ════════════════════════════════════════════════════════════════════════════
   Queries
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Returns current element count. NULL pq returns 0. */
#ifdef __FRAMAC__
/*@
  assigns \nothing;
  behavior null:
    assumes pq == \null;
    ensures \result == 0;
  behavior live:
    assumes pq != \null;
    requires \valid_read(pq);
    ensures \result == pq->len;
  complete behaviors;
  disjoint behaviors;
*/
#endif
static inline usize pq_len(borrowed(const PriorityQueue*) pq) {
    return pq ? pq->len : 0u;
}

/** @brief Returns maximum element capacity. NULL pq returns 0. */
#ifdef __FRAMAC__
/*@
  assigns \nothing;
  behavior null:
    assumes pq == \null;
    ensures \result == 0;
  behavior live:
    assumes pq != \null;
    requires \valid_read(pq);
    ensures \result == pq->capacity;
  complete behaviors;
  disjoint behaviors;
*/
#endif
static inline usize pq_capacity(borrowed(const PriorityQueue*) pq) {
    return pq ? pq->capacity : 0u;
}

/** @brief Returns number of remaining free slots. NULL pq returns 0. */
#ifdef __FRAMAC__
/*@
  assigns \nothing;
  behavior null:
    assumes pq == \null;
    ensures \result == 0;
  behavior live:
    assumes pq != \null;
    requires \valid_read(pq);
    requires pq->len <= pq->capacity;      // the structural invariant, needed
                                           // so the subtraction cannot wrap
    ensures \result == pq->capacity - pq->len;
  complete behaviors;
  disjoint behaviors;
*/
#endif
static inline usize pq_remaining(borrowed(const PriorityQueue*) pq) {
    return pq ? (pq->capacity - pq->len) : 0u;
}

/** @brief Returns true if the queue has no elements. NULL pq returns true. */
#ifdef __FRAMAC__
/*@
  assigns \nothing;
  behavior null:
    assumes pq == \null;
    ensures \result == \true;             // a null queue reads as empty
  behavior live:
    assumes pq != \null;
    requires \valid_read(pq);
    ensures \result <==> (pq->len == 0);
  complete behaviors;
  disjoint behaviors;
*/
#endif
static inline bool pq_is_empty(borrowed(const PriorityQueue*) pq) {
    return !pq || (pq->len == 0u);
}

/** @brief Returns true if the queue is at capacity. NULL pq returns false. */
#ifdef __FRAMAC__
/*@
  assigns \nothing;
  behavior null:
    assumes pq == \null;
    ensures \result == \false;            // and NOT as full: the two
                                           // null answers are deliberately
                                           // not complements
  behavior live:
    assumes pq != \null;
    requires \valid_read(pq);
    requires pq->len <= pq->capacity;      // run-1 correction: the code tests
                                           // >=, and without this bound
                                           // len > capacity makes the ensures
                                           // FALSE. Not hard, wrong. Under the
                                           // invariant >= and == coincide.
    ensures \result <==> (pq->len == pq->capacity);
  complete behaviors;
  disjoint behaviors;
*/
#endif
static inline bool pq_is_full(borrowed(const PriorityQueue*) pq) {
    return pq && (pq->len >= pq->capacity);
}

/**
 * @brief Returns a bytes_t view over the live heap elements
 *
 * Covers only [0, len). NULL or empty pq returns bytes_empty().
 * Non-owning — do not free the returned bytes_t.ptr.
 *
 * Performance: O(1)
 */
#ifdef __FRAMAC__
/*@
  requires pq == \null || \valid_read(pq);
  assigns \nothing;
  behavior empty:
    assumes pq == \null || pq->data == \null || pq->len == 0;
    ensures \result.len == 0;
  behavior live:
    assumes pq != \null && pq->data != \null && pq->len > 0;
    requires pq_wf(pq);
    ensures \result.ptr == (u8*)pq->data;
    ensures \result.len == pq->len * pq->elem_size;
  complete behaviors;
  disjoint behaviors;
  // commit 5. bytes_from needs the len*elem_size prefix valid; pq_wf gives
  // validity of capacity*elem_size and len <= capacity, so this is
  // len*es <= capacity*es -- monotonicity of multiplication by a positive
  // elem_size. Whether the provers close it is a question for the run.
*/
#endif
static inline bytes_t pq_as_bytes(borrowed(PriorityQueue*) pq) {
    if (!pq || !pq->data || (pq->len == 0u)) { return bytes_empty(); }
    return bytes_from(pq->data, pq->len * pq->elem_size);
}

/**
 * @brief Read-only twin of pq_as_bytes() — see API-001
 */
#ifdef __FRAMAC__
/*@
  requires pq == \null || \valid_read(pq);
  assigns \nothing;
  behavior empty:
    assumes pq == \null || pq->data == \null || pq->len == 0;
    ensures \result.len == 0;
  behavior live:
    assumes pq != \null && pq->data != \null && pq->len > 0;
    requires pq_wf(pq);
    ensures \result.ptr == (const u8*)pq->data;
    ensures \result.len == pq->len * pq->elem_size;
  complete behaviors;
  disjoint behaviors;
  // commit 5. bytes_from needs the len*elem_size prefix valid; pq_wf gives
  // validity of capacity*elem_size and len <= capacity, so this is
  // len*es <= capacity*es -- monotonicity of multiplication by a positive
  // elem_size. Whether the provers close it is a question for the run.
*/
#endif
static inline cbytes_t pq_as_cbytes(borrowed(const PriorityQueue*) pq) {
    if (!pq || !pq->data || (pq->len == 0u)) { return cbytes_empty(); }
    return cbytes_from(pq->data, pq->len * pq->elem_size);
}

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_PRIORITY_QUEUE(type) — typed wrapper macro
   ════════════════════════════════════════════════════════════════════════════
   Generates a type-safe pq_T wrapper over PriorityQueue.
   Pop and peek return option_T instead of raw void* out-params.

   Requires CANON_OPTION(type) to be instantiated before expanding this macro.

   The typed wrapper just contains a PriorityQueue _pq member — the lt
   field (and all lifetime bookkeeping) is inherited transparently from
   the base struct. No per-type lifetime helpers needed.

   Usage:
   ```c
   CANON_OPTION(int)
   DEFINE_PRIORITY_QUEUE(int)

   int buf[64];
   pq_int h;
   pq_int_init(&h, buf, 64, algo_cmp_i32, NULL);
   pq_int_push_result(&h, 42);
   pq_int_push_result(&h, 7);

   option_int top = pq_int_pop_option(&h);  // Some(7) — min-heap
   if (option_int_is_some(top)) {
       printf("%d\n", option_int_unwrap(top));
   }
   ```
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Generates a fully type-safe priority queue for a given element type
 *
 * @param type Element type — CANON_OPTION(type) must be instantiated first
 *
 * Note: pq_##type##_push_result and pq_##type##_remove_at_result return
 * result__Bool_Error (not result_bool_Error) — CANON_RESULT(bool, Error)
 * token-pastes to result__Bool_Error in C99 because bool expands to _Bool
 * before ## sees it.
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_PRIORITY_QUEUE(type)                                                          \
                                                                                             \
typedef struct { PriorityQueue _pq; } pq_##type;                                            \
                                                                                             \
/** Initializes a typed priority queue over a caller-owned buffer */                         \
static inline void pq_##type##_init(                                                         \
    borrowed(pq_##type*) h,                                                                  \
    borrowed(type*)      buf,                                                              \
    usize                cap,                                                                \
    algo_cmp_fn          cmp,                                                                \
    void*                ctx)                                                                \
{                                                                                            \
    pq_init(&h->_pq, buf, cap, sizeof(type), cmp, ctx);                                     \
}                                                                                            \
                                                                                             \
/** Heapifies len pre-existing elements already in the buffer — O(n) */                      \
static inline void pq_##type##_heapify(borrowed(pq_##type*) h, usize len) {                 \
    pq_heapify(&h->_pq, len);                                                                \
}                                                                                            \
                                                                                             \
/** Inserts val — returns result__Bool_Error */                                               \
static inline result__Bool_Error pq_##type##_push_result(                                    \
    borrowed(pq_##type*) h, type val)                                                        \
{                                                                                            \
    return pq_push_result(&h->_pq, &val);                                                    \
}                                                                                            \
                                                                                             \
/** Removes and returns the top element as option_##type */                                   \
static inline option_##type pq_##type##_pop_option(borrowed(pq_##type*) h) {                \
    type val = {0}; /* zero-init so val is never uninitialized on any path */               \
    if (!pq_pop_raw(&h->_pq, &val)) { return option_##type##_none(); }                      \
    return option_##type##_some(val);                                                        \
}                                                                                            \
                                                                                             \
/** Returns the top element as option_##type without removing it */                           \
static inline option_##type pq_##type##_peek_option(borrowed(const pq_##type*) h) {         \
    const void* raw = pq_peek_raw(&h->_pq);                                                  \
    if (!raw) { return option_##type##_none(); }                                             \
    type val;                                                                                \
    mem_copy(&val, raw, sizeof(type));                                                       \
    return option_##type##_some(val);                                                        \
}                                                                                            \
                                                                                             \
/** Removes element at heap index i — returns result__Bool_Error */                           \
static inline result__Bool_Error pq_##type##_remove_at_result(                              \
    borrowed(pq_##type*) h, usize i)                                                         \
{                                                                                            \
    return pq_remove_at_result(&h->_pq, i);                                                  \
}                                                                                            \
                                                                                             \
/* ── Legacy bool wrappers ──────────────────────────────────────────────── */               \
static inline bool pq_##type##_push(borrowed(pq_##type*) h, type val) {                     \
    return pq_push(&h->_pq, &val);                                                           \
}                                                                                            \
static inline bool pq_##type##_pop(borrowed(pq_##type*) h, type* out) {                   \
    return pq_pop_raw(&h->_pq, out);                                                         \
}                                                                                            \
static inline bool pq_##type##_peek(borrowed(const pq_##type*) h, type* out) {            \
    return pq_peek(&h->_pq, out);                                                            \
}                                                                                            \
static inline bool pq_##type##_remove_at(borrowed(pq_##type*) h, usize i) {                 \
    return pq_remove_at(&h->_pq, i);                                                         \
}                                                                                            \
                                                                                             \
/* ── Queries ────────────────────────────────────────────────────────────── */              \
static inline usize   pq_##type##_len(borrowed(const pq_##type*) h)       { return pq_len(&h->_pq); }      \
static inline usize   pq_##type##_capacity(borrowed(const pq_##type*) h)  { return pq_capacity(&h->_pq); } \
static inline usize   pq_##type##_remaining(borrowed(const pq_##type*) h) { return pq_remaining(&h->_pq); }\
static inline bool    pq_##type##_is_empty(borrowed(const pq_##type*) h)  { return pq_is_empty(&h->_pq); } \
static inline bool    pq_##type##_is_full(borrowed(const pq_##type*) h)   { return pq_is_full(&h->_pq); }  \
static inline bytes_t  pq_##type##_as_bytes(borrowed(pq_##type*) h)        { return pq_as_bytes(&h->_pq); } \
static inline cbytes_t pq_##type##_as_cbytes(borrowed(const pq_##type*) h) { return pq_as_cbytes(&h->_pq); }

#endif /* CANON_DATA_PRIORITY_QUEUE_H */
