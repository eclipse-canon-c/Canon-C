#ifndef CANON_SEMANTICS_BORROW_H
#define CANON_SEMANTICS_BORROW_H

#include "core/primitives/types.h"      /* usize, bool, u8 */
#include "core/primitives/contract.h"   /* require_msg     */
#include "core/slice.h"                 /* str_t, cbytes_t, slice_##type */
#include "core/ownership.h"             /* borrowed(T) annotation style  */

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
 * - No runtime enforcement — documentation + discipline.
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
 * The source field is a debug tag. Pass &owning_struct as source.
 * It is never dereferenced by this header — for human / debugger inspection
 * only.
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
 * Dependency rule:
 * --------------------------------------------------------------------------
 * borrow.h lives in semantics/ and may include core/ only.
 * No data/, util/, or other semantics/ headers.
 *
 * Portability:
 * --------------------------------------------------------------------------
 * - Requires C99 or later (no GCC/Clang extensions used).
 * - No allocations, no platform-specific code.
 *
 * Performance:
 * --------------------------------------------------------------------------
 * - All types are approximately sizeof(ptr) + sizeof(usize) + sizeof(void *).
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
 * @sa core/slice.h     — underlying str_t, cbytes_t, slice_##type
 * @sa core/ownership.h — borrowed(T) annotation macros
 */

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
 */
typedef struct {
    const void *ptr;    /**< Borrowed pointer — do NOT free. */
    const void *source; /**< Owning object's address (debug tag only). */
} borrowed_ptr;

/**
 * @brief Constructs a borrowed_ptr from a raw pointer and an owner tag.
 * @param ptr    Non-owning pointer (may be NULL).
 * @param source Address of the owning object (debug tag; may be NULL).
 * @return       A borrowed_ptr wrapping ptr.
 */
static inline borrowed_ptr borrowed_ptr_from(const void *ptr,
                                              const void *source)
{
    borrowed_ptr b;
    b.ptr    = ptr;
    b.source = source;
    return b;
}

/**
 * @brief Constructs a NULL / absent borrowed_ptr.
 * @return A borrowed_ptr with ptr == NULL and source == NULL.
 */
static inline borrowed_ptr borrowed_ptr_null(void)
{
    borrowed_ptr b;
    b.ptr    = NULL;
    b.source = NULL;
    return b;
}

/**
 * @brief Returns the raw pointer stored in b.
 *
 * The caller is responsible for casting to the correct type.
 * Do NOT free the returned pointer.
 *
 * @pre  b != NULL  (checked with require_msg in debug builds).
 * @note Returns NULL when b == NULL in release builds so that callers
 *       that do not check remain crash-safe if require_msg is a no-op.
 * @param b Pointer to borrowed_ptr; must not be NULL.
 * @return  Stored const void * (may itself be NULL).
 */
static inline const void *borrowed_ptr_get(const borrowed_ptr *b)
{
    require_msg(b != NULL, "borrowed_ptr_get: b must not be NULL");
    if (b == NULL) { return NULL; }
    return b->ptr;
}

/**
 * @brief Returns non-zero (true) if b is non-NULL and b->ptr is non-NULL.
 * @param b Pointer to borrowed_ptr (may be NULL).
 * @return  1 if valid, 0 otherwise.
 */
static inline bool borrowed_ptr_is_valid(const borrowed_ptr *b)
{
    return (b != NULL) && (b->ptr != NULL);
}

/**
 * @brief Returns non-zero (true) if a and b point to the same address.
 *
 * Source tags are intentionally ignored — equality is based on the
 * pointed-to address alone.
 *
 * @param a First borrowed_ptr (by value).
 * @param b Second borrowed_ptr (by value).
 * @return  1 if a.ptr == b.ptr, 0 otherwise.
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
 */
typedef struct {
    str_t        str;    /**< Borrowed string view — do NOT free str.ptr. */
    const void  *source; /**< Owning object's address (debug tag only). */
} borrowed_str;

/**
 * @brief Constructs a borrowed_str from a str_t and an owner tag.
 * @param s      The str_t to wrap.
 * @param source Address of the owning object (debug tag; may be NULL).
 * @return       A borrowed_str wrapping s.
 */
static inline borrowed_str borrowed_str_from(str_t s, const void *source)
{
    borrowed_str b;
    b.str    = s;
    b.source = source;
    return b;
}

/**
 * @brief Constructs a borrowed_str from a null-terminated C string.
 * @param cstr   Null-terminated string (may be NULL, yielding empty view).
 * @param source Address of the owning object (debug tag; may be NULL).
 * @return       A borrowed_str wrapping cstr via str_from_cstr.
 */
