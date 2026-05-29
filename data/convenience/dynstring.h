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

#ifndef CANON_DATA_CONVENIENCE_DYNSTRING_H
#define CANON_DATA_CONVENIENCE_DYNSTRING_H

#include <stdarg.h>                      /* va_list, va_start, va_end — for append_fmt */
#include <stdlib.h>                      /* malloc, realloc, free */
#include <stdio.h>                       /* vsnprintf */

#include "core/primitives/types.h"       /* usize, bool */
#include "core/primitives/contract.h"    /* require_msg, ensure_msg */
#include "core/memory.h"                 /* mem_copy */

#ifdef CANON_LIFETIME_DEBUG
    #include <stdint.h>                    /* uintptr_t */
    #include "core/primitives/lifetime.h"  /* region_id_t, lifetime_t */
#endif

/**
 * @file convenience/dynstring.h
 * @brief Auto-growing string builder with hidden heap allocation
 *
 * CONVENIENCE LAYER — trades explicitness for ergonomics.
 * All appends grow automatically. Caller must call dynstring_free() when done.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Automatic heap allocation — no caller-provided buffer needed
 * - Automatic 2× growth on overflow
 * - Owns its own memory — must free to avoid leaks
 * - Always null-terminated, even when empty
 * - No arena support — use data/stringbuf.h for arena-backed strings
 *
 * When to use this instead of data/stringbuf.h:
 * ────────────────────────────────────────────────────────────────────────────
 * - Building strings of unknown or unbounded length
 * - Rapid prototyping or desktop/server applications
 * - JSON/XML generation, log formatting, dynamic output
 * - When convenience matters more than determinism
 *
 * When to use data/stringbuf.h instead:
 * ────────────────────────────────────────────────────────────────────────────
 * - Performance-critical or real-time code
 * - Embedded systems or bounded-memory environments
 * - When maximum size is known upfront
 * - Arena-backed string lifetime management
 *
 * Growth strategy:
 * ────────────────────────────────────────────────────────────────────────────
 * - Initial capacity: DYNSTRING_INITIAL_CAPACITY (default 64 bytes)
 * - Growth factor: DYNSTRING_GROWTH_FACTOR (default 2×)
 * - No automatic shrinking — call dynstring_shrink_to_fit() explicitly
 *
 * Lifetime tracking (define CANON_LIFETIME_DEBUG before including):
 * ────────────────────────────────────────────────────────────────────────────
 *   - Embeds a lifetime_t lt field (id + open) on the DynString.
 *   - dynstring_init(), dynstring_with_capacity(), and dynstring_from() open
 *     a fresh lifetime; the ID is derived from a per-TU monotonic counter
 *     XOR'd with &s. The counter is necessary because DynString constructors
 *     return by value — the compiler reuses the same stack slot for
 *     consecutive constructor calls, and the bare &s derivation would
 *     collide across calls, breaking invalidation on subsequent struct
 *     copies or swaps.
 *   - dynstring_ensure_capacity() and dynstring_shrink_to_fit() RESTAMP the
 *     lifetime ID conditionally — only when realloc actually moves the
 *     buffer. Operations that don't trigger reallocation (append when cap
 *     is sufficient, clear, truncate) do NOT restamp because the buffer
 *     address is unchanged and existing borrows remain valid.
 *   - Restamp draws a fresh id from the per-TU counter — guaranteed
 *     distinct from any prior id within the TU. Previously the restamp
 *     XOR'd a constant into the id, which could cycle (x ^ K ^ K == x)
 *     across grow-then-shrink and spuriously re-validate a stale borrow.
 *   - dynstring_free() CLOSES the lifetime (lt.open = false). Any borrow
 *     that captured this DynString's ID will fire require_msg via
 *     lifetime_assert_valid() on the next read.
 *   - dynstring_clear() and dynstring_truncate() do NOT restamp. They only
 *     adjust len; the buffer is unchanged. Lifetime tracking catches
 *     use-after-relocation and use-after-free; it does NOT catch
 *     use-of-truncated-characters, because the memory addresses of
 *     truncated bytes are still valid bytes in the same buffer owned by
 *     the same DynString. That bug class is out of scope for the
 *     substrate.
 *   Zero cost in release builds — struct layout is identical without the flag.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Each DynString instance is independent — no shared state.
 * Concurrent modifications require external synchronization.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Uses <stdarg.h> + Canon-C core modules
 * - No platform-specific code
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - append: Amortized O(n), worst-case O(len + n) on realloc
 * - append_char: Amortized O(1), worst-case O(len) on realloc
 * - append_fmt: O(n) — two formatting passes
 * - All queries: O(1)
 * - dynstring_free: O(1)
 *
 * Quick start:
 * ```c
 * #include "data/convenience/dynstring.h"
 *
 * DynString s = dynstring_init();
 * dynstring_append(&s, "Hello, ");
 * dynstring_append_fmt(&s, "%s!", name);
 * printf("%s\n", dynstring_str(&s));
 * dynstring_free(&s); // REQUIRED — always call this
 * ```
 *
 * @sa data/stringbuf.h            — fixed-capacity, arena-friendly alternative
 * @sa core/primitives/lifetime.h  — canonical home of region_id_t and lifetime_t
 * @sa core/region.h               — lifetime_assert_valid runtime check
 */

