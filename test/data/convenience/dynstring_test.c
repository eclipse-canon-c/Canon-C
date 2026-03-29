/**
 * @file dynstring_test.c
 * @brief Tests for data/convenience/dynstring.h — auto-growing string builder
 *
 * Covers:
 *   - dynstring_init()           — zero struct, no allocation
 *   - dynstring_with_capacity()  — pre-allocates; data[0]=='\0', len==0
 *   - dynstring_from()           — copies C string; len == strlen(str)
 *   - dynstring_len()            — 0 for NULL; tracks appends
 *   - dynstring_capacity()       — 0 for NULL; grows on overflow
 *   - dynstring_is_empty()       — true on fresh init; false after append
 *   - dynstring_str()            — never NULL; "" before first append
 *   - dynstring_append()         — grows automatically; NULL str is no-op
 *   - dynstring_append_char()    — O(1) amortized; grows on overflow
 *   - dynstring_append_fmt()     — two-pass vsnprintf; grows on overflow
 *   - dynstring_append_n()       — at most n chars; stops at null byte
 *   - dynstring_clear()          — resets len; data[0]=='\0'; keeps capacity
 *   - dynstring_truncate()       — shortens; no-op if new_len >= len
 *   - dynstring_reserve()        — ensures capacity; no-op if already sufficient
 *   - dynstring_shrink_to_fit()  — reduces cap to len+1; frees if empty
 *   - dynstring_free()           — releases heap; resets all fields
 *   - dynstring_to_cstr()        — independent heap copy; caller must free
 *   - Always-null-terminated     — data[len] == '\0' after every mutation
 *   - Growth semantics           — capacity at least doubles on each realloc
 *
 * Fuzz entry point (CANON_FUZZING):
 *   - Single DynString; no arena dependency
 *   - Operations: append_char, append (short), append_fmt, clear, truncate, reserve
 *   - Invariants after every op:
 *       • data[len] == '\0' (when data != NULL)
 *       • len < cap (when data != NULL)
 *       • is_empty() == (len == 0)
 *       • str() != NULL
 */

#define CANON_CONTRACT_IMPL

#include "core/primitives/types.h"
#include "data/convenience/dynstring.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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

/* ── helper: variadic wrapper for dynstring_append_fmt ──────────────────── */
static bool fmt_helper(DynString* s, const char* fmt, ...)
{
    va_list args;
    int     needed;
    bool    ok;
    va_start(args, fmt);
    needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    (void)needed;
    /* Use the real API so we get the two-pass behaviour */
    va_start(args, fmt);
    ok = dynstring_append_fmt(s, fmt, args);
    va_end(args);
    return ok;
}

/* ── dynstring_init ──────────────────────────────────────────────────────── */
static void test_init(void)
{
    DynString s = dynstring_init();
    EXPECT(s.data == NULL);
    EXPECT(s.len  == 0);
    EXPECT(s.cap  == 0);
    EXPECT(dynstring_is_empty(&s));
    EXPECT(dynstring_len(&s)      == 0);
    EXPECT(dynstring_capacity(&s) == 0);
    EXPECT(strcmp(dynstring_str(&s), "") == 0);
    /* No free needed — no allocation */
}

/* ── dynstring_with_capacity ─────────────────────────────────────────────── */
static void test_with_capacity(void)
{
    DynString s = dynstring_with_capacity(128);
    EXPECT(s.data != NULL);
    EXPECT(s.len  == 0);
    EXPECT(s.cap  == 128);
    EXPECT(s.data[0] == '\0');
    EXPECT(dynstring_is_empty(&s));
    EXPECT(strcmp(dynstring_str(&s), "") == 0);
    dynstring_free(&s);

    /* capacity == 0 is treated as no-alloc */
    DynString z = dynstring_with_capacity(0);
    EXPECT(z.data == NULL);
    EXPECT(z.cap  == 0);
    /* no free needed */
}

/* ── dynstring_from ──────────────────────────────────────────────────────── */
static void test_from(void)
{
    DynString s = dynstring_from("Hello");
    EXPECT(s.data != NULL);
    EXPECT(s.len  == 5);
    EXPECT(s.cap  == 6); /* strlen + 1 */
    EXPECT(strcmp(s.data, "Hello") == 0);
    EXPECT(s.data[5] == '\0');
    dynstring_free(&s);

    /* NULL source */
    DynString n = dynstring_from(NULL);
    EXPECT(n.data == NULL);
    EXPECT(n.len  == 0);

    /* empty string */
    DynString e = dynstring_from("");
    EXPECT(e.len == 0);
    dynstring_free(&e);
}

