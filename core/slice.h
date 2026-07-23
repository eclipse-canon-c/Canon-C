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

/**
 * @file slice.h
 * @brief Non-owning views into contiguous memory — the canonical "borrow" primitive
 *
 * Provides the canonical {pointer, length} borrow types for Canon-C:
 * `bytes_t` (mutable byte view), `cbytes_t` (read-only byte view),
 * `str_t` (read-only character view), and the `DEFINE_SLICE(T)` macro
 * for typed element views.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicitness: Slice = pointer + length. No hidden capacity, no metadata.
 * - Non-ownership: Slices view memory; backing buffer must outlive every slice.
 * - Bounds safety: All operations are bounds-checked and NULL-safe except
 *   the *_unchecked variants (debug-checked via ensure_msg, UB in release).
 * - Type invariant: ptr != NULL || len == 0 — the empty case allows NULL,
 *   any non-empty slice has a real backing buffer.
 *
 * Types provided:
 * ────────────────────────────────────────────────────────────────────────────
 * - bytes_t   — mutable untyped byte view
 * - cbytes_t  — read-only untyped byte view
 * - str_t     — read-only character view (no null terminator required)
 * - DEFINE_SLICE(T) — typed element view, generated per type
 *
 * Operations covered:
 * ────────────────────────────────────────────────────────────────────────────
 * - Construction: from(ptr, len), empty(), from_cstr() (str_t only)
 * - Queries: is_empty, at, equal, starts_with / ends_with (str_t only)
 * - Slicing: slice(start, end), take(n), skip(n) — all O(1), no copy
 * - Conversion: bytes_as_const, str_as_bytes, slice_T_as_bytes/as_cbytes
 *
 * Formal verification:
 * ────────────────────────────────────────────────────────────────────────────
 * Every non-macro function in this header carries an ACSL contract suitable
 * for Frama-C WP. The verification uses `-wp-model Typed+Cast` to permit
 * the void* to u8* casts in bytes_from / cbytes_from — the same model as
 * compare.h (VERIFY-005) and ptr.h (VERIFY-006).
 *
 * Type invariants are encoded as named ACSL predicates (`bytes_invariant`,
 * `cbytes_invariant`, `str_invariant`) and validity predicates with `{L}`
 * labels (`bytes_valid`, `cbytes_valid`, `str_valid`). These are the lemmas
 * WP uses to discharge the four `!ptr` defensive branches in bytes_slice,
 * bytes_skip, str_slice, and str_skip as unreachable — closing the gap
 * documented in MCDC-002 (docs/deviations.md).
 *
 * Equality functions (bytes_equal, str_equal, str_starts_with, str_ends_with)
 * carry partial functional specs only. Full equality semantics would require
 * memcmp-axiomatic reasoning that the project's WP setup has not yet
 * validated. The contracts prove range, length-mismatch, same-pointer
 * fast path, and absence of runtime errors. Full equality validated by
 * testing. See VERIFY-007 in docs/deviations.md.
 *
 * Macro-generated functions (DEFINE_SLICE(T) — slice_T_from, slice_T_at,
 * slice_T_first, etc.) are NOT WP-verified in this baseline. The C
 * preprocessor strips ACSL annotation comments inside #define bodies
 * before macro expansion, so contracts cannot be carried into the
 * generated code through the macro. The contract comments inside the
 * macro body are retained as human-readable documentation; they will
 * convert to real ACSL the day a separate slice_verify.h instantiates
 * DEFINE_SLICE(i32) for analysis. Until then, slice_T_* functions are
 * validated by testing and fuzzing only. See VERIFY-007 for the full
 * scope statement.
 *
 * When Frama-C is running, `__FRAMAC__` is defined automatically. slice.h
 * uses this transitively through contract.h, where the contract handler
 * body is replaced by a non-terminating loop to hide stdio from WP while
 * preserving the `ensures \false` / `exits \false` contract.
 *
 * Verification status (as of v1.3.x baseline):
 *   - bytes_t / cbytes_t / str_t functions: WP-verified, see verification.md
 *   - DEFINE_SLICE(T) macro: documentation only, see VERIFY-007
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - All slicing operations: O(1) — pointer + length adjustment, no copy
 * - Equality / starts_with / ends_with: O(n) — single memcmp pass
 * - str_from_cstr: O(n) — single strlen pass
 * - All other operations: O(1)
 * - All functions are static inline → zero call overhead
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Reading from a slice is safe to do from multiple threads concurrently
 * provided the backing buffer is not being mutated. Writing through a
 * mutable slice (bytes_t, slice_T) requires external synchronization
 * if concurrent readers exist. The slice value itself (the {ptr, len}
 * pair) is trivially copyable and passed by value — no synchronization
 * needed for slice values.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (designated initializers, inline functions,
 *   stdint.h, stdbool.h via types.h)
 * - <string.h> for memcmp / strlen — standard since C89
 * - No platform-specific code
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Buffer parameters: `void process(cbytes_t input, bytes_t output)`
 *   instead of pointer + length pair, eliminates length/pointer mismatch
 * - Substring extraction: `str_t name = str_slice(line, 0, comma_pos)`
 *   — zero-copy, lifetime tied to backing buffer
 * - Typed element views: `DEFINE_SLICE(i32); slice_i32 row = ...`
 *   — bounds-checked indexing without per-call length parameter
 * - Read-only borrows: `cbytes_t header = bytes_as_const(buf);`
 *   — explicit conversion, prevents accidental mutation through alias
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Owned buffers (use vec, dynvec, or arena-backed allocations)
 * - Mutable string operations across slice boundaries (slices are views,
 *   not buffers; mutation requires the backing buffer)
 * - Storage that outlives the function scope unless backed by a known-
 *   lifetime allocator (arena with explicit lifetime, static buffer, etc.)
 *
 * @sa types.h, contract.h, ptr.h, semantics/borrow.h
 */

