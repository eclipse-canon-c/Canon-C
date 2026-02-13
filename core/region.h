#ifndef CANON_CORE_REGION_H
#define CANON_CORE_REGION_H

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/arena.h"
#include "core/ownership.h"

/**
 * @file core/region.h
 * @brief Explicit lifetime region tokens — named boundaries for borrow validity
 *
 * A region is a named lifetime boundary. It answers the question:
 * "how long is this borrowed value valid?"
 *
 * Canon-C's borrow system (core/slice.h, semantics/borrow.h) gives you
 * non-owning views with a `source` tag. But source is just a void* address —
 * it tells you *what* owns the data, not *how long* it lives.
 *
 * Region fills that gap:
 *   - A Region token represents a lifetime scope
 *   - Borrowed views can be stamped with a region ID
 *   - At region_end(), all borrows stamped with that region become invalid
 *   - debug builds can assert that a borrow's region is still open
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - A Region is a stack-allocated struct — never heap-allocated
 * - Regions are identified by a unique monotonic ID (u64)
 * - Region tokens are independent of arenas by default
 * - An arena can be attached to a region — arena_reset() is called on region_end()
 * - Up to REGION_MAX_CLEANUP hooks can be registered per region
 * - Nesting is explicit — parent/child regions are tracked by the caller
 * - No automatic propagation, no thread-local state, no global registry
 *
 * What region is NOT:
 * ────────────────────────────────────────────────────────────────────────────
 * - Not a second allocator (use arena for allocation)
 * - Not automatic cleanup (use scope.h for RAII patterns)
 * - Not enforced by the compiler (C99 — semantic, not structural)
 * - Not thread-local (each region lives on its owner's stack)
 * - Not a garbage collector
 *
 * Relationship to other modules:
 * ────────────────────────────────────────────────────────────────────────────
 * - core/arena.h       — optional arena attachment; region_end() resets it
 * - core/ownership.h   — owned/borrowed vocabulary; region extends it
 * - core/slice.h       — borrowed views carry a region_id for validity checks
 * - semantics/borrow.h — borrowed_str / borrowed_bytes can carry region_id
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * region.h lives in core/ and may include core/primitives/ and core/arena.h
 * and core/ownership.h only.
 * No data/, semantics/, or util/ headers may be included here.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - No platform-specific code
 * - No dynamic allocation anywhere in this file
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Regions are stack-local and not thread-safe by design.
 * Do not share Region pointers across threads without external synchronization.
 * The global region ID counter uses a simple increment — not atomic.
 * For multi-threaded code, use one region per thread.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - region_begin:          O(1)
 * - region_end:            O(k) where k = number of registered cleanup hooks
 * - region_attach_arena:   O(1)
 * - region_register:       O(1)
 * - region_is_open:        O(1)
 * - region_assert_valid:   O(1) — debug only, no-op in release
 * - sizeof(Region):        fixed — REGION_MAX_CLEANUP * sizeof(hook) + metadata
 *
 * Quick start:
 * ```c
 * #include "core/region.h"
 *
 * // Basic region — lifetime boundary only
 * Region r = region_begin();
 * // ... allocate, borrow, work ...
 * region_end(&r);   // borrows from this region are now invalid
 *
 * // Arena-attached region — reset arena on end
 * Region r = region_begin();
 * region_attach_arena(&r, &my_arena);
 * void* buf = arena_alloc(&my_arena, 256);  // lives until region_end
 * region_end(&r);   // calls arena_reset(&my_arena) automatically
 *
 * // Region-stamped borrow (manual discipline)
 * region_id_t rid = region_id(&r);
 * borrowed_str name = borrowed_str_from(str_from_cstr("Alice"), &r);
 * // ... pass name around ...
 * region_assert_borrow_valid(&r, rid);  // debug: still open?
 *
 * // Nested regions
 * Region outer = region_begin();
 * Region inner = region_begin();
 * region_set_parent(&inner, &outer);
 * // ...
 * region_end(&inner);  // inner ends first
 * region_end(&outer);  // outer ends last
 *
 * // Cleanup hook — called on region_end
 * void release_lock(void* ctx) { mutex_unlock((Mutex*)ctx); }
 * region_register(&r, release_lock, &my_mutex);
 * ```
 *
 * @sa core/arena.h      — bump allocator; attach to a region for scoped allocation
 * @sa core/ownership.h  — owned/borrowed annotation vocabulary
 * @sa core/slice.h      — bytes_t, str_t, slice_##type used with region lifetime
 * @sa core/scope.h      — RAII-style cleanup for simpler single-resource cases
 */