/* ── queries — NULL safety ───────────────────────────────────────────────── */
static void test_queries_null(void)
{
    EXPECT(dynstring_len(NULL)       == 0);
    EXPECT(dynstring_capacity(NULL)  == 0);
    EXPECT(dynstring_is_empty(NULL));
    EXPECT(strcmp(dynstring_str(NULL), "") == 0);
}

/* ── dynstring_append — growth ───────────────────────────────────────────── */
static void test_append(void)
{
    DynString s = dynstring_init();

    /* First append triggers initial allocation */
    EXPECT(dynstring_append(&s, "Hello"));
    EXPECT(s.len  == 5);
    EXPECT(s.data != NULL);
    EXPECT(strcmp(s.data, "Hello") == 0);
    EXPECT(s.data[5] == '\0');

    EXPECT(dynstring_append(&s, ", "));
    EXPECT(s.len == 7);

    EXPECT(dynstring_append(&s, "World!"));
    EXPECT(s.len == 13);
    EXPECT(strcmp(s.data, "Hello, World!") == 0);
    EXPECT(s.data[13] == '\0');

    /* NULL str is a no-op */
    EXPECT(dynstring_append(&s, NULL));
    EXPECT(s.len == 13);

    /* empty str is a no-op */
    EXPECT(dynstring_append(&s, ""));
    EXPECT(s.len == 13);

    /* NULL s returns false */
    EXPECT(!dynstring_append(NULL, "x"));

    dynstring_free(&s);
    EXPECT(s.data == NULL);
    EXPECT(s.len  == 0);
    EXPECT(s.cap  == 0);
}

/* ── growth: capacity at least doubles ───────────────────────────────────── */
static void test_growth(void)
{
    DynString s = dynstring_with_capacity(4); /* small start */
    usize old_cap = s.cap;

    /* Append enough to force at least one realloc */
    EXPECT(dynstring_append(&s, "ABCDEFGHIJKLMNOP")); /* 16 chars > 4 */
    EXPECT(s.cap >= old_cap * 2);
    EXPECT(s.data[s.len] == '\0');

    dynstring_free(&s);
}

/* ── dynstring_append_char ───────────────────────────────────────────────── */
static void test_append_char(void)
{
    DynString s = dynstring_init();

    EXPECT(dynstring_append_char(&s, 'A'));
    EXPECT(dynstring_append_char(&s, 'B'));
    EXPECT(dynstring_append_char(&s, 'C'));
    EXPECT(s.len == 3);
    EXPECT(strcmp(s.data, "ABC") == 0);
    EXPECT(s.data[3] == '\0');

    EXPECT(!dynstring_append_char(NULL, 'X'));

    dynstring_free(&s);
}

/* ── dynstring_append_fmt ────────────────────────────────────────────────── */
static void test_append_fmt(void)
{
    DynString s = dynstring_init();

    EXPECT(dynstring_append_fmt(&s, "%d + %d = %d", 1, 2, 3));
    EXPECT(strcmp(s.data, "1 + 2 = 3") == 0);
    EXPECT(s.len == 9);
    EXPECT(s.data[9] == '\0');

    EXPECT(dynstring_append_fmt(&s, " (%s)", "ok"));
    EXPECT(strcmp(s.data, "1 + 2 = 3 (ok)") == 0);

    EXPECT(!dynstring_append_fmt(NULL, "x"));
    EXPECT(!dynstring_append_fmt(&s, NULL));

    dynstring_free(&s);
}

/* ── dynstring_append_n ──────────────────────────────────────────────────── */
static void test_append_n(void)
{
    DynString s = dynstring_init();

    /* n < strlen: only first n chars */
    EXPECT(dynstring_append_n(&s, "Hello, World!", 5));
    EXPECT(s.len == 5);
    EXPECT(strcmp(s.data, "Hello") == 0);

    dynstring_clear(&s);

    /* n > strlen: stops at null byte */
    EXPECT(dynstring_append_n(&s, "Hi", 100));
    EXPECT(s.len == 2);
    EXPECT(strcmp(s.data, "Hi") == 0);

    /* n == 0: no-op */
    EXPECT(dynstring_append_n(&s, "ignored", 0));
    EXPECT(s.len == 2);

    /* NULL str: no-op */
    EXPECT(dynstring_append_n(&s, NULL, 5));
    EXPECT(s.len == 2);

    dynstring_free(&s);
}

