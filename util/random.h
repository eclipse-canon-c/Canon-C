// util/random.h
#ifndef CANON_UTIL_RANDOM_H
#define CANON_UTIL_RANDOM_H

#include <stdint.h>

/**
 * @file random.h
 * @brief Fast, high-quality, fully explicit PCG32 pseudo-random number generator
 *
 * Implements the **PCG32** algorithm (Permuted Congruential Generator)  
 * — one of the best small-footprint PRNGs available: fast, statistically excellent,  
 * and completely deterministic with explicit control over streams.
 *
 * Key properties:
 * ────────────────────────────────────────────────────────────────────────────
 * - **No global state** — each Random instance is independent
 * - **Deterministic** — same seed + sequence = always same output sequence
 * - **Very fast** — single 64-bit multiply + shift/xor/rotate operations
 * - **Excellent quality** — passes TestU01 BigCrush, PractRand (long runs)
 * - **Tiny code size** — no external dependencies
 * - **Thread-safe** — when each thread uses its own Random instance
 * - **2¹²⁸ possible streams** — via 64-bit seed + 64-bit sequence
 * - **Not cryptographically secure** — suitable for simulations, games, procedural generation, testing, etc.
 *
 * Design philosophy:
 * - Treat randomness as a pure, explicit, reproducible function
 * - Give full control: caller manages state and stream identity
 * - Zero hidden initialization or global RNG
 * - Safe defaults: NULL pointer returns 0 (non-crashing)
 * - Unbiased range sampling (rejection method)
 *
 * Typical usage patterns:
 * ────────────────────────────────────────────────────────────────────────────
 * 1. Simple single RNG:
 *    Random rng;
 *    random_seed(&rng, time(NULL), 0xdeadbeef);
 *    for (int i = 0; i < 10; i++) {
 *        printf("%u\n", random_range(&rng, 100));
 *    }
 *
 * 2. Multiple independent streams:
 *    Random player_rng, enemy_rng;
 *    random_seed(&player_rng, 12345, 111);
 *    random_seed(&enemy_rng, 12345, 222);  // same seed, different stream
 *
 * 3. Reproducible tests:
 *    random_seed(&rng, 42, 0xBADC0DE);  // fixed for CI/tests
 */

/**
 * @brief PCG32 generator state (128 bits total)
 *
 * Do **not** modify fields directly — use random_seed(), random_u32(), etc.
 */
typedef struct {
    uint64_t state;  ///< Current internal state (updated each step)
    uint64_t inc;    ///< Sequence/stream constant (always odd)
} Random;

/* ────────────────────────────────────────────────────────────────────────────
   Initialization
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Seeds or reseeds the random number generator
 *
 * @param r     Pointer to Random instance (NULL-safe: does nothing)
 * @param seed  64-bit arbitrary seed value (starting point)
 * @param seq   64-bit sequence/stream identifier (recommended: odd number)
 *
 * Behavior:
 * - Forces lowest bit of seq to 1 (ensures valid stream)
 * - Performs two warm-up steps → good initial randomness
 * - Same (seed, seq) always produces identical sequence
 *
 * Recommended:
 * - Use different seq values for independent streams
 * - For reproducibility: fix both seed and seq
 * - For "random" start: use time(NULL) or device entropy for seed
 */
static inline void random_seed(Random* r, uint64_t seed, uint64_t seq) {
    if (!r) return;

    r->state = 0U;
    r->inc = (seq << 1u) | 1u;  // force odd

    // Warm-up (recommended for PCG family)
    random_u32(r);
    r->state += seed;
    random_u32(r);
}

/* ────────────────────────────────────────────────────────────────────────────
   Core random generation
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Generates next 32-bit uniform random integer
 *
 * @param r Valid Random pointer (NULL → returns 0)
 * @return uint32_t in range [0, UINT32_MAX]
 *
 * Core PCG32 step: multiply → add → xor-shift → rotate
 */
static inline uint32_t random_u32(Random* r) {
    if (!r) return 0;

    uint64_t oldstate = r->state;
    r->state = oldstate * 6364136223846793005ULL + r->inc;

    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);

    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenient derived distributions
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Uniform random integer in range [0, bound)
 *
 * @param r     Valid Random
 * @param bound Upper bound (exclusive) — must be > 0
 * @return Value in [0, bound-1], unbiased; 0 on invalid input
 *
 * Uses rejection sampling → statistically perfect uniformity  
 * (no bias even when bound is not power of 2)
 */
static inline uint32_t random_range(Random* r, uint32_t bound) {
    if (!r || bound == 0) return 0;

    uint32_t threshold = -bound % bound;
    uint32_t r32;

    do {
        r32 = random_u32(r);
    } while (r32 < threshold);

    return r32 % bound;
}

/**
 * @brief Uniform random double in [0.0, 1.0)
 *
 * @param r Valid Random pointer
 * @return double in [0.0, 1.0) with 32-bit mantissa precision
 *
 * Simple division of 32-bit random value → very fast
 */
static inline double random_double(Random* r) {
    return (double)random_u32(r) / 4294967296.0;  // exactly 1 / 2³²
}

#endif /* CANON_UTIL_RANDOM_H */