/* ════════════════════════════════════════════════════════════════════════════
   Configuration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Maximum number of cleanup hooks per Region
 *
 * Each hook is a {fn, ctx} pair. Hooks are called in reverse registration
 * order on region_end() (LIFO — last registered, first called).
 * Increase if your regions manage many resources.
 *
 * sizeof(Region) grows with this value.
 * Default: 8 hooks per region.
 */
#ifndef REGION_MAX_CLEANUP
    #define REGION_MAX_CLEANUP ((usize)8)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Region ID
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Unique monotonic identifier for a Region lifetime
 *
 * Assigned at region_begin(). Monotonically increasing per process.
 * Used to stamp borrowed views and validate their lifetime in debug builds.
 *
 * ID 0 is reserved — represents "no region" / "static lifetime".
 */
typedef u64 region_id_t;

/**
 * @brief The reserved "no region" / static lifetime ID
 *
 * Borrows stamped with REGION_ID_STATIC are considered always valid —
 * they live as long as the program runs (string literals, static buffers).
 */
#define REGION_ID_STATIC ((region_id_t)0)

/**
 * @brief Returns the next unique region ID (not thread-safe)
 *
 * Called internally by region_begin(). Do not call directly.
 *
 * @note Uses a file-scoped static counter. Single-threaded use only.
 */
static inline region_id_t region_next_id(void) {
    static region_id_t counter = 0;
    return ++counter;
}

/* ════════════════════════════════════════════════════════════════════════════
   RegionCleanup — a single registered cleanup hook
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief A cleanup function and its context, registered with a Region
 *
 * Called on region_end() in reverse registration order (LIFO).
 *
 * Fields:
 * - fn:  Cleanup function — receives ctx, must not call region_end
 * - ctx: Caller-provided context pointer (may be NULL)
 */
typedef struct {
    void (*fn)(void* ctx); ///< Cleanup function (NULL = slot is empty)
    void* ctx;             ///< Context passed to fn
} RegionCleanup;

/* ════════════════════════════════════════════════════════════════════════════
   Region — the lifetime token
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief A named lifetime boundary — stack-allocated, never heap-allocated
 *
 * A Region token represents the lifetime of a scope. Borrowed views
 * can be stamped with a region_id to express "valid until this region ends."
 *
 * Fields:
 * - id:         Unique monotonic ID assigned at region_begin()
 * - open:       true between region_begin() and region_end()
 * - arena:      Optional attached arena (reset on region_end if non-NULL)
 * - parent:     Optional parent region (informational — not auto-ended)
 * - cleanups:   Registered cleanup hooks (called LIFO on region_end)
 * - num_hooks:  Number of registered hooks
 *
 * Invariants:
 * - id > 0 after region_begin()
 * - open == true between begin and end
 * - open == false after region_end()
 * - num_hooks <= REGION_MAX_CLEANUP
 *
 * Do not access fields directly — use the provided functions.
 */
typedef struct Region Region;

struct Region {
    region_id_t   id;                            ///< Unique ID assigned at begin
    bool          open;                          ///< true between begin and end
    Arena*        arena;                         ///< Optional attached arena (may be NULL)
    Region*       parent;                        ///< Optional parent region (may be NULL)
    RegionCleanup cleanups[REGION_MAX_CLEANUP];  ///< Registered cleanup hooks
    usize         num_hooks;                     ///< Number of registered hooks
};

/* ════════════════════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Opens a new Region and assigns it a unique ID
 *
 * The returned Region is fully initialized and open.
 * It must be closed with region_end() before going out of scope.
 *
 * @return Initialized open Region with unique ID, no arena, no hooks
 *
 * @post result.open == true
 * @post result.id > 0
 * @post result.arena == NULL
 * @post result.num_hooks == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: sizeof(Region) on the stack — no heap allocation
 */
