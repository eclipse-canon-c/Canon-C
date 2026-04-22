/**
 * @file ptr_test.c
 * @brief Unit tests (and optional fuzz harness) for ptr.h
 *
 * Build modes
 * ───────────────────────────────────────────────────────────────────────────
 *   Unit test  (default)  : int main(void) runs all test groups
 *   Fuzz twin  (CANON_FUZZING defined by CMake):
 *              LLVMFuzzerTestOneInput exercises the same logic with
 *              arbitrary byte input.
 *
 * Testing scope
 * ───────────────────────────────────────────────────────────────────────────
 * This file tests the HAPPY PATHS and defined behaviors of every function
 * in ptr.h. It does NOT test contract violations (NULL inputs to null-
 * intolerant functions, non-power-of-two alignments, etc.) because
 * verifying that require_msg() fires requires a separate build mode
 * (CANON_STRICT with assertion capture) not yet implemented.
 *
 * Functions are categorized per ptr.h's NULL handling convention:
 *
 *   Null-tolerant (NULL has defined safe response — tested here):
 *     ptr_offset, ptr_offset_const, ptr_retreat, ptr_align_up,
 *     ptr_align_down, ptr_is_aligned, ptr_align_padding, ptr_in_range,
 *     ptr_range_in_range, ptr_or, ptr_or_const, ptr_is_valid
 *
 *   Null-intolerant (NULL is contract violation — NOT tested with NULL):
 *     ptr_elem, ptr_elem_const, ptr_span, ptr_diff
 *
 * Coverage
 * ───────────────────────────────────────────────────────────────────────────
 *   is_power_of_two()     — zero, powers, non-powers
 *   is_aligned()          — multiples and non-multiples
 *   align_up()            — already aligned, round-up, zero
 *   align_down()          — already aligned, round-down, zero
 *   align_padding()       — zero when aligned, positive when not
 *   ptr_align_up()        — NULL passthrough, alignment, already aligned
 *   ptr_align_down()      — NULL passthrough, alignment
 *   ptr_is_aligned()      — NULL returns false, aligned/unaligned pointers
 *   ptr_align_padding()   — NULL returns 0, positive padding
 *   ptr_offset()          — NULL passthrough, forward advance
 *   ptr_offset_const()    — same as ptr_offset for const pointers
 *   ptr_retreat()         — NULL passthrough, backward retreat
 *   ptr_diff()            — positive, negative, zero distances (no NULL)
 *   ptr_span()            — unsigned distance, equal pointers (no NULL)
 *   ptr_in_range()        — NULL args, inside, at start, at end, outside
 *   ptr_range_in_range()  — NULL args, fits, overflow-safe boundary
 *   ptr_elem()            — index 0, 1, last; elem_size 1, N (no NULL base)
 *   ptr_elem_const()      — same as ptr_elem for const pointers
 *   ptr_or()              — non-NULL returns p, NULL returns fallback
 *   ptr_or_const()        — same for const
 *   ptr_is_valid()        — NULL → false, non-NULL → true
 *   PTR_OFFSET_OF()       — offset matches manual layout
 *   PTR_CONTAINER_OF()    — recover struct from member pointer
 *   ALIGN_MAX()           — picks the larger value
 *
 * Note on ALIGN_OF()
 * ───────────────────────────────────────────────────────────────────────────
 *   The C99 fallback for ALIGN_OF() uses offsetof(struct { char _c; T _m; }, _m)
 *   which defines an anonymous type inside offsetof(). Clang rejects this as
 *   a non-standard extension (-Wgnu-offsetof-extensions / -Wc23-extensions)
 *   and MSVC raises C4116. Rather than suppress the warning project-wide we
 *   simply avoid ALIGN_OF() in this test file. The macro is tested indirectly
 *   through ptr_is_aligned() and ptr_align_up() which use it internally.
 *   The ALIGN_MAX() macro is tested with concrete constant values instead.
 *
 * Note on buf initialization
 * ───────────────────────────────────────────────────────────────────────────
 *   Several tests below call memset(buf, 0, sizeof(buf)) on local buffers
 *   even though the test code never reads buf's contents — it only takes
 *   addresses into the buffer and passes those pointers to ptr.h functions
 *   that compare or arithmetic on the pointer values without dereferencing
 *   the underlying storage. Taking addresses into uninitialized storage is
 *   defined C behavior, but GCC's -Wmaybe-uninitialized flow analysis
 *   isn't precise enough to see through the function calls and confirm no
 *   read happens. Under -Werror it flags the derived pointer variables as
 *   potentially uninitialized. The memset is a workaround for the analyzer,
 *   not a C correctness requirement.
 *
 * Portability note
 * ───────────────────────────────────────────────────────────────────────────
 *   No 0b literals. Variables declared before use (C99).
 */

