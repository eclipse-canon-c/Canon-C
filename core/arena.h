// core/arena.h
#ifndef CANON_CORE_ARENA_H
#define CANON_CORE_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "memory.h"

/**
 * @file arena.h
 * @brief Linear bump allocator with explicit lifetime control
 *
 * Provides a very fast, simple, predictable arena (bump-pointer) allocator
 * that allocates sequentially from a fixed, caller-owned buffer.
 *
 * No individual deallocation is supported — memory is released only via
 * full reset or rollback to a previous mark.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Extremely fast: pointer bump + minimal alignment padding
 * - Zero hidden allocations or metadata overhead
 * - Caller completely controls memory lifetime and buffer
 * - Cache-friendly: all allocations are contiguous
 * - Explicit lifetime: reset or rollback required to reuse memory
 * - Debug-friendly: many assertions in debug builds
 * - Two reset modes: fast (just offset=0) and secure (memset 0)
 * - Checkpoint/rollback pattern via ArenaMark
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - Time complexity: O(1) for alloc (amortized), O(n) for secure reset
 * - Space complexity: O(1) metadata (~24 bytes per arena)
 * - No per-allocation overhead (no headers/footers)
 * - Alignment padding is the only "waste"
 * - Typical alloc: <10 CPU cycles (just add + align)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * NOT thread-safe by design.
 * No internal locking — caller must synchronize access when used
 * from multiple threads (mutex, per-thread arenas, etc.).
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Relies on: uintptr_t, inline functions, assert()
 * - Optional GNU statement expressions for *_zero macros
 *   (disabled with #define CANON_NO_GNU_EXTENSIONS)
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Per-frame / per-request temporary allocations
 * - Parser / lexer scratch memory
 * - AST / DOM / intermediate representation nodes
 * - Command buffers, transaction scratchpads
 * - Scoped temporary strings / buffers
 * - Game entity component temporary data
 * - Any workload with clear "begin → work → reset" phases
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Long-lived objects that outlive the arena
 * - General-purpose allocation (use malloc / pool instead)
 * - Very large individual allocations (> remaining space)
 * - Objects that require individual deallocation
 * - Multi-threaded allocation without external synchronization
 *
 * @sa arena_alloc(), arena_alloc_aligned(), arena_reset_to(), arena_mark()
 */

/**
 * @brief Arena instance — holds buffer, capacity and current position
 */
typedef struct Arena {
    uint8_t* buffer;   ///< Start of the memory block (caller-owned)
    size_t   capacity; ///< Total usable bytes in buffer
    size_t   offset;   ///< Current bump pointer (next free byte)
} Arena;

/**
 * @brief Opaque checkpoint type for partial rollback
 * @sa arena_mark(), arena_reset_to()
 */
typedef size_t ArenaMark;

/* ────────────────────────────────────────────────────────────────────────────
   Initialization & State Management
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Initializes an arena using caller-provided memory
 *
 * @param arena    Valid pointer to uninitialized Arena struct
 * @param buffer   Pointer to memory block (must remain valid)
 * @param capacity Size of buffer in bytes (> 0)
 *
 * @pre  arena != NULL
 * @pre  buffer != NULL
 * @pre  capacity > 0
 * @pre  buffer points to at least capacity bytes
 *
 * @post arena->buffer   == buffer
 * @post arena->capacity == capacity
 * @post arena->offset   == 0
 *
 * @note In debug builds, preconditions are asserted.
 *       In release builds, invalid inputs leave arena unusable.
 *
 * @sa arena_reset()
 */
static inline void arena_init(Arena* arena, void* buffer, size_t capacity) {
    assert(arena != NULL   && "arena_init: arena cannot be NULL");
    assert(buffer != NULL  && "arena_init: buffer cannot be NULL");
    assert(capacity > 0    && "arena_init: capacity must be > 0");

    if (!arena || !buffer || capacity == 0) return;

    arena->buffer   = (uint8_t*)buffer;
    arena->capacity = capacity;
    arena->offset   = 0;
}

/**
 * @brief Resets arena to empty state (fast path)
 *
 * Invalidates all previous allocations by moving offset back to 0.
 * Memory content is **not** cleared.
 *
 * @param arena Arena to reset (NULL-safe)
 *
 * @sa arena_reset_secure(), arena_reset_to()
 */
static inline void arena_reset(Arena* arena) {
    if (arena) {
        arena->offset = 0;
    }
}

