/**
 * @file hashmap_fmt.h
 * @brief Optional formatting extension: serialize a hashmap to a StringBuf
 *
 * Provides hashmap_to_stringbuf() — a diagnostic helper that writes a
 * human-readable representation of the hashmap into a caller-provided
 * StringBuf. Intended for debugging and logging, not serialization.
 *
 * This is an OPTIONAL extension. It is NOT included by hashmap.h by default.
 * Include it explicitly only when you need formatted output.
 *
 * Dependency:
 * ────────────────────────────────────────────────────────────────────────────
 * Requires data/stringbuf.h. hashmap_fmt.h sits at the data/ layer boundary
 * and may include other data/ headers. It must not include algo/ or util/.
 *
 * Required user definitions:
 * ────────────────────────────────────────────────────────────────────────────
 * Before including this file, define:
 *
 *   HASHMAP_FMT_KEY_FN  — formats a key into a StringBuf
 *     Signature: bool fn(StringBuf* sb, const HASHMAP_KEY_TYPE* key)
 *     Returns true on success, false if the buffer is full.
 *
 *   HASHMAP_FMT_VAL_FN  — formats a value into a StringBuf
 *     Signature: bool fn(StringBuf* sb, const HASHMAP_VAL_TYPE* val)
 *     Returns true on success, false if the buffer is full.
 *
 * Both functions must be defined BEFORE including this header.
 *
 * Usage example:
 * ────────────────────────────────────────────────────────────────────────────
 * ```c
 * #include "data/stringbuf.h"
 *
 * static bool fmt_u64_key(StringBuf* sb, const u64* k) {
 *     char tmp[24];
 *     snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)*k);
 *     return stringbuf_append(sb, tmp);
 * }
 *
 * static bool fmt_int_val(StringBuf* sb, const int* v) {
 *     char tmp[16];
 *     snprintf(tmp, sizeof(tmp), "%d", *v);
 *     return stringbuf_append(sb, tmp);
 * }
 *
 * #define HASHMAP_KEY_TYPE  u64
 * #define HASHMAP_VAL_TYPE  int
 * #define HASHMAP_HASH_FN   hash_u64
 * #define HASHMAP_EQ_FN     eq_u64
 * #define HASHMAP_FMT_KEY_FN fmt_u64_key
 * #define HASHMAP_FMT_VAL_FN fmt_int_val
 * #include "data/hashmap/hashmap.h"
 * #include "data/hashmap/hashmap_fmt.h"
 *
 * // Later:
 * u8 strbuf_mem[4096];
 * StringBuf sb = stringbuf_from(bytes_from(strbuf_mem, sizeof(strbuf_mem)));
 * bool ok = hashmap_to_stringbuf(&sb, &my_map);
 * if (ok) printf("%.*s\n", (int)sb.len, sb.ptr);
 * ```
 *
 * Output format:
 * ────────────────────────────────────────────────────────────────────────────
 * hashmap { len=3, cap=8, load=0.375 } {
 *   [key0] => value0
 *   [key1] => value1
 *   [key2] => value2
 * }
 *
 * @sa hashmap.h        — main entry point, must be included before this
 * @sa data/stringbuf.h — required dependency
 */

#ifndef CANON_DATA_HASHMAP_FMT_H
#define CANON_DATA_HASHMAP_FMT_H

#include "hashmap_mangle.h"
#include "data/stringbuf.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"

/* ============================================================================
 * Required user-defined formatter functions
 * ========================================================================= */
#ifndef HASHMAP_FMT_KEY_FN
    #error "hashmap_fmt.h: define HASHMAP_FMT_KEY_FN (bool fn(StringBuf*, const K*)) before including"
#endif
#ifndef HASHMAP_FMT_VAL_FN
    #error "hashmap_fmt.h: define HASHMAP_FMT_VAL_FN (bool fn(StringBuf*, const V*)) before including"
#endif

/* ============================================================================
 * HASHMAP_FN(to_stringbuf) — serialize hashmap to StringBuf
 * ========================================================================= */

/**
 * @brief Writes a human-readable representation of the hashmap into sb
 *
 * Iterates all occupied slots and formats each key–value pair using
 * the caller-supplied HASHMAP_FMT_KEY_FN and HASHMAP_FMT_VAL_FN functions.
 *
 * Output is appended to sb's current contents. If the StringBuf fills up
 * before all entries are written, the function returns false and sb contains
 * a partial result — no entries are lost from the map itself.
 *
 * @param sb   Pointer to an initialized StringBuf (must not be NULL)
 * @param map  Pointer to initialized hashmap (must not be NULL)
 * @return     true if all output fit into sb, false if sb was too small
 *
 * Performance:
 * - Time:  O(capacity) — iterates all slots regardless of occupancy
 * - Space: O(1) — no allocation
 */
static inline bool HASHMAP_FN(to_stringbuf)(
    borrowed(StringBuf*)         sb,
    const HASHMAP_TYPE_NAME*     map
) {
    require_msg(sb != NULL,  "hashmap_to_stringbuf: sb cannot be NULL");
    require_msg(map != NULL, "hashmap_to_stringbuf: map cannot be NULL");

    /* Header: hashmap { len=N, cap=N, load=0.NNN } { */
    f64 load = (map->capacity > 0)
        ? (f64)map->len / (f64)map->capacity
        : 0.0;

    if (!stringbuf_append_fmt(sb, "hashmap { len=%zu, cap=%zu, load=%.3f } {\n",
            (size_t)map->len, (size_t)map->capacity, load))
        return false;

    /* Entries */
    usize iter = 0;
    const HASHMAP_KEY_TYPE* k;
    const HASHMAP_VAL_TYPE* v;

    while (HASHMAP_FN(iter_next)(map, &iter, &k, &v)) {
        if (!stringbuf_append(sb, "  ["))   return false;
        if (!HASHMAP_FMT_KEY_FN(sb, k))     return false;
        if (!stringbuf_append(sb, "] => ")) return false;
        if (!HASHMAP_FMT_VAL_FN(sb, v))     return false;
        if (!stringbuf_append(sb, "\n"))    return false;
    }

    if (!stringbuf_append(sb, "}\n")) return false;

    return true;
}

#endif /* CANON_DATA_HASHMAP_FMT_H */