#define CANON_CONTRACT_IMPL
#include "contract.h"
#include "ptr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

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
 *
 * EXPECT(expr)           : boolean expression, shows the expression on fail
 * EXPECT_EQ(act, exp)    : integral equality, shows actual vs expected
 * EXPECT_PTR_EQ(act, exp): pointer equality, shows actual vs expected
 * EXPECT_TRUE(expr)      : alias for EXPECT, semantically clearer
 * EXPECT_FALSE(expr)     : opposite of EXPECT_TRUE
 * ====================================================================== */

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT(expr)                                                \
    do {                                                            \
        if (expr) {                                                 \
            g_pass++;                                               \
        } else {                                                    \
            g_fail++;                                               \
            fprintf(stderr, "FAIL  %s:%d  %s\n",                    \
                    __FILE__, __LINE__, #expr);                     \
        }                                                           \
    } while (0)

#define EXPECT_TRUE(expr)  EXPECT(expr)
#define EXPECT_FALSE(expr) EXPECT(!(expr))

#define EXPECT_EQ(actual, expected)                                 \
    do {                                                            \
        long long _a = (long long)(actual);                         \
        long long _e = (long long)(expected);                       \
        if (_a == _e) {                                             \
            g_pass++;                                               \
        } else {                                                    \
            g_fail++;                                               \
            fprintf(stderr,                                         \
                "FAIL  %s:%d  %s == %s  (got %lld, expected %lld)\n", \
                __FILE__, __LINE__, #actual, #expected, _a, _e);    \
        }                                                           \
    } while (0)

#define EXPECT_PTR_EQ(actual, expected)                             \
    do {                                                            \
        const void* _a = (const void*)(actual);                     \
        const void* _e = (const void*)(expected);                   \
        if (_a == _e) {                                             \
            g_pass++;                                               \
        } else {                                                    \
            g_fail++;                                               \
            fprintf(stderr,                                         \
                "FAIL  %s:%d  %s == %s  (got %p, expected %p)\n",   \
                __FILE__, __LINE__, #actual, #expected, _a, _e);    \
        }                                                           \
    } while (0)

/* =========================================================================
 * Compile-time invariants
 *
 * ALIGN_MAX() is tested with concrete constants only — no ALIGN_OF() calls
 * to avoid the anonymous-struct-in-offsetof extension warning on Clang/MSVC.
 * ====================================================================== */

static_require(ALIGN_MAX(1, 2)   == 2,  align_max_picks_larger_1_2);
static_require(ALIGN_MAX(8, 4)   == 8,  align_max_picks_larger_8_4);
static_require(ALIGN_MAX(4, 4)   == 4,  align_max_equal_inputs);
static_require(ALIGN_MAX(16, 8)  == 16, align_max_16_8);
static_require(ALIGN_MAX(8, 16)  == 16, align_max_8_16);

/* =========================================================================
 * is_power_of_two
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_is_power_of_two(void) {
    /* Zero is NOT a power of two */
    EXPECT_FALSE(is_power_of_two(0));

    /* True powers */
    EXPECT_TRUE(is_power_of_two(1));
    EXPECT_TRUE(is_power_of_two(2));
    EXPECT_TRUE(is_power_of_two(4));
    EXPECT_TRUE(is_power_of_two(8));
    EXPECT_TRUE(is_power_of_two(16));
    EXPECT_TRUE(is_power_of_two(32));
    EXPECT_TRUE(is_power_of_two(64));
    EXPECT_TRUE(is_power_of_two(128));
    EXPECT_TRUE(is_power_of_two(256));
    EXPECT_TRUE(is_power_of_two((usize)1 << 31));
    EXPECT_TRUE(is_power_of_two((usize)1 << 62));

    /* Non-powers */
    EXPECT_FALSE(is_power_of_two(3));
    EXPECT_FALSE(is_power_of_two(5));
    EXPECT_FALSE(is_power_of_two(6));
    EXPECT_FALSE(is_power_of_two(7));
    EXPECT_FALSE(is_power_of_two(9));
    EXPECT_FALSE(is_power_of_two(15));
    EXPECT_FALSE(is_power_of_two(17));
    EXPECT_FALSE(is_power_of_two(100));
    EXPECT_FALSE(is_power_of_two(255));
    EXPECT_FALSE(is_power_of_two((usize)~(usize)0));
}

/* =========================================================================
 * is_aligned
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_is_aligned(void) {
    EXPECT_TRUE(is_aligned(0,  8));
    EXPECT_TRUE(is_aligned(8,  8));
    EXPECT_TRUE(is_aligned(16, 8));
    EXPECT_FALSE(is_aligned(7,  8));
    EXPECT_FALSE(is_aligned(9,  8));
    EXPECT_TRUE(is_aligned(1,  1));
    EXPECT_TRUE(is_aligned(0,  1));
    EXPECT_TRUE(is_aligned(100, 4));
    EXPECT_FALSE(is_aligned(101, 4));
}

/* =========================================================================
 * align_up
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_align_up(void) {
    EXPECT_EQ(align_up(0,  8), 0);
    EXPECT_EQ(align_up(1,  8), 8);
    EXPECT_EQ(align_up(7,  8), 8);
    EXPECT_EQ(align_up(8,  8), 8);
    EXPECT_EQ(align_up(9,  8), 16);
    EXPECT_EQ(align_up(13, 8), 16);
    EXPECT_EQ(align_up(16, 8), 16);
    EXPECT_EQ(align_up(1,  1), 1);
    EXPECT_EQ(align_up(0,  1), 0);
    EXPECT_EQ(align_up(100, 16), 112);
    EXPECT_EQ(align_up(112, 16), 112);
    EXPECT_EQ(align_up(113, 16), 128);

    /* Property: align_up(n, a) >= n */
    {
        usize vals[5];
        int idx;
        vals[0] = 0;
        vals[1] = 1;
        vals[2] = 100;
        vals[3] = 255;
        vals[4] = 1024;
        for (idx = 0; idx < 5; idx++) {
            EXPECT_TRUE(align_up(vals[idx], 8) >= vals[idx]);
        }
    }
}

/* =========================================================================
 * align_down
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_align_down(void) {
    EXPECT_EQ(align_down(0,  8), 0);
    EXPECT_EQ(align_down(7,  8), 0);
    EXPECT_EQ(align_down(8,  8), 8);
    EXPECT_EQ(align_down(9,  8), 8);
    EXPECT_EQ(align_down(15, 8), 8);
    EXPECT_EQ(align_down(16, 8), 16);
    EXPECT_EQ(align_down(13, 8), 8);
    EXPECT_EQ(align_down(1,  1), 1);
    EXPECT_EQ(align_down(100, 16), 96);
    EXPECT_EQ(align_down(112, 16), 112);

    /* Property: align_down(n, a) <= n */
    {
        usize vals[5];
        int idx;
        vals[0] = 0;
        vals[1] = 1;
        vals[2] = 100;
        vals[3] = 255;
        vals[4] = 1024;
        for (idx = 0; idx < 5; idx++) {
            EXPECT_TRUE(align_down(vals[idx], 8) <= vals[idx]);
        }
    }
}

