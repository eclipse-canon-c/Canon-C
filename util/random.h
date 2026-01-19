// util/random.h
#ifndef CANON_UTIL_RANDOM_H
#define CANON_UTIL_RANDOM_H

#include <stdint.h>

/**
 * @file random.h
 * @brief Fast, high-quality, fully explicit PCG32 pseudo-random number generator
 *
 * Implements the PCG32 algorithm (Permuted Congruential Generator), one of
 * the best small-footprint PRNGs available for non-cryptographic use. Provides
 * fast generation with excellent statistical properties and full deterministic
 * control over random streams.
 *
 * Portability:
 *   - Requires C99 or later (for uint64_t, uint32_t, inline functions)
 *   - Pure integer arithmetic - no platform-specific instructions
 *   - Works on any architecture (32-bit, 64-bit, big/little endian)
 *   - No external dependencies (no libc rand(), no system entropy)
 *   - Deterministic across all platforms (same seed → same sequence)
 *
 * Thread-safety: Functions are reentrant. Each Random instance is independent.
 *                Safe to use from multiple threads if each thread has its own
 *                Random instance. NOT safe if multiple threads share one instance
 *                without synchronization (state is mutated on each call).
 *
 * Performance:
 *   - Time complexity: O(1) per random number generated
 *   - Space complexity: O(1) - 16 bytes per Random instance
 *   - Operations per number: 1 multiply, 1 add, 3 shifts, 2 xors, 1 rotate
 *   - Extremely fast: typically 2-5 CPU cycles per random number
 *   - No branches in hot path (except random_range rejection loop)
 *   - Cache-friendly: entire state fits in single cache line
 *   - No system calls, no locking, no allocations
 *
 * Core ideas:
 * ────────────────────────────────────────────────────────────────────────────
 * - Explicit over implicit — no global state, caller owns Random instance
 * - Deterministic by design — same seed always produces same sequence
 * - Multiple independent streams — via 64-bit sequence parameter
 * - Statistical excellence — passes rigorous statistical test suites
 * - Practical performance — one of fastest high-quality PRNGs available
 * - Zero hidden magic — all state visible, all operations traceable
 * - Not cryptographic — use /dev/urandom or OpenSSL for security-critical needs
 * - Reproducibility first — perfect for simulations, testing, procedural generation
 * - Unbiased sampling — rejection method ensures true uniform distribution
 * - Safe defaults — NULL pointers return 0 instead of crashing
 *
 * What is PCG32?
 * ────────────────────────────────────────────────────────────────────────────
 * PCG (Permuted Congruential Generator) is a family of PRNGs designed by
 * Melissa O'Neill. PCG32 is the most popular variant:
 *
 * - Internal state: 64 bits (Linear Congruential Generator)
 * - Output: 32 bits per call (permuted via xorshift and rotation)
 * - Period: 2^64 (each stream has 2^64 unique outputs before repeating)
 * - Streams: 2^63 independent sequences (via sequence parameter)
 * - Quality: Passes TestU01 BigCrush, PractRand (tested to 16TB+)
 *
 * PCG32 combines:
 * - LCG for long period and simple state update (multiply-add)
 * - Permutation function to eliminate LCG's known statistical weaknesses
 * - Stream selection to create independent parallel generators
 *
 * Why PCG32 over alternatives?
 * ────────────────────────────────────────────────────────────────────────────
 * Compared to common alternatives:
 *
 * vs libc rand():
 *   + Much better statistical quality (rand() often fails basic tests)
 *   + Explicit state (no global variable)
 *   + Longer period (2^64 vs typically 2^31 or 2^48)
 *   + Faster on modern CPUs
 *   + Deterministic across platforms (rand() varies by implementation)
 *
 * vs Mersenne Twister (MT19937):
 *   + 390 bytes smaller state (16 vs 2500 bytes)
 *   + Much faster to seed (2 calls vs hundreds)
 *   + Better statistical quality in high dimensions
 *   + Faster on modern CPUs
 *   - Shorter period (2^64 vs 2^19937, but 2^64 is plenty for most uses)
 *
 * vs xorshift variants:
 *   + Better statistical quality (many xorshift variants fail tests)
 *   + Multiple independent streams via sequence parameter
 *   + Slightly slower but negligible difference
 *
 * vs cryptographic RNGs (AES-CTR, ChaCha20):
 *   + 10-100x faster
 *   - NOT cryptographically secure (predictable from output)
 *   - Don't use for keys, tokens, passwords, or security
 *
 * Deterministic behavior and reproducibility:
 * ────────────────────────────────────────────────────────────────────────────
 * PCG32 is completely deterministic. Given the same seed and sequence, it will
 * always produce the exact same sequence of random numbers, regardless of:
 * - Platform (x86, ARM, etc.)
 * - Operating system (Windows, Linux, macOS)
 * - Compiler (GCC, Clang, MSVC)
 * - Time of day
 * - Process ID
 * - Any other external factors
 *
 * This makes it perfect for:
 * - Reproducible testing (fixed seed for CI/tests)
 * - Procedural generation (same seed → same world)
 * - Debugging (replay exact sequence)
 * - Parallel simulations (different streams per thread)
 * - Monte Carlo methods (verify results across runs)
 *
 * Example:
 * ```c
 * Random rng1, rng2;
 * random_seed(&rng1, 12345, 0xABCD);
 * random_seed(&rng2, 12345, 0xABCD);
 * 
 * // These will produce IDENTICAL sequences
 * for (int i = 0; i < 1000; i++) {
 *     assert(random_u32(&rng1) == random_u32(&rng2));
 * }
 * ```
 *
 * Multiple independent streams:
 * ────────────────────────────────────────────────────────────────────────────
 * The sequence parameter allows creating 2^63 independent random streams.
 * Each stream has its own 2^64 period, giving enormous total coverage.
 *
 * Use cases:
 * - One stream per game entity (player, enemy, chest)
 * - One stream per simulation thread
 * - One stream per Monte Carlo trial
 * - Hierarchical generation (terrain stream, vegetation stream, etc.)
 *
 * Example:
 * ```c
 * Random player_rng, enemy_rng;
 * uint64_t base_seed = 12345;
 * 
 * random_seed(&player_rng, base_seed, 0x1111);  // Player stream
 * random_seed(&enemy_rng, base_seed, 0x2222);   // Enemy stream
 * 
 * // Completely independent sequences even with same base seed
 * uint32_t player_crit = random_range(&player_rng, 100);
 * uint32_t enemy_crit = random_range(&enemy_rng, 100);
 * ```
 *
 * Seeding strategies:
 * ────────────────────────────────────────────────────────────────────────────
 * Choose seeding approach based on your needs:
 *
 * REPRODUCIBLE (testing, procedural generation):
 *   random_seed(&rng, 42, 0xDEADBEEF);
 *   - Fixed seed and sequence
 *   - Same results every run
 *   - Perfect for unit tests, CI/CD
 *
 * PSEUDO-RANDOM (games, simulations):
 *   random_seed(&rng, time(NULL), 0);
 *   - Changes each run (based on time)
 *   - Still deterministic within single run
 *   - Good for most game/simulation needs
 *
 * BETTER PSEUDO-RANDOM (when time alone isn't enough):
 *   uint64_t seed = (uint64_t)time(NULL) ^ ((uint64_t)clock() << 32);
 *   random_seed(&rng, seed, getpid());
 *   - Combines multiple entropy sources
 *   - Less likely to repeat in rapid succession
 *
 * CRYPTOGRAPHIC (security-sensitive):
 *   DON'T use this library! Use:
 *   - /dev/urandom (Unix)
 *   - CryptGenRandom (Windows)
 *   - OpenSSL, libsodium, etc.
 *
 * Statistical quality:
 * ────────────────────────────────────────────────────────────────────────────
 * PCG32 passes all major statistical test suites:
 *
 * - TestU01 SmallCrush: PASS (all tests)
 * - TestU01 Crush: PASS (all tests)
 * - TestU01 BigCrush: PASS (all tests) - most rigorous battery
 * - PractRand: PASS at 16TB+ output
 * - Dieharder: PASS (all tests)
 *
 * This means:
 * - Numbers are uniformly distributed
 * - No detectable patterns in output
 * - Independent across dimensions
 * - Suitable for Monte Carlo simulations
 * - Suitable for games, procedural generation
 * - NOT suitable for cryptography
 *
 * Unbiased range generation:
 * ────────────────────────────────────────────────────────────────────────────
 * The random_range() function uses rejection sampling to ensure perfectly
 * uniform distribution even when bound is not a power of 2.
 *
 * Naive approach (BIASED):
 *   return random_u32(r) % bound;  // Wrong!
 *
 * Problem: If 2^32 doesn't divide evenly by bound, some values appear more
 * often than others. For example, random_u32() % 3 gives:
 *   - 0 appears 1431655766 times
 *   - 1 appears 1431655765 times  (bias!)
 *   - 2 appears 1431655765 times  (bias!)
 *
 * Correct approach (rejection sampling):
 *   threshold = -bound % bound;
 *   do {
 *       value = random_u32(r);
 *   } while (value < threshold);
 *   return value % bound;
 *
 * This rejects values in the "biased zone" and ensures perfect uniformity.
 * The rejection loop rarely runs more than once (< 50% probability).
 *
 * Typical use cases:
 * ────────────────────────────────────────────────────────────────────────────
 * - Game development (loot drops, critical hits, spawning)
 * - Procedural generation (terrain, dungeons, levels)
 * - Simulations (Monte Carlo, agent behavior)
 * - Testing (random test cases, fuzzing)
 * - Algorithms (randomized quicksort, skip lists)
 * - Sampling (random selection from collections)
 * - Noise generation (combined with hash functions)
 * - AI behavior (decision randomization)
 * - Shuffle algorithms (Fisher-Yates)
 * - Random walks and stochastic processes
 *
 * NOT suitable for:
 * - Cryptographic key generation
 * - Password generation
 * - Security tokens
 * - Authentication nonces
 * - Any security-critical randomness
 *
 * Usage examples:
 *
 * Basic initialization and generation:
 * ```c
 * Random rng;
 * random_seed(&rng, 12345, 0);
 * 
 * // Generate some random numbers
 * uint32_t dice = random_range(&rng, 6) + 1;     // 1-6
 * uint32_t percent = random_range(&rng, 100);    // 0-99
 * double normalized = random_double(&rng);        // 0.0-1.0
 * ```
 *
 * Reproducible testing:
 * ```c
 * void test_combat_system(void) {
 *     Random rng;
 *     random_seed(&rng, 42, 0);  // Fixed seed
 *     
 *     // Combat will be identical every test run
 *     int damage = calculate_damage(&rng);
 *     assert(damage == 15);  // Always 15 with this seed
 * }
 * ```
 *
 * Multiple independent streams:
 * ```c
 * Random world_gen, loot_gen, enemy_ai;
 * 
 * random_seed(&world_gen, time(NULL), 0x1111);
 * random_seed(&loot_gen,  time(NULL), 0x2222);
 * random_seed(&enemy_ai,  time(NULL), 0x3333);
 * 
 * // Each has independent random sequence
 * ```
 *
 * Floating-point ranges:
 * ```c
 * // Random float in [min, max)
 * double random_float_range(Random* r, double min, double max) {
 *     return min + random_double(r) * (max - min);
 * }
 * 
 * double speed = random_float_range(&rng, 5.0, 10.0);  // 5.0-10.0
 * ```
 */