/* ════════════════════════════════════════════════════════════════════════════
   Configuration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initial heap allocation size in bytes (including null terminator)
 *
 * Override before including this file if a different default is needed.
 * Default: 64 bytes.
 */
#ifndef DYNSTRING_INITIAL_CAPACITY
    #define DYNSTRING_INITIAL_CAPACITY ((usize)64)
#endif

/**
 * @brief Capacity growth multiplier applied when buffer is full
 *
 * Default: 2 (doubles on each realloc).
 * Increase for fewer allocations; decrease for lower memory usage.
 */
#ifndef DYNSTRING_GROWTH_FACTOR
    #define DYNSTRING_GROWTH_FACTOR ((usize)2)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   Branch hint helpers
   ════════════════════════════════════════════════════════════════════════════ */

#if defined(__GNUC__) || defined(__clang__)
    #define DYNSTRING_LIKELY(x) __builtin_expect(!!(x), 1)
    #define DYNSTRING_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define DYNSTRING_LIKELY(x) (x)
    #define DYNSTRING_UNLIKELY(x) (x)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   DynString struct
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Auto-growing heap-allocated string builder
 *
 * Invariants:
 * - data[len] == '\0' (always null-terminated when data != NULL)
 * - len < cap (always room for null terminator when data != NULL)
 * - data is heap-allocated (or NULL before first append)
 *
 * Do not access or modify fields directly — use the provided functions.
 *
 * Memory layout:
 * - sizeof(DynString) = sizeof(char*) + 2*sizeof(usize) [+ sizeof(lifetime_t) under CANON_LIFETIME_DEBUG]
 * - Backing buffer: cap bytes on the heap
 */
typedef struct {
    char* data;     ///< Heap-allocated buffer (NULL until first append)
    usize len;      ///< Current string length (excluding '\0')
    usize cap;      ///< Total buffer capacity (including '\0')
#ifdef CANON_LIFETIME_DEBUG
    lifetime_t lt;  ///< [debug] Lifetime token: id + open
#endif
} DynString;

/* ════════════════════════════════════════════════════════════════════════════
   Internal: lifetime helpers (compiled away in release)
   ════════════════════════════════════════════════════════════════════════════
   When CANON_LIFETIME_DEBUG is enabled, a DynString exposes a lifetime_t
   that borrows can capture and validate against. The helpers below
   manage the (id, open) pair across the DynString lifecycle:

     dynstring_lifetime_open_(s)
       Called by init, with_capacity, and from. Draws a fresh id from
       the per-TU monotonic counter (XOR'd with &s for cross-TU
       diversity) and marks the lifetime open.

     dynstring_lifetime_restamp_(s)
       Called by ensure_capacity and shrink_to_fit when realloc moves
       the buffer. Draws a fresh id from the per-TU counter, guaranteed
       distinct from any prior id within the TU. Open flag stays true;
       DynStrings are not "closed" by reallocation — they are recycled.

     dynstring_lifetime_close_(s)
       Called by free. Marks the lifetime closed, which makes any
       subsequent borrow read fire lifetime_assert_valid.

   In release builds (CANON_LIFETIME_DEBUG undefined) all three
   helpers are no-ops — the DynString struct does not have an lt
   field and no code touches it.
   ════════════════════════════════════════════════════════════════════════════ */

