#ifndef CANON_DATA_DEQUE_DEFN_H
#define CANON_DATA_DEQUE_DEFN_H

#include "data/deque/deque_mangle.h"
#include "data/deque/deque_impl.h"

/**
 * @file deque_defn.h
 * @brief Function definitions for Canon-C typed deques
 *
 * Use DEFINE_DEQUE(linkage, type) to emit all struct typedefs and function
 * definitions for a typed deque. The linkage parameter controls visibility.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Emits complete function bodies via deque_impl.h macros
 * - Linkage is caller-controlled: static inline, static, or extern
 * - For header-only use, pass `static inline`
 * - For separate compilation, pass empty linkage + use deque_decl.h
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 *
 * Header-only usage:
 * ```c
 * #include "data/deque/deque_defn.h"
 * DEFINE_DEQUE(static inline, int)
 *
 * int buf[64];
 * canon_deque_int d;
 * canon_deque_int_init(&d, buf, 64);
 * canon_deque_int_push_back(&d, 42);
 * ```
 *
 * Separate compilation:
 * ```c
 * // In tasks.h:
 * #include "data/deque/deque_decl.h"
 * DECLARE_DEQUE(Task)
 *
 * // In tasks.c:
 * #include "data/deque/deque_defn.h"
 * DEFINE_DEQUE(, Task)
 * ```
 *
 * Pointer types (typedef first):
 * ```c
 * typedef void* voidptr;
 * DEFINE_DEQUE(static inline, voidptr)
 * ```
 *
 * @sa deque_decl.h, deque_mangle.h, deque_impl.h, deque.h
 */

/* ════════════════════════════════════════════════════════════════════════════
   DEFINE_DEQUE — emit all typedefs and function definitions for a typed deque
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Instantiates a complete typed deque for element type `type`
 *
 * Generated type (using default mangle):
 * - canon_deque_##type
 *
 * Generated functions (using default mangle):
 *
 * Constructor:
 * - canon_deque_##type##_init(d, buffer, capacity)   → void
 * - canon_deque_##type##_empty()                     → canon_deque_##type
 *
 * Queries:
 * - canon_deque_##type##_len(d)                      → usize
 * - canon_deque_##type##_capacity(d)                 → usize
 * - canon_deque_##type##_remaining(d)                → usize
 * - canon_deque_##type##_is_empty(d)                 → bool
 * - canon_deque_##type##_is_full(d)                  → bool
 *
 * Push:
 * - canon_deque_##type##_push_front(d, item)         → result_bool_Error
 * - canon_deque_##type##_push_back(d, item)          → result_bool_Error
 *
 * Pop (Result variants):
 * - canon_deque_##type##_pop_front(d, out)           → result_bool_Error
 * - canon_deque_##type##_pop_back(d, out)            → result_bool_Error
 *
 * Pop (Option variants):
 * - canon_deque_##type##_pop_front_option(d)         → option_##type
 * - canon_deque_##type##_pop_back_option(d)          → option_##type
 *
 * Peek (bool variants):
 * - canon_deque_##type##_peek_front(d, out)          → bool
 * - canon_deque_##type##_peek_back(d, out)           → bool
 *
 * Peek (Option variants):
 * - canon_deque_##type##_peek_front_option(d)        → option_##type
 * - canon_deque_##type##_peek_back_option(d)         → option_##type
 *
 * Misc:
 * - canon_deque_##type##_clear(d)                    → void
 * - canon_deque_##type##_swap(a, b)                  → void
 *
 * @param linkage C linkage specifier: `static inline`, `static`, or empty
 * @param type    Element type (must be a valid C identifier)
 *
 * @note option_##type must be defined before calling DEFINE_DEQUE.
 *       Either include semantics/option.h and call CANON_OPTION(type, Error),
 *       or ensure the Option type is already instantiated.
 *
 * @note For pointer types, typedef first:
 *       typedef void* voidptr;
 *       DEFINE_DEQUE(static inline, voidptr)
 */
#define DEFINE_DEQUE(linkage, type) \
\
IMPL_DEQUE_STRUCT( \
    MANGLE_DEQUE_TYPE(type), \
    MANGLE_DEQUE_STRUCT_TAG(type), \
    type \
) \
\
IMPL_DEQUE_INIT(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_INIT(type),  type) \
IMPL_DEQUE_EMPTY(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_EMPTY(type)) \
\
IMPL_DEQUE_LEN(linkage,       MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_LEN(type)) \
IMPL_DEQUE_CAPACITY(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_CAPACITY(type)) \
IMPL_DEQUE_REMAINING(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_REMAINING(type)) \
IMPL_DEQUE_IS_EMPTY(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_IS_EMPTY(type)) \
IMPL_DEQUE_IS_FULL(linkage,   MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_IS_FULL(type)) \
\
IMPL_DEQUE_PUSH_FRONT(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PUSH_FRONT(type), type) \
IMPL_DEQUE_PUSH_BACK(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PUSH_BACK(type),  type) \
\
IMPL_DEQUE_POP_FRONT(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_POP_FRONT(type), type) \
IMPL_DEQUE_POP_BACK(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_POP_BACK(type),  type) \
\
IMPL_DEQUE_POP_FRONT_OPTION(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_POP_FRONT_OPTION(type), MANGLE_DEQUE_POP_FRONT(type), option_##type, option_##type##_some, option_##type##_none, type) \
IMPL_DEQUE_POP_BACK_OPTION(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_POP_BACK_OPTION(type),  MANGLE_DEQUE_POP_BACK(type),  option_##type, option_##type##_some, option_##type##_none, type) \
\
IMPL_DEQUE_PEEK_FRONT(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PEEK_FRONT(type), type) \
IMPL_DEQUE_PEEK_BACK(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PEEK_BACK(type),  type) \
\
IMPL_DEQUE_PEEK_FRONT_OPTION(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PEEK_FRONT_OPTION(type), MANGLE_DEQUE_PEEK_FRONT(type), option_##type, option_##type##_some, option_##type##_none, type) \
IMPL_DEQUE_PEEK_BACK_OPTION(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PEEK_BACK_OPTION(type),  MANGLE_DEQUE_PEEK_BACK(type),  option_##type, option_##type##_some, option_##type##_none, type) \
\
IMPL_DEQUE_CLEAR(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_CLEAR(type)) \
IMPL_DEQUE_SWAP(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_SWAP(type))

#endif /* CANON_DATA_DEQUE_DEFN_H */
