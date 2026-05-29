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

#ifndef CANON_DATA_VEC_DEFN_H
#define CANON_DATA_VEC_DEFN_H

#include "data/vec/vec_mangle.h"
#include "data/vec/vec_impl.h"

/**
 * @file vec_defn.h
 * @brief Function definitions for Canon-C typed vectors
 *
 * Use DEFINE_VEC(linkage, type) to emit all struct typedefs and function
 * definitions for a typed vector. The linkage parameter controls visibility.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Emits complete function bodies via vec_impl.h macros
 * - Linkage is caller-controlled: static inline, static, or extern
 * - For header-only use, pass `static inline`
 * - For separate compilation, pass `static` or leave empty + use vec_decl.h
 * - Instantiating the same type twice in the same translation unit is safe
 *   (include guard on this file + impl guards on struct defs)
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 *
 * Header-only usage (no .c file needed):
 * ```c
 * // In any .c or .h file:
 * #include "data/vec/vec_defn.h"
 * DEFINE_VEC(static inline, int)
 * DEFINE_VEC(static inline, float)
 *
 * // Use immediately:
 * int buf[64];
 * vec_int v = vec_int_init(buf, 64);
 * vec_int_push(&v, 42);
 * ```
 *
 * Separate compilation usage:
 * ```c
 * // In tasks.h:
 * #include "data/vec/vec_decl.h"
 * DECLARE_VEC(Task)
 *
 * // In tasks.c:
 * #include "data/vec/vec_defn.h"
 * DEFINE_VEC(, Task)  // empty linkage = external linkage
 * ```
 *
 * Pointer types (use typedef first):
 * ```c
 * typedef const char* cstr;
 * DEFINE_VEC(static inline, cstr)
 * ```
 *
 * voidptr specialization (replaces old vec_voidptr):
 * ```c
 * typedef void* voidptr;
 * DEFINE_VEC(static inline, voidptr)
 * // vec_voidptr v = vec_voidptr_init(buf, 64);
 * ```
 *
 * @sa vec_decl.h, vec_mangle.h, vec_impl.h, vec.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_VEC — emit all typedefs and function definitions for a typed vector
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Instantiates a complete typed vector for element type `type`
 *
 * Generated types (using default mangle):
 * - vec_##type
 * - vec_##type##_iter
 * - vec_##type##_slice
 *
 * Generated functions (using default mangle):
 *
 * Constructors:
 * - vec_##type##_init(buffer, capacity)     → vec_##type
 * - vec_##type##_empty()                    → vec_##type
 * - vec_##type##_alloc(capacity)            → vec_##type
 * - vec_##type##_arena_alloc(arena, cap)    → vec_##type
 * - vec_##type##_free(v)                    → void
 *
 * Queries:
 * - vec_##type##_len(v)                     → usize
 * - vec_##type##_capacity(v)                → usize
 * - vec_##type##_remaining(v)               → usize
 * - vec_##type##_is_empty(v)                → bool
 * - vec_##type##_is_full(v)                 → bool
 *
 * Element access:
 * - vec_##type##_get(v, i, out)             → bool
 * - vec_##type##_get_option(v, i)           → option_##type
 * - vec_##type##_get_unchecked(v, i)        → type
 * - vec_##type##_at(v, i)                   → type*
 * - vec_##type##_set(v, i, val)             → bool
 * - vec_##type##_first(v)                   → type*
 * - vec_##type##_last(v)                    → type*
 * - vec_##type##_data(v)                    → type*
 *
 * Modification:
 * - vec_##type##_push(v, item)              → result_bool_Error
 * - vec_##type##_try_push(v, item)          → bool
 * - vec_##type##_push_unchecked(v, item)    → void
 * - vec_##type##_pop(v, out)                → result_bool_Error
 * - vec_##type##_pop_option(v)              → option_##type
 * - vec_##type##_clear(v)                   → void
 * - vec_##type##_insert(v, i, item)         → result_bool_Error
 * - vec_##type##_remove(v, i, out)          → result_bool_Error
 * - vec_##type##_remove_option(v, i)        → option_##type
 *
 * Bulk:
 * - vec_##type##_append_array(v, src, n)    → result_bool_Error
 * - vec_##type##_extend(v, src, n)          → result_bool_Error
 * - vec_##type##_fill(v, val, n)            → void
 * - vec_##type##_swap(a, b)                 → void
 *
 * Iterator:
 * - vec_##type##_iter_init(v)               → vec_##type##_iter
 * - vec_##type##_iter_next(it, out)         → bool
 *
 * Slice:
 * - vec_##type##_slice_init(v, start, end)  → vec_##type##_slice
 * - vec_##type##_slice_get(s, i)            → type*
 *
 * Internal (always emitted; bodies empty without CANON_LIFETIME_DEBUG):
 * - vec_##type##_lifetime_open_(v)          → void
 * - vec_##type##_lifetime_close_(v)         → void
 *
 * @param linkage C linkage specifier: `static inline`, `static`, or empty
 * @param type    Element type (must be a valid C identifier)
 *
 * @note For pointer types, typedef first:
 *       typedef int* int_ptr;
 *       DEFINE_VEC(static inline, int_ptr)
 *
 * @note option_##type must be defined before calling DEFINE_VEC.
 *       Either include semantics/option.h and call CANON_OPTION(type),
 *       or ensure the Option type is already instantiated.
 */
