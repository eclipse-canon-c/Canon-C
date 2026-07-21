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

#ifndef CANON_CORE_REGION_H
#define CANON_CORE_REGION_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/primitives/lifetime.h"  /* region_id_t, REGION_ID_STATIC, lifetime_t */
#include "core/arena.h"

/**
 * @file core/region.h
 * @brief Explicit lifetime region tokens — named boundaries for borrowed values
 *
 * A Region is a stack-allocated lifetime token. It answers:
 * "how long is this borrowed value valid?"
 *
 * Borrowed views (core/slice.h, semantics/borrow.h) carry a source tag that
 * identifies *what* owns the data. A region_id_t identifies *how long* it lives.
 *
 * Usage:
 * ────────────────────────────────────────────────────────────────────────────
 *   Region r;
 *   region_begin(&r);
 *   // ... work ...
 *   region_end(&r);
 *
 * The pointer-based API is intentional: &r is the caller's actual stack address,
 * which is used as the region's unique ID. A by-value region_begin() would
 * capture the callee's frame address — a different, immediately-dead pointer.
 *
 * Core properties:
 * ────────────────────────────────────────────────────────────────────────────
 * - Stack-allocated — never heap-allocated, never dynamically sized
 * - ID derived from the Region's own stack address — no global counter,
 *   no shared state, fully thread-safe
 * - Optional arena attachment — arena_reset() called on region_end()
 * - Up to REGION_MAX_CLEANUP cleanup hooks per region, called LIFO
 * - Nesting is explicit and caller-managed — no implicit propagation
 * - No global state anywhere in this file
 *
 * LIFO cleanup is region.h's own mechanism:
 * ────────────────────────────────────────────────────────────────────────────
 * Region maintains its own runtime array of {fn, ctx} cleanup hooks
 * registered via region_register() and walked in reverse order by
 * region_end(). This is independent of core/scope.h — region's LIFO
 * ordering is a runtime property of the hook array, not a compile-time
 * property of any macro. The two headers solve different problems:
 *
 *   core/scope.h  — inline cleanup expression paired with a single block,
 *                   zero runtime state, fires on normal exit and continue.
 *
 *   core/region.h — runtime-registered multi-hook cleanup, arena attachment,
 *                   lifetime ID for borrow validation, fires unconditionally
 *                   on region_end() regardless of how the caller reached it.
 *
 * Generalized lifetime mechanism — see lifetime_t in core/primitives/lifetime.h:
 * ────────────────────────────────────────────────────────────────────────────
 * Region's (id, open) pair is the prototype for a wider lifetime-tracking
 * mechanism used by other Canon-C modules (arena, pool, dynvec, hashmap,
 * stringbuf) under CANON_LIFETIME_DEBUG. Those modules embed lifetime_t
 * directly so that borrows can validate against any source via a single
 * const lifetime_t* pointer.
 *
 * Region itself does NOT embed lifetime_t — its id/open/arena/cleanups
 * fields are layout-frozen by the existing test suite and refactoring
 * them would be churn for no gain. lifetime_assert_valid() takes (id,
 * open) values directly, so Region's separate fields and other modules'
 * embedded lifetime_t are interoperable at the assertion site.
 *
 * What region is NOT:
 * ────────────────────────────────────────────────────────────────────────────
 * - Not an allocator (use core/arena.h)
 * - Not automatic — region_end() must be called explicitly on every exit
 *   path, or wrapped in a goto-cleanup block. For functions with early
 *   returns, use the goto cleanup pattern (see core/scope.h documentation).
 * - Not compiler-enforced (C99 — semantic contract, not structural)
 * - Not thread-local (stack-local; don't share across threads)
 * - Not a garbage collector
 *
 * ID design — address-based, no global counter:
 * ────────────────────────────────────────────────────────────────────────────
 *   r.id = (region_id_t)(uintptr_t)r   // r is the caller's Region*
 *
 * Properties:
 * - Unique across all simultaneously live regions (distinct stack frames)
 * - Not monotonic — addresses, not sequence numbers
 * - Safe alias across sequential lifetimes: region_assert_borrow_valid()
 *   checks r->open in addition to the ID, so a reused address on a closed
 *   region correctly fails the open check
 * - No race conditions — each thread's stack is independent
 * - No atomic operations — no synchronization needed
 * - Compliant with DO-178C / ISO 26262 (no global state)
 *
 * Build flags:
 * ────────────────────────────────────────────────────────────────────────────
 * REGION_MAX_CLEANUP (default 8)
 *   Max cleanup hooks per Region. sizeof(Region) grows with this.
 *   #define REGION_MAX_CLEANUP 4  // before including this header
 *
 * CANON_NO_REGION_PARENT
 *   Strips the parent pointer field. Smaller struct, simpler layout.
 *   region_set_parent() and region_has_parent() are removed/stubbed.
 *   Use for certified builds where parent tracking serves no mechanical role.
 *   #define CANON_NO_REGION_PARENT  // before including this header
 *
 * CANON_STRICT (propagated from contract.h)
 *   Promotes ensure_msg() to always-on. region_assert_*() become
 *   always-active in certified builds without any changes here.
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * region.h is in core/ and includes only core/primitives/ and core/arena.h.
 * The shared lifetime types (region_id_t, REGION_ID_STATIC, lifetime_t)
 * come from core/primitives/lifetime.h, which is also included by
 * core/arena.h under CANON_LIFETIME_DEBUG. lifetime_assert_valid lives
 * here in region.h because it depends on require_msg from contract.h.
 * No data/, semantics/, algo/, or util/ headers may be included here.
 *
 * Thread safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Regions are stack-local. region_begin() touches no shared state.
 * Do not share a Region* across threads. A Region must not be accessed
 * after its owner's stack frame exits.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - region_begin:        O(1) — memset + address cast
 * - region_end:          O(k) — k = registered hooks
 * - region_attach_arena: O(1)
 * - region_register:     O(1)
 * - region_is_open:      O(1)
 * - region_assert_*:     O(1) — debug-only by default, always-on with CANON_STRICT
 * - lifetime_assert_valid: O(1) — always-on; disabled only by CANON_NO_REQUIRE
 * - sizeof(Region):      fixed — no dynamic sizing
 *
 * Verification (Frama-C WP):
 * ────────────────────────────────────────────────────────────────────────────
 * Every function in this file carries an ACSL contract. region_invariant()
 * is the central predicate (validity, hook-count bound, arena validity);
 * it composes arena_invariant() from core/arena.h. region_end() is the one
 * function whose verification has an inherent boundary: it dispatches
 * caller-supplied cleanup hooks through an opaque function pointer, which
 * WP cannot reason about without a `calls` clause that the arbitrary-hook
 * design cannot supply. region_end() declares `assigns *r`; the structural
 * postconditions are re-established by unconditional writes *after* the
 * hook loop. WP cannot prove the indirect hook call respects the `*r`
 * frame (a hook could, as far as WP knows, alias r), so that call is the
 * documented region.h-own residual (see docs/deviations.md VERIFY-011 /
 * OWN-003 and the report-only WP step in .github/workflows/cmake-multi-platform.yml).
 *
 * Quick start:
 * ────────────────────────────────────────────────────────────────────────────
 * @code
 *   // Basic lifetime boundary
 *   Region r;
 *   region_begin(&r);
 *   // ... work ...
 *   region_end(&r);
 *
 *   // Arena-attached — arena reset on end
 *   Region r;
 *   region_begin(&r);
 *   region_attach_arena(&r, &scratch);
 *   void* buf = arena_alloc(&scratch, 256);
 *   region_end(&r); // arena_reset(&scratch) called automatically
 *
 *   // Stamped borrow validity check
 *   region_id_t rid = region_id(&r);
 *   // ... pass rid alongside borrowed pointer ...
 *   region_assert_borrow_valid(&r, rid); // debug: still open?
 *
 *   // Cleanup hook
 *   void release_lock(void* ctx) { mutex_unlock((Mutex*)ctx); }
 *   region_register(&r, release_lock, &my_mutex);
 *
 *   // Nested regions
 *   Region outer, inner;
 *   region_begin(&outer);
 *   region_begin(&inner);
 *   region_set_parent(&inner, &outer); // informational only
 *   region_end(&inner);
 *   region_end(&outer);
 *
 *   // Combining region with DEFER for run-to-completion blocks:
 *   // DEFER fires region_end() on the block's normal exit. Do not use
 *   // this pattern for functions that may return or goto out of the
 *   // block on error paths — DEFER does not cover those exits, and
 *   // region_end() would be silently skipped. For error-handled
 *   // functions, use the goto cleanup pattern instead.
 *   Region r;
 *   region_begin(&r);
 *   DEFER(region_end(&r)) {
 *       region_attach_arena(&r, &scratch);
 *       // ... run-to-completion work ...
 *   }
 *
 *   // For functions with error paths, use goto cleanup:
 *   Region r;
 *   int    rc = 0;
 *   region_begin(&r);
 *   region_attach_arena(&r, &scratch);
 *
 *   if (step_one(&r)   != OK) { rc = ERR_ONE;   goto done; }
 *   if (step_two(&r)   != OK) { rc = ERR_TWO;   goto done; }
 *   if (step_three(&r) != OK) { rc = ERR_THREE; goto done; }
 *
 * done:
 *   region_end(&r);
 *   return rc;
 * @endcode
 *
 * @sa core/primitives/lifetime.h — canonical home of region_id_t and lifetime_t
 * @sa core/arena.h               — bump allocator; attach to a region for scoped allocation
 * @sa core/slice.h               — bytes_t / str_t used with region lifetimes
 * @sa core/scope.h               — DEFER(expr) { body } for inline run-to-completion
 *                                  cleanup; complementary to region, not a replacement
 */

