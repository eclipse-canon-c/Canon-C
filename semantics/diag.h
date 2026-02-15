#ifndef CANON_SEMANTICS_DIAG_H
#define CANON_SEMANTICS_DIAG_H

#include <string.h>
#include <stdio.h>

#include "core/primitives/types.h"
#include "core/primitives/limits.h"
#include "core/primitives/contract.h"
#include "core/slice.h"
#include "semantics/error.h"

/**
 * @file semantics/diag.h
 * @brief Structured diagnostic frames — the missing third leg of error handling
 *
 * Canon-C's error handling has two legs:
 *   - error.h   → what failed (error codes)
 *   - result.h  → propagating it (Result<T, E>)
 *
 * diag.h is the third leg:
 *   - diag.h    → why it failed and where (context chain, no allocation)
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - A Diag is a stack-allocated chain of diagnostic frames
 * - Each frame records: file, line, function name, message, error code
 * - Frames are pushed manually as errors propagate up the call stack
 * - Maximum chain depth is fixed (DIAG_MAX_FRAMES, default 8)
 * - No heap allocation — ever
 * - Messages are string literals or stack strings — never heap strings
 * - The chain reads bottom-up: first frame is the root cause, last is the surface
 *
 * Design philosophy:
 * ────────────────────────────────────────────────────────────────────────────
 * This is NOT an exception system. There is no unwinding, no longjmp,
 * no automatic propagation. Every frame is pushed explicitly by the caller.
 * The discipline is: if you check an error, you push a frame. If you ignore
 * an error, you own that decision.
 *
 * This is NOT a logging system. Diag frames are structured data, not text.
 * They can be rendered to a StringBuf, printed to stderr, or serialized —
 * but that is the caller's job, not diag.h's.
 *
 * Relationship to other modules:
 * ────────────────────────────────────────────────────────────────────────────
 * - semantics/error.h  — DiagFrame.code is a Canon_Error value
 * - semantics/result/result.h — Diag can be attached to a Result failure path
 * - data/stringbuf.h   — diag_render() formats a Diag into a StringBuf
 *                        (included conditionally — see diag_render below)
 * - core/slice.h       — DiagFrame.message is a str_t (no null terminator req.)
 *
 * Dependency rule:
 * ────────────────────────────────────────────────────────────────────────────
 * diag.h lives in semantics/ and may include core/ and semantics/error.h only.
 * It must NOT include data/stringbuf.h — render functions are in diag_render.h.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Requires C99 or later
 * - Uses <string.h> for strncpy only (in diag_frame_msg)
 * - Uses <stdio.h> for diag_print only
 * - No platform-specific code, no dynamic allocation
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - diag_push:     O(1) — array write + increment
 * - diag_clear:    O(1) — reset counter
 * - diag_print:    O(n) — one fprintf per frame
 * - sizeof(Diag):  DIAG_MAX_FRAMES * sizeof(DiagFrame) + sizeof(usize)
 *                  ≈ 8 * ~80 bytes = ~640 bytes on 64-bit — stack-safe
 *
 * Quick start:
 * ```c
 * #include "semantics/diag.h"
 *
 * // At the root of failure — push the cause
 * bool parse_int(const char* s, int* out, Diag* diag) {
 *     if (!s) {
 *         DIAG_PUSH(diag, ERR_INVALID_ARG, "input string is NULL");
 *         return false;
 *     }
 *     // ...
 * }
 *
 * // One level up — add context before propagating
 * bool load_config(const char* path, Config* cfg, Diag* diag) {
 *     int port;
 *     if (!parse_int(port_str, &port, diag)) {
 *         DIAG_PUSH(diag, ERR_INVALID_ARG, "failed to parse port number");
 *         return false;
 *     }
 *     // ...
 * }
 *
 * // At the top — inspect or print the chain
 * Diag diag = diag_init();
 * if (!load_config("app.conf", &cfg, &diag)) {
 *     diag_print(&diag, stderr);
 *     // prints full chain: root cause → surface error
 * }
 * ```
 *
 * @sa semantics/error.h  — Canon_Error codes used in DiagFrame.code
 * @sa semantics/result.h — Result<T,E> for propagating errors with diag
 * @sa data/stringbuf.h   — for rendering a Diag to a string buffer
 */