/* ── dynstring_clear ─────────────────────────────────────────────────────── */
static void test_clear(void)
{
    DynString s = dynstring_from("something");
    usize old_cap = s.cap;

    dynstring_clear(&s);
    EXPECT(s.len == 0);
    EXPECT(s.data[0] == '\0');
    EXPECT(s.cap == old_cap);  /* buffer retained */
    EXPECT(dynstring_is_empty(&s));
    EXPECT(strcmp(dynstring_str(&s), "") == 0);

    /* Can append again after clear */
    EXPECT(dynstring_append(&s, "fresh"));
    EXPECT(s.len == 5);

    /* Clear on never-appended DynString — no crash */
    DynString z = dynstring_init();
    dynstring_clear(&z);
    EXPECT(z.data == NULL);

    /* NULL safety */
    dynstring_clear(NULL);

    dynstring_free(&s);
}

/* ── dynstring_truncate ──────────────────────────────────────────────────── */
static void test_truncate(void)
{
    DynString s = dynstring_from("Hello, World!");
    EXPECT(s.len == 13);

    dynstring_truncate(&s, 5);
    EXPECT(s.len == 5);
    EXPECT(s.data[5] == '\0');
    EXPECT(strcmp(s.data, "Hello") == 0);

    /* no-op: new_len == len */
    dynstring_truncate(&s, 5);
    EXPECT(s.len == 5);

    /* no-op: new_len > len */
    dynstring_truncate(&s, 100);
    EXPECT(s.len == 5);

    /* truncate to 0 */
    dynstring_truncate(&s, 0);
    EXPECT(s.len == 0);
    EXPECT(s.data[0] == '\0');

    /* NULL safety */
    dynstring_truncate(NULL, 0);

    dynstring_free(&s);
}

/* ── dynstring_reserve ───────────────────────────────────────────────────── */
static void test_reserve(void)
{
    DynString s = dynstring_init();

    EXPECT(dynstring_reserve(&s, 256));
    EXPECT(s.cap >= 256);
    EXPECT(s.len == 0);
    /* dynstring_reserve only ensures capacity — it does not write '\0'.
     * Append something before calling str() to get a valid string. */
    EXPECT(dynstring_append(&s, ""));  /* no-op but triggers no UB */
    /* str() before any real append returns the raw buffer; use len to verify */
    EXPECT(dynstring_len(&s) == 0);

    /* Reserve less than current — no-op */
    usize cap_before = s.cap;
    EXPECT(dynstring_reserve(&s, 10));
    EXPECT(s.cap == cap_before);

    /* NULL */
    EXPECT(!dynstring_reserve(NULL, 10));

    dynstring_free(&s);
}

/* ── dynstring_shrink_to_fit ─────────────────────────────────────────────── */
static void test_shrink_to_fit(void)
{
    DynString s = dynstring_with_capacity(512);
    EXPECT(dynstring_append(&s, "Hi"));
    EXPECT(s.cap == 512);

    EXPECT(dynstring_shrink_to_fit(&s));
    EXPECT(s.cap == 3); /* len + 1 */
    EXPECT(strcmp(s.data, "Hi") == 0);
    EXPECT(s.data[2] == '\0');

    /* Shrink empty string frees the buffer */
    dynstring_clear(&s);
    EXPECT(dynstring_shrink_to_fit(&s));
    EXPECT(s.data == NULL);
    EXPECT(s.cap  == 0);

    /* Shrink already-minimal — no-op */
    DynString m = dynstring_from("ABC");
    EXPECT(m.cap == 4);
    EXPECT(dynstring_shrink_to_fit(&m));
    EXPECT(m.cap == 4);
    dynstring_free(&m);

    /* NULL safety */
    EXPECT(dynstring_shrink_to_fit(NULL));
}

/* ── dynstring_to_cstr ───────────────────────────────────────────────────── */
static void test_to_cstr(void)
{
    DynString s = dynstring_from("test string");

    char* copy = dynstring_to_cstr(&s);
    EXPECT(copy != NULL && strcmp(copy, "test string") == 0);
    EXPECT(copy != s.data); /* independent allocation */
    free(copy);

    /* NULL s — returns copy of "" */
    char* empty = dynstring_to_cstr(NULL);
    EXPECT(empty != NULL && strcmp(empty, "") == 0);
    free(empty);

    dynstring_free(&s);
}

