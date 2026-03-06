/**
 * @file hashmap.h
 * @brief Generic Robin Hood open-addressed hashmap — Canon-C user-facing API
 *
 * This is the entry point for header-only usage. Including this file generates
 * a complete, statically-inlined hashmap implementation for your key and value
 * types. No .c file or build system integration is required.
 *
 * For separate compilation (external linkage), use hashmap_decl.h in headers
 * and hashmap_defn.h in exactly one .c file instead.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * QUICK START
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Step 1 — Provide a hash function and an equality function for your key type:
 * ```c
 * static u64 hash_u64(const u64* key, void* ctx) {
 *     (void)ctx;
 *     // Fibonacci hashing (fast, good distribution)
 *     u64 h = *key * 11400714819323198485ULL;
 *     return h == 0 ? 1 : h;  // 0 is reserved as the "empty" sentinel
 * }
 *
 * static bool eq_u64(const u64* a, const u64* b, void* ctx) {
 *     (void)ctx;
 *     return *a == *b;
 * }
 * ```
 *
 * Step 2 — Configure and include:
 * ```c
 * #define HASHMAP_KEY_TYPE u64
 * #define HASHMAP_VAL_TYPE int
 * #define HASHMAP_HASH_FN  hash_u64
 * #define HASHMAP_EQ_FN    eq_u64
 * #include "data/hashmap/hashmap.h"
 * ```
 *
 * Step 3 — Use:
 * ```c
 * // Size your buffer: at minimum capacity * sizeof(hashmap_slot)
 * // Use bits_next_power_of_two() to round up to a power of 2
 * // Use hashmap_buffer_size(cap) to get exact byte count
 * usize cap = bits_next_power_of_two(100 * 4 / 3 + 1);  // 100 elements at 75% load → 256
 * u8 buf[hashmap_buffer_size(cap)];                       // stack or static allocation
 * // or: u8* buf = malloc(hashmap_buffer_size(cap));      // heap allocation
 *
 * hashmap map;
 * result_bool_Error init_res = hashmap_init(&map, bytes_from(buf, sizeof(buf)), cap, NULL);
 * if (result_bool_Error_is_err(init_res)) { ... handle error ... }
 *
 * // Insert
 * u64 key = 42; int val = 100;
 * result_bool_Error ins = hashmap_insert(&map, &key, &val);
 * // ins is Ok(true) if new key, Ok(false) if key already existed (value updated)
 *
 * // Lookup (returns copy)
 * option_int found = hashmap_get(&map, &key);
 * if (option_int_is_some(found)) {
 *     int v = option_int_unwrap(found);
 * }
 *
 * // Lookup (returns pointer — valid until next mutation)
 * int* vptr = hashmap_get_or_null(&map, &key);
 * if (vptr) { printf("%d\n", *vptr); }
 *
 * // Remove
 * result_int_Error rem = hashmap_remove(&map, &key);
 * if (result_int_Error_is_ok(rem)) {
 *     int old = result_int_Error_unwrap(rem);
 * }
 *
 * // Iterate
 * usize iter = 0;
 * const u64* k; const int* v;
 * while (hashmap_iter_next(&map, &iter, &k, &v)) {
 *     printf("%llu => %d\n", (unsigned long long)*k, *v);
 * }
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * ALGORITHM: Robin Hood Open Addressing
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Open addressing stores all elements in a flat array — no linked lists,
 * no per-element allocation, no pointer chasing. Cache-friendly.
 *
 * Robin Hood hashing improves on linear probing by enforcing fairness:
 * during insertion, if the incoming element has probed farther from its
 * home slot than the resident element, they swap. The displaced element
 * continues probing. This keeps probe sequence lengths (PSL) short and
 * near-equal across all elements, bounding worst-case lookup time.
 *
 * Deletion uses backward shift (no tombstones): after removing a slot,
 * subsequent elements with PSL > 0 are shifted back one position,
 * restoring the invariant without degrading future lookups.
 *
 * Lookup early-exit: if a probe reaches a slot whose PSL is less than
 * the current probe distance, the key cannot exist (Robin Hood would have
 * displaced it). This terminates failed lookups faster than vanilla linear.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * CAPACITY AND LOAD FACTOR
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * - Capacity must be a power of 2 (checked at init time)
 * - hashmap_insert() returns ERR_CAPACITY_EXCEEDED when len >= cap * 3 / 4
 * - To store N elements safely: capacity >= bits_next_power_of_two(N * 4 / 3 + 1)
 * - hashmap_buffer_size(cap) gives the exact byte count for a given slot count
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * HASH AND EQUALITY FUNCTION GUIDELINES
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Hash function signature:
 *   u64 my_hash(const KeyType* key, void* ctx);
 *   - Must return a non-zero value (0 is reserved; the impl normalizes it to 1)
 *   - Must be deterministic: same key always produces same hash
 *   - Should distribute keys uniformly across [1, UINT64_MAX]
 *   - ctx is the optional context passed to hashmap_init(); may be NULL
 *
 * Equality function signature:
 *   bool my_eq(const KeyType* a, const KeyType* b, void* ctx);
 *   - Must be reflexive: my_eq(a, a) == true
 *   - Must be symmetric: my_eq(a, b) == my_eq(b, a)
 *   - Must be consistent with hash: if my_eq(a, b) then my_hash(a) == my_hash(b)
 *   - ctx is the optional context; may be NULL
 *
 * Built-in hash recipes (copy and adapt):
 * ```c
 * // Integer keys — Fibonacci hashing
 * static u64 hash_u64(const u64* k, void* ctx) {
 *     (void)ctx;
 *     u64 h = *k * 11400714819323198485ULL;
 *     return h ? h : 1;
 * }
 *
 * // String keys (FNV-1a over str_t)
 * static u64 hash_str(const str_t* s, void* ctx) {
 *     (void)ctx;
 *     u64 h = 14695981039346656037ULL;
 *     for (usize i = 0; i < s->len; i++) {
 *         h ^= (u8)s->ptr[i];
 *         h *= 1099511628211ULL;
 *     }
 *     return h ? h : 1;
 * }
 *
 * // Pointer identity
 * static u64 hash_ptr(const void** p, void* ctx) {
 *     (void)ctx;
 *     u64 h = (u64)(uintptr_t)*p * 11400714819323198485ULL;
 *     return h ? h : 1;
 * }
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * GENERATED API SUMMARY
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * (Assuming defaults: HASHMAP_TYPE_NAME = hashmap, HASHMAP_FN(x) = hashmap_x)
 *
 * Type:
 *   hashmap              — the map struct (caller owns, stack or heap)
 *   hashmap_slot         — internal slot (do not access directly)
 *
 * Sizing:
 *   hashmap_buffer_size(capacity)  → usize
 *     Returns the byte count for a flat buffer that can hold `capacity` slots.
 *     Call this to size your u8 array before hashmap_init().
 *
 * Lifecycle:
 *   hashmap_init(&map, buf, capacity, ctx)  → result_bool_Error
 *     Initializes map over caller-owned buf. Zeroes all slots. O(capacity).
 *
 *   hashmap_clear(&map)                     → void
 *     Removes all entries. Buffer and capacity unchanged. O(capacity).
 *
 * Queries:
 *   hashmap_len(&map)              → usize   — number of stored entries
 *   hashmap_capacity(&map)         → usize   — total slot count
 *   hashmap_is_empty(&map)         → bool    — true if len == 0
 *   hashmap_load_factor(&map)      → f64     — len / capacity in [0.0, 1.0]
 *
 * Mutation:
 *   hashmap_insert(&map, &key, &val)  → result_bool_Error
 *     Ok(true):  new key inserted
 *     Ok(false): key existed, value overwritten
 *     Err(ERR_CAPACITY_EXCEEDED): load >= 75%, no insertion performed
 *     Err(ERR_INVALID_ARG): any pointer is NULL
 *
 *   hashmap_remove(&map, &key)        → result_VAL_Error
 *     Ok(old_value): key found and removed, old value returned
 *     Err(ERR_NOT_FOUND): key does not exist
 *     Err(ERR_INVALID_ARG): any pointer is NULL
 *
 * Lookup:
 *   hashmap_get(&map, &key)           → option_VAL
 *     Some(value): key found, returns a copy of the value
 *     None:        key not found
 *
 *   hashmap_get_or_null(&map, &key)   → VAL* (borrowed) or NULL
 *     Returns a direct pointer into the slot — valid until next insert/remove.
 *     NULL if not found.
 *
 *   hashmap_contains_key(&map, &key)  → bool
 *
 * Iteration:
 *   hashmap_iter_next(&map, &iter, &key_out, &val_out)  → bool
 *     Stateless forward iterator. Initialize iter=0, call until false.
 *     Do not mutate the map during iteration.
 *
 * Optional extensions (include separately):
 *   hashmap_fmt.h   — hashmap_to_stringbuf() for debugging
 *   hashmap_range.h — hashmap_collect_keys/values(), HASHMAP_FOR_EACH()
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * MULTIPLE INSTANTIATIONS IN ONE TRANSLATION UNIT
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * hashmap.h is intentionally NOT guarded with an include-once guard on the
 * outer scope — it can be included multiple times with different macro configs
 * to generate distinct instantiations:
 *
 * ```c
 * // First instantiation: u64 → int
 * #define HASHMAP_KEY_TYPE  u64
 * #define HASHMAP_VAL_TYPE  int
 * #define HASHMAP_HASH_FN   hash_u64
 * #define HASHMAP_EQ_FN     eq_u64
 * #define HASHMAP_TYPE_NAME map_u64_int
 * #define HASHMAP_FN(name)  map_u64_int_##name
 * #include "data/hashmap/hashmap.h"
 * #undef HASHMAP_KEY_TYPE
 * #undef HASHMAP_VAL_TYPE
 * // ... undef all, then:
 *
 * // Second instantiation: str_t → u32
 * #define HASHMAP_KEY_TYPE  str_t
 * #define HASHMAP_VAL_TYPE  u32
 * #define HASHMAP_HASH_FN   hash_str
 * #define HASHMAP_EQ_FN     eq_str
 * #define HASHMAP_TYPE_NAME map_str_u32
 * #define HASHMAP_FN(name)  map_str_u32_##name
 * #include "data/hashmap/hashmap.h"
 * ```
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * DESIGN NOTES
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Ownership:
 *   The hashmap does not own any memory. The caller owns the buffer passed to
 *   hashmap_init(). Keys and values are copied into slots on insert — the
 *   originals are not retained and need not outlive the call.
 *
 * No rehashing:
 *   The hashmap never rehashes. Fixed capacity is a feature, not a limitation.
 *   If you need auto-growth, create your own (dynhashmap.h).
 *
 * No global state:
 *   All state lives in the hashmap struct. Multiple hashmaps are fully
 *   independent. Thread safety requires external synchronization per-map.
 *
 * No null keys:
 *   Key pointers passed to insert/get/remove must be non-NULL (checked by
 *   require()). Keys that contain NULL as a logical value (e.g. a nullable
 *   pointer key) are fine — the pointer to the key must be non-NULL.
 *
 * @sa hashmap_mangle.h  — name customization
 * @sa hashmap_decl.h    — forward declarations for separate compilation
 * @sa hashmap_defn.h    — definitions for separate compilation
 * @sa hashmap_impl.h    — pure implementation logic (do not include directly)
 * @sa hashmap_fmt.h     — optional: diagnostic string formatting
 * @sa hashmap_range.h   — optional: bulk key/value collection
 */

/*
 * NOTE: hashmap.h intentionally has NO include guard.
 * It can be included multiple times with different macro configurations
 * to instantiate distinct hashmap types in the same translation unit.
 * See "Multiple Instantiations" section above.
 */

/*
 * Set linkage to static inline before pulling in the implementation.
 * This is the header-only mode — all functions are inlined at call sites.
 */
#define HASHMAP_LINKAGE static inline

#include "hashmap_impl.h"

#undef HASHMAP_LINKAGE
