#ifndef CANON_UTIL_RANDOM_H
#define CANON_UTIL_RANDOM_H

#include "core/primitives/types.h"   // u32, u64, usize, bool

/**
 * @file util/random.h
 * @brief Fast, high-quality, explicit PCG32 pseudo-random number generator
 *
 * Implements PCG32 (Permuted Congruential Generator) — one of the best small PRNGs
 * for non-cryptographic use. Provides excellent statistical properties, full
 * deterministic control, and independent streams via sequence parameter.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Pure integer arithmetic — no platform-specific code
 * - Works on any architecture (32/64-bit, big/little endian)
 * - Deterministic across all platforms (same seed/seq → same sequence)
 * - No external dependencies (no libc rand, no system calls)
 *
 * Thread-safety:
 * ────────────────────────────────────────────────────────────────────────────
 * - Each Random instance is independent
 * - Safe to use different instances from multiple threads
 * - NOT safe to share one instance across threads without external locking
 * - Recommended: per-thread Random or mutex around shared instance
 *
 * Performance:
 * ────────────────────────────────────────────────────────────────────────────
 * - O(1) per random number
 * - 16 bytes per instance (fits in one cache line)
 * - ~2–5 CPU cycles per u32 on modern CPUs
 * - No branches in hot path (except range rejection loop)
 * - No allocations, no syscalls
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit state — caller owns Random instance
 * - Deterministic — same seed + seq = identical sequence
 * - Multiple streams — 2^63 independent sequences via seq parameter
 * - High quality — passes TestU01 BigCrush, PractRand 16TB+
 * - Unbiased ranges — rejection sampling for perfect uniformity
 * - No hidden magic — all operations traceable
 * - Not cryptographic — use /dev/urandom or libsodium for security
 *
 * What is PCG32?
 * ────────────────────────────────────────────────────────────────────────────
 * - 64-bit state + 64-bit inc → 2^64 period per stream
 * - Permutation: xorshift + rotate-right
 * - Very small, very fast, statistically excellent
 * - Designed by Melissa O'Neill (pcg-random.org)
 *
 * Why PCG32 over others?
 * ────────────────────────────────────────────────────────────────────────────
 * vs rand(): better quality, explicit state, longer period, faster
 * vs Mersenne Twister: 100× smaller state, faster, better stats
 * vs xorshift: better quality, multiple streams
 *
 * Seeding strategies:
 * - Reproducible: fixed seed + seq
 * - Pseudo-random: time(NULL) ^ getpid() or similar
 * - Multiple streams: same seed + different seq
 *
 * Typical use cases:
 * - Games (loot, AI, procedural generation)
 * - Simulations (Monte Carlo)
 * - Testing (reproducible random tests)
 * - Shuffling, sampling
 * - Noise functions
 *
 * NOT for: cryptography, keys, nonces, security tokens
 */
/**
 * @brief PCG32 generator state (128 bits total)
 */
typedef struct {
    u64 state;  // Current LCG state
    u64 inc;    // Stream/sequence constant (always odd)
} Random;

/* ────────────────────────────────────────────────────────────────────────────
   Initialization
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Seeds or reseeds the generator
 *
 * Same (seed, seq) always produces identical sequence.
 * seq selects independent stream (2^63 possible).
 *
 * @param r     Valid Random pointer (NULL-safe: no-op)
 * @param seed  64-bit seed (starting point)
 * @param seq   64-bit sequence/stream ID (lowest bit forced to 1)
 */
static inline void random_seed(Random* r, u64 seed, u64 seq) {
    if (!r) return;
    r->state = 0;
    r->inc   = (seq << 1) | 1;  // Ensure odd
    r->state += seed;
    random_u32(r);              // Warm-up 1
    random_u32(r);              // Warm-up 2
}

/* ────────────────────────────────────────────────────────────────────────────
   Core generation
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Generates next uniform 32-bit integer
 *
 * @param r Valid Random pointer (NULL-safe: returns 0)
 * @return Uniform u32 in [0, UINT32_MAX]
 */
static inline u32 random_u32(Random* r) {
    if (!r) return 0;

    u64 oldstate = r->state;
    r->state = oldstate * 6364136223846793005ULL + r->inc;

    u32 xorshifted = (u32)(((oldstate >> 18u) ^ oldstate) >> 27u);
    u32 rot = (u32)(oldstate >> 59u);

    // Branchless rotate right
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

/**
 * @brief Generates uniform u32 in [0, bound)
 *
 * Uses rejection sampling → perfect uniformity (no bias).
 *
 * @param r     Valid Random pointer
 * @param bound Upper bound (exclusive), must be > 0
 * @return Uniform value in [0, bound-1], or 0 on invalid input
 */
static inline u32 random_range(Random* r, u32 bound) {
    if (!r || bound == 0) return 0;

    // Rejection sampling for perfect uniformity
    u32 threshold = -bound % bound;  // = (UINT32_MAX + 1 - bound) % bound
    u32 value;
    do {
        value = random_u32(r);
    } while (value < threshold);

    return value % bound;
}

/**
 * @brief Uniform double in [0.0, 1.0)
 *
 * 32 bits of randomness in mantissa (sufficient for most uses).
 *
 * @param r Valid Random pointer
 * @return Double in [0.0, 1.0)
 */
static inline double random_double(Random* r) {
    return (double)random_u32(r) / 4294967296.0;  // exactly 2^32
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience helpers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Random bool (true/false, 50% each)
 */
static inline bool random_bool(Random* r) {
    return random_u32(r) & 1;
}

/**
 * @brief Random signed int in [min, max] inclusive
 */
static inline i32 random_int_range(Random* r, i32 min, i32 max) {
    if (min >= max) return min;
    u32 range = (u32)(max - min + 1);
    return min + (i32)random_range(r, range);
}

/**
 * @brief Random float in [min, max)
 */
static inline float random_float_range(Random* r, float min, float max) {
    return min + (float)random_double(r) * (max - min);
}

/**
 * @brief Random double in [min, max)
 */
static inline double random_double_range(Random* r, double min, double max) {
    return min + random_double(r) * (max - min);
}

#endif /* CANON_UTIL_RANDOM_H */