/**
 * @brief Resets arena and securely wipes all previously allocated memory
 *
 * Use when arena contained sensitive data (keys, passwords, tokens, etc.).
 *
 * @param arena Arena to reset (NULL-safe)
 *
 * @remark Time complexity: O(used bytes)
 * @sa arena_reset()
 */
static inline void arena_reset_secure(Arena* arena) {
    if (arena && arena->offset > 0) {
        memset(arena->buffer, 0, arena->offset);
        arena->offset = 0;
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Allocation Functions
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocates size bytes with natural alignment
 *
 * Natural alignment is typically:
 *   - 8 bytes on 64-bit platforms
 *   - 4 bytes on 32-bit platforms
 *
 * @param arena Valid initialized arena
 * @param size  Bytes to allocate (> 0)
 *
 * @return Pointer to aligned memory block, or NULL if:
 *         - arena == NULL
 *         - size == 0
 *         - insufficient remaining space (after alignment padding)
 *
 * @pre arena is initialized via arena_init()
 *
 * @note Alignment padding is inserted if needed.
 *       No per-allocation metadata is stored.
 *
 * @sa arena_alloc_aligned(), arena_alloc_type()
 */
static inline void* arena_alloc(Arena* arena, size_t size) {
    assert(arena != NULL && "arena_alloc: arena cannot be NULL");

    if (!arena || size == 0) return NULL;

    size_t aligned = mem_align(size);
    if (aligned < size) return NULL; // size_t overflow (very unlikely)

    if (arena->offset + aligned > arena->capacity) {
        return NULL;
    }

    void* ptr = arena->buffer + arena->offset;
    arena->offset += aligned;
    return ptr;
}

/**
 * @brief Allocates size bytes with requested alignment
 *
 * @param arena     Valid initialized arena
 * @param size      Bytes to allocate (> 0)
 * @param alignment Power-of-2 alignment requirement (1,2,4,8,16,…)
 *
 * @return Aligned pointer or NULL if:
 *         - arena == NULL
 *         - size == 0
 *         - alignment == 0 or not power of 2
 *         - insufficient space (including padding)
 *
 * @pre alignment is power of 2 (checked in debug builds)
 *
 * @remark Inserts padding bytes before allocation if current offset
 *         is not already suitably aligned.
 *
 * Example:
 * ```c
 * float* vec = arena_alloc_aligned(&arena, sizeof(float)*4, 16);
 * ```
 */
static inline void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment) {
    assert(arena != NULL && "arena_alloc_aligned: arena cannot be NULL");
    assert((alignment & (alignment-1)) == 0 && "alignment must be power of 2");

    if (!arena || size == 0 || alignment == 0 || (alignment & (alignment-1)) != 0) {
        return NULL;
    }

    uintptr_t curr = (uintptr_t)(arena->buffer + arena->offset);
    uintptr_t next = (curr + alignment - 1) & ~(alignment - 1);
    size_t    pad  = (size_t)(next - curr);

    // Overflow checks (defensive)
    if (arena->offset + pad < arena->offset ||
        arena->offset + pad + size < arena->offset + pad ||
        arena->offset + pad + size > arena->capacity) {
        return NULL;
    }

    arena->offset += pad;
    void* ptr = arena->buffer + arena->offset;
    arena->offset += size;
    return ptr;
}

/* ────────────────────────────────────────────────────────────────────────────
   Query & Checkpoint Functions
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns number of bytes still available for allocation
 * @param arena Arena to query (NULL → 0)
 * @return Remaining capacity in bytes
 */
static inline size_t arena_remaining(const Arena* arena) {
    return arena ? (arena->capacity - arena->offset) : 0;
}

/**
 * @brief Returns number of bytes currently allocated
 * @param arena Arena to query (NULL → 0)
 * @return Used bytes
 */
static inline size_t arena_used(const Arena* arena) {
    return arena ? arena->offset : 0;
}

/**
 * @brief Checks whether arena has no active allocations
 * @param arena Arena to check (NULL → true)
 * @return true if offset == 0
 */
static inline int arena_is_empty(const Arena* arena) {
    return !arena || arena->offset == 0;
}

/**
 * @brief Checks whether arena has no remaining space
 * @param arena Arena to check (NULL → true)
 * @return true if offset >= capacity
 */
static inline int arena_is_full(const Arena* arena) {
    return !arena || arena->offset >= arena->capacity;
}

/**
 * @brief Creates a checkpoint of current allocation position
 *
 * Pair with arena_reset_to() for scoped temporary allocations.
 *
 * @param arena Arena to mark (NULL → 0)
 * @return Opaque mark value (current offset)
 *
 * @sa arena_reset_to()
 */
static inline ArenaMark arena_mark(const Arena* arena) {
    return arena ? arena->offset : 0;
}

/**
 * @brief Rolls back arena state to a previous mark
 *
 * Invalidates all allocations made after the mark.
 *
 * @param arena Arena to roll back
 * @param mark  Value previously returned by arena_mark()
 *
 * @pre mark <= current offset (enforced in debug builds)
 *
 * @sa arena_mark()
 */
static inline void arena_reset_to(Arena* arena, ArenaMark mark) {
    if (!arena) return;

    assert(mark <= arena->offset && "arena_reset_to: mark is in the future");

    if (mark <= arena->offset) {
        arena->offset = mark;
    }
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience Typed Allocation Helpers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Allocate one object of type Type
 * @hideinitializer
 */
#define arena_alloc_type(arena, Type) \
    ((Type*)arena_alloc((arena), sizeof(Type)))

/**
 * @brief Allocate array of count objects of type Type
 * @hideinitializer
 */
#define arena_alloc_array(arena, Type, count) \
    ((Type*)arena_alloc((arena), sizeof(Type) * (count)))

/* ────────────────────────────────────────────────────────────────────────────
   Zero-initializing variants (GNU C or standard fallback)
   ──────────────────────────────────────────────────────────────────────────── */

#ifndef CANON_NO_GNU_EXTENSIONS
#  define ARENA_ZERO_GNU 1
#else
#  define ARENA_ZERO_GNU 0
#endif

#if ARENA_ZERO_GNU

/**
 * @brief Allocate and zero one object (GNU statement expr version)
 * @warning Requires GNU C extensions
 */
#define arena_alloc_type_zero(arena, Type) \
    ({ Type* _p = arena_alloc_type((arena), Type); \
       if (_p) memset(_p, 0, sizeof(Type)); \
       _p; })

/**
 * @brief Allocate and zero count objects (GNU statement expr version)
 * @warning Requires GNU C extensions
 */
#define arena_alloc_array_zero(arena, Type, count) \
    ({ Type* _p = arena_alloc_array((arena), Type, (count)); \
       if (_p) memset(_p, 0, sizeof(Type)*(count)); \
       _p; })

#else

/**
 * @brief Allocate and zero one object (portable version)
 * @remark Fixed: evaluates arena only once
 */
#define arena_alloc_type_zero(arena, Type) \
    ({ void* _tmp = arena_alloc_type((arena), Type); \
       if (_tmp) memset(_tmp, 0, sizeof(Type)); \
       (Type*)_tmp; })

/**
 * @brief Allocate and zero count objects (portable version)
 * @remark Fixed: evaluates arena and count only once
 */
#define arena_alloc_array_zero(arena, Type, count) \
    ({ size_t _count = (count); \
       void* _tmp = arena_alloc_array((arena), Type, _count); \
       if (_tmp) memset(_tmp, 0, sizeof(Type)*_count); \
       (Type*)_tmp; })

#endif

/* ────────────────────────────────────────────────────────────────────────────
   Example Usage (documentation only)
   ────────────────────────────────────────────────────────────────────────────

#include "core/arena.h"
#include <stdio.h>

void example_usage(void) {
    alignas(64) uint8_t mem[64 * 1024];
    Arena a;
    arena_init(&a, mem, sizeof(mem));

    // Normal allocation
    int* scores = arena_alloc_array(&a, int, 256);

    // Scoped temporary block
    ArenaMark mark = arena_mark(&a);
    char* path = arena_alloc_array(&a, char, MAX_PATH);
    // ... use path ...
    arena_reset_to(&a, mark);           // path invalid now

    // Zeroed struct
    typedef struct { float x, y, z; } Vec3;
    Vec3* origin = arena_alloc_type_zero(&a, Vec3);

    // Aligned SIMD-friendly buffer
    float* points = arena_alloc_aligned(&a, sizeof(float)*4*1000, 32);

    printf("Used: %zu / %zu bytes\n", arena_used(&a), a.capacity);

    arena_reset(&a);                    // everything invalid
}

──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_CORE_ARENA_H */