#ifndef CANON_CORE_SLICE_H
#define CANON_CORE_SLICE_H

#include <string.h>

#include "core/primitives/types.h"
#include "core/primitives/contract.h"

/* ============================================================================
 * Type definitions
 *
 * The {ptr, len} layout is fixed and public. Direct field access is
 * permitted and idiomatic — slices are not opaque types. The defensive
 * `!ptr` checks in slice operations exist precisely to handle the case
 * where a caller constructs a malformed value through direct struct init
 * (e.g. `bytes_t b = {NULL, 5};`), bypassing the bytes_from contract.
 *
 * These four defensive branches in bytes_slice, bytes_skip, str_slice,
 * and str_skip are documented in MCDC-002 as API-unreachable in MC/DC
 * isolation. WP discharges them statically under the type invariant.
 * ========================================================================= */

/** Mutable non-owning byte view. Invariant: ptr != NULL || len == 0. */
typedef struct { u8*       ptr; usize len; } bytes_t;

/** Read-only non-owning byte view. Invariant: ptr != NULL || len == 0. */
typedef struct { const u8* ptr; usize len; } cbytes_t;

/** Read-only non-owning character view. No null terminator required.
 *  Invariant: ptr != NULL || len == 0. */
typedef struct { const char* ptr; usize len; } str_t;

/* ============================================================================
 * ACSL predicates
 *
 * These predicates encode the type invariants stated above as
 * machine-checkable formulas. WP uses them to:
 *   - prove that constructor outputs satisfy the invariant
 *   - discharge defensive `!ptr` checks as unreachable when the
 *     caller's input satisfies the invariant (the MCDC-002 closure)
 *
 * The validity predicates carry an `{L}` memory-state label and require
 * that when len > 0 the pointer range [ptr, ptr+len) is allocated and
 * accessible. The `_write` variant requires write access; the read-only
 * forms (cbytes_valid, str_valid) and the `_read` form for bytes_t use
 * \valid_read.
 *
 * Convention: Functions that read through the slice take the read-only
 * predicate as precondition; functions that mutate take the _write form.
 * Functions that only inspect the {ptr, len} layout (bytes_is_empty,
 * len queries) take only the bare invariant predicate.
 * ========================================================================= */

/*@ // ----- type invariants (no memory dependency) -----
    predicate bytes_invariant(bytes_t b) =
        b.ptr != \null || b.len == 0;

    predicate cbytes_invariant(cbytes_t b) =
        b.ptr != \null || b.len == 0;

    predicate str_invariant(str_t s) =
        s.ptr != \null || s.len == 0;

    // ----- validity: invariant + memory accessibility -----
    predicate bytes_valid_read{L}(bytes_t b) =
        bytes_invariant(b) &&
        (b.len > 0 ==> \valid_read(b.ptr + (0 .. b.len - 1)));

    predicate bytes_valid_write{L}(bytes_t b) =
        bytes_invariant(b) &&
        (b.len > 0 ==> \valid(b.ptr + (0 .. b.len - 1)));

    predicate cbytes_valid{L}(cbytes_t b) =
        cbytes_invariant(b) &&
        (b.len > 0 ==> \valid_read(b.ptr + (0 .. b.len - 1)));

    predicate str_valid{L}(str_t s) =
        str_invariant(s) &&
        (s.len > 0 ==> \valid_read(s.ptr + (0 .. s.len - 1)));
*/

/* ============================================================================
 * bytes_t — construction
 * ========================================================================= */

/**
 * @brief Wraps [ptr, ptr+len) as a mutable byte view
 *
 * @param ptr Backing buffer (may be NULL only if len == 0)
 * @param len Number of bytes in the view
 *
 * @return bytes_t with ptr cast to u8* and the supplied length
 *
 * @pre ptr != NULL || len == 0 (asserted via require_msg in debug builds)
 *
 * @remark Does not copy. Caller retains ownership of the backing buffer.
 *         The returned view is valid only as long as ptr remains live.
 *
 * Example:
 * ```c
 * u8 buf[256];
 * bytes_t view = bytes_from(buf, sizeof(buf));
 * ```
 *
 * @sa bytes_empty(), cbytes_from()
 */
