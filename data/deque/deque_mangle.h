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
    #define MANGLE_DEQUE_TYPE(type)                 canon_deque_##type
#endif

/**
 * @brief Struct tag for the deque type
 *
 * Default: canon_deque_##type##_s
 */
#ifndef MANGLE_DEQUE_STRUCT_TAG
    #define MANGLE_DEQUE_STRUCT_TAG(type)           canon_deque_##type##_s
#endif

/* ════════════════════════════════════════════════════════════════
   Option integration names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the Option type for a given element type
 *
 * Default: option_##type  (matches CANON_OPTION naming from option.h)
 * Example: MANGLE_DEQUE_OPTION_TYPE(int) → option_int
 *
 * Used by: IMPL_DEQUE_POP_FRONT_OPTION, IMPL_DEQUE_POP_BACK_OPTION,
 *          IMPL_DEQUE_PEEK_FRONT_OPTION, IMPL_DEQUE_PEEK_BACK_OPTION,
 *          and DECLARE_DEQUE extern signatures.
 */
#ifndef MANGLE_DEQUE_OPTION_TYPE
    #define MANGLE_DEQUE_OPTION_TYPE(type)          option_##type
#endif

/**
 * @brief Name of the Option Some constructor for a given element type
 *
 * Default: option_##type##_some  (matches CANON_OPTION naming from option.h)
 * Example: MANGLE_DEQUE_OPTION_SOME(int) → option_int_some
 *
 * Used by: IMPL_DEQUE_POP_FRONT_OPTION, IMPL_DEQUE_POP_BACK_OPTION,
 *          IMPL_DEQUE_PEEK_FRONT_OPTION, IMPL_DEQUE_PEEK_BACK_OPTION
 */
#ifndef MANGLE_DEQUE_OPTION_SOME
    #define MANGLE_DEQUE_OPTION_SOME(type)          option_##type##_some
#endif

/**
 * @brief Name of the Option None constructor for a given element type
 *
 * Default: option_##type##_none  (matches CANON_OPTION naming from option.h)
 * Example: MANGLE_DEQUE_OPTION_NONE(int) → option_int_none
 *
 * Used by: IMPL_DEQUE_POP_FRONT_OPTION, IMPL_DEQUE_POP_BACK_OPTION,
 *          IMPL_DEQUE_PEEK_FRONT_OPTION, IMPL_DEQUE_PEEK_BACK_OPTION
 */
#ifndef MANGLE_DEQUE_OPTION_NONE
    #define MANGLE_DEQUE_OPTION_NONE(type)          option_##type##_none
#endif

/* ════════════════════════════════════════════════════════════════
   Result integration names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the result__Bool_Error is_ok checker
 *
 * Default: result__Bool_Error_is_ok
 *
 * Note: CANON_RESULT(bool, Error) generates result__Bool_Error (not
 * result_bool_Error) because bool expands to _Bool before token-pasting
 * in C99. This macro uses the correct generated name.
 *
 * The type parameter is accepted for macro uniformity but ignored in the
 * default expansion — the result type is fixed as result__Bool_Error across
 * all deque instantiations (see CANON_RESULT_BOOL_ERROR_DEFINED in deque_impl.h).
 *
 * Override only if you have replaced the result type used by push/pop.
 *
 * Used by: IMPL_DEQUE_POP_FRONT_OPTION, IMPL_DEQUE_POP_BACK_OPTION
 */
#ifndef MANGLE_DEQUE_RESULT_IS_OK
    #define MANGLE_DEQUE_RESULT_IS_OK(type)         result__Bool_Error_is_ok
#endif

/* ════════════════════════════════════════════════════════════════
   Constructor names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the init function (caller-owned buffer constructor)
 *
 * Default: canon_deque_##type##_init
 */
#ifndef MANGLE_DEQUE_INIT
    #define MANGLE_DEQUE_INIT(type)                 canon_deque_##type##_init
#endif

/**
 * @brief Name of the empty constructor (null/zero state)
 *
 * Default: canon_deque_##type##_empty
 */
#ifndef MANGLE_DEQUE_EMPTY
    #define MANGLE_DEQUE_EMPTY(type)                canon_deque_##type##_empty
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
    #define MANGLE_DEQUE_LEN(type)                  canon_deque_##type##_len
