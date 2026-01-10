// util/random.h
#ifndef CANON_UTIL_RANDOM_H
#define CANON_UTIL_RANDOM_H

#include <stdint.h>

/**
 * @file random.h
 * @brief Minimal, deterministic, high-quality PRNG (PCG32)
 *
 * Implements the PCG32 algorithm — fast, small-footprint, good statistical quality.
 * Fully explicit: no global state, no hidden initialization.
 *
 * Key properties:
 *   - Zero global state — each Random instance is independent
 *   - Deterministic: same seed + sequence → same sequence of numbers
 *   - Very fast (single 64-bit multiply + shift/xor/rotate)
 *   - Excellent statistical properties for its size (passes TestU01, PractRand)
 *   - Small code size, no external dependencies
 *   - Thread-safe when used with separate instances per thread
 *
 * Usage pattern:
 *   Random rng;
 *   random_seed(&rng, 123456789ULL, 987654321ULL);  // seed + sequence
 *   uint32_t n = random_u32(&rng);
 *   double f = random_double(&rng);
 *   uint32_t dice = random_range(&rng, 6) + 1;  // 1..6
 *
 * Notes:
 *   - Seed and sequence are 64-bit → 2^128 possible streams
 *   - Sequence (inc) should be odd (lowest bit forced to 1 internally)
 *   - Warm-up call in seed() ensures good initial state
 *   - Safe to call with NULL pointer (returns 0/default)
 *   - Not cryptographically secure (use for games, simulations, not security)
 */

/**
 * @brief PCG32 random number generator state
 *
 * Simple 128-bit state (state + inc).
 * Do not modify fields directly — use provided functions.
 */
typedef struct {
    uint64_t state;  ///< Internal state (updated on each call)
    uint64_t inc;    ///< Sequence/stream constant (odd, determines RNG stream)
} Random;

/**
 * @brief Seeds (or reseeds) the random number generator
 *
 * @param r     Valid Random pointer (if NULL, does nothing)
 * @param seed  64-bit seed value (arbitrary, affects starting point)
 * @param seq   64-bit sequence/stream identifier (should be odd)
 *
 * Performs warm-up steps for good initial randomness.
 * Same (seed, seq) pair always produces the same sequence.
 */
static inline void random_seed(Random* r, uint64_t seed, uint64_t seq) {
    if (!r) return;

    r->state = 0U;
    r->inc   = (seq << 1u) | 1u;  // force odd

    random_u32(r);                // warm-up step 1
    r->state += seed;
    random_u32(r);                // warm-up step 2
}

/**
 * @brief Generates next 32-bit unsigned integer
 *
 * @param r Valid Random (if NULL, returns 0)
 * @return  Uniform random uint32_t in [0, UINT32_MAX]
 */
static inline uint32_t random_u32(Random* r) {
    if (!r) return 0;

    uint64_t old_state = r->state;
    r->state = old_state * 6364136223846793005ULL + r->inc;

    uint32_t xor_shifted = (uint32_t)(((old_state >> 18u) ^ old_state) >> 27u);
    uint32_t rot = (uint32_t)(old_state >> 59u);

    return (xor_shifted >> rot) | (xor_shifted << ((-rot) & 31));
}

/**
 * @brief Generates uniform random integer in [0, bound)
 *
 * @param r     Valid Random
 * @param bound Upper bound (exclusive), must be > 0
 * @return      Uniform value in [0, bound-1], 0 on invalid input
 *
 * Uses unbiased rejection sampling — no bias even for non-power-of-2 bounds.
 */
static inline uint32_t random_range(Random* r, uint32_t bound) {
    if (!r || bound == 0) return 0;

    uint32_t threshold = -bound % bound;
    uint32_t val;

    do {
        val = random_u32(r);
    } while (val < threshold);

    return val % bound;
}

/**
 * @brief Generates uniform random double in [0.0, 1.0)
 *
 * @param r Valid Random
 * @return  Double in [0.0, 1.0) with 32 bits of precision
 */
static inline double random_double(Random* r) {
    return (double)random_u32(r) / 4294967296.0;  // 2^32
}

#endif /* CANON_UTIL_RANDOM_H */