#ifdef CANON_LIFETIME_DEBUG
    /* Per-TU counter used to derive unique lifetime ids.
     *
     * Why not just use (uintptr_t)s?
     *   dynstring_init() / with_capacity() / from() return by value — the
     *   compiler reuses the same stack slot for consecutive constructor
     *   calls, so &s collides across calls. After the struct copy out,
     *   two DynStrings declared back-to-back end up with identical ids —
     *   defeating invalidation on subsequent struct copies or swaps.
     *
     * The counter is a `static` inside a `static inline` function, so
     * each translation unit has its own copy and increments are TU-local.
     * Two DynStrings constructed in the same TU get different ids;
     * DynStrings constructed in different TUs get ids from independent
     * counters but mixed with the local-variable address, making
     * cross-TU collisions vanishingly unlikely. Same pattern as vec /
     * deque / pq / hashmap / dynvec / smallvec.
     *
     * The counter also makes restamp collision-proof: previously the
     * restamp XOR'd a constant into the id, which could cycle
     * (x ^ K ^ K == x) across grow-then-shrink — spuriously re-validating
     * a borrow captured before the first restamp. Drawing a fresh value
     * from the counter eliminates that issue.
     *
     * No thread-safety guarantee: if a single TU's constructors or
     * restamps are invoked concurrently, the counter may race and
     * collide. Callers needing concurrent construction must externally
     * synchronize — the same constraint applies to every Canon-C
     * container.
     *
     * REGION_ID_STATIC (0) is reserved; the counter starts at 1 and the
     * id derivation never produces 0 defensively.
     */
    static inline region_id_t dynstring_lifetime_next_id_(void* sp) {
        static region_id_t counter_ = 1;
        region_id_t id = (region_id_t)(counter_++)
                       ^ (region_id_t)(uintptr_t)(sp);
        if (id == REGION_ID_STATIC) id = (region_id_t)1;
        return id;
    }
#endif

static inline void dynstring_lifetime_open_(DynString* s) {
#ifdef CANON_LIFETIME_DEBUG
    s->lt.id   = dynstring_lifetime_next_id_(s);
    s->lt.open = true;
#endif
    (void)s;
}

/* Restamp draws a fresh id from the per-TU counter — guaranteed distinct
 * from any prior id within the TU, unlike the previous XOR-with-constant
 * approach which could cycle (x ^ K ^ K == x) across grow-then-shrink. */
static inline void dynstring_lifetime_restamp_(DynString* s) {
#ifdef CANON_LIFETIME_DEBUG
    s->lt.id = dynstring_lifetime_next_id_(s);
#endif
    (void)s;
}

static inline void dynstring_lifetime_close_(DynString* s) {
#ifdef CANON_LIFETIME_DEBUG
    s->lt.open = false;
#endif
    (void)s;
}

/* ════════════════════════════════════════════════════════════════════════════
   Internal helper — capacity management
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ensures the buffer has at least min_cap bytes of capacity
 *
 * Uses 2× doubling strategy. If current cap is 0, starts at
 * DYNSTRING_INITIAL_CAPACITY. If doubling is still insufficient,
 * uses min_cap directly.
 *
 * @param s DynString to grow
 * @param min_cap Minimum required capacity (including null terminator)
 * @return true on success, false on allocation failure
 *
 * @pre s != NULL — checked via ensure_msg() (debug builds only)
 *
 * @note Internal use only — do not call directly.
 *
 * Lifetime (CANON_LIFETIME_DEBUG): restamps lt.id ONLY if realloc moved
 * the buffer. If cap was already sufficient (no realloc) or realloc
 * returned the same address (in-place expansion), lt.id is preserved
 * and existing borrows remain valid.
 *
 * Performance:
 * - Time: O(len) on realloc, O(1) if already sufficient
 * - Space: Allocates up to 2× current capacity
 */