#define DEFINE_VEC(linkage, type) \
\
IMPL_VEC_STRUCTS( \
    MANGLE_VEC_TYPE(type), \
    MANGLE_VEC_STRUCT_TAG(type), \
    MANGLE_VEC_ITER_TYPE(type), \
    MANGLE_VEC_ITER_STRUCT_TAG(type), \
    MANGLE_VEC_SLICE_TYPE(type), \
    MANGLE_VEC_SLICE_STRUCT_TAG(type), \
    MANGLE_VEC_LIFETIME_OPEN(type), \
    MANGLE_VEC_LIFETIME_CLOSE(type), \
    type \
) \
\
IMPL_VEC_INIT(linkage,        MANGLE_VEC_TYPE(type), MANGLE_VEC_INIT(type),  MANGLE_VEC_LIFETIME_OPEN(type), type) \
IMPL_VEC_EMPTY(linkage,       MANGLE_VEC_TYPE(type), MANGLE_VEC_EMPTY(type), MANGLE_VEC_LIFETIME_OPEN(type)) \
IMPL_VEC_ALLOC(linkage,       MANGLE_VEC_TYPE(type), MANGLE_VEC_ALLOC(type), MANGLE_VEC_EMPTY(type), MANGLE_VEC_INIT(type), type) \
IMPL_VEC_ARENA_ALLOC(linkage, MANGLE_VEC_TYPE(type), MANGLE_VEC_ARENA_ALLOC(type), MANGLE_VEC_EMPTY(type), MANGLE_VEC_INIT(type), type) \
IMPL_VEC_FREE(linkage,        MANGLE_VEC_TYPE(type), MANGLE_VEC_FREE(type), MANGLE_VEC_LIFETIME_CLOSE(type)) \
\
IMPL_VEC_LEN(linkage,       MANGLE_VEC_TYPE(type), MANGLE_VEC_LEN(type)) \
IMPL_VEC_CAPACITY(linkage,  MANGLE_VEC_TYPE(type), MANGLE_VEC_CAPACITY(type)) \
IMPL_VEC_REMAINING(linkage, MANGLE_VEC_TYPE(type), MANGLE_VEC_REMAINING(type)) \
IMPL_VEC_IS_EMPTY(linkage,  MANGLE_VEC_TYPE(type), MANGLE_VEC_IS_EMPTY(type)) \
IMPL_VEC_IS_FULL(linkage,   MANGLE_VEC_TYPE(type), MANGLE_VEC_IS_FULL(type)) \
\
IMPL_VEC_GET(linkage,           MANGLE_VEC_TYPE(type), MANGLE_VEC_GET(type),           type) \
IMPL_VEC_GET_OPTION(linkage,    MANGLE_VEC_TYPE(type), MANGLE_VEC_GET_OPTION(type),    MANGLE_VEC_OPTION_TYPE(type), MANGLE_VEC_OPTION_SOME(type), MANGLE_VEC_OPTION_NONE(type), type) \
IMPL_VEC_GET_UNCHECKED(linkage, MANGLE_VEC_TYPE(type), MANGLE_VEC_GET_UNCHECKED(type), type) \
IMPL_VEC_AT(linkage,            MANGLE_VEC_TYPE(type), MANGLE_VEC_AT(type),            type) \
IMPL_VEC_SET(linkage,           MANGLE_VEC_TYPE(type), MANGLE_VEC_SET(type),           type) \
IMPL_VEC_FIRST(linkage,         MANGLE_VEC_TYPE(type), MANGLE_VEC_FIRST(type),         type) \
IMPL_VEC_LAST(linkage,          MANGLE_VEC_TYPE(type), MANGLE_VEC_LAST(type),          type) \
IMPL_VEC_DATA(linkage,          MANGLE_VEC_TYPE(type), MANGLE_VEC_DATA(type),          type) \
\
IMPL_VEC_PUSH(linkage,           MANGLE_VEC_TYPE(type), MANGLE_VEC_PUSH(type),           type) \
IMPL_VEC_TRY_PUSH(linkage,       MANGLE_VEC_TYPE(type), MANGLE_VEC_TRY_PUSH(type),       type) \
IMPL_VEC_PUSH_UNCHECKED(linkage, MANGLE_VEC_TYPE(type), MANGLE_VEC_PUSH_UNCHECKED(type), type) \
IMPL_VEC_POP(linkage,            MANGLE_VEC_TYPE(type), MANGLE_VEC_POP(type),            type) \
IMPL_VEC_POP_OPTION(linkage,     MANGLE_VEC_TYPE(type), MANGLE_VEC_POP_OPTION(type),     MANGLE_VEC_POP(type), MANGLE_VEC_OPTION_TYPE(type), MANGLE_VEC_OPTION_SOME(type), MANGLE_VEC_OPTION_NONE(type), MANGLE_VEC_RESULT_IS_OK(type), type) \
IMPL_VEC_CLEAR(linkage,          MANGLE_VEC_TYPE(type), MANGLE_VEC_CLEAR(type)) \
IMPL_VEC_INSERT(linkage,         MANGLE_VEC_TYPE(type), MANGLE_VEC_INSERT(type),         type) \
IMPL_VEC_REMOVE(linkage,         MANGLE_VEC_TYPE(type), MANGLE_VEC_REMOVE(type),         type) \
IMPL_VEC_REMOVE_OPTION(linkage,  MANGLE_VEC_TYPE(type), MANGLE_VEC_REMOVE_OPTION(type),  MANGLE_VEC_REMOVE(type), MANGLE_VEC_OPTION_TYPE(type), MANGLE_VEC_OPTION_SOME(type), MANGLE_VEC_OPTION_NONE(type), MANGLE_VEC_RESULT_IS_OK(type), type) \
\
IMPL_VEC_APPEND_ARRAY(linkage, MANGLE_VEC_TYPE(type), MANGLE_VEC_APPEND_ARRAY(type), type) \
IMPL_VEC_EXTEND(linkage,       MANGLE_VEC_TYPE(type), MANGLE_VEC_EXTEND(type),       MANGLE_VEC_APPEND_ARRAY(type), type) \
IMPL_VEC_FILL(linkage,         MANGLE_VEC_TYPE(type), MANGLE_VEC_FILL(type),         type) \
IMPL_VEC_SWAP(linkage,         MANGLE_VEC_TYPE(type), MANGLE_VEC_SWAP(type)) \
\
IMPL_VEC_ITER_INIT(linkage, MANGLE_VEC_ITER_TYPE(type),  MANGLE_VEC_ITER_INIT(type), MANGLE_VEC_TYPE(type)) \
IMPL_VEC_ITER_NEXT(linkage, MANGLE_VEC_ITER_TYPE(type),  MANGLE_VEC_ITER_NEXT(type), type) \
\
IMPL_VEC_SLICE_INIT(linkage, MANGLE_VEC_SLICE_TYPE(type), MANGLE_VEC_SLICE_INIT(type), MANGLE_VEC_TYPE(type), type) \
IMPL_VEC_SLICE_GET(linkage,  MANGLE_VEC_SLICE_TYPE(type), MANGLE_VEC_SLICE_GET(type),  type)

#endif /* CANON_DATA_VEC_DEFN_H */
