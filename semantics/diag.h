#ifndef CANON_SEMANTICS_DIAG_H
#define CANON_SEMANTICS_DIAG_H

#include "core/primitives/types.h"      // usize, bool
#include "core/primitives/contract.h"   // require_msg, ensure_msg
#include "core/slice.h"                 // str_t
#include "semantics/error.h"            // Error enum

/**
 * @file semantics/diag.h
 * @brief Structured diagnostic frames — context chain for error propagation
 *
 * Complements error.h (what failed) and result.h (propagation) by answering "why and where".
 * Diag chains are stack-allocated, fixed-depth, allocation-free context records.
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Stack-allocated chain of frames (no heap ever)
 * - Each frame: file/line/func + error code + short message
 * - Pushed explicitly as errors propagate up
 * - Fixed max depth (DIAG_MAX_FRAMES, default 8)
 * - Messages are inline char arrays (DIAG_MAX_MSG_LEN, default 128)
 * - Chain reads bottom-up: frame[0] = root cause, last = surface error
 *
 * Design philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * - NOT exceptions — no unwinding, no longjmp, no auto-propagation
 * - Explicit push only — caller decides when to annotate
 * - NOT logging — structured data, caller decides rendering
 * - Zero allocation — safe in allocators, kernels, embedded
 * - Brief messages — add detailed context at call sites
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * diag.h lives in semantics/ and may include core/primitives/, core/slice.h,
 * and semantics/error.h only.
 * No data/stringbuf.h — rendering is in diag_render.h (optional).
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - diag_push: O(1) normal, O(DIAG_MAX_FRAMES) on overflow (rare memmove)
 * - diag_clear: O(1)
 * - sizeof(Diag) ≈ 8 frames × ~80 bytes ≈ 640 bytes (stack-safe)
 *
 * Quick start:
 * ```c
 * Diag diag = diag_init();
 * if (!parse_int(str, &val, &diag)) {
 *     DIAG_PUSH(&diag, ERR_PARSE_FAILED, "invalid number format");
 *     diag_print(&diag, stderr);
 *     return false;
 * }
 * ```
 */
/* ════════════════════════════════════════════════════════════════════════════
   Configuration
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Maximum number of frames in a Diag chain
 *
 * Default 8 is sufficient for most real-world call chains.
 * Increase for deep propagation paths (but keep stack usage reasonable).
 */
#ifndef DIAG_MAX_FRAMES
    #define DIAG_MAX_FRAMES ((usize)8)
#endif

/**
 * @brief Maximum length of inline message in a DiagFrame
 *
 * Longer messages are truncated. Sized for stack efficiency.
 */
#ifndef DIAG_MAX_MSG_LEN
    #define DIAG_MAX_MSG_LEN ((usize)128)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   DiagFrame — single context record
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Single frame in diagnostic chain
 *
 * Captures source location + error + short context message.
 * file/func are string literals (no copy, no free).
 * message is inline char array (null-terminated).
 */
typedef struct {
    const char* file;               ///< __FILE__ (literal)
    usize       line;               ///< __LINE__
    const char* func;               ///< __func__ (literal)
    Error       code;               ///< Error code at this level
    char        message[DIAG_MAX_MSG_LEN]; ///< Inline context message
} DiagFrame;

/* ════════════════════════════════════════════════════════════════════════════
   Diag — the chain itself
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Stack-allocated diagnostic chain
 *
 * Fixed-size array of frames.
 * depth == 0 means no error / empty chain.
 * Passed by pointer — NULL is always safe (push is skipped).
 */
typedef struct {
    DiagFrame frames[DIAG_MAX_FRAMES];
    usize     depth;                ///< Current number of frames (0..DIAG_MAX_FRAMES)
} Diag;

/* ════════════════════════════════════════════════════════════════════════════
   Construction & reset
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Creates an empty diagnostic chain
 *
 * @return Zero-initialized Diag (depth == 0)
 */
static inline Diag diag_init(void) {
    Diag d = {0};                   // zeros depth and all frames
    return d;
}

/**
 * @brief Clears the chain (sets depth = 0)
 *
 * Does NOT zero frame contents (fast, safe).
 * @param d Diag pointer (NULL-safe)
 */
static inline void diag_clear(Diag* d) {
    if (d) d->depth = 0;
}

/* ════════════════════════════════════════════════════════════════════════════
   Pushing frames
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Pushes a new diagnostic frame
 *
 * If d == NULL → no-op (safe).
 * If chain full → drops oldest frame (shifts), keeps newest context.
 *
 * @param d     Diag chain (NULL-safe)
 * @param file  __FILE__
 * @param line  __LINE__
 * @param func  __func__
 * @param code  Error code
 * @param msg   Short message (copied inline, truncated if too long)
 */
