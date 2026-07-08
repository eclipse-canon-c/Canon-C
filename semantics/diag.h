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

#ifndef CANON_SEMANTICS_DIAG_H
#define CANON_SEMANTICS_DIAG_H

#include "core/primitives/types.h"    /* usize, bool */
#include "core/primitives/contract.h" /* require_msg, static_require */
#include "semantics/error.h"          /* Error, ERR_OK */

/* Standard library headers required by this file */
#include <string.h>  /* memmove, memcpy */
#include <stdio.h>   /* FILE, fprintf, snprintf */

#ifdef __FRAMAC__
/* ────────────────────────────────────────────────────────────────────────
   WP-only bounded assigns for the buffer-writing stdio functions.

   Root cause (diagnosed via the borrow.h contrast): borrow.h's libc calls
   (memcmp/strlen) have bounded assigns and verify cleanly as residuals.
   snprintf WRITES the caller buffer, and the spec WP resolves for it in
   Frama-C 29 carries `assigns *(buf+(0..))` — an open range WP's Typed+Cast
   model rejects with 'Invalid infinite range', aborting the whole run
   before any summary. Bounding the write to buf[0..size-1] gives WP a
   finite location.

   These redeclarations are visible ONLY to Frama-C (__FRAMAC__), never to
   the C compiler, so they cannot clash with the real <stdio.h>. They
   require -variadic-no-translation in the WP job: with translation ON the
   call is rewritten to snprintf_va_2 before WP runs and a spec on plain
   `snprintf` never binds. With translation OFF the call stays `snprintf`
   and this spec attaches.

   Return value left loosely bounded: the render functions handle both
   truncation (>= size) and the n<0 encoding-error path (the line-668
   libc-environmental residual), so no exact return relation is asserted. */
/*@
  requires valid_buf: \valid(((char *)buf) + (0 .. size - 1));
  requires valid_fmt: valid_read_string(format);
  assigns  ((char *)buf)[0 .. size - 1];
  assigns  \result \from indirect: size, indirect: ((char *)buf)[0 .. size - 1],
                         indirect: format[0 .. ];
  ensures  bounded: \result >= -1;
*/
extern int snprintf(char *buf, size_t size, const char *format, ...);

