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

#ifndef CANON_SEMANTICS_BORROW_H
#define CANON_SEMANTICS_BORROW_H

#include "core/primitives/types.h"      /* usize, bool, u8              */
#include "core/primitives/contract.h"   /* require_msg                  */
#include "core/primitives/checked.h"    /* checked_mul                  */
#include "core/slice.h"                 /* str_t, cbytes_t, slice_##type */
#include "core/ownership.h"             /* borrowed(T) annotation style  */
#include <string.h>                     /* memcmp — used by borrowed_bytes_eq */

#ifdef CANON_LIFETIME_DEBUG
    #include "core/primitives/lifetime.h"  /* lifetime_t, region_id_t */
    #include "core/region.h"               /* lifetime_assert_valid   */
#endif

/**
 * @file semantics/borrow.h
 * @brief Semantic borrowing types — non-owning views with explicit lifetime intent
 *
 * Completes the ownership vocabulary from core/ownership.h.
 * ownership.h provides annotation macros (borrowed(T)).
 * borrow.h provides concrete, reusable types for common borrow patterns.
 *
 * Core ideas:
 * --------------------------------------------------------------------------
 * - All types are non-owning views. Never free the .ptr / .str / .bytes.
 * - An optional source tag (const void *) is carried for debugging and
 *   assertions. It is never dereferenced by this header.
 * - Lifetime rule: a borrowed value is valid only as long as its source
 *   is alive and the pointed-to memory is unmodified.
 * - Default mode: documentation + discipline, no runtime enforcement.
 * - CANON_LIFETIME_DEBUG mode: borrows additionally capture the source's
 *   lifetime ID at construction (via the _from_lifetime constructors) and
 *   validate it on every read via lifetime_assert_valid. See "Lifetime
 *   tracking" section below.
 *
 * Types provided:
 *   borrowed_ptr         — generic non-owning pointer to any T
 *   borrowed_str         — non-owning string view  (wraps str_t)
 *   borrowed_bytes       — non-owning byte view    (wraps cbytes_t)
 *   borrowed_slice_<T>   — non-owning typed slice  (wraps slice_<T>)
 *                          (instantiated via DEFINE_BORROWED_SLICE(type))
 *
 * Why separate from slice.h?
 * --------------------------------------------------------------------------
 * slice_t / str_t / cbytes_t are pure {ptr, len} data structures.
 * They carry no ownership intent — a returned slice might be owned or
 * borrowed.
 *
 * borrowed_* types make non-ownership explicit at the type level.
 * When you see borrowed_str in a signature, the reader immediately knows:
 *   - Do NOT free it.
 *   - Do NOT outlive its source.
 *
 * Lifetime model (documentation convention):
 * --------------------------------------------------------------------------
 *   borrowed_str s = stringbuf_borrow_str(&sb);
 *   // s is valid as long as sb is alive and unmodified.
 *   // s becomes invalid after: stringbuf_clear(&sb),
 *   //   arena_reset(sb.arena), free(sb), etc.
 *
 * Lifetime tracking (CANON_LIFETIME_DEBUG):
 * --------------------------------------------------------------------------
 * When the build defines CANON_LIFETIME_DEBUG, each borrow type embeds
 * two extra fields:
 *
 *   const lifetime_t *source_lt;   // pointer to source's embedded lifetime
 *   region_id_t       captured_id; // ID copied at construction time
 *
 * Use the *_from_lifetime constructors to capture an owner's lifetime:
 *
 *   borrowed_bytes b = borrowed_bytes_from_lifetime(
 *       arena_buffer(&arena), arena_used(&arena),
 *       &arena.lt, &arena);
 *
 *   // ... arena_reset(&arena) re-stamps arena.lt.id ...
 *
 *   borrowed_bytes_get(&b);   // fires require_msg in debug builds
 *
 * The classic *_from constructors remain available for borrows from non-
 * Canon-C sources (string literals, foreign data, anything without a
 * lifetime_t). Borrows constructed via *_from carry source_lt == NULL,
 * which the _get checks treat as "untracked — always valid."
 *
 * In default builds (CANON_LIFETIME_DEBUG undefined) the source_lt and
 * captured_id fields do not exist; the structs are byte-equivalent to
 * today's layout, the *_from_lifetime constructors compile to the same
 * code as *_from with the lifetime arguments ignored, and the _get
 * functions perform no extra work.
 *
 * Lifetime propagation through derived views:
 * --------------------------------------------------------------------------
 * Operations that produce a sub-borrow over the same underlying memory
 * (borrowed_str_slice, borrowed_bytes_slice, borrowed_slice_<T>_slice,
 * borrowed_slice_<T>_as_bytes) inherit the source's lifetime tracking via
 * BORROW_LT_INHERIT_. The substrate contract is "lifetime stays attached
 * to the underlying memory" — type erasure (e.g. slice_<T> → bytes) must
 * not silently strip the safety check.
 *
 * Source tag lifetime warning:
 * --------------------------------------------------------------------------
 * The source field stores the address of the owning object as a debug tag.
 * It is NEVER dereferenced by this header, but it can itself become a
 * dangling pointer if the owning object is destroyed while a borrowed value
 * is still in scope:
 *
 *   borrowed_str name;
 *   {
 *       MyStruct obj = {0};
 *       name = borrowed_str_from_cstr("hello", &obj);
 *   }
 *   // obj destroyed — name.source is now a dangling pointer.
 *   // name.str.ptr may still be valid (e.g. if it points to a string literal)
 *   // but name.source points to freed stack memory.
 *   // Do NOT pass name.source to a debugger or dereference it here.
 *
 * This is a known, unavoidable limitation of storing a raw address without
 * compiler-enforced lifetime tracking. The source tag is a human aid, not
 * a safety mechanism. Never dereference source outside of the owning
 * object's lifetime. In practice, if you pass &owning_struct as source and
 * pass the borrowed value only to functions called from the same scope,
 * the tag is always valid at the point of inspection.
 *
 * Under CANON_LIFETIME_DEBUG, the source_lt pointer has the same dangling
 * risk if the owning object is destroyed while the borrow is live. But
 * because the lifetime token's `open` flag is flipped to false at owner
 * destruction (in modules that opt in to the convention), the validity
 * check fires *before* dereferencing past the owner's lifetime in well-
 * structured code. The protection is best-effort, not absolute.
 *
 * NULL-safety contract:
 * --------------------------------------------------------------------------
 * All functions that receive a pointer-to-borrowed-X (const borrowed_X *b)
 * treat b == NULL as "absent borrow" and return a safe empty/null value,
 * EXCEPT where noted with require_msg. Functions that fire require_msg on
 * NULL are marked with @pre b != NULL in their documentation.
 *
 * The goal is: callers that do not check for NULL still get deterministic,
 * non-crashing behaviour in release builds where require_msg is a no-op,
 * while debug builds catch misuse early.
 *
 * Equality semantics:
 * --------------------------------------------------------------------------
 * borrowed_ptr_eq   — pointer identity (a.ptr == b.ptr).
 * borrowed_str_eq   — content equality via str_equal (same as strcmp-equal).
 * borrowed_bytes_eq — content equality via memcmp (same bytes, same length).
 * These differ because the underlying types have different natural equality.
 * Source tags and lifetime fields are always ignored by all _eq functions.
 *
 * Formal verification (Frama-C/WP):
 * --------------------------------------------------------------------------
 * The non-macro functions in this header carry ACSL contracts and are
 * verified with Frama-C/WP in the default configuration (CANON_LIFETIME
 * off, -DCANON_NO_REQUIRE -DNDEBUG) — i.e. the exact shipped bodies the
 * ABI guarantee describes; the BORROW_LT_* macros expand to nothing in
 * that configuration and the lifetime-debug path remains runtime-verified
 * by borrow_test.c across the CI matrix (see OWN-001 §7).
 *
 * The `source` field appears in no validity clause anywhere below: this
 * header never dereferences it (it is a debug tag), so to the specs it is
 * inert data — stored and propagated, never read through.
 *
 * DEFINE_BORROWED_SLICE is deliberately outside the verified surface —
 * macro-family disposition tracked in docs/vmacros.md alongside
 * DEFINE_SLICE (documented + tested, not WP-verified).
 *
 * See docs/deviations.md VERIFY-016 for the goal inventory and residuals.
 *
 * Dependency rule:
 * --------------------------------------------------------------------------
 * borrow.h lives in semantics/ and may include core/ only.
 * No data/, util/, or other semantics/ headers.
 * <string.h> is included for memcmp used in borrowed_bytes_eq.
 * Under CANON_LIFETIME_DEBUG, borrow.h additionally includes
 * core/primitives/lifetime.h (for the lifetime_t and region_id_t types)
 * and core/region.h (for the lifetime_assert_valid runtime check, which
 * lives in region.h because it depends on ensure_msg from contract.h).
 * The two includes are explicit so a reader can see what comes from where.
 *
 * Portability:
 * --------------------------------------------------------------------------
 * - Requires C99 or later (no GCC/Clang extensions used).
 * - No allocations, no platform-specific code.
 *
 * Performance:
 * --------------------------------------------------------------------------
 * - Default mode: each type is approximately sizeof(ptr) + sizeof(usize) +
 *   sizeof(void *).
 * - CANON_LIFETIME_DEBUG mode: each type grows by sizeof(const void*) +
 *   sizeof(region_id_t) (typically 16 bytes), and each _get adds two
 *   compares + one branch via lifetime_assert_valid.
 * - All functions are static inline — zero call overhead.
 *
 * Quick start:
 * --------------------------------------------------------------------------
 *   borrowed_str name = borrowed_str_from_cstr("Alice", NULL);
 *   printf("%.*s\n", (int)name.str.len, name.str.ptr);
 *
 *   u8 buf[256];
 *   borrowed_bytes region = borrowed_bytes_from(buf, 256u, buf);
 *
 *   MyStruct obj = {0};
 *   borrowed_ptr ref = borrowed_ptr_from(&obj, &obj);
 *   const MyStruct *p = (const MyStruct *)borrowed_ptr_get(&ref);
 *
 *   DEFINE_SLICE(int)
 *   DEFINE_BORROWED_SLICE(int)
 *   int arr[4] = {1, 2, 3, 4};
 *   borrowed_slice_int view =
 *       borrowed_slice_int_from(slice_int_from(arr, 4u), arr);
 *
 * @sa core/slice.h               — underlying str_t, cbytes_t, slice_##type
 * @sa core/ownership.h           — borrowed(T) annotation macros
 * @sa core/primitives/lifetime.h — lifetime_t, region_id_t (canonical home)
 * @sa core/region.h              — lifetime_assert_valid (runtime check)
 * @sa core/primitives/checked.h  — checked_mul used in _as_bytes overflow guard
 */