/*@
    requires ptr != \null || len == 0;
    requires len > 0 ==> \valid((u8 *)ptr + (0 .. len - 1));
    assigns  \nothing;
    ensures  \result.ptr == (u8 *)ptr;
    ensures  \result.len == len;
    ensures  bytes_invariant(\result);
 */
static inline bytes_t bytes_from(void* ptr, usize len) {
    require_msg((ptr != NULL) || (len == 0u), "bytes_from: NULL ptr with non-zero len");
    return (bytes_t){ .ptr = (u8*)ptr, .len = len };
}

/**
 * @brief Returns an empty bytes_t (ptr == NULL, len == 0)
 *
 * @return bytes_t with both fields zero
 *
 * @remark Equivalent to `(bytes_t){NULL, 0}`. Useful as a sentinel
 *         return value or to initialize a default-constructed slice.
 *
 * @sa bytes_from(), bytes_is_empty()
 */
/*@
    assigns \nothing;
    ensures \result.ptr == \null;
    ensures \result.len == 0;
    ensures bytes_invariant(\result);
 */
static inline bytes_t bytes_empty(void) {
    return (bytes_t){ .ptr = NULL, .len = 0 };
}

/**
 * @brief Wraps [ptr, ptr+len) as a read-only byte view
 *
 * @param ptr Backing buffer (may be NULL only if len == 0)
 * @param len Number of bytes in the view
 *
 * @return cbytes_t with ptr cast to const u8* and the supplied length
 *
 * @pre ptr != NULL || len == 0
 *
 * @sa cbytes_empty(), bytes_as_const()
 */
/*@
    requires ptr != \null || len == 0;
    requires len > 0 ==> \valid_read((const u8 *)ptr + (0 .. len - 1));
    assigns  \nothing;
    ensures  \result.ptr == (const u8 *)ptr;
    ensures  \result.len == len;
    ensures  cbytes_invariant(\result);
 */
static inline cbytes_t cbytes_from(const void* ptr, usize len) {
    require_msg((ptr != NULL) || (len == 0u), "cbytes_from: NULL ptr with non-zero len");
    return (cbytes_t){ .ptr = (const u8*)ptr, .len = len };
}

/**
 * @brief Returns an empty cbytes_t (ptr == NULL, len == 0)
 *
 * @sa cbytes_from(), bytes_empty()
 */
/*@
    assigns \nothing;
    ensures \result.ptr == \null;
    ensures \result.len == 0;
    ensures cbytes_invariant(\result);
 */
static inline cbytes_t cbytes_empty(void) {
    return (cbytes_t){ .ptr = NULL, .len = 0 };
}

/**
 * @brief Adds const: converts bytes_t to cbytes_t
 *
 * @param b Mutable byte view
 *
 * @return cbytes_t pointing to the same memory, read-only
 *
 * @remark Zero-cost conversion (compiles to a no-op). Useful when
 *         passing a mutable view to a function that requires read-only.
 *
 * @sa bytes_from(), cbytes_from()
 */
/*@
    requires bytes_invariant(b);
    assigns  \nothing;
    ensures  \result.ptr == b.ptr;
    ensures  \result.len == b.len;
    ensures  cbytes_invariant(\result);
 */
static inline cbytes_t bytes_as_const(bytes_t b) {
    return (cbytes_t){ .ptr = b.ptr, .len = b.len };
}

/* ============================================================================
 * bytes_t — queries
 * ========================================================================= */

/**
 * @brief Returns true if len == 0
 *
 * @param b The view to check
 *
 * @return true iff b.len == 0 (regardless of ptr)
 *
 * @remark Both `bytes_empty()` and `bytes_from(buf, 0)` are empty.
 */
/*@
    assigns \nothing;
    ensures \result <==> b.len == 0;
 */
static inline bool bytes_is_empty(bytes_t b) {
    return b.len == 0u;
}

/**
 * @brief Returns pointer to byte at index i, or NULL if out of bounds
 *
 * @param b The view
 * @param i Byte index (must be < b.len for non-NULL result)
 *
 * @return Pointer to b.ptr[i], or NULL if b.ptr == NULL or i >= b.len
 *
 * @remark Returns a writable pointer; mutating through the result
 *         affects the backing buffer.
 *
 * Example:
 * ```c
 * u8* p = bytes_at(view, 5);
 * if (p) *p = 0xFF;
 * ```
 */
/*@
    requires bytes_valid_write(b);
    assigns  \nothing;
    behavior in_bounds:
        assumes b.ptr != \null && i < b.len;
        ensures \result == b.ptr + i;
    behavior out_of_bounds:
        assumes b.ptr == \null || i >= b.len;
        ensures \result == \null;
    complete behaviors;
    disjoint behaviors;
 */