static inline Region region_begin(void) {
    Region r;
    r.id        = region_next_id();
    r.open      = true;
    r.arena     = NULL;
    r.parent    = NULL;
    r.num_hooks = 0;
    /* zero hook slots for clean debugger display */
    for (usize i = 0; i < REGION_MAX_CLEANUP; i++) {
        r.cleanups[i].fn  = NULL;
        r.cleanups[i].ctx = NULL;
    }
    return r;
}

/**
 * @brief Closes a Region — calls all registered cleanup hooks, resets arena
 *
 * Hooks are called in LIFO order (last registered, first called).
 * If an arena was attached, arena_reset() is called after all hooks.
 * After this call, r->open == false and r->id is preserved for inspection.
 *
 * @param r Region to close (must not be NULL, must be open)
 *
 * @pre r != NULL
 * @pre r->open == true
 *
 * @post r->open == false
 * @post All hooks have been called
 * @post r->arena has been reset (if non-NULL)
 * @post r->num_hooks == 0
 *
 * Performance:
 * - Time:  O(num_hooks)
 * - Space: O(1)
 */
static inline void region_end(Region* r) {
    require_msg(r != NULL,   "region_end: r cannot be NULL");
    require_msg(r->open,     "region_end: region is already closed");

    /* Call cleanup hooks LIFO */
    for (usize i = r->num_hooks; i > 0; i--) {
        RegionCleanup* hook = &r->cleanups[i - 1];
        if (hook->fn) {
            hook->fn(hook->ctx);
            hook->fn  = NULL;
            hook->ctx = NULL;
        }
    }
    r->num_hooks = 0;

    /* Reset attached arena */
    if (r->arena) {
        arena_reset(r->arena);
        r->arena = NULL;
    }

    r->open = false;
}

/* ════════════════════════════════════════════════════════════════════════════
   Configuration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Attaches an arena to this region
 *
 * The arena will be reset (via arena_reset()) when region_end() is called.
 * Only one arena can be attached per region. Attaching a second arena
 * replaces the first (the first is NOT reset at replacement time).
 *
 * @param r     Region to attach arena to (must not be NULL, must be open)
 * @param arena Arena to attach (must not be NULL)
 *
 * @pre r != NULL && r->open
 * @pre arena != NULL
 *
 * @post r->arena == arena
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline void region_attach_arena(Region* r, Arena* arena) {
    require_msg(r != NULL,    "region_attach_arena: r cannot be NULL");
    require_msg(r->open,      "region_attach_arena: region is not open");
    require_msg(arena != NULL,"region_attach_arena: arena cannot be NULL");
    r->arena = arena;
}

/**
 * @brief Sets the parent region (informational — parent is NOT auto-ended)
 *
 * Parent is used to express nesting intent. It is never dereferenced
 * by region.h for automatic propagation — that is always the caller's job.
 * Useful for debugging and diagnostic tools that want to walk the region tree.
 *
 * @param r      Child region (must not be NULL, must be open)
 * @param parent Parent region (must not be NULL, must be open)
 *
 * @pre r != NULL && r->open
 * @pre parent != NULL && parent->open
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline void region_set_parent(Region* r, Region* parent) {
    require_msg(r != NULL,       "region_set_parent: r cannot be NULL");
    require_msg(r->open,         "region_set_parent: region is not open");
    require_msg(parent != NULL,  "region_set_parent: parent cannot be NULL");
    require_msg(parent->open,    "region_set_parent: parent region is not open");
    r->parent = parent;
}

/**
 * @brief Registers a cleanup hook to be called on region_end()
 *
 * Hooks are called LIFO — the last hook registered is the first called.
 * If the hook table is full (num_hooks == REGION_MAX_CLEANUP), registration
 * fails and returns false. Increase REGION_MAX_CLEANUP if needed.
 *
 * @param r   Region to register with (must not be NULL, must be open)
 * @param fn  Cleanup function — void fn(void* ctx) (must not be NULL)
 * @param ctx Context passed to fn (may be NULL)
 * @return true on success, false if hook table is full
 *
 * @pre r != NULL && r->open
 * @pre fn != NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool region_register(Region* r, void (*fn)(void* ctx), void* ctx) {
    require_msg(r != NULL,   "region_register: r cannot be NULL");
    require_msg(r->open,     "region_register: region is not open");
    require_msg(fn != NULL,  "region_register: fn cannot be NULL");

    if (r->num_hooks >= REGION_MAX_CLEANUP) return false;

    r->cleanups[r->num_hooks].fn  = fn;
    r->cleanups[r->num_hooks].ctx = ctx;
    r->num_hooks++;
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Inspection
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the unique ID of this region
 *
 * @param r Region to query (NULL returns REGION_ID_STATIC)
 * @return region_id_t — 0 if r == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline region_id_t region_id(const Region* r) {
    return r ? r->id : REGION_ID_STATIC;
}

/**
 * @brief Returns true if the region is currently open
 *
 * @param r Region to check (NULL returns false)
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool region_is_open(const Region* r) {
    return r && r->open;
}

/**
 * @brief Returns true if the region has a parent
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool region_has_parent(const Region* r) {
    return r && r->parent != NULL;
}

/**
 * @brief Returns the number of registered cleanup hooks
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline usize region_hook_count(const Region* r) {
    return r ? r->num_hooks : 0;
}

/* ════════════════════════════════════════════════════════════════════════════
   Lifetime assertions — debug builds only
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Asserts that a region is still open (debug builds only)
 *
 * Call this before using a borrowed value that was stamped with this region.
 * In release builds, expands to nothing — zero overhead.
 *
 * @param r  Region to check (must be non-NULL and open)
 *
 * Example:
 * ```c
 * region_assert_open(&r);
 * const char* p = borrowed_str_get(&name).ptr;  // safe — region confirmed open
 * ```
 */