/* =========================================================================
 * align_padding
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_align_padding(void) {
    EXPECT_EQ(align_padding(0,  8), 0);
    EXPECT_EQ(align_padding(8,  8), 0);
    EXPECT_EQ(align_padding(1,  8), 7);
    EXPECT_EQ(align_padding(7,  8), 1);
    EXPECT_EQ(align_padding(9,  8), 7);
    EXPECT_EQ(align_padding(13, 8), 3);
    EXPECT_EQ(align_padding(16, 8), 0);
    EXPECT_EQ(align_padding(1,  1), 0);

    /* Property: align_up(n, a) == n + align_padding(n, a) */
    {
        usize values[6];
        int idx;
        values[0] = 0;
        values[1] = 1;
        values[2] = 7;
        values[3] = 13;
        values[4] = 64;
        values[5] = 1023;
        for (idx = 0; idx < 6; idx++) {
            usize val = values[idx];
            EXPECT_EQ(align_up(val, 8),  val + align_padding(val, 8));
            EXPECT_EQ(align_up(val, 16), val + align_padding(val, 16));
        }
    }
}

/* =========================================================================
 * ptr_align_up / ptr_align_down / ptr_is_aligned / ptr_align_padding
 *
 * Null-tolerant functions: NULL input has defined safe response.
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_pointer_alignment(void) {
    unsigned char buf[256];
    void* base;
    void* unaligned;
    void* result;

    /* Find a 16-byte aligned address inside buf */
    base = (void*)(((uintptr_t)buf + 15) & ~(uintptr_t)15);

    /* NULL passthrough — null-tolerant convention */
    EXPECT_PTR_EQ(ptr_align_up(NULL, 8),   NULL);
    EXPECT_PTR_EQ(ptr_align_down(NULL, 8), NULL);
    EXPECT_FALSE(ptr_is_aligned(NULL, 8));
    EXPECT_EQ(ptr_align_padding(NULL, 8), 0);

    /* Already aligned — align_up returns same address */
    EXPECT_PTR_EQ(ptr_align_up(base, 16),   base);
    EXPECT_PTR_EQ(ptr_align_down(base, 16), base);
    EXPECT_TRUE(ptr_is_aligned(base, 16));
    EXPECT_EQ(ptr_align_padding(base, 16), 0);

    /* Unaligned: base + 3 */
    unaligned = (void*)((u8*)base + 3);

    result = ptr_align_up(unaligned, 8);
    EXPECT_TRUE((uintptr_t)result >= (uintptr_t)unaligned);
    EXPECT_TRUE(((uintptr_t)result & 7) == 0);

    result = ptr_align_down(unaligned, 8);
    EXPECT_TRUE((uintptr_t)result <= (uintptr_t)unaligned);
    EXPECT_TRUE(((uintptr_t)result & 7) == 0);

    EXPECT_FALSE(ptr_is_aligned(unaligned, 8));
    EXPECT_TRUE(ptr_is_aligned(unaligned, 1));

    EXPECT_TRUE(ptr_align_padding(unaligned, 8) > 0);
    EXPECT_EQ(ptr_align_padding(base, 16), 0);

    /* ptr_align_up/down result is always aligned */
    {
        int offset;
        for (offset = 0; offset < 32; offset++) {
            void* p    = (void*)((u8*)base + offset);
            void* up   = ptr_align_up(p, 16);
            void* down = ptr_align_down(p, 16);
            EXPECT_TRUE(ptr_is_aligned(up,   16));
            EXPECT_TRUE(ptr_is_aligned(down, 16));
            EXPECT_TRUE((uintptr_t)up   >= (uintptr_t)p);
            EXPECT_TRUE((uintptr_t)down <= (uintptr_t)p);
        }
    }
}