static inline u8* bytes_at(bytes_t b, usize i) {
    if (!b.ptr || (i >= b.len)) { return NULL; }
    return b.ptr + i;
}

/**
 * @brief Returns true if a and b have identical contents (O(n))
 *
 * @param a First view
 * @param b Second view
 *
 * @return true iff a.len == b.len and bytewise contents match
 *
 * @remark The contract proves structural properties (range,
 *         length-mismatch returns false, same-pointer fast path returns
 *         true) and absence of runtime errors. Full equality semantics
 *         require memcmp-axiomatic reasoning deferred to testing.
 *         See VERIFY-007 for scope.
 *
 * @note On the compare path both views must contain initialized bytes
 *       (ACSL \initialized precondition; the Valgrind CI job enforces
 *       the same property at runtime).
 *
 * @sa str_equal()
 */
/*@
    requires bytes_valid_write(a);
    requires bytes_valid_write(b);
    requires init_ab: (a.len == b.len && a.ptr != b.ptr &&
                       a.ptr != \null && b.ptr != \null && a.len > 0) ==>
             (\initialized((char*)a.ptr + (0 .. (integer)a.len - 1)) &&
              \initialized((char*)b.ptr + (0 .. (integer)b.len - 1)));
    assigns  \nothing;
    ensures  \result == \true || \result == \false;
    ensures  a.len != b.len ==> \result == \false;
    ensures  (a.ptr == b.ptr && a.len == b.len) ==> \result == \true;
 */
