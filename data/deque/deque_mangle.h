#ifndef CANON_DATA_DEQUE_MANGLE_H
#define CANON_DATA_DEQUE_MANGLE_H

/**
 * @file deque_mangle.h
 * @brief Name mangling conventions for the Canon-C deque module
 *
 * Defines all identifier-generation macros used by deque_decl.h and deque_defn.h.
 * Every generated name in the deque module derives from exactly one macro here,
 * making the entire naming scheme overridable in one place.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Single source of truth for all deque identifiers
 * - Every macro is individually overridable via #ifndef guards
 * - Override before including this file to rename types or functions
 * - No logic here — pure token generation only
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (token pasting via ##)
 * - No includes required — pure preprocessor
 *
 * Override example — add a project prefix:
 * ```c
 * #define MANGLE_DEQUE_TYPE(type)  myproject_deque_##type
 * #include "data/deque/deque_mangle.h"
 * #include "data/deque/deque_defn.h"
 * DEFINE_DEQUE(static inline, int)
 * ```
 *
 * @sa deque_decl.h, deque_defn.h, deque.h
 */

/* ════════════════════════════════════════════════════════════════
   Type and struct tag names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the deque type for a given element type
 *
 * Default: canon_deque_##type
 * Example: MANGLE_DEQUE_TYPE(int) → canon_deque_int
 */
#ifndef MANGLE_DEQUE_TYPE
    #define MANGLE_DEQUE_TYPE(type)             canon_deque_##type
#endif

/**
 * @brief Struct tag for the deque type
 *
 * Default: canon_deque_##type##_s
 */
#ifndef MANGLE_DEQUE_STRUCT_TAG
    #define MANGLE_DEQUE_STRUCT_TAG(type)       canon_deque_##type##_s
#endif

/* ════════════════════════════════════════════════════════════════
   Constructor name
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the init function (caller-owned buffer constructor)
 *
 * Default: canon_deque_##type##_init
 */
#ifndef MANGLE_DEQUE_INIT
    #define MANGLE_DEQUE_INIT(type)             canon_deque_##type##_init
#endif

/**
 * @brief Name of the empty constructor (null/zero state)
 *
 * Default: canon_deque_##type##_empty
 */
#ifndef MANGLE_DEQUE_EMPTY
    #define MANGLE_DEQUE_EMPTY(type)            canon_deque_##type##_empty
#endif

/* ════════════════════════════════════════════════════════════════
   Query names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the length query function
 *
 * Default: canon_deque_##type##_len
 */
#ifndef MANGLE_DEQUE_LEN
    #define MANGLE_DEQUE_LEN(type)              canon_deque_##type##_len
#endif

/**
 * @brief Name of the capacity query function
 *
 * Default: canon_deque_##type##_capacity
 */
#ifndef MANGLE_DEQUE_CAPACITY
    #define MANGLE_DEQUE_CAPACITY(type)         canon_deque_##type##_capacity
#endif

/**
 * @brief Name of the remaining-space query function
 *
 * Default: canon_deque_##type##_remaining
 */
#ifndef MANGLE_DEQUE_REMAINING
    #define MANGLE_DEQUE_REMAINING(type)        canon_deque_##type##_remaining
#endif

/**
 * @brief Name of the is-empty predicate
 *
 * Default: canon_deque_##type##_is_empty
 */
#ifndef MANGLE_DEQUE_IS_EMPTY
    #define MANGLE_DEQUE_IS_EMPTY(type)         canon_deque_##type##_is_empty
#endif

/**
 * @brief Name of the is-full predicate
 *
 * Default: canon_deque_##type##_is_full
 */
#ifndef MANGLE_DEQUE_IS_FULL
    #define MANGLE_DEQUE_IS_FULL(type)          canon_deque_##type##_is_full
#endif

/* ════════════════════════════════════════════════════════════════
   Push names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the push-to-front function
 *
 * Default: canon_deque_##type##_push_front
 */