/* ============================================================================
   Internal helpers — lifetime field declaration and check
   ============================================================================
   When CANON_LIFETIME_DEBUG is enabled these macros expand to real fields
   and a real assertion call. Otherwise they vanish, leaving struct layouts
   and function bodies byte-identical to the default build.
   ============================================================================ */

#ifdef CANON_LIFETIME_DEBUG
    #define BORROW_LT_FIELDS_                       \
        const lifetime_t *source_lt;                \
        region_id_t       captured_id;
    #define BORROW_LT_INIT_(b, src_lt)              \
        do {                                         \
            (b).source_lt   = (src_lt);              \
            (b).captured_id = (src_lt) ? (src_lt)->id : REGION_ID_STATIC; \
        } while (0)
    #define BORROW_LT_INIT_NONE_(b)                 \
        do {                                         \
            (b).source_lt   = NULL;                  \
            (b).captured_id = REGION_ID_STATIC;      \
        } while (0)
    #define BORROW_LT_INHERIT_(dst, src)            \
        do {                                         \
            (dst).source_lt   = (src).source_lt;     \
            (dst).captured_id = (src).captured_id;   \
        } while (0)
    #define BORROW_LT_CHECK_(b, site)               \
        do {                                         \
            if ((b)->source_lt != NULL) {            \
                lifetime_assert_valid(               \
                    (b)->source_lt->id,              \
                    (b)->source_lt->open,            \
                    (b)->captured_id,                \
                    (site));                         \
            }                                        \
        } while (0)