static inline bool bytes_equal(bytes_t a, bytes_t b) {
    if (a.len != b.len) { return false; }
    if (a.ptr == b.ptr) { return true; }  /* same pointer — covers two bytes_empty() */
    if (!a.ptr || !b.ptr) { return false; }
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

/* ============================================================================
 * bytes_t — slicing (no copy)
 * ========================================================================= */

/**
 * @brief Returns sub-view [start, end). end is clamped to b.len. O(1)
 *
 * @param b The source view
 * @param start Inclusive start index
 * @param end Exclusive end index (clamped to b.len)
 *
 * @return Sub-view, or empty if start >= b.len, start >= end, or b.ptr is NULL
 *
 * @remark No allocation, no copy — result shares b's backing buffer.
 *
 * Example:
 * ```c
 * bytes_t mid = bytes_slice(view, 4, 8);   // bytes 4..7
 * bytes_t tail = bytes_slice(view, 4, 999); // bytes 4 to end (clamped)
 * ```
 */
/*@
    requires bytes_valid_write(b);
    assigns  \nothing;
    ensures  bytes_invariant(\result);
    ensures  \result.len <= b.len;
    behavior in_range:
        assumes b.ptr != \null && start < b.len && start < end;
        ensures \result.ptr == b.ptr + start;
        ensures \result.len == (end <= b.len ? end - start : b.len - start);
    behavior out_of_range:
        assumes b.ptr == \null || start >= b.len || start >= end;
        ensures \result.ptr == \null;
        ensures \result.len == 0;
    complete behaviors;
    disjoint behaviors;
 */
static inline bytes_t bytes_slice(bytes_t b, usize start, usize end) {
    if (!b.ptr || (start >= b.len)) { return bytes_empty(); }
    if (end > b.len) { end = b.len; }
    if (start >= end) { return bytes_empty(); }
    return (bytes_t){ .ptr = b.ptr + start, .len = end - start };
}

/**
 * @brief Returns first n bytes. n is clamped to b.len. O(1)
 *
 * @param b The source view
 * @param n Number of bytes to take (clamped to b.len)
 *
 * @return Sub-view of the first min(n, b.len) bytes
 */
/*@
    requires bytes_valid_write(b);
    assigns  \nothing;
    ensures  bytes_invariant(\result);
    ensures  \result.len <= b.len;
    ensures  \result.len <= n;
 */
static inline bytes_t bytes_take(bytes_t b, usize n) {
    return bytes_slice(b, 0, n);
}

/**
 * @brief Returns bytes after skipping the first n. O(1)
 *
 * @param b The source view
 * @param n Number of bytes to skip
 *
 * @return Sub-view of bytes [n, b.len), or empty if n >= b.len
 */
/*@
    requires bytes_valid_write(b);
    assigns  \nothing;
    ensures  bytes_invariant(\result);
    ensures  \result.len <= b.len;
    behavior in_range:
        assumes b.ptr != \null && n < b.len;
        ensures \result.ptr == b.ptr + n;
        ensures \result.len == b.len - n;
    behavior out_of_range:
        assumes b.ptr == \null || n >= b.len;
        ensures \result.ptr == \null;
        ensures \result.len == 0;
    complete behaviors;
    disjoint behaviors;
 */
static inline bytes_t bytes_skip(bytes_t b, usize n) {
    if (!b.ptr || (n >= b.len)) { return bytes_empty(); }
    return (bytes_t){ .ptr = b.ptr + n, .len = b.len - n };
}

/* ============================================================================
 * str_t — construction
 * ========================================================================= */

/**
 * @brief Creates str_t from pointer and explicit length
 *
 * @param ptr Backing string (may be NULL only if len == 0)
 * @param len Number of characters in the view (no null terminator required)
 *
 * @pre ptr != NULL || len == 0
 *
 * @remark Does not require null termination. The view covers exactly len
 *         characters starting at ptr.
 */
/*@
    requires ptr != \null || len == 0;
    requires len > 0 ==> \valid_read(ptr + (0 .. len - 1));
    assigns  \nothing;
    ensures  \result.ptr == ptr;
    ensures  \result.len == len;
    ensures  str_invariant(\result);
 */
static inline str_t str_from(const char* ptr, usize len) {
    require_msg((ptr != NULL) || (len == 0u), "str_from: NULL ptr with non-zero len");
    return (str_t){ .ptr = ptr, .len = len };
}

/**
 * @brief Creates str_t from a null-terminated C string. O(n) via strlen
 *
 * @param cstr Null-terminated C string, or NULL
 *
 * @return str_t covering cstr through (but not including) the null
 *         terminator, or empty str_t if cstr is NULL
 *
 * @remark The contract specifies pointer and length-equality but does
 *         not constrain `\result.len` to `strlen(cstr)` because the
 *         project's WP setup has not validated the strlen logic
 *         function. Full functional spec deferred to testing — see
 *         VERIFY-007.
 */
/*@
    assigns  \nothing;
    ensures  str_invariant(\result);
    behavior null_input:
        assumes cstr == \null;
        ensures \result.ptr == \null;
        ensures \result.len == 0;
    behavior valid_input:
        assumes cstr != \null;
        ensures \result.ptr == cstr;
    complete behaviors;
    disjoint behaviors;
 */
static inline str_t str_from_cstr(const char* cstr) {
    if (!cstr) { return (str_t){ .ptr = NULL, .len = 0 }; }
    return (str_t){ .ptr = cstr, .len = (usize)strlen(cstr) };
}

/**
 * @brief Returns an empty str_t (ptr == NULL, len == 0)
 *
 * @sa str_from(), str_from_cstr()
 */
/*@
    assigns \nothing;
    ensures \result.ptr == \null;
    ensures \result.len == 0;
    ensures str_invariant(\result);
 */
static inline str_t str_empty(void) {
    return (str_t){ .ptr = NULL, .len = 0 };
}

/* ============================================================================
 * str_t — queries
 * ========================================================================= */

/**
 * @brief Returns true if len == 0
 */
/*@
    assigns \nothing;
    ensures \result <==> s.len == 0;
 */
static inline bool str_is_empty(str_t s) {
    return s.len == 0u;
}

/**
 * @brief Returns true if a and b have identical contents (O(n))
 *
 * @remark Partial functional spec — see bytes_equal note and VERIFY-007.
 * @note On the compare path both views must contain initialized bytes
 *       (ACSL \initialized precondition; the Valgrind CI job enforces
 *       the same property at runtime).
 *
 * @sa bytes_equal()
 */
/*@
    requires str_valid(a);
    requires str_valid(b);
    requires init_ab: (a.len == b.len && a.ptr != b.ptr &&
                       a.ptr != \null && b.ptr != \null && a.len > 0) ==>
             (\initialized((char*)a.ptr + (0 .. (integer)a.len - 1)) &&
              \initialized((char*)b.ptr + (0 .. (integer)b.len - 1)));
    assigns  \nothing;
    ensures  \result == \true || \result == \false;
    ensures  a.len != b.len ==> \result == \false;
    ensures  (a.ptr == b.ptr && a.len == b.len) ==> \result == \true;
 */
static inline bool str_equal(str_t a, str_t b) {
    if (a.len != b.len) { return false; }
    if (a.ptr == b.ptr) { return true; }
    if (!a.ptr || !b.ptr) { return false; }
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

/**
 * @brief Returns true if s begins with prefix. O(prefix.len)
 *
 * @remark Partial functional spec — see bytes_equal note and VERIFY-007.
 * @note On the compare path the first prefix.len bytes of both views must
 *       contain initialized bytes (ACSL \initialized precondition; the
 *       Valgrind CI job enforces the same property at runtime).
 */
/*@
    requires str_valid(s);
    requires str_valid(prefix);
    requires init_sp: (prefix.len <= s.len && prefix.ptr != \null &&
                       prefix.len > 0) ==>
             (\initialized((char*)s.ptr + (0 .. (integer)prefix.len - 1)) &&
              \initialized((char*)prefix.ptr + (0 .. (integer)prefix.len - 1)));
    assigns  \nothing;
    ensures  \result == \true || \result == \false;
    ensures  prefix.len > s.len ==> \result == \false;
    ensures  prefix.len == 0    ==> \result == \true;
 */
static inline bool str_starts_with(str_t s, str_t prefix) {
    if (prefix.len > s.len) { return false; }
    if (!prefix.ptr || (prefix.len == 0u)) { return true; }
    return memcmp(s.ptr, prefix.ptr, prefix.len) == 0;
}

/**
 * @brief Returns true if s ends with suffix. O(suffix.len)
 *
 * @remark Partial functional spec — see bytes_equal note and VERIFY-007.
 * @note On the compare path the trailing suffix.len bytes of s and all of
 *       suffix must contain initialized bytes (ACSL \initialized
 *       precondition; the Valgrind CI job enforces the same property at
 *       runtime). Only the compared window of s carries the obligation.
 */
/*@
    requires str_valid(s);
    requires str_valid(suffix);
    requires init_ss: (suffix.len <= s.len && suffix.ptr != \null &&
                       suffix.len > 0) ==>
             (\initialized((char*)s.ptr + ((integer)s.len - (integer)suffix.len ..
                                            (integer)s.len - 1)) &&
              \initialized((char*)suffix.ptr + (0 .. (integer)suffix.len - 1)));
    assigns  \nothing;
    ensures  \result == \true || \result == \false;
    ensures  suffix.len > s.len ==> \result == \false;
    ensures  suffix.len == 0    ==> \result == \true;
 */
static inline bool str_ends_with(str_t s, str_t suffix) {
    if (suffix.len > s.len) { return false; }
    if (!suffix.ptr || (suffix.len == 0u)) { return true; }
    return memcmp(s.ptr + (s.len - suffix.len), suffix.ptr, suffix.len) == 0;
}

/* ============================================================================
 * str_t — slicing (no copy)
 * ========================================================================= */

/**
 * @brief Returns sub-view [start, end). end is clamped to s.len. O(1)
 */
/*@
    requires str_valid(s);
    assigns  \nothing;
    ensures  str_invariant(\result);
    ensures  \result.len <= s.len;
    behavior in_range:
        assumes s.ptr != \null && start < s.len && start < end;
        ensures \result.ptr == s.ptr + start;
        ensures \result.len == (end <= s.len ? end - start : s.len - start);
    behavior out_of_range:
        assumes s.ptr == \null || start >= s.len || start >= end;
        ensures \result.ptr == \null;
        ensures \result.len == 0;
    complete behaviors;
    disjoint behaviors;
 */
static inline str_t str_slice(str_t s, usize start, usize end) {
    if (!s.ptr || (start >= s.len)) { return str_empty(); }
    if (end > s.len) { end = s.len; }
    if (start >= end) { return str_empty(); }
    return (str_t){ .ptr = s.ptr + start, .len = end - start };
}

/**
 * @brief Returns first n characters. n is clamped to s.len. O(1)
 */
/*@
    requires str_valid(s);
    assigns  \nothing;
    ensures  str_invariant(\result);
    ensures  \result.len <= s.len;
    ensures  \result.len <= n;
 */
static inline str_t str_take(str_t s, usize n) {
    return str_slice(s, 0, n);
}

/**
 * @brief Returns characters after skipping the first n. O(1)
 */
/*@
    requires str_valid(s);
    assigns  \nothing;
    ensures  str_invariant(\result);
    ensures  \result.len <= s.len;
    behavior in_range:
        assumes s.ptr != \null && n < s.len;
        ensures \result.ptr == s.ptr + n;
        ensures \result.len == s.len - n;
    behavior out_of_range:
        assumes s.ptr == \null || n >= s.len;
        ensures \result.ptr == \null;
        ensures \result.len == 0;
    complete behaviors;
    disjoint behaviors;
 */
static inline str_t str_skip(str_t s, usize n) {
    if (!s.ptr || (n >= s.len)) { return str_empty(); }
    return (str_t){ .ptr = s.ptr + n, .len = s.len - n };
}

/**
 * @brief Reinterprets str_t as a read-only byte view. O(1)
 *
 * @remark Zero-cost conversion. The byte view covers the same memory
 *         as the str_t with the same length.
 */
/*@
    requires str_invariant(s);
    assigns  \nothing;
    ensures  \result.ptr == (const u8 *)s.ptr;
    ensures  \result.len == s.len;
    ensures  cbytes_invariant(\result);
 */
static inline cbytes_t str_as_bytes(str_t s) {
    return (cbytes_t){ .ptr = (const u8*)s.ptr, .len = s.len };
}

/* ============================================================================
 * DEFINE_SLICE(type) — typed non-owning element view
 *
 * Generates per-type:
 *   slice_T                         struct { T* ptr; usize len; }
 *   slice_T_from(ptr, len)          → slice_T
 *   slice_T_empty()                 → slice_T
 *   slice_T_len(s)                  → usize
 *   slice_T_is_empty(s)             → bool
 *   slice_T_get(s, i, out)          → bool  (bounds-checked, copies element)
 *   slice_T_get_unchecked(s, i)     → T     (debug-checked — UB in release)
 *   slice_T_at(s, i)                → T*    (NULL if OOB)
 *   slice_T_first(s)                → T*    (NULL if empty)
 *   slice_T_last(s)                 → T*    (NULL if empty)
 *   slice_T_slice(s, start, end)    → slice_T
 *   slice_T_take(s, n)              → slice_T
 *   slice_T_skip(s, n)              → slice_T
 *   slice_T_as_bytes(s)             → bytes_t   (mutable byte view)
 *   slice_T_as_cbytes(s)            → cbytes_t  (read-only byte view)
 *
 * Usage:
 *   DEFINE_SLICE(int)
 *   int arr[4] = {1,2,3,4};
 *   slice_int sv = slice_int_from(arr, 4);
 *   int val;
 *   slice_int_get(sv, 2, &val);   // val == 3
 *
 * For pointer types, typedef first:
 *   typedef void* voidptr;
 *   DEFINE_SLICE(voidptr)
 *
 * Verification scope (VERIFY-007):
 * ─────────────────────────────────────────────────────────────────────────
 * The macro-generated functions are NOT WP-verified in the current baseline.
 * The C preprocessor strips comments inside #define bodies before macro
 * expansion, so ACSL annotations cannot be carried into the generated code
 * through the macro itself. The contract comments below are retained as
 * human-readable specifications and are intended to be machine-checkable
 * the day a separate slice_verify.h instantiates DEFINE_SLICE for analysis.
 *
 * Until then, slice_T_* functions are validated by:
 *   - Unit testing against slice_test.c (bytes_t/cbytes_t/str_t conventions
 *     give symmetric coverage)
 *   - Fuzzing through the slice_test fuzz harness
 *   - 100% MC/DC coverage (when measured via the i32 instantiation)
 *
 * The semantics of every macro-generated function exactly mirror its
 * bytes_t / str_t counterpart, so the WP discharge of the non-macro
 * functions provides indirect evidence for the macro-generated ones —
 * any bug in the macro body would be detectable by comparing observable
 * behavior against the verified bytes_t baseline.
 * ========================================================================= */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_SLICE(type)                                                          \
                                                                                    \
typedef struct { type* ptr; usize len; } slice_##type;                             \
                                                                                    \
/** Creates slice_##type from ptr and len. @pre ptr!=NULL||len==0                   \
 *                                                                                  \
 * Spec (VERIFY-007 — documentation only, not WP-verified):                         \
 *   requires ptr != \null || len == 0;                                             \
 *   requires len > 0 ==> \valid(ptr + (0 .. len - 1));                             \
 *   ensures  \result.ptr == ptr && \result.len == len;                             \
 *   assigns  \nothing;                                                             \
 */                                                                                 \
static inline slice_##type slice_##type##_from(type* ptr, usize len) {             \
    require_msg(ptr != NULL || len == 0,                                            \
        "slice_" #type "_from: NULL ptr with non-zero len");                        \
    return (slice_##type){ .ptr = ptr, .len = len };                                \
}                                                                                   \
                                                                                    \
/** Returns empty slice_##type (ptr==NULL, len==0).                                 \
 *                                                                                  \
 * Spec: ensures \result.ptr == \null && \result.len == 0; assigns \nothing;        \
 */                                                                                 \
static inline slice_##type slice_##type##_empty(void) {                            \
    return (slice_##type){ .ptr = NULL, .len = 0 };                                 \
}                                                                                   \
                                                                                    \
/** Returns element count.                                                          \
 *                                                                                  \
 * Spec: ensures \result == s.len; assigns \nothing;                                \
 */                                                                                 \
static inline usize slice_##type##_len(slice_##type s) {                           \
    return s.len;                                                                   \
}                                                                                   \
                                                                                    \
/** Returns true if len == 0.                                                       \
 *                                                                                  \
 * Spec: ensures \result <==> s.len == 0; assigns \nothing;                         \
 */                                                                                 \
static inline bool slice_##type##_is_empty(slice_##type s) {                       \
    return s.len == 0;                                                              \
}                                                                                   \
                                                                                    \
/** Copies element at i into *out. Returns false if out==NULL or i>=len.            \
 *                                                                                  \
 * Spec:                                                                            \
 *   requires s.ptr != \null || s.len == 0;                                         \
 *   requires s.len > 0 ==> \valid(s.ptr + (0 .. s.len - 1));                       \
 *   requires out == \null || \valid(out);                                          \
 *   assigns  *out;                                                                 \
 *   ensures (s.ptr != \null && out != \null && i < s.len) ==>                      \
 *           (\result == \true && *out == s.ptr[i]);                                \
 *   ensures (s.ptr == \null || out == \null || i >= s.len) ==> \result == \false;  \
 */                                                                                 \
static inline bool slice_##type##_get(slice_##type s, usize i, type* out) {        \
    if (!s.ptr || !out || i >= s.len) { return false; }                             \
    *out = s.ptr[i]; return true;                                                   \
}                                                                                   \
                                                                                    \
