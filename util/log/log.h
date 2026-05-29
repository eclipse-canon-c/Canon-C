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

#ifndef CANON_UTIL_LOG_H
#define CANON_UTIL_LOG_H

#include <stdarg.h>
#include <stdio.h>

#include "core/primitives/types.h"
#include "core/ownership.h"
#include "semantics/result/result.h"
#include "semantics/error.h"

/* ────────────────────────────────────────────────────────────────────────────
   result_bool_Error instantiation
   ──────────────────────────────────────────────────────────────────────────

   In C99, bool is a macro (#define bool _Bool), so CANON_RESULT(bool, Error)
   would mangle to result__Bool_Error — not result_bool_Error. To get the
   correct name, we use a typedef alias that is a plain identifier.

   Guard: only instantiate if not already provided by another header. */

#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
#define CANON_RESULT_BOOL_ERROR_DEFINED

#include <stdbool.h>

typedef bool canon_bool_;

/* Instantiate with the typedef, producing result_canon_bool__Error.
 * Then typedef to the canonical name result_bool_Error and provide
 * wrapper functions with the expected names. */
CANON_RESULT(canon_bool_, Error)

typedef result_canon_bool__Error result_bool_Error;

static inline result_bool_Error result_bool_Error_ok(bool v) {
    return result_canon_bool__Error_ok(v);
}
static inline result_bool_Error result_bool_Error_err(Error e) {
    return result_canon_bool__Error_err(e);
}
static inline bool result_bool_Error_is_ok(result_bool_Error r) {
    return result_canon_bool__Error_is_ok(r);
}
static inline bool result_bool_Error_is_err(result_bool_Error r) {
    return result_canon_bool__Error_is_err(r);
}
static inline bool result_bool_Error_unwrap(result_bool_Error r) {
    return result_canon_bool__Error_unwrap(r);
}
static inline Error result_bool_Error_unwrap_err(result_bool_Error r) {
    return result_canon_bool__Error_unwrap_err(r);
}

#endif /* CANON_RESULT_BOOL_ERROR_DEFINED */

/**
 * @file util/log/log.h
 * @brief Minimal, explicit, zero-allocation logging with observable failures
 *
 * Provides a simple, safe logging facility where every I/O operation
 * returns result_bool_Error — forcing callers to consciously handle (or
 * ignore) potential failures. No global state, no hidden configuration,
 * no silent drops.
 *
 * Error handling rationale:
 * ────────────────────────────────────────────────────────────────────────────
 * NULL stream and NULL format are data errors (Err(ERR_INVALID_ARG)),
 * not contract violations. A logger that aborts your program on bad
 * input defeats its purpose.
 *
 * Runtime dependency:
 * ────────────────────────────────────────────────────────────────────────────
 * This module depends on <stdio.h> and <stdarg.h>. All Canon-C layers
 * below util/ remain stdio-free.
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * Reentrant but not thread-safe on the same FILE* stream.
 *
 * Error codes returned:
 * ────────────────────────────────────────────────────────────────────────────
 * - ERR_INVALID_ARG  — NULL stream, NULL format string, or NULL message
 * - ERR_IO_FAILED    — fputs, vfprintf, fputc, or fflush returned EOF/error
 *
 * @sa util/log/log_macros.h — ergonomic fire-and-forget macro layer
 * @sa semantics/error.h     — Error codes and error_message()
 */

/**
 * @brief Logging severity levels
 */
typedef enum {
    LOG_INFO,   ///< "[INFO] "  → stdout
    LOG_WARN,   ///< "[WARN] "  → stdout
    LOG_ERROR   ///< "[ERROR] " → stderr
} log_level;

/* ────────────────────────────────────────────────────────────────────────────
   Core low-level API
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Low-level formatted logging with va_list (foundation function)
 *
 * @param stream  Valid open FILE* (NULL → Err(ERR_INVALID_ARG))
 * @param level   Severity level — controls the written prefix only
 * @param fmt     printf-style format string (NULL → Err(ERR_INVALID_ARG))
 * @param args    va_list initialized by caller via va_start()
 * @return Ok(true) on success, Err on failure
 */
static inline result_bool_Error log_vfmt_to(
    borrowed(FILE*)       stream,
    log_level             level,
    borrowed(const char*) fmt,
    va_list               args)
{
    if (!stream) return result_bool_Error_err(ERR_INVALID_ARG);
    if (!fmt)    return result_bool_Error_err(ERR_INVALID_ARG);

    const char* prefix;
    switch (level) {
        case LOG_INFO:  prefix = "[INFO] ";  break;
        case LOG_WARN:  prefix = "[WARN] ";  break;
        case LOG_ERROR: prefix = "[ERROR] "; break;
        default:        prefix = "[????] ";  break;
    }

    if (fputs(prefix, stream) == EOF)
        return result_bool_Error_err(ERR_IO_FAILED);

    if (vfprintf(stream, fmt, args) < 0)
        return result_bool_Error_err(ERR_IO_FAILED);

    if (fputc('\n', stream) == EOF)
        return result_bool_Error_err(ERR_IO_FAILED);

    if (fflush(stream) == EOF)
        return result_bool_Error_err(ERR_IO_FAILED);

    return result_bool_Error_ok(true);
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience wrappers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Formatted logging to an explicit stream
 */
static inline result_bool_Error log_fmt_to(
    borrowed(FILE*)       stream,
    log_level             level,
    borrowed(const char*) fmt,
    ...)
{
    va_list args;
    va_start(args, fmt);
    result_bool_Error res = log_vfmt_to(stream, level, fmt, args);
    va_end(args);
    return res;
}

/**
 * @brief Plain string logging to an explicit stream
 */
static inline result_bool_Error log_to(
    borrowed(FILE*)       stream,
    log_level             level,
    borrowed(const char*) msg)
{
    if (!msg)    return result_bool_Error_err(ERR_INVALID_ARG);
    if (!stream) return result_bool_Error_err(ERR_INVALID_ARG);
    return log_fmt_to(stream, level, "%s", msg);
}

/**
 * @brief Formatted logging to the default stream for the given level
 */
static inline result_bool_Error log_fmt(
    log_level             level,
    borrowed(const char*) fmt,
    ...)
{
    FILE* stream = (level == LOG_ERROR) ? stderr : stdout;
    va_list args;
    va_start(args, fmt);
    result_bool_Error res = log_vfmt_to(stream, level, fmt, args);
    va_end(args);
    return res;
}

/**
 * @brief Plain string logging to the default stream for the given level
 */
static inline result_bool_Error log_msg(
    log_level             level,
    borrowed(const char*) msg)
{
    FILE* stream = (level == LOG_ERROR) ? stderr : stdout;
    return log_to(stream, level, msg);
}

#endif /* CANON_UTIL_LOG_H */