static inline bool dynstring_ensure_capacity(DynString* s, usize min_cap) {
    ensure_msg(s != NULL, "dynstring_ensure_capacity: s cannot be NULL");
    if (!s) return false;
    if (s->cap >= min_cap) return true;

    usize new_cap = (s->cap == 0)
        ? DYNSTRING_INITIAL_CAPACITY
        : s->cap * DYNSTRING_GROWTH_FACTOR;

    if (new_cap < min_cap) new_cap = min_cap;

    char* old_data = s->data;
    char* new_data = (char*)realloc(s->data, new_cap);
    if (!new_data) return false;

    s->data = new_data;
    s->cap = new_cap;
    if (new_data != old_data) {
        dynstring_lifetime_restamp_(s);
    }
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Constructors
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Creates an empty DynString with no initial allocation
 *
 * No heap allocation occurs until the first append.
 *
 * @return Zero-initialized DynString
 *
 * @post result.data == NULL, result.len == 0, result.cap == 0
 *
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token. The ID
 * is derived from a per-TU counter XOR'd with the returned struct's
 * address — borrows constructed against this DynString carry this ID.
 *
 * Performance:
 * - Time: O(1)
 * - Space: sizeof(DynString) — no heap allocation
 */
static inline DynString dynstring_init(void) {
    DynString s;
    s.data = NULL;
    s.len  = 0;
    s.cap  = 0;
    dynstring_lifetime_open_(&s);
    return s;
}

/**
 * @brief Creates a DynString with pre-allocated heap capacity
 *
 * Useful when the expected final size is known, avoiding
 * intermediate reallocations.
 *
 * @param capacity Initial buffer size in bytes (including null terminator)
 * @return Initialized DynString with empty string and reserved capacity
 *
 * @post On success: result.cap == capacity, result.len == 0, result.data[0] == '\0'
 * @post On failure (OOM): returns dynstring_init() — check with dynstring_capacity()
 *
 * @note Call dynstring_free() when done.
 *
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token, same as init.
 *
 * Performance:
 * - Time: O(1)
 * - Space: capacity bytes on the heap
 */
static inline DynString dynstring_with_capacity(usize capacity) {
    DynString s = dynstring_init();
    if (capacity == 0) return s;

    s.data = (char*)malloc(capacity);
    if (s.data) {
        s.data[0] = '\0';
        s.cap = capacity;
    }
    return s;
}

/**
 * @brief Creates a DynString by copying a C string
 *
 * @param str Source C string (NULL returns empty DynString)
 * @return DynString containing a copy of str
 *
 * @post On success: dynstring_str(&result) equals str
 * @post On failure (OOM): returns dynstring_init()
 *
 * @note Call dynstring_free() when done.
 *
 * Lifetime (CANON_LIFETIME_DEBUG): opens a fresh lifetime token, same as init.
 *
 * Performance:
 * - Time: O(strlen(str))
 * - Space: strlen(str) + 1 bytes on the heap
 */
static inline DynString dynstring_from(const char* str) {
    DynString s = dynstring_init();
    if (!str) return s;

    usize len = strlen(str);
    usize cap = len + 1;

    s.data = (char*)malloc(cap);
    if (s.data) {
        mem_copy(s.data, str, len + 1);  /* includes null terminator */
        s.len = len;
        s.cap = cap;
    }
    return s;
}

/* ════════════════════════════════════════════════════════════════════════════
   Queries
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the current string length (excluding null terminator)
 *
 * @param s DynString to query (NULL-safe)
 * @return usize — 0 if s == NULL
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline usize dynstring_len(const DynString* s) {
    return s ? s->len : 0;
}

/**
 * @brief Returns the current heap capacity (including null terminator byte)
 *
 * @param s DynString to query (NULL-safe)
 * @return usize — 0 if s == NULL or no allocation yet
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline usize dynstring_capacity(const DynString* s) {
    return s ? s->cap : 0;
}

/**
 * @brief Returns true if the string has no characters
 *
 * @param s DynString to check (NULL-safe)
 * @return true if empty or NULL
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline bool dynstring_is_empty(const DynString* s) {
    return !s || s->len == 0;
}

/**
 * @brief Returns a pointer to the current null-terminated string (borrowed view)
 *
 * Always safe — returns "" for NULL or uninitialized DynString.
 * Pointer is valid until the next modification or dynstring_free().
 *
 * @param s DynString to access (NULL-safe)
 * @return Pointer to null-terminated string — never NULL
 *
 * @note Do not free the returned pointer — it is owned by the DynString.
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline const char* dynstring_str(const DynString* s) {
    return (s && s->data) ? s->data : "";
}

/* ════════════════════════════════════════════════════════════════════════════
   Append operations (auto-growing)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Appends a null-terminated C string, growing if needed
 *
 * @param s DynString to append to
 * @param str String to append (NULL treated as empty — no-op, returns true)
 * @return true on success, false on allocation failure
 *
 * @post On true: str appended, s->len increased by strlen(str)
 * @post On false: s is unchanged
 *
 * @note May allocate heap memory.
 *
 * Lifetime (CANON_LIFETIME_DEBUG): if growth was triggered AND the buffer
 * was relocated, the lifetime ID is restamped (via ensure_capacity). If
 * no growth was needed, the lifetime ID is preserved.
 *
 * Performance:
 * - Time: Amortized O(strlen(str)), worst-case O(len + strlen(str)) on realloc
 * - Space: May allocate up to 2× current capacity
 */
static inline bool dynstring_append(DynString* s, const char* str) {
    if (!s) return false;
    if (!str) return true;  /* NULL is a no-op */

    usize add_len = strlen(str);
    if (add_len == 0) return true;

    usize required_cap = s->len + add_len + 1;
    if (DYNSTRING_UNLIKELY(!dynstring_ensure_capacity(s, required_cap))) {
        return false;
    }

    mem_copy(s->data + s->len, str, add_len);
    s->len += add_len;
    s->data[s->len] = '\0';
    return true;
}

/**
 * @brief Appends a single character, growing if needed
 *
 * @param s DynString to append to
 * @param c Character to append
 * @return true on success, false on allocation failure
 *
 * @post On true: c appended, s->len incremented by 1
 *
 * @note May allocate heap memory.
 *
 * Lifetime (CANON_LIFETIME_DEBUG): same as dynstring_append — restamps
 * only if realloc moves the buffer.
 *
 * Performance:
 * - Time: Amortized O(1), worst-case O(len) on realloc
 * - Space: May allocate up to 2× current capacity
 */
static inline bool dynstring_append_char(DynString* s, char c) {
    if (!s) return false;

    usize required_cap = s->len + 2;  /* char + '\0' */
    if (DYNSTRING_UNLIKELY(!dynstring_ensure_capacity(s, required_cap))) {
        return false;
    }

    s->data[s->len++] = c;
    s->data[s->len] = '\0';
    return true;
}

/**
 * @brief Appends formatted text (printf-style), growing if needed
 *
 * Uses vsnprintf internally — two passes (measure then write).
 *
 * @param s DynString to append to
 * @param fmt printf-style format string
 * @param ... Format arguments
 * @return true on success, false on format error or allocation failure
 *
 * @post On true: formatted text appended, s->len increased accordingly
 * @post On false: s is unchanged
 *
 * @note May allocate heap memory.
 *
 * Lifetime (CANON_LIFETIME_DEBUG): same as dynstring_append — restamps
 * only if realloc moves the buffer.
 *
 * Performance:
 * - Time: O(n) where n = formatted string length — two vsnprintf passes
 * - Space: May allocate up to 2× current capacity
 */
static inline bool dynstring_append_fmt(DynString* s, const char* fmt, ...) {
    if (!s || !fmt) return false;

    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed < 0) return false;

    usize required_cap = s->len + (usize)needed + 1;
    if (!dynstring_ensure_capacity(s, required_cap)) return false;

    va_start(args, fmt);
    int written = vsnprintf(s->data + s->len, s->cap - s->len, fmt, args);
    va_end(args);

    if (written < 0 || written != needed) return false;

    s->len += (usize)written;
    return true;
}

