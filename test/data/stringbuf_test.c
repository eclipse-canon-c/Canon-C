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

/**
 * @file stringbuf_test.c
 * @brief Tests for data/stringbuf.h — fixed-capacity string builder
 *
 * Covers:
 *   - stringbuf_init_buffer()       — zero-alloc stack init; data[0]=='\0', len==0
 *   - stringbuf_init_arena()        — arena-backed init; is_arena_backed==true
 *   - stringbuf_append()            — C string append; NULL s is no-op; full fails
 *   - stringbuf_append_str()        — str_t append; empty str_t no-op
 *   - stringbuf_append_char()       — single char O(1); full boundary
 *   - stringbuf_append_fmt()        — printf-style format; two-pass measure+write
 *   - stringbuf_append_fmt_va()     — va_list variant; original va_list intact
 *   - stringbuf_append_n()          — at most n chars; stops at null byte
 *   - stringbuf_printf              — macro alias for stringbuf_append_fmt
 *   - stringbuf_str()               — null-terminated C string view; "" for NULL
 *   - stringbuf_as_str()            — str_t view; len matches
 *   - stringbuf_as_bytes()          — bytes_t view; len excludes null terminator
 *   - stringbuf_buffer_bytes()      — capacity-wide view
 *   - stringbuf_len()               — current length; 0 for NULL
 *   - stringbuf_capacity()          — total capacity; 0 for NULL
 *   - stringbuf_remaining()         — capacity - len - 1; 0 when full
 *   - stringbuf_is_empty()          — true on fresh init; false after append
 *   - stringbuf_is_full()           — true when no usable bytes remain
 *   - stringbuf_is_arena_backed()   — distinguishes arena vs buffer init
 *   - stringbuf_clear()             — resets to empty; data[0]=='\0'
 *   - stringbuf_truncate()          — shorten; no-op if new_len >= len
 *   - Always-null-terminated        — data[len] == '\0' after every mutation
 *   - Failed appends                — buffer unchanged on capacity exceeded
 *   - NULL safety                   — all query/view functions NULL-safe
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - 64-byte stack-backed StringBuf
 *   - Operations: append_char, append (short string), append_fmt, clear, truncate
 *   - Invariants after every op:
 *       • data[len] == '\0'
 *       • len < capacity
 *       • stringbuf_remaining() == capacity - len - 1
 *       • stringbuf_is_empty() == (len == 0)
 *       • stringbuf_is_full() == (len + 1 >= capacity)
 */

#define CANON_CONTRACT_IMPL

#include "core/primitives/types.h"
#include "core/arena.h"
#include "core/slice.h"
#include "data/stringbuf.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ════════════════════════════════════════════════════════════════════════════
   Unit test build
   ════════════════════════════════════════════════════════════════════════════ */

#ifndef CANON_FUZZING

static int g_failed = 0;

#define EXPECT(cond)                                                  \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n",                      \
                    __FILE__, __LINE__, #cond);                       \
            g_failed++;                                               \
        }                                                             \
    } while (0)

/* ── helper: call stringbuf_append_fmt_va through a wrapper ─────────────── */
static bool fmt_va_helper(StringBuf* sb, const char* fmt, ...)
{
    va_list args;
    bool    ok;
    va_start(args, fmt);
    ok = stringbuf_append_fmt_va(sb, fmt, args);
    va_end(args);
    return ok;
}

/* ── init_buffer ─────────────────────────────────────────────────────────── */
static void test_init_buffer(void)
{
    char buf[64] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    EXPECT(sb.data == buf);
    EXPECT(sb.len      == 0);
    EXPECT(sb.capacity == 64);
    EXPECT(sb.arena    == NULL);
    EXPECT(sb.data[0]  == '\0');

    EXPECT(stringbuf_is_empty(&sb));
    EXPECT(!stringbuf_is_full(&sb));
    EXPECT(!stringbuf_is_arena_backed(&sb));
    EXPECT(stringbuf_len(&sb)      == 0);
    EXPECT(stringbuf_capacity(&sb) == 64);
    EXPECT(stringbuf_remaining(&sb) == 63); /* capacity - 0 - 1 */
}