/* ════════════════════════════════════════════════════════════════════════════
   Configuration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Maximum cleanup hooks per Region (default: 8)
 *
 * Each hook is a {fn, ctx} pair called LIFO on region_end().
 * sizeof(Region) grows linearly with this value.
 * Define before including this header to override.
 */
#ifndef REGION_MAX_CLEANUP
    #define REGION_MAX_CLEANUP 8
#endif

/* ════════════════════════════════════════════════════════════════════════════
   lifetime_assert_valid
   ════════════════════════════════════════════════════════════════════════════
   The three lifetime types (region_id_t, REGION_ID_STATIC, lifetime_t)
   come from core/primitives/lifetime.h, included at the top of this file.
   lifetime_assert_valid lives here in region.h because it depends on
   require_msg from contract.h — keeping it in primitives/ would force
   primitives/lifetime.h to include contract.h, which is heavier than
   the rest of the header warrants.

   Gating discipline — IMPORTANT:
   ──────────────────────────────────────────────────────────────────────────
   lifetime_assert_valid uses require_msg (always-on), NOT ensure_msg
   (NDEBUG-gated). The two assertion macros in region.h are deliberately
   gated differently:

     region_assert_open, region_assert_borrow_valid — ensure_msg
       These existed before CANON_LIFETIME_DEBUG and have always been
       NDEBUG-gated like assert(). Preserving that gating is the right
       call for the Region API contract from before lifetime tracking.

     lifetime_assert_valid                          — require_msg
       This is new infrastructure built specifically for the
       CANON_LIFETIME_DEBUG opt-in path. The user has already paid the
       cost of opting in (struct layout changes, additional code paths,
       extra fields on every borrow). Compiling out the actual check in
       Release would defeat the entire point of the flag — you would
       pay the cost of tracking and get none of the safety. The flag's
       contract is "if you set CANON_LIFETIME=debug, the checks fire,"
       period. require_msg honors that contract in Release as well as
       Debug. The escape hatch for formally-verified builds (Frama-C,
       SPARK) is CANON_NO_REQUIRE, which compiles out require_msg
       library-wide; that is the right place to disable this check
       since it disables all the other call-site preconditions too.

   See the CI history that led to this design choice: when
   lifetime_assert_valid was originally written with ensure_msg, the
   CANON_LIFETIME=debug + Release configuration silently no-op'd the
   check on every platform, making borrow_test's
   test_lifetime_get_after_reset_fires fail to fire across the entire
   lifetime-debug matrix expansion.
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Asserts that a borrow stamped with borrow_id is still valid
 *        against a (id, open) lifetime pair.
 *
 * Generic, type-agnostic version of region_assert_borrow_valid for
 * modules that embed lifetime_t (or two equivalent fields) without
 * embedding a full Region.
 *
 * Always-on regardless of NDEBUG. Disabled only by CANON_NO_REQUIRE,
 * which is intended for builds where formal verification has proved
 * all call sites safe and the runtime check is no longer needed.
 *
 * @param source_id   Lifetime ID currently held by the source
 * @param source_open Whether the source is currently open
 * @param borrow_id   ID stamped on the borrow at creation
 * @param site        Diagnostic name for require_msg failure messages
 *
 * Complexity: O(1)
 */