#ifndef MANGLE_DEQUE_PUSH_FRONT
    #define MANGLE_DEQUE_PUSH_FRONT(type)       canon_deque_##type##_push_front
#endif

/**
 * @brief Name of the push-to-back function
 *
 * Default: canon_deque_##type##_push_back
 */
#ifndef MANGLE_DEQUE_PUSH_BACK
    #define MANGLE_DEQUE_PUSH_BACK(type)        canon_deque_##type##_push_back
#endif

/* ════════════════════════════════════════════════════════════════
   Pop names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the pop-from-front function
 *
 * Default: canon_deque_##type##_pop_front
 */
#ifndef MANGLE_DEQUE_POP_FRONT
    #define MANGLE_DEQUE_POP_FRONT(type)        canon_deque_##type##_pop_front
#endif

/**
 * @brief Name of the pop-from-back function
 *
 * Default: canon_deque_##type##_pop_back
 */
#ifndef MANGLE_DEQUE_POP_BACK
    #define MANGLE_DEQUE_POP_BACK(type)         canon_deque_##type##_pop_back
#endif

/**
 * @brief Name of the Option-returning pop-from-front function
 *
 * Default: canon_deque_##type##_pop_front_option
 */
#ifndef MANGLE_DEQUE_POP_FRONT_OPTION
    #define MANGLE_DEQUE_POP_FRONT_OPTION(type) canon_deque_##type##_pop_front_option
#endif

/**
 * @brief Name of the Option-returning pop-from-back function
 *
 * Default: canon_deque_##type##_pop_back_option
 */
#ifndef MANGLE_DEQUE_POP_BACK_OPTION
    #define MANGLE_DEQUE_POP_BACK_OPTION(type)  canon_deque_##type##_pop_back_option
#endif

/* ════════════════════════════════════════════════════════════════
   Peek names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the peek-at-front function (non-mutating)
 *
 * Default: canon_deque_##type##_peek_front
 */
#ifndef MANGLE_DEQUE_PEEK_FRONT
    #define MANGLE_DEQUE_PEEK_FRONT(type)       canon_deque_##type##_peek_front
#endif

/**
 * @brief Name of the peek-at-back function (non-mutating)
 *
 * Default: canon_deque_##type##_peek_back
 */
#ifndef MANGLE_DEQUE_PEEK_BACK
    #define MANGLE_DEQUE_PEEK_BACK(type)        canon_deque_##type##_peek_back
#endif

/**
 * @brief Name of the Option-returning peek-at-front function
 *
 * Default: canon_deque_##type##_peek_front_option
 */
#ifndef MANGLE_DEQUE_PEEK_FRONT_OPTION
    #define MANGLE_DEQUE_PEEK_FRONT_OPTION(type) canon_deque_##type##_peek_front_option
#endif

/**
 * @brief Name of the Option-returning peek-at-back function
 *
 * Default: canon_deque_##type##_peek_back_option
 */
#ifndef MANGLE_DEQUE_PEEK_BACK_OPTION
    #define MANGLE_DEQUE_PEEK_BACK_OPTION(type)  canon_deque_##type##_peek_back_option
#endif

/* ════════════════════════════════════════════════════════════════
   Misc operation names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the clear function (reset size/head/tail to 0)
 *
 * Default: canon_deque_##type##_clear
 */
#ifndef MANGLE_DEQUE_CLEAR
    #define MANGLE_DEQUE_CLEAR(type)            canon_deque_##type##_clear
#endif

/**
 * @brief Name of the swap function (O(1) exchange of two deques)
 *
 * Default: canon_deque_##type##_swap
 */
#ifndef MANGLE_DEQUE_SWAP
    #define MANGLE_DEQUE_SWAP(type)             canon_deque_##type##_swap
#endif

#endif /* CANON_DATA_DEQUE_MANGLE_H */