#endif

/**
 * @brief Name of the capacity query function
 *
 * Default: canon_deque_##type##_capacity
 */
#ifndef MANGLE_DEQUE_CAPACITY
    #define MANGLE_DEQUE_CAPACITY(type)             canon_deque_##type##_capacity
#endif

/**
 * @brief Name of the remaining-space query function
 *
 * Default: canon_deque_##type##_remaining
 */
#ifndef MANGLE_DEQUE_REMAINING
    #define MANGLE_DEQUE_REMAINING(type)            canon_deque_##type##_remaining
#endif

/**
 * @brief Name of the is-empty predicate
 *
 * Default: canon_deque_##type##_is_empty
 */
#ifndef MANGLE_DEQUE_IS_EMPTY
    #define MANGLE_DEQUE_IS_EMPTY(type)             canon_deque_##type##_is_empty
#endif

/**
 * @brief Name of the is-full predicate
 *
 * Default: canon_deque_##type##_is_full
 */
#ifndef MANGLE_DEQUE_IS_FULL
    #define MANGLE_DEQUE_IS_FULL(type)              canon_deque_##type##_is_full
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
    #define MANGLE_DEQUE_PUSH_FRONT(type)           canon_deque_##type##_push_front
#endif

/**
 * @brief Name of the push-to-back function
 *
 * Default: canon_deque_##type##_push_back
 */
#ifndef MANGLE_DEQUE_PUSH_BACK
    #define MANGLE_DEQUE_PUSH_BACK(type)            canon_deque_##type##_push_back
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
    #define MANGLE_DEQUE_POP_FRONT(type)            canon_deque_##type##_pop_front
#endif

/**
 * @brief Name of the pop-from-back function
 *
 * Default: canon_deque_##type##_pop_back
 */
#ifndef MANGLE_DEQUE_POP_BACK
    #define MANGLE_DEQUE_POP_BACK(type)             canon_deque_##type##_pop_back
#endif

/**
 * @brief Name of the Option-returning pop-from-front function
 *
 * Default: canon_deque_##type##_pop_front_option
 */
#ifndef MANGLE_DEQUE_POP_FRONT_OPTION
    #define MANGLE_DEQUE_POP_FRONT_OPTION(type)     canon_deque_##type##_pop_front_option
#endif

/**
 * @brief Name of the Option-returning pop-from-back function
 *
 * Default: canon_deque_##type##_pop_back_option
 */
#ifndef MANGLE_DEQUE_POP_BACK_OPTION
    #define MANGLE_DEQUE_POP_BACK_OPTION(type)      canon_deque_##type##_pop_back_option
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
    #define MANGLE_DEQUE_PEEK_FRONT(type)           canon_deque_##type##_peek_front
#endif

/**
 * @brief Name of the peek-at-back function (non-mutating)
 *
 * Default: canon_deque_##type##_peek_back
 */
#ifndef MANGLE_DEQUE_PEEK_BACK
    #define MANGLE_DEQUE_PEEK_BACK(type)            canon_deque_##type##_peek_back
#endif

/**
 * @brief Name of the Option-returning peek-at-front function
 *
 * Default: canon_deque_##type##_peek_front_option
 */
#ifndef MANGLE_DEQUE_PEEK_FRONT_OPTION
    #define MANGLE_DEQUE_PEEK_FRONT_OPTION(type)    canon_deque_##type##_peek_front_option
#endif

/**
 * @brief Name of the Option-returning peek-at-back function
 *
 * Default: canon_deque_##type##_peek_back_option
 */
#ifndef MANGLE_DEQUE_PEEK_BACK_OPTION
    #define MANGLE_DEQUE_PEEK_BACK_OPTION(type)     canon_deque_##type##_peek_back_option
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
    #define MANGLE_DEQUE_CLEAR(type)                canon_deque_##type##_clear
#endif

/**
 * @brief Name of the swap function (O(1) exchange of two deques)
 *
 * Default: canon_deque_##type##_swap
 */
#ifndef MANGLE_DEQUE_SWAP
    #define MANGLE_DEQUE_SWAP(type)                 canon_deque_##type##_swap
#endif

#endif /* CANON_DATA_DEQUE_MANGLE_H */