/*@
  assigns \nothing;
*/
static inline void lifetime_assert_valid(
    region_id_t source_id,
    bool        source_open,
    region_id_t borrow_id,
    const char* site)
{
    if (borrow_id == REGION_ID_STATIC) { return; }
    require_msg(source_open, site);
    require_msg(source_id == borrow_id, site);
    /* Suppress unused-parameter warnings when require_msg compiles away
     * (CANON_NO_REQUIRE defined). The parameters are genuinely used in
     * default builds. */
    (void)source_id;
    (void)source_open;
    (void)site;
}

/* ════════════════════════════════════════════════════════════════════════════
   RegionCleanup
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief A single registered cleanup hook
 *
 * fn  — cleanup function; receives ctx; must not call region_end()
 * ctx — caller-provided context (may be NULL)
 */
typedef struct {
    void (*fn)(void* ctx);
    void* ctx;
} RegionCleanup;

/* ════════════════════════════════════════════════════════════════════════════
   Region
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief A named lifetime token — stack-allocated, never heap-allocated
 *
 * Fields (do not access directly — use the provided functions):
 *   id        — stack-address-derived ID, set by region_begin()
 *   open      — true between region_begin() and region_end()
 *   arena     — optional attached arena, reset on region_end() (may be NULL)
 *   parent    — optional parent region (absent with CANON_NO_REGION_PARENT)
 *   cleanups  — registered hooks, called LIFO on region_end()
 *   num_hooks — number of registered hooks
 *
 * Invariants:
 *   id != REGION_ID_STATIC after region_begin()
 *   open == true  between begin and end
 *   open == false after region_end()
 *   num_hooks <= REGION_MAX_CLEANUP
 */