/* ── init_arena ──────────────────────────────────────────────────────────── */
static void test_init_arena(void)
{
    u8    arena_buf[512];
    Arena arena;
    arena_init(&arena, arena_buf, sizeof(arena_buf));

    StringBuf sb = {0};
    EXPECT(stringbuf_init_arena(&sb, &arena, 128));
    EXPECT(sb.len      == 0);
    EXPECT(sb.capacity == 128);
    EXPECT(sb.arena    == &arena);
    EXPECT(stringbuf_is_empty(&sb)); /* len==0 implies data[0]=='\0' */
    EXPECT(stringbuf_is_arena_backed(&sb));
    EXPECT(!stringbuf_is_arena_backed(NULL));

    /* Arena failure: request more than available */
    StringBuf big = {0};
    EXPECT(!stringbuf_init_arena(&big, &arena, 1024)); /* can't fit */
}

/* ── append (C string) ───────────────────────────────────────────────────── */
static void test_append(void)
{
    char buf[16] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    /* basic append */
    EXPECT(stringbuf_append(&sb, "Hello"));
    EXPECT(sb.len      == 5);
    EXPECT(sb.data[5]  == '\0');
    EXPECT(strcmp(sb.data, "Hello") == 0);

    /* append more */
    EXPECT(stringbuf_append(&sb, ", "));
    EXPECT(sb.len == 7);

    /* NULL s is a no-op, returns true */
    EXPECT(stringbuf_append(&sb, NULL));
    EXPECT(sb.len == 7);

    /* overflow fails gracefully */
    EXPECT(!stringbuf_append(&sb, "this is too long to fit"));
    EXPECT(sb.len == 7); /* unchanged */
    EXPECT(sb.data[7] == '\0');

    /* NULL sb returns false */
    EXPECT(!stringbuf_append(NULL, "x"));
}

