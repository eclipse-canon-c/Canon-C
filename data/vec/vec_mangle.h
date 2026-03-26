#ifndef CANON_DATA_VEC_MANGLE_H
#define CANON_DATA_VEC_MANGLE_H

/**
 * @file vec_mangle.h
 * @brief Name mangling conventions for the Canon-C vec module
 *
 * Defines all identifier-generation macros used by vec_decl.h and vec_defn.h.
 * Every generated name in the vec module derives from exactly one macro here,
 * making the entire naming scheme overridable in one place.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Single source of truth for all vec identifiers
 * - Every macro is individually overridable via #ifndef guards
 * - Override before including this file to rename types or functions
 * - No logic here — pure token generation only
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later (token pasting via ##)
 * - No includes required — pure preprocessor
 *
 * Override example — rename vec_int to my_int_vec:
 * ```c
 * #define MANGLE_VEC_TYPE(type)  my_##type##_vec
 * #include "data/vec/vec_mangle.h"
 * #include "data/vec/vec_defn.h"
 * DEFINE_VEC(static inline, int)
 * // my_int_vec v = my_int_vec_init(buf, 64);
 * ```
 *
 * Override example — add a project prefix to all vec types:
 * ```c
 * #define MANGLE_VEC_TYPE(type)  myproject_vec_##type
 * #define MANGLE_VEC_ITER_TYPE(type)  myproject_vec_##type##_iter
 * #define MANGLE_VEC_SLICE_TYPE(type) myproject_vec_##type##_slice
 * #include "data/vec/vec_mangle.h"
 * ```
 *
 * @sa vec_decl.h, vec_defn.h, vec.h
 */

/* ════════════════════════════════════════════════════════════════
   Type and struct tag names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the vector type for a given element type
 *
 * Default: canon_vec_##type
 * Example: MANGLE_VEC_TYPE(int) → canon_vec_int
 */
#ifndef MANGLE_VEC_TYPE
    #define MANGLE_VEC_TYPE(type)               canon_vec_##type
#endif

/**
 * @brief Struct tag for the vector type (used in typedef struct TAG { } NAME)
 *
 * Default: canon_vec_##type##_s
 * Example: MANGLE_VEC_STRUCT_TAG(int) → canon_vec_int_s
 */
#ifndef MANGLE_VEC_STRUCT_TAG
    #define MANGLE_VEC_STRUCT_TAG(type)         canon_vec_##type##_s
#endif

/**
 * @brief Name of the iterator type for a given element type
 *
 * Default: canon_vec_##type##_iter
 * Example: MANGLE_VEC_ITER_TYPE(int) → canon_vec_int_iter
 */
#ifndef MANGLE_VEC_ITER_TYPE
    #define MANGLE_VEC_ITER_TYPE(type)          canon_vec_##type##_iter
#endif

/**
 * @brief Struct tag for the iterator type
 *
 * Default: canon_vec_##type##_iter_s
 */
#ifndef MANGLE_VEC_ITER_STRUCT_TAG
    #define MANGLE_VEC_ITER_STRUCT_TAG(type)    canon_vec_##type##_iter_s
#endif

/**
 * @brief Name of the slice type for a given element type
 *
 * Default: canon_vec_##type##_slice
 * Example: MANGLE_VEC_SLICE_TYPE(int) → canon_vec_int_slice
 */
#ifndef MANGLE_VEC_SLICE_TYPE
    #define MANGLE_VEC_SLICE_TYPE(type)         canon_vec_##type##_slice
#endif

/**
 * @brief Struct tag for the slice type
 *
 * Default: canon_vec_##type##_slice_s
 */
#ifndef MANGLE_VEC_SLICE_STRUCT_TAG
    #define MANGLE_VEC_SLICE_STRUCT_TAG(type)   canon_vec_##type##_slice_s
#endif