/* ════════════════════════════════════════════════════════════════════════════
   Configuration
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Maximum number of frames in a Diag chain
 *
 * A frame is pushed each time an error crosses a call boundary with context.
 * 8 is sufficient for nearly all real-world call chains.
 * Increase if your architecture has deeper error propagation paths.
 *
 * sizeof(Diag) ≈ DIAG_MAX_FRAMES * sizeof(DiagFrame)
 */
#ifndef DIAG_MAX_FRAMES
    #define DIAG_MAX_FRAMES ((usize)8)
#endif

/**
 * @brief Maximum length of an inline message string in a DiagFrame
 *
 * Messages longer than this are silently truncated.
 * Sized to keep DiagFrame stack-friendly on 32-bit and 64-bit targets.
 */
#ifndef DIAG_MAX_MSG_LEN
    #define DIAG_MAX_MSG_LEN ((usize)128)
#endif

/* ════════════════════════════════════════════════════════════════════════════
   DiagFrame — a single step in the error chain
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief A single diagnostic frame — one step in an error propagation chain
 *
 * Each frame captures the source location and context message at the point
 * where an error was observed and annotated. Frames are pushed bottom-up:
 * frame[0] is the root cause, frame[depth-1] is the most recent context.
 *
 * Fields:
 * - file:     Source file name (__FILE__ — string literal, not owned)
 * - line:     Source line number (__LINE__)
 * - func:     Function name (__func__ — string literal, not owned)
 * - code:     Canon_Error code at this frame (may differ per level)
 * - message:  Inline null-terminated message string (DIAG_MAX_MSG_LEN bytes)
 *
 * Memory:
 * - file and func are string literals — not copied, not freed
 * - message is an inline char array — no pointer, no allocation
 * - sizeof(DiagFrame) = 3*sizeof(const char*) + sizeof(usize) +
 *                       sizeof(Canon_Error) + DIAG_MAX_MSG_LEN
 */
typedef struct {
    const char* file;                  ///< Source file (__FILE__)
    usize       line;                  ///< Source line (__LINE__)
    const char* func;                  ///< Function name (__func__)
    Canon_Error code;                  ///< Error code at this frame
    char        message[DIAG_MAX_MSG_LEN]; ///< Context message (inline, null-terminated)
} DiagFrame;

/* ════════════════════════════════════════════════════════════════════════════
   Diag — the diagnostic chain
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief A stack-allocated chain of diagnostic frames
 *
 * Diag is a fixed-depth array of DiagFrame values.
 * It is always stack-allocated — never heap-allocated.
 * It is passed by pointer to functions that may push frames onto it.
 * Passing NULL for the Diag* parameter is always valid — all functions
 * silently skip pushing when diag == NULL.
 *
 * Invariants:
 * - depth <= DIAG_MAX_FRAMES
 * - frames[0..depth-1] are valid DiagFrame values
 * - When depth == DIAG_MAX_FRAMES, push silently drops the oldest frame
 *   (shifts the chain) to preserve the most recent context
 *
 * Usage pattern:
 * ```c
 * Diag diag = diag_init();
 * if (!some_operation(&diag)) {
 *     diag_print(&diag, stderr);
 * }
 * ```
 */
typedef struct {
    DiagFrame frames[DIAG_MAX_FRAMES]; ///< Frame chain (index 0 = root cause)
    usize     depth;                   ///< Number of frames currently pushed
} Diag;

/* ════════════════════════════════════════════════════════════════════════════
   Construction
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initializes an empty Diag chain
 *
 * @return Zero-initialized Diag with depth == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: sizeof(Diag) on the stack — no heap allocation
 */
static inline Diag diag_init(void) {
    Diag d;
    d.depth = 0;
    /* zero the frames for clean debugger display */
    memset(d.frames, 0, sizeof(d.frames));
    return d;
}

/**
 * @brief Resets a Diag chain to empty without clearing frame memory
 *
 * @param d Diag to reset (NULL-safe)
 *
 * @post d->depth == 0
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline void diag_clear(Diag* d) {
    if (d) d->depth = 0;
}

/* ════════════════════════════════════════════════════════════════════════════
   Pushing frames
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Pushes a diagnostic frame onto the chain
 *
 * If d == NULL, does nothing (safe to call with NULL diag).
 * If the chain is full (depth == DIAG_MAX_FRAMES), shifts all frames
 * down by one (dropping the oldest root frame) to preserve recent context.
 *
 * @param d    Diag chain to push onto (NULL-safe)
 * @param file Source file name (pass __FILE__)
 * @param line Source line number (pass __LINE__)
 * @param func Function name (pass __func__)
 * @param code Canon_Error code at this frame
 * @param msg  Context message (string literal or stack string — copied inline)
 *
 * Performance:
 * - Time:  O(1) normal case, O(DIAG_MAX_FRAMES) on overflow (memmove)
 * - Space: O(1)
 */