typedef struct Region Region;
struct Region {
    region_id_t   id;
    bool          open;
    Arena*        arena;
#ifndef CANON_NO_REGION_PARENT
    Region*       parent;
#endif
    RegionCleanup cleanups[REGION_MAX_CLEANUP];
    usize         num_hooks;
};

/* ════════════════════════════════════════════════════════════════════════════
   region_invariant — central ACSL predicate
   ════════════════════════════════════════════════════════════════════════════
   Mirrors the role arena_invariant / pool_invariant play in their headers:
   a single named predicate carried as a precondition by every function that
   takes a live Region, and re-established as a postcondition by every
   mutating function. It composes arena_invariant from core/arena.h for the
   optional attached arena. The hook count is bounded by REGION_MAX_CLEANUP
   so the region_end() walk and region_register() store are in-bounds.
   ════════════════════════════════════════════════════════════════════════════ */

/*@
  predicate region_invariant(Region *r) =
      \valid(r)
      && 0 <= r->num_hooks <= REGION_MAX_CLEANUP
      && (r->arena == \null || arena_invariant(r->arena));
*/

/* ════════════════════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Opens a Region and assigns it a unique ID
 *
 * The ID is derived from r's own address — no global counter, no shared
 * state, fully thread-safe. Must be paired with region_end().
 *
 * @param r  Caller's Region (must not be NULL)
 *
 * @pre  r != NULL
 * @post r->open == true
 * @post r->id   == (region_id_t)(uintptr_t)r  (never REGION_ID_STATIC)
 * @post r->arena     == NULL
 * @post r->num_hooks == 0
 *
 * Complexity: O(1)
 */