/* =========================================================================
 * ptr_offset / ptr_offset_const / ptr_retreat
 *
 * Null-tolerant functions: NULL input returns NULL.
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_ptr_arithmetic(void) {
    unsigned char buf[128];
    void* base = (void*)buf;

    /* NULL passthrough — null-tolerant convention */
    EXPECT_PTR_EQ(ptr_offset(NULL, 10),       NULL);
    EXPECT_PTR_EQ(ptr_offset_const(NULL, 10), NULL);
    EXPECT_PTR_EQ(ptr_retreat(NULL, 10),      NULL);

    /* ptr_offset advances */
    EXPECT_PTR_EQ(ptr_offset(base, 0),  base);
    EXPECT_PTR_EQ(ptr_offset(base, 1),  (void*)((u8*)base + 1));
    EXPECT_PTR_EQ(ptr_offset(base, 64), (void*)((u8*)base + 64));

    /* ptr_offset_const */
    {
        const void* cb = (const void*)buf;
        EXPECT_PTR_EQ(ptr_offset_const(cb, 0),  cb);
        EXPECT_PTR_EQ(ptr_offset_const(cb, 32), (const void*)((const u8*)cb + 32));
    }

    /* ptr_retreat */
    {
        void* mid = ptr_offset(base, 64);
        EXPECT_PTR_EQ(ptr_retreat(mid, 0),  mid);
        EXPECT_PTR_EQ(ptr_retreat(mid, 1),  ptr_offset(base, 63));
        EXPECT_PTR_EQ(ptr_retreat(mid, 64), base);
    }

    /* ptr_offset then ptr_retreat recovers original */
    {
        int step;
        for (step = 0; step < 64; step++) {
            void* fwd = ptr_offset(base, (usize)step);
            void* bck = ptr_retreat(fwd, (usize)step);
            EXPECT_PTR_EQ(bck, base);
        }
    }
}

