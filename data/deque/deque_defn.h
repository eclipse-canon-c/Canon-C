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
 *
 * // Option must be instantiated before DEFINE_DEQUE
 * CANON_OPTION(int)
 * DEFINE_DEQUE(static inline, int)
 *
 * int buf[64];
 * deque_int d;
 * deque_int_init(&d, buf, 64);
 * deque_int_push_back(&d, 42);
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
 * CANON_OPTION(Task)
 * DEFINE_DEQUE(, Task)
 * ```
 *
 * Pointer types (typedef first):
 * ```c
 * typedef void* voidptr;
 * CANON_OPTION(voidptr)
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
 * - deque_##type
 *
 * Generated functions (using default mangle):
 *
 * Constructor:
 * - deque_##type##_init(d, buffer, capacity)   → void
 * - deque_##type##_empty()                     → deque_##type
 *
 * Queries:
 * - deque_##type##_len(d)                      → usize
 * - deque_##type##_capacity(d)                 → usize
 * - deque_##type##_remaining(d)                → usize
 * - deque_##type##_is_empty(d)                 → bool
 * - deque_##type##_is_full(d)                  → bool
 *
 * Push (Result variants — full diagnostics):
 * - deque_##type##_push_front(d, item)         → result_bool_Error
 * - deque_##type##_push_back(d, item)          → result_bool_Error
 *
 * Push (bool variants — no Result overhead):
 * - deque_##type##_try_push_front(d, item)     → bool
 * - deque_##type##_try_push_back(d, item)      → bool
 *
 * Push (unchecked variants — debug-only assertions):
 * - deque_##type##_push_front_unchecked(d, item) → void
 * - deque_##type##_push_back_unchecked(d, item)  → void
 *
 * Pop (Result variants):
 * - deque_##type##_pop_front(d, out)           → result_bool_Error
 * - deque_##type##_pop_back(d, out)            → result_bool_Error
 *
 * Pop (Option variants):
 * - deque_##type##_pop_front_option(d)         → option_##type
 * - deque_##type##_pop_back_option(d)          → option_##type
 *
 * Peek (bool variants):
 * - deque_##type##_peek_front(d, out)          → bool
 * - deque_##type##_peek_back(d, out)           → bool
 *
 * Peek (Option variants):
 * - deque_##type##_peek_front_option(d)        → option_##type
 * - deque_##type##_peek_back_option(d)         → option_##type
 *
 * Misc:
 * - deque_##type##_clear(d)                    → void
 * - deque_##type##_swap(a, b)                  → void
 *
 * @param linkage C linkage specifier: `static inline`, `static`, or empty
 * @param type    Element type (must be a valid C identifier)
 *
 * @pre CANON_OPTION(type) has already been called for the same type
 *
 * @note For pointer types, typedef first:
 *       typedef void* voidptr;
 *       CANON_OPTION(voidptr)
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
IMPL_DEQUE_TRY_PUSH_FRONT(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_TRY_PUSH_FRONT(type), type) \
IMPL_DEQUE_TRY_PUSH_BACK(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_TRY_PUSH_BACK(type),  type) \
\
IMPL_DEQUE_PUSH_FRONT_UNCHECKED(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PUSH_FRONT_UNCHECKED(type), type) \
IMPL_DEQUE_PUSH_BACK_UNCHECKED(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PUSH_BACK_UNCHECKED(type),  type) \
\
IMPL_DEQUE_POP_FRONT(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_POP_FRONT(type), type) \
IMPL_DEQUE_POP_BACK(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_POP_BACK(type),  type) \
\
IMPL_DEQUE_POP_FRONT_OPTION(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_POP_FRONT_OPTION(type), MANGLE_DEQUE_POP_FRONT(type), MANGLE_DEQUE_OPTION_TYPE(type), MANGLE_DEQUE_OPTION_SOME(type), MANGLE_DEQUE_OPTION_NONE(type), MANGLE_DEQUE_RESULT_IS_OK(type), type) \
IMPL_DEQUE_POP_BACK_OPTION(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_POP_BACK_OPTION(type),  MANGLE_DEQUE_POP_BACK(type),  MANGLE_DEQUE_OPTION_TYPE(type), MANGLE_DEQUE_OPTION_SOME(type), MANGLE_DEQUE_OPTION_NONE(type), MANGLE_DEQUE_RESULT_IS_OK(type), type) \
\
IMPL_DEQUE_PEEK_FRONT(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PEEK_FRONT(type), type) \
IMPL_DEQUE_PEEK_BACK(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PEEK_BACK(type),  type) \
\
IMPL_DEQUE_PEEK_FRONT_OPTION(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PEEK_FRONT_OPTION(type), MANGLE_DEQUE_PEEK_FRONT(type), MANGLE_DEQUE_OPTION_TYPE(type), MANGLE_DEQUE_OPTION_SOME(type), MANGLE_DEQUE_OPTION_NONE(type), type) \
IMPL_DEQUE_PEEK_BACK_OPTION(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_PEEK_BACK_OPTION(type),  MANGLE_DEQUE_PEEK_BACK(type),  MANGLE_DEQUE_OPTION_TYPE(type), MANGLE_DEQUE_OPTION_SOME(type), MANGLE_DEQUE_OPTION_NONE(type), type) \
\
IMPL_DEQUE_CLEAR(linkage, MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_CLEAR(type)) \
IMPL_DEQUE_SWAP(linkage,  MANGLE_DEQUE_TYPE(type), MANGLE_DEQUE_SWAP(type))

#endif /* CANON_DATA_DEQUE_DEFN_H */