/*@
  requires valid_stream: \valid(stream);
  requires valid_fmt: valid_read_string(format);
  assigns  *stream \from *stream, indirect: format[0 .. ];
  assigns  \result \from indirect: *stream;
*/
extern int fprintf(FILE *stream, const char *format, ...);
#endif /* __FRAMAC__ */

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
 * No data/stringbuf.h — rendering uses snprintf into caller-supplied buffers.
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * Let N = DIAG_MAX_FRAMES, M = DIAG_MAX_MSG_LEN.
 *
 *   Function              | Time          | Space (stack)
 *   ──────────────────────|───────────────|──────────────────────────────
 *   diag_init             | O(N × M)      | O(1)  (returns by value)
 *   diag_clear            | O(1)          | O(1)
 *   diag_push             | O(M) normal   | O(1)
 *                         | O(N × M) overflow (memmove shift)
 *   diag_depth            | O(1)          | O(1)
 *   diag_has_error        | O(1)          | O(1)
 *   diag_frame_at         | O(1)          | O(1)
 *   diag_root             | O(1)          | O(1)
 *   diag_latest           | O(1)          | O(1)
 *   diag_root_code        | O(1)          | O(1)
 *   diag_latest_code      | O(1)          | O(1)
 *   diag_print            | O(N × M)      | O(1)
 *   diag_render_frame     | O(M)          | O(1)  (writes to caller buffer)
 *   diag_render           | O(N × M)      | O(1)  (writes to caller buffer)
 *
 *   sizeof(Diag) ≈ N × (M + 3 pointers + 2 integers) + 1 integer
 *
 *   Default (N=8, M=128):  ≈ 1280 bytes
 *   Compact (N=4, M=64):   ≈  416 bytes
 *   Minimal (N=2, M=32):   ≈  136 bytes
 *
 *   Choose N and M based on your stack budget. On a Cortex-M with 4 KB
 *   stack, N=4 M=64 uses ~10%. On 8 KB stack, defaults use ~16%.
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
 *
 * Bare-metal rendering (no FILE*):
 * ```c
 * char buf[512];
 * usize n = diag_render(&diag, buf, sizeof(buf));
 * uart_write(buf, n);
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
 * Compile-time contracts: configuration values must satisfy their documented
 * lower bounds. static_require() from contract.h fires as a hard compile
 * error if either condition is false — it never reaches runtime.
 * The msg argument must be an identifier (no spaces, no quotes).
 */
static_require(DIAG_MAX_FRAMES  >= 1, DIAG_MAX_FRAMES_must_be_at_least_1);
static_require(DIAG_MAX_MSG_LEN >= 1, DIAG_MAX_MSG_LEN_must_be_at_least_1);

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
   ACSL predicates — shared verification vocabulary (Frama-C/WP)
   ════════════════════════════════════════════════════════════════════════════

   Verification round-1 notes (VERIFY-017 candidate):
   - Predicted own-residual classes: (a) memmove/memcpy call-site goals in
     diag_push — the byte-level libc specs vs. Typed field reasoning class
     known from prior headers, at new sites; (b) snprintf/fprintf goals in
     the rendering functions — variadic translation plus deliberately weak
     stdio specs, a class new to the project and intrinsic to stdio-facing
     code.
   - The overflow clamp in diag_push carries a named dead-code assert
     (dead_by_invariant_clamp), closing the MC/DC line-293 partial
     cross-stream once discharged.
   - Libc-facing clauses are drafted against Frama-C's stock libc specs and
     must be aligned to the actual specs from the first WP output, not
     defended against it. */

/*@
  predicate diag_invariant(Diag d) =
    d.depth <= DIAG_MAX_FRAMES;

  predicate frame_strings_ok(DiagFrame f) =
    (f.file == \null || valid_read_string(f.file)) &&
    (f.func == \null || valid_read_string(f.func)) &&
    (\exists integer k;
       0 <= k < DIAG_MAX_MSG_LEN && f.message[k] == '\0');
*/

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
 *
 * Time:  O(N × M) — memset over sizeof(Diag).
 * Space: O(1) — returned by value, no heap.
 */
/*@
  assigns \nothing;
  ensures init_empty: \result.depth == 0;
*/
static inline Diag diag_init(void)
{
    Diag d;
    memset(&d, 0, sizeof(d));
    /* Redundant with the memset, but restates depth == 0 as a typed
     * store: WP derives the postcondition directly instead of through
     * byte-level reinterpretation of the memset, which the Typed memory
     * model cannot do. Same authoring-time pattern as the overflow clamp
     * in diag_push. The optimizer folds the dead store. */
    d.depth = 0;
    return d;
}

/**
 * @brief Resets the chain to empty (sets depth = 0).
 *
 * Does NOT zero frame contents (intentional — O(1), safe).
 * Previously pushed frames become invisible but their memory is reused on
 * the next push.
 *
 * Time:  O(1).
 * Space: O(1).
 *
 * @param d  Diag pointer. NULL-safe: no-op when NULL.
 */
/*@
  behavior null_diag:
    assumes d == \null;
    assigns \nothing;
  behavior valid_diag:
    assumes d != \null;
    requires clear_valid: \valid(d);
    assigns d->depth;
    ensures clear_empty: d->depth == 0;
  complete behaviors;
  disjoint behaviors;
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
 * Time:  O(M) normal (message copy).
 *        O(N × M) on overflow (memmove shifts N-1 frames, each ~M bytes).
 * Space: O(1).
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
/*@
  behavior null_diag:
    assumes d == \null;
    assigns \nothing;
    ensures push_null_noop: \result == \false;

  behavior valid_diag:
    assumes d != \null;
    requires push_valid:     \valid(d);
    requires push_invariant: diag_invariant(*d);
    requires push_msg_read:  msg == \null || valid_read_string(msg);
    requires push_no_alias:  msg == \null ||
             \separated(msg + (0 .. strlen(msg)),
                        (char *)d + (0 .. sizeof(Diag) - 1));
    assigns d->depth, d->frames[0 .. DIAG_MAX_FRAMES - 1];
    ensures push_depth_bounds: 1 <= d->depth <= DIAG_MAX_FRAMES;
    ensures push_depth_step:
      d->depth == (\old(d->depth) == DIAG_MAX_FRAMES
                   ? DIAG_MAX_FRAMES
                   : \old(d->depth) + 1);
    ensures push_overflow_flag:
      \result <==> (\old(d->depth) == DIAG_MAX_FRAMES);
    ensures push_frame_file: d->frames[d->depth - 1].file == file;
    ensures push_frame_func: d->frames[d->depth - 1].func == func;
    ensures push_frame_line: d->frames[d->depth - 1].line == line;
    ensures push_frame_code: d->frames[d->depth - 1].code == code;
    ensures push_msg_terminated:
      \exists integer k;
        0 <= k < DIAG_MAX_MSG_LEN
        && d->frames[d->depth - 1].message[k] == '\0';
    ensures push_shift_semantics:
      \old(d->depth) == DIAG_MAX_FRAMES ==>
        (\forall integer j; 0 <= j < DIAG_MAX_FRAMES - 1 ==>
           d->frames[j].code == \old(d->frames[j + 1].code));
    ensures push_frames_preserved:
      \old(d->depth) < DIAG_MAX_FRAMES ==>
        (\forall integer j; 0 <= j < \old(d->depth) ==>
           d->frames[j].code == \old(d->frames[j].code));

  complete behaviors;
  disjoint behaviors;
*/
static inline bool diag_push(Diag       *d,
                              const char *file,
                              usize       line,
                              const char *func,
                              Error       code,
                              const char *msg)
{
    bool       dropped;
    DiagFrame *f;
    usize      len;

    if (d == NULL) {
        return false;
    }

    /* Internal invariant: depth can never legally exceed DIAG_MAX_FRAMES. */
    require_msg(d->depth <= DIAG_MAX_FRAMES,
                "diag_push: depth exceeded DIAG_MAX_FRAMES -- chain is corrupt");

    dropped = false;

    if (d->depth == DIAG_MAX_FRAMES) {
        /* Chain full: discard oldest frame, shift remaining frames down. */
        memmove(&d->frames[0],
                &d->frames[1],
                (DIAG_MAX_FRAMES - 1u) * sizeof(DiagFrame));
        d->depth--;
        dropped = true;
    }

    /* Redundant under the require_msg invariant above and the overflow
     * branch (which guarantees depth < DIAG_MAX_FRAMES here), but makes the
     * d->depth < DIAG_MAX_FRAMES bound visible to the optimizer even under
     * -DNDEBUG, where require_msg compiles out. Without this, GCC 16 at -O3
     * mis-analyzes the inlined overflow path and fires a spurious
     * -Wstringop-overflow on the f->message write below. No-op on every
     * correct execution. */
    if (d->depth >= DIAG_MAX_FRAMES) {
        /*@ assert dead_by_invariant_clamp: \false; */
        d->depth = DIAG_MAX_FRAMES - 1u;
    }

    f = &d->frames[d->depth];
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
        len = 0u;
        /*@
          loop invariant copy_len_bounds: 0 <= len <= DIAG_MAX_MSG_LEN - 1;
          loop invariant copy_scanned:
            \forall integer k; 0 <= k < len ==> msg[k] != '\0';
          loop assigns len;
          loop variant DIAG_MAX_MSG_LEN - 1 - len;
        */
        while (len < (DIAG_MAX_MSG_LEN - 1u) && msg[len] != '\0') {
            len++;
        }
        memcpy(f->message, msg, len);
        f->message[len] = '\0';
    } else {
        f->message[0] = '\0';
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
 * Time:  O(1).
 * Space: O(1).
 *
 * @param d  Diag pointer. NULL-safe: returns 0.
 */
/*@
  requires depth_read: d == \null || \valid_read(d);
  assigns \nothing;
  ensures depth_null: d == \null ==> \result == 0;
  ensures depth_value: d != \null ==> \result == d->depth;
*/
static inline usize diag_depth(const Diag *d)
{
    return (d != NULL) ? d->depth : (usize)0;
}

/**
 * @brief Returns true if the chain contains at least one frame.
 *
 * Time:  O(1).
 * Space: O(1).
 *
 * @param d  Diag pointer. NULL-safe: returns false.
 */
/*@
  requires he_read: d == \null || \valid_read(d);
  assigns \nothing;
  ensures he_iff: \result <==> (d != \null && d->depth > 0);
*/
static inline bool diag_has_error(const Diag *d)
{
    return (d != NULL) && (d->depth > 0u);
}

/**
 * @brief Returns a pointer to the frame at index `i` (0 = root cause).
 *
 * Time:  O(1).
 * Space: O(1).
 *
 * @param d  Diag pointer. NULL-safe: returns NULL.
 * @param i  Frame index. Returns NULL if i >= depth.
 * @return   Pointer to DiagFrame, or NULL if out of range.
 */
/*@
  requires fa_read: d == \null || \valid_read(d);
  requires fa_inv:  d == \null || diag_invariant(*d);
  assigns \nothing;
  ensures fa_miss: (d == \null || i >= d->depth) ==> \result == \null;
  ensures fa_hit:  (d != \null && i <  d->depth) ==> \result == &d->frames[i];
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
 * Time:  O(1).
 * Space: O(1).
 *
 * @param d  Diag pointer. NULL-safe: returns NULL.
 * @return   Pointer to DiagFrame, or NULL if chain is empty.
 */
/*@
  requires root_read: d == \null || \valid_read(d);
  requires root_inv:  d == \null || diag_invariant(*d);
  assigns \nothing;
  ensures root_miss: (d == \null || d->depth == 0) ==> \result == \null;
  ensures root_hit:  (d != \null && d->depth > 0)  ==> \result == &d->frames[0];
*/
static inline const DiagFrame *diag_root(const Diag *d)
{
    return diag_frame_at(d, 0u);
}

/**
 * @brief Returns a pointer to the most recently pushed frame (surface error).
 *
 * Time:  O(1).
 * Space: O(1).
 *
 * @param d  Diag pointer. NULL-safe: returns NULL.
 * @return   Pointer to DiagFrame, or NULL if chain is empty.
 */
/*@
  requires lat_read: d == \null || \valid_read(d);
  requires lat_inv:  d == \null || diag_invariant(*d);
  assigns \nothing;
  ensures lat_miss: (d == \null || d->depth == 0) ==> \result == \null;
  ensures lat_hit:
    (d != \null && d->depth > 0) ==> \result == &d->frames[d->depth - 1];
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
 * Time:  O(1).
 * Space: O(1).
 *
 * @param d  Diag pointer. NULL-safe: returns ERR_OK.
 * @return   Error code, or ERR_OK if the chain is empty.
 */
/*@
  requires rc_read: d == \null || \valid_read(d);
  requires rc_inv:  d == \null || diag_invariant(*d);
  assigns \nothing;
  ensures rc_empty: (d == \null || d->depth == 0) ==> \result == ERR_OK;
  ensures rc_value:
    (d != \null && d->depth > 0) ==> \result == d->frames[0].code;
*/
static inline Error diag_root_code(const Diag *d)
{
    const DiagFrame *f = diag_root(d);
    return (f != NULL) ? f->code : ERR_OK;
}

/**
 * @brief Returns the error code of the most recently pushed frame.
 *
 * Time:  O(1).
 * Space: O(1).
 *
 * @param d  Diag pointer. NULL-safe: returns ERR_OK.
 * @return   Error code, or ERR_OK if the chain is empty.
 */
/*@
  requires lc_read: d == \null || \valid_read(d);
  requires lc_inv:  d == \null || diag_invariant(*d);
  assigns \nothing;
  ensures lc_empty: (d == \null || d->depth == 0) ==> \result == ERR_OK;
  ensures lc_value:
    (d != \null && d->depth > 0) ==>
      \result == d->frames[d->depth - 1].code;
*/
static inline Error diag_latest_code(const Diag *d)
{
    const DiagFrame *f = diag_latest(d);
    return (f != NULL) ? f->code : ERR_OK;
}

/* ════════════════════════════════════════════════════════════════════════════
   Output
   ════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Prints the full chain to `stream`, from root cause to latest frame.
 *
 * Output format per frame:
 *   [<index>] <file>:<line> in <func>() -- error <code>: "<message>"
 *
 * The separator is ASCII "--" for portability across all platforms,
 * terminal codepages, and embedded serial outputs. A UTF-8 em dash would
 * produce mojibake on Windows legacy codepages and non-UTF-8 targets.
 *
 * No trailing newline after the last frame beyond what each line already has.
 *
 * On bare-metal targets where FILE* is unavailable, use diag_render()
 * instead — same output format, writes to a caller-supplied buffer.
 *
 * Time:  O(N × M) — iterates all frames, fprintf per frame.
 * Space: O(1) — no buffers, writes directly to stream.
 *
 * @param d       Diag pointer. NULL-safe: no-op.
 * @param stream  Output FILE*. NULL-safe: no-op.
 */
/*@
  requires pr_read: d == \null || \valid_read(d);
  requires pr_inv:  d == \null || diag_invariant(*d);
  requires pr_strings: d == \null ||
    (\forall integer j;
       0 <= j < d->depth ==> frame_strings_ok(d->frames[j]));
  requires pr_stream: stream == \null || \valid(stream);

  behavior noop:
    assumes d == \null || stream == \null
            || (d != \null && d->depth == 0);
    assigns \nothing;
  behavior prints:
    assumes d != \null && stream != \null && d->depth > 0;
    assigns *stream;
  complete behaviors;
  disjoint behaviors;
*/
static inline void diag_print(const Diag *d, FILE *stream)
{
    usize i;

    if (d == NULL || stream == NULL || d->depth == 0u) {
        return;
    }

    /*@
      loop invariant print_i_bounds: 0 <= i <= d->depth;
      loop assigns i, *stream;
      loop variant d->depth - i;
    */
    for (i = 0u; i < d->depth; i++) {
        const DiagFrame *f = &d->frames[i];
        fprintf(stream,
                "[%zu] %s:%zu in %s() -- error %d: \"%s\"\n",
                i,
                (f->file != NULL) ? f->file : "?",
                f->line,
                (f->func != NULL) ? f->func : "?",
                (int)f->code,
                (f->message[0] != '\0') ? f->message : "(no message)");
    }
}

/**
 * @brief Renders a single diagnostic frame into a caller-supplied buffer.
 *
 * Output format (same as diag_print, without trailing newline):
 *   [<index>] <file>:<line> in <func>() -- error <code>: "<message>"
 *
 * No trailing newline — the caller decides line endings. This makes
 * diag_render_frame() composable for telemetry packets, structured logs,
 * or any output format where newlines are not appropriate.
 *
 * Follows snprintf return semantics:
 * - Returns the number of bytes that would be written (excluding null
 *   terminator), even if the buffer is too small.
 * - If the return value >= buf_size, the output was truncated.
 * - Always null-terminates when buf_size > 0.
 *
 * Time:  O(M) — snprintf over one frame's fields.
 * Space: O(1) — writes to caller-supplied buffer, no internal allocation.
 *
 * @param f         Pointer to a DiagFrame. NULL-safe: returns 0.
 * @param index     Frame index (for display in the [<index>] prefix).
 * @param buf       Destination buffer. NULL-safe: returns 0.
 * @param buf_size  Size of buf in bytes. 0-safe: returns 0.
 * @return          Bytes that would be written (excluding null terminator).
 */
/*@
  requires rf_frame:   f == \null || \valid_read(f);
  requires rf_strings: f == \null || frame_strings_ok(*f);
  requires rf_buf:
    buf == \null || buf_size == 0 || \valid(buf + (0 .. buf_size - 1));

  behavior no_buffer:
    assumes buf == \null || buf_size == 0;
    assigns \nothing;
    ensures rf_nb_zero: \result == 0;
  behavior null_frame:
    assumes buf != \null && buf_size > 0 && f == \null;
    assigns buf[0];
    ensures rf_nf_zero: \result == 0;
    ensures rf_nf_cleared: buf[0] == '\0';
  behavior rendered:
    assumes buf != \null && buf_size > 0 && f != \null;
    requires rf_rendered_valid: \valid(buf + (0 .. buf_size - 1));
    assigns buf[0 .. buf_size - 1];
    ensures rf_terminated:
      \exists integer k; 0 <= k < buf_size && buf[k] == '\0';
  complete behaviors;
  disjoint behaviors;
*/
static inline usize diag_render_frame(const DiagFrame *f,
                                       usize            index,
                                       char            *buf,
                                       usize            buf_size)
{
    int n;

    if (buf != NULL && buf_size > 0u) {
        buf[0] = '\0';
    }

    if (f == NULL || buf == NULL || buf_size == 0u) {
        return 0u;
    }

    /* GCC's -Wformat-truncation (a middle-end warning, active under
     * optimization) statically proves truncation whenever a caller passes
     * a fixed-size buffer smaller than the worst-case rendering — but
     * truncation is this API's documented, intended behaviour: snprintf
     * return semantics exist precisely so the caller can detect it.
     * Without suppression, correct user code fails under -Wall -Werror
     * at -O2+ (first observed on MinGW-UCRT GCC 14, reproducible on
     * Linux GCC 14 when inlining propagates the buffer size). The
     * diagnostic is attributed to this line, so the suppression must
     * live here, not at call sites. Scope: exactly this snprintf. */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
    n = snprintf(buf, buf_size,
                 "[%zu] %s:%zu in %s() -- error %d: \"%s\"",
                 index,
                 (f->file != NULL) ? f->file : "?",
                 f->line,
                 (f->func != NULL) ? f->func : "?",
                 (int)f->code,
                 (f->message[0] != '\0') ? f->message : "(no message)");
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif

    return (n < 0) ? 0u : (usize)n;
}

/**
 * @brief Renders the full diagnostic chain into a caller-supplied buffer.
 *
 * Output format (identical to diag_print):
 *   [0] file.c:24 in parse_timeout() -- error 3: "timeout value invalid"
 *   [1] config.c:51 in load_config() -- error 4: "failed to parse timeout"
 *
 * Each frame is followed by a newline. Frames are rendered from root cause
 * (index 0) to surface error (index depth-1).
 *
 * This is the primary rendering path on bare-metal and RTOS targets where
 * FILE* is unavailable — the caller owns the buffer and decides how to
 * emit it (UART, telemetry packet, flash log, debug register).
 *
 * Follows snprintf return semantics:
 * - Returns the total number of bytes that would be written (excluding
 *   null terminator), even if the buffer is too small.
 * - If the return value >= buf_size, the output was truncated.
 * - Always null-terminates when buf_size > 0.
 * - Truncation may occur mid-frame. Use diag_render_frame() if per-frame
 *   control is needed.
 *
 * Time:  O(N × M) — snprintf per frame, iterates all frames.
 *        When buffer is full, remaining frames are still measured
 *        (snprintf with size 0) to produce the correct total count.
 * Space: O(1) — writes to caller-supplied buffer, no internal allocation.
 *
 * @param d         Diag chain. NULL-safe: returns 0.
 * @param buf       Destination buffer. NULL-safe: returns 0.
 * @param buf_size  Size of buf in bytes. 0-safe: returns 0.
 * @return          Total bytes that would be written (excluding null
 *                  terminator). Compare against buf_size to detect
 *                  truncation.
 */
/*@
  requires r_read: d == \null || \valid_read(d);
  requires r_inv:  d == \null || diag_invariant(*d);
  requires r_strings: d == \null ||
    (\forall integer j;
       0 <= j < d->depth ==> frame_strings_ok(d->frames[j]));
  requires r_buf:
    buf == \null || buf_size == 0 || \valid(buf + (0 .. buf_size - 1));

  behavior no_buffer:
    assumes buf == \null || buf_size == 0;
    assigns \nothing;
    ensures r_nb_zero: \result == 0;
  behavior nothing_to_render:
    assumes buf != \null && buf_size > 0
            && (d == \null || (d != \null && d->depth == 0));
    assigns buf[0];
    ensures r_ntr_zero: \result == 0;
    ensures r_ntr_cleared: buf[0] == '\0';
  behavior rendered:
    assumes buf != \null && buf_size > 0 && d != \null && d->depth > 0;
    requires r_rendered_valid: \valid(buf + (0 .. buf_size - 1));
    assigns buf[0 .. buf_size - 1];
    ensures r_terminated:
      \exists integer k; 0 <= k < buf_size && buf[k] == '\0';
  complete behaviors;
  disjoint behaviors;
*/
static inline usize diag_render(const Diag *d,
                                 char       *buf,
                                 usize       buf_size)
{
    usize total = 0u;
    usize i;

    if (buf != NULL && buf_size > 0u) {
        buf[0] = '\0';
    }

    /* buf/buf_size must be rejected here, not just above: the loop below
     * passes dst = buf + total to snprintf with rem > 0 when total <
     * buf_size, which requires a valid pointer. Rejecting NULL/0 enforces
     * the documented "NULL-safe: returns 0" / "0-safe: returns 0" contract
     * and matches diag_render_frame's guard shape. */
    if (d == NULL || buf == NULL || buf_size == 0u || d->depth == 0u) {
        return 0u;
    }

    /*@
      loop invariant render_i_bounds: 0 <= i <= d->depth;
      loop assigns i, total, buf[0 .. buf_size - 1];
      loop variant d->depth - i;
    */
    for (i = 0u; i < d->depth; i++) {
        const DiagFrame *f = &d->frames[i];
        usize            rem;
        char            *dst;
        int              n;

        /*
         * Write into remaining buffer space, or compute count without
         * writing if the buffer is already full.
         *
         * snprintf(ptr, 0, ...) is valid C99: writes nothing, returns
         * the byte count. The pointer must be valid but is not
         * dereferenced when size is 0.
         */
        if (total < buf_size) {
            dst = buf + total;
            rem = buf_size - total;
        } else {
            dst = buf;
            rem = 0u;
        }

        /* Same -Wformat-truncation suppression as diag_render_frame:
         * truncation into a small caller buffer is documented behaviour
         * (the would-be total is returned for detection). See the comment
         * on the snprintf in diag_render_frame above. */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
        n = snprintf(dst, rem,
                     "[%zu] %s:%zu in %s() -- error %d: \"%s\"\n",
                     i,
                     (f->file != NULL) ? f->file : "?",
                     f->line,
                     (f->func != NULL) ? f->func : "?",
                     (int)f->code,
                     (f->message[0] != '\0') ? f->message : "(no message)");
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif

        if (n < 0) { continue; } /* encoding error — skip frame */
        total += (usize)n;
    }

    return total;
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