/* =========================================================================
 * ptr_diff / ptr_span
 *
 * Null-intolerant functions: NULL input is a contract violation.
 * This test covers happy paths only — passing NULL would trigger
 * require_msg() and abort, which requires a separate test mode to verify.
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_ptr_diff_span(void) {
    /* Initialize buf to work around GCC's -Wmaybe-uninitialized.
     * See file-level "Note on buf initialization" for the rationale:
     * the code never reads buf's contents, only takes addresses into
     * it, but GCC's flow analysis isn't precise enough to see that.
     * Under -Werror the warning becomes fatal. */
    unsigned char buf[128];
    void* start;
    void* mid;
    void* end;
    memset(buf, 0, sizeof(buf));
    start = (void*)(buf);
    mid   = (void*)(buf + 64);
    end   = (void*)(buf + 128);

    /* ptr_diff: signed distance */
    EXPECT_EQ(ptr_diff(start, start), 0);
    EXPECT_EQ(ptr_diff(mid,   start), 64);
    EXPECT_EQ(ptr_diff(end,   start), 128);
    EXPECT_EQ(ptr_diff(start, mid),   -64);
    EXPECT_EQ(ptr_diff(start, end),   -128);
    EXPECT_EQ(ptr_diff(mid,   end),   -64);

    /* ptr_span: unsigned, requires to >= from */
    EXPECT_EQ(ptr_span(start, start), 0);
    EXPECT_EQ(ptr_span(mid,   start), 64);
    EXPECT_EQ(ptr_span(end,   start), 128);
    EXPECT_EQ(ptr_span(end,   mid),   64);

    /* ptr_diff and ptr_span agree for to >= from */
    EXPECT_EQ((isize)ptr_span(end, start), ptr_diff(end, start));
    EXPECT_EQ((isize)ptr_span(mid, start), ptr_diff(mid, start));
}

/* =========================================================================
 * ptr_in_range / ptr_range_in_range
 *
 * Null-tolerant: NULL arguments return false.
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_bounds_checking(void) {
    /* Initialize buf to work around GCC's -Wmaybe-uninitialized.
     * See file-level "Note on buf initialization" for the rationale. */
    unsigned char buf[64];
    void* start;
    void* end;
    void* mid;
    void* last;
    memset(buf, 0, sizeof(buf));
    start = (void*)(buf);
    end   = (void*)(buf + 64);
    mid   = (void*)(buf + 32);
    last  = (void*)(buf + 63);

    /* NULL args — null-tolerant convention returns false */
    EXPECT_FALSE(ptr_in_range(NULL,  start, end));
    EXPECT_FALSE(ptr_in_range(mid,   NULL,  end));
    EXPECT_FALSE(ptr_in_range(mid,   start, NULL));

    /* Inside */
    EXPECT_TRUE(ptr_in_range(start, start, end));
    EXPECT_TRUE(ptr_in_range(mid,   start, end));
    EXPECT_TRUE(ptr_in_range(last,  start, end));

    /* Outside: end is excluded */
    EXPECT_FALSE(ptr_in_range(end, start, end));

    /* ptr_range_in_range — NULL args */
    EXPECT_FALSE(ptr_range_in_range(NULL,  10, start, end));
    EXPECT_FALSE(ptr_range_in_range(start, 10, NULL,  end));
    EXPECT_FALSE(ptr_range_in_range(start, 10, start, NULL));

    /* Fits entirely */
    EXPECT_TRUE(ptr_range_in_range(start,  0, start, end));
    EXPECT_TRUE(ptr_range_in_range(start, 64, start, end));
    EXPECT_TRUE(ptr_range_in_range(mid,   32, start, end));
    EXPECT_TRUE(ptr_range_in_range(start,  1, start, end));

    /* Overflows region */
    EXPECT_FALSE(ptr_range_in_range(start, 65, start, end));
    EXPECT_FALSE(ptr_range_in_range(mid,   33, start, end));
    EXPECT_FALSE(ptr_range_in_range(last,   2, start, end));

    /* Zero-length range at any in-range pointer */
    EXPECT_TRUE(ptr_range_in_range(start, 0, start, end));
    EXPECT_TRUE(ptr_range_in_range(mid,   0, start, end));
}