/* ════════════════════════════════════════════════════════════════
   Option integration names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the Option type for a given element type
 *
 * Default: option_##type  (matches CANON_OPTION naming from option.h)
 * Example: MANGLE_VEC_OPTION_TYPE(int) → option_int
 *
 * Used by: IMPL_VEC_GET_OPTION, IMPL_VEC_POP_OPTION, IMPL_VEC_REMOVE_OPTION
 */
#ifndef MANGLE_VEC_OPTION_TYPE
    #define MANGLE_VEC_OPTION_TYPE(type)        option_##type
#endif

/**
 * @brief Name of the Option Some constructor for a given element type
 *
 * Default: option_##type##_some  (matches CANON_OPTION naming from option.h)
 * Example: MANGLE_VEC_OPTION_SOME(int) → option_int_some
 *
 * Used by: IMPL_VEC_GET_OPTION, IMPL_VEC_POP_OPTION, IMPL_VEC_REMOVE_OPTION
 */
#ifndef MANGLE_VEC_OPTION_SOME
    #define MANGLE_VEC_OPTION_SOME(type)        option_##type##_some
#endif

/**
 * @brief Name of the Option None constructor for a given element type
 *
 * Default: option_##type##_none  (matches CANON_OPTION naming from option.h)
 * Example: MANGLE_VEC_OPTION_NONE(int) → option_int_none
 *
 * Used by: IMPL_VEC_GET_OPTION, IMPL_VEC_POP_OPTION, IMPL_VEC_REMOVE_OPTION
 */
#ifndef MANGLE_VEC_OPTION_NONE
    #define MANGLE_VEC_OPTION_NONE(type)        option_##type##_none
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
 * all vec instantiations (see CANON_RESULT_BOOL_ERROR_DEFINED in vec_impl.h).
 *
 * Override only if you have replaced the result type used by push/pop/insert/remove.
 *
 * Used by: IMPL_VEC_POP_OPTION, IMPL_VEC_REMOVE_OPTION
 */
#ifndef MANGLE_VEC_RESULT_IS_OK
    #define MANGLE_VEC_RESULT_IS_OK(type)       result__Bool_Error_is_ok
#endif

/* ════════════════════════════════════════════════════════════════
   Constructor / destructor names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the init function (caller-owned buffer constructor)
 *
 * Default: canon_vec_##type##_init
 */
#ifndef MANGLE_VEC_INIT
    #define MANGLE_VEC_INIT(type)               canon_vec_##type##_init
#endif

/**
 * @brief Name of the empty constructor (null/zero state)
 *
 * Default: canon_vec_##type##_empty
 */
#ifndef MANGLE_VEC_EMPTY
    #define MANGLE_VEC_EMPTY(type)              canon_vec_##type##_empty
#endif

/**
 * @brief Name of the heap allocation constructor
 *
 * Default: canon_vec_##type##_alloc
 */
#ifndef MANGLE_VEC_ALLOC
    #define MANGLE_VEC_ALLOC(type)              canon_vec_##type##_alloc
#endif

/**
 * @brief Name of the arena allocation constructor
 *
 * Default: canon_vec_##type##_arena_alloc
 */
#ifndef MANGLE_VEC_ARENA_ALLOC
    #define MANGLE_VEC_ARENA_ALLOC(type)        canon_vec_##type##_arena_alloc
#endif

/**
 * @brief Name of the heap free function
 *
 * Default: canon_vec_##type##_free
 */
#ifndef MANGLE_VEC_FREE
    #define MANGLE_VEC_FREE(type)               canon_vec_##type##_free
#endif

/* ════════════════════════════════════════════════════════════════
   Query names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the length query function
 *
 * Default: canon_vec_##type##_len
 */
#ifndef MANGLE_VEC_LEN
    #define MANGLE_VEC_LEN(type)                canon_vec_##type##_len
#endif

/**
 * @brief Name of the capacity query function
 *
 * Default: canon_vec_##type##_capacity
 */
#ifndef MANGLE_VEC_CAPACITY
    #define MANGLE_VEC_CAPACITY(type)           canon_vec_##type##_capacity
#endif

/**
 * @brief Name of the remaining-space query function
 *
 * Default: canon_vec_##type##_remaining
 */
