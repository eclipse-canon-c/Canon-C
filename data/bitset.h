#ifndef CANON_DATA_BITSET_H
#define CANON_DATA_BITSET_H

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/bits.h"
#include "core/primitives/contract.h"
#include "core/memory.h"
#include "core/slice.h"
#include "core/ownership.h"

/**
 * @file data/bitset.h
 * @brief Fixed-capacity bitset with O(1) set/clear/test and O(n/64) bulk ops
 *
 * A Bitset is a compact array of bits backed by a caller-provided u64 buffer.
 * Capacity is fixed at initialization. No allocation inside any function.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Capacity is fixed — no growth, no reallocation
 * - Caller owns the backing buffer (stack, arena, static)
 * - All bit operations are O(1); bulk ops (count, find, and/or/xor) are O(n/64)
 * - Backed by u64 words — 64 bits per word, cache-friendly
 * - Bits beyond capacity are always kept zero (padding invariant)
 * - bytes_t view available via bitset_as_bytes() for I/O and serialization
 *
 * NULL contract (uniform across all functions):
 * ────────────────────────────────────────────────────────────────────────────
 * - A NULL Bitset* is always a silent no-op for void functions
 * - Query functions return 0, false, or BITSET_NPOS for NULL input
 * - Out-of-bounds index on a valid (non-NULL) Bitset* is a contract violation
 *   and fires require_msg — it is a programming error, not a recoverable condition
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * data/bitset.h is in data/. It depends only on core/.
 * No other data/ headers may be included here.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * NOT thread-safe. Caller must synchronize if shared across threads.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Assumes u64 is 64 bits (guaranteed by core/primitives/types.h)
 * - No platform-specific code
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - set / clear / toggle / test:  O(1)
 * - count (popcount all bits):    O(n/64)
 * - find_first / find_next:       O(n/64) worst case
 * - and / or / xor / not:         O(n/64)
 * - clear_all / set_all:          O(n/64)
 *
 * Buffer sizing:
 * ────────────────────────────────────────────────────────────────────────────
 * Use BITSET_WORD_COUNT(n) to compute the number of u64 words needed for n bits.
 *
 * Quick start:
 * ```c
 * #include "data/bitset.h"
 *
 * // Stack-backed bitset for 200 bits
 * u64 words[BITSET_WORD_COUNT(200)];
 * Bitset bs;
 * bitset_init(&bs, words, 200);
 *
 * bitset_set(&bs, 42);
 * bitset_set(&bs, 199);
 * bool v = bitset_test(&bs, 42);      // true
 * usize n = bitset_count(&bs);        // 2
 * usize f = bitset_find_first(&bs);   // 42
 *
 * // Arena-backed
 * u64* buf = arena_alloc(&arena, BITSET_WORD_COUNT(1024) * sizeof(u64));
 * Bitset bs2;
 * bitset_init(&bs2, buf, 1024);
 * ```
 *
 * @sa core/primitives/bits.h — bits_popcount, bits_ctz, bits_clz used internally
 * @sa core/slice.h           — bytes_t used by bitset_as_bytes()
 * @sa core/ownership.h       — borrowed() annotation
 */

/* ════════════════════════════════════════════════════════════════════════════
   Constants and sizing macros
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Bits per backing word */
#define BITSET_BITS_PER_WORD ((usize)64)

/**
 * @brief Number of u64 words required to hold n bits
 *
 * Example: BITSET_WORD_COUNT(64) == 1, BITSET_WORD_COUNT(65) == 2
 */
#define BITSET_WORD_COUNT(n) \
    (((usize)(n) + BITSET_BITS_PER_WORD - 1) / BITSET_BITS_PER_WORD)

/** @brief Sentinel returned by bitset_find_first / bitset_find_next when no bit is set */
#define BITSET_NPOS CANON_USIZE_MAX

/* ════════════════════════════════════════════════════════════════════════════
   Bitset struct
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Fixed-capacity bitset
 *
 * Do not access fields directly — use the provided functions.
 *
 * Invariant: bits at indices [capacity, word_count*64) are always zero.
 */
typedef struct {
    u64*  words;      ///< Backing word array (caller-owned)
    usize capacity;   ///< Number of usable bits
    usize word_count; ///< Number of u64 words in backing array
} Bitset;

/* ════════════════════════════════════════════════════════════════════════════
   Internal helpers
   ════════════════════════════════════════════════════════════════════════════ */

/** @brief Word index for bit i */
static inline usize bitset_word_of(usize i) { return i / BITSET_BITS_PER_WORD; }