/* =========================================================================
 * ptr_elem / ptr_elem_const
 *
 * Null-intolerant: NULL base is a contract violation (not tested here).
 * Tests cover index and elem_size variations with valid base only.
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_ptr_elem(void) {
    unsigned char buf[128];
    void* base = (void*)buf;
    int idx;

    /* elem_size == 1 */
    for (idx = 0; idx < 64; idx++) {
        EXPECT_PTR_EQ(ptr_elem(base, (usize)idx, 1),
                      (void*)((u8*)base + idx));
    }

    /* elem_size == 4 */
    EXPECT_PTR_EQ(ptr_elem(base, 0, 4), base);
    EXPECT_PTR_EQ(ptr_elem(base, 1, 4), (void*)((u8*)base + 4));
    EXPECT_PTR_EQ(ptr_elem(base, 2, 4), (void*)((u8*)base + 8));
    EXPECT_PTR_EQ(ptr_elem(base, 3, 4), (void*)((u8*)base + 12));

    /* elem_size == 8 */
    EXPECT_PTR_EQ(ptr_elem(base, 0,  8), base);
    EXPECT_PTR_EQ(ptr_elem(base, 1,  8), (void*)((u8*)base + 8));
    EXPECT_PTR_EQ(ptr_elem(base, 15, 8), (void*)((u8*)base + 120));

    /* ptr_elem_const */
    {
        const void* cb = (const void*)buf;
        EXPECT_PTR_EQ(ptr_elem_const(cb, 0, 4), cb);
        EXPECT_PTR_EQ(ptr_elem_const(cb, 3, 4),
                      (const void*)((const u8*)cb + 12));
    }

    /* Property: ptr_elem(base, i, sz) == ptr_offset(base, i * sz) */
    {
        usize sizes[3];
        usize idxs[4];
        int si, ii;
        sizes[0] = 1;  sizes[1] = 4;  sizes[2] = 8;
        idxs[0]  = 0;  idxs[1]  = 1;  idxs[2]  = 5;  idxs[3] = 10;
        for (si = 0; si < 3; si++) {
            for (ii = 0; ii < 4; ii++) {
                EXPECT_PTR_EQ(ptr_elem(base, idxs[ii], sizes[si]),
                              ptr_offset(base, idxs[ii] * sizes[si]));
            }
        }
    }
}

/* =========================================================================
 * ptr_or / ptr_or_const / ptr_is_valid
 *
 * Null-tolerant utility functions.
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_ptr_null_safety(void) {
    /* Initialize buf to work around GCC's -Wmaybe-uninitialized.
     * See file-level "Note on buf initialization" for the rationale. */
    unsigned char buf[8];
    void* p;
    void* fallback;
    memset(buf, 0, sizeof(buf));
    p        = (void*)buf;
    fallback = (void*)(buf + 4);

    /* ptr_or */
    EXPECT_PTR_EQ(ptr_or(p,    fallback), p);
    EXPECT_PTR_EQ(ptr_or(NULL, fallback), fallback);
    EXPECT_PTR_EQ(ptr_or(NULL, NULL),     NULL);

    /* ptr_or_const */
    {
        const void* cp = (const void*)buf;
        const void* cf = (const void*)(buf + 4);
        EXPECT_PTR_EQ(ptr_or_const(cp,   cf),   cp);
        EXPECT_PTR_EQ(ptr_or_const(NULL, cf),   cf);
        EXPECT_PTR_EQ(ptr_or_const(NULL, NULL), NULL);
    }

    /* ptr_is_valid */
    EXPECT_TRUE(ptr_is_valid(p));
    EXPECT_FALSE(ptr_is_valid(NULL));
    EXPECT_TRUE(ptr_is_valid((void*)buf));
}

/* =========================================================================
 * PTR_OFFSET_OF / PTR_CONTAINER_OF
 * ====================================================================== */