/** Returns element at i. Debug-only bounds check — UB in release on bad input.     \
 *                                                                                  \
 * Spec:                                                                            \
 *   requires s.ptr != \null && i < s.len;                                          \
 *   requires \valid(s.ptr + (0 .. s.len - 1));                                     \
 *   ensures  \result == s.ptr[i]; assigns \nothing;                                \
 */                                                                                 \
static inline type slice_##type##_get_unchecked(slice_##type s, usize i) {         \
    ensure_msg(s.ptr != NULL, "slice_" #type "_get_unchecked: NULL ptr");           \
    ensure_msg(i < s.len,     "slice_" #type "_get_unchecked: index out of bounds");\
    return s.ptr[i];                                                                \
}                                                                                   \
                                                                                    \
/** Returns pointer to element at i, or NULL if out of bounds.                      \
 *                                                                                  \
 * Spec: behavior in_bounds, behavior out_of_bounds; complete; disjoint;            \
 *       ensures returns s.ptr + i or \null; assigns \nothing;                      \
 */                                                                                 \
static inline type* slice_##type##_at(slice_##type s, usize i) {                   \
    if (!s.ptr || i >= s.len) { return NULL; }                                      \
    return &s.ptr[i];                                                               \
}                                                                                   \
                                                                                    \
/** Returns pointer to first element, or NULL if empty.                             \
 *                                                                                  \
 * Spec: behavior non_empty: ensures \result == s.ptr;                              \
 *       behavior empty:     ensures \result == \null; complete; disjoint;          \
 */                                                                                 \