#else
    #define BORROW_LT_FIELDS_                       /* no fields */
    #define BORROW_LT_INIT_(b, src_lt)        ((void)0)
    #define BORROW_LT_INIT_NONE_(b)           ((void)0)
    #define BORROW_LT_INHERIT_(dst, src)      ((void)0)
    #define BORROW_LT_CHECK_(b, site)         ((void)0)
#endif

/* ============================================================================
   ACSL verification predicates (Frama-C/WP)
   ============================================================================
   borrow.h defines no predicates of its own: the wrapped views reuse
   slice.h's exported predicates verbatim —

     cbytes_invariant(c) :  c.ptr != \null || c.len == 0
     str_invariant(s)    :  s.ptr != \null || s.len == 0
     str_valid(s)        :  str_invariant(s) &&
                            (s.len > 0 ==> \valid_read(s.ptr + ..))

   The invariant is the constructor-enforced shape: cbytes_from refuses
   {NULL, len>0} (it is a stated precondition there), and every function
   below preserves it. cbytes_invariant is exactly the fact needed to
   discharge the "impossible by construction" one-NULL guard in
   borrowed_bytes_eq as dead code (the MCDC-008 cross-stream argument —
   see the named assert dead_by_invariant in that function).

   Note the direction: the invariant permits a non-NULL pointer with
   len == 0 (see test_bytes_eq_zero_length_different_ptrs); only
   {NULL, len>0} is malformed.
   ============================================================================ */

/* ============================================================================
   borrowed_ptr — generic non-owning pointer
   ============================================================================ */

/**
 * @brief Generic non-owning pointer with optional source tag.
 *
 * Use when the pointee type is unknown or erased at the call site.
 * Prefer typed borrowed wrappers when the type is known.
 *
 * Invariants:
 *   - ptr may be NULL (absent borrow).
 *   - source is a debug-only tag; it is never dereferenced.
 *   - The object at ptr must outlive this struct.
 *
 * Under CANON_LIFETIME_DEBUG, additional fields (source_lt, captured_id)
 * carry the source's lifetime token at construction time. See file-level
 * "Lifetime tracking" section.
 *
 * @warning source can itself become a dangling pointer if the owning object
 *          is destroyed while this struct is still in scope. See "Source tag
 *          lifetime warning" in the file-level documentation.
 */
typedef struct {
    const void *ptr;    /**< Borrowed pointer — do NOT free. */
    const void *source; /**< Owning object's address (debug tag only).
                             Never dereference outside owning object's lifetime. */
    BORROW_LT_FIELDS_
} borrowed_ptr;

/**
 * @brief Constructs a borrowed_ptr from a raw pointer and an owner tag.
 * @param ptr    Non-owning pointer (may be NULL).
 * @param source Address of the owning object (debug tag; may be NULL).
 * @return       A borrowed_ptr wrapping ptr.
 *
 * In CANON_LIFETIME_DEBUG builds the borrow is constructed without a
 * tracked lifetime (source_lt == NULL); _get will skip the lifetime
 * check. Use borrowed_ptr_from_lifetime() to opt into tracking.
 */
/*@ assigns \nothing;
    ensures ptr_stored:    \result.ptr    == ptr;
    ensures source_stored: \result.source == source;
*/
static inline borrowed_ptr borrowed_ptr_from(const void *ptr,
                                              const void *source)
{
    borrowed_ptr b;
    b.ptr    = ptr;
    b.source = source;
    BORROW_LT_INIT_NONE_(b);
    return b;
}

/**
 * @brief Constructs a borrowed_ptr that captures the source's lifetime.
 *
 * In CANON_LIFETIME_DEBUG builds, source_lt->id is copied into the borrow
 * and verified by borrowed_ptr_get on every read. In default builds the
 * source_lt argument is ignored.
 *
 * @param ptr       Non-owning pointer (may be NULL).
 * @param source_lt Pointer to the source's embedded lifetime_t (may be NULL,
 *                  in which case the borrow is untracked).
 * @param source    Address of the owning object (debug tag; may be NULL).
 * @return          A borrowed_ptr wrapping ptr with captured lifetime.
 */
/*@ assigns \nothing;
    ensures ptr_stored:    \result.ptr    == ptr;
    ensures source_stored: \result.source == source;
*/
static inline borrowed_ptr borrowed_ptr_from_lifetime(
    const void *ptr,
#ifdef CANON_LIFETIME_DEBUG
    const lifetime_t *source_lt,
#else
    const void *source_lt,
#endif
    const void *source)
{
    borrowed_ptr b;
    b.ptr    = ptr;
    b.source = source;
    BORROW_LT_INIT_(b, source_lt);
    (void)source_lt; /* unused in default builds */
    return b;
}

/**
 * @brief Constructs a NULL / absent borrowed_ptr.
 * @return A borrowed_ptr with ptr == NULL and source == NULL.
 */
/*@ assigns \nothing;
    ensures null_ptr:    \result.ptr    == \null;
    ensures null_source: \result.source == \null;
*/
static inline borrowed_ptr borrowed_ptr_null(void)
{
    borrowed_ptr b;
    b.ptr    = NULL;
    b.source = NULL;
    BORROW_LT_INIT_NONE_(b);
    return b;
}

/**
 * @brief Returns the raw pointer stored in b.
 *
 * The caller is responsible for casting to the correct type.
 * Do NOT free the returned pointer.
 *
 * In CANON_LIFETIME_DEBUG builds, fires require_msg via lifetime_assert_valid
 * if the captured lifetime ID no longer matches the source's current ID,
 * or if the source has been closed.
 *
 * @pre  b != NULL  (checked with require_msg in debug builds).
 * @note Returns NULL when b == NULL in release builds so that callers
 *       that do not check remain crash-safe if require_msg is a no-op.
 * @param b Pointer to borrowed_ptr; must not be NULL.
 * @return  Stored const void * (may itself be NULL).
 */
