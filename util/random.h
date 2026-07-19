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

#ifndef CANON_UTIL_RANDOM_H
#define CANON_UTIL_RANDOM_H

#include <stdbool.h>

#include "core/primitives/types.h"
#include "core/primitives/contract.h"
#include "core/ownership.h"

/**
 * @file util/random.h
 * @brief Fast, high-quality, explicit PCG32 pseudo-random number generator
 *
 * Implements PCG32 (Permuted Congruential Generator) — one of the best small
 * PRNGs for non-cryptographic use. Provides excellent statistical properties,
 * full deterministic control, and independent streams via sequence parameter.
 *
 * Portability:
 * ────────────────────────────────────────────────────────────────────────────
 * - Pure integer arithmetic — no platform-specific code
 * - Works on any architecture (32/64-bit, big/little endian)
 * - Deterministic across all platforms (same seed/seq → same sequence)
 * - No external dependencies beyond Canon-C core types
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
 * - ~2-5 CPU cycles per u32 on modern CPUs
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
 * - Contracts — NULL pointer is a programming error (require_msg)
 * - Not cryptographic — use /dev/urandom or libsodium for security
 *
 * Contracts:
 * ────────────────────────────────────────────────────────────────────────────
 * - r (Random pointer) must not be NULL — violated → contract abort
 * - bound == 0 in random_range returns 0 (defined, not a contract violation)
 * - min >= max in random_i32_range returns min (defined, not a contract violation)
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
 * vs rand():             better quality, explicit state, longer period, faster
 * vs Mersenne Twister:   100x smaller state, faster, better stats
 * vs xorshift:           better quality, multiple independent streams
 *
 * Seeding strategies:
 * ────────────────────────────────────────────────────────────────────────────
 * - Reproducible:      fixed seed + fixed seq
 * - Pseudo-random:     time(NULL) ^ getpid() or similar (caller provides)
 * - Multiple streams:  same seed + different seq values
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Games (loot, AI, procedural generation)
 * - Simulations (Monte Carlo)
 * - Testing (reproducible random inputs)
 * - Shuffling, sampling
 * - Noise functions
 *
 * NOT suitable for:
 * ────────────────────────────────────────────────────────────────────────────
 * - Cryptography, keys, nonces, or security tokens
 * - Applications requiring a CSPRNG
 */

/**
 * @brief PCG32 generator state (128 bits / 16 bytes)
 *
 * Each instance is fully independent. Initialize with random_seed() before use.
 * Uninitialized use (state == 0, inc == 0) produces defined but poor output.
 */
typedef struct {
    u64 state;  ///< Current LCG state
    u64 inc;    ///< Stream/sequence constant (always kept odd)
} Random;

/* ────────────────────────────────────────────────────────────────────────────
   Core generation — defined before random_seed (used inside it)
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Generates the next uniform 32-bit integer
 *
 * Core PCG32 step: LCG advance + output permutation (xorshift + rotate-right).
 * This is the hot path — all other generation functions build on this.
 *
 * @param r Valid Random pointer (must not be NULL — contract)
 * @return Uniform u32 in [0, U32_MAX]
 */
static inline u32 random_u32(borrowed(Random*) r) {
    require_msg(r != NULL, "random_u32: r is NULL");

    u64 oldstate  = r->state;
    r->state      = oldstate * 6364136223846793005ULL + r->inc;

    u32 xorshifted = (u32)(((oldstate >> 18u) ^ oldstate) >> 27u);
    u32 rot        = (u32)(oldstate >> 59u);

    /* Branchless rotate-right */
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

/* ────────────────────────────────────────────────────────────────────────────
   Initialization
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Seeds or reseeds the generator
 *
 * Same (seed, seq) pair always produces an identical output sequence.
 * Two calls with different seq values produce statistically independent streams.
 * Two warm-up steps are performed to mix the initial state.
 *
 * @param r    Valid Random pointer (must not be NULL — contract)
 * @param seed 64-bit seed value (starting point in the state space)
 * @param seq  64-bit sequence/stream ID (lowest bit forced to 1 internally)
 */
static inline void random_seed(borrowed(Random*) r, u64 seed, u64 seq) {
    require_msg(r != NULL, "random_seed: r is NULL");
    r->state = 0;
    r->inc   = (seq << 1u) | 1u;  /* ensure inc is always odd */
    r->state += seed;
    random_u32(r);              /* warm-up step 1 */
    random_u32(r);              /* warm-up step 2 */
}

/* ────────────────────────────────────────────────────────────────────────────
   Range generation
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Generates a uniform u32 in [0, bound)
 *
 * Uses rejection sampling to eliminate modulo bias — every value in
 * [0, bound) has exactly equal probability.
 *
 * @param r     Valid Random pointer (must not be NULL — contract)
 * @param bound Upper bound (exclusive) — bound == 0 returns 0
 * @return Uniform value in [0, bound-1], or 0 if bound == 0
 */
static inline u32 random_range(borrowed(Random*) r, u32 bound) {
    require_msg(r != NULL, "random_range: r is NULL");
    if (bound == 0u) return 0u;

    /* Rejection threshold eliminates bias from modulo wrap */
    u32 threshold = (u32)(-(u32)bound % bound);
    u32 value;
    do {
        value = random_u32(r);
    } while (value < threshold);

    return value % bound;
}

/**
 * @brief Generates a uniform f64 in [0.0, 1.0)
 *
 * 32 bits of randomness mapped into the mantissa.
 * Sufficient for most simulation and game use cases.
 *
 * @param r Valid Random pointer (must not be NULL — contract)
 * @return f64 in [0.0, 1.0)
 */
static inline f64 random_f64(borrowed(Random*) r) {
    return (f64)random_u32(r) / 4294967296.0;  /* divide by exactly 2^32 */
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenience helpers
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Random bool — true or false with equal probability
 *
 * @param r Valid Random pointer (must not be NULL — contract)
 */
static inline bool random_bool(borrowed(Random*) r) {
    return (random_u32(r) & 1u) != 0u;
}

/**
 * @brief Random signed integer in [min, max] inclusive
 *
 * @param r   Valid Random pointer (must not be NULL — contract)
 * @param min Lower bound (inclusive)
 * @param max Upper bound (inclusive) — if min >= max, returns min
 * @return Uniform i32 in [min, max]
 */
static inline i32 random_i32_range(borrowed(Random*) r, i32 min, i32 max) {
    require_msg(r != NULL, "random_i32_range: r is NULL");
    if (min >= max) return min;
    u32 range = (u32)((i64)max - (i64)min + 1);
    return min + (i32)random_range(r, range);
}

/**
 * @brief Random f32 in [min, max)
 *
 * @param r   Valid Random pointer (must not be NULL — contract)
 * @param min Lower bound (inclusive)
 * @param max Upper bound (exclusive)
 * @return f32 in [min, max)
 */
static inline f32 random_f32_range(borrowed(Random*) r, f32 min, f32 max) {
    return min + (f32)random_f64(r) * (max - min);
}

/**
 * @brief Random f64 in [min, max)
 *
 * @param r   Valid Random pointer (must not be NULL — contract)
 * @param min Lower bound (inclusive)
 * @param max Upper bound (exclusive)
 * @return f64 in [min, max)
 */
static inline f64 random_f64_range(borrowed(Random*) r, f64 min, f64 max) {
    return min + random_f64(r) * (max - min);
}

#endif /* CANON_UTIL_RANDOM_H */