static inline borrowed_str borrowed_str_from_cstr(const char *cstr,
                                                   const void *source)
{
    borrowed_str b;
    b.str    = str_from_cstr(cstr);
    b.source = source;
    return b;
}

/**
 * @brief Constructs an empty (zero-length, NULL ptr) borrowed_str.
 * @return A borrowed_str with str == str_empty() and source == NULL.
 */
static inline borrowed_str borrowed_str_empty(void)
{
    borrowed_str b;
    b.str    = str_empty();
    b.source = NULL;
    return b;
}

/**
 * @brief Returns the underlying str_t.
 *
 * Do NOT free the returned ptr field.
 *
 * @pre  b != NULL  (checked with require_msg in debug builds).
 * @note Returns str_empty() when b == NULL in release builds.
 * @param b Pointer to borrowed_str; must not be NULL.
 * @return  The wrapped str_t.
 */
static inline str_t borrowed_str_get(const borrowed_str *b)
{
    require_msg(b != NULL, "borrowed_str_get: b must not be NULL");
    if (b == NULL) { return str_empty(); }
    return b->str;
}

/**
 * @brief Returns non-zero (true) if b is non-NULL and b->str.ptr is non-NULL.
 * @param b Pointer to borrowed_str (may be NULL).
 * @return  1 if valid, 0 otherwise.
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
static inline usize borrowed_str_len(const borrowed_str *b)
{
    return (b != NULL) ? b->str.len : (usize)0;
}

/**
 * @brief Returns non-zero (true) if both strings have equal content.
 *
 * Source tags are intentionally ignored — equality is content-only.
 *
 * @param a First borrowed_str (by value).
 * @param b Second borrowed_str (by value).
 * @return  1 if str_equal(a.str, b.str), 0 otherwise.
 */
static inline bool borrowed_str_eq(borrowed_str a, borrowed_str b)
{
    return str_equal(a.str, b.str);
}

/**
 * @brief Returns a sub-borrow covering bytes [start, end).
 *
 * The returned view inherits the source tag of b.
 * If start >= end or the range is out of bounds, behaviour is defined by
 * str_slice (typically returns an empty view).
 *
 * @param b     The original borrowed_str (by value).
 * @param start First byte index (inclusive).
 * @param end   One-past-last byte index (exclusive).
 * @return      A new borrowed_str covering [start, end).
 */
static inline borrowed_str borrowed_str_slice(borrowed_str b,
                                               usize start, usize end)
{
    borrowed_str r;
    r.str    = str_slice(b.str, start, end);
    r.source = b.source;
    return r;
}

/* ============================================================================
   borrowed_bytes — non-owning byte view
   ============================================================================ */

/**
 * @brief Non-owning read-only byte region with explicit borrowing intent.
 *
 * Wraps cbytes_t. Do NOT free bytes.ptr — it is owned by source.
 */
typedef struct {
    cbytes_t     bytes;  /**< Borrowed byte view — do NOT free bytes.ptr. */
    const void  *source; /**< Owning object's address (debug tag only). */
} borrowed_bytes;

/**
 * @brief Constructs a borrowed_bytes from a raw pointer, length, and owner.
 *
 * ptr is cast to const u8 * internally via cbytes_from. The caller must
 * ensure ptr points to at least len readable bytes.
 *
 * @param ptr    Start of the byte region (may be NULL if len == 0).
 * @param len    Number of bytes in the region.
 * @param source Address of the owning object (debug tag; may be NULL).
 * @return       A borrowed_bytes wrapping the region.
 */
static inline borrowed_bytes borrowed_bytes_from(const void *ptr, usize len,
                                                  const void *source)
{
    borrowed_bytes b;
    b.bytes  = cbytes_from(ptr, len);
    b.source = source;
    return b;
}

/**
 * @brief Constructs a borrowed_bytes from an existing cbytes_t and an owner.
 * @param cb     The cbytes_t to wrap.
 * @param source Address of the owning object (debug tag; may be NULL).
 * @return       A borrowed_bytes wrapping cb.
 */
static inline borrowed_bytes borrowed_bytes_from_cbytes(cbytes_t cb,
                                                         const void *source)
{
    borrowed_bytes b;
    b.bytes  = cb;
    b.source = source;
    return b;
}

/**
 * @brief Constructs an empty (zero-length, NULL ptr) borrowed_bytes.
 * @return A borrowed_bytes with bytes.ptr == NULL, bytes.len == 0,
 *         and source == NULL.
 */
static inline borrowed_bytes borrowed_bytes_empty(void)
{
    borrowed_bytes b;
    b.bytes.ptr = NULL;
    b.bytes.len = (usize)0;
    b.source    = NULL;
    return b;
}