/*@ requires readable_b: b != \null ==> \valid_read(b);
    assigns \nothing;
    behavior null_b:
      assumes b == \null;
      ensures null_result: \result == \null;
    behavior non_null:
      assumes b != \null;
      ensures stored_result: \result == b->ptr;
    complete behaviors;
    disjoint behaviors;
*/
static inline const void *borrowed_ptr_get(const borrowed_ptr *b)
{
    require_msg(b != NULL, "borrowed_ptr_get: b must not be NULL");
    if (b == NULL) { return NULL; }
    BORROW_LT_CHECK_(b, "borrowed_ptr_get");
    return b->ptr;
}

/**
 * @brief Returns non-zero (true) if b is non-NULL and b->ptr is non-NULL.
 * @param b Pointer to borrowed_ptr (may be NULL).
 * @return  1 if valid, 0 otherwise.
 */
/*@ requires readable_b: b != \null ==> \valid_read(b);
    assigns \nothing;
    behavior null_b:
      assumes b == \null;
      ensures is_false: \result == 0;
    behavior non_null:
      assumes b != \null;
      ensures validity_def: (\result != 0) <==> b->ptr != \null;
    complete behaviors;
    disjoint behaviors;
*/
static inline bool borrowed_ptr_is_valid(const borrowed_ptr *b)
{
    return (b != NULL) && (b->ptr != NULL);
}

/**
 * @brief Returns non-zero (true) if a and b point to the same address.
 *
 * Equality is pointer identity — source tags and lifetime fields are
 * intentionally ignored. For content equality over pointed-to data,
 * dereference and compare manually.
 *
 * @param a First borrowed_ptr (by value).
 * @param b Second borrowed_ptr (by value).
 * @return  1 if a.ptr == b.ptr, 0 otherwise.
 */
/*@ assigns \nothing;
    ensures identity_def: (\result != 0) <==> a.ptr == b.ptr;
*/
static inline bool borrowed_ptr_eq(borrowed_ptr a, borrowed_ptr b)
{
    return a.ptr == b.ptr;
}

/* ============================================================================
   borrowed_str — non-owning string view
   ============================================================================ */

/**
 * @brief Non-owning string view with explicit borrowing intent.
 *
 * Wraps str_t. The character data is NOT null-terminated in general.
 * Do NOT free str.ptr — it is owned by source.
 *
 * Under CANON_LIFETIME_DEBUG, additional fields (source_lt, captured_id)
 * carry the source's lifetime token at construction time.
 *
 * @warning source can itself become a dangling pointer if the owning object
 *          is destroyed while this struct is still in scope. See "Source tag
 *          lifetime warning" in the file-level documentation.
 */
typedef struct {
    str_t        str;    /**< Borrowed string view — do NOT free str.ptr. */
    const void  *source; /**< Owning object's address (debug tag only).
                              Never dereference outside owning object's lifetime. */
    BORROW_LT_FIELDS_
} borrowed_str;

/**
 * @brief Constructs a borrowed_str from a str_t and an owner tag.
 * @param s      The str_t to wrap.
 * @param source Address of the owning object (debug tag; may be NULL).
 * @return       A borrowed_str wrapping s.
 *
 * In CANON_LIFETIME_DEBUG builds the borrow is untracked. Use
 * borrowed_str_from_lifetime() to opt into tracking.
 */
/*@ assigns \nothing;
    ensures str_stored:    \result.str.ptr == s.ptr && \result.str.len == s.len;
    ensures source_stored: \result.source == source;
*/
static inline borrowed_str borrowed_str_from(str_t s, const void *source)
{
    borrowed_str b;
    b.str    = s;
    b.source = source;
    BORROW_LT_INIT_NONE_(b);
    return b;
}

/**
 * @brief Constructs a borrowed_str that captures the source's lifetime.
 *
 * In CANON_LIFETIME_DEBUG builds, source_lt->id is copied into the borrow
 * and verified on every read.
 *
 * @param s         The str_t to wrap.
 * @param source_lt Pointer to source's embedded lifetime_t (may be NULL).
 * @param source    Address of the owning object (debug tag; may be NULL).
 */
/*@ assigns \nothing;
    ensures str_stored:    \result.str.ptr == s.ptr && \result.str.len == s.len;
    ensures source_stored: \result.source == source;
*/
static inline borrowed_str borrowed_str_from_lifetime(
    str_t s,
#ifdef CANON_LIFETIME_DEBUG
    const lifetime_t *source_lt,
#else
    const void *source_lt,
#endif
    const void *source)
{
    borrowed_str b;
    b.str    = s;
    b.source = source;
    BORROW_LT_INIT_(b, source_lt);
    (void)source_lt;
    return b;
}

/**
 * @brief Constructs a borrowed_str from a null-terminated C string.
 * @param cstr   Null-terminated string (may be NULL, yielding empty view).
 * @param source Address of the owning object (debug tag; may be NULL).
 * @return       A borrowed_str wrapping cstr via str_from_cstr.
 *
 * String literals have static lifetime; this constructor produces an
 * untracked borrow appropriate for that case.
 */
/*@ assigns \nothing;
    ensures source_stored: \result.source == source;
    ensures null_cstr_empty:
      cstr == \null ==> \result.str.ptr == \null && \result.str.len == 0;
*/
static inline borrowed_str borrowed_str_from_cstr(const char *cstr,
                                                   const void *source)
{
    borrowed_str b;
    b.str    = str_from_cstr(cstr);
    b.source = source;
    BORROW_LT_INIT_NONE_(b);
    return b;
}

/**
 * @brief Constructs an empty (zero-length, NULL ptr) borrowed_str.
 * @return A borrowed_str with str == str_empty() and source == NULL.
 */
/*@ assigns \nothing;
    ensures empty_str:   \result.str.ptr == \null && \result.str.len == 0;
    ensures null_source: \result.source == \null;
*/
static inline borrowed_str borrowed_str_empty(void)
{
    borrowed_str b;
    b.str    = str_empty();
    b.source = NULL;
    BORROW_LT_INIT_NONE_(b);
    return b;
}