static inline void region_assert_open(const Region* r) {
    ensure_msg(r != NULL, "region_assert_open: r cannot be NULL");
    ensure_msg(r->open,   "region_assert_open: region is closed — borrow may be invalid");
}

/**
 * @brief Asserts that a borrow stamped with borrow_region_id is still valid
 *
 * A borrow is valid if its region is still open OR it has the static lifetime
 * (REGION_ID_STATIC). Passes silently for static borrows.
 *
 * @param r               The region that owns the borrowed data
 * @param borrow_region_id The ID stamped on the borrow at creation time
 *
 * Example:
 * ```c
 * region_id_t rid = region_id(&r);
 * // ... pass rid alongside borrowed views ...
 * region_assert_borrow_valid(&r, rid);  // confirms region still open
 * ```
 */
static inline void region_assert_borrow_valid(const Region* r, region_id_t borrow_region_id) {
    if (borrow_region_id == REGION_ID_STATIC) return; /* static lifetime — always valid */
    ensure_msg(r != NULL,                          "region_assert_borrow_valid: r cannot be NULL");
    ensure_msg(r->open,                            "region_assert_borrow_valid: region is closed");
    ensure_msg(r->id == borrow_region_id,
        "region_assert_borrow_valid: borrow region ID does not match current region");
}

/* ════════════════════════════════════════════════════════════════════════════
   REGION_SCOPE — automatic region_end via scope.h convention (optional)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def REGION_SCOPE(name)
 * @brief Declares a Region that is automatically ended at scope exit
 *
 * Requires scope.h (SCOPE_DEFER or equivalent) to be available.
 * If scope.h is not included, use region_begin() / region_end() manually.
 *
 * @param name Variable name for the Region
 *
 * Example:
 * ```c
 * #include "core/scope.h"
 * #include "core/region.h"
 *
 * void my_function(void) {
 *     REGION_SCOPE(r);
 *     region_attach_arena(&r, &scratch);
 *     // ... work ...
 * }   // region_end(&r) called automatically
 * ```
 */
#ifdef CANON_CORE_SCOPE_H
    #define REGION_SCOPE(name) \
        Region name = region_begin(); \
        SCOPE_DEFER(region_end(&name))
#else
    /* scope.h not included — define as manual open only, remind caller to close */
    #define REGION_SCOPE(name) \
        Region name = region_begin() /* remember to call region_end(&name) */
#endif

#endif /* CANON_CORE_REGION_H */