#ifndef MANGLE_VEC_REMAINING
    #define MANGLE_VEC_REMAINING(type)          canon_vec_##type##_remaining
#endif

/**
 * @brief Name of the is-empty predicate
 *
 * Default: canon_vec_##type##_is_empty
 */
#ifndef MANGLE_VEC_IS_EMPTY
    #define MANGLE_VEC_IS_EMPTY(type)           canon_vec_##type##_is_empty
#endif

/**
 * @brief Name of the is-full predicate
 *
 * Default: canon_vec_##type##_is_full
 */
#ifndef MANGLE_VEC_IS_FULL
    #define MANGLE_VEC_IS_FULL(type)            canon_vec_##type##_is_full
#endif

/* ════════════════════════════════════════════════════════════════
   Element access names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the checked get function (returns bool + out param)
 *
 * Default: canon_vec_##type##_get
 */
#ifndef MANGLE_VEC_GET
    #define MANGLE_VEC_GET(type)                canon_vec_##type##_get
#endif

/**
 * @brief Name of the Option-returning get function
 *
 * Default: canon_vec_##type##_get_option
 */
#ifndef MANGLE_VEC_GET_OPTION
    #define MANGLE_VEC_GET_OPTION(type)         canon_vec_##type##_get_option
#endif

/**
 * @brief Name of the unchecked get function (no bounds check, fast path)
 *
 * Default: canon_vec_##type##_get_unchecked
 */
#ifndef MANGLE_VEC_GET_UNCHECKED
    #define MANGLE_VEC_GET_UNCHECKED(type)      canon_vec_##type##_get_unchecked
#endif

/**
 * @brief Name of the pointer-to-element accessor (at)
 *
 * Default: canon_vec_##type##_at
 */
#ifndef MANGLE_VEC_AT
    #define MANGLE_VEC_AT(type)                 canon_vec_##type##_at
#endif

/**
 * @brief Name of the checked set function
 *
 * Default: canon_vec_##type##_set
 */
#ifndef MANGLE_VEC_SET
    #define MANGLE_VEC_SET(type)                canon_vec_##type##_set
#endif

/**
 * @brief Name of the pointer-to-first-element function
 *
 * Default: canon_vec_##type##_first
 */
#ifndef MANGLE_VEC_FIRST
    #define MANGLE_VEC_FIRST(type)              canon_vec_##type##_first
#endif

/**
 * @brief Name of the pointer-to-last-element function
 *
 * Default: canon_vec_##type##_last
 */
#ifndef MANGLE_VEC_LAST
    #define MANGLE_VEC_LAST(type)               canon_vec_##type##_last
#endif

/**
 * @brief Name of the raw buffer pointer function
 *
 * Default: canon_vec_##type##_data
 */
#ifndef MANGLE_VEC_DATA
    #define MANGLE_VEC_DATA(type)               canon_vec_##type##_data
#endif

/* ════════════════════════════════════════════════════════════════
   Modification names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the Result-returning push function
 *
 * Default: canon_vec_##type##_push
 */
#ifndef MANGLE_VEC_PUSH
    #define MANGLE_VEC_PUSH(type)               canon_vec_##type##_push
#endif

/**
 * @brief Name of the bool-returning push function (no Result overhead)
 *
 * Default: canon_vec_##type##_try_push
 */
#ifndef MANGLE_VEC_TRY_PUSH
    #define MANGLE_VEC_TRY_PUSH(type)           canon_vec_##type##_try_push
#endif

/**
 * @brief Name of the unchecked push function (no bounds check, fast path)
 *
 * Default: canon_vec_##type##_push_unchecked
 */
#ifndef MANGLE_VEC_PUSH_UNCHECKED
    #define MANGLE_VEC_PUSH_UNCHECKED(type)     canon_vec_##type##_push_unchecked
#endif

/**
 * @brief Name of the Result-returning pop function
 *
 * Default: canon_vec_##type##_pop
 */
