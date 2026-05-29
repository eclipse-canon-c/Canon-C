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
 * @file file_test.c
 * @brief Unit tests for util/file.h
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   file_exists            — nonexistent, existing file
 *   file_write_all         — write success, byte count, empty content
 *   file_read_all_arena    — read back, empty file, large file, arena exhaustion
 *   file_read_all          — heap-backed read, must free with str_free
 *   file_write_all_atomic  — atomic write + verify, path too long
 *   round-trip             — write → read → verify content
 *   overwrite              — second write replaces first
 *
 * Contract note
 * ───────────────────────────────────────────────────────────────────────────
 *   NULL input to all file_* functions is a contract violation
 *   (require_msg → abort). This is not tested here — contract violations
 *   are tested separately via fork/signal in contract_test.c patterns.
 *   I/O failures (file not found, arena exhausted) are data errors
 *   returned as None or Err.
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   Tests create temporary files in the current working directory with
 *   a "canon_test_" prefix and remove them in cleanup. Loop variables
 *   are declared before the loop body (C99).
 */

#define CANON_CONTRACT_IMPL
#include "util/file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Portability: unused-function suppression
 * ====================================================================== */

#if defined(__GNUC__) || defined(__clang__)
    #define CANON_MAYBE_UNUSED __attribute__((unused))
#else
    #define CANON_MAYBE_UNUSED
#endif

/* =========================================================================
 * Minimal test framework
 * ====================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) static CANON_MAYBE_UNUSED void test_##name(void)
#define RUN(name)  do { test_##name(); } while (0)

#define EXPECT(expr)                                                \
    do {                                                            \
        if (expr) {                                                 \
            g_pass++;                                               \
        } else {                                                    \
            g_fail++;                                               \
            fprintf(stderr, "FAIL  %s:%d  %s\n",                   \
                    __FILE__, __LINE__, #expr);                     \
        }                                                           \
    } while (0)

/* =========================================================================
 * Test file helpers
 * ====================================================================== */

static const char* TEST_FILE       = "canon_test_file.tmp";
static const char* TEST_ATOMIC     = "canon_test_atomic.tmp";
static const char* TEST_NONEXIST   = "canon_test_nonexistent_file_xyz.tmp";

static void cleanup_test_files(void) {
    remove(TEST_FILE);
    remove(TEST_ATOMIC);
    remove("canon_test_atomic.tmp.tmp");
}

/* =========================================================================
 * file_exists
 * ====================================================================== */

TEST(exists_nonexistent) {
    remove(TEST_NONEXIST);
    EXPECT(file_exists(TEST_NONEXIST) == false);
}

TEST(exists_real_file) {
    result_usize_Error r = file_write_all(TEST_FILE, "hello");
    EXPECT(result_usize_Error_is_ok(r));
    EXPECT(file_exists(TEST_FILE) == true);
    remove(TEST_FILE);
    EXPECT(file_exists(TEST_FILE) == false);
}

/* =========================================================================
 * file_write_all — success and byte count
 * ====================================================================== */

TEST(write_success) {
    const char* content = "Hello, Canon-C!";
    result_usize_Error r = file_write_all(TEST_FILE, content);

    EXPECT(result_usize_Error_is_ok(r));
    EXPECT(result_usize_Error_unwrap(r) == str_len(content));

    remove(TEST_FILE);
}

TEST(write_empty) {
    result_usize_Error r = file_write_all(TEST_FILE, "");

    EXPECT(result_usize_Error_is_ok(r));
    EXPECT(result_usize_Error_unwrap(r) == 0);

    EXPECT(file_exists(TEST_FILE) == true);
    remove(TEST_FILE);
}

/* =========================================================================
 * file_read_all_arena — nonexistent file
 * ====================================================================== */

TEST(read_arena_nonexistent) {
    u8 buf[256];
    Arena arena;
    arena_init(&arena, buf, sizeof(buf));

    remove(TEST_NONEXIST);
    EXPECT(option_charp_is_none(file_read_all_arena(TEST_NONEXIST, &arena)));
}

/* =========================================================================
 * Round trip: write → read_arena → verify
 * ====================================================================== */

TEST(round_trip_arena) {
    const char* content = "The quick brown fox jumps over the lazy dog.";
    u8 buf[4096];
    Arena arena;

    result_usize_Error wr = file_write_all(TEST_FILE, content);
    EXPECT(result_usize_Error_is_ok(wr));

    arena_init(&arena, buf, sizeof(buf));
    option_charp rd = file_read_all_arena(TEST_FILE, &arena);
    EXPECT(option_charp_is_some(rd));

    char* got = option_charp_unwrap(rd);
    EXPECT(strcmp(got, content) == 0);

    remove(TEST_FILE);
}

/* =========================================================================
 * Round trip: empty file
 * ====================================================================== */

TEST(round_trip_empty) {
    u8 buf[256];
    Arena arena;

    file_write_all(TEST_FILE, "");

    arena_init(&arena, buf, sizeof(buf));
    option_charp rd = file_read_all_arena(TEST_FILE, &arena);
    EXPECT(option_charp_is_some(rd));
    EXPECT(strcmp(option_charp_unwrap(rd), "") == 0);

    remove(TEST_FILE);
}