/*@
  requires \valid(r);
  assigns  *r;
  ensures  r->open == \true;
  ensures  r->arena == \null;
  ensures  r->num_hooks == 0;
  ensures  r->id == (region_id_t)(uintptr_t)r;
  ensures  region_invariant(r);
*/
static inline void region_begin(Region* r) {
    require_msg(r != NULL, "region_begin: r cannot be NULL");
    *r      = (Region){0};
    r->id   = (region_id_t)(uintptr_t)r;
    r->open = true;
}

/**
 * @brief Closes a Region — calls cleanup hooks LIFO, resets arena
 *
 * After this call r->open == false. r->id is preserved for post-mortem
 * inspection but the region is considered dead.
 *
 * @param r  Open Region to close (must not be NULL)
 *
 * @pre  r != NULL
 * @pre  r->open == true
 * @post r->open == false
 * @post All hooks called and cleared
 * @post r->arena reset and cleared (if was non-NULL)
 * @post r->num_hooks == 0
 *
 * Complexity: O(num_hooks)
 *
 * Verification note: the attached arena pointer is captured into a local
 * BEFORE the hook loop, and r->arena is cleared unconditionally AFTER it.
 * Functionally identical to clearing inside the `if` for any conforming use
 * (hooks may allocate from the arena but must not repoint r->arena), and it
 * lets WP discharge the arena_reset call against the captured (loop-immune)
 * local rather than against a field the opaque hooks have havoc'd.
 */
/*@
  requires region_invariant(r);
  requires r->open;
  assigns  *r;
  ensures  r->open == \false;
  ensures  r->num_hooks == 0;
  ensures  r->arena == \null;
  ensures  region_invariant(r);
*/
static inline void region_end(Region* r) {
    usize          i;
    RegionCleanup* h;
    Arena*         saved_arena;

    require_msg(r != NULL, "region_end: r cannot be NULL");
    require_msg(r->open,   "region_end: region is already closed");

    /* Capture before the hooks run: a non-NULL value satisfies
     * arena_invariant by region_invariant, and a local is immune to any
     * memory the opaque hook dispatch below may touch. */
    saved_arena = r->arena;

    /* Hooks — LIFO.
     * h->fn(h->ctx) is an indirect call through a caller-supplied pointer.
     * WP has no `calls` clause to reason about it (hooks are arbitrary).
     * region_end's declared frame is `assigns *r`; the indirect call may,
     * for all WP can prove, write through a pointer aliasing r, so the
     * call's conformance to that frame is the documented region.h-own
     * residual (VERIFY-011 / OWN-003). The loop annotation below bounds the
     * loop's own writes (the counter and the cleanups array) so the array-
     * index RTE and termination discharge; the function's structural
     * postconditions are re-established by the unconditional writes after
     * the loop. */
    /*@
      loop invariant 0 <= i <= REGION_MAX_CLEANUP;
      loop assigns  i, r->cleanups[0 .. REGION_MAX_CLEANUP - 1];
      loop variant  i;
    */
    for (i = r->num_hooks; i > 0u; i--) {
        h = &r->cleanups[i - 1u];
        if (h->fn) {
            /* h->fn is an opaque caller-supplied pointer with no `calls`
             * clause. WP assumes it could call region_end itself, so it
             * models region_end as potentially recursive (hence the
             * Missing-decreases / non-terminating-call warnings). A
             * statement-level `assigns` contract here is NOT honored —
             * WP reports "Statement specifications not yet supported
             * (skipped)". The honest boundary is documented as the
             * region.h-own residual (VERIFY-011 / OWN-003); the
             * report-only WP step samples it with a short per-goal
             * timeout rather than discharging it. The hook contract
             * (a hook must not call region_end on this region or repoint
             * r's fields) is what makes the modelled recursion vacuous. */
            h->fn(h->ctx);
            h->fn  = NULL;
            h->ctx = NULL;
        }
    }
    r->num_hooks = 0;

    /* Reset attached arena (after hooks — hooks may still allocate from it) */
    if (saved_arena) {
        arena_reset(saved_arena);
    }
    r->arena = NULL;

    r->open = false;
}