typedef struct {
    u32  alpha;
    u8   beta;
    u16  gamma;
    u64  delta;
} TestStruct;

typedef struct {
    int  value;
    u8   tag;
    u32  next_offset;
} NodeStruct;

static CANON_MAYBE_UNUSED void test_offset_of(void) {
    /* Values match the standard offsetof — no numeric assumptions made
     * about specific offsets (layout is implementation-defined). */
    EXPECT_EQ(PTR_OFFSET_OF(TestStruct, alpha), offsetof(TestStruct, alpha));
    EXPECT_EQ(PTR_OFFSET_OF(TestStruct, beta),  offsetof(TestStruct, beta));
    EXPECT_EQ(PTR_OFFSET_OF(TestStruct, gamma), offsetof(TestStruct, gamma));
    EXPECT_EQ(PTR_OFFSET_OF(TestStruct, delta), offsetof(TestStruct, delta));

    /* alpha is first member, offset 0 */
    EXPECT_EQ(PTR_OFFSET_OF(TestStruct, alpha), 0);

    EXPECT_EQ(PTR_OFFSET_OF(NodeStruct, value), offsetof(NodeStruct, value));
    EXPECT_EQ(PTR_OFFSET_OF(NodeStruct, value), 0);
}

static CANON_MAYBE_UNUSED void test_container_of(void) {
    TestStruct obj;
    TestStruct* recovered;
    u8* member_ptr;

    obj.alpha = 0xDEADBEEFU;
    obj.beta  = 0xAB;
    obj.gamma = 0x1234;
    obj.delta = 0xCAFEBABEDEADBEEFULL;

    /* Recover from &obj.beta */
    member_ptr = &obj.beta;
    recovered  = PTR_CONTAINER_OF(member_ptr, TestStruct, beta);
    EXPECT_PTR_EQ(recovered, &obj);
    EXPECT_EQ(recovered->alpha, 0xDEADBEEFU);
    EXPECT_EQ(recovered->beta,  0xAB);
    EXPECT_TRUE(recovered->delta == 0xCAFEBABEDEADBEEFULL);

    /* Recover from &obj.delta */
    {
        u64* delta_ptr = &obj.delta;
        TestStruct* rec2 = PTR_CONTAINER_OF(delta_ptr, TestStruct, delta);
        EXPECT_PTR_EQ(rec2, &obj);
        EXPECT_EQ(rec2->alpha, 0xDEADBEEFU);
    }

    /* Recover from &obj.alpha (offset 0) */
    {
        u32* alpha_ptr = &obj.alpha;
        TestStruct* rec3 = PTR_CONTAINER_OF(alpha_ptr, TestStruct, alpha);
        EXPECT_PTR_EQ(rec3, &obj);
    }

    /* Array: recover each element from its alpha member */
    {
        TestStruct arr[4];
        int ii;
        for (ii = 0; ii < 4; ii++) {
            arr[ii].alpha = (u32)ii;
        }
        for (ii = 0; ii < 4; ii++) {
            u32* ap  = &arr[ii].alpha;
            TestStruct* rec = PTR_CONTAINER_OF(ap, TestStruct, alpha);
            EXPECT_PTR_EQ(rec, &arr[ii]);
            EXPECT_EQ(rec->alpha, (u32)ii);
        }
    }
}

/* =========================================================================
 * ALIGN_MAX runtime tests (concrete constants only; no ALIGN_OF())
 * ====================================================================== */

static CANON_MAYBE_UNUSED void test_align_max(void) {
    EXPECT_EQ(ALIGN_MAX(1,  2),  2);
    EXPECT_EQ(ALIGN_MAX(2,  1),  2);
    EXPECT_EQ(ALIGN_MAX(4,  4),  4);
    EXPECT_EQ(ALIGN_MAX(8,  16), 16);
    EXPECT_EQ(ALIGN_MAX(16, 8),  16);
    EXPECT_EQ(ALIGN_MAX(32, 64), 64);
    EXPECT_EQ(ALIGN_MAX(64, 32), 64);

    /* Property: result is always >= both inputs */
    EXPECT_TRUE(ALIGN_MAX(8,  16) >= 8);
    EXPECT_TRUE(ALIGN_MAX(8,  16) >= 16);
    EXPECT_TRUE(ALIGN_MAX(16, 8)  >= 16);
    EXPECT_TRUE(ALIGN_MAX(16, 8)  >= 8);
}