/* =========================================================================
 * Round trip: larger content
 * ====================================================================== */

TEST(round_trip_large) {
    char content[10240];
    int i;
    for (i = 0; i < 10239; i++) {
        content[i] = 'A' + (char)(i % 26);
    }
    content[10239] = '\0';

    u8 arena_buf[12288];
    Arena arena;

    file_write_all(TEST_FILE, content);

    arena_init(&arena, arena_buf, sizeof(arena_buf));
    option_charp rd = file_read_all_arena(TEST_FILE, &arena);
    EXPECT(option_charp_is_some(rd));
    EXPECT(strcmp(option_charp_unwrap(rd), content) == 0);

    remove(TEST_FILE);
}

/* =========================================================================
 * Arena exhaustion — arena too small for file
 * ====================================================================== */

TEST(arena_exhaustion) {
    char content[101];
    int i;
    for (i = 0; i < 100; i++) content[i] = 'X';
    content[100] = '\0';

    u8 tiny[10];
    Arena arena;

    file_write_all(TEST_FILE, content);

    arena_init(&arena, tiny, sizeof(tiny));
    option_charp rd = file_read_all_arena(TEST_FILE, &arena);
    EXPECT(option_charp_is_none(rd));

    remove(TEST_FILE);
}

/* =========================================================================
 * file_read_all — heap-backed round trip
 * ====================================================================== */

TEST(round_trip_heap) {
    const char* content = "Heap-backed file read test content.";
    u8 scratch_buf[4096];
    Arena scratch;

    file_write_all(TEST_FILE, content);

    arena_init(&scratch, scratch_buf, sizeof(scratch_buf));
    option_charp rd = file_read_all(TEST_FILE, &scratch);
    EXPECT(option_charp_is_some(rd));

    char* got = option_charp_unwrap(rd);
    EXPECT(strcmp(got, content) == 0);

    /* Must free heap-allocated result */
    str_free(got);
    remove(TEST_FILE);
}

/* =========================================================================
 * file_write_all_atomic — success round trip
 * ====================================================================== */

TEST(atomic_round_trip) {
    const char* content = "Atomic write test content.";
    u8 buf[4096];
    Arena arena;

    result_usize_Error wr = file_write_all_atomic(TEST_ATOMIC, content);
    EXPECT(result_usize_Error_is_ok(wr));
    EXPECT(result_usize_Error_unwrap(wr) == str_len(content));

    /* Temp file should be gone after successful rename */
    EXPECT(file_exists("canon_test_atomic.tmp.tmp") == false);

    /* Read back and verify */
    arena_init(&arena, buf, sizeof(buf));
    option_charp rd = file_read_all_arena(TEST_ATOMIC, &arena);
    EXPECT(option_charp_is_some(rd));
    EXPECT(strcmp(option_charp_unwrap(rd), content) == 0);

    remove(TEST_ATOMIC);
}

/* =========================================================================
 * file_write_all_atomic — path too long
 * ====================================================================== */

TEST(atomic_path_too_long) {
    char long_path[FILE_MAX_PATH + 16];
    usize i;
    for (i = 0; i < FILE_MAX_PATH; i++) {
        long_path[i] = 'a';
    }
    long_path[FILE_MAX_PATH] = '\0';

    result_usize_Error r = file_write_all_atomic(long_path, "test");
    EXPECT(result_usize_Error_is_err(r));
    EXPECT(result_usize_Error_unwrap_err(r) == ERR_BUFFER_TOO_SMALL);
}

/* =========================================================================
 * Overwrite — writing to existing file replaces content
 * ====================================================================== */

TEST(overwrite) {
    u8 buf[4096];
    Arena arena;
    option_charp rd;

    file_write_all(TEST_FILE, "first content");
    file_write_all(TEST_FILE, "second");

    arena_init(&arena, buf, sizeof(buf));
    rd = file_read_all_arena(TEST_FILE, &arena);
    EXPECT(option_charp_is_some(rd));
    EXPECT(strcmp(option_charp_unwrap(rd), "second") == 0);

    remove(TEST_FILE);
}

/* =========================================================================
 * Entry point
 * ====================================================================== */

int main(void) {
    cleanup_test_files();

    /* file_exists */
    RUN(exists_nonexistent);
    RUN(exists_real_file);

    /* file_write_all */
    RUN(write_success);
    RUN(write_empty);

    /* file_read_all_arena */
    RUN(read_arena_nonexistent);
    RUN(round_trip_arena);
    RUN(round_trip_empty);
    RUN(round_trip_large);
    RUN(arena_exhaustion);

    /* file_read_all (heap) */
    RUN(round_trip_heap);

    /* file_write_all_atomic */
    RUN(atomic_round_trip);
    RUN(atomic_path_too_long);

    /* Edge cases */
    RUN(overwrite);

    cleanup_test_files();

    printf("\nfile_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