/* ── append_str ──────────────────────────────────────────────────────────── */
static void test_append_str(void)
{
    char buf[32] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    str_t s = str_from("World", 5);
    EXPECT(stringbuf_append_str(&sb, s));
    EXPECT(sb.len == 5);
    EXPECT(strcmp(sb.data, "World") == 0);

    /* empty str_t is a no-op */
    EXPECT(stringbuf_append_str(&sb, str_empty()));
    EXPECT(sb.len == 5);

    /* overflow */
    str_t big = str_from("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 30);
    EXPECT(!stringbuf_append_str(&sb, big));
    EXPECT(sb.len == 5);

    /* NULL sb */
    EXPECT(!stringbuf_append_str(NULL, s));
}

/* ── append_char ─────────────────────────────────────────────────────────── */
static void test_append_char(void)
{
    char buf[4] = {0}; /* capacity 4: can hold 3 chars + null */
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    EXPECT(stringbuf_append_char(&sb, 'A'));
    EXPECT(stringbuf_append_char(&sb, 'B'));
    EXPECT(stringbuf_append_char(&sb, 'C'));
    EXPECT(sb.len == 3);
    EXPECT(sb.data[3] == '\0');
    EXPECT(strcmp(sb.data, "ABC") == 0);
    EXPECT(stringbuf_is_full(&sb));

    /* Full — must fail */
    EXPECT(!stringbuf_append_char(&sb, 'D'));
    EXPECT(sb.len == 3);
    EXPECT(sb.data[3] == '\0');

    /* NULL sb */
    EXPECT(!stringbuf_append_char(NULL, 'X'));
}

/* ── append_fmt ──────────────────────────────────────────────────────────── */
static void test_append_fmt(void)
{
    char buf[64] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    EXPECT(stringbuf_append_fmt(&sb, "%d + %d = %d", 1, 2, 3));
    EXPECT(strcmp(sb.data, "1 + 2 = 3") == 0);
    EXPECT(sb.len == 9);
    EXPECT(sb.data[9] == '\0');

    /* Append another format */
    EXPECT(stringbuf_append_fmt(&sb, " (ok)"));
    EXPECT(strcmp(sb.data, "1 + 2 = 3 (ok)") == 0);

    /* Overflow: format would exceed capacity */
    char big_buf[8];
    StringBuf small;
    stringbuf_init_buffer(&small, big_buf, sizeof(big_buf));
    EXPECT(!stringbuf_append_fmt(&small, "%s", "TOOLONG_STRING"));
    EXPECT(small.len == 0); /* unchanged */
    EXPECT(small.data[0] == '\0');

    /* NULL fmt */
    EXPECT(!stringbuf_append_fmt(&sb, NULL));

    /* stringbuf_printf alias */
    stringbuf_clear(&sb);
    EXPECT(stringbuf_printf(&sb, "x=%d", 42));
    EXPECT(strcmp(sb.data, "x=42") == 0);
}

/* ── append_fmt_va ───────────────────────────────────────────────────────── */
static void test_append_fmt_va(void)
{
    char buf[64] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    EXPECT(fmt_va_helper(&sb, "val=%s len=%d", "abc", 3));
    EXPECT(strcmp(sb.data, "val=abc len=3") == 0);
    EXPECT(sb.data[sb.len] == '\0');

    /* NULL sb */
    EXPECT(!fmt_va_helper(NULL, "x"));
}

/* ── append_n ────────────────────────────────────────────────────────────── */
static void test_append_n(void)
{
    char buf[32] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    /* n < strlen(s) — only first n chars copied */
    EXPECT(stringbuf_append_n(&sb, "Hello, World!", 5));
    EXPECT(sb.len == 5);
    EXPECT(strcmp(sb.data, "Hello") == 0);

    stringbuf_clear(&sb);

    /* n > strlen(s) — stops at null byte */
    EXPECT(stringbuf_append_n(&sb, "Hi", 100));
    EXPECT(sb.len == 2);
    EXPECT(strcmp(sb.data, "Hi") == 0);

    /* n == 0 — no-op */
    EXPECT(stringbuf_append_n(&sb, "ignored", 0));
    EXPECT(sb.len == 2);

    /* NULL s — no-op */
    EXPECT(stringbuf_append_n(&sb, NULL, 5));
    EXPECT(sb.len == 2);

    /* overflow */
    stringbuf_clear(&sb);
    EXPECT(!stringbuf_append_n(&sb, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 34));
    EXPECT(sb.len == 0);
}

/* ── views ───────────────────────────────────────────────────────────────── */
static void test_views(void)
{
    char buf[32] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));
    stringbuf_append(&sb, "test");

    /* stringbuf_str */
    const char* s = stringbuf_str(&sb);
    EXPECT(strcmp(s, "test") == 0);
    EXPECT(strcmp(stringbuf_str(NULL), "") == 0);

    /* stringbuf_as_str */
    str_t sv = stringbuf_as_str(&sb);
    EXPECT(sv.len == 4);
    EXPECT(sv.ptr != NULL);
    EXPECT(memcmp(sv.ptr, "test", 4) == 0);

    str_t empty_sv = stringbuf_as_str(NULL);
    EXPECT(empty_sv.ptr == NULL || empty_sv.len == 0);

    /* stringbuf_as_bytes */
    bytes_t bv = stringbuf_as_bytes(&sb);
    EXPECT(bv.len == 4); /* does NOT include null terminator */
    EXPECT(bv.ptr[0] == 't');

    bytes_t empty_bv = stringbuf_as_bytes(NULL);
    EXPECT(empty_bv.len == 0);

    /* stringbuf_buffer_bytes — covers entire capacity */
    bytes_t bbv = stringbuf_buffer_bytes(&sb);
    EXPECT(bbv.len == 32); /* full capacity */
    EXPECT(bbv.ptr == (const u8*)buf);

    bytes_t empty_bbv = stringbuf_buffer_bytes(NULL);
    EXPECT(empty_bbv.len == 0);
}

/* ── queries ─────────────────────────────────────────────────────────────── */
static void test_queries(void)
{
    char buf[8] = {0}; /* capacity 8, usable 7 */
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    EXPECT(stringbuf_len(&sb)       == 0);
    EXPECT(stringbuf_capacity(&sb)  == 8);
    EXPECT(stringbuf_remaining(&sb) == 7);
    EXPECT(stringbuf_is_empty(&sb));
    EXPECT(!stringbuf_is_full(&sb));

    stringbuf_append(&sb, "ABC"); /* len = 3 */
    EXPECT(stringbuf_len(&sb)       == 3);
    EXPECT(stringbuf_remaining(&sb) == 4); /* 8 - 3 - 1 */
    EXPECT(!stringbuf_is_empty(&sb));
    EXPECT(!stringbuf_is_full(&sb));

    stringbuf_append(&sb, "DEFG"); /* len = 7 — fully used */
    EXPECT(stringbuf_len(&sb)       == 7);
    EXPECT(stringbuf_remaining(&sb) == 0);
    EXPECT(stringbuf_is_full(&sb));
    EXPECT(!stringbuf_is_empty(&sb));

    /* NULL safety */
    EXPECT(stringbuf_len(NULL)       == 0);
    EXPECT(stringbuf_capacity(NULL)  == 0);
    EXPECT(stringbuf_remaining(NULL) == 0);
    EXPECT(stringbuf_is_empty(NULL));
    EXPECT(stringbuf_is_full(NULL));
}