/**
 * @brief Returns the underlying str_t.
 *
 * Do NOT free the returned ptr field.
 *
 * In CANON_LIFETIME_DEBUG builds, fires require_msg if the captured
 * lifetime ID no longer matches the source's current ID.
 *
 * @pre  b != NULL  (checked with require_msg in debug builds).
 * @note Returns str_empty() when b == NULL in release builds.
 * @param b Pointer to borrowed_str; must not be NULL.
 * @return  The wrapped str_t.
 */
/*@ requires readable_b: b != \null ==> \valid_read(b);
    assigns \nothing;
    behavior null_b:
      assumes b == \null;
      ensures empty_result: \result.ptr == \null && \result.len == 0;
    behavior non_null:
      assumes b != \null;
      ensures stored_result:
        \result.ptr == b->str.ptr && \result.len == b->str.len;
    complete behaviors;
    disjoint behaviors;
*/
static inline str_t borrowed_str_get(const borrowed_str *b)
{
    require_msg(b != NULL, "borrowed_str_get: b must not be NULL");
    if (b == NULL) { return str_empty(); }
    BORROW_LT_CHECK_(b, "borrowed_str_get");
    return b->str;
}

/**
 * @brief Returns non-zero (true) if b is non-NULL and b->str.ptr is non-NULL.
 * @param b Pointer to borrowed_str (may be NULL).
 * @return  1 if valid, 0 otherwise.
 */
/*@ requires readable_b: b != \null ==> \valid_read(b);
    assigns \nothing;
    behavior null_b:
      assumes b == \null;
      ensures is_false: \result == 0;
    behavior non_null:
      assumes b != \null;
      ensures validity_def: (\result != 0) <==> b->str.ptr != \null;
    complete behaviors;
    disjoint behaviors;
*/
static inline bool borrowed_str_is_valid(const borrowed_str *b)
{
    return (b != NULL) && (b->str.ptr != NULL);
}

/**
 * @brief Returns the byte length of the borrowed string.
 * @param b Pointer to borrowed_str (may be NULL, returns 0).
 * @return  b->str.len, or 0 if b is NULL.
 */
/*@ requires readable_b: b != \null ==> \valid_read(b);
    assigns \nothing;
    behavior null_b:
      assumes b == \null;
      ensures zero_len: \result == 0;
    behavior non_null:
      assumes b != \null;
      ensures stored_len: \result == b->str.len;
    complete behaviors;
    disjoint behaviors;
*/
static inline usize borrowed_str_len(const borrowed_str *b)
{
    return (b != NULL) ? b->str.len : (usize)0;
}

/**
 * @brief Returns non-zero (true) if both strings have equal content.
 *
 * Equality is content-based via str_equal — source tags and lifetime
 * fields are intentionally ignored. This differs from borrowed_ptr_eq
 * which uses pointer identity; the difference reflects the natural
 * equality of the underlying types.
 *
 * @param a First borrowed_str (by value).
 * @param b Second borrowed_str (by value).
 * @return  1 if str_equal(a.str, b.str), 0 otherwise.
 */
/*@ requires ok_a: str_valid(a.str);
    requires ok_b: str_valid(b.str);
    requires init_ab:
      (a.str.len == b.str.len && a.str.ptr != b.str.ptr &&
       a.str.ptr != \null && b.str.ptr != \null && a.str.len > 0) ==>
        (\initialized((char*)a.str.ptr + (0 .. (integer)a.str.len - 1)) &&
         \initialized((char*)b.str.ptr + (0 .. (integer)b.str.len - 1)));
    assigns \nothing;
    ensures bool_result:  \result == \true || \result == \false;
    ensures len_mismatch: a.str.len != b.str.len ==> \result == \false;
    ensures same_view:
      (a.str.ptr == b.str.ptr && a.str.len == b.str.len) ==>
        \result == \true;
*/
/* Partial functional spec by delegation: str_equal's own contract is
   deliberately partial (see slice.h remark and VERIFY-007), so full
   content equality is not stated here — it cannot be derived from the
   callee. Contrast borrowed_bytes_eq below, whose body is self-contained
   and carries the full content-equality postcondition.                  */
static inline bool borrowed_str_eq(borrowed_str a, borrowed_str b)
{
    return str_equal(a.str, b.str);
}

/**
 * @brief Returns a sub-borrow covering bytes [start, end).
 *
 * The returned view inherits the source tag and lifetime fields of b.
 * If start >= end or the range is out of bounds, behaviour is defined by
 * str_slice (typically returns an empty view).
 *
 * @param b     The original borrowed_str (by value).
 * @param start First byte index (inclusive).
 * @param end   One-past-last byte index (exclusive).
 * @return      A new borrowed_str covering [start, end).
 */
/*@ requires ok_b: str_valid(b.str);
    assigns \nothing;
    ensures source_inherited: \result.source == b.source;
*/
static inline borrowed_str borrowed_str_slice(borrowed_str b,
                                               usize start, usize end)
{
    borrowed_str r;
    r.str    = str_slice(b.str, start, end);
    r.source = b.source;
    BORROW_LT_INHERIT_(r, b);
    return r;
}

/* ============================================================================
   borrowed_bytes — non-owning byte view
   ============================================================================ */

/**
 * @brief Non-owning read-only byte region with explicit borrowing intent.
 *
 * Wraps cbytes_t. Do NOT free bytes.ptr — it is owned by source.
 *
 * Under CANON_LIFETIME_DEBUG, additional fields (source_lt, captured_id)
 * carry the source's lifetime token at construction time.
 *
 * @warning source can itself become a dangling pointer if the owning object
 *          is destroyed while this struct is still in scope. See "Source tag
 *          lifetime warning" in the file-level documentation.
 */
typedef struct {
    cbytes_t     bytes;  /**< Borrowed byte view — do NOT free bytes.ptr. */
    const void  *source; /**< Owning object's address (debug tag only).
                              Never dereference outside owning object's lifetime. */
    BORROW_LT_FIELDS_
} borrowed_bytes;

/**
 * @brief Constructs a borrowed_bytes from a raw pointer, length, and owner.
 *
 * ptr is cast to const u8 * internally via cbytes_from. The caller must
 * ensure ptr points to at least len readable bytes.
 *
 * In CANON_LIFETIME_DEBUG builds the borrow is untracked. Use
 * borrowed_bytes_from_lifetime() to opt into tracking.
 *
 * @param ptr    Start of the byte region (may be NULL if len == 0).
 * @param len    Number of bytes in the region.
 * @param source Address of the owning object (debug tag; may be NULL).
 * @return       A borrowed_bytes wrapping the region.
 */