/* =========================================================================
 * Fuzz harness
 *
 * Tests invariants with arbitrary byte input. Contract violations are not
 * fuzzed — feeding NULL to null-intolerant functions or non-power-of-two
 * alignments would deliberately trigger require_msg() and abort, which is
 * correct behavior but not bug-finding.
 * ====================================================================== */

#ifdef CANON_FUZZING

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
    unsigned char buf[256];
    usize offset_a = 0, offset_b = 0;
    usize align_exp = 0;
    usize align     = 0;

    if (size < 4) return 0;

    offset_a  = (usize)data[0];
    offset_b  = (usize)data[1];
    align_exp = (usize)(data[2] & 7);
    align     = (usize)1 << align_exp;

    memset(buf, 0, sizeof(buf));

    if (offset_a >= 128) offset_a = 127;
    if (offset_b >= 128) offset_b = 127;

    {
        void* base = (void*)buf;
        void* pa   = ptr_offset(base, offset_a);
        void* pb   = ptr_offset(base, offset_b);

        /* align_up result is >= input and is aligned */
        {
            usize up_a = align_up(offset_a, align);
            usize dn_a = align_down(offset_a, align);
            if (up_a < offset_a) __builtin_trap();
            if (dn_a > offset_a) __builtin_trap();
            if (!is_aligned(up_a, align)) __builtin_trap();
            if (!is_aligned(dn_a, align)) __builtin_trap();
            if (align_padding(offset_a, align) + offset_a != up_a)
                __builtin_trap();
        }

        /* ptr_align_up result >= pa and is aligned */
        {
            void* up = ptr_align_up(pa, align);
            if ((uintptr_t)up < (uintptr_t)pa) __builtin_trap();
            if (!ptr_is_aligned(up, align))     __builtin_trap();
        }

        /* ptr_align_down result <= pa and is aligned */
        {
            void* dn = ptr_align_down(pa, align);
            if ((uintptr_t)dn > (uintptr_t)pa) __builtin_trap();
            if (!ptr_is_aligned(dn, align))     __builtin_trap();
        }

        /* ptr_offset then ptr_retreat round-trips */
        {
            void* fwd = ptr_offset(base, offset_a);
            void* bck = ptr_retreat(fwd, offset_a);
            if (bck != base) __builtin_trap();
        }

        /* ptr_diff is antisymmetric (both pointers in buf — safe to diff) */
        {
            isize d_ab = ptr_diff(pa, pb);
            isize d_ba = ptr_diff(pb, pa);
            if (d_ab != -d_ba) __builtin_trap();
        }

        /* ptr_span agrees with ptr_diff for to >= from */
        if ((uintptr_t)pa >= (uintptr_t)pb) {
            usize span = ptr_span(pa, pb);
            isize diff = ptr_diff(pa, pb);
            if ((isize)span != diff) __builtin_trap();
        }

        /* Both pa and pb are in [base, base+128) */
        {
            void* bend = ptr_offset(base, 128);
            if (!ptr_in_range(pa, base, bend)) __builtin_trap();
            if (!ptr_in_range(pb, base, bend)) __builtin_trap();
        }

        /* Null-tolerant NULL passthrough invariants */
        if (ptr_offset(NULL, offset_a)     != NULL)     __builtin_trap();
        if (ptr_align_up(NULL, align)      != NULL)     __builtin_trap();
        if (ptr_align_down(NULL, align)    != NULL)     __builtin_trap();
        if (ptr_is_aligned(NULL, align)    != false)    __builtin_trap();
        if (ptr_align_padding(NULL, align) != (usize)0) __builtin_trap();
        if (ptr_is_valid(NULL)             != false)    __builtin_trap();
    }

    return 0;
}

#else

int main(void) {
    test_is_power_of_two();
    test_is_aligned();
    test_align_up();
    test_align_down();
    test_align_padding();
    test_pointer_alignment();
    test_ptr_arithmetic();
    test_ptr_diff_span();
    test_bounds_checking();
    test_ptr_elem();
    test_ptr_null_safety();
    test_offset_of();
    test_container_of();
    test_align_max();

    printf("\nptr_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif /* CANON_FUZZING */