/* ── clear ───────────────────────────────────────────────────────────────── */
static void test_clear(void)
{
    char buf[32] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    stringbuf_append(&sb, "something");
    EXPECT(sb.len == 9);

    stringbuf_clear(&sb);
    EXPECT(sb.len    == 0);
    EXPECT(sb.data[0] == '\0');
    EXPECT(stringbuf_is_empty(&sb));
    EXPECT(stringbuf_remaining(&sb) == 31);

    /* Can append again after clear */
    EXPECT(stringbuf_append(&sb, "fresh"));
    EXPECT(sb.len == 5);

    /* NULL safety */
    stringbuf_clear(NULL); /* must not crash */
}

/* ── truncate ────────────────────────────────────────────────────────────── */
static void test_truncate(void)
{
    char buf[32] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    stringbuf_append(&sb, "Hello, World!");
    EXPECT(sb.len == 13);

    /* Truncate to 5 */
    stringbuf_truncate(&sb, 5);
    EXPECT(sb.len == 5);
    EXPECT(sb.data[5] == '\0');
    EXPECT(strcmp(sb.data, "Hello") == 0);

    /* Truncate to same length — no-op */
    stringbuf_truncate(&sb, 5);
    EXPECT(sb.len == 5);

    /* Truncate to larger — no-op */
    stringbuf_truncate(&sb, 100);
    EXPECT(sb.len == 5);

    /* Truncate to 0 */
    stringbuf_truncate(&sb, 0);
    EXPECT(sb.len == 0);
    EXPECT(sb.data[0] == '\0');

    /* NULL safety */
    stringbuf_truncate(NULL, 0); /* must not crash */
}

/* ── always null-terminated ──────────────────────────────────────────────── */
static void test_always_null_terminated(void)
{
    char buf[8] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));

    /* After every successful append, data[len] == '\0' */
    const char* words[] = {"A", "BB", "CCC", "X"};
    for (usize i = 0; i < 4; i++) {
        stringbuf_append(&sb, words[i]);
        EXPECT(sb.data[sb.len] == '\0');
    }

    /* After failed append */
    EXPECT(!stringbuf_append(&sb, "OVERFLOW"));
    EXPECT(sb.data[sb.len] == '\0');

    /* After clear */
    stringbuf_clear(&sb);
    EXPECT(sb.data[0] == '\0');

    /* After truncate */
    stringbuf_append(&sb, "Hello");
    stringbuf_truncate(&sb, 2);
    EXPECT(sb.data[2] == '\0');
}

/* ── failed append leaves buffer unchanged ───────────────────────────────── */
static void test_failed_append_unchanged(void)
{
    char buf[8] = {0};
    StringBuf sb;
    stringbuf_init_buffer(&sb, buf, sizeof(buf));
    stringbuf_append(&sb, "AB");

    usize saved_len = sb.len;
    char  saved_content[8];
    memcpy(saved_content, sb.data, sizeof(saved_content));

    /* All these should fail and leave sb unchanged */
    EXPECT(!stringbuf_append(&sb, "TOOOOOLONG"));
    EXPECT(sb.len == saved_len);
    EXPECT(memcmp(sb.data, saved_content, saved_len) == 0);
    EXPECT(sb.data[saved_len] == '\0');

    EXPECT(!stringbuf_append_str(&sb, str_from("TOOOOOLONG", 10)));
    EXPECT(sb.len == saved_len);

    EXPECT(!stringbuf_append_fmt(&sb, "%s", "TOOOOOLONG"));
    EXPECT(sb.len == saved_len);
    EXPECT(sb.data[saved_len] == '\0');
}

/* ── Suppress unused ─────────────────────────────────────────────────────── */
static void stringbuf_suppress_unused(void)
{
    /* All public functions are exercised above; this is a safety net
     * for any that the compiler determines unreachable in this TU. */
    (void)stringbuf_init_arena;
    (void)stringbuf_append_fmt_va;
}

