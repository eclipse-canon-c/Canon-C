#ifndef CANON_SEMANTICS_DIAG_H
#define CANON_SEMANTICS_DIAG_H

#include "core/primitives/types.h"    /* usize, bool */
#include "core/primitives/contract.h" /* require_msg */
#include "semantics/error.h"          /* Error, ERR_OK */

/* Standard library headers required by this file */
#include <string.h>  /* memmove, memcpy */
#include <stdio.h>   /* FILE, fprintf, snprintf */

/**
 * @file semantics/diag.h
 * @brief Structured diagnostic frames — context chain for error propagation
 *
 * Complements error.h (what failed) and result.h (propagation) by answering
 * "why and where". Diag chains are stack-allocated, fixed-depth,
 * allocation-free context records.
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
 * - Plain C99 — no GNU extensions unless CANON_NO_GNU_EXTENSIONS is NOT defined
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * diag.h lives in semantics/ and may include core/primitives/ and
 * semantics/error.h only. core/slice.h is intentionally excluded — diag.h
 * operates on plain char* and does not deal in slice views.
 * No data/stringbuf.h — rendering is in diag_render.h (optional).
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - diag_push: O(1) normal, O(DIAG_MAX_FRAMES) on overflow (memmove shift)
 * - diag_clear: O(1)
 * - sizeof(Diag) ≈ 8 frames × ~160 bytes ≈ 1280 bytes (stack-safe)
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
 * @brief Maximum number of frames in a Diag chain.
 *
 * Default 8 is sufficient for most real-world call chains.
 * Increase for deep propagation paths (but keep stack usage reasonable).
 * Must be >= 1.
 */
#ifndef DIAG_MAX_FRAMES
    #define DIAG_MAX_FRAMES ((usize)8)
#endif

/**
 * @brief Maximum byte length of the inline message in a DiagFrame,
 *        including the null terminator.
 *
 * Longer messages are silently truncated. Sized for stack efficiency.
 * Must be >= 1.
 */
#ifndef DIAG_MAX_MSG_LEN
    #define DIAG_MAX_MSG_LEN ((usize)128)
#endif

/*
 * Compile-time contract: configuration values must satisfy their documented
 * lower bounds. These fire as hard errors before any code is compiled.
 * require_msg is from core/primitives/contract.h.
 */
require_msg(DIAG_MAX_FRAMES  >= (usize)1,
            "DIAG_MAX_FRAMES must be at least 1");
require_msg(DIAG_MAX_MSG_LEN >= (usize)1,
            "DIAG_MAX_MSG_LEN must be at least 1");

/* ════════════════════════════════════════════════════════════════════════════
   DiagFrame — single context record
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Single frame in a diagnostic chain.
 *
 * Captures source location + error code + short context message.
 * `file` and `func` point to string literals (no copy, no free needed).
 * `message` is a null-terminated inline char array.
 *
 * Never construct directly — use diag_push() or DIAG_PUSH().
 */
typedef struct {
    const char *file;            /**< __FILE__ (string literal, not owned) */
    const char *func;            /**< __func__ (string literal, not owned) */
    usize       line;            /**< __LINE__ */
    Error       code;            /**< Error code at this propagation level */
    char        message[DIAG_MAX_MSG_LEN]; /**< Inline null-terminated message */
} DiagFrame;

/* ════════════════════════════════════════════════════════════════════════════
   Diag — the chain itself
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Stack-allocated diagnostic chain.
 *
 * Fixed-size array of DiagFrame records.
 * `depth == 0` means the chain is empty (no error recorded).
 *
 * Always pass by pointer. NULL is always safe — all functions and macros
 * that accept a `Diag *` treat NULL as a no-op.
 *
 * Do not read `frames` directly; use the inspection helpers below.
 */
typedef struct {
    DiagFrame frames[DIAG_MAX_FRAMES]; /**< Frame storage, [0] = root cause */
    usize     depth;                   /**< Number of valid frames (0..DIAG_MAX_FRAMES) */
} Diag;

/* ════════════════════════════════════════════════════════════════════════════
   Construction & reset
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Creates an empty diagnostic chain.
 *
 * Returns a zero-initialised Diag with depth == 0.
 * Assign the return value directly:
 *
 *   Diag d = diag_init();
 */
static inline Diag diag_init(void)
{
    Diag d;
    memset(&d, 0, sizeof(d));
    return d;
}

/**
 * @brief Resets the chain to empty (sets depth = 0).
 *
 * Does NOT zero frame contents (intentional — O(1), safe).
 * Previously pushed frames become invisible but their memory is reused on
 * the next push.
 *
 * @param d  Diag pointer. NULL-safe: no-op when NULL.
 */
