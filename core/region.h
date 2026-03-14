#ifndef CANON_CORE_REGION_H
#define CANON_CORE_REGION_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/arena.h"

/**
 * @file core/region.h
 * @brief Explicit lifetime region tokens — named boundaries for borrow validity
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
 * What region is NOT:
 * ────────────────────────────────────────────────────────────────────────────
 * - Not an allocator (use core/arena.h)
 * - Not automatic RAII (use core/scope.h for that pattern)
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
 * - sizeof(Region):      fixed — no dynamic sizing
 *
 * Quick start:
 * ────────────────────────────────────────────────────────────────────────────
 * @code
 *   // Basic lifetime boundary
 *   Region r;
 *   region_begin(&r);
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
 * @endcode
 *
 * @sa core/arena.h   — bump allocator; attach to a region for scoped allocation
 * @sa core/slice.h   — bytes_t / str_t used with region lifetimes
 * @sa core/scope.h   — RAII-style cleanup for simpler single-resource cases
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
   Region ID
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Unique identifier for a Region lifetime
 *
 * Derived from the Region's stack address in region_begin().
 * Unique across all simultaneously live regions.
 * Not monotonic — addresses, not sequence numbers.
 *
 * 0 is reserved as REGION_ID_STATIC ("no region" / static lifetime).
 * A valid Region will never have ID 0: no stack object has address 0
 * on any conforming C99 implementation.
 */
typedef u64 region_id_t;

/** Reserved ID — "no region" / static lifetime. Borrows with this ID never expire. */
#define REGION_ID_STATIC ((region_id_t)0)

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
static inline void region_begin(Region* r) {
    require_msg(r != NULL, "region_begin: r cannot be NULL");
    /* Zero-initialize all fields, then set id and open.
     * Casting through a char* loop is C99-portable and avoids
     * a memset dependency; compilers reduce this to a single instruction. */
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
 */
static inline void region_end(Region* r) {
    usize          i;
    RegionCleanup* h;

    require_msg(r != NULL, "region_end: r cannot be NULL");
    require_msg(r->open,   "region_end: region is already closed");

    /* Hooks — LIFO */
    for (i = r->num_hooks; i > 0; i--) {
        h = &r->cleanups[i - 1];
        if (h->fn) {
            h->fn(h->ctx);
            h->fn  = NULL;
            h->ctx = NULL;
        }
    }
    r->num_hooks = 0;

    /* Reset attached arena (after hooks — hooks may still allocate from it) */
    if (r->arena) {
        arena_reset(r->arena);
        r->arena = NULL;
    }

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
static inline bool region_is_open(const Region* r) {
    return r != NULL && r->open;
}

/**
 * @brief Returns true if the region has a parent set
 *
 * Always returns false when CANON_NO_REGION_PARENT is defined.
 *
 * Complexity: O(1)
 */
static inline bool region_has_parent(const Region* r) {
#ifdef CANON_NO_REGION_PARENT
    (void)r;
    return false;
#else
    return r != NULL && r->parent != NULL;
#endif
}

/**
 * @brief Returns the number of registered cleanup hooks
 *
 * Returns 0 if r == NULL.
 *
 * Complexity: O(1)
 */
static inline usize region_hook_count(const Region* r) {
    return r ? r->num_hooks : 0;
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
static inline void region_assert_open(const Region* r) {
    ensure_msg(r != NULL, "region_assert_open: r cannot be NULL");
    ensure_msg(r->open,   "region_assert_open: region is closed — borrow may be invalid");
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
static inline void region_assert_borrow_valid(const Region* r, region_id_t borrow_rid) {
    if (borrow_rid == REGION_ID_STATIC) {
        return; /* static lifetime — always valid, no check needed */
    }
    ensure_msg(r != NULL,           "region_assert_borrow_valid: r cannot be NULL");
    ensure_msg(r->open,             "region_assert_borrow_valid: region is closed");
    ensure_msg(r->id == borrow_rid, "region_assert_borrow_valid: ID mismatch — wrong region");
}

/* ════════════════════════════════════════════════════════════════════════════
   REGION_SCOPE — optional automatic region_end via scope.h
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def REGION_SCOPE(name)
 * @brief Declares a Region that is automatically ended at scope exit
 *
 * Requires scope.h (SCOPE_DEFER) to be included first.
 * If scope.h is absent, expands to region_begin(&name) only —
 * region_end() must be called manually.
 *
 * Thread-safe: region_begin() uses the stack address as ID.
 *
 * @param name  Variable name for the Region
 *
 * @code
 *   #include "core/scope.h"
 *   #include "core/region.h"
 *
 *   void my_function(void) {
 *       REGION_SCOPE(r);
 *       region_attach_arena(&r, &scratch);
 *       // ... work ...
 *   } // region_end(&r) called automatically
 * @endcode
 */
#ifdef CANON_CORE_SCOPE_H
    #define REGION_SCOPE(name) \
        Region name;           \
        region_begin(&name);   \
        SCOPE_DEFER(region_end(&name))
#else
    #define REGION_SCOPE(name) \
        Region name;           \
        region_begin(&name)    /* remember: call region_end(&name) manually */
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Build configuration summary
   ════════════════════════════════════════════════════════════════════════════

   Development (default):
     Assertions:      debug only (NDEBUG disables ensure_msg)
     Parent tracking: enabled
     gcc -o myapp main.c

   Release:
     Assertions:      compiled away
     Parent tracking: enabled
     gcc -DNDEBUG -o myapp main.c

   Certified (DO-178C / ISO 26262 / IEC 61508):
     Assertions:      always-on via CANON_STRICT
     Parent tracking: stripped via CANON_NO_REGION_PARENT
     Struct layout:   minimal and fully provable
     gcc -DCANON_STRICT -DCANON_NO_REGION_PARENT -o myapp main.c

   ════════════════════════════════════════════════════════════════════════════ */

#endif /* CANON_CORE_REGION_H */