/**
 * @brief Returns the underlying cbytes_t.
 *
 * Do NOT free the returned ptr field.
 *
 * @pre  b != NULL  (checked with require_msg in debug builds).
 * @note Returns an empty cbytes_t when b == NULL in release builds.
 * @param b Pointer to borrowed_bytes; must not be NULL.
 * @return  The wrapped cbytes_t.
 */
static inline cbytes_t borrowed_bytes_get(const borrowed_bytes *b)
{
    require_msg(b != NULL, "borrowed_bytes_get: b must not be NULL");
    if (b != NULL) { return b->bytes; }
    {
        cbytes_t empty;
        empty.ptr = NULL;
        empty.len = (usize)0;
        return empty;
    }
}

/**
 * @brief Returns the number of bytes in the view.
 * @param b Pointer to borrowed_bytes (may be NULL, returns 0).
 * @return  b->bytes.len, or 0 if b is NULL.
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
static inline bool borrowed_bytes_is_valid(const borrowed_bytes *b)
{
    return (b != NULL) && (b->bytes.ptr != NULL);
}

/**
 * @brief Returns a sub-borrow covering bytes [start, end).
 *
 * The returned view inherits the source tag of b.
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
 *   borrowed_slice_<type>_from      — construct from slice_<type> + source
 *   borrowed_slice_<type>_empty     — construct empty view
 *   borrowed_slice_<type>_get       — return inner slice_<type>
 *   borrowed_slice_<type>_len       — element count
 *   borrowed_slice_<type>_is_valid  — non-null check
 *   borrowed_slice_<type>_at        — bounds-checked const element pointer
 *   borrowed_slice_<type>_slice     — sub-borrow [start, end)
 *   borrowed_slice_<type>_as_bytes  — raw borrowed_bytes view over slice
 *
 * No block comments are used inside the macro body to ensure compatibility
 * with all C99-conforming preprocessors.
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
#define DEFINE_BORROWED_SLICE(type)                                            \
                                                                               \
typedef struct {                                                               \
    slice_##type  slice;                                                       \
    const void   *source;                                                      \
} borrowed_slice_##type;                                                       \
                                                                               \
static inline borrowed_slice_##type                                            \
borrowed_slice_##type##_from(slice_##type s, const void *source)               \
{                                                                              \
    borrowed_slice_##type b;                                                   \
    b.slice  = s;                                                              \
    b.source = source;                                                         \
    return b;                                                                  \
}                                                                              \
                                                                               \
static inline borrowed_slice_##type                                            \
borrowed_slice_##type##_empty(void)                                            \
{                                                                              \
    borrowed_slice_##type b;                                                   \
    b.slice  = slice_##type##_empty();                                         \
    b.source = NULL;                                                           \
    return b;                                                                  \
}                                                                              \
                                                                               \
static inline slice_##type                                                     \
borrowed_slice_##type##_get(const borrowed_slice_##type *b)                    \
{                                                                              \
    require_msg(b != NULL,                                                     \
                "borrowed_slice_" #type "_get: b must not be NULL");           \
    if (b == NULL) { return slice_##type##_empty(); }                          \
    return b->slice;                                                           \
}                                                                              \
                                                                               \
static inline usize                                                            \
borrowed_slice_##type##_len(const borrowed_slice_##type *b)                    \
{                                                                              \
    return (b != NULL) ? b->slice.len : (usize)0;                             \
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
    if ((b == NULL) || (b->slice.ptr == NULL) || (i >= b->slice.len)) {       \
        return NULL;                                                           \
    }                                                                          \
    return &b->slice.ptr[i];                                                  \
}                                                                              \
                                                                               \
static inline borrowed_slice_##type                                            \
borrowed_slice_##type##_slice(borrowed_slice_##type b,                         \
                               usize start, usize end)                         \
{                                                                              \
    borrowed_slice_##type r;                                                   \
    r.slice  = slice_##type##_slice(b.slice, start, end);                     \
    r.source = b.source;                                                       \
    return r;                                                                  \
}                                                                              \
                                                                               \
static inline borrowed_bytes                                                   \
borrowed_slice_##type##_as_bytes(const borrowed_slice_##type *b)               \
{                                                                              \
    borrowed_bytes r;                                                          \
    if ((b == NULL) || (b->slice.ptr == NULL)) {                              \
        return borrowed_bytes_empty();                                         \
    }                                                                          \
    r.bytes.ptr = (const u8 *)b->slice.ptr;                                   \
    r.bytes.len = b->slice.len * sizeof(type);                                 \
    r.source    = b.source;                                                    \
    return r;                                                                  \
}

#endif /* CANON_SEMANTICS_BORROW_H */