/* ════════════════════════════════════════════════════════════════════════════
   Arena attachment
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Attaches an arena — arena_reset() will be called on region_end()
 *
 * Only one arena per region. Attaching a second replaces the first;
 * the first is NOT reset at replacement time.
 *
 * @param r      Open Region (must not be NULL)
 * @param arena  Arena to attach (must not be NULL)
 *
 * @pre  r != NULL && r->open
 * @pre  arena != NULL
 * @post r->arena == arena
 *
 * Complexity: O(1)
 */
/*@
  requires region_invariant(r);
  requires r->open;
  requires arena_invariant(arena);
  assigns  r->arena;
  ensures  r->arena == arena;
  ensures  region_invariant(r);
*/
static inline void region_attach_arena(Region* r, Arena* arena) {
    require_msg(r != NULL,     "region_attach_arena: r cannot be NULL");
    require_msg(r->open,       "region_attach_arena: region is not open");
    require_msg(arena != NULL, "region_attach_arena: arena cannot be NULL");
    r->arena = arena;
}

/* ════════════════════════════════════════════════════════════════════════════
   Cleanup registration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Registers a cleanup hook — called LIFO on region_end()
 *
 * Returns false if the hook table is full (num_hooks == REGION_MAX_CLEANUP).
 * Increase REGION_MAX_CLEANUP if needed.
 *
 * @param r    Open Region (must not be NULL)
 * @param fn   Cleanup function — void fn(void* ctx); must not be NULL;
 *             must not call region_end() on this region
 * @param ctx  Context passed to fn (may be NULL)
 * @return     true on success, false if table is full
 *
 * @pre  r != NULL && r->open
 * @pre  fn != NULL
 *
 * Complexity: O(1)
 */