/* ── Unit test entry point ───────────────────────────────────────────────── */
int main(void)
{
    (void)stringbuf_suppress_unused;

    test_init_buffer();
    test_init_arena();
    test_append();
    test_append_str();
    test_append_char();
    test_append_fmt();
    test_append_fmt_va();
    test_append_n();
    test_views();
    test_queries();
    test_clear();
    test_truncate();
    test_always_null_terminated();
    test_failed_append_unchanged();

    if (g_failed == 0) {
        printf("OK  stringbuf_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr,
            "FAILED  stringbuf_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

static void stringbuf_fuzz_suppress_unused(void)
{
    /* Not used in fuzz path */
    (void)stringbuf_init_arena;
    (void)stringbuf_append_str;
    (void)stringbuf_append_fmt_va;
    (void)stringbuf_append_n;
    (void)stringbuf_as_str;
    (void)stringbuf_as_bytes;
    (void)stringbuf_buffer_bytes;
    (void)stringbuf_remaining;
    (void)stringbuf_is_arena_backed;
    (void)stringbuf_capacity;
    (void)stringbuf_str;
}

/*
 * Fuzz a 64-byte stack-backed StringBuf.
 *
 * Input layout — each byte is one operation:
 *   high nibble: op (0–5)
 *   low  nibble: aux value (0–15)
 *
 * Operations:
 *   0 — append_char(printable[aux % 26])
 *   1 — append("STR" repeated aux+1 times, clamped to 8 chars)
 *   2 — append_fmt("%d", (int)aux)
 *   3 — clear()
 *   4 — truncate(aux * 4)      — may be no-op if > len
 *   5 — append_char('\0')      — edge case: explicit null byte attempt
 *
 * Invariants after every operation:
 *   - data[len] == '\0'
 *   - len < capacity
 *   - stringbuf_is_empty() == (len == 0)
 *   - stringbuf_is_full() == (len + 1 >= capacity)
 */

#define FUZZ_CAP 64

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    (void)stringbuf_fuzz_suppress_unused;

    char      backing[FUZZ_CAP];
    StringBuf sb;
    stringbuf_init_buffer(&sb, backing, FUZZ_CAP);

    #define CHECK_INVARIANTS()                                                  \
        do {                                                                    \
            /* null terminator */                                               \
            if (sb.data[sb.len] != '\0')          __builtin_trap();            \
            /* len < capacity */                                                \
            if (sb.len >= sb.capacity)             __builtin_trap();            \
            /* is_empty */                                                      \
            if (stringbuf_is_empty(&sb) != (sb.len == 0)) __builtin_trap();   \
            /* is_full */                                                       \
            bool _exp_full = (sb.len + 1u >= sb.capacity);                     \
            if (stringbuf_is_full(&sb) != _exp_full)      __builtin_trap();    \
        } while (0)

    CHECK_INVARIANTS();

    for (usize i = 0; i < size; i++) {
        u8    byte = data[i];
        u8    op   = (u8)(byte >> 4u) % 6u;
        usize aux  = (usize)(byte & 0x0Fu);

        switch (op) {
            case 0: {
                char c = (char)('a' + (aux % 26));
                stringbuf_append_char(&sb, c);
                break;
            }
            case 1: {
                /* Short fixed string, 1..8 chars */
                static const char seg[] = "STRINGSTR";
                usize n = (aux % 8) + 1;
                stringbuf_append_n(&sb, seg, n);
                break;
            }
            case 2: {
                stringbuf_append_fmt(&sb, "%u", (unsigned)aux);
                break;
            }
            case 3: {
                stringbuf_clear(&sb);
                break;
            }
            case 4: {
                stringbuf_truncate(&sb, aux * 4u);
                break;
            }
            case 5: {
                /* '\0' is a valid char value; append_char should handle it
                 * (it will fit as long as space remains, written into the
                 * buffer, then the invariant null is placed after it) */
                stringbuf_append_char(&sb, '\0');
                break;
            }
            default:
                break;
        }

        CHECK_INVARIANTS();
    }

    #undef CHECK_INVARIANTS
    #undef FUZZ_CAP

    return 0;
}

#endif /* CANON_FUZZING */