/**
 * @brief PCG32 generator state (128 bits total)
 *
 * Opaque structure containing the complete state of a PCG32 generator.
 * Do NOT modify fields directly — always use the provided functions.
 *
 * Fields:
 * - state: Current internal LCG state (updated each generation)
 * - inc:   Stream constant (always odd, determines which stream)
 *
 * Size: 16 bytes (fits in single cache line on most architectures)
 *
 * Lifecycle:
 * 1. Declare: Random rng;
 * 2. Initialize: random_seed(&rng, seed, seq);
 * 3. Use: random_u32(&rng), random_range(&rng, N), etc.
 * 4. No cleanup needed (no resources to free)
 *
 * Example:
 * ```c
 * Random rng;  // Uninitialized state
 * random_seed(&rng, 12345, 0);  // Now ready to use
 * uint32_t value = random_u32(&rng);  // Generate numbers
 * ```
 *
 * Thread-safety:
 * - Each Random instance is independent
 * - Safe to use different instances from different threads
 * - NOT safe to share one instance across threads without locking
 *
 * Copying:
 * ```c
 * Random rng1;
 * random_seed(&rng1, 12345, 0);
 * 
 * Random rng2 = rng1;  // Copy creates identical generator
 * assert(random_u32(&rng1) == random_u32(&rng2));  // Same output
 * ```
 */