/*@
  requires region_invariant(r);
  requires r->open;
  requires fn != \null;
  assigns  r->cleanups[0 .. REGION_MAX_CLEANUP - 1], r->num_hooks;
  ensures  region_invariant(r);
  behavior full:
    assumes r->num_hooks >= REGION_MAX_CLEANUP;
    ensures \result == \false;
    ensures r->num_hooks == \old(r->num_hooks);
  behavior has_space:
    assumes r->num_hooks < REGION_MAX_CLEANUP;
    ensures \result == \true;
    ensures r->num_hooks == \old(r->num_hooks) + 1;
    ensures r->cleanups[\old(r->num_hooks)].fn  == fn;
    ensures r->cleanups[\old(r->num_hooks)].ctx == ctx;
  complete behaviors;
  disjoint behaviors;
*/
static inline bool region_register(Region* r, void (*fn)(void* ctx), void* ctx) {
    require_msg(r != NULL,  "region_register: r cannot be NULL");
    require_msg(r->open,    "region_register: region is not open");
    require_msg(fn != NULL, "region_register: fn cannot be NULL");

    if (r->num_hooks >= (usize)REGION_MAX_CLEANUP) {
        return false;
    }

    r->cleanups[r->num_hooks].fn  = fn;
    r->cleanups[r->num_hooks].ctx = ctx;
    r->num_hooks++;
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Parent tracking — disabled with CANON_NO_REGION_PARENT
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_NO_REGION_PARENT

/**
 * @brief Sets the parent region (informational — parent is never auto-ended)
 *
 * Expresses nesting intent. region.h never dereferences parent for automatic
 * propagation — that is always the caller's responsibility. Useful for
 * diagnostic tools that walk the region tree.
 *
 * Absent when CANON_NO_REGION_PARENT is defined.
 *
 * @param r       Child region (must not be NULL, must be open)
 * @param parent  Parent region (must not be NULL, must be open)
 *
 * @pre  r != NULL && r->open
 * @pre  parent != NULL && parent->open
 *
 * Complexity: O(1)
 */
/*@
  requires region_invariant(r);
  requires r->open;
  requires \valid_read(parent);
  requires parent->open;
  assigns  r->parent;
  ensures  r->parent == parent;
  ensures  region_invariant(r);
*/
static inline void region_set_parent(Region* r, Region* parent) {
    require_msg(r != NULL,      "region_set_parent: r cannot be NULL");
    require_msg(r->open,        "region_set_parent: region is not open");
    require_msg(parent != NULL, "region_set_parent: parent cannot be NULL");
    require_msg(parent->open,   "region_set_parent: parent is not open");
    r->parent = parent;
}

#endif /* CANON_NO_REGION_PARENT */

/* ════════════════════════════════════════════════════════════════════════════
   Inspection
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the region's unique ID
 *
 * Returns REGION_ID_STATIC (0) if r == NULL.
 *
 * Complexity: O(1)
 */
/*@
  requires r == \null || \valid_read(r);
  assigns  \nothing;
  behavior null:
    assumes r == \null;
    ensures \result == REGION_ID_STATIC;
  behavior nonnull:
    assumes r != \null;
    ensures \result == r->id;
  complete behaviors;
  disjoint behaviors;
*/
static inline region_id_t region_id(const Region* r) {
    return r ? r->id : REGION_ID_STATIC;
}

/**
 * @brief Returns true if the region is currently open
 *
 * Returns false if r == NULL.
 *
 * Complexity: O(1)
 */
/*@
  requires r == \null || \valid_read(r);
  assigns  \nothing;
  ensures  \result <==> (r != \null && r->open);
*/
static inline bool region_is_open(const Region* r) {
    return (r != NULL) && r->open;
}

/**
 * @brief Returns true if the region has a parent set
 *
 * Always returns false when CANON_NO_REGION_PARENT is defined.
 *
 * Complexity: O(1)
 */
#ifdef CANON_NO_REGION_PARENT
/*@
  requires r == \null || \valid_read(r);
  assigns  \nothing;
  ensures  \result == \false;
*/
#else
/*@
  requires r == \null || \valid_read(r);
  assigns  \nothing;
  ensures  \result <==> (r != \null && r->parent != \null);
*/
#endif
static inline bool region_has_parent(const Region* r) {
#ifdef CANON_NO_REGION_PARENT
    (void)r;
    return false;
#else
    return (r != NULL) && (r->parent != NULL);
#endif
}

/**
 * @brief Returns the number of registered cleanup hooks
 *
 * Returns 0 if r == NULL.
 *
 * Complexity: O(1)
 */
/*@
  requires r == \null || \valid_read(r);
  assigns  \nothing;
  behavior null:
    assumes r == \null;
    ensures \result == 0;
  behavior nonnull:
    assumes r != \null;
    ensures \result == r->num_hooks;
  complete behaviors;
  disjoint behaviors;
*/
static inline usize region_hook_count(const Region* r) {
    return r ? r->num_hooks : 0u;
}

/* ════════════════════════════════════════════════════════════════════════════
   Lifetime assertions
   ════════════════════════════════════════════════════════════════════════════
   Default:      debug builds only — compiled away under NDEBUG
   CANON_STRICT: always-on — propagated automatically from contract.h
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Asserts that the region is still open
 *
 * Call before using a borrowed value stamped with this region's ID.
 * Compiled away under NDEBUG unless CANON_STRICT is defined.
 *
 * @param r  Region to check (must not be NULL)
 *
 * Complexity: O(1)
 */
/*@
  requires r == \null || \valid_read(r);
  assigns  \nothing;
*/
static inline void region_assert_open(const Region* r) {
    ensure_msg(r != NULL, "region_assert_open: r cannot be NULL");
    ensure_msg(r->open,   "region_assert_open: region is closed — borrow may be invalid");
    /* Suppress unused-parameter warning when ensure_msg compiles away (NDEBUG
     * without CANON_STRICT). The parameter is genuinely used in debug builds. */
    (void)r;
}

/**
 * @brief Asserts that a borrow stamped with borrow_rid is still valid
 *
 * A borrow is valid if:
 *   - borrow_rid == REGION_ID_STATIC (static lifetime — always valid), OR
 *   - the region is open AND r->id == borrow_rid
 *
 * Compiled away under NDEBUG unless CANON_STRICT is defined.
 *
 * @param r          Region that owns the borrowed data (must not be NULL)
 * @param borrow_rid ID stamped on the borrow at creation time
 *
 * Complexity: O(1)
 */
/*@
  requires r == \null || \valid_read(r);
  assigns  \nothing;
*/
static inline void region_assert_borrow_valid(const Region* r, region_id_t borrow_rid) {
    if (borrow_rid == REGION_ID_STATIC) {
        return; /* static lifetime — always valid, no check needed */
    }
    ensure_msg(r != NULL,           "region_assert_borrow_valid: r cannot be NULL");
    ensure_msg(r->open,             "region_assert_borrow_valid: region is closed");
    ensure_msg(r->id == borrow_rid, "region_assert_borrow_valid: ID mismatch — wrong region");
    /* Suppress unused-parameter warning when ensure_msg compiles away (NDEBUG
     * without CANON_STRICT). The parameter is genuinely used in debug builds. */
    (void)r;
}

/* ════════════════════════════════════════════════════════════════════════════
   Note on automatic region_end

   Earlier versions of this header provided a REGION_SCOPE(name) macro that
   declared a Region and relied on core/scope.h's old SCOPE_DEFER { block }
   form to call region_end() at scope exit. Both the macro it depended on
   and the implicit semantics it promised have been removed:

     1. scope.h's SCOPE_DEFER { block } form could not deliver scope-exit
        semantics in pure C99 (cleanup ran at declaration, not at exit).
        It has been replaced with DEFER(expr) { body }, which delivers
        correct scope-exit semantics for normal fall-through and continue,
        but NOT for break, return, or outward goto.

     2. region_end() is load-bearing — it calls registered hooks, resets
        the attached arena, and invalidates all borrows stamped with this
        region's ID. Silently skipping it on an early return is exactly
        the kind of bug region.h exists to prevent.

     3. A REGION_SCOPE macro built on DEFER would silently inherit DEFER's
        exit-method limits, meaning region_end() would not fire on any
        early return or outward goto from inside the scope. This is the
        same category of bug the old header suffered from, in new clothes.

   The honest replacement is explicit. Call region_begin() and region_end()
   directly. For functions where the region's body runs to completion,
   wrap region_end() in a DEFER so the pairing is lexically visible:

       Region r;
       region_begin(&r);
       DEFER(region_end(&r)) {
           // run-to-completion work
       }

   For functions with error-return paths, use the goto cleanup pattern.
   region_end() at the bottom of the function in a single cleanup block
   is the auditable, certification-friendly shape, and it fires on every
   exit path including returns:

       Region r;
       int    rc = 0;
       region_begin(&r);

       if (step_one(&r)   != OK) { rc = ERR_ONE;   goto done; }
       if (step_two(&r)   != OK) { rc = ERR_TWO;   goto done; }
       rc = step_three(&r);

   done:
       region_end(&r);
       return rc;

   Both patterns are first-class in Canon-C. Use the one that matches
   the function's exit structure.
   ════════════════════════════════════════════════════════════════════════════ */

#endif /* CANON_CORE_REGION_H */