/**
 * @brief Appends at most n characters from str, growing if needed
 *
 * Appends min(n, strlen(str)) characters.
 *
 * @param s DynString to append to
 * @param str Source string (NULL treated as empty — no-op, returns true)
 * @param n Maximum number of characters to append
 * @return true on success, false on allocation failure
 *
 * @note May allocate heap memory.
 *
 * Lifetime (CANON_LIFETIME_DEBUG): same as dynstring_append — restamps
 * only if realloc moves the buffer.
 *
 * Performance:
 * - Time: Amortized O(min(n, strlen(str)))
 * - Space: May allocate up to 2× current capacity
 */
static inline bool dynstring_append_n(DynString* s, const char* str, usize n) {
    if (!s) return false;
    if (!str || n == 0) return true;

    usize actual_len = 0;
    while (actual_len < n && str[actual_len] != '\0') actual_len++;

    if (actual_len == 0) return true;

    usize required_cap = s->len + actual_len + 1;
    if (!dynstring_ensure_capacity(s, required_cap)) return false;

    mem_copy(s->data + s->len, str, actual_len);
    s->len += actual_len;
    s->data[s->len] = '\0';
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Mutation
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Resets the string to empty without freeing the heap buffer
 *
 * If data != NULL: resets len to 0 and writes '\0' at index 0.
 * If data == NULL (never appended to): this is a no-op — safe to call.
 * dynstring_str() returns "" in either case.
 *
 * @param s DynString to clear (NULL-safe)
 *
 * @post If s->data != NULL: s->len == 0 and s->data[0] == '\0'
 * @post If s->data == NULL: s is unchanged (no-op)
 * @note Buffer is NOT zeroed — only logical state is reset.
 * Use dynstring_shrink_to_fit() afterward to release excess memory.
 *
 * Lifetime (CANON_LIFETIME_DEBUG): does NOT restamp. The buffer is
 * unchanged and existing borrows still point at valid memory. The
 * substrate does not catch use-of-cleared-characters — see the file
 * docblock.
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void dynstring_clear(DynString* s) {
    if (s && s->data) {
        s->len = 0;
        s->data[0] = '\0';
    }
}

/**
 * @brief Truncates the string to new_len characters
 *
 * Does nothing if new_len >= current length.
 *
 * @param s DynString to truncate (NULL-safe)
 * @param new_len New length — must be <= current length to take effect
 *
 * @post If new_len < s->len: s->len == new_len, s->data[new_len] == '\0'
 *
 * Lifetime (CANON_LIFETIME_DEBUG): does NOT restamp. Buffer unchanged.
 * Same rationale as dynstring_clear.
 *
 * Performance:
 * - Time: O(1)
 * - Space: O(1)
 */
static inline void dynstring_truncate(DynString* s, usize new_len) {
    if (s && s->data && new_len < s->len) {
        s->len = new_len;
        s->data[s->len] = '\0';
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Capacity management
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ensures the buffer can hold at least min_cap bytes
 *
 * Does nothing if current capacity is already sufficient.
 *
 * @param s DynString to reserve into (NULL-safe)
 * @param min_cap Minimum required capacity in bytes (including null terminator)
 * @return true on success (or if already sufficient), false on allocation failure
 *
 * @note May allocate heap memory.
 *
 * Lifetime (CANON_LIFETIME_DEBUG): if reserve triggers a realloc that
 * relocates the buffer, the lifetime ID is restamped (via
 * ensure_capacity). If cap is already sufficient, the lifetime ID is
 * preserved.
 *
 * Performance:
 * - Time: O(len) on realloc, O(1) if already sufficient
 * - Space: May allocate
 */
static inline bool dynstring_reserve(DynString* s, usize min_cap) {
    if (!s) return false;
    return dynstring_ensure_capacity(s, min_cap);
}

/**
 * @brief Shrinks the heap buffer to exactly fit the current string
 *
 * Frees excess capacity. If the string is empty, frees the entire buffer.
 *
 * @param s DynString to shrink (NULL-safe)
 * @return true on success, false on realloc failure (s is unchanged on failure)
 *
 * Lifetime (CANON_LIFETIME_DEBUG):
 *   - If len == 0 and data != NULL: frees the buffer and RESTAMPS the
 *     lifetime ID. The container is recycled to an empty state —
 *     subsequent operations work normally (append will grow from zero).
 *     lt.open stays true.
 *   - If len > 0 and realloc moves the buffer: RESTAMPS.
 *   - If len > 0 and realloc returns the same address (or cap was
 *     already equal to len + 1): lifetime ID is preserved.
 *
 * Performance:
 * - Time: O(len) — realloc
 * - Space: Frees (cap - len - 1) bytes
 */
static inline bool dynstring_shrink_to_fit(DynString* s) {
    if (!s || !s->data) return true;

    if (s->len == 0) {
        free(s->data);
        s->data = NULL;
        s->cap = 0;
        dynstring_lifetime_restamp_(s);
        return true;
    }

    if (s->cap == s->len + 1) return true;  /* already minimal */

    usize new_cap = s->len + 1;
    char* old_data = s->data;
    char* new_data = (char*)realloc(s->data, new_cap);
    if (!new_data) return false;

    s->data = new_data;
    s->cap = new_cap;
    if (new_data != old_data) {
        dynstring_lifetime_restamp_(s);
    }
    return true;
}

/* ════════════════════════════════════════════════════════════════════════════
   Memory management
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Frees the heap buffer and resets all fields to zero
 *
 * @param s DynString to free (NULL-safe)
 *
 * @post s->data == NULL, s->len == 0, s->cap == 0
 *
 * ⚠️ MUST be called when done to avoid memory leaks.
 * Subsequent use requires reinitializing via dynstring_init() or dynstring_from().
 *
 * Lifetime (CANON_LIFETIME_DEBUG): CLOSES the lifetime (lt.open = false).
 * Any borrow that captured this DynString's ID will fire require_msg on
 * the next read. The lifetime is reopened only by a fresh init /
 * with_capacity / from call.
 *
 * Performance:
 * - Time: O(1)
 * - Space: Frees cap bytes
 */
static inline void dynstring_free(DynString* s) {
    if (s && s->data) {
        free(s->data);
        s->data = NULL;
        s->len = 0;
        s->cap = 0;
        dynstring_lifetime_close_(s);
    } else if (s) {
        /* No buffer to free, but still close the lifetime so the
         * contract holds regardless of whether anything was appended. */
        dynstring_lifetime_close_(s);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Utility
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Creates a heap-allocated copy of the string as a plain C string
 *
 * The returned pointer is independent of the DynString — caller must free it.
 *
 * @param s DynString to copy (NULL-safe)
 * @return Heap-allocated null-terminated string, or NULL on OOM
 *
 * @note Caller is responsible for calling free() on the returned pointer.
 * @note Returns NULL on allocation failure — always check the return value.
 *
 * Performance:
 * - Time: O(len)
 * - Space: len + 1 bytes allocated on the heap
 */
static inline char* dynstring_to_cstr(const DynString* s) {
    const char* src = (s && s->data) ? s->data : "";
    usize len       = (s && s->data) ? s->len  : 0;

    char* copy = (char*)malloc(len + 1);
    if (!copy) return NULL;  /* OOM — caller must check */
    mem_copy(copy, src, len + 1);
    return copy;
}

#endif /* CANON_DATA_CONVENIENCE_DYNSTRING_H */