#ifndef MANGLE_VEC_POP
    #define MANGLE_VEC_POP(type)                canon_vec_##type##_pop
#endif

/**
 * @brief Name of the Option-returning pop function
 *
 * Default: canon_vec_##type##_pop_option
 */
#ifndef MANGLE_VEC_POP_OPTION
    #define MANGLE_VEC_POP_OPTION(type)         canon_vec_##type##_pop_option
#endif

/**
 * @brief Name of the clear function (reset len to 0)
 *
 * Default: canon_vec_##type##_clear
 */
#ifndef MANGLE_VEC_CLEAR
    #define MANGLE_VEC_CLEAR(type)              canon_vec_##type##_clear
#endif

/**
 * @brief Name of the Result-returning insert function
 *
 * Default: canon_vec_##type##_insert
 */
#ifndef MANGLE_VEC_INSERT
    #define MANGLE_VEC_INSERT(type)             canon_vec_##type##_insert
#endif

/**
 * @brief Name of the Result-returning remove function
 *
 * Default: canon_vec_##type##_remove
 */
#ifndef MANGLE_VEC_REMOVE
    #define MANGLE_VEC_REMOVE(type)             canon_vec_##type##_remove
#endif

/**
 * @brief Name of the Option-returning remove function
 *
 * Default: canon_vec_##type##_remove_option
 */
#ifndef MANGLE_VEC_REMOVE_OPTION
    #define MANGLE_VEC_REMOVE_OPTION(type)      canon_vec_##type##_remove_option
#endif

/* ════════════════════════════════════════════════════════════════
   Bulk operation names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the append-array bulk copy function
 *
 * Default: canon_vec_##type##_append_array
 */
#ifndef MANGLE_VEC_APPEND_ARRAY
    #define MANGLE_VEC_APPEND_ARRAY(type)       canon_vec_##type##_append_array
#endif

/**
 * @brief Name of the extend function (alias for append_array)
 *
 * Default: canon_vec_##type##_extend
 */
#ifndef MANGLE_VEC_EXTEND
    #define MANGLE_VEC_EXTEND(type)             canon_vec_##type##_extend
#endif

/**
 * @brief Name of the fill function (repeat a value n times)
 *
 * Default: canon_vec_##type##_fill
 */
#ifndef MANGLE_VEC_FILL
    #define MANGLE_VEC_FILL(type)               canon_vec_##type##_fill
#endif

/**
 * @brief Name of the swap function (O(1) swap of two vecs)
 *
 * Default: canon_vec_##type##_swap
 */
#ifndef MANGLE_VEC_SWAP
    #define MANGLE_VEC_SWAP(type)               canon_vec_##type##_swap
#endif

/* ════════════════════════════════════════════════════════════════
   Iterator names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the iterator constructor
 *
 * Default: canon_vec_##type##_iter_init
 */
#ifndef MANGLE_VEC_ITER_INIT
    #define MANGLE_VEC_ITER_INIT(type)          canon_vec_##type##_iter_init
#endif

/**
 * @brief Name of the iterator advance function
 *
 * Default: canon_vec_##type##_iter_next
 */
#ifndef MANGLE_VEC_ITER_NEXT
    #define MANGLE_VEC_ITER_NEXT(type)          canon_vec_##type##_iter_next
#endif

/* ════════════════════════════════════════════════════════════════
   Slice names
   ════════════════════════════════════════════════════════════════ */

/**
 * @brief Name of the slice constructor
 *
 * Default: canon_vec_##type##_slice_init
 */
#ifndef MANGLE_VEC_SLICE_INIT
    #define MANGLE_VEC_SLICE_INIT(type)         canon_vec_##type##_slice_init
#endif

/**
 * @brief Name of the slice element accessor
 *
 * Default: canon_vec_##type##_slice_get
 */
#ifndef MANGLE_VEC_SLICE_GET
    #define MANGLE_VEC_SLICE_GET(type)          canon_vec_##type##_slice_get
#endif

#endif /* CANON_DATA_VEC_MANGLE_H */