typedef struct {
    uint64_t state;  ///< Current internal state (LCG state, updated each step)
    uint64_t inc;    ///< Sequence/stream constant (always odd, determines stream)
} Random;

/* ────────────────────────────────────────────────────────────────────────────
   Initialization
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Seeds or reseeds the random number generator
 *
 * Initializes a Random instance with the given seed and sequence values.
 * After calling this function, the generator will produce a deterministic
 * sequence of random numbers based on these parameters.
 *
 * The same (seed, sequence) pair will ALWAYS produce the exact same sequence
 * of random numbers, regardless of platform, compiler, or time.
 *
 * Algorithm:
 *   - Set initial state to 0
 *   - Set increment to (seq << 1) | 1 (ensures odd number)
 *   - Perform two warm-up iterations to mix the seed
 *   - Result: generator is ready with good initial state
 *
 * @param r     Pointer to Random instance to initialize (NULL-safe: no-op)
 * @param seed  64-bit arbitrary seed value (starting point for sequence)
 * @param seq   64-bit sequence/stream identifier (selects which stream)
 *
 * Preconditions:
 *   - r should point to valid Random struct (or be NULL for no-op)
 *
 * Postconditions:
 *   - If r is not NULL: generator is fully initialized and ready to use
 *   - If r is NULL: function does nothing (safe no-op)
 *   - Generator will produce deterministic sequence based on seed and seq
 *
 * Parameters explained:
 *   - seed: Determines where in the stream you start
 *     - Different seeds → different random sequences
 *     - Same seed + same seq → identical sequences
 *     - Use time(NULL) for "random" behavior
 *     - Use fixed value for reproducibility
 *
 *   - seq: Selects which independent stream to use
 *     - Different seq → completely independent sequences
 *     - Same seed + different seq → uncorrelated outputs
 *     - Lowest bit forced to 1 (ensures valid stream)
 *     - Use different seq for each independent generator
 *
 * Performance:
 *   - Time: O(1) - constant time (2 RNG iterations + setup)
 *   - Space: O(1) - no allocations
 *   - Very fast: typically <10 CPU cycles
 *
 * Thread-safety: Safe to call from any thread (no shared state)
 *
 * Example - Basic seeding:
 * ```c
 * Random rng;
 * random_seed(&rng, 12345, 0);
 * // Generator ready, will produce deterministic sequence
 * ```
 *
 * Example - Time-based seeding:
 * ```c
 * Random rng;
 * random_seed(&rng, time(NULL), 0);
 * // Different sequence each run (based on current time)
 * ```
 *
 * Example - Multiple independent streams:
 * ```c
 * Random player_rng, enemy_rng, world_rng;
 * uint64_t base_seed = 12345;
 * 
 * random_seed(&player_rng, base_seed, 1);
 * random_seed(&enemy_rng,  base_seed, 2);
 * random_seed(&world_rng,  base_seed, 3);
 * 
 * // All use same base seed but have independent sequences
 * ```
 *
 * Example - Reseeding:
 * ```c
 * Random rng;
 * random_seed(&rng, 100, 0);
 * uint32_t a = random_u32(&rng);
 * 
 * random_seed(&rng, 100, 0);  // Reseed with same values
 * uint32_t b = random_u32(&rng);
 * 
 * assert(a == b);  // Produces same value after reseed
 * ```
 *
 * Example - Procedural generation:
 * ```c
 * // World generation - same seed = same world
 * void generate_world(int world_id) {
 *     Random rng;
 *     random_seed(&rng, world_id, 0);
 *     
 *     // Generate terrain, biomes, etc.
 *     // Results are deterministic based on world_id
 * }
 * ```
 *
 * Recommended practices:
 * - Use 0 for seq if you only need one stream
 * - Use odd numbers for seq if you pick manually
 * - Use entity ID or thread ID for seq in multi-stream scenarios
 * - For testing: use small, memorable seeds (42, 12345, etc.)
 * - For production: use time(NULL) or better entropy source
 *
 * Common pitfall - forgetting to seed:
 * ```c
 * Random rng;  // Uninitialized!
 * uint32_t x = random_u32(&rng);  // Undefined behavior
 * 
 * // Correct:
 * Random rng;
 * random_seed(&rng, 12345, 0);  // Initialize first
 * uint32_t x = random_u32(&rng);  // Now safe
 * ```
 */