/*@ requires shape: ptr != \null || len == 0;
    requires readable_region:
      len > 0 ==> \valid_read((const u8 *)ptr + (0 .. len - 1));
    assigns \nothing;
    ensures source_stored: \result.source == source;
    ensures shape_ok: cbytes_invariant(\result.bytes);
    ensures stored:
      \result.bytes.ptr == (const u8 *)ptr && \result.bytes.len == len;
*/
static inline borrowed_bytes borrowed_bytes_from(const void *ptr, usize len,
                                                  const void *source)
{
    borrowed_bytes b;
    b.bytes  = cbytes_from(ptr, len);
    b.source = source;
    BORROW_LT_INIT_NONE_(b);
    return b;
}

/**
 * @brief Constructs a borrowed_bytes that captures the source's lifetime.
 *
 * In CANON_LIFETIME_DEBUG builds, source_lt->id is copied into the borrow
 * and verified by borrowed_bytes_get on every read.
 *
 * @param ptr       Start of the byte region (may be NULL if len == 0).
 * @param len       Number of bytes in the region.
 * @param source_lt Pointer to source's embedded lifetime_t (may be NULL).
 * @param source    Address of the owning object (debug tag; may be NULL).
 */
/*@ requires shape: ptr != \null || len == 0;
    requires readable_region:
      len > 0 ==> \valid_read((const u8 *)ptr + (0 .. len - 1));
    assigns \nothing;
    ensures source_stored: \result.source == source;
    ensures shape_ok: cbytes_invariant(\result.bytes);
    ensures stored:
      \result.bytes.ptr == (const u8 *)ptr && \result.bytes.len == len;
*/
static inline borrowed_bytes borrowed_bytes_from_lifetime(
    const void *ptr, usize len,
#ifdef CANON_LIFETIME_DEBUG
    const lifetime_t *source_lt,
#else
    const void *source_lt,
#endif
    const void *source)
{
    borrowed_bytes b;
    b.bytes  = cbytes_from(ptr, len);
    b.source = source;
    BORROW_LT_INIT_(b, source_lt);
    (void)source_lt;
    return b;
}

/**
 * @brief Constructs a borrowed_bytes from an existing cbytes_t and an owner.
 * @param cb     The cbytes_t to wrap.
 * @param source Address of the owning object (debug tag; may be NULL).
 * @return       A borrowed_bytes wrapping cb.
 */
/*@ requires ok_cb: cbytes_invariant(cb);
    assigns \nothing;
    ensures bytes_stored:
      \result.bytes.ptr == cb.ptr && \result.bytes.len == cb.len;
    ensures source_stored: \result.source == source;
    ensures shape_ok: cbytes_invariant(\result.bytes);
*/
static inline borrowed_bytes borrowed_bytes_from_cbytes(cbytes_t cb,
                                                         const void *source)
{
    borrowed_bytes b;
    b.bytes  = cb;
    b.source = source;
    BORROW_LT_INIT_NONE_(b);
    return b;
}

/**
 * @brief Constructs an empty (zero-length, NULL ptr) borrowed_bytes.
 * @return A borrowed_bytes with bytes == cbytes_empty() and source == NULL.
 */
/*@ assigns \nothing;
    ensures empty_bytes: \result.bytes.ptr == \null && \result.bytes.len == 0;
    ensures null_source: \result.source == \null;
    ensures shape_ok:    cbytes_invariant(\result.bytes);
*/
static inline borrowed_bytes borrowed_bytes_empty(void)
{
    borrowed_bytes b;
    b.bytes  = cbytes_empty();
    b.source = NULL;
    BORROW_LT_INIT_NONE_(b);
    return b;
}

/**
 * @brief Returns the underlying cbytes_t.
 *
 * Do NOT free the returned ptr field.
 *
 * In CANON_LIFETIME_DEBUG builds, fires require_msg if the captured
 * lifetime ID no longer matches the source's current ID.
 *
 * @pre  b != NULL  (checked with require_msg in debug builds).
 * @note Returns cbytes_empty() when b == NULL in release builds.
 * @param b Pointer to borrowed_bytes; must not be NULL.
 * @return  The wrapped cbytes_t.
 */
/*@ requires readable_b: b != \null ==> \valid_read(b);
    assigns \nothing;
    behavior null_b:
      assumes b == \null;
      ensures empty_result: \result.ptr == \null && \result.len == 0;
    behavior non_null:
      assumes b != \null;
      ensures stored_result:
        \result.ptr == b->bytes.ptr && \result.len == b->bytes.len;
    complete behaviors;
    disjoint behaviors;
*/
static inline cbytes_t borrowed_bytes_get(const borrowed_bytes *b)
{
    require_msg(b != NULL, "borrowed_bytes_get: b must not be NULL");
    if (b == NULL) { return cbytes_empty(); }
    BORROW_LT_CHECK_(b, "borrowed_bytes_get");
    return b->bytes;
}

/**
 * @brief Returns the number of bytes in the view.
 * @param b Pointer to borrowed_bytes (may be NULL, returns 0).
 * @return  b->bytes.len, or 0 if b is NULL.
 */
/*@ requires readable_b: b != \null ==> \valid_read(b);
    assigns \nothing;
    behavior null_b:
      assumes b == \null;
      ensures zero_len: \result == 0;
    behavior non_null:
      assumes b != \null;
      ensures stored_len: \result == b->bytes.len;
    complete behaviors;
    disjoint behaviors;
*/
static inline usize borrowed_bytes_len(const borrowed_bytes *b)
{
    return (b != NULL) ? b->bytes.len : (usize)0;
}

/**
 * @brief Returns non-zero (true) if b is non-NULL and b->bytes.ptr is non-NULL.
 * @param b Pointer to borrowed_bytes (may be NULL).
 * @return  1 if valid, 0 otherwise.
 */