static inline void diag_push(Diag* d,
                             const char* file,
                             usize line,
                             const char* func,
                             Error code,
                             const char* msg) {
    if (!d) return;

    if (d->depth == DIAG_MAX_FRAMES) {
        // Chain full → drop oldest frame, shift down
        memmove(&d->frames[0], &d->frames[1],
                (DIAG_MAX_FRAMES - 1) * sizeof(DiagFrame));
        d->depth--;
    }

    DiagFrame* f = &d->frames[d->depth++];
    f->file = file;
    f->line = line;
    f->func = func;
    f->code = code;

    if (msg) {
        strncpy(f->message, msg, DIAG_MAX_MSG_LEN - 1);
        f->message[DIAG_MAX_MSG_LEN - 1] = '\0';
    } else {
        f->message[0] = '\0';
    }
}

/**
 * @def DIAG_PUSH(diag, code, msg)
 * @brief Pushes frame with automatic __FILE__/__LINE__/__func__
 *
 * Primary macro for adding context.
 * @param diag Diag* (NULL-safe)
 * @param code Error code
 * @param msg  String literal or const char*
 */
#define DIAG_PUSH(diag, code, msg) \
    diag_push((diag), __FILE__, (usize)__LINE__, __func__, (code), (msg))

/**
 * @def DIAG_PUSH_FMT(diag, code, buf, buf_size, fmt, ...)
 * @brief Pushes frame with formatted message (stack buffer only)
 *
 * Uses snprintf into caller-provided buffer, then copies inline.
 */
#define DIAG_PUSH_FMT(diag, code, buf, buf_size, fmt, ...) \
    do { \
        snprintf((buf), (buf_size), (fmt), ##__VA_ARGS__); \
        DIAG_PUSH((diag), (code), (buf)); \
    } while (0)

/* ════════════════════════════════════════════════════════════════════════════
   Inspection
   ════════════════════════════════════════════════════════════════════════════ */
/** @brief Number of frames in chain (NULL-safe) */
static inline usize diag_depth(const Diag* d) {
    return d ? d->depth : 0;
}

/** @brief True if chain contains any frames */
static inline bool diag_has_error(const Diag* d) {
    return d && d->depth > 0;
}

/** @brief Frame at index i (0 = root cause) */
static inline const DiagFrame* diag_frame_at(const Diag* d, usize i) {
    if (!d || i >= d->depth) return NULL;
    return &d->frames[i];
}

/** @brief Root cause frame (deepest) */
static inline const DiagFrame* diag_root(const Diag* d) {
    return diag_frame_at(d, 0);
}

/** @brief Most recent frame (surface error) */
static inline const DiagFrame* diag_latest(const Diag* d) {
    if (!d || d->depth == 0) return NULL;
    return &d->frames[d->depth - 1];
}

/** @brief Root cause error code (ERR_OK if empty) */
static inline Error diag_root_code(const Diag* d) {
    const DiagFrame* f = diag_root(d);
    return f ? f->code : ERR_OK;
}

/** @brief Latest error code (ERR_OK if empty) */
static inline Error diag_latest_code(const Diag* d) {
    const DiagFrame* f = diag_latest(d);
    return f ? f->code : ERR_OK;
}

/* ════════════════════════════════════════════════════════════════════════════
   Output (basic printing)
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @brief Prints full chain to FILE* (root → latest)
 *
 * Format: [0] file.c:42 in func() — error N: "message"
 */
static inline void diag_print(const Diag* d, FILE* stream) {
    if (!d || !stream || d->depth == 0) return;

    for (usize i = 0; i < d->depth; i++) {
        const DiagFrame* f = &d->frames[i];
        fprintf(stream,
                "[%zu] %s:%zu in %s() — error %d: \"%s\"\n",
                i,
                f->file ? f->file : "?",
                f->line,
                f->func ? f->func : "?",
                (int)f->code,
                f->message[0] ? f->message : "(no message)");
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Propagation helpers
   ════════════════════════════════════════════════════════════════════════════ */
/**
 * @def DIAG_RETURN_IF(cond, diag, code, msg, retval)
 * @brief Push frame and return if condition true
 */
#define DIAG_RETURN_IF(cond, diag, code, msg, retval) \
    do { \
        if (cond) { \
            DIAG_PUSH((diag), (code), (msg)); \
            return (retval); \
        } \
    } while (0)

/**
 * @def DIAG_PROPAGATE(call, diag, code, msg, retval)
 * @brief Call expression; if false → push context and return
 */
#define DIAG_PROPAGATE(call, diag, code, msg, retval) \
    do { \
        if (!(call)) { \
            DIAG_PUSH((diag), (code), (msg)); \
            return (retval); \
        } \
    } while (0)

#endif /* CANON_SEMANTICS_DIAG_H */