static inline void random_seed(Random* r, uint64_t seed, uint64_t seq) {
    if (!r) return;

    r->state = 0U;
    r->inc = (seq << 1u) | 1u;  // Force odd (required for valid stream)

    // Warm-up: two iterations recommended for PCG family
    // This ensures good mixing of the seed into internal state
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
 * Core PCG32 generation function. Produces a uniformly distributed 32-bit
 * unsigned integer and advances the internal state. This is the fundamental
 * building block - all other random functions use this.
 *
 * Algorithm (PCG32 permutation function):
 *   1. Save current state (64-bit)
 *   2. Advance LCG: state = state * multiplier + increment
 *   3. XOR-shift the old state to mix bits
 *   4. Rotate result based on high bits of old state
 *   5. Return permuted 32-bit output
 *
 * The permutation step (xorshift + rotation) eliminates the linear
 * congruential generator's well-known statistical defects while preserving
 * its long period and fast state update.
 *
 * @param r Valid Random pointer (NULL-safe: returns 0)
 *
 * @return  Uniformly distributed uint32_t in full range [0, UINT32_MAX]
 *          Returns 0 if r is NULL (safe default)
 *
 * Preconditions:
 *   - r should point to initialized Random instance
 *   - Random should be seeded via random_seed() before first call
 *
 * Postconditions:
 *   - Internal state advanced to next value
 *   - Returns statistically uniform 32-bit value
 *   - Calling again produces next value in sequence
 *
 * Distribution:
 *   - All 2^32 possible values equally likely
 *   - No bias toward any particular value
 *   - Passes all statistical tests for uniformity
 *
 * Performance:
 *   - Time: O(1) - constant time, extremely fast
 *   - Operations: 1 multiply, 1 add, 3 shifts, 2 xors, 1 rotate
 *   - Typical: 2-5 CPU cycles on modern processors
 *   - No branches in critical path
 *   - Space: O(1) - no allocations
 *
 * Thread-safety: Safe if r is not shared across threads
 *
 * Example - Basic generation:
 * ```c
 * Random rng;
 * random_seed(&rng, 12345, 0);
 * 
 * uint32_t x = random_u32(&rng);  // e.g., 2845213478
 * uint32_t y = random_u32(&rng);  // e.g., 1591842019 (different)
 * uint32_t z = random_u32(&rng);  // e.g., 3927401847 (different)
 * ```
 *
 * Example - Generating many values:
 * ```c
 * Random rng;
 * random_seed(&rng, 42, 0);
 * 
 * for (int i = 0; i < 1000000; i++) {
 *     uint32_t value = random_u32(&rng);
 *     process(value);
 * }
 * ```
 *
 * Example - Using full 32-bit range:
 * ```c
 * // Random 32-bit hash/ID
 * uint32_t random_id = random_u32(&rng);
 * 
 * // Random 32-bit bitmask
 * uint32_t flags = random_u32(&rng);
 * 
 * // Random memory address (in 32-bit space)
 * void* random_ptr = (void*)(uintptr_t)random_u32(&rng);
 * ```
 *
 * Example - NULL safety:
 * ```c
 * Random* rng = NULL;
 * uint32_t x = random_u32(rng);  // x = 0 (doesn't crash)
 * ```
 *
 * Use cases:
 * - Base for all other random generation functions
 * - Direct use when you need full 32-bit range
 * - Building custom distributions
 * - Hash functions and fingerprints
 * - Bit manipulation and flags
 *
 * Common pattern - converting to other ranges:
 * ```c
 * // DON'T do this (biased):
 * uint32_t bad = random_u32(&rng) % 100;
 * 
 * // DO use random_range instead (unbiased):
 * uint32_t good = random_range(&rng, 100);
 * ```
 */
static inline uint32_t random_u32(Random* r) {
    if (!r) return 0;

    // Save old state for permutation step
    uint64_t oldstate = r->state;
    
    // Advance internal LCG state
    // Multiplier: 6364136223846793005 (chosen for good statistical properties)
    r->state = oldstate * 6364136223846793005ULL + r->inc;

    // Permutation step: xorshift
    // Shift right by 18, XOR with original, shift right by 27
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    
    // Rotation amount from high bits
    uint32_t rot = (uint32_t)(oldstate >> 59u);

    // Final rotation permutation
    // This is equivalent to: rotate_right(xorshifted, rot)
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

/* ────────────────────────────────────────────────────────────────────────────
   Convenient derived distributions
   ──────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Uniform random integer in range [0, bound)
 *
 * Generates a uniformly distributed random integer in the half-open interval
 * [0, bound). Uses rejection sampling to ensure perfect uniformity with zero
 * bias, even when bound is not a power of 2.
 *
 * This is the preferred way to get random integers in a specific range.
 * Do NOT use random_u32() % bound as it introduces bias.
 *
 * Algorithm (unbiased rejection sampling):
 *   1. Calculate threshold = -bound % bound (size of biased zone)
 *   2. Generate random value
 *   3. If value < threshold, reject and try again (in biased zone)
 *   4. Otherwise, return value % bound
 *
 * The rejection loop rarely iterates more than once. Probability of
 * rejection is at most 50% and typically much less.
 *
 * @param r     Valid Random pointer (NULL-safe: returns 0)
 * @param bound Upper bound (exclusive) - must be > 0
 *              Range: [1, UINT32_MAX]
 *
 * @return      Value uniformly distributed in [0, bound-1]
 *              Returns 0 if r is NULL or bound is 0
 *
 * Preconditions:
 *   - r should point to initialized Random instance
 *   - bound should be > 0 for meaningful result
 *
 * Postconditions:
 *   - Return value is in range [0, bound-1]
 *   - All values in range are equally likely
 *   - Perfect uniform distribution (no bias)
 *
 * Performance:
 *   - Time: O(1) expected (rejection loop almost never runs twice)
 *   - Worst case: O(infinity) theoretical, but probability → 0
 *   - Typical: Same as random_u32() plus one modulo operation
 *   - Expected iterations: < 2 for all bounds
 *
 * Thread-safety: Safe if r is not shared across threads
 *
 * Example - Dice roll:
 * ```c
 * Random rng;
 * random_seed(&rng, time(NULL), 0);
 * 
 * uint32_t d6 = random_range(&rng, 6) + 1;    // 1-6
 * uint32_t d20 = random_range(&rng, 20) + 1;  // 1-20
 * uint32_t d100 = random_range(&rng, 100) + 1;// 1-100
 * ```
 *
 * Example - Array index:
 * ```c
 * int values[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
 * uint32_t idx = random_range(&rng, 10);  // 0-9
 * int random_value = values[idx];
 * ```
 *
 * Example - Percentage/probability:
 * ```c
 * uint32_t percent = random_range(&rng, 100);  // 0-99
 * 
 * if (percent < 25) {
 *     // 25% chance
 * } else if (percent < 75) {
 *     // 50% chance
 * } else {
 *     // 25% chance
 * }
 * ```
 *
 * Example - Random selection:
 * ```c
 * const char* enemies[] = {"Goblin", "Orc", "Troll", "Dragon"};
 * size_t count = sizeof(enemies) / sizeof(enemies[0]);
 * 
 * uint32_t idx = random_range(&rng, count);
 * printf("You encounter a %s!\n", enemies[idx]);
 * ```
 *
 * Example - Loot drop:
 * ```c
 * uint32_t roll = random_range(&rng, 100);
 * 
 * if (roll < 1) {
 *     drop_legendary();  // 1% chance
 * } else if (roll < 10) {
 *     drop_rare();       // 9% chance
 * } else if (roll < 40) {
 *     drop_uncommon();   // 30% chance
 * } else {
 *     drop_common();     // 60% chance
 * }
 * ```
 *
 * Example - Shuffling (Fisher-Yates):
 * ```c
 * void shuffle(int* array, size_t n, Random* rng) {
 *     for (size_t i = n - 1; i > 0; i--) {
 *         size_t j = random_range(rng, i + 1);
 *         int temp = array[i];
 *         array[i] = array[j];
 *         array[j] = temp;
 *     }
 * }
 * ```
 *
 * Example - Random spawn position:
 * ```c
 * int map_width = 1000;
 * int map_height = 800;
 * 
 * int x = random_range(&rng, map_width);   // 0-999
 * int y = random_range(&rng, map_height);  // 0-799
 * spawn_entity(x, y);
 * ```
 *
 * Why rejection sampling is necessary:
 * ```c
 * // WRONG - Biased distribution:
 * uint32_t bad = random_u32(&rng) % 3;
 * // Values: 0 appears 1431655766 times
 * //         1 appears 1431655765 times (1 less!)
 * //         2 appears 1431655765 times (1 less!)
 * 
 * // CORRECT - Perfectly uniform:
 * uint32_t good = random_range(&rng, 3);
 * // All values appear exactly equal number of times
 * ```
 *
 * Common patterns:
 * ```c
 * // Inclusive range [min, max]
 * uint32_t random_range_inclusive(Random* r, uint32_t min, uint32_t max) {
 *     return min + random_range(r, max - min + 1);
 * }
 * 
 * // Boolean (true/false)
 * bool random_bool(Random* r) {
 *     return random_range(r, 2) == 1;
 * }
 * 
 * // Weighted choice (weights must sum to positive value)
 * int random_weighted(Random* r, int* weights, size_t count) {
 *     int total = 0;
 *     for (size_t i = 0; i < count; i++) total += weights[i];
 *     
 *     int roll = random_range(r, total);
 *     for (size_t i = 0; i < count; i++) {
 *         if (roll < weights[i]) return i;
 *         roll -= weights[i];
 *     }
 *     return count - 1;
 * }
 * ```
 */
static inline uint32_t random_range(Random* r, uint32_t bound) {
    if (!r || bound == 0) return 0;

    // Calculate threshold for rejection sampling
    // -bound % bound gives size of the "biased zone"
    uint32_t threshold = -bound % bound;
    uint32_t r32;

    // Rejection loop: discard values in biased zone
    // Expected iterations: < 2 for all bounds
    do {
        r32 = random_u32(r);
    } while (r32 < threshold);

    // Now r32 is in unbiased zone, safe to use modulo
    return r32 % bound;
}

/**
 * @brief Uniform random double in [0.0, 1.0)
 *
 * Generates a uniformly distributed double-precision floating-point number
 * in the half-open interval [0.0, 1.0). This is the standard normalized
 * random number, useful for probabilities and continuous distributions.
 *
 * Algorithm:
 *   - Generate 32-bit random integer
 *   - Divide by 2^32 to get value in [0.0, 1.0)
 *   - Uses exact divisor 4294967296.0 (exactly 2^32)
 *
 * The result has 32 bits of randomness in the mantissa. This is sufficient
 * for most applications but less than double's full 53-bit precision.
 *
 * @param r Valid Random pointer (NULL-safe: returns 0.0)
 *
 * @return  Double-precision value uniformly distributed in [0.0, 1.0)
 *          Returns 0.0 if r is NULL
 *
 * Preconditions:
 *   - r should point to initialized Random instance
 *
 * Postconditions:
 *   - Return value is in range [0.0, 1.0)
 *   - Can return 0.0, never returns 1.0
 *   - Uniformly distributed (to 32-bit precision)
 *
 * Performance:
 *   - Time: O(1) - one random_u32() plus one float division
 *   - Very fast on modern CPUs with FPU
 *
 * Thread-safety: Safe if r is not shared across threads
 *
 * Example - Basic probability:
 * ```c
 * Random rng;
 * random_seed(&rng, time(NULL), 0);
 * 
 * double prob = random_double(&rng);
 * if (prob < 0.3) {
 *     printf("30%% event occurred\n");
 * }
 * ```
 *
 * Example - Random float in range:
 * ```c
 * // Random double in [min, max)
 * double random_range_double(Random* r, double min, double max) {
 *     return min + random_double(r) * (max - min);
 * }
 * 
 * double speed = random_range_double(&rng, 5.0, 10.0);  // 5.0-10.0
 * double angle = random_range_double(&rng, 0.0, 2.0 * M_PI);  // 0-2π
 * ```
 *
 * Example - Monte Carlo simulation:
 * ```c
 * // Estimate π using Monte Carlo
 * size_t inside = 0;
 * size_t total = 1000000;
 * 
 * for (size_t i = 0; i < total; i++) {
 *     double x = random_double(&rng);
 *     double y = random_double(&rng);
 *     if (x*x + y*y < 1.0) inside++;
 * }
 * 
 * double pi_estimate = 4.0 * inside / total;
 * ```
 *
 * Example - Weighted probability:
 * ```c
 * double roll = random_double(&rng);
 * 
 * if (roll < 0.6) {
 *     // 60% chance
 *     action_common();
 * } else if (roll < 0.9) {
 *     // 30% chance (0.9 - 0.6)
 *     action_uncommon();
 * } else {
 *     // 10% chance (1.0 - 0.9)
 *     action_rare();
 * }
 * ```
 *
 * Example - Gaussian distribution (Box-Muller):
 * ```c
 * // Generate normally distributed random number
 * double random_gaussian(Random* r, double mean, double stddev) {
 *     double u1 = random_double(r);
 *     double u2 = random_double(r);
 *     
 *     double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
 *     return mean + z0 * stddev;
 * }
 * ```
 *
 * Example - Random point in circle:
 * ```c
 * void random_point_in_circle(Random* r, double* x, double* y, double radius) {
 *     double angle = random_double(r) * 2.0 * M_PI;
 *     double r_sqrt = sqrt(random_double(r)) * radius;
 *     
 *     *x = r_sqrt * cos(angle);
 *     *y = r_sqrt * sin(angle);
 * }
 * ```
 *
 * Example - Acceptance-rejection sampling:
 * ```c
 * // Sample from custom distribution
 * double sample_custom_distribution(Random* rng) {
 *     double x, y;
 *     do {
 *         x = random_double(rng);
 *         y = random_double(rng);
 *     } while (y > custom_pdf(x));  // Accept if y under PDF
 *     return x;
 * }
 * ```
 *
 * Precision note:
 * - Result has 32 bits of randomness
 * - Double mantissa is 53 bits
 * - For full 53-bit precision, you'd need to combine two random_u32() calls
 * - 32 bits is sufficient for most applications
 *
 * Common patterns:
 * ```c
 * // Random boolean with custom probability
 * bool random_chance(Random* r, double probability) {
 *     return random_double(r) < probability;
 * }
 * 
 * // Random sign (±1)
 * double random_sign(Random* r) {
 *     return random_double(r) < 0.5 ? -1.0 : 1.0;
 * }
 * 
 * // Exponential distribution
 * double random_exponential(Random* r, double lambda) {
 *     return -log(1.0 - random_double(r)) / lambda;
 * }
 * ```
 */
static inline double random_double(Random* r) {
    // Divide by exactly 2^32 to get [0.0, 1.0)
    return (double)random_u32(r) / 4294967296.0;
}

/* ────────────────────────────────────────────────────────────────────────────
   Complete Usage Examples
   ──────────────────────────────────────────────────────────────────────────

    #include "util/random.h"
    #include <stdio.h>
    #include <time.h>
    #include <math.h>
    
    // Example 1: Basic random number generation
    void example_basic(void) {
        Random rng;
        random_seed(&rng, 12345, 0);
        
        printf("Random integers:\n");
        for (int i = 0; i < 5; i++) {
            printf("  %u\n", random_u32(&rng));
        }
        
        printf("\nRandom in range [0, 100):\n");
        for (int i = 0; i < 5; i++) {
            printf("  %u\n", random_range(&rng, 100));
        }
        
        printf("\nRandom doubles [0.0, 1.0):\n");
        for (int i = 0; i < 5; i++) {
            printf("  %.6f\n", random_double(&rng));
        }
    }
    
    // Example 2: Dice rolling
    void example_dice(void) {
        Random rng;
        random_seed(&rng, time(NULL), 0);
        
        // Roll various dice
        uint32_t d6 = random_range(&rng, 6) + 1;
        uint32_t d20 = random_range(&rng, 20) + 1;
        
        printf("Rolled d6: %u\n", d6);
        printf("Rolled d20: %u\n", d20);
        
        // Roll 3d6
        uint32_t sum = 0;
        for (int i = 0; i < 3; i++) {
            sum += random_range(&rng, 6) + 1;
        }
        printf("Rolled 3d6: %u\n", sum);
    }
    
    // Example 3: Reproducible sequences
    void example_reproducible(void) {
        Random rng1, rng2;
        
        // Same seed → same sequence
        random_seed(&rng1, 42, 0);
        random_seed(&rng2, 42, 0);
        
        printf("Both generators produce identical sequences:\n");
        for (int i = 0; i < 5; i++) {
            uint32_t a = random_u32(&rng1);
            uint32_t b = random_u32(&rng2);
            printf("  %u == %u: %s\n", a, b, a == b ? "✓" : "✗");
        }
    }
    
    // Example 4: Multiple independent streams
    void example_streams(void) {
        Random player, enemy, loot;
        uint64_t base_seed = time(NULL);
        
        random_seed(&player, base_seed, 0x1111);
        random_seed(&enemy,  base_seed, 0x2222);
        random_seed(&loot,   base_seed, 0x3333);
        
        printf("Independent streams:\n");
        printf("  Player: %u\n", random_range(&player, 100));
        printf("  Enemy:  %u\n", random_range(&enemy, 100));
        printf("  Loot:   %u\n", random_range(&loot, 100));
    }
    
    // Example 5: Fisher-Yates shuffle
    void shuffle_array(int* arr, size_t n, Random* rng) {
        for (size_t i = n - 1; i > 0; i--) {
            size_t j = random_range(rng, i + 1);
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    
    void example_shuffle(void) {
        Random rng;
        random_seed(&rng, time(NULL), 0);
        
        int deck[52];
        for (int i = 0; i < 52; i++) deck[i] = i;
        
        shuffle_array(deck, 52, &rng);
        
        printf("Shuffled deck (first 10): ");
        for (int i = 0; i < 10; i++) {
            printf("%d ", deck[i]);
        }
        printf("\n");
    }
    
    // Example 6: Procedural generation
    typedef struct { int x, y, type; } Entity;
    
    void generate_level(int level_id, Entity* entities, size_t* count) {
        Random rng;
        random_seed(&rng, level_id, 0);  // Same level ID = same layout
        
        *count = 10 + random_range(&rng, 20);  // 10-29 entities
        
        for (size_t i = 0; i < *count; i++) {
            entities[i].x = random_range(&rng, 100);
            entities[i].y = random_range(&rng, 100);
            entities[i].type = random_range(&rng, 5);
        }
    }
    
    void example_procedural(void) {
        Entity entities[30];
        size_t count;
        
        generate_level(42, entities, &count);
        printf("Level 42 has %zu entities\n", count);
        
        // Generate again - identical!
        Entity entities2[30];
        size_t count2;
        generate_level(42, entities2, &count2);
        printf("Regenerated: %zu entities\n", count2);
    }
    
    // Example 7: Monte Carlo π estimation
    void example_monte_carlo(void) {
        Random rng;
        random_seed(&rng, time(NULL), 0);
        
        size_t inside = 0;
        size_t total = 1000000;
        
        for (size_t i = 0; i < total; i++) {
            double x = random_double(&rng);
            double y = random_double(&rng);
            if (x*x + y*y < 1.0) inside++;
        }
        
        double pi_est = 4.0 * inside / total;
        printf("π estimate: %.6f (error: %.6f)\n", pi_est, fabs(pi_est - M_PI));
    }
    
    // Example 8: Weighted random selection
    int random_weighted(Random* rng, int* weights, size_t count) {
        int total = 0;
        for (size_t i = 0; i < count; i++) total += weights[i];
        
        int roll = random_range(rng, total);
        for (size_t i = 0; i < count; i++) {
            if (roll < weights[i]) return i;
            roll -= weights[i];
        }
        return count - 1;
    }
    
    void example_weighted(void) {
        Random rng;
        random_seed(&rng, time(NULL), 0);
        
        const char* items[] = {"Common", "Uncommon", "Rare", "Legendary"};
        int weights[] = {60, 30, 9, 1};  // 60%, 30%, 9%, 1%
        
        int counts[4] = {0};
        for (int i = 0; i < 10000; i++) {
            int idx = random_weighted(&rng, weights, 4);
            counts[idx]++;
        }
        
        printf("10000 drops:\n");
        for (int i = 0; i < 4; i++) {
            printf("  %s: %d (%.1f%%)\n", 
                   items[i], counts[i], 100.0 * counts[i] / 10000);
        }
    }
    
    // Example 9: Random range helpers
    double random_float_range(Random* r, double min, double max) {
        return min + random_double(r) * (max - min);
    }
    
    int random_int_range(Random* r, int min, int max) {
        return min + random_range(r, max - min + 1);
    }
    
    void example_ranges(void) {
        Random rng;
        random_seed(&rng, time(NULL), 0);
        
        double temp = random_float_range(&rng, -10.0, 35.0);
        int score = random_int_range(&rng, 1, 100);
        
        printf("Temperature: %.2f°C\n", temp);
        printf("Score: %d\n", score);
    }
    
    // Example 10: Gaussian distribution (Box-Muller)
    double random_gaussian(Random* r, double mean, double stddev) {
        double u1 = random_double(r);
        double u2 = random_double(r);
        double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
        return mean + z0 * stddev;
    }
    
    void example_gaussian(void) {
        Random rng;
        random_seed(&rng, 12345, 0);
        
        printf("Gaussian samples (mean=100, stddev=15):\n");
        for (int i = 0; i < 10; i++) {
            printf("  %.2f\n", random_gaussian(&rng, 100.0, 15.0));
        }
    }
    
    // Example 11: Probability checks
    bool random_chance(Random* r, double probability) {
        return random_double(r) < probability;
    }
    
    void example_probability(void) {
        Random rng;
        random_seed(&rng, time(NULL), 0);
        
        int successes = 0;
        for (int i = 0; i < 1000; i++) {
            if (random_chance(&rng, 0.25)) {  // 25% chance
                successes++;
            }
        }
        
        printf("25%% probability: %d/1000 successes\n", successes);
    }
    
    // Example 12: Random color generation
    typedef struct { uint8_t r, g, b; } Color;
    
    Color random_color(Random* rng) {
        Color c;
        c.r = random_range(rng, 256);
        c.g = random_range(rng, 256);
        c.b = random_range(rng, 256);
        return c;
    }
    
    void example_colors(void) {
        Random rng;
        random_seed(&rng, time(NULL), 0);
        
        for (int i = 0; i < 5; i++) {
            Color c = random_color(&rng);
            printf("Color: RGB(%u, %u, %u)\n", c.r, c.g, c.b);
        }
    }
    
    // Example 13: Random point in circle
    void random_point_in_circle(Random* r, double* x, double* y, double radius) {
        double angle = random_double(r) * 2.0 * M_PI;
        double r_val = sqrt(random_double(r)) * radius;
        *x = r_val * cos(angle);
        *y = r_val * sin(angle);
    }
    
    void example_circle(void) {
        Random rng;
        random_seed(&rng, 12345, 0);
        
        printf("Random points in circle (radius 10):\n");
        for (int i = 0; i < 5; i++) {
            double x, y;
            random_point_in_circle(&rng, &x, &y, 10.0);
            printf("  (%.2f, %.2f)\n", x, y);
        }
    }
    
    // Example 14: Testing randomness quality
    void example_uniformity_test(void) {
        Random rng;
        random_seed(&rng, time(NULL), 0);
        
        int buckets[10] = {0};
        int samples = 100000;
        
        for (int i = 0; i < samples; i++) {
            uint32_t val = random_range(&rng, 10);
            buckets[val]++;
        }
        
        printf("Uniformity test (%d samples in 10 buckets):\n", samples);
        for (int i = 0; i < 10; i++) {
            printf("  Bucket %d: %d (%.2f%%)\n", 
                   i, buckets[i], 100.0 * buckets[i] / samples);
        }
    }
    
    // Example 15: Game AI - random decisions
    typedef enum { ATTACK, DEFEND, FLEE, SPECIAL } Action;
    
    Action ai_decide(Random* rng, int health_percent) {
        if (health_percent < 20 && random_chance(rng, 0.7)) {
            return FLEE;  // 70% flee when low health
        }
        
        double roll = random_double(rng);
        if (roll < 0.5) return ATTACK;
        if (roll < 0.8) return DEFEND;
        return SPECIAL;
    }
    
    void example_ai(void) {
        Random rng;
        random_seed(&rng, time(NULL), 0);
        
        const char* action_names[] = {"ATTACK", "DEFEND", "FLEE", "SPECIAL"};
        
        printf("AI decisions at 100%% health:\n");
        for (int i = 0; i < 5; i++) {
            Action a = ai_decide(&rng, 100);
            printf("  %s\n", action_names[a]);
        }
        
        printf("\nAI decisions at 15%% health:\n");
        for (int i = 0; i < 5; i++) {
            Action a = ai_decide(&rng, 15);
            printf("  %s\n", action_names[a]);
        }
    }

   ──────────────────────────────────────────────────────────────────────────── */

#endif /* CANON_UTIL_RANDOM_H */