/*@ requires readable_b: b != \null ==> \valid_read(b);
    assigns \nothing;
    behavior null_b:
      assumes b == \null;
      ensures is_false: \result == 0;
    behavior non_null:
      assumes b != \null;
      ensures validity_def: (\result != 0) <==> b->bytes.ptr != \null;
    complete behaviors;
    disjoint behaviors;
*/
static inline bool borrowed_bytes_is_valid(const borrowed_bytes *b)
{
    return (b != NULL) && (b->bytes.ptr != NULL);
}

/**
 * @brief Returns non-zero (true) if both byte regions have equal content.
 *
 * Equality is content-based via memcmp — lengths must match and every byte
 * must be identical. Source tags and lifetime fields are intentionally
 * ignored, consistent with all other _eq functions in this header.
 *
 * Two empty regions (len == 0) are always equal regardless of ptr value,
 * consistent with memcmp semantics (zero-length comparison is vacuously true).
 *
 * Two regions that share the same ptr and len are equal without calling
 * memcmp (short-circuit for performance).
 *
 * @param a First borrowed_bytes (by value).
 * @param b Second borrowed_bytes (by value).
 * @return  1 if regions have equal length and identical byte content,
 *          0 otherwise.
 */
/*@ requires ok_a: cbytes_invariant(a.bytes);
    requires ok_b: cbytes_invariant(b.bytes);
    requires valid_a:
      a.bytes.len > 0 && a.bytes.ptr != b.bytes.ptr ==>
        \valid_read(a.bytes.ptr + (0 .. a.bytes.len - 1));
    requires valid_b:
      b.bytes.len > 0 && a.bytes.ptr != b.bytes.ptr ==>
        \valid_read(b.bytes.ptr + (0 .. b.bytes.len - 1));
    requires init_a:
      a.bytes.len > 0 && a.bytes.ptr != b.bytes.ptr ==>
        \initialized(a.bytes.ptr + (0 .. a.bytes.len - 1));
    requires init_b:
      b.bytes.len > 0 && a.bytes.ptr != b.bytes.ptr ==>
        \initialized(b.bytes.ptr + (0 .. b.bytes.len - 1));
    assigns \nothing;
    ensures content_eq_def:
      (\result != 0) <==>
        (a.bytes.len == b.bytes.len &&
         \forall integer i; 0 <= i < a.bytes.len ==>
           a.bytes.ptr[i] == b.bytes.ptr[i]);
*/
static inline bool borrowed_bytes_eq(borrowed_bytes a, borrowed_bytes b)
{
    if (a.bytes.len != b.bytes.len) {
        return false;
    }
    if (a.bytes.len == 0u) {
        return true; /* vacuously equal — no bytes to compare */
    }
    if (a.bytes.ptr == b.bytes.ptr) {
        return true; /* same pointer, same length — identical by definition */
    }
    if ((a.bytes.ptr == NULL) || (b.bytes.ptr == NULL)) {
        /* One NULL, one non-NULL, same length > 0 — impossible by
           construction (cbytes_invariant forbids {NULL, len>0}) but
           guarded for safety. The assert below asks WP to prove this
           path infeasible under the invariant preconditions: on this
           path len_a == len_b > 0 and one pointer is NULL, so ok_a/ok_b
           force a contradiction. This named goal is the cross-stream
           half of MCDC-008 — the two condition outcomes gcov reports
           uncovered here are dead code under the type invariant.       */
        /*@ assert dead_by_invariant: \false; */
        return false;
    }
    return memcmp(a.bytes.ptr, b.bytes.ptr, a.bytes.len) == 0;
}

/**
 * @brief Returns a sub-borrow covering bytes [start, end).
 *
 * The returned view inherits the source tag and lifetime fields of b.
 * Returns borrowed_bytes_empty() when:
 *   - b.bytes.ptr is NULL, or
 *   - start >= b.bytes.len, or
 *   - start >= end (after clamping end to len).
 *
 * end is silently clamped to b.bytes.len if it exceeds it.
 *
 * Pointer arithmetic is performed on const u8 * (not const void *)
 * to remain strictly conformant to C99.
 *
 * @param b     The original borrowed_bytes (by value).
 * @param start First byte index (inclusive).
 * @param end   One-past-last byte index (exclusive).
 * @return      A new borrowed_bytes covering [start, end).
 */
/*@ requires readable_region:
      b.bytes.ptr != \null && b.bytes.len > 0 ==>
        \valid_read(b.bytes.ptr + (0 .. b.bytes.len - 1));
    assigns \nothing;
    behavior returns_empty:
      assumes b.bytes.ptr == \null || start >= b.bytes.len || end <= start;
      ensures empty_ptr:    \result.bytes.ptr == \null;
      ensures empty_len:    \result.bytes.len == 0;
      ensures empty_source: \result.source == \null;
    behavior returns_sub:
      assumes b.bytes.ptr != \null && start < b.bytes.len && start < end;
      ensures sub_ptr:    \result.bytes.ptr == b.bytes.ptr + start;
      ensures sub_len:
        \result.bytes.len == \min(end, b.bytes.len) - start;
      ensures sub_source: \result.source == b.source;
    complete behaviors;
    disjoint behaviors;
*/
static inline borrowed_bytes borrowed_bytes_slice(borrowed_bytes b,
                                                   usize start, usize end)
{
    const u8 *base = (const u8 *)b.bytes.ptr; /* C99: cast void* -> u8* OK */
    usize     len  = b.bytes.len;
    borrowed_bytes r;

    if ((base == NULL) || (start >= len)) {
        return borrowed_bytes_empty();
    }
    if (end > len) {
        end = len;
    }
    if (start >= end) {
        return borrowed_bytes_empty();
    }

    r.bytes.ptr = base + start;   /* arithmetic on u8 *, not void * */
    r.bytes.len = end - start;
    r.source    = b.source;
    BORROW_LT_INHERIT_(r, b);
    return r;
}