static inline void diag_clear(Diag *d)
{
    if (d != NULL) {
        d->depth = 0;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Pushing frames
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Pushes a new diagnostic frame onto the chain.
 *
 * Overflow behaviour: if the chain is already at DIAG_MAX_FRAMES, the oldest
 * frame (index 0) is discarded and all remaining frames are shifted down by
 * one, preserving the most recent context. The function returns true in this
 * case so the caller can detect information loss if needed.
 *
 * `msg` may be NULL — results in an empty message string.
 * Messages longer than DIAG_MAX_MSG_LEN-1 bytes are silently truncated.
 *
 * Prefer the DIAG_PUSH() macro over calling this function directly, so that
 * __FILE__, __LINE__, and __func__ are captured automatically.
 *
 * @param d     Diag chain. NULL-safe: no-op, returns false.
 * @param file  Source file name (string literal).
 * @param line  Source line number.
 * @param func  Function name (string literal).
 * @param code  Error code for this propagation level.
 * @param msg   Short context message (copied; may be NULL).
 * @return      true if the oldest frame was dropped due to overflow,
 *              false otherwise (including when d is NULL).
 */
static inline bool diag_push(Diag       *d,
                              const char *file,
                              usize       line,
                              const char *func,
                              Error       code,
                              const char *msg)
{
    if (d == NULL) {
        return false;
    }

    /* Internal invariant: depth can never legally exceed DIAG_MAX_FRAMES. */
    require_msg(d->depth <= DIAG_MAX_FRAMES,
                "diag_push: depth exceeded DIAG_MAX_FRAMES — chain is corrupt");

    bool dropped = false;

    if (d->depth == DIAG_MAX_FRAMES) {
        /* Chain full: discard oldest frame, shift remaining frames down. */
        memmove(&d->frames[0],
                &d->frames[1],
                (DIAG_MAX_FRAMES - 1u) * sizeof(DiagFrame));
        d->depth--;
        dropped = true;
    }

    {
        DiagFrame *f = &d->frames[d->depth];
        d->depth++;

        f->file = file;
        f->line = line;
        f->func = func;
        f->code = code;

        if (msg != NULL) {
            /*
             * Use memcpy instead of strncpy to avoid implementation-defined
             * zero-padding behaviour and compiler warnings on MSVC/clang-tidy.
             * We manually find the length, clamp it, copy, and null-terminate.
             */
            usize len = 0;
            while (len < (DIAG_MAX_MSG_LEN - 1u) && msg[len] != '\0') {
                len++;
            }
            memcpy(f->message, msg, len);
            f->message[len] = '\0';
        } else {
            f->message[0] = '\0';
        }
    }

    return dropped;
}

/**
 * @def DIAG_PUSH(diag, code, msg)
 * @brief Push a frame with automatic __FILE__ / __LINE__ / __func__ capture.
 *
 * This is the primary macro for annotating error propagation paths.
 * The return value of diag_push() (overflow indicator) is discarded; if you
 * need it, call diag_push() directly.
 *
 * @param diag  Diag* (NULL-safe).
 * @param code  Error code (Error enum value).
 * @param msg   String literal or const char* message (may be NULL).
 */
#define DIAG_PUSH(diag, code, msg) \
    ((void)diag_push((diag), __FILE__, (usize)(__LINE__), __func__, (code), (msg)))

/**
 * @def DIAG_PUSH_FMT(diag, code, buf, buf_size, fmt, ...)
 * @brief Push a frame with a formatted message using a caller-supplied buffer.
 *
 * Formats the message into `buf` (size `buf_size`) via snprintf, then pushes
 * it. No heap allocation — the caller owns the buffer and it need only live
 * until diag_push copies the message inline.
 *
 * Requirements (caller's responsibility):
 *   - buf  != NULL
 *   - buf_size >= 1
 *   - fmt  != NULL
 *
 * Example:
 *   char tmp[64];
 *   DIAG_PUSH_FMT(&diag, ERR_RANGE, tmp, sizeof(tmp),
 *                 "index %zu out of range [0, %zu)", idx, len);
 *
 * Portability note:
 *   The trailing-comma-before-__VA_ARGS__ form used here (`fmt, ##__VA_ARGS__`
 *   or `fmt __VA_OPT__(,) __VA_ARGS__`) requires either:
 *     - GNU C extension (##__VA_ARGS__) — GCC, Clang, MSVC 2019+, or
 *     - C23 __VA_OPT__                 — C23 conforming compilers.
 *   Define CANON_NO_GNU_EXTENSIONS to exclude this macro entirely on compilers
 *   that support neither. Use a plain snprintf + DIAG_PUSH instead.
 */
#ifndef CANON_NO_GNU_EXTENSIONS
#define DIAG_PUSH_FMT(diag, code, buf, buf_size, fmt, ...) \
    do { \
        require_msg((buf)      != NULL, "DIAG_PUSH_FMT: buf must not be NULL"); \
        require_msg((buf_size) >= 1u,   "DIAG_PUSH_FMT: buf_size must be >= 1"); \
        require_msg((fmt)      != NULL, "DIAG_PUSH_FMT: fmt must not be NULL"); \
        snprintf((buf), (buf_size), (fmt), ##__VA_ARGS__); \
        DIAG_PUSH((diag), (code), (buf)); \
    } while (0)
#endif /* CANON_NO_GNU_EXTENSIONS */

/* ════════════════════════════════════════════════════════════════════════════
   Inspection
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the number of frames currently in the chain.
 *
 * @param d  Diag pointer. NULL-safe: returns 0.
 */
static inline usize diag_depth(const Diag *d)
{
    return (d != NULL) ? d->depth : (usize)0;
}

/**
 * @brief Returns true if the chain contains at least one frame.
 *
 * @param d  Diag pointer. NULL-safe: returns false.
 */
static inline bool diag_has_error(const Diag *d)
{
    return (d != NULL) && (d->depth > 0u);
}

/**
 * @brief Returns a pointer to the frame at index `i` (0 = root cause).
 *
 * @param d  Diag pointer. NULL-safe: returns NULL.
 * @param i  Frame index. Returns NULL if i >= depth.
 * @return   Pointer to DiagFrame, or NULL if out of range.
 */
static inline const DiagFrame *diag_frame_at(const Diag *d, usize i)
{
    if (d == NULL || i >= d->depth) {
        return NULL;
    }
    return &d->frames[i];
}

/**
 * @brief Returns a pointer to the root-cause frame (index 0).
 *
 * @param d  Diag pointer. NULL-safe: returns NULL.
 * @return   Pointer to DiagFrame, or NULL if chain is empty.
 */
static inline const DiagFrame *diag_root(const Diag *d)
{
    return diag_frame_at(d, 0u);
}

/**
 * @brief Returns a pointer to the most recently pushed frame (surface error).
 *
 * @param d  Diag pointer. NULL-safe: returns NULL.
 * @return   Pointer to DiagFrame, or NULL if chain is empty.
 */
static inline const DiagFrame *diag_latest(const Diag *d)
{
    if (d == NULL || d->depth == 0u) {
        return NULL;
    }
    return &d->frames[d->depth - 1u];
}

/**
 * @brief Returns the error code of the root-cause frame.
 *
 * @param d  Diag pointer. NULL-safe: returns ERR_OK.
 * @return   Error code, or ERR_OK if the chain is empty.
 */
static inline Error diag_root_code(const Diag *d)
{
    const DiagFrame *f = diag_root(d);
    return (f != NULL) ? f->code : ERR_OK;
}

/**
 * @brief Returns the error code of the most recently pushed frame.
 *
 * @param d  Diag pointer. NULL-safe: returns ERR_OK.
 * @return   Error code, or ERR_OK if the chain is empty.
 */
static inline Error diag_latest_code(const Diag *d)
{
    const DiagFrame *f = diag_latest(d);
    return (f != NULL) ? f->code : ERR_OK;
}

/* ════════════════════════════════════════════════════════════════════════════
   Output (basic printing)
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Prints the full chain to `stream`, from root cause to latest frame.
 *
 * Output format per frame:
 *   [<index>] <file>:<line> in <func>() — error <code>: "<message>"
 *
 * No trailing newline after the last frame beyond what each line already has.
 *
 * @param d       Diag pointer. NULL-safe: no-op.
 * @param stream  Output FILE*. NULL-safe: no-op.
 */
static inline void diag_print(const Diag *d, FILE *stream)
{
    usize i;

    if (d == NULL || stream == NULL || d->depth == 0u) {
        return;
    }

    for (i = 0u; i < d->depth; i++) {
        const DiagFrame *f = &d->frames[i];
        fprintf(stream,
                "[%zu] %s:%zu in %s() \xe2\x80\x94 error %d: \"%s\"\n",
                i,
                (f->file != NULL) ? f->file : "?",
                f->line,
                (f->func != NULL) ? f->func : "?",
                (int)f->code,
                (f->message[0] != '\0') ? f->message : "(no message)");
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Propagation helpers
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def DIAG_RETURN_IF(cond, diag, code, msg, retval)
 * @brief If `cond` is true, push a frame and return `retval`.
 *
 * Evaluates `cond` exactly once. Intended for guard clauses.
 *
 * @param cond    Boolean condition.
 * @param diag    Diag* (NULL-safe).
 * @param code    Error code to record.
 * @param msg     Context message string (may be NULL).
 * @param retval  Value to return from the enclosing function.
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
 * @brief Evaluate `call`; if it returns false (or 0), push a frame and return.
 *
 * `call` must be an expression that evaluates to a value comparable to false.
 * It is evaluated exactly once. Intended for chaining bool-returning functions.
 *
 * Example:
 *   DIAG_PROPAGATE(parse_int(str, &val, diag), diag,
 *                  ERR_PARSE_FAILED, "parse_int failed", false);
 *
 * @param call    Expression to evaluate (evaluated once).
 * @param diag    Diag* (NULL-safe).
 * @param code    Error code to record.
 * @param msg     Context message string (may be NULL).
 * @param retval  Value to return from the enclosing function.
 */
#define DIAG_PROPAGATE(call, diag, code, msg, retval) \
    do { \
        if (!(call)) { \
            DIAG_PUSH((diag), (code), (msg)); \
            return (retval); \
        } \
    } while (0)

#endif /* CANON_SEMANTICS_DIAG_H */