/** @brief Bit mask for bit i within its word */
static inline u64 bitset_mask_of(usize i) { return (u64)1 << (i % BITSET_BITS_PER_WORD); }

/**
 * @brief Clears padding bits in the last word to maintain the invariant
 *
 * Called after set_all and bitset_not to ensure bits beyond capacity stay zero.
 */
static inline void bitset_clear_padding(borrowed(Bitset*) bs) {
    require_msg(bs != NULL, "bitset_clear_padding: bs cannot be NULL");
    usize rem = bs->capacity % BITSET_BITS_PER_WORD;
    if (rem != 0) {
        bs->words[bs->word_count - 1] &= ((u64)1 << rem) - 1;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Initialization
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes a Bitset with a caller-provided u64 buffer
 *
 * All bits are cleared on initialization.
 *
 * @param bs       Pointer to uninitialized Bitset
 * @param words    Caller-provided buffer of at least BITSET_WORD_COUNT(capacity) u64s
 * @param capacity Number of usable bits (> 0)
 *
 * @pre bs != NULL
 * @pre words != NULL
 * @pre capacity > 0
 * @post All bits cleared, bs ready to use
 *
 * Performance: O(n/64) — clears backing words
 */
static inline void bitset_init(borrowed(Bitset*) bs, borrowed(u64*) words, usize capacity) {
    require_msg(bs       != NULL, "bitset_init: bs cannot be NULL");
    require_msg(words    != NULL, "bitset_init: words cannot be NULL");
    require_msg(capacity  > 0,    "bitset_init: capacity must be > 0");

    bs->words      = words;
    bs->capacity   = capacity;
    bs->word_count = BITSET_WORD_COUNT(capacity);
    mem_zero(words, bs->word_count * sizeof(u64));
}

/* ════════════════════════════════════════════════════════════════════════════
   Single-bit operations — O(1)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sets bit i (makes it 1)
 *
 * NULL bs is a no-op. Out-of-bounds i on a valid bs is a contract violation.
 *
 * @pre i < bs->capacity (when bs != NULL)
 * Performance: O(1)
 */
static inline void bitset_set(borrowed(Bitset*) bs, usize i) {
    if (!bs) return;
    require_msg(i < bs->capacity, "bitset_set: index out of range");
    bs->words[bitset_word_of(i)] |= bitset_mask_of(i);
}

/**
 * @brief Clears bit i (makes it 0)
 *
 * NULL bs is a no-op. Out-of-bounds i on a valid bs is a contract violation.
 *
 * @pre i < bs->capacity (when bs != NULL)
 * Performance: O(1)
 */
static inline void bitset_clear(borrowed(Bitset*) bs, usize i) {
    if (!bs) return;
    require_msg(i < bs->capacity, "bitset_clear: index out of range");
    bs->words[bitset_word_of(i)] &= ~bitset_mask_of(i);
}

/**
 * @brief Toggles bit i (0 → 1, 1 → 0)
 *
 * NULL bs is a no-op. Out-of-bounds i on a valid bs is a contract violation.
 *
 * @pre i < bs->capacity (when bs != NULL)
 * Performance: O(1)
 */
static inline void bitset_toggle(borrowed(Bitset*) bs, usize i) {
    if (!bs) return;
    require_msg(i < bs->capacity, "bitset_toggle: index out of range");
    bs->words[bitset_word_of(i)] ^= bitset_mask_of(i);
}

/**
 * @brief Returns true if bit i is set
 *
 * NULL bs returns false. Out-of-bounds i on a valid bs is a contract violation.
 *
 * @pre i < bs->capacity (when bs != NULL)
 * @return false if bs == NULL
 * Performance: O(1)
 */
static inline bool bitset_test(borrowed(const Bitset*) bs, usize i) {
    if (!bs) return false;
    require_msg(i < bs->capacity, "bitset_test: index out of range");
    return (bs->words[bitset_word_of(i)] & bitset_mask_of(i)) != 0;
}

/**
 * @brief Sets bit i to value (true = 1, false = 0)
 *
 * NULL bs is a no-op. Out-of-bounds i on a valid bs is a contract violation.
 *
 * @pre i < bs->capacity (when bs != NULL)
 * Performance: O(1)
 */
static inline void bitset_assign(borrowed(Bitset*) bs, usize i, bool value) {
    if (value) bitset_set(bs, i);
    else       bitset_clear(bs, i);
}

/* ════════════════════════════════════════════════════════════════════════════
   Bulk operations — O(n/64)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Clears all bits
 *
 * NULL bs is a no-op.
 * Performance: O(n/64)
 */
static inline void bitset_clear_all(borrowed(Bitset*) bs) {
    if (!bs || !bs->words) return;
    mem_zero(bs->words, bs->word_count * sizeof(u64));
}

/**
 * @brief Sets all bits within capacity
 *
 * NULL bs is a no-op.
 * Performance: O(n/64)
 */
static inline void bitset_set_all(borrowed(Bitset*) bs) {
    if (!bs || !bs->words) return;
    mem_set(bs->words, 0xFF, bs->word_count * sizeof(u64));
    bitset_clear_padding(bs);
}

/**
 * @brief Inverts all bits within capacity
 *
 * NULL bs is a no-op.
 * Performance: O(n/64)
 */
static inline void bitset_not(borrowed(Bitset*) bs) {
    if (!bs || !bs->words) return;
    for (usize w = 0; w < bs->word_count; w++) {
        bs->words[w] = ~bs->words[w];
    }
    bitset_clear_padding(bs);
}

/**
 * @brief AND: bs &= other (in-place)
 *
 * NULL bs or other is a no-op.
 * Operates on min(bs->word_count, other->word_count) words.
 * Performance: O(n/64)
 */
static inline void bitset_and(borrowed(Bitset*) bs, borrowed(const Bitset*) other) {
    if (!bs || !other) return;
    usize n = bs->word_count < other->word_count ? bs->word_count : other->word_count;
    for (usize w = 0; w < n; w++) {
        bs->words[w] &= other->words[w];
    }
    /* words beyond other's range become 0 — AND with implicit 0 */
    for (usize w = n; w < bs->word_count; w++) {
        bs->words[w] = 0;
    }
}

/**
 * @brief OR: bs |= other (in-place)
 *
 * NULL bs or other is a no-op.
 * Operates on min(bs->word_count, other->word_count) words.
 * Performance: O(n/64)
 */
static inline void bitset_or(borrowed(Bitset*) bs, borrowed(const Bitset*) other) {
    if (!bs || !other) return;
    usize n = bs->word_count < other->word_count ? bs->word_count : other->word_count;
    for (usize w = 0; w < n; w++) {
        bs->words[w] |= other->words[w];
    }
}

/**
 * @brief XOR: bs ^= other (in-place)
 *
 * NULL bs or other is a no-op.
 * Operates on min(bs->word_count, other->word_count) words.
 * Performance: O(n/64)
 */
static inline void bitset_xor(borrowed(Bitset*) bs, borrowed(const Bitset*) other) {
    if (!bs || !other) return;
    usize n = bs->word_count < other->word_count ? bs->word_count : other->word_count;
    for (usize w = 0; w < n; w++) {
        bs->words[w] ^= other->words[w];
    }
    bitset_clear_padding(bs);
}

/* ════════════════════════════════════════════════════════════════════════════
   Query — O(n/64)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the number of set bits (popcount)
 *
 * NULL bs returns 0.
 * Uses bits_popcount from core/primitives/bits.h per word.
 * Performance: O(n/64)
 */
static inline usize bitset_count(borrowed(const Bitset*) bs) {
    if (!bs || !bs->words) return 0;
    usize total = 0;
    for (usize w = 0; w < bs->word_count; w++) {
        total += (usize)bits_popcount(bs->words[w]);
    }
    return total;
}

/**
 * @brief Returns true if no bits are set
 *
 * NULL bs returns true.
 * Performance: O(n/64)
 */
static inline bool bitset_is_empty(borrowed(const Bitset*) bs) {
    if (!bs || !bs->words) return true;
    for (usize w = 0; w < bs->word_count; w++) {
        if (bs->words[w] != 0) return false;
    }
    return true;
}

/**
 * @brief Returns true if all bits within capacity are set
 *
 * NULL bs returns false.
 * Performance: O(n/64)
 */
static inline bool bitset_is_full(borrowed(const Bitset*) bs) {
    if (!bs || !bs->words) return false;
    return bitset_count(bs) == bs->capacity;
}

/**
 * @brief Returns true if bs and other have no bits in common (disjoint)
 *
 * NULL bs or other returns true.
 * Performance: O(n/64)
 */
static inline bool bitset_is_disjoint(borrowed(const Bitset*) bs,
                                      borrowed(const Bitset*) other) {
    if (!bs || !other) return true;
    usize n = bs->word_count < other->word_count ? bs->word_count : other->word_count;
    for (usize w = 0; w < n; w++) {
        if (bs->words[w] & other->words[w]) return false;
    }
    return true;
}

/**
 * @brief Returns the capacity (total number of usable bits)
 *
 * NULL bs returns 0.
 * Performance: O(1)
 */
static inline usize bitset_capacity(borrowed(const Bitset*) bs) {
    return bs ? bs->capacity : 0;
}

/* ════════════════════════════════════════════════════════════════════════════
   Search — O(n/64)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the index of the first set bit, or BITSET_NPOS if none
 *
 * NULL bs returns BITSET_NPOS.
 * Uses bits_ctz (count trailing zeros) from core/primitives/bits.h.
 * Performance: O(n/64) worst case
 */
static inline usize bitset_find_first(borrowed(const Bitset*) bs) {
    if (!bs || !bs->words) return BITSET_NPOS;
    for (usize w = 0; w < bs->word_count; w++) {
        if (bs->words[w] != 0) {
            usize bit = w * BITSET_BITS_PER_WORD + (usize)bits_ctz(bs->words[w]);
            return bit < bs->capacity ? bit : BITSET_NPOS;
        }
    }
    return BITSET_NPOS;
}

/**
 * @brief Returns the index of the next set bit after position prev, or BITSET_NPOS
 *
 * NULL bs returns BITSET_NPOS.
 * Used for iterating over set bits:
 * ```c
 * for (usize i = bitset_find_first(&bs);
 *      i != BITSET_NPOS;
 *      i = bitset_find_next(&bs, i)) {
 *     // process bit i
 * }
 * ```
 *
 * Performance: O(n/64) worst case per call
 */
static inline usize bitset_find_next(borrowed(const Bitset*) bs, usize prev) {
    if (!bs || !bs->words || prev >= bs->capacity) return BITSET_NPOS;

    usize start = prev + 1;
    if (start >= bs->capacity) return BITSET_NPOS;

    usize w   = bitset_word_of(start);
    usize bit = start % BITSET_BITS_PER_WORD;

    /* mask off bits before 'start' in the first word */
    u64 word = bs->words[w] >> bit;
    if (word != 0) {
        usize found = w * BITSET_BITS_PER_WORD + bit + (usize)bits_ctz(word);
        return found < bs->capacity ? found : BITSET_NPOS;
    }

    for (w++; w < bs->word_count; w++) {
        if (bs->words[w] != 0) {
            usize found = w * BITSET_BITS_PER_WORD + (usize)bits_ctz(bs->words[w]);
            return found < bs->capacity ? found : BITSET_NPOS;
        }
    }
    return BITSET_NPOS;
}

/**
 * @brief Returns the index of the last set bit, or BITSET_NPOS if none
 *
 * NULL bs returns BITSET_NPOS.
 * Uses bits_clz (count leading zeros) from core/primitives/bits.h.
 * Performance: O(n/64) worst case
 */
static inline usize bitset_find_last(borrowed(const Bitset*) bs) {
    if (!bs || !bs->words) return BITSET_NPOS;
    usize w = bs->word_count;
    while (w-- > 0) {
        if (bs->words[w] != 0) {
            usize bit = w * BITSET_BITS_PER_WORD +
                        (BITSET_BITS_PER_WORD - 1 - (usize)bits_clz(bs->words[w]));
            return bit < bs->capacity ? bit : BITSET_NPOS;
        }
    }
    return BITSET_NPOS;
}

/* ════════════════════════════════════════════════════════════════════════════
   View — O(1)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns a bytes_t view over the raw backing words
 *
 * NULL bs returns bytes_empty().
 * Covers the entire word array including any padding bits (always zero).
 * Useful for serialization, hashing, or passing to byte-level APIs.
 * Non-owning — do not free the returned bytes_t.ptr.
 *
 * Performance: O(1)
 */
static inline bytes_t bitset_as_bytes(borrowed(const Bitset*) bs) {
    if (!bs || !bs->words) return bytes_empty();
    return bytes_from(bs->words, bs->word_count * sizeof(u64));
}

/* ════════════════════════════════════════════════════════════════════════════
   Iteration macro
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def BITSET_FOR_EACH(bs_ptr, idx_var)
 * @brief Iterates over all set bit indices in a Bitset
 *
 * @param bs_ptr  Pointer to Bitset
 * @param idx_var usize variable name to use as the loop index
 *
 * Example:
 * ```c
 * BITSET_FOR_EACH(&bs, i) {
 *     printf("bit %zu is set\n", i);
 * }
 * ```
 */
#define BITSET_FOR_EACH(bs_ptr, idx_var) \
    for (usize idx_var = bitset_find_first(bs_ptr); \
         idx_var != BITSET_NPOS; \
         idx_var = bitset_find_next((bs_ptr), idx_var))

#endif /* CANON_DATA_BITSET_H */