/* ============================================================================
   DEFINE_BORROWED_SLICE — typed borrowed slice instantiation macro
   ============================================================================
 *
 * Generates a typed borrowed_slice_<type> wrapper for element type `type`.
 *
 * Requires a prior DEFINE_SLICE(type) in the same translation unit or
 * an included header.
 *
 * Generated type:
 *   borrowed_slice_<type>
 *
 * Generated functions (all static inline):
 *   borrowed_slice_<type>_from           — construct from slice_<type> + source
 *   borrowed_slice_<type>_from_lifetime  — construct with captured lifetime
 *   borrowed_slice_<type>_empty          — construct empty view
 *   borrowed_slice_<type>_get            — return inner slice_<type>
 *   borrowed_slice_<type>_len            — element count
 *   borrowed_slice_<type>_is_valid       — non-null check
 *   borrowed_slice_<type>_at             — bounds-checked const element pointer
 *   borrowed_slice_<type>_slice          — sub-borrow [start, end)
 *   borrowed_slice_<type>_as_bytes       — raw borrowed_bytes view over slice
 *                                          (inherits lifetime tracking from
 *                                           the source slice; returns
 *                                           borrowed_bytes_empty() on overflow
 *                                           of len * sizeof(type))
 *
 * Example:
 *   DEFINE_SLICE(int)
 *   DEFINE_BORROWED_SLICE(int)
 *
 *   int arr[4] = {1, 2, 3, 4};
 *   borrowed_slice_int view =
 *       borrowed_slice_int_from(slice_int_from(arr, 4u), arr);
 *   const int *third = borrowed_slice_int_at(&view, 2u);
 */
/* cppcheck-suppress misra-c2012-20.7 ; MISRA-DEV-012 */
#define DEFINE_BORROWED_SLICE(type)                                            \
                                                                               \
typedef struct {                                                               \
    slice_##type  slice;                                                       \
    const void   *source;                                                      \
    BORROW_LT_FIELDS_                                                          \
} borrowed_slice_##type;                                                       \
                                                                               \
static inline borrowed_slice_##type                                            \
borrowed_slice_##type##_from(slice_##type s, const void *source)               \
{                                                                              \
    borrowed_slice_##type b;                                                   \
    b.slice  = s;                                                              \
    b.source = source;                                                         \
    BORROW_LT_INIT_NONE_(b);                                                   \
    return b;                                                                  \
}                                                                              \
                                                                               \
static inline borrowed_slice_##type                                            \
borrowed_slice_##type##_from_lifetime(                                         \
    slice_##type s,                                                            \
    const void *source_lt_arg,                                                 \
    const void *source)                                                        \
{                                                                              \
    borrowed_slice_##type b;                                                   \
    b.slice  = s;                                                              \
    b.source = source;                                                         \
    /* In CANON_LIFETIME_DEBUG builds, source_lt_arg is treated as a           \
       const lifetime_t* and captured. In default builds it is ignored. */     \
    BORROW_LT_INIT_(b, (const lifetime_t *)source_lt_arg);                     \
    (void)source_lt_arg;                                                       \
    return b;                                                                  \
}                                                                              \
                                                                               \
static inline borrowed_slice_##type                                            \
borrowed_slice_##type##_empty(void)                                            \
{                                                                              \
    borrowed_slice_##type b;                                                   \
    b.slice  = slice_##type##_empty();                                         \
    b.source = NULL;                                                           \
    BORROW_LT_INIT_NONE_(b);                                                   \
    return b;                                                                  \
}                                                                              \
                                                                               \
static inline slice_##type                                                     \
borrowed_slice_##type##_get(const borrowed_slice_##type *b)                    \
{                                                                              \
    require_msg(b != NULL,                                                     \
                "borrowed_slice_" #type "_get: b must not be NULL");           \
    if (b == NULL) { return slice_##type##_empty(); }                          \
    BORROW_LT_CHECK_(b, "borrowed_slice_" #type "_get");                       \
    return b->slice;                                                           \
}                                                                              \
                                                                               \
static inline usize                                                            \
borrowed_slice_##type##_len(const borrowed_slice_##type *b)                    \
{                                                                              \
    return (b != NULL) ? b->slice.len : (usize)0;                              \
}                                                                              \
                                                                               \
static inline bool                                                             \
borrowed_slice_##type##_is_valid(const borrowed_slice_##type *b)               \
{                                                                              \
    return (b != NULL) && (b->slice.ptr != NULL);                              \
}                                                                              \
                                                                               \
static inline const type *                                                     \
borrowed_slice_##type##_at(const borrowed_slice_##type *b, usize i)            \
{                                                                              \
    if ((b == NULL) || (b->slice.ptr == NULL) || (i >= b->slice.len)) {        \
        return NULL;                                                           \
    }                                                                          \
    return &b->slice.ptr[i];                                                   \
}                                                                              \
                                                                               \
static inline borrowed_slice_##type                                            \
borrowed_slice_##type##_slice(borrowed_slice_##type b,                         \
                               usize start, usize end)                         \
{                                                                              \
    borrowed_slice_##type r;                                                   \
    r.slice  = slice_##type##_slice(b.slice, start, end);                      \
    r.source = b.source;                                                       \
    BORROW_LT_INHERIT_(r, b);                                                  \
    return r;                                                                  \
}                                                                              \
                                                                               \
static inline borrowed_bytes                                                   \
borrowed_slice_##type##_as_bytes(const borrowed_slice_##type *b)               \
{                                                                              \
    usize byte_len;                                                            \
    borrowed_bytes r;                                                          \
    if ((b == NULL) || (b->slice.ptr == NULL)) {                               \
        return borrowed_bytes_empty();                                         \
    }                                                                          \
    if (!checked_mul(b->slice.len, sizeof(type), &byte_len)) {                 \
        return borrowed_bytes_empty();                                         \
    }                                                                          \
    r.bytes.ptr = (const u8 *)b->slice.ptr;                                    \
    r.bytes.len = byte_len;                                                    \
    r.source    = b->source;                                                   \
    /* Lifetime stays with the underlying memory across type erasure —         \
       a byte view derived from a tracked slice must remain tracked,           \
       otherwise the substrate's safety check is silently stripped. */         \
    BORROW_LT_INHERIT_(r, *b);                                                 \
    return r;                                                                  \
}

#endif /* CANON_SEMANTICS_BORROW_H */
