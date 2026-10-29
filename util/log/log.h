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
/* cppcheck-suppress misra-c2012-21.6 ; MISRA-DEV-016 */
#include <stdio.h>

#include "core/primitives/types.h"
#include "core/ownership.h"
#include "semantics/result/result.h"
#include "semantics/error.h"

/* ────────────────────────────────────────────────────────────────────────────
   result_bool_Error instantiation
   ──────────────────────────────────────────────────────────────────────────

   The instantiation is written with _Bool, so the generated name is
   result__Bool_Error in every standard. Written with bool it would be the
   same in C99 to C17, where bool is a macro for _Bool, but not in C23,
   where bool is a keyword and pastes as written (VERIFY-044).

   Guard: only instantiate if not already provided by another header. */

#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
#define CANON_RESULT_BOOL_ERROR_DEFINED

#include <stdbool.h>

/* Instantiate exactly as every other canon header does, under the shared
 * guard, so the struct tag is declared at most once per translation unit. */
/* cppcheck-suppress misra-c2012-19.2 ; MISRA-DEV-014 */
CANON_RESULT(_Bool, Error)

#endif /* CANON_RESULT_BOOL_ERROR_DEFINED */

/* log.h's public API spells this type `result_bool_Error`, but CANON_RESULT
 * mangles the `bool` keyword through `_Bool`, yielding `result__Bool_Error`.
 * Use the project's established compatibility shim, guarded by
 * CANON_RESULT_BOOL_ERROR_COMPAT exactly as algo/fold/fold.h does, so the
 * alias is declared once per translation unit no matter how many canon
 * headers a user combines.
 *
 * Previously log.h instantiated a SEPARATE type
 * (`CANON_RESULT(canon_bool_, Error)`) and aliased that to this name under
 * the instantiation guard, which made log.h mutually exclusive with every
 * other header providing result_bool_Error. */
#ifndef CANON_RESULT_BOOL_ERROR_COMPAT
#define CANON_RESULT_BOOL_ERROR_COMPAT
typedef result__Bool_Error result_bool_Error;
#define result_bool_Error_ok         result__Bool_Error_ok
#define result_bool_Error_err        result__Bool_Error_err
#define result_bool_Error_is_ok      result__Bool_Error_is_ok
#define result_bool_Error_is_err     result__Bool_Error_is_err
#define result_bool_Error_get_ok     result__Bool_Error_get_ok
#define result_bool_Error_get_err    result__Bool_Error_get_err
#define result_bool_Error_unwrap     result__Bool_Error_unwrap
#define result_bool_Error_unwrap_err result__Bool_Error_unwrap_err
#define result_bool_Error_unwrap_or  result__Bool_Error_unwrap_or
#define result_bool_Error_expect     result__Bool_Error_expect
#define result_bool_Error_map        result__Bool_Error_map
#define result_bool_Error_map_err    result__Bool_Error_map_err
#define result_bool_Error_and_then   result__Bool_Error_and_then
#define result_bool_Error_or_else    result__Bool_Error_or_else
#define result_bool_Error_and        result__Bool_Error_and
#define result_bool_Error_or         result__Bool_Error_or
#define result_bool_Error_eq         result__Bool_Error_eq
#endif /* CANON_RESULT_BOOL_ERROR_COMPAT */

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
    if (!stream) { return result_bool_Error_err(ERR_INVALID_ARG); }
    if (!fmt)    { return result_bool_Error_err(ERR_INVALID_ARG); }

    const char* prefix;
    switch (level) {
        case LOG_INFO:  prefix = "[INFO] ";  break;
        case LOG_WARN:  prefix = "[WARN] ";  break;
        case LOG_ERROR: prefix = "[ERROR] "; break;
        default:        prefix = "[????] ";  break;
    }

    if (fputs(prefix, stream) == EOF) {
        return result_bool_Error_err(ERR_IO_FAILED);
    }

    if (vfprintf(stream, fmt, args) < 0) {
        return result_bool_Error_err(ERR_IO_FAILED);
    }

    if (fputc('\n', stream) == EOF) {
        return result_bool_Error_err(ERR_IO_FAILED);
    }

    if (fflush(stream) == EOF) {
        return result_bool_Error_err(ERR_IO_FAILED);
    }

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
    if (!msg)    { return result_bool_Error_err(ERR_INVALID_ARG); }
    if (!stream) { return result_bool_Error_err(ERR_INVALID_ARG); }
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