static inline type* slice_##type##_first(slice_##type s) {                         \
    return (s.ptr && s.len > 0) ? &s.ptr[0] : NULL;                                \
}                                                                                   \
                                                                                    \
/** Returns pointer to last element, or NULL if empty.                              \
 *                                                                                  \
 * Spec: behavior non_empty: ensures \result == s.ptr + s.len - 1;                  \
 *       behavior empty:     ensures \result == \null; complete; disjoint;          \
 */                                                                                 \
static inline type* slice_##type##_last(slice_##type s) {                          \
    return (s.ptr && s.len > 0) ? &s.ptr[s.len - 1] : NULL;                        \
}                                                                                   \
                                                                                    \
/** Returns sub-view [start, end). end clamped to s.len. O(1).                      \
 *                                                                                  \
 * Spec mirrors bytes_slice — see VERIFY-007.                                       \
 */                                                                                 \
static inline slice_##type slice_##type##_slice(                                    \
        slice_##type s, usize start, usize end) {                                   \
    if (!s.ptr || start >= s.len) { return slice_##type##_empty(); }                \
    if (end > s.len) { end = s.len; }                                               \
    if (start >= end) { return slice_##type##_empty(); }                            \
    return (slice_##type){ .ptr = s.ptr + start, .len = end - start };              \
}                                                                                   \
                                                                                    \
/** Returns first n elements. n clamped to s.len. O(1).                             \
 *                                                                                  \
 * Spec mirrors bytes_take — see VERIFY-007.                                        \
 */                                                                                 \
static inline slice_##type slice_##type##_take(slice_##type s, usize n) {          \
    return slice_##type##_slice(s, 0, n);                                           \
}                                                                                   \
                                                                                    \
/** Returns elements after skipping first n. O(1).                                  \
 *                                                                                  \
 * Spec mirrors bytes_skip — see VERIFY-007.                                        \
 */                                                                                 \
static inline slice_##type slice_##type##_skip(slice_##type s, usize n) {          \
    if (!s.ptr || n >= s.len) { return slice_##type##_empty(); }                    \
    return (slice_##type){ .ptr = s.ptr + n, .len = s.len - n };                    \
}                                                                                   \
                                                                                    \
/** Mutable raw byte view over backing memory. len = s.len * sizeof(type).          \
 *                                                                                  \
 * Spec: requires s.ptr != \null || s.len == 0;                                     \
 *       requires (integer)s.len * (integer)sizeof(type) <= CANON_USIZE_MAX;        \
 *       ensures  \result.len == s.len * sizeof(type);                              \
 *       assigns  \nothing; — see VERIFY-007.                                       \
 */                                                                                 \
static inline bytes_t slice_##type##_as_bytes(slice_##type s) {                    \
    if (!s.ptr) { return bytes_empty(); }                                           \
    return (bytes_t){ .ptr = (u8*)s.ptr, .len = s.len * sizeof(type) };            \
}                                                                                   \
                                                                                    \
/** Read-only raw byte view over backing memory. len = s.len * sizeof(type).        \
 *                                                                                  \
 * Spec mirrors slice_T_as_bytes — see VERIFY-007.                                  \
 */                                                                                 \
static inline cbytes_t slice_##type##_as_cbytes(slice_##type s) {                  \
    if (!s.ptr) { return (cbytes_t){ .ptr = NULL, .len = 0 }; }                    \
    return (cbytes_t){ .ptr = (const u8*)s.ptr, .len = s.len * sizeof(type) };     \
}

#endif /* CANON_CORE_SLICE_H */