static inline void diag_push(Diag* d,
                               const char* file,
                               usize line,
                               const char* func,
                               Canon_Error code,
                               const char* msg) {
    if (!d) return;

    if (d->depth == DIAG_MAX_FRAMES) {
        /* Chain full — shift frames down, drop oldest, preserve most recent context */
        memmove(&d->frames[0], &d->frames[1],
                (DIAG_MAX_FRAMES - 1) * sizeof(DiagFrame));
        d->depth = DIAG_MAX_FRAMES - 1;
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
 * @brief Pushes a frame with automatic file/line/func capture
 *
 * The primary way to annotate an error at a call site.
 * Captures __FILE__, __LINE__, __func__ automatically.
 *
 * @param diag Pointer to Diag (may be NULL — push is skipped)
 * @param code Canon_Error code
 * @param msg  String literal or const char* message
 *
 * Example:
 * ```c
 * if (!read_file(path, &buf, diag)) {
 *     DIAG_PUSH(diag, ERR_IO, "failed to read configuration file");
 *     return false;
 * }
 * ```
 */
#define DIAG_PUSH(diag, code, msg) \
    diag_push((diag), __FILE__, (usize)__LINE__, __func__, (code), (msg))

/**
 * @def DIAG_PUSH_FMT(diag, code, buf, buf_size, fmt, ...)
 * @brief Pushes a frame with a formatted message (no heap allocation)
 *
 * Formats the message into a caller-provided stack buffer before pushing.
 * The formatted string is copied into the DiagFrame inline storage —
 * the caller's buffer is only needed for the duration of the push.
 *
 * @param diag     Pointer to Diag (may be NULL)
 * @param code     Canon_Error code
 * @param buf      Caller-provided char buffer for formatting (stack-allocated)
 * @param buf_size sizeof(buf)
 * @param fmt      printf-style format string
 * @param ...      Format arguments
 *
 * Example:
 * ```c
 * char msg_buf[128];
 * DIAG_PUSH_FMT(diag, ERR_INVALID_ARG, msg_buf, sizeof(msg_buf),
 *               "index %zu out of range [0, %zu)", i, len);
 * ```
 */
#define DIAG_PUSH_FMT(diag, code, buf, buf_size, fmt, ...) \
    do { \
        snprintf((buf), (buf_size), (fmt), __VA_ARGS__); \
        diag_push((diag), __FILE__, (usize)__LINE__, __func__, (code), (buf)); \
    } while (0)

/* ════════════════════════════════════════════════════════════════════════════
   Inspection
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns the number of frames currently in the chain
 *
 * @param d Diag to query (NULL-safe)
 * @return usize — 0 if d == NULL or chain is empty
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline usize diag_depth(const Diag* d) {
    return d ? d->depth : 0;
}

/**
 * @brief Returns true if the Diag chain has any frames
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline bool diag_has_error(const Diag* d) {
    return d && d->depth > 0;
}

/**
 * @brief Returns a pointer to the frame at index i
 *
 * @param d Diag chain to inspect (NULL-safe)
 * @param i Frame index (0 = root cause, depth-1 = most recent)
 * @return Pointer to DiagFrame, or NULL if d == NULL or i >= depth
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline const DiagFrame* diag_frame_at(const Diag* d, usize i) {
    if (!d || i >= d->depth) return NULL;
    return &d->frames[i];
}

/**
 * @brief Returns a pointer to the root cause frame (index 0)
 *
 * The first frame pushed — deepest in the call stack.
 *
 * @param d Diag chain (NULL-safe)
 * @return Pointer to root DiagFrame, or NULL if chain is empty
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline const DiagFrame* diag_root(const Diag* d) {
    return diag_frame_at(d, 0);
}

/**
 * @brief Returns a pointer to the most recent frame (index depth-1)
 *
 * The last frame pushed — closest to the surface error.
 *
 * @param d Diag chain (NULL-safe)
 * @return Pointer to most recent DiagFrame, or NULL if chain is empty
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline const DiagFrame* diag_latest(const Diag* d) {
    if (!d || d->depth == 0) return NULL;
    return &d->frames[d->depth - 1];
}

/**
 * @brief Returns the error code of the root cause frame
 *
 * @param d Diag chain (NULL-safe)
 * @return Canon_Error — ERR_NONE if chain is empty or d == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline Canon_Error diag_root_code(const Diag* d) {
    const DiagFrame* f = diag_root(d);
    return f ? f->code : ERR_NONE;
}

/**
 * @brief Returns the error code of the most recent frame
 *
 * @param d Diag chain (NULL-safe)
 * @return Canon_Error — ERR_NONE if chain is empty or d == NULL
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline Canon_Error diag_latest_code(const Diag* d) {
    const DiagFrame* f = diag_latest(d);
    return f ? f->code : ERR_NONE;
}

/* ════════════════════════════════════════════════════════════════════════════
   Output
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Prints the full diagnostic chain to a FILE stream
 *
 * Prints frames from root cause (index 0) to most recent (index depth-1).
 * Each frame is printed on one line:
 *
 *   [0] file.c:42 in my_func() — ERR_INVALID_ARG: "input was NULL"
 *   [1] caller.c:17 in load() — ERR_INVALID_ARG: "failed to parse port"
 *
 * @param d      Diag chain to print (NULL-safe — prints nothing if NULL)
 * @param stream Output stream (e.g. stderr, stdout)
 *
 * Performance:
 * - Time:  O(depth) — one fprintf per frame
 * - Space: O(1)
 */
static inline void diag_print(const Diag* d, FILE* stream) {
    if (!d || !stream || d->depth == 0) return;

    for (usize i = 0; i < d->depth; i++) {
        const DiagFrame* f = &d->frames[i];
        fprintf(stream,
            "[%zu] %s:%zu in %s() — error %d: \"%s\"\n",
            i,
            f->file  ? f->file : "?",
            f->line,
            f->func  ? f->func : "?",
            (int)f->code,
            f->message[0] ? f->message : "(no message)"
        );
    }
}

/**
 * @brief Prints only the root cause frame to a FILE stream
 *
 * Useful when you want a one-line error summary.
 *
 * @param d      Diag chain (NULL-safe)
 * @param stream Output stream
 *
 * Performance:
 * - Time:  O(1)
 * - Space: O(1)
 */
static inline void diag_print_root(const Diag* d, FILE* stream) {
    if (!d || !stream || d->depth == 0) return;
    const DiagFrame* f = diag_root(d);
    fprintf(stream,
        "%s:%zu in %s() — error %d: \"%s\"\n",
        f->file  ? f->file : "?",
        f->line,
        f->func  ? f->func : "?",
        (int)f->code,
        f->message[0] ? f->message : "(no message)"
    );
}

/* ════════════════════════════════════════════════════════════════════════════
   Propagation helpers
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @def DIAG_RETURN_IF(cond, diag, code, msg, retval)
 * @brief Pushes a frame and returns retval if cond is true
 *
 * Common pattern: check a condition, annotate with context, early-return.
 *
 * @param cond   Condition that signals failure
 * @param diag   Pointer to Diag (may be NULL)
 * @param code   Canon_Error code
 * @param msg    Context message
 * @param retval Value to return on failure
 *
 * Example:
 * ```c
 * DIAG_RETURN_IF(!buf, diag, ERR_INVALID_ARG, "buffer is NULL", false);
 * ```
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
 * @brief Calls an expression; if falsy, pushes context frame and returns retval
 *
 * The most common propagation pattern in Canon-C:
 * call a function, check its result, push context if it failed.
 *
 * @param call   Expression to evaluate (must be convertible to bool)
 * @param diag   Pointer to Diag (may be NULL)
 * @param code   Canon_Error code for this context frame
 * @param msg    Context message at this level
 * @param retval Value to return on failure
 *
 * Example:
 * ```c
 * DIAG_PROPAGATE(
 *     parse_int(str, &val, diag),
 *     diag, ERR_INVALID_ARG, "failed to parse timeout value", false
 * );
 * ```
 */
#define DIAG_PROPAGATE(call, diag, code, msg, retval) \
    do { \
        if (!(call)) { \
            DIAG_PUSH((diag), (code), (msg)); \
            return (retval); \
        } \
    } while (0)

#endif /* CANON_SEMANTICS_DIAG_H */