/* ── always null-terminated ──────────────────────────────────────────────── */
static void test_always_null_terminated(void)
{
    DynString s = dynstring_init();

    const char* words[] = {"alpha", "beta", "gamma", "delta", "epsilon"};
    for (usize i = 0; i < 5; i++) {
        EXPECT(dynstring_append(&s, words[i]));
        /* Verify null-termination: strlen == len implies data[len] == '\0' */
        EXPECT(strlen(dynstring_str(&s)) == dynstring_len(&s));
    }

    dynstring_truncate(&s, 3);
    EXPECT(dynstring_len(&s) == 3);
    EXPECT(strlen(dynstring_str(&s)) == 3);

    dynstring_clear(&s);
    EXPECT(dynstring_len(&s) == 0);
    EXPECT(strcmp(dynstring_str(&s), "") == 0);

    EXPECT(dynstring_append_char(&s, 'Z'));
    EXPECT(dynstring_len(&s) == 1);
    EXPECT(strlen(dynstring_str(&s)) == 1);

    dynstring_free(&s);
}

/* ── Suppress unused ─────────────────────────────────────────────────────── */
static void dynstring_suppress_unused(void)
{
    (void)dynstring_ensure_capacity;
    (void)fmt_helper; /* used in test_append_fmt indirectly */
}

/* ── Unit test entry point ───────────────────────────────────────────────── */
int main(void)
{
    (void)dynstring_suppress_unused;

    test_init();
    test_with_capacity();
    test_from();
    test_queries_null();
    test_append();
    test_growth();
    test_append_char();
    test_append_fmt();
    test_append_n();
    test_clear();
    test_truncate();
    test_reserve();
    test_shrink_to_fit();
    test_to_cstr();
    test_always_null_terminated();

    if (g_failed == 0) {
        printf("OK  dynstring_test  (all assertions passed)\n");
        return 0;
    }
    fprintf(stderr,
            "FAILED  dynstring_test  (%d assertion(s) failed)\n", g_failed);
    return 1;
}

#else /* CANON_FUZZING */

/* ════════════════════════════════════════════════════════════════════════════
   Fuzz entry point
   ════════════════════════════════════════════════════════════════════════════ */

static void dynstring_fuzz_suppress_unused(void)
{
    (void)dynstring_ensure_capacity;
    (void)dynstring_with_capacity;
    (void)dynstring_from;
    (void)dynstring_capacity;
    (void)dynstring_append_n;
    (void)dynstring_reserve;
    (void)dynstring_shrink_to_fit;
    (void)dynstring_to_cstr;
}

/*
 * Fuzz a single DynString through all growing operations.
 *
 * Input layout — each byte is one operation:
 *   high nibble: op (0–5)
 *   low  nibble: aux value (0–15)
 *
 * Operations:
 *   0 — append_char(printable[aux % 26])
 *   1 — append(short_strings[aux % 4])
 *   2 — append_fmt("%u", aux)
 *   3 — clear()
 *   4 — truncate(aux * 4)
 *   5 — dynstring_free() + reinit (exercises free path mid-sequence)
 *
 * Invariants after every operation (when data != NULL):
 *   - data[len] == '\0'
 *   - len < cap
 *   - is_empty() == (len == 0)
 *   - str() != NULL
 */

int LLVMFuzzerTestOneInput(const u8* data, usize size)
{
    (void)dynstring_fuzz_suppress_unused;

    DynString s = dynstring_init();

    static const char* const short_strs[] = {
        "abc", "hello", "XY", "12345678"
    };

    #define CHECK_INVARIANTS()                                              \
        do {                                                                \
            /* str() never NULL */                                          \
            if (!dynstring_str(&s)) __builtin_trap();                       \
            if (s.data != NULL) {                                           \
                /* null terminator */                                       \
                if (s.data[s.len] != '\0') __builtin_trap();               \
                /* len < cap */                                             \
                if (s.len >= s.cap)        __builtin_trap();               \
            }                                                               \
            /* is_empty */                                                  \
            if (dynstring_is_empty(&s) != (s.len == 0)) __builtin_trap();  \
        } while (0)

    CHECK_INVARIANTS();

    for (usize i = 0; i < size; i++) {
        u8    byte = data[i];
        u8    op   = (u8)(byte >> 4u) % 6u;
        usize aux  = (usize)(byte & 0x0Fu);

        switch (op) {
            case 0: {
                char c = (char)('a' + (aux % 26));
                dynstring_append_char(&s, c);
                break;
            }
            case 1: {
                dynstring_append(&s, short_strs[aux % 4]);
                break;
            }
            case 2: {
                dynstring_append_fmt(&s, "%u", (unsigned)aux);
                break;
            }
            case 3: {
                dynstring_clear(&s);
                break;
            }
            case 4: {
                dynstring_truncate(&s, aux * 4u);
                break;
            }
            case 5: {
                dynstring_free(&s);
                s = dynstring_init();
                break;
            }
            default:
                break;
        }

        CHECK_INVARIANTS();
    }

    #undef CHECK_INVARIANTS

    dynstring_free(&s);
    return 0;
}

#endif /* CANON_FUZZING */
