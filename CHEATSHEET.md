# Canon-C Cheatsheet

> Quick reference manual for all Canon-C modules.

---

## Table of Contents

### `core/primitives/`
- [`bits.h`](#bitsh) — Portable bit manipulation
- [`checked.h`](#checkedh) — Overflow-safe arithmetic
- [`compare.h`](#compareh) — Comparator and predicate types
- [`contract.h`](#contracth) — Contracts and assertions
- [`limits.h`](#limitsh) — Numeric limits and constants
- [`ptr.h`](#ptrh) — Named pointer arithmetic
- [`types.h`](#typesh) — Type aliases

### `core/`
- [`arena.h`](#arenah) — Linear bump allocator
- [`memory.h`](#memoryh) — Safe low-level memory operations
- [`ownership.h`](#ownershiph) — Ownership and borrowing annotations
- [`pool.h`](#poolh) — Fixed-size object pool allocator
- [`region.h`](#regionh) — Explicit lifetime region tokens
- [`scope.h`](#scopeh) — RAII-style deferred cleanup
- [`slice.h`](#sliceh) — Non-owning views into contiguous memory

### `semantics/option/`
- [`option.h`](#optionh) — Rust-style Option\<T\> for C
- [`option_decl.h`](#option_declh) — Declaration macros (separate compilation)
- [`option_defn.h`](#option_defnh) — Definition macros (implementation generation)
- [`option_impl.h`](#option_implh) — Pure implementation logic
- [`option_mangle.h`](#option_mangleh) — Name mangling conventions

### `semantics/result/`
- [`result.h`](#resulth) — Rust-style Result\<T, E\> for C
- [`result_decl.h`](#result_declh) — Declaration macros (separate compilation)
- [`result_defn.h`](#result_defnh) — Definition macros (implementation generation)
- [`result_impl.h`](#result_implh) — Pure implementation logic
- [`result_mangle.h`](#result_mangleh) — Name mangling conventions

### `semantics/`
- [`borrow.h`](#borrowh) — Non-owning view types with explicit borrowing intent
- [`diag.h`](#diagh) — Structured diagnostic frames for error context chains
- [`error.h`](#errorh) — Common error codes and Result\<T, Error\> helpers

### `data/convenience/`
- [`dynstring.h`](#dynstringh) — Auto-growing heap string builder
- [`dynvec.h`](#dynvech) — Auto-growing typed heap vector
- [`smallvec.h`](#smallvech) — Inline-first vector with at-most-one heap/arena spill

### `data/deque/`
- [`deque.h`](#dequeh) — Bounded double-ended queue (ring buffer)
- [`deque_decl.h`](#deque_declh) — Declaration macros (separate compilation)
- [`deque_defn.h`](#deque_defnh) — Definition macros (implementation generation)
- [`deque_impl.h`](#deque_implh) — Pure implementation logic
- [`deque_mangle.h`](#deque_mangleh) — Name mangling conventions

### `data/hashmap/`
- [`hashmap.h`](#hashmaph) — Generic Robin Hood open-addressed hashmap
- [`hashmap_decl.h`](#hashmap_declh) — Declaration macros (separate compilation)
- [`hashmap_defn.h`](#hashmap_defnh) — Definition macros (implementation generation)
- [`hashmap_impl.h`](#hashmap_implh) — Pure implementation logic
- [`hashmap_mangle.h`](#hashmap_mangleh) — Name mangling conventions
- [`hashmap_fmt.h`](#hashmap_fmth) — Optional: diagnostic string formatting
- [`hashmap_range.h`](#hashmap_rangeh) — Optional: bulk key/value collection

### `data/vec/`
- [`vec.h`](#vech) — Bounded typed vector with caller-owned buffer
- [`vec_decl.h`](#vec_declh) — Declaration macros (separate compilation)
- [`vec_defn.h`](#vec_defnh) — Definition macros (implementation generation)
- [`vec_impl.h`](#vec_implh) — Pure implementation logic
- [`vec_mangle.h`](#vec_mangleh) — Name mangling conventions
- [`vec_fmt.h`](#vec_fmth) — Optional: StringBuf formatting
- [`vec_range.h`](#vec_rangeh) — Optional: range-fill extension

### `data/`
- [`array.h`](#arrayh) — Fixed-size typed array with compile-time capacity
- [`bitset.h`](#bitseth) — Fixed-capacity bitset with O(1) bit ops
- [`priority_queue.h`](#priority_queueh) — Fixed-capacity binary heap (min/max)
- [`queue.h`](#queueh) — Bounded FIFO queue (thin wrapper over deque)
- [`range.h`](#rangeh) — Explicit bounded integer range generator
- [`stack.h`](#stackh) — Bounded LIFO stack (thin wrapper over vec)
- [`stringbuf.h`](#stringbufh) — Fixed-capacity incremental string builder

### `algo/`
- [`any_all.h`](#any_allh) — Existential and universal predicate testing
- [`filter.h`](#filterh) — Select elements matching a predicate
- [`find.h`](#findh) — Locate first element matching a predicate
- [`fold.h`](#foldh) — Functional reduction of sequences to single values
- [`map.h`](#maph) — Element-wise transformations over sequences
- [`reverse.h`](#reverseh) — In-place sequence reversal
- [`search.h`](#searchh) — Binary search utilities for sorted sequences
- [`sort.h`](#sorth) — Stable in-place hybrid insertion/merge sort
- [`unique.h`](#uniqueh) — Remove consecutive duplicate elements


---

## `core/primitives/`

> All functions are `static inline` — zero call overhead.

---

### `bits.h`
> Bit test, set, clear, toggle, extract, insert, count, rotate, and byte swap. All operate on `u64`.

#### Single Bit

**`bool bits_test(u64 value, u32 bit)`**
Returns `true` if the bit at position `bit` is set.
```c
bits_test(0b1010, 1)  // → true
```

**`u64 bits_set(u64 value, u32 bit)`**
Returns value with bit at position `bit` set to 1.
```c
bits_set(0b0000, 3)  // → 0b1000
```

**`u64 bits_clear(u64 value, u32 bit)`**
Returns value with bit at position `bit` cleared to 0.
```c
bits_clear(0b1111, 2)  // → 0b1011
```

**`u64 bits_toggle(u64 value, u32 bit)`**
Returns value with bit at position `bit` flipped.
```c
bits_toggle(0b1010, 0)  // → 0b1011
```

#### Multi-Bit

**`u64 bits_extract(u64 value, u32 start, u32 count)`**
Extracts `count` bits starting at `start`, right-justified.
```c
bits_extract(0b11010110, 2, 4)  // → 0b0101
```

**`u64 bits_insert(u64 dst, u64 src, u32 start, u32 count)`**
Inserts the low `count` bits of `src` into `dst` at position `start`. Other bits preserved.
```c
bits_insert(0b11110000, 0b0101, 2, 4)  // → 0b11010100
```

#### Bit Counting

**`u32 bits_popcount(u64 value)`**
Returns the number of set bits (1s).
```c
bits_popcount(0b10110101)  // → 5
```

**`u32 bits_clz(u64 value)`**
Returns the number of leading zeros from the MSB. Returns `64` if value is `0`.
```c
bits_clz(0b00001010)  // → 60
```

**`u32 bits_ctz(u64 value)`**
Returns the number of trailing zeros from the LSB. Returns `64` if value is `0`.
```c
bits_ctz(0b10100000)  // → 5
```

**`u32 bits_ffs(u64 value)`**
Returns the position of the lowest set bit, 1-indexed. Returns `0` if value is `0`.
```c
bits_ffs(0b10100000)  // → 6
```

**`u32 bits_fls(u64 value)`**
Returns the position of the highest set bit, 1-indexed. Returns `0` if value is `0`.
```c
bits_fls(0b10100000)  // → 8
```

#### Rotation

**`u64 bits_rotl(u64 value, u32 shift)`**
Rotates bits left by `shift` positions. Bits that fall off wrap to the right.
```c
bits_rotl(0b10110001, 2)  // → 0b11000110  (8-bit context)
```

**`u64 bits_rotr(u64 value, u32 shift)`**
Rotates bits right by `shift` positions. Bits that fall off wrap to the left.
```c
bits_rotr(0b10110001, 2)  // → 0b01101110  (8-bit context)
```

#### Power of Two

**`bool bits_is_power_of_two(u64 value)`**
Returns `true` if value is a power of 2. Returns `false` for `0`.
```c
bits_is_power_of_two(16)  // → true
bits_is_power_of_two(15)  // → false
```

**`u64 bits_next_power_of_two(u64 value)`**
Returns the smallest power of 2 >= value. Returns `0` on overflow or if value is `0`.
```c
bits_next_power_of_two(17)    // → 32
bits_next_power_of_two(1000)  // → 1024
```

#### Byte Swap

**`u16 bits_bswap16(u16 value)`**
Reverses bytes in a 16-bit value.
```c
bits_bswap16(0x1234)  // → 0x3412
```

**`u32 bits_bswap32(u32 value)`**
Reverses bytes in a 32-bit value.
```c
bits_bswap32(0x12345678)  // → 0x78563412
```

**`u64 bits_bswap64(u64 value)`**
Reverses bytes in a 64-bit value.
```c
bits_bswap64(0x0102030405060708)  // → 0x0807060504030201
```

> **Known Limitations:** All bit positions are 0-indexed (0 = LSB, 63 = MSB). Passing `bit >= 64` is undefined behavior — not checked at runtime.

---

### `checked.h`
> Overflow-safe add, sub, mul for all integer sizes. Returns `true` on success, `false` on overflow. Also includes min/max/clamp macros.

All functions return `true` on success, `false` on overflow/underflow, and write the result to an output pointer.
```c
usize result;
if (!checked_add(a, b, &result)) {
    // overflow — handle error
}
// use result safely
```

#### Addition

**`bool checked_add(usize a, usize b, usize* result)`**
**`bool checked_add_u8(u8 a, u8 b, u8* result)`**
**`bool checked_add_u16(u16 a, u16 b, u16* result)`**
**`bool checked_add_u32(u32 a, u32 b, u32* result)`**
**`bool checked_add_u64(u64 a, u64 b, u64* result)`**
**`bool checked_add_isize(isize a, isize b, isize* result)`**
Returns `false` if `a + b` overflows the type.
```c
checked_add(1024, 512, &out)      // → true,  out = 1536
checked_add(USIZE_MAX, 1, &out)   // → false
```

#### Subtraction

**`bool checked_sub(usize a, usize b, usize* result)`**
**`bool checked_sub_u8(u8 a, u8 b, u8* result)`**
**`bool checked_sub_u16(u16 a, u16 b, u16* result)`**
**`bool checked_sub_u32(u32 a, u32 b, u32* result)`**
**`bool checked_sub_u64(u64 a, u64 b, u64* result)`**
**`bool checked_sub_isize(isize a, isize b, isize* result)`**
Returns `false` if `a - b` underflows (i.e. `b > a` for unsigned).
```c
checked_sub(10, 3, &out)   // → true,  out = 7
checked_sub(3, 10, &out)   // → false
```

#### Multiplication

**`bool checked_mul(usize a, usize b, usize* result)`**
**`bool checked_mul_u8(u8 a, u8 b, u8* result)`**
**`bool checked_mul_u16(u16 a, u16 b, u16* result)`**
**`bool checked_mul_u32(u32 a, u32 b, u32* result)`**
**`bool checked_mul_u64(u64 a, u64 b, u64* result)`**
**`bool checked_mul_isize(isize a, isize b, isize* result)`**
Returns `false` if `a * b` overflows. Safe with `a == 0` or `b == 0` (returns `true`, result = 0).
```c
checked_mul(1000, sizeof(Item), &out)  // → true  (typically)
checked_mul(USIZE_MAX, 2, &out)        // → false
```

> **Known Limitations:** All signed fallback implementations (`checked_add_isize`, `checked_sub_isize`, `checked_mul_isize`) check bounds **before** performing the operation to avoid signed integer overflow UB. The builtin path (`__builtin_*_overflow`) is always preferred when available.

#### Min / Max / Clamp (macros)
```c
checked_min(a, b)         // smaller of a, b
checked_max(a, b)         // larger of a, b
checked_clamp(x, lo, hi)  // x clamped to [lo, hi]
```
```c
checked_min(3, 7)         // → 3
checked_max(3, 7)         // → 7
checked_clamp(15, 0, 10)  // → 10
```

> **Known Limitations:**
> - Macros (`checked_min`, `checked_max`, `checked_clamp`) — arguments may be evaluated more than once; don't put function calls or `i++` inside them.
> - All signed fallback implementations (`checked_add_isize`, `checked_sub_isize`, `checked_mul_isize`) check bounds **before** performing the operation to avoid signed integer overflow UB. The builtin path (`__builtin_*_overflow`) is always preferred when available.
> - Requires `limits.h` for `CANON_ISIZE_MAX` / `CANON_ISIZE_MIN` — used by the signed fallback paths.
> - Alignment helpers (`align_up`, `align_down`, `is_aligned`, `is_ptr_aligned`) have moved to `ptr.h`, which provides guarded versions with `require_msg` precondition checks.

---

### `compare.h`
> `algo_cmp_fn` and `algo_pred_fn` typedefs plus built-in comparators for `int`, `usize`, `double`, `u8`, `i64`.

#### Function Types

```c
typedef int  (*algo_cmp_fn) (const void* a, const void* b, void* ctx);
typedef bool (*algo_pred_fn)(const void* elem, void* ctx);
```

**`algo_cmp_fn`** — comparator for sorting and ordered containers. Returns `< 0` if a < b, `0` if equal, `> 0` if a > b.

**`algo_pred_fn`** — predicate for testing a single element. Returns `true` if the element satisfies the condition.

`ctx` is an optional caller-provided context pointer, may be `NULL`.

#### Built-in Comparators

**`int algo_cmp_int(const void* a, const void* b, void* ctx)`**
**`int algo_cmp_int_desc(const void* a, const void* b, void* ctx)`**
Ascending / descending `int` comparison.

**`int algo_cmp_usize(const void* a, const void* b, void* ctx)`**
**`int algo_cmp_usize_desc(const void* a, const void* b, void* ctx)`**
Ascending / descending `usize` comparison.

**`int algo_cmp_double(const void* a, const void* b, void* ctx)`**
**`int algo_cmp_double_desc(const void* a, const void* b, void* ctx)`**
Ascending / descending `double` comparison. NaN is sorted last in both directions.

**`int algo_cmp_u8(const void* a, const void* b, void* ctx)`**
Ascending `u8` comparison.

**`int algo_cmp_i64(const void* a, const void* b, void* ctx)`**
Ascending `i64` comparison.

#### Custom Comparator Example

```c
int my_cmp(const void* a, const void* b, void* ctx) {
    const MyStruct* x = (const MyStruct*)a;
    const MyStruct* y = (const MyStruct*)b;
    return (x->value > y->value) - (x->value < y->value);
}
```

> **Known Limitations:** Comparators must satisfy total order — reflexive, antisymmetric, and transitive. Violating this causes undefined behavior in sort and search functions.

---

### `contract.h`
> `require`, `ensure`, `unreachable`, `panic`, `static_require`. Replaces `assert()` with semantic intent.

#### Core Macros

**`require(cond)`**
**`require_msg(cond, msg)`**
Always-on precondition check. Triggers panic if `cond` is false. Never disabled, even in release builds.
```c
require(ptr != NULL);
require_msg(index < len, "index out of bounds");
```

**`ensure(cond)`**
**`ensure_msg(cond, msg)`**
Debug-only check. Same as `require` but compiled out when `NDEBUG` is defined. Use for internal invariants.
```c
ensure(pool->used <= pool->capacity);
ensure_msg(is_aligned(ptr, 8), "pointer must be 8-byte aligned");
```

**`unreachable()`**
**`unreachable_msg(msg)`**
Marks a code path that should never execute. In debug: triggers panic. In release: compiler optimization hint.
```c
switch (state) {
    case STATE_A: return handle_a();
    case STATE_B: return handle_b();
    default: unreachable();
}
```

**`panic(msg)`**
Unconditional panic with a message. Use for fatal errors with no recovery.
```c
panic("failed to parse configuration file");
```

**`static_require(cond, msg)`**
Compile-time assertion. Fails at compile time if `cond` is false. `msg` must be an identifier (no spaces).
```c
static_require(sizeof(Header) == 64, header_must_be_64_bytes);
```

#### Custom Panic Handler

**`void contract_set_handler(contract_handler_fn handler)`**
Replaces the default panic handler (which prints to stderr and calls `abort()`).
```c
void my_handler(const char* file, int line, const char* func,
                const char* expr, const char* msg) {
    log_fatal("%s at %s:%d", expr, file, line);
    exit(1);
}

contract_set_handler(my_handler);
```

> **Note:** The custom handler must never return.

#### Quick Reference

| Macro | Always on | Has `_msg` variant | Release behavior |
|---|---|---|---|
| `require` | ✅ | ✅ | Panics |
| `ensure` | ❌ | ✅ | No-op |
| `unreachable` | ❌ | ✅ | Compiler hint |
| `panic` | ✅ | — | Panics |
| `static_require` | ✅ | — | Compile error |

> **Known Limitations:** Contracts are for catching bugs, not handling expected errors. For recoverable failures, use the `Result` type instead.

---

### `limits.h`
> Common constants and limits for Canon-C. Provides numeric limits, alignment constants, platform-derived values, and practical capacity constraints. All values are compile-time.

#### Design Goals

- **Explicitness:** Named constants instead of magic numbers  
- **Readability:** `CANON_USIZE_MAX` > `((usize)-1)`  
- **Portability:** Values derived from standard C macros  
- **Performance:** All constants fold at compile time  
- **Safety:** Capacity caps + overflow-friendly design  

---

## Integer Type Limits

| Constant | Value |
|----------|-------|
| `CANON_U8_MAX` | 255 |
| `CANON_U16_MAX` | 65,535 |
| `CANON_U32_MAX` | 4,294,967,295 |
| `CANON_U64_MAX` | 18,446,744,073,709,551,615 |
| `CANON_I8_MIN` / `CANON_I8_MAX` | -128 / 127 |
| `CANON_I16_MIN` / `CANON_I16_MAX` | -32,768 / 32,767 |
| `CANON_I32_MIN` / `CANON_I32_MAX` | -2,147,483,648 / 2,147,483,647 |
| `CANON_I64_MIN` / `CANON_I64_MAX` | -9,223,372,036,854,775,808 / 9,223,372,036,854,775,807 |

---

## Size Type Limits (Platform-Dependent)

| Constant | Value |
|----------|-------|
| `CANON_USIZE_MAX` | `SIZE_MAX` |
| `CANON_ISIZE_MAX` | `PTRDIFF_MAX` |
| `CANON_ISIZE_MIN` | `PTRDIFF_MIN` |

---

## Alignment Constants

| Constant | Value | Notes |
|----------|-------|--------|
| `CANON_DEFAULT_ALIGN` | `_Alignof(max_align_t)` (C11) / 16 fallback | General-purpose allocation |
| `CANON_CACHE_LINE` | 64 bytes | Prevent false sharing |
| `CANON_SIMD_ALIGN` | 16 bytes | SSE / NEON |
| `CANON_SIMD_ALIGN_AVX` | 32 bytes | AVX |
| `CANON_SIMD_ALIGN_AVX512` | 64 bytes | AVX-512 |
| `CANON_ATOMIC_ALIGN` | 16 bytes | Lock-free / atomic data |
| `CANON_PAGE_SIZE` | 4096 bytes (overrideable) | 16 KiB on Android 15+ / ARM64 servers |

Helper:
```c
CANON_ALIGN_MAX(a, b)
```

---

## Common Size Constants

```c
CANON_KB  // 1,024
CANON_MB  // 1,048,576
CANON_GB  // 1,073,741,824
```

Example:
```c
Arena arena = arena_create(buf, 2 * CANON_MB);
```

---

## Practical Capacity Limits

| Constant | Default | Overridable |
|----------|----------|--------------|
| `CANON_VEC_MAX_CAPACITY` | 1 GB | ✅ |
| `CANON_STRING_MAX_SIZE` | 16 MB | ✅ |
| `CANON_ARENA_MAX_SIZE` | 1 GB | ✅ |

Override before including:
```c
#define CANON_PAGE_SIZE 16384
#define CANON_VEC_MAX_CAPACITY (1ULL << 40)
#define CANON_CACHE_LINE 128
#include "limits.h"
```

---

## Small Buffer Optimization Thresholds

```c
CANON_SSO_THRESHOLD  // 23 bytes — inline string storage
CANON_SVO_THRESHOLD  // 8 elements — inline vector storage
```

---

## Growth Factor

```c
CANON_GROWTH_FACTOR_NUM    // 3
CANON_GROWTH_FACTOR_DENOM  // 2
// → 1.5x growth
```

```c
CANON_MIN_ALLOCATION  // 32 bytes
```

Example:
```c
usize new_cap = old_cap * CANON_GROWTH_FACTOR_NUM / CANON_GROWTH_FACTOR_DENOM;
```

---

## Pointer Tagging

```c
CANON_PTR_TAG_BITS   // 3 (64-bit) / 2 (32-bit)
CANON_PTR_TAG_MASK
CANON_PTR_ADDR_MASK
```

```c
uintptr_t tagged   = ((uintptr_t)ptr & CANON_PTR_ADDR_MASK) | tag;
void*     untagged = (void*)(tagged & CANON_PTR_ADDR_MASK);
usize     tag      = tagged & CANON_PTR_TAG_MASK;
```

---

## Platform Info

```c
CANON_POINTER_SIZE   // sizeof(void*)
CANON_BITS_PER_BYTE  // CHAR_BIT (8)
CANON_POINTER_BITS   // 32 or 64
```

---

## Notes & Guarantees

- All constants are **compile-time**.
- All limits derive from **standard C macros**.
- Alignment uses **max_align_t when available (C11)**.
- Page size is **overrideable** for 16 KiB systems (Android 15+, ARM64 servers).
- Pointer tagging assumes **natural alignment**.

---

### `ptr.h`
> Explicit, named pointer arithmetic and alignment utilities. Centralizes all pointer manipulation for safety, clarity, and performance. All functions are `static inline` and compile away to minimal instructions.

#### Design Goals

- **Explicitness:** All pointer arithmetic is named — no raw `+` on `void*`
- **Safety:** NULL checks, overflow detection, underflow protection
- **Portability:** Flat-address-space safe, no platform intrinsics
- **Performance:** All functions inline → 1–3 instructions after optimization
- **Correctness:** Alignment is always explicit, never assumed

---

## Integer Alignment Helpers

### `usize align_up(usize n, usize align)`
Rounds `n` up to the next multiple of `align`.

```c
align_up(13, 8)  // → 16
align_up(16, 8)  // → 16
align_up(0, 16)  // → 0
```

**Preconditions:**
- `align > 0`
- `align` must be a power of two

---

### `usize align_down(usize n, usize align)`
Rounds `n` down to the nearest multiple of `align`.

```c
align_down(13, 8)  // → 8
align_down(16, 8)  // → 16
```

---

### `usize align_padding(usize n, usize align)`
Returns number of bytes needed to align `n`.

```c
align_padding(13, 8)  // → 3
align_padding(16, 8)  // → 0
```

---

### `bool is_aligned(usize n, usize align)`
Returns true if `n` is aligned to `align`.

```c
is_aligned(16, 8)  // → true
is_aligned(13, 8)  // → false
```

---

### `bool is_power_of_two(usize n)`
Returns true if `n` is a power of two.

```c
is_power_of_two(16)  // → true
is_power_of_two(15)  // → false
```

---

## Pointer Alignment

### `void* ptr_align_up(void* p, usize align)`
Rounds pointer upward to nearest alignment.

```c
void* aligned = ptr_align_up(raw, 16);
```

Returns `NULL` if `p == NULL`.

---

### `void* ptr_align_down(void* p, usize align)`
Rounds pointer downward to nearest alignment.

---

### `bool ptr_is_aligned(const void* p, usize align)`
Returns true if pointer is aligned. Returns false for `NULL`.

---

### `usize ptr_align_padding(const void* p, usize align)`
Returns padding bytes needed to align pointer. Returns 0 for `NULL`.

---

## Compile-Time Alignment Helpers (NEW)

### `ALIGN_OF(type)`
Returns required alignment of a type.

```c
usize a = ALIGN_OF(double);
```

Uses `_Alignof` when available, portable C99 fallback otherwise.

---

### `ALIGN_MAX(a, b)`
Returns maximum of two alignments.

```c
usize worst = ALIGN_MAX(ALIGN_OF(int), ALIGN_OF(double));
```

---

### Struct Attribute Helpers (NEW)

```c
PACKED
CACHE_ALIGNED
ALIGNAS(n)
```

Example:
```c
typedef struct PACKED {
    u32 id;
    u8  flags;
} PackedHeader;

typedef struct CACHE_ALIGNED {
    u64 counters[8];
} ThreadLocalData;
```

Enabled only on GCC / Clang unless CANON_NO_GNU_EXTENSIONS is defined.

---

## Pointer Arithmetic

### `void* ptr_offset(void* p, usize n)`
Moves pointer forward by `n` bytes.

Overflow-checked.

```c
void* next = ptr_offset(buf, 64);
```

Returns `NULL` if `p == NULL`.

---

### `void* ptr_retreat(void* p, usize n)`
Moves pointer backward by `n` bytes.

Underflow-checked.

---

### `const void* ptr_offset_const(const void* p, usize n)`
Const-correct variant of `ptr_offset`.

---

### `isize ptr_diff(const void* to, const void* from)`
Signed byte difference: `to - from`.

```c
isize used = ptr_diff(arena->current, arena->start);
```

---

### `usize ptr_span(const void* to, const void* from)`
Unsigned distance. Fails contract if `to < from`.

---

## Bounds Checking

### `bool ptr_in_range(const void* p, const void* start, const void* end)`
Returns true if:

```
start <= p < end
```

Returns false if any pointer is `NULL`.

---

### `bool ptr_range_in_range(const void* p, usize len, const void* start, const void* end)`
Returns true if entire `[p, p+len)` lies inside `[start, end)`.

Overflow-safe and region-length bounded.

---

## Typed Element Access

### `void* ptr_elem(void* base, usize index, usize elem_size)`
Untyped array indexing helper. 

Note: index * elem_size is not overflow-checked.
If index or element size is untrusted, use checked_mul() before calling.

```c
void* slot = ptr_elem(buffer, 3, sizeof(MyStruct));
```

---

### `const void* ptr_elem_const(...)`
Const version.

---

## Null Safety

### `void* ptr_or(void* p, void* fallback)`
Returns `p` if non-NULL, otherwise `fallback`.

---

### `bool ptr_is_valid(const void* p)`
Explicit NULL test.

---

## Convenience Macros

### `PTR_OFFSET_OF(type, member)`
Portable offset-of macro.

```c
usize off = PTR_OFFSET_OF(MyStruct, field);
```

---

### `PTR_CONTAINER_OF(ptr, type, member)`
Recover enclosing struct from member pointer.

```c
MyItem* item = PTR_CONTAINER_OF(node_ptr, MyItem, node);
```

---

## Guarantees

- All functions are **static inline**
- All arithmetic is **overflow / underflow guarded**
- NULL inputs are **always safe**
- No UB from pointer arithmetic
- No platform-specific assumptions

---

### `types.h`
> Short type aliases for `stdint.h` types. Foundation header — include this everywhere.

#### Type Aliases

| Alias | Underlying type | Range |
|---|---|---|
| `u8` | `uint8_t` | 0 to 255 |
| `u16` | `uint16_t` | 0 to 65,535 |
| `u32` | `uint32_t` | 0 to 4,294,967,295 |
| `u64` | `uint64_t` | 0 to 18,446,744,073,709,551,615 |
| `i8` | `int8_t` | -128 to 127 |
| `i16` | `int16_t` | -32,768 to 32,767 |
| `i32` | `int32_t` | -2,147,483,648 to 2,147,483,647 |
| `i64` | `int64_t` | -9,223,372,036,854,775,807 to 9,223,372,036,854,775,807 |
| `usize` | `size_t` | Platform-dependent (32 or 64-bit) |
| `isize` | `ptrdiff_t` | Platform-dependent (32 or 64-bit) |
| `byte` | `u8` | Raw/untyped binary data |
| `f32` | `float` | 32-bit IEEE 754 |
| `f64` | `double` | 64-bit IEEE 754 |

`bool`, `true`, `false` come from standard `<stdbool.h>` — no alias needed.

#### When to use what

- `u8*` / `byte*` — raw buffers and binary data (`byte` signals intent more clearly)
- `usize` — array indices, lengths, sizes, capacities
- `isize` — pointer differences, signed offsets
- `u32` / `u64` — hashes, timestamps, fixed-width values
- `f32` / `f64` — floating point math

> **Known Limitations:** Not suitable for bit-fields (use `unsigned int`), atomic operations (use `_Atomic` directly), or printf format specifiers (use `PRIu32`, `PRId64` from `<inttypes.h>`).

---

## `core/`

---

### `arena.h`
> Linear bump allocator. Allocates sequentially from a fixed caller-owned buffer. No individual deallocation — reset or rollback only.

#### Initialization

**`void arena_init(Arena* arena, void* buffer, usize capacity)`**
Initializes arena using caller-provided memory. Buffer must remain valid for the arena's lifetime.
```c
u8 buf[4096];
Arena arena;
arena_init(&arena, buf, sizeof(buf));
```

#### Allocation

**`void* arena_alloc(Arena* arena, usize size)`**
Allocates `size` bytes with natural alignment. Returns `NULL` on failure.
```c
MyStruct* s = arena_alloc(&arena, sizeof(MyStruct));
```

**`void* arena_alloc_aligned(Arena* arena, usize size, usize alignment)`**
Same as `arena_alloc` but with explicit power-of-2 alignment.

**`void* arena_alloc_zero(Arena* arena, usize size)`**
Allocates and zero-initializes. O(size).

**`void* arena_alloc_aligned_zero(Arena* arena, usize size, usize alignment)`**
Allocates with explicit alignment and zero-initializes. O(size).

#### Try Variants

**`bool arena_try_alloc(Arena* arena, usize size, void** out)`**
**`bool arena_try_alloc_aligned(Arena* arena, usize size, usize alignment, void** out)`**
Same as above but returns `false` instead of `NULL` on failure. Writes pointer to `out`.

#### Typed Macros

```c
arena_alloc_type(arena, Type)              // allocates one Type
arena_alloc_array(arena, Type, count)      // allocates count Types
arena_alloc_type_zero(arena, Type)         // allocates and zero-initializes one Type
arena_alloc_array_zero(arena, Type, count) // allocates and zero-initializes count Types
```
```c
MyStruct* s   = arena_alloc_type(&arena, MyStruct);
MyStruct* arr = arena_alloc_array(&arena, MyStruct, 16);
```

#### Reset

**`void arena_reset(Arena* arena)`**
Resets offset to 0. Fast — O(1). Does NOT clear memory.

**`void arena_reset_secure(Arena* arena)`**
Resets and wipes previously allocated memory. Use for sensitive data. O(used bytes).

#### Checkpoint / Rollback

**`ArenaMark arena_mark(const Arena* arena)`**
Saves current allocation position.

**`void arena_reset_to(Arena* arena, ArenaMark mark)`**
Rolls back to a previous mark. Invalidates all allocations made after the mark.
```c
ArenaMark mark = arena_mark(&arena);
// ... temporary allocations ...
arena_reset_to(&arena, mark);  // rolled back
```

#### Query

**`usize arena_capacity(const Arena* arena)`** — total bytes in buffer.
**`usize arena_remaining(const Arena* arena)`** — bytes still available.
**`usize arena_used(const Arena* arena)`** — bytes currently allocated.
**`bool arena_is_empty(const Arena* arena)`** — true if offset == 0.
**`bool arena_is_full(const Arena* arena)`** — true if no space remains.

#### Byte Views (slice.h integration)

**`bytes_t arena_as_bytes(const Arena* arena)`** — view over allocated region `[buffer, buffer+offset)`.
**`bytes_t arena_buffer_bytes(const Arena* arena)`** — view over entire buffer including free space.
**`bytes_t arena_free_bytes(const Arena* arena)`** — view over unallocated region only.

#### Debug Mode (`CANON_ARENA_DEBUG`)

Define `CANON_ARENA_DEBUG` before including to enable tracking fields:
```c
#define CANON_ARENA_DEBUG
#include "core/arena.h"

ArenaStats s = arena_stats(&arena);
// s.used, s.remaining, s.capacity, s.peak, s.alloc_count
```

> **Known Limitations:**
> - Not thread-safe — caller must synchronize if sharing across threads.
> - `arena_alloc_array` and `arena_alloc_array_zero` do not overflow-check `sizeof(Type) * count` — use `checked_mul` first for untrusted counts.
> - `arena_reset_to` does not roll back debug counters (`alloc_count`, `peak`) — they reflect cumulative activity since last full reset.
> - Memory is not cleared on `arena_reset` — old data remains readable until overwritten.

---

### `memory.h`
> Safe, explicit wrappers over `memcpy`, `memmove`, `memset`, `memcmp`, `malloc`, and `free`. Null-safe and zero-size safe throughout. Prefer `arena_alloc()` for temporary allocations; use `mem_alloc()` only when heap + explicit free is required.

#### Heap Allocation

**`void* mem_alloc(usize size)`**
Allocates `size` bytes on the heap. Returns `NULL` if `size == 0` or allocation fails. Free with `mem_free()`.

**`void mem_free(void* ptr)`**
Frees heap memory. NULL-safe. Do NOT use on arena/pool memory.

#### Alignment Utilities

**`usize mem_align(usize size)`**
Rounds `size` up to the next multiple of `CANON_DEFAULT_ALIGN`. Returns `CANON_USIZE_MAX` on overflow.

**`usize mem_align_to(usize size, usize alignment)`**
Rounds `size` up to the next multiple of `alignment` (must be power of 2). Returns `CANON_USIZE_MAX` on overflow or invalid alignment.

**`bool mem_is_aligned(const void* ptr, usize alignment)`**
Returns `true` if `ptr` satisfies the given power-of-2 alignment. `NULL` → false.

**`usize mem_get_alignment(const void* ptr)`**
Returns the largest power-of-2 alignment of a pointer's address. `NULL` → 0.

**`bool mem_is_power_of_two(usize n)`**
Returns `true` if `n` is a power of two and > 0.

#### Raw Memory Operations

**`void mem_copy(void* dest, const void* src, usize size)`**
Non-overlapping copy (wraps `memcpy`). Null-safe, zero-size safe.
```c
mem_copy(dst, src, sizeof(MyStruct));
```

**`void mem_move(void* dest, const void* src, usize size)`**
Overlapping-safe copy (wraps `memmove`).

**`void mem_zero(void* ptr, usize size)`**
Zero-fills a region. Null-safe.

**`void mem_secure_zero(void* ptr, usize size)`**
Zero-fills without compiler optimization (uses `memset_s` or volatile loop). Use for secrets.

**`void mem_set(void* ptr, int value, usize size)`**
Fills region with repeated byte `value`.

**`int mem_compare(const void* a, const void* b, usize size)`**
Byte comparison (`memcmp` semantics). Returns 0 for NULL or size == 0. **Not constant-time** — do not use for crypto.

**`bool mem_equal(const void* a, const void* b, usize size)`**
Returns `true` if regions are byte-identical. Both NULL → true, size == 0 → true.

**`bool mem_is_all(const void* ptr, int value, usize size)`**
Returns `true` if every byte equals `value`. NULL or size == 0 → true.

**`bool mem_is_zero(const void* ptr, usize size)`**
Returns `true` if every byte is zero.

**`void mem_swap(void* a, void* b, usize size)`**
Swaps two non-overlapping regions using a stack buffer.
```c
// size must be <= CANON_MEM_SWAP_MAX (default: 256 bytes)
mem_swap(&items[i], &items[j], sizeof(Item));
```

#### `bytes_t` / `cbytes_t` Variants

| Function | Description |
|---|---|
| `usize mem_copy_bytes(bytes_t dest, cbytes_t src)` | Copy; requires `dest.len >= src.len`. Returns bytes copied. |
| `usize mem_move_bytes(bytes_t dest, cbytes_t src)` | Move (overlap-safe); requires `dest.len >= src.len`. |
| `void mem_zero_bytes(bytes_t b)` | Zero-fill entire view. |
| `void mem_set_bytes(bytes_t b, int value)` | Fill view with byte value. |
| `bool mem_equal_bytes(cbytes_t a, cbytes_t b)` | True if same length and byte-identical. |
| `bool mem_is_zero_bytes(cbytes_t b)` | True if all bytes are zero. |
| `void mem_secure_zero_bytes(bytes_t b)` | Secure zero, optimizer-resistant. |

#### Type-Safe Macros
```c
mem_zero_type(ptr)               // zero one object
mem_secure_zero_type(ptr)        // secure zero one object
mem_zero_array(array)            // zero a fixed-size array
mem_copy_type(dest, src)         // copy one object
mem_compare_type(a, b)           // compare two objects
mem_equal_type(a, b)             // equality of two objects
mem_alloc_type(Type)             // heap-alloc one Type
mem_alloc_array(Type, count)     // heap-alloc array of Type
```

> **Known Limitations:**
> - `mem_swap` requires `size <= CANON_MEM_SWAP_MAX` (default 256 bytes) — override by defining `CANON_MEM_SWAP_MAX` before including.
> - `mem_compare` / `mem_equal` are not constant-time — never use for comparing cryptographic secrets.
> - `mem_alloc_array` macro does not overflow-check `sizeof(Type) * count` — use `checked_mul` first for untrusted counts.
> - `mem_secure_zero` falls back to a volatile loop when `memset_s` is unavailable (non-C11 Annex K platforms).

---

### `ownership.h`
> Explicit ownership and borrowing annotations for Canon-C APIs. Pure preprocessor — zero runtime cost, zero enforcement. Annotations are promises, not guarantees.

#### Annotation Macros

These all expand to `T` at the compiler level — their value is documentation and grep-ability.

**`owned(T)`**
Pointer transfers ownership. On a parameter: callee owns it, caller must not use or free it after. On a return type: caller receives ownership and must eventually free it.

**`borrowed(T)`**
Non-owning view. On a parameter: caller retains ownership. On a return type: caller must NOT free it and must not use it after the owner is freed.

**`moved(T)`**
Signals the original value should be treated as invalid after the call. C cannot enforce this.

**`dropped(T)`**
Signals the value has been released and the pointer is now invalid.
```c
owned(Arena)        arena_create(owned(u8*) buffer, usize size);
owned(void*)        arena_alloc(borrowed(Arena*) arena, usize size);
void                arena_destroy(owned(Arena*) arena);
borrowed(const char*) stringbuf_str(borrowed(const StringBuf*) sb);
```

#### Drop Helpers

**`CANON_DROP(ptr, free_fn)`**
Calls `free_fn(ptr)` then sets `ptr = NULL`. Prevents accidental reuse but does not prevent UB if dereferenced.
```c
CANON_DROP(my_arena, arena_destroy);
```

**`CANON_DROP_IF(ptr, free_fn)`**
Same as `CANON_DROP` but skips the call if `ptr` is already `NULL`. Safe for lazily-initialized resources.
```c
CANON_DROP_IF(optional_buf, mem_free);
```

#### Strong Wrappers (optional)

Use `DEFINE_OWNED` / `DEFINE_BORROWED` when you want a distinct struct type that the compiler won't silently coerce to a raw pointer.

**`DEFINE_OWNED(type)`**
Generates `owned_##type` struct + helpers:

| Helper | Description |
|---|---|
| `owned_T_wrap(ptr)` | Wraps a raw pointer into the owned struct |
| `owned_T_unwrap(o)` | Extracts raw pointer and NULLs the wrapper |
| `owned_T_borrow(o)` | Returns raw pointer without transferring ownership |
| `owned_T_is_valid(o)` | Returns `true` if non-NULL and holds a non-NULL ptr |
| `owned_T_drop(o, free_fn)` | Calls `free_fn`, NULLs internal ptr |

**`DEFINE_BORROWED(type)`**
Generates `borrowed_##type` struct + helpers:

| Helper | Description |
|---|---|
| `borrowed_T_from(ptr)` | Wraps a `const T*` into the borrowed struct |
| `borrowed_T_get(b)` | Returns the `const T*` |
| `borrowed_T_is_valid(b)` | Returns `true` if non-NULL and holds a non-NULL ptr |

#### Convention Summary
```c
// Caller owns, function borrows (most common)
void vec_push(borrowed(Vec*) v, int item);

// Function returns owned — caller must free
owned(char*) str_duplicate(borrowed(const char*) src);

// Function takes owned — callee responsible after call
void dynvec_free(dropped(dynvec*) v);

// Function returns borrowed — caller must NOT free
borrowed(const char*) stringbuf_str(borrowed(const StringBuf*) sb);
```

> **Known Limitations:** Annotations are documentation only — no compiler enforcement, no protection against use-after-move or double-free. `DEFINE_OWNED` / `DEFINE_BORROWED` give type-level distinction but still require manual discipline. The custom handler in `contract.h` applies if a `require_msg` inside a generated helper fires.

---

### `pool.h`
> Fixed-size object pool allocator backed by an `Arena`. O(1) allocation for many objects of the same size. No individual deallocation — reset only.

#### Initialization

**`bool pool_init(Pool* pool, Arena* arena, usize object_size, usize max_objects)`**
Initializes the pool using space from `arena`. Object sizes are aligned up automatically. Returns `false` if the arena has insufficient space or arguments are invalid.
```c
Pool pool;
pool_init(&pool, &arena, sizeof(MyStruct), 128);
```

**`pool_init_type(pool, arena, Type, max_objects)`** *(macro)*
Type-safe shorthand — passes `sizeof(Type)` automatically.
```c
pool_init_type(&pool, &arena, MyStruct, 128);
```

#### Allocation

| Function | Description |
|---|---|
| `void* pool_alloc(Pool* pool)` | Allocates one object slot. Returns `NULL` if full. |
| `void* pool_alloc_zero(Pool* pool)` | Same, but zero-initializes. |
| `bool pool_try_alloc(Pool* pool, void** out)` | Writes pointer to `out`, returns `false` if full. |
| `bool pool_try_alloc_zero(Pool* pool, void** out)` | Same, but zero-initializes. |

#### Index Access

**`void* pool_get(const Pool* pool, usize i)`**
Returns a pointer to the object at index `i`. Bounds-checked against `pool->used`. Returns `NULL` if out of bounds.

**`const void* pool_get_const(const Pool* pool, usize i)`**
Same, returning a `const` pointer.

#### Query
```c
pool_used(pool)             // number of allocated objects
pool_capacity(pool)         // maximum objects
pool_remaining(pool)        // slots still available
pool_is_full(pool)          // true if used == capacity
pool_is_empty(pool)         // true if used == 0
pool_object_size(pool)      // aligned size per object in bytes
pool_memory_used(pool)      // object_size * used
pool_memory_reserved(pool)  // object_size * capacity
```

#### Reset

**`void pool_reset(Pool* pool)`**
Resets `used` to 0 and rolls back the arena to the pool's base mark. O(1). Does NOT zero memory. All prior `pool_get()` / `pool_alloc()` pointers are invalidated.

**`void pool_reset_secure(Pool* pool)`**
Same, but securely zeros all previously allocated object memory first. O(used bytes).

#### Byte Views

**`bytes_t pool_as_bytes(const Pool* pool)`**
View over `[base, base + used * object_size)`. Non-owning — invalid after reset.

**`bytes_t pool_reserved_bytes(const Pool* pool)`**
View over the entire reserved region including unallocated slots.

#### Type-Safe Macros
```c
pool_alloc_type(pool, Type)              // (Type*) pool_alloc
pool_alloc_type_zero(pool, Type)         // (Type*) pool_alloc_zero
pool_get_type(pool, i, Type)             // (Type*) pool_get
pool_get_type_const(pool, i, Type)       // (const Type*) pool_get_const
```

> **Known Limitations:**
> - No individual deallocation — `pool_reset()` resets the entire pool at once.
> - `pool_get()` requires objects to be contiguous from `base_mark` — do not interleave other arena allocations between pool objects after `pool_init()`.
> - `pool_init()` does not overflow-check `aligned_size * max_objects` beyond a basic guard — use `checked_mul` if counts are untrusted.
> - Not thread-safe — inherits arena's single-threaded constraint.

---

### `region.h`
> Named lifetime boundaries for borrow validity. A Region token represents a scope — borrowed views can be stamped with its ID and validated against it in debug builds. Stack-allocated only, zero heap use.

#### Lifecycle

**`Region region_begin(void)`**
Opens a new region with a unique monotonic ID. Returns a fully initialized stack-allocated `Region`.
```c
Region r = region_begin();
```

**`void region_end(Region* r)`**
Closes the region. Calls all registered cleanup hooks in LIFO order, then resets the attached arena (if any). After this call `r->open == false`.
```c
region_end(&r);
```

#### Configuration

**`void region_attach_arena(Region* r, Arena* arena)`**
Attaches an arena — `arena_reset()` will be called automatically on `region_end()`. Only one arena per region; attaching a second replaces the first without resetting it.
```c
Region r = region_begin();
region_attach_arena(&r, &scratch);
void* buf = arena_alloc(&scratch, 256);  // lives until region_end
region_end(&r);                          // arena_reset called automatically
```

**`void region_set_parent(Region* r, Region* parent)`**
Records a parent region for nesting intent. Informational only — the parent is never auto-ended.

**`bool region_register(Region* r, void (*fn)(void* ctx), void* ctx)`**
Registers a cleanup hook. Hooks are called LIFO on `region_end()`. Returns `false` if the hook table is full (`REGION_MAX_CLEANUP`, default 8).
```c
void release_lock(void* ctx) { mutex_unlock((Mutex*)ctx); }
region_register(&r, release_lock, &my_mutex);
```

#### Inspection
```c
region_id(&r)           // unique region_id_t (0 if r == NULL)
region_is_open(&r)      // true between begin and end
region_has_parent(&r)   // true if parent was set
region_hook_count(&r)   // number of registered hooks
```

#### Lifetime Assertions (debug only, no-op in release)

**`void region_assert_open(const Region* r)`**
Fires `ensure_msg` if the region is closed. Call before using a borrowed value tied to this region.

**`void region_assert_borrow_valid(const Region* r, region_id_t borrow_region_id)`**
Asserts the borrow's stamped ID matches this region and the region is still open. Silently passes for `REGION_ID_STATIC` (always valid).

#### Special IDs

**`REGION_ID_STATIC`** — reserved ID `0`. Borrows stamped with this are considered permanently valid (string literals, static buffers).

#### `REGION_SCOPE` Macro

If `scope.h` is included, `REGION_SCOPE(name)` declares a region that is automatically ended at scope exit via `SCOPE_DEFER`. Without `scope.h`, it expands to `region_begin()` only — `region_end()` must be called manually.
```c
#include "core/scope.h"
#include "core/region.h"

void my_function(void) {
    REGION_SCOPE(r);
    region_attach_arena(&r, &scratch);
    // ... region_end(&r) called automatically at scope exit
}
```

> **Known Limitations:**
> - `region_next_id()` uses a static counter — not thread-safe. Use one region per thread in multi-threaded code.
> - `REGION_MAX_CLEANUP` defaults to 8 hooks; override by defining it before including the header.
> - Parent regions are never auto-ended — nesting order is always the caller's responsibility.
> - Lifetime assertions are `ensure_msg` — compiled out in release (`NDEBUG`). No runtime protection in production builds.
> - Attaching a second arena silently replaces the first without resetting it.

---

### `scope.h`
> RAII-style deferred cleanup via zero-overhead macros. Cleanup code runs automatically when leaving a block — on normal end, `return`, `break`, `continue`, or `goto` within the block. Multiple defers in the same block execute in LIFO order.

#### Macros

**`SCOPE_DEFER { ... }`**
Executes the block exactly once when control leaves the enclosing scope. Expands to nested `for`-loops — compiled away entirely at `-O2` or higher.

**`defer { ... }`**
Short alias for `SCOPE_DEFER` (Go/Zig-style). Disable with `#define CANON_NO_DEFER_ALIAS`.

#### Usage
```c
// Single resource
FILE* f = fopen("data.txt", "r");
if (!f) return ERR_OPEN;
defer { fclose(f); }
// f is guaranteed closed on any normal exit

// Multiple resources — released in reverse order
void* mem = malloc(4096);
defer { free(mem); }         // runs second

FILE* log = fopen("log.txt", "a");
defer { fclose(log); }       // runs first

// Arena checkpoint
ArenaMark mark = arena_mark(&arena);
defer { arena_reset_to(&arena, mark); }

// Lock guard
pthread_mutex_lock(&mtx);
defer { pthread_mutex_unlock(&mtx); }
```

#### Exit Method Behavior

| Exit method | Cleanup runs? |
|---|---|
| Normal end of block | ✅ |
| `return` | ✅ |
| `break` / `continue` | ✅ |
| `goto` inside block | ✅ |
| `goto` outside block | ❌ |
| `longjmp` out | ❌ |
| `exit()` / `abort()` | ❌ |

#### Critical Rules

- Never put `return`, `break`, `continue`, or outward `goto` **inside** a defer block
- Never use `longjmp` across a defer boundary — cleanup will be skipped
- Keep defer blocks simple: `free`, `fclose`, `unlock`, `arena_reset_to`, etc.

> **Known Limitations:** `goto` jumping **out** of the enclosing block silently skips all defers in that block — this is the most common footgun. `longjmp` bypasses all defers completely. Not a substitute for exception handling in large codebases with complex control flow.

---

### `slice.h`
> Non-owning `{pointer, length}` views into contiguous memory. No allocations, no ownership. All operations are bounds-checked and NULL-safe. All functions are `static inline` — zero call overhead.

#### Types

**`bytes_t`** — mutable untyped byte view `{u8* ptr, usize len}`
**`cbytes_t`** — read-only untyped byte view `{const u8* ptr, usize len}`
**`str_t`** — read-only character view `{const char* ptr, usize len}` — null terminator NOT required

#### `bytes_t` Construction
```c
bytes_from(ptr, len)      // mutable view over [ptr, ptr+len)
bytes_empty()             // {NULL, 0}
cbytes_from(ptr, len)     // read-only view
bytes_as_const(b)         // bytes_t → cbytes_t
```

#### `bytes_t` Queries & Slicing
```c
bytes_is_empty(b)              // true if len == 0
bytes_at(b, i)                 // u8* at index i, NULL if OOB
bytes_equal(a, b)              // true if same length and contents — O(n)
bytes_slice(b, start, end)     // sub-view [start, end), no copy
bytes_take(b, n)               // first n bytes
bytes_skip(b, n)               // bytes after skipping n
```

#### `str_t` Construction
```c
str_from(ptr, len)       // view from pointer + explicit length
str_from_cstr(cstr)      // view from null-terminated string — O(n) strlen
str_empty()              // {NULL, 0}
```

#### `str_t` Queries & Slicing
```c
str_is_empty(s)                 // true if len == 0
str_equal(a, b)                 // true if same length and contents — O(n)
str_starts_with(s, prefix)      // O(prefix.len)
str_ends_with(s, suffix)        // O(suffix.len)
str_slice(s, start, end)        // sub-view [start, end), no copy
str_take(s, n)                  // first n characters
str_skip(s, n)                  // characters after skipping n
str_as_bytes(s)                 // str_t → cbytes_t
```

#### `DEFINE_SLICE(type)` — Typed Element Views

Generates a `slice_##type` struct and helpers for any C type. For pointer types, `typedef` first.
```c
DEFINE_SLICE(int)

int arr[8] = {1,2,3,4,5,6,7,8};
slice_int sv = slice_int_from(arr, 8);
```

Generated functions:

| Function | Returns | Description |
|---|---|---|
| `slice_T_from(ptr, len)` | `slice_T` | Construct from pointer + length |
| `slice_T_empty()` | `slice_T` | `{NULL, 0}` |
| `slice_T_len(s)` | `usize` | Element count |
| `slice_T_is_empty(s)` | `bool` | True if len == 0 |
| `slice_T_get(s, i, out)` | `bool` | Bounds-checked copy into `*out` |
| `slice_T_get_unchecked(s, i)` | `type` | No bounds check (debug assert only) |
| `slice_T_at(s, i)` | `type*` | Pointer to element, NULL if OOB |
| `slice_T_first(s)` | `type*` | Pointer to first, NULL if empty |
| `slice_T_last(s)` | `type*` | Pointer to last, NULL if empty |
| `slice_T_slice(s, start, end)` | `slice_T` | Sub-view, no copy |
| `slice_T_take(s, n)` | `slice_T` | First n elements |
| `slice_T_skip(s, n)` | `slice_T` | After skipping n |
| `slice_T_as_bytes(s)` | `bytes_t` | Raw byte view over backing memory |

> **Known Limitations:**
> - Slices do NOT own memory — they become invalid after `arena_reset()`, `mem_free()`, or when the backing stack buffer goes out of scope.
> - `str_from_cstr()` calls `strlen()` — O(n). Use `str_from()` when length is already known.
> - `slice_T_get_unchecked()` only asserts in debug builds (`ensure_msg`) — no protection in release.
> - `DEFINE_SLICE` cannot be used directly with pointer types — `typedef` the pointer type first (e.g. `typedef int* intp;`).
> - `bytes_equal` / `str_equal` / `slice_T_get` are not constant-time — do not use for cryptographic comparison.

---

## `semantics/option/`

---

### `option.h`
> Rust-style `Option<T>` for C. Represents either `Some(value)` or `None`. Zero-cost: `bool + T` on the stack, no heap allocation. One concrete struct and full function set generated per type via `CANON_OPTION(type)`.

#### Setup
```c
CANON_OPTION(int)     // generates option_int + all functions
CANON_OPTION(float)

// Pointer types — typedef first:
typedef void* void_ptr;
CANON_OPTION(void_ptr)

// Struct types:
typedef struct { int x, y; } Point;
CANON_OPTION(Point)
```

Must be used at file/global scope, not inside functions. Use once per type.

#### Constructors
```c
option_int_some(42)   // Some(42)
option_int_none()     // None
```

#### Queries
```c
option_int_is_some(o)   // true if contains a value
option_int_is_none(o)   // true if empty
```

#### Safe Extraction
```c
int out;
if (option_int_get(o, &out)) { /* use out */ }   // pointer output

int val = option_int_unwrap_or(o, 0);            // default if None
```

#### Unsafe Extraction
```c
int val = option_int_unwrap(o);              // panics if None
int val = option_int_expect(o, "msg");       // panics with message if None
```

Only use after an `is_some()` check, or when value presence is a guaranteed invariant.

#### Combinators
```c
opt = option_int_map(opt, double_it);          // T → T, None passthrough
opt = option_int_filter(opt, is_even);         // None if predicate fails
opt = option_int_and_then(opt, half_if_even);  // T → option_T, no nesting
opt = option_int_or_else(opt, get_default);    // alternative if None
opt = option_int_zip(opt_a, opt_b, combine);   // Some(f(a,b)) if both Some
```

#### Mutation
```c
option_int old   = option_int_replace(&opt, 20); // swap in new, get old
option_int taken = option_int_take(&opt);         // extract, leave None
```

#### Comparison
```c
bool eq = option_int_eq(opt_a, opt_b, int_eq_fn);
// true if both None, or both Some with equal values
```

#### Propagation Macros (GNU C / C23 only)

Disable with `#define CANON_NO_GNU_EXTENSIONS`.
```c
// Return err_code from current function if opt is None, else yield value
int val = TRY_SOME(int, get_optional(), err(ERROR_NOT_FOUND));

// Yield value or default as an inline expression
int x = UNWRAP_OR_DEFAULT(int, get_config(), 100);
```

> **Known Limitations:**
> - `CANON_OPTION(T)` generates ~1-2KB of code per type — only instantiate types you use.
> - `unwrap()` and `expect()` use `require()` from `contract.h` — always-on panic, not debug-only.
> - `TRY_SOME` / `UNWRAP_OR_DEFAULT` require GNU C statement expressions or C23 — not valid in strict C99.
> - `map`, `and_then`, `filter`, `zip` take function pointers — cannot inline lambdas in C99.
> - Do not use `Option<bool>` — prefer an explicit enum or `Result` instead.

---

### `option_decl.h`
> Declaration macros for `Option<T>` — for separate compilation. Use in `.h` files to declare types and function signatures without generating implementations.

#### When to use

Use `option_decl.h` + `option_defn.h` together when you want faster incremental builds by defining Option types once in a `.c` file instead of re-generating them in every translation unit.

#### Workflow
```c
// my_types.h — declare only
#include "option_decl.h"
DECLARE_OPTION_ALL(extern, int)

// my_types.c — define once
#include "option_defn.h"
DEFINE_OPTION_ALL(, int)   // empty linkage for extern

// main.c — just include the header
#include "my_types.h"
option_int x = option_int_some(42);
```

#### Macros

**`DECLARE_OPTION_ALL(linkage, T)`**
One-shot: declares the typedef, struct, and all function signatures.

**`DECLARE_OPTION_TYPEDEF(T)`** — typedef only
**`DECLARE_OPTION_STRUCT(T)`** — struct layout only
**`DECLARE_OPTION_FUNCTIONS(linkage, T)`** — all function signatures, no type

Individual function declaration macros (all take `linkage, T`):
```c
DECLARE_OPTION_SOME(linkage, T)
DECLARE_OPTION_NONE(linkage, T)
DECLARE_OPTION_IS_SOME(linkage, T)
DECLARE_OPTION_IS_NONE(linkage, T)
DECLARE_OPTION_GET(linkage, T)
DECLARE_OPTION_UNWRAP_OR(linkage, T)
DECLARE_OPTION_UNWRAP(linkage, T)
DECLARE_OPTION_EXPECT(linkage, T)
DECLARE_OPTION_MAP(linkage, T)
DECLARE_OPTION_AND_THEN(linkage, T)
DECLARE_OPTION_OR_ELSE(linkage, T)
DECLARE_OPTION_FILTER(linkage, T)
DECLARE_OPTION_ZIP(linkage, T)
DECLARE_OPTION_REPLACE(linkage, T)
DECLARE_OPTION_TAKE(linkage, T)
DECLARE_OPTION_EQ(linkage, T)
```

#### Minimal API pattern

Expose only what you want callers to see:
```c
DECLARE_OPTION_TYPEDEF(int)
DECLARE_OPTION_STRUCT(int)
DECLARE_OPTION_SOME(extern, int)
DECLARE_OPTION_NONE(extern, int)
DECLARE_OPTION_IS_SOME(extern, int)
// unwrap, map, etc. stay internal
```

> **Known Limitations:** Declarations are compile-time only — no runtime cost, no code generation. Linkage attributes like `__declspec` are compiler-specific; pass them via the `linkage` parameter.

---

### `option_defn.h`
> Definition macros for `Option<T>` — generates actual implementations. Use in `.c` files for separate compilation, or in `.h` files with `static inline` for header-only libraries.

#### Workflow
```c
// Header-only (traditional Canon-C style)
#include "option_defn.h"
DEFINE_OPTION_ALL(static inline, int)

// Separate compilation (.c file only)
#include "option_defn.h"
DEFINE_OPTION_ALL(, int)   // empty linkage = extern
```

#### Macros

**`DEFINE_OPTION_ALL(linkage, T)`**
One-shot: defines the struct and all function implementations.

**`DEFINE_OPTION_STRUCT(T)`** — struct layout only
**`DEFINE_OPTION_FUNCTIONS(linkage, T)`** — all functions, no struct

Individual function definition macros (all take `linkage, T`):
```c
DEFINE_OPTION_SOME(linkage, T)
DEFINE_OPTION_NONE(linkage, T)
DEFINE_OPTION_IS_SOME(linkage, T)
DEFINE_OPTION_IS_NONE(linkage, T)
DEFINE_OPTION_GET(linkage, T)
DEFINE_OPTION_UNWRAP_OR(linkage, T)
DEFINE_OPTION_UNWRAP(linkage, T)
DEFINE_OPTION_EXPECT(linkage, T)
DEFINE_OPTION_MAP(linkage, T)
DEFINE_OPTION_AND_THEN(linkage, T)
DEFINE_OPTION_OR_ELSE(linkage, T)
DEFINE_OPTION_FILTER(linkage, T)
DEFINE_OPTION_ZIP(linkage, T)
DEFINE_OPTION_REPLACE(linkage, T)
DEFINE_OPTION_TAKE(linkage, T)
DEFINE_OPTION_EQ(linkage, T)
```

#### Custom Implementation Override

Override `IMPL_*` macros before including to specialize behavior for a type:
```c
#include "option_impl.h"
#undef IMPL_OPTION_SOME
#define IMPL_OPTION_SOME(t, topt, param) \
    { \
        require((param) != NULL, "option_some: NULL pointer not allowed"); \
        return (topt){ .has_value = true, .value = (param) }; \
    }

#include "option_defn.h"
typedef void* void_ptr;
DEFINE_OPTION_ALL(static inline, void_ptr)
// option_void_ptr_some(NULL) now panics at runtime
```

> **Known Limitations:** `DEFINE_OPTION_ALL` must be used at file/global scope. With `extern` linkage, define exactly once across all translation units — multiple definitions will cause linker errors. With `static inline`, each translation unit gets its own copy (safe but increases binary size).

---

### `option_impl.h`
> Pure implementation logic for `Option<T>` — no name mangling, no naming conventions. These are the raw behavior macros used internally by `option_defn.h`. Override individual macros to specialize behavior for specific types.

#### Struct Layout
```c
IMPL_OPTION_STRUCT(T)
// expands to: { bool has_value; T value; }
// sizeof(bool) + sizeof(T) + alignment padding
```

#### Available Implementation Macros

| Macro | Behavior |
|---|---|
| `IMPL_OPTION_SOME(t, topt, param)` | Returns `{true, param}` |
| `IMPL_OPTION_NONE(t, topt)` | Returns `{false}` |
| `IMPL_OPTION_IS_SOME(t, o)` | Returns `o.has_value` |
| `IMPL_OPTION_IS_NONE(t, o)` | Returns `!o.has_value` |
| `IMPL_OPTION_GET(t, o, out)` | Writes to `*out` if Some and out != NULL |
| `IMPL_OPTION_UNWRAP_OR(t, o, fallback)` | Returns value or fallback |
| `IMPL_OPTION_UNWRAP(t, o)` | `require(has_value)` then returns value |
| `IMPL_OPTION_EXPECT(t, o, msg)` | `require(has_value, msg)` then returns value |
| `IMPL_OPTION_MAP(t, topt, o, f, some_fn, none_fn)` | `some_fn(f(value))` or `none_fn()` |
| `IMPL_OPTION_AND_THEN(t, topt, o, f, none_fn)` | `f(value)` or `none_fn()` |
| `IMPL_OPTION_OR_ELSE(t, topt, o, fallback)` | `o` if Some, else `fallback()` |
| `IMPL_OPTION_FILTER(t, topt, o, pred, none_fn)` | `o` if `pred(value)`, else `none_fn()` |
| `IMPL_OPTION_ZIP(t, topt, o1, o2, combine, some_fn, none_fn)` | `some_fn(combine(a,b))` if both Some |
| `IMPL_OPTION_REPLACE(t, topt, o, new_value, some_fn)` | Swaps in new value, returns old |
| `IMPL_OPTION_TAKE(t, topt, o, none_fn)` | Extracts value, leaves None |
| `IMPL_OPTION_EQ(t, o1, o2, eq)` | Both None → true; both Some → `eq(a,b)` |

#### Specialization

Override any `IMPL_*` macro before including `option_defn.h` to customize behavior for a specific type:
```c
#include "option_impl.h"
#undef IMPL_OPTION_UNWRAP
#define IMPL_OPTION_UNWRAP(t, o) \
    { \
        require((o).has_value, "my_custom_panic_message"); \
        return (o).value; \
    }
```

> **Known Limitations:** These macros are internal — most users should never include `option_impl.h` directly. Overrides apply globally to all types defined after the `#undef`, not just one specific type. `IMPL_OPTION_GET` is intentionally permissive (NULL `out` allowed); `IMPL_OPTION_REPLACE` and `IMPL_OPTION_TAKE` are strict (NULL pointer triggers `require`).

---

### `option_mangle.h`
> Name mangling conventions for `Option<T>`. Single source of truth for all generated type and function names. Override any macro before including to rename the entire API globally.

#### Default Naming Scheme

For a type `T`, the defaults produce:

| What | Default name | Example for `int` |
|---|---|---|
| Type | `option_T` | `option_int` |
| Struct tag | `option_T_s` | `option_int_s` |
| `some` | `option_T_some` | `option_int_some` |
| `none` | `option_T_none` | `option_int_none` |
| `is_some` | `option_T_is_some` | `option_int_is_some` |
| `is_none` | `option_T_is_none` | `option_int_is_none` |
| `get` | `option_T_get` | `option_int_get` |
| `unwrap_or` | `option_T_unwrap_or` | `option_int_unwrap_or` |
| `unwrap` | `option_T_unwrap` | `option_int_unwrap` |
| `expect` | `option_T_expect` | `option_int_expect` |
| `map` | `option_T_map` | `option_int_map` |
| `and_then` | `option_T_and_then` | `option_int_and_then` |
| `or_else` | `option_T_or_else` | `option_int_or_else` |
| `filter` | `option_T_filter` | `option_int_filter` |
| `zip` | `option_T_zip` | `option_int_zip` |
| `replace` | `option_T_replace` | `option_int_replace` |
| `take` | `option_T_take` | `option_int_take` |
| `eq` | `option_T_eq` | `option_int_eq` |

#### Customization

Define overrides **before** including any option header:
```c
// Haskell-style Maybe
#define MANGLE_OPTION_TYPE(t)    Maybe##t
#define MANGLE_OPTION_SOME(t)    Maybe##t##_Just
#define MANGLE_OPTION_NONE(t)    Maybe##t##_Nothing
#define MANGLE_OPTION_IS_SOME(t) Maybe##t##_isJust
#define MANGLE_OPTION_IS_NONE(t) Maybe##t##_isNothing
#include "option.h"
CANON_OPTION(int)

MaybeInt x = MaybeInt_Just(42);

// Project namespace prefix
#define MANGLE_OPTION_TYPE(t)    myproject_opt_##t
#include "option.h"
CANON_OPTION(int)

myproject_opt_int x = myproject_opt_int_some(42);
```

> **Known Limitations:** All macros are guarded with `#ifndef` — overrides must be defined before the first include of any option header in that translation unit. Overrides apply to all types instantiated after them, not selectively per type.

---

## `semantics/result/`

---

### `result.h`
> Rust-style `Result<T, E>` for C. Represents either `Ok(value)` or `Err(error)`. Zero-cost: `bool + union{T, E}` on the stack — union saves memory over storing both separately. One concrete struct and full function set generated per `(T, E)` pair via `CANON_RESULT(value_type, error_type)`.

#### Setup
```c
typedef enum { ERR_NONE, ERR_IO, ERR_PARSE } error;

CANON_RESULT(int, error)     // generates result_int_error + all functions
CANON_RESULT(float, error)

// Pointer types — typedef first:
typedef const char* constcharptr;
CANON_RESULT(int, constcharptr)

// Combining with Option:
CANON_OPTION(int)
CANON_RESULT(option_int, error)  // Ok(Some/None) vs Err
```

Must be used at file/global scope. Use once per `(T, E)` pair.

#### Constructors
```c
result_int_error_ok(42)          // Ok(42)
result_int_error_err(ERR_IO)     // Err(ERR_IO)
```

#### Queries
```c
result_int_error_is_ok(r)    // true if Ok
result_int_error_is_err(r)   // true if Err
```

#### Safe Extraction
```c
int val;
if (result_int_error_get_ok(r, &val)) { /* use val */ }

error err;
if (result_int_error_get_err(r, &err)) { /* use err */ }

int val = result_int_error_unwrap_or(r, 0);   // default if Err
```

#### Unsafe Extraction
```c
int val  = result_int_error_unwrap(r);         // panics if Err
error e  = result_int_error_unwrap_err(r);     // panics if Ok
int val  = result_int_error_expect(r, "msg");  // panics with message if Err
```

#### Combinators
```c
r = result_int_error_map(r, double_it);        // T → T if Ok, Err passthrough
r = result_int_error_map_err(r, remap_err);    // E → E if Err, Ok passthrough
r = result_int_error_and_then(r, next_step);   // T → result_T_E, no nesting
r = result_int_error_or_else(r, recover);      // E → result_T_E if Err
```

#### Comparison
```c
bool eq = result_int_error_eq(r1, r2, int_eq, error_eq);
// true if both Ok with equal values, or both Err with equal errors
```

#### Propagation Macros (GNU C / C23 only)

Disable with `#define CANON_NO_GNU_EXTENSIONS`.
```c
// Early return with the Err if result is Err, continue if Ok
TRY(int, error, step1());

// Early return with a remapped error value
TRY_REMAP(int, error, step1(), ERR_CONTEXT);

// Extract value or early return with Err — use in expressions
int x = TRY_UNWRAP(int, error, step1());

// Extract value or use fallback — use in expressions
int x = UNWRAP_OR(int, error, step1(), 0);
```

#### Full propagation example
```c
result_int_error add_parsed(const char* a, const char* b) {
    int x = TRY_UNWRAP(int, error, parse_int(a));
    int y = TRY_UNWRAP(int, error, parse_int(b));
    return result_int_error_ok(x + y);
}
```

> **Known Limitations:**
> - `CANON_RESULT(T, E)` generates ~2-3KB of code per type pair — only instantiate pairs you use.
> - Accessing the wrong union member (`ok` when `is_err`, or vice versa) is undefined behavior — always check `is_ok` first.
> - `TRY`, `TRY_REMAP`, `TRY_UNWRAP`, `UNWRAP_OR` require GNU C statement expressions or C23 — not valid in strict C99.
> - `map` and `map_err` use the same `T` and `E` types — they cannot change the type, only transform the value within it.
> - For pointer types, `typedef` first — `CANON_RESULT(const char*, error)` will not work directly.

---

### `result_decl.h`
> Declaration macros for `Result<T, E>` — for separate compilation. Use in `.h` files to declare types and function signatures without generating implementations. Same pattern as `option_decl.h` but takes two type parameters `(T, E)`.

#### Workflow
```c
// my_results.h — declare only
#include "result_decl.h"
typedef enum { ERR_NONE, ERR_IO } error;
DECLARE_RESULT_ALL(extern, int, error)

// my_results.c — define once
#include "result_defn.h"
DEFINE_RESULT_ALL(, int, error)   // empty linkage for extern

// main.c — just include the header
#include "my_results.h"
result_int_error x = result_int_error_ok(42);
```

#### Macros

**`DECLARE_RESULT_ALL(linkage, T, E)`**
One-shot: declares the typedef, struct, and all function signatures.

**`DECLARE_RESULT_TYPEDEF(T, E)`** — typedef only
**`DECLARE_RESULT_STRUCT(T, E)`** — struct layout only
**`DECLARE_RESULT_FUNCTIONS(linkage, T, E)`** — all function signatures, no type

Individual function declaration macros (all take `linkage, T, E`):
```c
DECLARE_RESULT_OK(linkage, T, E)
DECLARE_RESULT_ERR(linkage, T, E)
DECLARE_RESULT_IS_OK(linkage, T, E)
DECLARE_RESULT_IS_ERR(linkage, T, E)
DECLARE_RESULT_GET_OK(linkage, T, E)
DECLARE_RESULT_GET_ERR(linkage, T, E)
DECLARE_RESULT_UNWRAP_OR(linkage, T, E)
DECLARE_RESULT_UNWRAP(linkage, T, E)
DECLARE_RESULT_UNWRAP_ERR(linkage, T, E)
DECLARE_RESULT_EXPECT(linkage, T, E)
DECLARE_RESULT_MAP(linkage, T, E)
DECLARE_RESULT_MAP_ERR(linkage, T, E)
DECLARE_RESULT_AND_THEN(linkage, T, E)
DECLARE_RESULT_OR_ELSE(linkage, T, E)
DECLARE_RESULT_EQ(linkage, T, E)
```

#### Minimal API pattern
```c
DECLARE_RESULT_TYPEDEF(int, error)
DECLARE_RESULT_STRUCT(int, error)
DECLARE_RESULT_OK(extern, int, error)
DECLARE_RESULT_ERR(extern, int, error)
DECLARE_RESULT_IS_OK(extern, int, error)
// unwrap, map, etc. stay internal
```

> **Known Limitations:** Same as `option_decl.h` — declarations are compile-time only, no code generated. With two type parameters, type names are concatenated as `result_T_E` — ensure `typedef` names are simple identifiers with no spaces or special characters.

---

### `result_defn.h`
> Definition macros for `Result<T, E>` — generates actual implementations. Use in `.c` files for separate compilation, or in `.h` files with `static inline` for header-only libraries. Same pattern as `option_defn.h` but takes two type parameters `(T, E)`.

#### Workflow
```c
// Header-only (traditional Canon-C style)
#include "result_defn.h"
typedef enum { ERR_NONE, ERR_IO } error;
DEFINE_RESULT_ALL(static inline, int, error)

// Separate compilation (.c file only)
#include "result_defn.h"
DEFINE_RESULT_ALL(, int, error)   // empty linkage = extern
```

#### Macros

**`DEFINE_RESULT_ALL(linkage, T, E)`**
One-shot: defines the struct and all function implementations.

**`DEFINE_RESULT_STRUCT(T, E)`** — struct layout only
**`DEFINE_RESULT_FUNCTIONS(linkage, T, E)`** — all functions, no struct

Individual function definition macros (all take `linkage, T, E`):
```c
DEFINE_RESULT_OK(linkage, T, E)
DEFINE_RESULT_ERR(linkage, T, E)
DEFINE_RESULT_IS_OK(linkage, T, E)
DEFINE_RESULT_IS_ERR(linkage, T, E)
DEFINE_RESULT_GET_OK(linkage, T, E)
DEFINE_RESULT_GET_ERR(linkage, T, E)
DEFINE_RESULT_UNWRAP_OR(linkage, T, E)
DEFINE_RESULT_UNWRAP(linkage, T, E)
DEFINE_RESULT_UNWRAP_ERR(linkage, T, E)
DEFINE_RESULT_EXPECT(linkage, T, E)
DEFINE_RESULT_MAP(linkage, T, E)
DEFINE_RESULT_MAP_ERR(linkage, T, E)
DEFINE_RESULT_AND_THEN(linkage, T, E)
DEFINE_RESULT_OR_ELSE(linkage, T, E)
DEFINE_RESULT_EQ(linkage, T, E)
```

#### Custom Implementation Override
```c
#include "result_impl.h"
#undef IMPL_RESULT_OK
#define IMPL_RESULT_OK(t, e, tres, param) \
    { \
        require((param) != NULL, "result_ok: NULL pointer not allowed"); \
        return (tres){ .is_ok = true, .ok = (param) }; \
    }

#include "result_defn.h"
typedef void* void_ptr;
DEFINE_RESULT_ALL(static inline, void_ptr, error)
// result_void_ptr_error_ok(NULL) now panics at runtime
```

> **Known Limitations:** `DEFINE_RESULT_ALL` must be used at file/global scope. With `extern` linkage, define exactly once across all translation units. With `static inline`, each translation unit gets its own copy. Struct uses an anonymous union — requires C99 or later; some strict C89 compilers may reject it.

---

### `result_impl.h`
> Pure implementation logic for `Result<T, E>` — no name mangling, no naming conventions. Raw behavior macros used internally by `result_defn.h`. Override individual macros to specialize behavior for specific type pairs.

#### Struct Layout
```c
IMPL_RESULT_STRUCT(T, E)
// expands to: { bool is_ok; union { T ok; E err; }; }
// sizeof(bool) + max(sizeof(T), sizeof(E)) + alignment padding
// Only one of ok/err is valid at any time — accessing the wrong member is UB
```

#### Available Implementation Macros

| Macro | Behavior |
|---|---|
| `IMPL_RESULT_OK(t, e, tres, param)` | Returns `{true, .ok=param}` |
| `IMPL_RESULT_ERR(t, e, tres, param)` | Returns `{false, .err=param}` |
| `IMPL_RESULT_IS_OK(t, e, r)` | Returns `r.is_ok` |
| `IMPL_RESULT_IS_ERR(t, e, r)` | Returns `!r.is_ok` |
| `IMPL_RESULT_GET_OK(t, e, r, out)` | Writes to `*out` if Ok and out != NULL |
| `IMPL_RESULT_GET_ERR(t, e, r, out)` | Writes to `*out` if Err and out != NULL |
| `IMPL_RESULT_UNWRAP_OR(t, e, r, fallback)` | Returns `r.ok` or fallback |
| `IMPL_RESULT_UNWRAP(t, e, r)` | `require(is_ok)` then returns `r.ok` |
| `IMPL_RESULT_UNWRAP_ERR(t, e, r)` | `require(!is_ok)` then returns `r.err` |
| `IMPL_RESULT_EXPECT(t, e, r, msg)` | `require(is_ok, msg)` then returns `r.ok` |
| `IMPL_RESULT_MAP(t, e, tres, r, f, ok_fn)` | `ok_fn(f(r.ok))` if Ok, else `r` unchanged |
| `IMPL_RESULT_MAP_ERR(t, e, tres, r, f, err_fn)` | `err_fn(f(r.err))` if Err, else `r` unchanged |
| `IMPL_RESULT_AND_THEN(t, e, tres, r, f)` | `f(r.ok)` if Ok, else `r` unchanged |
| `IMPL_RESULT_OR_ELSE(t, e, tres, r, f)` | `r` if Ok, else `f(r.err)` |
| `IMPL_RESULT_EQ(t, e, r1, r2, eq_ok, eq_err)` | Both Ok → `eq_ok(a,b)`; both Err → `eq_err(a,b)`; mixed → false |

#### Specialization
```c
#include "result_impl.h"
#undef IMPL_RESULT_UNWRAP
#define IMPL_RESULT_UNWRAP(t, e, r) \
    { \
        require((r).is_ok, "my_custom_unwrap_message"); \
        return (r).ok; \
    }
```

> **Known Limitations:** These macros are internal — most users should never include `result_impl.h` directly. Overrides apply globally to all type pairs defined after the `#undef`. The anonymous union in `IMPL_RESULT_STRUCT` requires C99 or later. `IMPL_RESULT_GET_OK` and `IMPL_RESULT_GET_ERR` are permissive (NULL `out` allowed); `IMPL_RESULT_UNWRAP` and `IMPL_RESULT_UNWRAP_ERR` are strict (`require` always fires on wrong variant).

---

### `result_mangle.h`
> Name mangling conventions for `Result<T, E>`. Single source of truth for all generated type and function names. Takes two type parameters unlike `option_mangle.h`. Override any macro before including to rename the entire API globally.

#### Default Naming Scheme

For types `T` and `E`, the defaults produce:

| What | Default name | Example for `int, error` |
|---|---|---|
| Type | `result_T_E` | `result_int_error` |
| Struct tag | `result_T_E_s` | `result_int_error_s` |
| `ok` | `result_T_E_ok` | `result_int_error_ok` |
| `err` | `result_T_E_err` | `result_int_error_err` |
| `is_ok` | `result_T_E_is_ok` | `result_int_error_is_ok` |
| `is_err` | `result_T_E_is_err` | `result_int_error_is_err` |
| `get_ok` | `result_T_E_get_ok` | `result_int_error_get_ok` |
| `get_err` | `result_T_E_get_err` | `result_int_error_get_err` |
| `unwrap_or` | `result_T_E_unwrap_or` | `result_int_error_unwrap_or` |
| `unwrap` | `result_T_E_unwrap` | `result_int_error_unwrap` |
| `unwrap_err` | `result_T_E_unwrap_err` | `result_int_error_unwrap_err` |
| `expect` | `result_T_E_expect` | `result_int_error_expect` |
| `map` | `result_T_E_map` | `result_int_error_map` |
| `map_err` | `result_T_E_map_err` | `result_int_error_map_err` |
| `and_then` | `result_T_E_and_then` | `result_int_error_and_then` |
| `or_else` | `result_T_E_or_else` | `result_int_error_or_else` |
| `eq` | `result_T_E_eq` | `result_int_error_eq` |

#### Customization

Define overrides **before** including any result header:
```c
// Haskell-style Either/Right/Left
#define MANGLE_RESULT_TYPE(t, e)   Either_##t##_##e
#define MANGLE_RESULT_OK(t, e)     Either_##t##_##e##_Right
#define MANGLE_RESULT_ERR(t, e)    Either_##t##_##e##_Left
#define MANGLE_RESULT_IS_OK(t, e)  Either_##t##_##e##_isRight
#define MANGLE_RESULT_IS_ERR(t, e) Either_##t##_##e##_isLeft
#include "result.h"
CANON_RESULT(int, error)

Either_int_error x = Either_int_error_Right(42);

// Project namespace prefix
#define MANGLE_RESULT_TYPE(t, e)   myproject_res_##t##_##e
#include "result.h"
CANON_RESULT(int, error)

myproject_res_int_error x = myproject_res_int_error_ok(42);
```

> **Known Limitations:** All macros are guarded with `#ifndef` — overrides must be defined before the first include of any result header in that translation unit. With two type parameters, generated names grow longer — keep `T` and `E` type names short to avoid unwieldy identifiers. Overrides apply to all type pairs instantiated after them, not selectively per pair.

---

## `semantics/`

---

### `borrow.h`
> Concrete non-owning view types with explicit borrowing intent. Complements `ownership.h` (annotation macros) with actual structs. All types carry a `source` debug tag — the address of the owning object — never dereferenced, only for human/debugger inspection.

#### Core idea

`slice_t`, `str_t`, `bytes_t` are plain `{ptr, len}` — they carry no ownership signal. `borrowed_*` types make non-ownership explicit at the type level: when you see `borrowed_str` in a signature you immediately know — do not free, do not outlive the source.

#### `borrowed_ptr` — generic non-owning pointer
```c
borrowed_ptr borrowed_ptr_from(const void* ptr, const void* source)
borrowed_ptr borrowed_ptr_null(void)                  // absent borrow
const void*  borrowed_ptr_get(const borrowed_ptr* b)  // never free result
bool         borrowed_ptr_is_valid(const borrowed_ptr* b)
bool         borrowed_ptr_eq(borrowed_ptr a, borrowed_ptr b)
```

Use when the type is unknown or erased. Prefer typed borrowed wrappers when the type is known.

#### `borrowed_str` — non-owning string view

Wraps `str_t`. Characters are not null-terminated. Never free `str.ptr`.
```c
borrowed_str borrowed_str_from(str_t s, const void* source)
borrowed_str borrowed_str_from_cstr(const char* cstr, const void* source)  // O(n) strlen
borrowed_str borrowed_str_empty(void)
str_t        borrowed_str_get(const borrowed_str* b)   // never free ptr
bool         borrowed_str_is_valid(const borrowed_str* b)
usize        borrowed_str_len(const borrowed_str* b)
bool         borrowed_str_eq(borrowed_str a, borrowed_str b)  // O(n) content compare
borrowed_str borrowed_str_slice(borrowed_str b, usize start, usize end)  // inherits source
```

#### `borrowed_bytes` — non-owning byte view

Wraps `cbytes_t` (read-only bytes). Never free `bytes.ptr`.
```c
borrowed_bytes borrowed_bytes_from(const void* ptr, usize len, const void* source)
borrowed_bytes borrowed_bytes_from_cbytes(cbytes_t b, const void* source)
borrowed_bytes borrowed_bytes_empty(void)
cbytes_t       borrowed_bytes_get(const borrowed_bytes* b)  // never free ptr
usize          borrowed_bytes_len(const borrowed_bytes* b)
bool           borrowed_bytes_is_valid(const borrowed_bytes* b)
borrowed_bytes borrowed_bytes_slice(borrowed_bytes b, usize start, usize end)  // clamps to len
```

#### `DEFINE_BORROWED_SLICE(type)` — typed borrowed slice

Requires prior `DEFINE_SLICE(type)`. Generates `borrowed_slice_T` with:
```c
DEFINE_SLICE(int)
DEFINE_BORROWED_SLICE(int)

borrowed_slice_int borrowed_slice_int_from(slice_int s, const void* source)
borrowed_slice_int borrowed_slice_int_empty(void)
slice_int          borrowed_slice_int_get(const borrowed_slice_int* b)
usize              borrowed_slice_int_len(const borrowed_slice_int* b)
bool               borrowed_slice_int_is_valid(const borrowed_slice_int* b)
const int*         borrowed_slice_int_at(const borrowed_slice_int* b, usize i)  // NULL if OOB
borrowed_slice_int borrowed_slice_int_slice(borrowed_slice_int b, usize start, usize end)
borrowed_bytes     borrowed_slice_int_as_bytes(const borrowed_slice_int* b)
```

#### Lifetime model
```c
borrowed_str s = borrowed_str_from_cstr("Alice", NULL);
// valid as long as the string literal lives (forever here)

borrowed_bytes region = borrowed_bytes_from(buf, 256, buf);
// invalid after buf goes out of scope

borrowed_slice_int view = borrowed_slice_int_from(slice_int_from(arr, 4), arr);
// invalid after arr is freed or goes out of scope
```

Pass `&owning_struct` as `source` for debugger visibility. Pass `NULL` if no owner is meaningful.

> **Known Limitations:**
> - No runtime lifetime enforcement — discipline only.
> - `source` is never dereferenced — it is a debug tag only, not a safe back-reference.
> - `borrowed_str_from_cstr` is O(n) due to `strlen`.
> - `borrowed_bytes_slice` clamps `end` to `len` silently rather than asserting.
> - `borrowed_ptr_get` requires non-NULL `b` (asserts via `require_msg`) but does not assert `ptr != NULL`.
> - `DEFINE_BORROWED_SLICE(type)` must be called at file scope after `DEFINE_SLICE(type)`.

---

### `diag.h`
> Structured diagnostic frames — stack-allocated, allocation-free context chain for error propagation. Answers "why and where" to complement `error.h` (what failed) and `result.h` (propagation). Chain reads bottom-up: `frame[0]` = root cause, last frame = surface error.

#### Configuration
```c
#define DIAG_MAX_FRAMES  8    // max frames before oldest is dropped
#define DIAG_MAX_MSG_LEN 128  // inline message length per frame (truncated if exceeded)
```

Override before including. `sizeof(Diag)` ≈ 640 bytes — stack-safe.

#### Types
```c
// Single context record
typedef struct {
    const char* file;                      // __FILE__ literal
    usize       line;                      // __LINE__
    const char* func;                      // __func__ literal
    Error       code;                      // error code at this level
    char        message[DIAG_MAX_MSG_LEN]; // inline, null-terminated
} DiagFrame;

// The chain itself — pass by pointer, NULL is always safe
typedef struct {
    DiagFrame frames[DIAG_MAX_FRAMES];
    usize     depth;  // 0 = empty
} Diag;
```

#### Construction & Reset
```c
Diag diag_init(void)       // zero-initialized, depth == 0
void diag_clear(Diag* d)   // sets depth = 0, NULL-safe, does not zero frames
```

#### Pushing Frames
```c
// Manual push (rarely needed directly)
void diag_push(Diag* d, const char* file, usize line,
               const char* func, Error code, const char* msg);

// Primary macro — captures __FILE__, __LINE__, __func__ automatically
DIAG_PUSH(diag, code, msg)

// Formatted message into caller-provided stack buffer
DIAG_PUSH_FMT(diag, code, buf, buf_size, fmt, ...)
```

If the chain is full, the oldest frame is dropped (shifted out) and the new frame is appended — newest context is always preserved.

#### Inspection
```c
usize            diag_depth(const Diag* d)           // NULL-safe
bool             diag_has_error(const Diag* d)        // depth > 0
const DiagFrame* diag_frame_at(const Diag* d, usize i) // NULL if OOB
const DiagFrame* diag_root(const Diag* d)             // frame[0], root cause
const DiagFrame* diag_latest(const Diag* d)           // frame[depth-1], surface error
Error            diag_root_code(const Diag* d)         // ERR_OK if empty
Error            diag_latest_code(const Diag* d)       // ERR_OK if empty
```

#### Output
```c
// Prints full chain root → latest to FILE*
// Format: [0] file.c:42 in func() — error N: "message"
void diag_print(const Diag* d, FILE* stream)
```

#### Propagation Macros
```c
// Push frame and return retval if condition is true
DIAG_RETURN_IF(cond, diag, code, msg, retval)

// Call expression; if it returns false → push frame and return retval
DIAG_PROPAGATE(call, diag, code, msg, retval)
```

#### Typical usage
```c
Diag diag = diag_init();

if (!parse_int(str, &val, &diag)) {
    DIAG_PUSH(&diag, ERR_PARSE_FAILED, "invalid number format");
    diag_print(&diag, stderr);
    return false;
}

// With DIAG_PROPAGATE:
DIAG_PROPAGATE(open_file(path, &diag), &diag, ERR_IO_FAILED, "could not open config", false);
```

> **Known Limitations:**
> - No runtime enforcement — caller must pass `&diag` explicitly at every level.
> - On overflow, oldest frame is silently dropped — deep chains lose root context if they exceed `DIAG_MAX_FRAMES`.
> - `DIAG_PUSH_FMT` requires a caller-provided stack buffer — no internal formatting buffer.
> - `diag_print` depends on `FILE*` / `fprintf` — not suitable for embedded targets without stdio.
> - `diag_clear` does not zero frame contents — stale data remains in unused slots (depth is the authority).
> - `file` and `func` are string literal pointers — never copied, never freed, but also never safe to store beyond program lifetime if pointing to dynamically loaded code.

---

### `error.h`
> Common error codes and human-readable messages for use with `Result<T, Error>` throughout the library. Simple flat enum — no hierarchies, zero runtime cost, thread-safe pure functions.

#### Error Codes

| Code | Range | Meaning |
|---|---|---|
| `ERR_OK` | 0 | Success (rarely appears in `Result::Err`) |
| `ERR_INVALID_ARG` | 1–99 | NULL pointer, invalid value, precondition failed |
| `ERR_OUT_OF_RANGE` | | Index/value exceeds bounds |
| `ERR_PARSE_FAILED` | | String/number parsing failed |
| `ERR_INVALID_FORMAT` | | Data format corrupted or invalid |
| `ERR_INVALID_STATE` | | Operation invalid in current state |
| `ERR_OUT_OF_MEMORY` | 100–199 | Allocation failed |
| `ERR_BUFFER_TOO_SMALL` | | Buffer too small for result |
| `ERR_CAPACITY_EXCEEDED` | | Fixed container or resource full |
| `ERR_NOT_FOUND` | | Key or resource not found |
| `ERR_IO_FAILED` | 200–299 | File/network/device I/O error |
| `ERR_PERMISSION` | | Access denied |
| `ERR_TIMEOUT` | | Operation timed out |
| `ERR_NOT_SUPPORTED` | | Feature unavailable on platform |
| `ERR_ALREADY_EXISTS` | | Resource or file already exists |
| `ERR_OVERFLOW` | 300–399 | Numeric overflow |
| `ERR_UNDERFLOW` | | Numeric underflow |
| `ERR_DIVIDE_BY_ZERO` | | Division or modulo by zero |
| `ERR_UNKNOWN` | 400+ | Catch-all / unspecified error |
| `ERR_NOT_IMPLEMENTED` | | Feature stubbed / not complete |
| `ERR_COUNT` | — | Sentinel — do not use as real error |

#### Utility Functions
```c
const char* error_message(Error e)   // static string, never NULL, O(1) switch lookup
bool        error_is_ok(Error e)     // true if ERR_OK
bool        error_is_valid(Error e)  // true if e >= ERR_OK && e < ERR_COUNT
int         error_code(Error e)      // plain integer cast
```

#### Result\<T, Error\> Convenience Macros

Requires `CANON_RESULT(T, Error)` to have been called first.
```c
RESULT_OK(T, val)       // result_T_Error_ok(val)
RESULT_ERR(T, code)     // result_T_Error_err(code)

// Returns error_message() string if Err, NULL if Ok
RESULT_ERROR_MSG(T, res)
```

#### Typical usage
```c
CANON_RESULT(int, Error)

result_int_Error parse_number(const char* s) {
    if (!s) return RESULT_ERR(int, ERR_INVALID_ARG);

    char* end;
    long val = strtol(s, &end, 10);
    if (end == s || *end != '\0') return RESULT_ERR(int, ERR_PARSE_FAILED);

    return RESULT_OK(int, (int)val);
}

// Caller:
result_int_Error r = parse_number(input);
if (result_int_Error_is_err(r)) {
    fprintf(stderr, "Error: %s\n", RESULT_ERROR_MSG(int, r));
}
```

#### Extension guide

Add new codes before `ERR_COUNT` in the enum, then add a `case` in `error_message()`. For domain-specific errors, define a separate enum (e.g. `ErrorJson`) and a separate `CANON_RESULT(T, ErrorJson)`.

> **Known Limitations:**
> - Flat enum only — no error hierarchies or subcategories.
> - `RESULT_OK` / `RESULT_ERR` / `RESULT_ERROR_MSG` macros assume the error type is literally `Error` — they will not work with custom error enums without modification.
> - `error_message` returns `"Unknown error"` for both `ERR_UNKNOWN` and any out-of-range value — callers cannot distinguish the two.
> - `ERR_COUNT` is a sentinel — using it as a real error code will produce misleading messages.

### `dynstring.h`
> Auto-growing heap string builder. Always null-terminated. No arena support — use `data/stringbuf.h` for arena-backed strings. Must call `dynstring_free()` when done.

#### Configuration (override before `#include`)
```c
DYNSTRING_INITIAL_CAPACITY  // default 64 — first heap allocation size in bytes
DYNSTRING_GROWTH_FACTOR     // default 2  — capacity multiplier on realloc
```

#### Struct
```c
typedef struct {
    char* data;  // heap buffer (NULL until first append)
    usize len;   // string length, excluding '\0'
    usize cap;   // total buffer capacity, including '\0'
} DynString;
```

Do not access or modify fields directly — use the provided functions.

#### Constructors

**`DynString dynstring_init(void)`**
Returns a zero-initialized DynString. No heap allocation until first append.
```c
DynString s = dynstring_init();
// s.data == NULL, s.len == 0, s.cap == 0
```

**`DynString dynstring_with_capacity(usize capacity)`**
Pre-allocates `capacity` bytes. Use when final size is roughly known.
On OOM returns `dynstring_init()` — check with `dynstring_capacity()`.
```c
DynString s = dynstring_with_capacity(256);
```

**`DynString dynstring_from(const char* str)`**
Creates a DynString by copying a C string. NULL input returns empty. On OOM returns `dynstring_init()`.
```c
DynString s = dynstring_from("hello");
```

#### Queries

**`usize dynstring_len(const DynString* s)`**
Returns string length excluding `'\0'`. NULL-safe — returns 0.

**`usize dynstring_capacity(const DynString* s)`**
Returns current heap capacity including `'\0'`. NULL-safe — returns 0.

**`bool dynstring_is_empty(const DynString* s)`**
Returns true if len == 0 or s == NULL.

**`const char* dynstring_str(const DynString* s)`**
Returns pointer to the null-terminated string. Never returns NULL — returns `""` if s == NULL or data == NULL. Pointer is valid until the next modification or `dynstring_free()`. Do not free the result.
```c
printf("%s\n", dynstring_str(&s));
```

#### Append

All append functions return `false` on allocation failure and leave `s` unchanged. NULL-safe on `s`.

**`bool dynstring_append(DynString* s, const char* str)`**
Appends a null-terminated C string. NULL `str` is a no-op — returns true.
```c
dynstring_append(&s, "world");
```

**`bool dynstring_append_char(DynString* s, char c)`**
Appends a single character.
```c
dynstring_append_char(&s, '!');
```

**`bool dynstring_append_fmt(DynString* s, const char* fmt, ...)`**
printf-style formatted append. Uses two `vsnprintf` passes — measure then write. Returns false on format error or OOM.
```c
dynstring_append_fmt(&s, "value=%d", 42);
```

**`bool dynstring_append_n(DynString* s, const char* str, usize n)`**
Appends at most `n` characters from `str`. Stops early at null terminator. NULL `str` is a no-op — returns true.
```c
dynstring_append_n(&s, "hello world", 5);  // appends "hello"
```

#### Mutation

**`void dynstring_clear(DynString* s)`**
Resets len to 0 and writes `'\0'` at index 0. No-op if data == NULL (e.g. after `dynstring_init()` with no appends). Does NOT free or zero the buffer. NULL-safe.

**`void dynstring_truncate(DynString* s, usize new_len)`**
Truncates to `new_len` characters. No-op if `new_len >= current len` or data == NULL. NULL-safe.
```c
dynstring_truncate(&s, 5);
```

#### Capacity Management

**`bool dynstring_reserve(DynString* s, usize min_cap)`**
Ensures buffer can hold at least `min_cap` bytes including null terminator. No-op if already sufficient. Returns false on OOM.

**`bool dynstring_shrink_to_fit(DynString* s)`**
Reallocates buffer to exactly `len + 1` bytes. Frees entirely if len == 0. Returns false if realloc fails — s is unchanged on failure.

#### Memory

**`void dynstring_free(DynString* s)`**
Frees the heap buffer and resets all fields to zero. NULL-safe.
```c
DynString s = dynstring_init();
dynstring_append(&s, "Hello, ");
dynstring_append_fmt(&s, "%s!", name);
printf("%s\n", dynstring_str(&s));
dynstring_free(&s);  // REQUIRED — always call this
```

#### Utility

**`char* dynstring_to_cstr(const DynString* s)`**
Returns a heap-allocated copy as a plain C string. The returned pointer is independent of the DynString. Caller must `free()` the result. Returns `NULL` on allocation failure — always check the return value.

#### Performance

| Operation | Time | Notes |
|---|---|---|
| `append` | Amortized O(n) | O(len + n) on realloc |
| `append_char` | Amortized O(1) | O(len) on realloc |
| `append_fmt` | O(n) | Two `vsnprintf` passes |
| `append_n` | Amortized O(n) | O(len + n) on realloc |
| `to_cstr` | O(len) | Returns NULL on OOM — always check |
| All queries | O(1) | |
| `free` | O(1) | |

> **Known Limitations:**
> - No arena support — use `data/stringbuf.h` for arena-backed strings.
> - `dynstring_clear()` is a no-op if data == NULL — calling it on a freshly `dynstring_init()`-ed string with no appends does nothing.
> - `dynstring_clear()` does NOT zero buffer contents — stale data remains readable until overwritten.
> - `dynstring_with_capacity()` and `dynstring_from()` silently return an empty `DynString` on OOM — check `dynstring_capacity()` if pre-allocation is required.
> - `dynstring_append_fmt()` makes two `vsnprintf` passes — for tight loops prefer `append_n` or `append` with pre-formatted strings.
> - `realloc` is called directly rather than through a `mem_realloc` wrapper — growth relies on `<stdlib.h>` being included explicitly.
> - No overflow guard on capacity growth — extremely large appends may cause `realloc` to fail without a prior size check.
> - Not thread-safe — concurrent access requires external synchronization.

### `dynvec.h`
> Auto-growing typed heap vector, generated per element type via `DEFINE_DYNVEC(type)`. No arena support — use `data/vec/vec.h` for arena-backed vectors. Must call `dynvec_##type##_free()` when done.

#### Configuration (override before `#include`)
```c
DYNVEC_INITIAL_CAPACITY  // default 8 — first heap allocation in elements
DYNVEC_GROWTH_FACTOR     // default 2 — capacity multiplier on realloc
```

#### Setup
```c
DEFINE_DYNVEC(int)   // generates dynvec_int + all functions

// For pointer types — typedef first:
typedef void* voidptr;
DEFINE_DYNVEC(voidptr)
```

Must be used at file/global scope. Use once per type. Element type must be trivially copyable (memcpy-safe).

#### Generated Struct
```c
typedef struct {
    type* data;  // heap buffer (NULL until first push)
    usize len;   // current element count
    usize cap;   // allocated capacity in elements
} dynvec_##type;
```

Do not access or modify fields directly — use the provided functions.

#### Constructors

**`dynvec_T dynvec_T_init(void)`**
Returns a zero-initialized dynvec. No heap allocation until first push.
```c
dynvec_int v = dynvec_int_init();
// v.data == NULL, v.len == 0, v.cap == 0
```

**`dynvec_T dynvec_T_with_capacity(usize capacity)`**
Pre-allocates `capacity` elements. Returns `_init()` result on OOM or `capacity == 0`.
```c
dynvec_int v = dynvec_int_with_capacity(64);
```

#### Queries

**`usize dynvec_T_len(const dynvec_T* v)`**
Returns current element count. NULL-safe — returns 0.

**`usize dynvec_T_capacity(const dynvec_T* v)`**
Returns current buffer capacity in elements. NULL-safe — returns 0.

**`bool dynvec_T_is_empty(const dynvec_T* v)`**
Returns true if len == 0 or v == NULL.

#### Element Access

**`bool dynvec_T_get(const dynvec_T* v, usize i, T* out)`**
Bounds-checked copy into `*out`. Returns false if v == NULL, out == NULL, or i >= len.
```c
int val;
if (dynvec_int_get(&v, 2, &val)) { /* use val */ }
```

**`T dynvec_T_get_unchecked(const dynvec_T* v, usize i)`**
No bounds check. Debug-only `ensure_msg` — UB in release if i >= len.

**`bool dynvec_T_set(dynvec_T* v, usize i, T value)`**
Bounds-checked write. Returns false if v == NULL or i >= len.

**`T* dynvec_T_data(const dynvec_T* v)`**
Returns raw buffer pointer. NULL if v == NULL or no allocation yet.

**`T* dynvec_T_first(const dynvec_T* v)`**
Returns pointer to first element. NULL if empty or v == NULL.

**`T* dynvec_T_last(const dynvec_T* v)`**
Returns pointer to last element. NULL if empty or v == NULL.

#### Modification

All modification functions return `false` on allocation failure and leave `v` unchanged.

**`bool dynvec_T_push(dynvec_T* v, T value)`**
Appends element, growing buffer if needed. Returns false on OOM or v == NULL.
```c
dynvec_int_push(&v, 42);
```

**`bool dynvec_T_pop(dynvec_T* v, T* out)`**
Removes and returns last element. Returns false if empty, v == NULL, or out == NULL.
```c
int val;
dynvec_int_pop(&v, &val);
```

**`bool dynvec_T_insert(dynvec_T* v, usize i, T value)`**
Inserts at index `i` (must be `<= len`), shifting elements right. May realloc. O(n).

**`bool dynvec_T_remove(dynvec_T* v, usize i, T* out)`**
Removes at index `i` (must be `< len`), shifting elements left. No shrink. O(n).

**`void dynvec_T_clear(dynvec_T* v)`**
Resets len to 0. Does NOT free buffer or zero contents. NULL-safe.

#### Bulk Operations

**`bool dynvec_T_extend(dynvec_T* v, const T* src, usize count)`**
Appends `count` elements from `src`. Returns false on v == NULL, src == NULL, or OOM.
```c
int items[] = {1, 2, 3};
dynvec_int_extend(&v, items, 3);
```

#### Capacity Management

**`bool dynvec_T_reserve(dynvec_T* v, usize min_cap)`**
Ensures capacity for at least `min_cap` elements. No-op if already sufficient. Returns false on OOM.

**`bool dynvec_T_shrink_to_fit(dynvec_T* v)`**
Reallocates to exact `len`. Frees entirely if len == 0. Returns false on realloc failure — v is unchanged on failure.

#### Memory

**`void dynvec_T_free(dynvec_T* v)`**
Frees buffer and resets all fields to zero. NULL-safe.
```c
dynvec_int v = dynvec_int_with_capacity(32);
dynvec_int_push(&v, 10);
dynvec_int_push(&v, 20);
dynvec_int_free(&v);  // REQUIRED — always call this
```

#### Performance

| Operation | Time | Notes |
|---|---|---|
| `push` | Amortized O(1) | O(n) on realloc |
| `pop` | O(1) | |
| `insert` / `remove` | O(n) | Shifts elements |
| `extend` | O(count) | May realloc |
| `get` / `set` / `first` / `last` | O(1) | |
| `shrink_to_fit` | O(n) | Realloc |
| `free` | O(1) | |

> **Known Limitations:**
> - Element type must be trivially copyable — types with non-trivial copy semantics or internal pointers will be shallow-copied by `mem_copy` / `mem_move`.
> - `get_unchecked` has no release-build bounds protection — always verify index bounds yourself.
> - `clear()` does not zero buffer contents — stale element data remains readable until overwritten.
> - `realloc` is called directly in `_grow()` rather than through a `mem_realloc` wrapper — growth relies on `<stdlib.h>` being included explicitly.
> - No overflow guard on capacity growth — `_grow()` does not check against `CANON_VEC_MAX_CAPACITY` despite `limits.h` being imported. Extremely large element counts may cause `realloc` to fail without a prior size check.
> - `mem_zero` is imported via `memory.h` but unused — likely intended for a future `_clear_zero()` variant mirroring `arena_reset_secure` and `pool_reset_secure`.
> - No arena support — use `data/vec/vec.h` for arena-backed or bounded vectors.
> - Not thread-safe — concurrent modifications require external synchronization.

### `smallvec.h`
> Inline-first typed vector with at-most-one spill to heap or arena, generated per type via `DEFINE_SMALLVEC(type, INLINE_CAP)`. Elements live inside the struct until `INLINE_CAP` is exceeded — at that point a single allocation occurs and capacity is fixed. No further growth after spill.

#### Core Behaviour

- While `len <= INLINE_CAP`: elements live in `inline_buf` inside the struct — zero heap allocation.
- On first overflow: spills once to heap (`malloc`) or arena, doubling capacity.
- After spill: capacity is fixed — push into a full post-spill buffer returns `false`, no second realloc.

#### Setup
```c
DEFINE_SMALLVEC(int, 8)   // inline storage for 8 ints; spills to heap on overflow

// Arena-backed spill — no free() needed:
smallvec_int v = smallvec_int_init_arena(&my_arena);

// For pointer types — typedef first:
typedef void* voidptr;
DEFINE_SMALLVEC(voidptr, 4)
```

Must be used at file/global scope. Use once per `(type, INLINE_CAP)` pair. Element type must be trivially copyable (mem_copy-safe). `INLINE_CAP` must be > 0.

#### Generated Struct
```c
typedef struct {
    type* data;                   // active buffer (inline_buf or heap/arena)
    usize len;                    // current element count
    usize cap;                    // capacity of active buffer in elements
    Arena* arena;                 // spill destination (NULL = malloc)
    bool using_inline;            // true if data == inline_buf
    type inline_buf[INLINE_CAP];  // inline storage — lives inside the struct
} smallvec_##type;
```

Do not access or modify fields directly — use the provided functions.

#### Constructors

**`smallvec_T smallvec_T_init(void)`**
Initializes with inline storage. No heap allocation.
```c
smallvec_int v = smallvec_int_init();
// v.data == v.inline_buf, v.cap == INLINE_CAP, v.using_inline == true
```

**`smallvec_T smallvec_T_init_arena(Arena* arena)`**
Same as `_init()` but stores arena pointer — spill allocates from arena instead of `malloc`. `arena` must not be NULL — checked via `require_msg()`.
```c
smallvec_int v = smallvec_int_init_arena(&scratch);
// spill goes into scratch — no free() needed, arena_reset() handles cleanup
```

#### Queries

**`usize smallvec_T_len(const smallvec_T* v)`**
Returns current element count. NULL-safe — returns 0.

**`usize smallvec_T_capacity(const smallvec_T* v)`**
Returns current buffer capacity in elements. NULL-safe — returns 0.

**`bool smallvec_T_is_empty(const smallvec_T* v)`**
Returns true if len == 0 or v == NULL.

**`bool smallvec_T_using_inline(const smallvec_T* v)`**
Returns true if spill has not yet occurred. NULL-safe — returns false.

#### Element Access

**`bool smallvec_T_get(const smallvec_T* v, usize i, T* out)`**
Bounds-checked copy into `*out`. Returns false if v == NULL, out == NULL, or i >= len.

**`T smallvec_T_get_unchecked(const smallvec_T* v, usize i)`**
No bounds check. Debug-only `ensure_msg` — UB in release if i >= len.

**`T* smallvec_T_data(const smallvec_T* v)`**
Returns raw buffer pointer. NULL if v == NULL.

**`T* smallvec_T_first(const smallvec_T* v)`**
Returns pointer to first element. NULL if empty or v == NULL.

**`T* smallvec_T_last(const smallvec_T* v)`**
Returns pointer to last element. NULL if empty or v == NULL.

#### Push / Pop

**`bool smallvec_T_push(smallvec_T* v, T value)`**
Appends element. Spills once if inline is full. Returns false on spill failure, if post-spill buffer is also full, or if v == NULL.
```c
smallvec_int_push(&v, 99);
```

**`bool smallvec_T_pop(smallvec_T* v, T* out)`**
Removes and returns last element. Returns false if empty, v == NULL, or out == NULL.

#### Insert / Remove

**`bool smallvec_T_insert(smallvec_T* v, usize i, T value)`**
Inserts at index `i` (must be `<= len`), shifting right. Triggers spill if inline is full. Returns false if already spilled and full — no second growth. O(n).

**`bool smallvec_T_remove(smallvec_T* v, usize i, T* out)`**
Removes at index `i` (must be `< len`), shifting left. Returns false if OOB, v == NULL, or out == NULL. O(n).

#### Bulk

**`bool smallvec_T_extend(smallvec_T* v, const T* src, usize count)`**
Appends `count` elements from `src`. Triggers spill if needed. Returns false on v == NULL, src == NULL, spill failure, or insufficient post-spill capacity.

#### Clear / Free

**`void smallvec_T_clear(smallvec_T* v)`**
Resets len to 0. Does not free spill buffer, zero contents, or revert to inline storage. NULL-safe.

**`void smallvec_T_free(smallvec_T* v)`**
If spilled to heap (not arena), calls `free()` on the spill buffer then re-initializes to inline state. No-op for arena-spilled instances — `arena_reset()` handles cleanup. NULL-safe.
```c
smallvec_int v = smallvec_int_init();
smallvec_int_push(&v, 1);
// ...
smallvec_int_free(&v);  // REQUIRED if heap-spilled — safe to call either way
```

#### Interop

**`canon_vec_T smallvec_T_as_vec(smallvec_T* v)`**
Returns a zero-copy borrowed `canon_vec_T` view over the current buffer. Does NOT transfer ownership — do not free or store beyond `v`'s lifetime. v must not be NULL — checked via `require_msg()`. Do not call any reallocating vec functions on the returned view.
```c
canon_vec_int view = smallvec_int_as_vec(&v);
// pass to any API accepting canon_vec_int
```

#### Spill Decision Summary

| Situation | Spill target | Free needed? |
|---|---|---|
| `arena == NULL` (default) | `malloc` heap | Yes — call `_free()` |
| `arena != NULL` | arena | No — `arena_reset()` handles it |
| Never exceeded `INLINE_CAP` | No spill | No |

#### Performance

| Operation | Time | Notes |
|---|---|---|
| `push` (inline) | O(1) | No allocation |
| `push` (spill) | O(n) | Single alloc + mem_copy, at most once |
| `push` (post-spill) | O(1) | Returns false if full |
| `pop` | O(1) | |
| `insert` / `remove` | O(n) | mem_move |
| `extend` | O(count) | May trigger one spill |
| All queries | O(1) | |
| `free` | O(1) | |

> **Known Limitations:**
> - At-most-one growth — post-spill push into a full buffer returns `false`. Size `INLINE_CAP` and expected max count accordingly.
> - `clear()` does not revert to inline storage — after a spill, the spill buffer is retained even after clear.
> - `clear()` does not zero buffer contents — stale element data remains readable until overwritten.
> - `get_unchecked` has no release-build bounds protection — always verify index bounds yourself.
> - Element type must be trivially copyable — spill uses `mem_copy`.
> - `as_vec` returns a borrowed view — do not call any reallocating vec functions on it and do not store it beyond `v`'s lifetime.
> - Struct size includes `INLINE_CAP` inline slots — large values on the stack may cause stack pressure in deeply recursive code.
> - No overflow guard on spill capacity — `_spill()` does not check against `CANON_VEC_MAX_CAPACITY` despite `limits.h` being imported. Extremely large spill counts may cause `malloc` to fail without a prior size check.
> - `realloc` is not used — spill is a single `malloc` or arena allocation. No `mem_realloc` wrapper needed here.
> - Not thread-safe — concurrent modifications require external synchronization.

### `deque.h`
> Bounded double-ended queue backed by a caller-owned ring buffer. Fixed capacity — no automatic growth. O(1) push/pop at both ends. Push/pop operations return `Result<bool, Error>`. `Option` variants available for pop and peek.

#### Ring Buffer Invariant

- `head` = index of front element (`pop_front` reads here)
- `tail` = index where next `push_back` writes
- `size` = current element count (`0 <= size <= capacity`)
- Full when `size == capacity`, empty when `size == 0`
- All index arithmetic is modulo capacity

#### Quick Start
```c
#include "data/deque/deque.h"

DEFINE_DEQUE(static inline, int)

int buf[64];
canon_deque_int d;
canon_deque_int_init(&d, buf, 64);

canon_deque_int_push_back(&d, 10);
canon_deque_int_push_front(&d, 5);
// d is now: [5, 10]

int val;
canon_deque_int_pop_front(&d, &val);  // val = 5
canon_deque_int_pop_back(&d, &val);   // val = 10

// Option variants — no out param needed
option_int front = canon_deque_int_pop_front_option(&d);
option_int back  = canon_deque_int_peek_back_option(&d);
```

#### Separate Compilation
```c
// In tasks.h:
#include "data/deque/deque_decl.h"
DECLARE_DEQUE(Task)

// In tasks.c:
#include "data/deque/deque_defn.h"
DEFINE_DEQUE(, Task)
```

#### Pointer Types
```c
typedef void* voidptr;
DEFINE_DEQUE(static inline, voidptr)
```

#### Common Use Cases
- Task queues — `push_back` to enqueue, `pop_front` to dequeue
- Sliding window — `push_back` + `pop_front` when full
- Undo/redo — `push_back`, `pop_back` to undo
- BFS frontier buffers
- Rate-limiting / token buckets

> **Known Limitations:**
> - `option_##type` must be instantiated via `CANON_OPTION(type)` before calling `DEFINE_DEQUE` — not enforced or included automatically. Missing this produces cryptic compile errors.
> - Fixed capacity — no growth. For auto-growing deques, build a wrapper using `dynvec.h`.
> - Not thread-safe — concurrent modifications require external synchronization.
> - Not suitable for random access by index — use `vec` instead.

### `deque_decl.h`
> Declaration macros for typed deques — for separate compilation. Use in `.h` files to declare a typed deque struct and all function signatures without generating implementations. Pair with `DEFINE_DEQUE()` in `deque_defn.h` in exactly one `.c` file.

#### When to Use

Use `deque_decl.h` + `deque_defn.h` together when you want faster incremental builds by defining deque types once in a `.c` file instead of re-generating them in every translation unit.

#### Workflow
```c
// In tasks.h — declare only
#include "data/deque/deque_decl.h"
DECLARE_DEQUE(int)
DECLARE_DEQUE(Task)

// In tasks.c — define once
#include "data/deque/deque_defn.h"
DEFINE_DEQUE(, int)
DEFINE_DEQUE(, Task)

// In main.c — just include the header
#include "tasks.h"
canon_deque_int d;
```

#### Pointer Types
```c
typedef const char* cstr;
DECLARE_DEQUE(cstr)
```

#### What `DECLARE_DEQUE(type)` Emits

- Struct typedef: `canon_deque_##type`
- `extern` declarations for all functions generated by `DEFINE_DEQUE()`

| Category | Declared functions |
|---|---|
| Constructor | `_init`, `_empty` |
| Queries | `_len`, `_capacity`, `_remaining`, `_is_empty`, `_is_full` |
| Push | `_push_front`, `_push_back` |
| Pop (Result) | `_pop_front`, `_pop_back` |
| Pop (Option) | `_pop_front_option`, `_pop_back_option` |
| Peek (bool) | `_peek_front`, `_peek_back` |
| Peek (Option) | `_peek_front_option`, `_peek_back_option` |
| Misc | `_clear`, `_swap` |

> **Known Limitations:**
> - Linkage is always `extern` — not configurable in the declaration path.
> - Must be matched by exactly one `DEFINE_DEQUE(, type)` in a `.c` file — multiple definitions cause linker errors.
> - Do NOT use `DECLARE_DEQUE` if you are using the header-only path via `deque.h`.
> - `option_##type` must be instantiated via `CANON_OPTION(type)` before `DECLARE_DEQUE` — not enforced automatically.

### `deque_defn.h`
> Definition macros for typed deques — generates all struct typedefs and function implementations. Use `DEFINE_DEQUE(linkage, type)` in `.c` files for separate compilation, or in `.h` files with `static inline` for header-only libraries.

#### Workflow

Header-only:
```c
#include "data/deque/deque_defn.h"
DEFINE_DEQUE(static inline, int)

int buf[64];
canon_deque_int d;
canon_deque_int_init(&d, buf, 64);
canon_deque_int_push_back(&d, 42);
```

Separate compilation:
```c
// In tasks.h:
#include "data/deque/deque_decl.h"
DECLARE_DEQUE(Task)

// In tasks.c:
#include "data/deque/deque_defn.h"
DEFINE_DEQUE(, Task)
```

Pointer types (typedef first):
```c
typedef void* voidptr;
DEFINE_DEQUE(static inline, voidptr)
```

#### Generated Functions for `DEFINE_DEQUE(linkage, type)`

| Category | Generated functions | Return type |
|---|---|---|
| Constructor | `_init(d, buffer, capacity)` | `void` |
| Constructor | `_empty()` | `canon_deque_##type` |
| Queries | `_len(d)` | `usize` |
| Queries | `_capacity(d)` | `usize` |
| Queries | `_remaining(d)` | `usize` |
| Queries | `_is_empty(d)` | `bool` |
| Queries | `_is_full(d)` | `bool` |
| Push | `_push_front(d, item)` | `result_bool_Error` |
| Push | `_push_back(d, item)` | `result_bool_Error` |
| Pop (Result) | `_pop_front(d, out)` | `result_bool_Error` |
| Pop (Result) | `_pop_back(d, out)` | `result_bool_Error` |
| Pop (Option) | `_pop_front_option(d)` | `option_##type` |
| Pop (Option) | `_pop_back_option(d)` | `option_##type` |
| Peek (bool) | `_peek_front(d, out)` | `bool` |
| Peek (bool) | `_peek_back(d, out)` | `bool` |
| Peek (Option) | `_peek_front_option(d)` | `option_##type` |
| Peek (Option) | `_peek_back_option(d)` | `option_##type` |
| Misc | `_clear(d)` | `void` |
| Misc | `_swap(a, b)` | `void` |

> **Known Limitations:**
> - `option_##type` must be instantiated via `CANON_OPTION(type)` before calling `DEFINE_DEQUE` — not enforced or included automatically.
> - `DEFINE_DEQUE` must be used at file/global scope, not inside functions.
> - With `extern` linkage, define exactly once across all translation units — multiple definitions cause linker errors.
> - With `static inline`, each translation unit gets its own copy — safe but increases binary size.
> - Element type must be a valid C identifier — use `typedef` for pointer types.

### `deque_impl.h`
> Pure implementation logic macros for the Canon-C deque module. No naming decisions are made here — every identifier is received as a parameter from `deque_defn.h`. Do not include this file directly.

#### Struct Layout
```c
IMPL_DEQUE_STRUCT(DequeType, DequeTag, type)
// expands to:
// {
//     type* buffer;   — caller-owned element buffer
//     usize capacity; — fixed maximum element count
//     usize head;     — index of front element (pop_front reads here)
//     usize tail;     — index where next push_back writes
//     usize size;     — current element count (0 <= size <= capacity)
// }
// sizeof(DequeType) = sizeof(type*) + 4*sizeof(usize)
// total memory: sizeof(DequeType) + capacity*sizeof(type)
```

#### Available Implementation Macros

| Macro | Behavior |
|---|---|
| `IMPL_DEQUE_STRUCT(DequeType, DequeTag, type)` | Generates struct typedef |
| `IMPL_DEQUE_INIT(linkage, DequeType, fn, type)` | Initializes deque with caller-owned buffer; requires `d != NULL`, `buffer != NULL`, `capacity > 0`, capacity within `CANON_DEQUE_MAX_CAPACITY` |
| `IMPL_DEQUE_EMPTY(linkage, DequeType, fn)` | Returns zero-initialized deque with no buffer |
| `IMPL_DEQUE_LEN(linkage, DequeType, fn)` | Returns `d->size`; 0 if `d == NULL` |
| `IMPL_DEQUE_CAPACITY(linkage, DequeType, fn)` | Returns `d->capacity`; 0 if `d == NULL` |
| `IMPL_DEQUE_REMAINING(linkage, DequeType, fn)` | Returns `capacity - size`; 0 if `d == NULL` |
| `IMPL_DEQUE_IS_EMPTY(linkage, DequeType, fn)` | Returns `true` if `size == 0` or `d == NULL` |
| `IMPL_DEQUE_IS_FULL(linkage, DequeType, fn)` | Returns `true` if `size >= capacity` or `d == NULL` |
| `IMPL_DEQUE_PUSH_FRONT(linkage, DequeType, fn, type)` | Decrements head modulo capacity, writes item; `Err(ERR_INVALID_ARG)` if null, `Err(ERR_CAPACITY_EXCEEDED)` if full |
| `IMPL_DEQUE_PUSH_BACK(linkage, DequeType, fn, type)` | Writes item at tail, increments tail modulo capacity; same error returns as push_front |
| `IMPL_DEQUE_POP_FRONT(linkage, DequeType, fn, type)` | Copies front element to `*out`, advances head; `Err(ERR_INVALID_ARG)` if null, `Err(ERR_INVALID_STATE)` if empty |
| `IMPL_DEQUE_POP_BACK(linkage, DequeType, fn, type)` | Decrements tail, copies back element to `*out`; same error returns as pop_front |
| `IMPL_DEQUE_POP_FRONT_OPTION(...)` | Calls pop_front internally; returns `Some(value)` or `None` |
| `IMPL_DEQUE_POP_BACK_OPTION(...)` | Calls pop_back internally; returns `Some(value)` or `None` |
| `IMPL_DEQUE_PEEK_FRONT(linkage, DequeType, fn, type)` | Copies front element to `*out` without removing; returns `false` if null or empty |
| `IMPL_DEQUE_PEEK_BACK(linkage, DequeType, fn, type)` | Copies back element to `*out` without removing; returns `false` if null or empty |
| `IMPL_DEQUE_PEEK_FRONT_OPTION(...)` | Calls peek_front internally; returns `Some(value)` or `None` |
| `IMPL_DEQUE_PEEK_BACK_OPTION(...)` | Calls peek_back internally; returns `Some(value)` or `None` |
| `IMPL_DEQUE_CLEAR(linkage, DequeType, fn)` | Resets `size`, `head`, `tail` to 0; NULL-safe; does NOT zero buffer contents |
| `IMPL_DEQUE_SWAP(linkage, DequeType, fn)` | Exchanges two deques via struct copy on stack; requires both non-NULL |

#### Capacity Limit
```c
// Default: reuses CANON_VEC_MAX_CAPACITY from limits.h (1 GB)
// Override before including:
#define CANON_DEQUE_MAX_CAPACITY (256 * CANON_MB)
#include "data/deque/deque_impl.h"
```

#### `result_bool_Error` Instantiation

`deque_impl.h` instantiates `CANON_RESULT(bool, Error)` exactly once using a define guard:
```c
#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
    #define CANON_RESULT_BOOL_ERROR_DEFINED
    CANON_RESULT(bool, Error)
#endif
```

> **Known Limitations:**
> - These macros are internal — most users should never include `deque_impl.h` directly. Use `deque.h` or `deque_defn.h` instead.
> - `IMPL_DEQUE_CLEAR` does not zero buffer contents — stale data remains readable until overwritten. For sensitive data, zero the buffer manually before calling clear.
> - `IMPL_DEQUE_INIT` enforces `capacity > 0` via `require_msg` — zero-capacity deques are not allowed.
> - `CANON_RESULT(bool, Error)` is instantiated at the top of `deque_impl.h` under a define guard (`CANON_RESULT_BOOL_ERROR_DEFINED`). This must appear before any `IMPL_DEQUE_PUSH_*/POP_*` expansion and is intentionally placed there for that reason.
> - Overrides to `CANON_DEQUE_MAX_CAPACITY` must be defined before the first include of `deque_impl.h` in that translation unit.

### `deque_mangle.h`
> Name mangling conventions for the Canon-C deque module. Single source of truth for all generated type and function names. Override any macro before including to rename the entire API globally.

#### Default Naming Scheme

For a type `T`, the defaults produce:

| What | Default name | Example for `int` |
|---|---|---|
| Type | `canon_deque_T` | `canon_deque_int` |
| Struct tag | `canon_deque_T_s` | `canon_deque_int_s` |
| `init` | `canon_deque_T_init` | `canon_deque_int_init` |
| `empty` | `canon_deque_T_empty` | `canon_deque_int_empty` |
| `len` | `canon_deque_T_len` | `canon_deque_int_len` |
| `capacity` | `canon_deque_T_capacity` | `canon_deque_int_capacity` |
| `remaining` | `canon_deque_T_remaining` | `canon_deque_int_remaining` |
| `is_empty` | `canon_deque_T_is_empty` | `canon_deque_int_is_empty` |
| `is_full` | `canon_deque_T_is_full` | `canon_deque_int_is_full` |
| `push_front` | `canon_deque_T_push_front` | `canon_deque_int_push_front` |
| `push_back` | `canon_deque_T_push_back` | `canon_deque_int_push_back` |
| `pop_front` | `canon_deque_T_pop_front` | `canon_deque_int_pop_front` |
| `pop_back` | `canon_deque_T_pop_back` | `canon_deque_int_pop_back` |
| `pop_front_option` | `canon_deque_T_pop_front_option` | `canon_deque_int_pop_front_option` |
| `pop_back_option` | `canon_deque_T_pop_back_option` | `canon_deque_int_pop_back_option` |
| `peek_front` | `canon_deque_T_peek_front` | `canon_deque_int_peek_front` |
| `peek_back` | `canon_deque_T_peek_back` | `canon_deque_int_peek_back` |
| `peek_front_option` | `canon_deque_T_peek_front_option` | `canon_deque_int_peek_front_option` |
| `peek_back_option` | `canon_deque_T_peek_back_option` | `canon_deque_int_peek_back_option` |
| `clear` | `canon_deque_T_clear` | `canon_deque_int_clear` |
| `swap` | `canon_deque_T_swap` | `canon_deque_int_swap` |

#### Customization

Define overrides **before** including any deque header:
```c
// Project namespace prefix
#define MANGLE_DEQUE_TYPE(type)       myproject_deque_##type
#define MANGLE_DEQUE_PUSH_FRONT(type) myproject_deque_##type##_push_front
#include "data/deque/deque_mangle.h"
#include "data/deque/deque_defn.h"
DEFINE_DEQUE(static inline, int)

myproject_deque_int d;
myproject_deque_int_push_front(&d, 42);
```

> **Known Limitations:**
> - All macros are guarded with `#ifndef` — overrides must be defined before the first include of any deque header in that translation unit.
> - Overrides apply to all types instantiated after them, not selectively per type.

### `hashmap.h`
> Generic Robin Hood open-addressed hashmap — header-only entry point. Including this file generates a complete, statically-inlined hashmap implementation for your key and value types. No `.c` file or build system integration required.

#### Setup

Before including, provide a hash function, an equality function, and define the configuration macros:
```c
static u64 hash_u64(const u64* key, void* ctx) {
    (void)ctx;
    u64 h = *key * 11400714819323198485ULL;
    return h == 0 ? 1 : h;  // 0 is reserved as empty sentinel
}

static bool eq_u64(const u64* a, const u64* b, void* ctx) {
    (void)ctx;
    return *a == *b;
}

#define HASHMAP_KEY_TYPE u64
#define HASHMAP_VAL_TYPE int
#define HASHMAP_HASH_FN  hash_u64
#define HASHMAP_EQ_FN    eq_u64
#include "data/hashmap/hashmap.h"
```

#### Sizing & Initialization
```c
// To store N elements safely: capacity >= bits_next_power_of_two(N * 4 / 3 + 1)
usize cap = bits_next_power_of_two(100 * 4 / 3 + 1);  // 100 elements → 256
u8 buf[hashmap_buffer_size(cap)];                       // stack or static
// or: u8* buf = malloc(hashmap_buffer_size(cap));      // heap

hashmap map;
result_bool_Error res = hashmap_init(&map, bytes_from(buf, sizeof(buf)), cap, NULL);
```

**`usize hashmap_buffer_size(usize capacity)`**
Returns the byte count needed for a flat buffer holding `capacity` slots. Use this to size your buffer before `hashmap_init()`. Returns 0 on overflow.

**`result_bool_Error hashmap_init(hashmap* map, bytes_t buf, usize capacity, void* ctx)`**
Initializes map over caller-owned buffer. Zeroes all slots. Capacity must be a power of 2. `ctx` is forwarded to hash/eq functions (may be NULL). O(capacity).
- `Err(ERR_INVALID_ARG)` — map/buf.ptr is NULL, capacity is 0, or capacity is not a power of 2
- `Err(ERR_BUFFER_TOO_SMALL)` — buf.len < hashmap_buffer_size(capacity)

#### Queries
```c
hashmap_len(&map)                // usize — number of stored entries
hashmap_capacity(&map)           // usize — total slot count
hashmap_is_empty(&map)           // bool  — true if len == 0
hashmap_load_factor(&map)        // f64   — len / capacity in [0.0, 1.0]
hashmap_contains_key(&map, &key) // bool
```

#### Mutation

**`result_bool_Error hashmap_insert(hashmap* map, const K* key, const V* val)`**
Inserts or overwrites a key-value pair. Keys and values are copied into slots — originals need not outlive the call.
- `Ok(true)` — new key inserted
- `Ok(false)` — key existed, value overwritten
- `Err(ERR_CAPACITY_EXCEEDED)` — load ≥ 75%, no insertion performed
- `Err(ERR_INVALID_ARG)` — any pointer is NULL

**`result_V_Error hashmap_remove(hashmap* map, const K* key)`**
Removes a key and returns its old value. Uses backward shift deletion — no tombstones.
- `Ok(old_value)` — key found and removed
- `Err(ERR_NOT_FOUND)` — key does not exist
- `Err(ERR_INVALID_ARG)` — any pointer is NULL

**`void hashmap_clear(hashmap* map)`**
Removes all entries. Buffer and capacity unchanged. O(capacity).

#### Lookup

**`option_V hashmap_get(const hashmap* map, const K* key)`**
Returns `Some(value)` (a copy) if found, `None` if not. O(1) expected — Robin Hood early termination on failed lookups.

**`V* hashmap_get_or_null(hashmap* map, const K* key)`**
Returns a direct pointer into the slot (borrowed). Valid until next insert or remove. NULL if not found. Avoids copy cost for large value types.

#### Iteration
```c
usize iter = 0;
const u64* k; const int* v;
while (hashmap_iter_next(&map, &iter, &k, &v)) {
    printf("%llu => %d\n", (unsigned long long)*k, *v);
}
```

Initialize `iter = 0` before the first call. Do not mutate the map during iteration. O(1) per call, O(capacity) to exhaust.

#### Multiple Instantiations

`hashmap.h` has no include guard and can be included multiple times with different macro configs to generate distinct types in the same translation unit:
```c
// First: u64 → int
#define HASHMAP_KEY_TYPE  u64
#define HASHMAP_VAL_TYPE  int
#define HASHMAP_HASH_FN   hash_u64
#define HASHMAP_EQ_FN     eq_u64
#define HASHMAP_TYPE_NAME map_u64_int
#define HASHMAP_FN(name)  map_u64_int_##name
#include "data/hashmap/hashmap.h"
#undef HASHMAP_KEY_TYPE
#undef HASHMAP_VAL_TYPE
// ... undef all ...

// Second: str_t → u32
#define HASHMAP_KEY_TYPE  str_t
#define HASHMAP_VAL_TYPE  u32
#define HASHMAP_HASH_FN   hash_str
#define HASHMAP_EQ_FN     eq_str
#define HASHMAP_TYPE_NAME map_str_u32
#define HASHMAP_FN(name)  map_str_u32_##name
#include "data/hashmap/hashmap.h"
```

#### Built-in Hash Recipes
```c
// Integer keys — Fibonacci hashing
static u64 hash_u64(const u64* k, void* ctx) {
    (void)ctx;
    u64 h = *k * 11400714819323198485ULL;
    return h ? h : 1;
}

// String keys — FNV-1a over str_t
static u64 hash_str(const str_t* s, void* ctx) {
    (void)ctx;
    u64 h = 14695981039346656037ULL;
    for (usize i = 0; i < s->len; i++) {
        h ^= (u8)s->ptr[i];
        h *= 1099511628211ULL;
    }
    return h ? h : 1;
}

// Pointer identity
static u64 hash_ptr(const void** p, void* ctx) {
    (void)ctx;
    u64 h = (u64)(uintptr_t)*p * 11400714819323198485ULL;
    return h ? h : 1;
}
```

> **Known Limitations:**
> - The hashmap never rehashes — capacity is fixed at init. For auto-growth, you should create your own `dynhashmap.h` wrapper.
> - Hash function must return non-zero; the impl normalizes 0 to 1 but callers should not rely on this.
> - `hashmap_get_or_null()` pointer is invalidated by any insert or remove.
> - Not thread-safe — concurrent access requires external synchronization per map.
> - Optional extensions (`hashmap_fmt.h`, `hashmap_range.h`) are not included by default.

### `hashmap_decl.h`
> Declaration macros for the Canon-C hashmap — for separate compilation. Use in `.h` files to declare hashmap types and function signatures without generating implementations. Pair with `hashmap_defn.h` in exactly one `.c` file.

#### Workflow
```c
// my_map.h — declare only
#define HASHMAP_KEY_TYPE  u64
#define HASHMAP_VAL_TYPE  int
#define HASHMAP_HASH_FN   hash_u64
#define HASHMAP_EQ_FN     eq_u64
#define HASHMAP_TYPE_NAME map_u64_int
#define HASHMAP_FN(name)  map_u64_int_##name
#include "data/hashmap/hashmap_decl.h"

// my_map.c — define once
#define HASHMAP_LINKAGE  /* external linkage */
#include "data/hashmap/hashmap_defn.h"

// main.c — just include the header
#include "my_map.h"
map_u64_int m;
```

#### What it generates

- Slot typedef: `hashmap_slot` (or `HASHMAP_SLOT_NAME`)
- Struct typedef: `hashmap` (or `HASHMAP_TYPE_NAME`)
- `extern` declarations for all functions:

| Category | Declared functions |
|---|---|
| Sizing | `buffer_size` |
| Lifecycle | `init`, `clear` |
| Queries | `len`, `capacity`, `is_empty`, `load_factor`, `contains_key` |
| Mutation | `insert`, `remove` |
| Lookup | `get`, `get_or_null` |
| Iteration | `iter_next` |

#### Option / Result instantiations

`hashmap_decl.h` also instantiates the following types needed by the API:
```c
CANON_OPTION(VAL_TYPE)
CANON_RESULT(bool, Error)
CANON_RESULT(VAL_TYPE, Error)
```

> **Known Limitations:**
> - Linkage is always `extern` in the declaration path — not configurable here.
> - Must be matched by exactly one `hashmap_defn.h` include in a `.c` file — multiple definitions cause linker errors.
> - Do NOT use `hashmap_decl.h` if you are using the header-only path via `hashmap.h`.
> - All `HASHMAP_*` macros must be defined before including — missing any required macro is a compile error.

### `hashmap_defn.h`
> Definition macros for the Canon-C hashmap — generates actual function implementations with external linkage. Include in exactly one `.c` file for separate compilation. All other translation units should use `hashmap_decl.h` for declarations, or `hashmap.h` for header-only usage.

#### Workflow
```c
// my_map.c — define once

// Step 1: provide hash and equality functions
static u64 hash_u64(const u64* k, void* ctx) {
    (void)ctx;
    u64 h = *k * 11400714819323198485ULL;
    return h == 0 ? 1 : h;
}
static bool eq_u64(const u64* a, const u64* b, void* ctx) {
    (void)ctx; return *a == *b;
}

// Step 2: configure
#define HASHMAP_KEY_TYPE  u64
#define HASHMAP_VAL_TYPE  int
#define HASHMAP_HASH_FN   hash_u64
#define HASHMAP_EQ_FN     eq_u64
#define HASHMAP_TYPE_NAME map_u64_int
#define HASHMAP_FN(name)  map_u64_int_##name

// Step 3: generate definitions
#include "data/hashmap/hashmap_defn.h"
```

#### What it generates

Pulls in `hashmap_impl.h` with `HASHMAP_LINKAGE` set to empty (external linkage). Generates definitions for all functions:

| Category | Generated functions |
|---|---|
| Sizing | `buffer_size` |
| Lifecycle | `init`, `clear` |
| Queries | `len`, `capacity`, `is_empty`, `load_factor`, `contains_key` |
| Mutation | `insert`, `remove` |
| Lookup | `get`, `get_or_null` |
| Iteration | `iter_next` |

#### vs. `hashmap.h`

| | `hashmap.h` | `hashmap_defn.h` |
|---|---|---|
| Linkage | `static inline` | external |
| Include in | any `.h` or `.c` | exactly one `.c` |
| Build style | header-only | separate compilation |
| Binary size | copy per TU | one shared definition |

> **Known Limitations:**
> - Must be included in exactly one translation unit — multiple inclusions with the same type configuration cause linker errors.
> - All `HASHMAP_*` macros must be defined before including.
> - `hashmap_impl.h` guards against direct inclusion — always go through `hashmap_defn.h` or `hashmap.h`.
> - Do not mix `hashmap.h` (static inline) and `hashmap_defn.h` (extern) for the same type configuration in the same program.


### `hashmap_impl.h`
> Pure implementation logic for the Canon-C hashmap — no naming assumptions, no linkage decisions. Every identifier comes from `hashmap_mangle.h`. Do not include this file directly. Use `hashmap.h` for header-only usage or `hashmap_decl.h` + `hashmap_defn.h` for separate compilation.

#### Algorithm: Robin Hood Open Addressing

All elements live in a flat caller-owned buffer — no linked lists, no per-element allocation, no pointer chasing. During insertion, if the incoming element has probed farther from its home slot than the resident element, they swap. This keeps probe sequence lengths (PSL) short and near-equal across all elements.

Deletion uses backward shift (no tombstones): after removing a slot, subsequent elements with PSL > 0 are shifted back one position, restoring the invariant without degrading future lookups.

Lookup early-exit: if a probe reaches a slot whose PSL is less than the current probe distance, the key cannot exist — Robin Hood would have displaced it. Failed lookups terminate faster than vanilla linear probing.

#### Memory Layout

The caller provides a flat `u8` buffer. `hashmap_init()` partitions it into an array of slot structs aligned to `alignof(hashmap_slot)`.

Slot layout per element:
```c
typedef struct {
    u64       hash;     // cached full hash (0 = unoccupied sentinel)
    u32       psl;      // probe sequence length (distance from home slot)
    bool      occupied; // explicit occupancy flag
    hm_key_t  key;      // stored key (copy)
    hm_val_t  value;    // stored value (copy)
} hashmap_slot;
```

#### Available Implementation Macros

| Macro / Function | Behavior |
|---|---|
| `_hm_home(hash, capacity)` | Computes home slot index via bitmask (power-of-2 only) |
| `_hm_wrap(index, capacity)` | Wraps slot index around capacity boundary |
| `_hm_normalize_hash(h)` | Returns `h == 0 ? 1 : h` — enforces non-zero hash invariant |
| `_HM_BUFFER_SIZE(capacity)` | `capacity * sizeof(slot)`, overflow-safe via `checked_mul` |
| `_HM_INIT(map, buf, cap, ctx)` | Zeroes all slots, validates power-of-2 capacity and buffer size |
| `_HM_CLEAR(map)` | `memset` all slots to zero, resets `len` |
| `_HM_LEN(map)` | Returns `map->len` |
| `_HM_CAPACITY(map)` | Returns `map->capacity` |
| `_HM_IS_EMPTY(map)` | Returns `map->len == 0` |
| `_HM_LOAD_FACTOR(map)` | Returns `(f64)len / (f64)capacity` |
| `_HM_INSERT(map, key, val)` | Robin Hood insertion with 75% load cap |
| `_HM_GET(map, key)` | Robin Hood lookup — returns `option_VAL` |
| `_HM_GET_OR_NULL(map, key)` | Robin Hood lookup — returns borrowed `VAL*` or NULL |
| `_HM_CONTAINS_KEY(map, key)` | Delegates to `_HM_GET`, checks `is_some` |
| `_HM_REMOVE(map, key)` | Finds slot then backward-shifts to close gap |
| `_HM_ITER_NEXT(map, iter, k, v)` | Advances `*iter` to next occupied slot |

#### Required User Definitions

All must be `#defined` before `hashmap_impl.h` is reached (via `hashmap.h` or `hashmap_defn.h`):
```c
HASHMAP_KEY_TYPE   // key type (e.g. u64, str_t, MyKey)
HASHMAP_VAL_TYPE   // value type (e.g. int, void*, Record)
HASHMAP_HASH_FN    // u64 fn(const K* key, void* ctx)
HASHMAP_EQ_FN      // bool fn(const K* a, const K* b, void* ctx)
HASHMAP_LINKAGE    // static inline (hashmap.h) or empty (hashmap_defn.h)
```

#### Internal Type Aliases

`hashmap_impl.h` defines `hm_key_t` and `hm_val_t` as aliases for `HASHMAP_KEY_TYPE` and `HASHMAP_VAL_TYPE` for readability inside the implementation. Both are `#undef`-ed at the end of the file to avoid polluting downstream includes.

#### Dependencies
```
core/primitives/types.h
core/primitives/contract.h
core/primitives/bits.h
core/primitives/checked.h
core/slice.h
core/ownership.h
semantics/option/option.h
semantics/result/result.h
semantics/error.h
hashmap_mangle.h
<string.h>  (memset, memcpy)
```

> **Known Limitations:**
> - Do not include directly — `HASHMAP_LINKAGE` guard will produce a compile error.
> - Internal helpers (`_hm_home`, `_hm_wrap`, `_hm_normalize_hash`) are `static inline` regardless of `HASHMAP_LINKAGE` — they are never exposed in the public API.
> - `hm_key_t` / `hm_val_t` aliases are cleaned up with `#undef` at end of file — do not rely on them outside this file.
> - `_HM_CONTAINS_KEY` has a typo in the source (`_hm_key_t` instead of `hm_key_t`) — may cause a compile error on some compilers; use `hashmap_get_or_null` as a workaround if needed.

### `hashmap_mangle.h`
> Name mangling conventions for the Canon-C hashmap. Single source of truth for all generated type and function names. Override any macro before including to rename the entire API globally.

#### Default Naming Scheme

For the default configuration, the following names are produced:

| What | Default name |
|---|---|
| Type | `hashmap` |
| Slot type | `hashmap_slot` |
| `buffer_size` | `hashmap_buffer_size` |
| `init` | `hashmap_init` |
| `clear` | `hashmap_clear` |
| `len` | `hashmap_len` |
| `capacity` | `hashmap_capacity` |
| `is_empty` | `hashmap_is_empty` |
| `load_factor` | `hashmap_load_factor` |
| `insert` | `hashmap_insert` |
| `remove` | `hashmap_remove` |
| `get` | `hashmap_get` |
| `get_or_null` | `hashmap_get_or_null` |
| `contains_key` | `hashmap_contains_key` |
| `iter_next` | `hashmap_iter_next` |

#### Customization

Define overrides **before** including any hashmap header:
```c
// Project namespace prefix
#define HASHMAP_TYPE_NAME   myproject_map
#define HASHMAP_SLOT_NAME   myproject_map_slot
#define HASHMAP_FN(name)    myproject_map_##name
#include "data/hashmap/hashmap.h"

myproject_map m;
myproject_map_insert(&m, &key, &val);

// Per-instantiation rename for multiple types in one TU
#define HASHMAP_TYPE_NAME   map_u64_int
#define HASHMAP_SLOT_NAME   map_u64_int_slot
#define HASHMAP_FN(name)    map_u64_int_##name
#include "data/hashmap/hashmap.h"
#undef HASHMAP_TYPE_NAME
#undef HASHMAP_SLOT_NAME
#undef HASHMAP_FN

#define HASHMAP_TYPE_NAME   map_str_u32
#define HASHMAP_SLOT_NAME   map_str_u32_slot
#define HASHMAP_FN(name)    map_str_u32_##name
#include "data/hashmap/hashmap.h"
```

#### Derived Internal Names

These are built from the macros above and should not be overridden directly:

| Internal macro | Expands to |
|---|---|
| `_HM_INIT` | `HASHMAP_FN(init)` |
| `_HM_CLEAR` | `HASHMAP_FN(clear)` |
| `_HM_LEN` | `HASHMAP_FN(len)` |
| `_HM_CAPACITY` | `HASHMAP_FN(capacity)` |
| `_HM_IS_EMPTY` | `HASHMAP_FN(is_empty)` |
| `_HM_LOAD_FACTOR` | `HASHMAP_FN(load_factor)` |
| `_HM_INSERT` | `HASHMAP_FN(insert)` |
| `_HM_GET` | `HASHMAP_FN(get)` |
| `_HM_GET_OR_NULL` | `HASHMAP_FN(get_or_null)` |
| `_HM_CONTAINS_KEY` | `HASHMAP_FN(contains_key)` |
| `_HM_REMOVE` | `HASHMAP_FN(remove)` |
| `_HM_ITER_NEXT` | `HASHMAP_FN(iter_next)` |
| `_HM_BUFFER_SIZE` | `HASHMAP_FN(buffer_size)` |

> **Known Limitations:**
> - All macros are guarded with `#ifndef` — overrides must be defined before the first include of any hashmap header in that translation unit.
> - `HASHMAP_TYPE_NAME` and `HASHMAP_SLOT_NAME` are independent — overriding one does not affect the other.
> - Overrides apply to all instantiations after them, not selectively per type — use `#undef` between instantiations when generating multiple distinct types.
> - `hashmap_mangle.h` has no includes of its own — it is safe to include from any layer.

### `hashmap_fmt.h`
> Optional formatting extension for the Canon-C hashmap. Provides `hashmap_to_stringbuf()` — a diagnostic helper that writes a human-readable representation of the hashmap into a caller-provided `StringBuf`. Intended for debugging and logging, not serialization. Not included by `hashmap.h` by default.

#### Setup

Requires `data/stringbuf.h`. Before including, define two formatter functions and include `hashmap.h` first:
```c
#include "data/stringbuf.h"

static bool fmt_u64_key(StringBuf* sb, const u64* k) {
    char tmp[24];
    snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)*k);
    return stringbuf_push_cstr(sb, tmp);
}

static bool fmt_int_val(StringBuf* sb, const int* v) {
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", *v);
    return stringbuf_push_cstr(sb, tmp);
}

#define HASHMAP_KEY_TYPE   u64
#define HASHMAP_VAL_TYPE   int
#define HASHMAP_HASH_FN    hash_u64
#define HASHMAP_EQ_FN      eq_u64
#define HASHMAP_FMT_KEY_FN fmt_u64_key
#define HASHMAP_FMT_VAL_FN fmt_int_val
#include "data/hashmap/hashmap.h"
#include "data/hashmap/hashmap_fmt.h"
```

#### Required User Definitions

| Macro | Signature | Description |
|---|---|---|
| `HASHMAP_FMT_KEY_FN` | `bool fn(StringBuf* sb, const K* key)` | Formats a key into sb |
| `HASHMAP_FMT_VAL_FN` | `bool fn(StringBuf* sb, const V* val)` | Formats a value into sb |

Both must return `true` on success, `false` if the buffer is full. Missing either is a compile error.

#### Usage

**`bool hashmap_to_stringbuf(StringBuf* sb, const hashmap* map)`**
Writes a human-readable representation of the hashmap into `sb`. Output is appended to whatever is already in `sb` — it is NOT cleared first. If `sb` fills before all entries are written, returns `false` and `sb` contains a partial result. No entries are lost from the map itself.
```c
u8 strbuf_mem[4096];
StringBuf sb = stringbuf_from(bytes_from(strbuf_mem, sizeof(strbuf_mem)));
bool ok = hashmap_to_stringbuf(&sb, &my_map);
if (ok) printf("%.*s\n", (int)sb.len, sb.ptr);
```

#### Output Format
```
hashmap { len=3, cap=8, load=0.375 } {
  [key0] => value0
  [key1] => value1
  [key2] => value2
}
```

#### Performance

- Time: O(capacity) — iterates all slots regardless of occupancy
- Space: O(1) — no allocation

> **Known Limitations:**
> - Not included by `hashmap.h` — must be included explicitly after `hashmap.h`.
> - `hashmap.h` must be included before `hashmap_fmt.h` in every translation unit that uses it.
> - Output is appended to `sb` — clear it manually before calling if a fresh output is needed.
> - Partial output on `StringBuf` overflow — no way to resume from where formatting stopped.
> - Depends on `data/stringbuf.h` — not suitable for targets without that layer available.
> - `HASHMAP_FMT_KEY_FN` and `HASHMAP_FMT_VAL_FN` must be defined before including — missing either is a compile error.

### `hashmap_range.h`
> Optional bulk collection extension for the Canon-C hashmap. Provides `hashmap_collect_keys()`, `hashmap_collect_values()`, and the `HASHMAP_FOR_EACH` iteration macro. Useful when you need a snapshot of keys or values for sorting, indexed access, or passing to algo/ functions. Not included by `hashmap.h` by default.

#### Setup

Requires `data/vec/vec.h`. The key and value vec types must be instantiated with `DEFINE_VEC` before including this header:
```c
#include "data/vec/vec.h"
DEFINE_VEC(static inline, u64)
DEFINE_VEC(static inline, int)

#define HASHMAP_KEY_TYPE  u64
#define HASHMAP_VAL_TYPE  int
#define HASHMAP_HASH_FN   hash_u64
#define HASHMAP_EQ_FN     eq_u64
#include "data/hashmap/hashmap.h"
#include "data/hashmap/hashmap_range.h"
```

For pointer types, typedef first:
```c
typedef const char* cstr;
DEFINE_VEC(static inline, cstr)
#define HASHMAP_KEY_TYPE u64
#define HASHMAP_VAL_TYPE cstr
```

#### `hashmap_collect_keys`

**`result_bool_Error hashmap_collect_keys(const hashmap* map, canon_vec_K* out)`**
Iterates all occupied slots and appends each key into `out` using `canon_vec_K_push()`. The vec is NOT cleared before insertion — keys are appended to whatever is already in it. Key ordering is unspecified.

- `Ok(true)` — all keys pushed successfully
- `Err(ERR_CAPACITY_EXCEEDED)` — vec filled before all keys were pushed
- `Err(ERR_INVALID_ARG)` — any pointer is NULL
```c
u64 key_buf[64];
canon_vec_u64 keys = canon_vec_u64_init(key_buf, 64);
result_bool_Error r = hashmap_collect_keys(&my_map, &keys);
```

#### `hashmap_collect_values`

**`result_bool_Error hashmap_collect_values(const hashmap* map, canon_vec_V* out)`**
Iterates all occupied slots and appends each value into `out` using `canon_vec_V_push()`. Value ordering corresponds to the order returned by `iter_next`. The vec is NOT cleared before insertion.

- `Ok(true)` — all values pushed successfully
- `Err(ERR_CAPACITY_EXCEEDED)` — vec filled before all values were pushed
- `Err(ERR_INVALID_ARG)` — any pointer is NULL
```c
int val_buf[64];
canon_vec_int vals = canon_vec_int_init(val_buf, 64);
result_bool_Error r = hashmap_collect_values(&my_map, &vals);
```

#### Key-Value Correspondence

If you need index correspondence between a keys vec and a values vec, call `collect_keys` then `collect_values` without mutating the map between calls. Both use `iter_next` which visits slots in the same deterministic order.

#### `HASHMAP_FOR_EACH`

Ergonomic for-loop macro for iterating all key-value pairs. No vec or allocation needed — direct forward iteration over the map.
```c
const u64* k;
const int* v;
HASHMAP_FOR_EACH(&my_map, k, v) {
    printf("%llu => %d\n", (unsigned long long)*k, *v);
}
```

Nested loops are supported — uses `__LINE__` to generate unique iterator variable names:
```c
const u64* k1; const int* v1;
const u64* k2; const int* v2;
HASHMAP_FOR_EACH(&map_a, k1, v1) {
    HASHMAP_FOR_EACH(&map_b, k2, v2) { ... }
}
```

`key_var` and `val_var` must be declared before the macro. `break` and `continue` work normally. The map must not be mutated during iteration.

#### Performance

| Operation | Time | Space |
|---|---|---|
| `collect_keys` | O(capacity) | O(1) — output goes into caller's vec |
| `collect_values` | O(capacity) | O(1) — output goes into caller's vec |
| `HASHMAP_FOR_EACH` per iteration | O(1) | O(1) |

> **Known Limitations:**
> - Not included by `hashmap.h` — must be included explicitly after `hashmap.h`.
> - `DEFINE_VEC` must be called for both key and value types before including this header — missing instantiations cause compile errors.
> - Push function names are derived directly from `HASHMAP_KEY_TYPE` and `HASHMAP_VAL_TYPE` via the `canon_vec_##TYPE##_push` naming convention — types must be simple identifiers, not raw pointer types.
> - `collect_keys` and `collect_values` append to existing vec contents — clear the vec manually before calling if a fresh snapshot is needed.
> - Key and value ordering is unspecified — sort with `algo/sort.h` if ordered output is needed.
> - `HASHMAP_FOR_EACH` uses `__LINE__` for unique iterator names — two invocations on the same source line (e.g. via another macro) will collide. Expand manually if needed.
> - The map must not be mutated during `HASHMAP_FOR_EACH` — inserts or removes may displace slots and corrupt the iterator position.

### `vec.h`
> Bounded typed vector with explicit caller-owned buffer — header-only entry point. Fixed capacity, no automatic growth. All bounds-checked operations return `Result<bool, Error>` or `Option<T>`. Three allocation strategies: caller buffer, heap, or arena.

#### Setup
```c
#include "data/vec/vec.h"

DEFINE_VEC(static inline, int)

// Pointer types — typedef first:
typedef void* voidptr;
DEFINE_VEC(static inline, voidptr)
```

Must be used at file/global scope. `option_##type` must be instantiated via `CANON_OPTION(type)` before calling `DEFINE_VEC`.

#### Constructors

**`canon_vec_T canon_vec_T_init(T* buffer, usize capacity)`**
Wraps a caller-owned buffer. No allocation. Returns zero struct if capacity exceeds `CANON_VEC_MAX_CAPACITY / sizeof(T)` or buffer is NULL with capacity > 0. O(1).
```c
int buf[64];
canon_vec_int v = canon_vec_int_init(buf, 64);
```

**`canon_vec_T canon_vec_T_empty(void)`**
Returns a zero-initialized vec with no buffer. Safe starting point before a buffer is available. O(1).

**`canon_vec_T canon_vec_T_alloc(usize capacity)`**
Heap-allocates a buffer of `capacity` elements. Returns `_empty()` on OOM, overflow, or capacity == 0. Must call `_free()` when done. O(1).
```c
canon_vec_int h = canon_vec_int_alloc(128);
canon_vec_int_push(&h, 99);
canon_vec_int_free(&h);
```

**`canon_vec_T canon_vec_T_arena_alloc(Arena* arena, usize capacity)`**
Allocates buffer from arena. Returns `_empty()` on failure or arena == NULL. No `_free()` needed — arena owns the memory. O(1).
```c
canon_vec_int a = canon_vec_int_arena_alloc(&arena, 32);
// no free needed — arena_reset() handles cleanup
```

**`void canon_vec_T_free(canon_vec_T* v)`**
Frees a heap-allocated buffer. NULL-safe. Resets all fields to zero. Do NOT call on stack or arena-backed vecs.

#### Queries
```c
canon_vec_int_len(&v)        // usize — current element count
canon_vec_int_capacity(&v)   // usize — maximum elements
canon_vec_int_remaining(&v)  // usize — free slots (capacity - len)
canon_vec_int_is_empty(&v)   // bool
canon_vec_int_is_full(&v)    // bool
```

All query functions are NULL-safe — return 0 or true if v == NULL.

#### Element Access

**`bool canon_vec_T_get(const canon_vec_T* v, usize i, T* out)`**
Bounds-checked copy into `*out`. Returns false if v == NULL, out == NULL, or i >= len. O(1).

**`option_T canon_vec_T_get_option(const canon_vec_T* v, usize i)`**
Returns `Some(value)` or `None` — no out param. O(1).

**`T canon_vec_T_get_unchecked(const canon_vec_T* v, usize i)`**
No bounds check — debug-only `ensure_msg`. UB in release if violated.

**`T* canon_vec_T_at(const canon_vec_T* v, usize i)`**
Pointer to element for in-place mutation. Debug-checked. Invalidated by any modification.

**`bool canon_vec_T_set(canon_vec_T* v, usize i, T val)`**
Bounds-checked write. Returns false if OOB or v == NULL. O(1).

**`T* canon_vec_T_first(const canon_vec_T* v)`** / **`T* canon_vec_T_last(const canon_vec_T* v)`**
Pointer to first/last element. NULL if empty or v == NULL.

**`T* canon_vec_T_data(const canon_vec_T* v)`**
Raw buffer pointer. NULL if v == NULL.

#### Modification

**`result_bool_Error canon_vec_T_push(canon_vec_T* v, T item)`**
Appends one element. O(1).
- `Ok(true)` — element appended
- `Err(ERR_INVALID_ARG)` — v or v->items is NULL
- `Err(ERR_CAPACITY_EXCEEDED)` — vec is full

**`bool canon_vec_T_try_push(canon_vec_T* v, T item)`**
Same but returns plain bool — no `Result` overhead. Returns false on any failure.

**`void canon_vec_T_push_unchecked(canon_vec_T* v, T item)`**
No checking. Debug-only `ensure_msg`. UB in release if full.

**`result_bool_Error canon_vec_T_pop(canon_vec_T* v, T* out)`**
Removes and returns last element. O(1).
- `Ok(true)` — element removed into `*out`
- `Err(ERR_INVALID_ARG)` — v, out, or v->items is NULL
- `Err(ERR_INVALID_STATE)` — vec is empty

**`option_T canon_vec_T_pop_option(canon_vec_T* v)`**
Same but returns `Option<T>` — no out param. `None` if empty.

**`void canon_vec_T_clear(canon_vec_T* v)`**
Resets len to 0. Does NOT zero memory. NULL-safe. O(1).

**`result_bool_Error canon_vec_T_insert(canon_vec_T* v, usize i, T item)`**
Inserts at index `i`, shifts elements right. O(n).
- `Err(ERR_OUT_OF_RANGE)` — i > len
- `Err(ERR_CAPACITY_EXCEEDED)` — vec is full

**`result_bool_Error canon_vec_T_remove(canon_vec_T* v, usize i, T* out)`**
Removes at index `i`, shifts elements left. O(n).
- `Err(ERR_INVALID_STATE)` — vec is empty
- `Err(ERR_OUT_OF_RANGE)` — i >= len

**`option_T canon_vec_T_remove_option(canon_vec_T* v, usize i)`**
Same but returns `Option<T>`.

#### Bulk Operations

**`result_bool_Error canon_vec_T_append_array(canon_vec_T* v, const T* src, usize count)`**
Appends `count` elements from array. Overflow-checked via `checked_add`. O(count).
- `Err(ERR_OVERFLOW)` — v->len + count overflows usize
- `Err(ERR_CAPACITY_EXCEEDED)` — insufficient space

**`result_bool_Error canon_vec_T_extend(canon_vec_T* v, const T* src, usize count)`**
Alias for `append_array` — identical semantics.

**`void canon_vec_T_fill(canon_vec_T* v, T value, usize count)`**
Appends `value` repeated `min(count, remaining)` times. Silently stops at capacity — does not error on overflow.

**`void canon_vec_T_swap(canon_vec_T* a, canon_vec_T* b)`**
Swaps two vecs in O(1) — struct copy on stack. Both must be non-NULL (`require_msg`).

#### Iterator
```c
canon_vec_int_iter it = canon_vec_int_iter_init(&v);
int val;
while (canon_vec_int_iter_next(&it, &val)) {
    // use val
}
```
Iterator is invalidated by any modification to the vec. O(1) per step.

#### Vec-Internal Slice

**`canon_vec_T_slice canon_vec_T_slice_init(canon_vec_T* v, usize start, usize end)`**
Zero-copy sub-view of `[start, end)`. Returns empty slice on invalid range. Invalidated by any modification. O(1).

**`T* canon_vec_T_slice_get(const canon_vec_T_slice* s, usize i)`**
Pointer to slice element. Debug-checked only. O(1).

#### `slice.h` Integration (`DEFINE_VEC_SLICE`)

Call after both `DEFINE_VEC(linkage, type)` and `DEFINE_SLICE(type)` from `core/slice.h`:
```c
DEFINE_SLICE(int)
DEFINE_VEC_SLICE(int)

slice_int sv = canon_vec_int_as_slice(&v);       // typed view of [data, data+len)
slice_int sf = canon_vec_int_as_slice_full(&v);  // typed view of [data, data+cap)
bytes_t   bv = canon_vec_int_as_bytes(&v);       // raw byte view of used elements
```
All views are non-owning and invalidated by any modification to the vec.

#### Struct Layout
```c
typedef struct {
    T*    items;    // caller-owned element buffer
    usize len;      // current element count (0 <= len <= capacity)
    usize capacity; // maximum elements buffer can hold
} canon_vec_T;

typedef struct {
    canon_vec_T* vec;  // pointer to parent vec
    usize        index; // current position
} canon_vec_T_iter;

typedef struct {
    T*    items; // pointer into parent vec's buffer (not owned)
    usize len;   // number of elements in slice
} canon_vec_T_slice;

// sizeof(canon_vec_T) = sizeof(T*) + 2*sizeof(usize)
// element overhead: 0 bytes beyond sizeof(T) per element
```

#### Performance

| Operation | Time | Notes |
|---|---|---|
| `push` / `pop` | O(1) | No allocation |
| `insert` / `remove` | O(n) | Shifts elements |
| `append_array` / `extend` | O(k) | Copies k elements |
| `get` / `set` / `at` | O(1) | |
| `iter_next` | O(1) per step | |
| `as_slice` / `as_bytes` | O(1) | Zero-copy view |
| `alloc` | O(1) | Single malloc |
| `arena_alloc` | O(1) | Arena bump |

> **Known Limitations:**
> - Fixed capacity — no automatic growth. For auto-growing vectors use `data/convenience/dynvec.h`.
> - `option_##type` must be instantiated via `CANON_OPTION(type)` before calling `DEFINE_VEC` — missing this produces cryptic compile errors.
> - `get_unchecked`, `at`, `push_unchecked`, and `slice_get` have no release-build bounds protection — verify bounds yourself before calling.
> - `clear()` does not zero buffer contents — stale data remains readable until overwritten. Zero manually first for sensitive data.
> - `fill()` silently stops at capacity rather than returning an error — check `remaining()` first if strict filling is required.
> - Element type must be trivially copyable — `mem_copy` / `mem_move` used internally for insert, remove, and bulk operations.
> - `_free()` must NOT be called on stack or arena-backed vecs — only on vecs created via `_alloc()`.
> - Slices, byte views, and iterators are all invalidated by any modification to the vec.
> - `_alloc()` and `_arena_alloc()` both return `_empty()` silently on failure — check `capacity()` afterwards if pre-allocation is required.
> - Not thread-safe — concurrent modifications require external synchronization. Concurrent reads on the same instance are safe.

### `vec_decl.h`
> Declaration macros for typed Canon-C vectors — for separate compilation. Use in `.h` files to declare a typed vector struct and all function signatures without generating implementations. Pair with `DEFINE_VEC()` in `vec_defn.h` in exactly one `.c` file.

#### Workflow
```c
// In tasks.h — declare only
#include "data/vec/vec_decl.h"
DECLARE_VEC(int)
DECLARE_VEC(Task)

// In tasks.c — define once
#include "data/vec/vec_defn.h"
DEFINE_VEC(static, int)
DEFINE_VEC(static, Task)

// In main.c — just include the header
#include "tasks.h"
canon_vec_int v;
```

Pointer types:
```c
typedef const char* cstr;
DECLARE_VEC(cstr)
```

#### What `DECLARE_VEC(type)` Emits

Struct typedefs: `canon_vec_T`, `canon_vec_T_iter`, `canon_vec_T_slice`

`extern` declarations for all functions:

| Category | Declared functions |
|---|---|
| Constructors | `_init`, `_empty`, `_alloc`, `_arena_alloc`, `_free` |
| Queries | `_len`, `_capacity`, `_remaining`, `_is_empty`, `_is_full` |
| Element access | `_get`, `_get_option`, `_get_unchecked`, `_at`, `_set`, `_first`, `_last`, `_data` |
| Modification | `_push`, `_try_push`, `_push_unchecked`, `_pop`, `_pop_option`, `_clear`, `_insert`, `_remove`, `_remove_option` |
| Bulk | `_append_array`, `_extend`, `_fill`, `_swap` |
| Iterator | `_iter_init`, `_iter_next` |
| Slice | `_slice_init`, `_slice_get` |

> **Known Limitations:**
> - Linkage is always `extern` in the declaration path — not configurable here.
> - Must be matched by exactly one `DEFINE_VEC(linkage, type)` in a `.c` file — multiple definitions cause linker errors.
> - Do NOT use `DECLARE_VEC` if you are using the header-only path via `vec.h`.
> - `option_##type` must be instantiated via `CANON_OPTION(type)` before `DECLARE_VEC` — not enforced automatically, missing it produces cryptic compile errors.
> - `DEFINE_VEC_SLICE` from `vec.h` is not covered by `DECLARE_VEC` — slice.h integration functions (`as_slice`, `as_slice_full`, `as_bytes`) must be declared separately if needed.

### `vec_defn.h`
> Definition macros for typed Canon-C vectors — generates all struct typedefs and function implementations. Use `DEFINE_VEC(linkage, type)` in `.c` files for separate compilation, or in any file with `static inline` for header-only usage.

#### Workflow

Header-only:
```c
#include "data/vec/vec_defn.h"
DEFINE_VEC(static inline, int)

int buf[64];
canon_vec_int v = canon_vec_int_init(buf, 64);
canon_vec_int_push(&v, 42);
```

Separate compilation:
```c
// In tasks.h:
#include "data/vec/vec_decl.h"
DECLARE_VEC(Task)

// In tasks.c:
#include "data/vec/vec_defn.h"
DEFINE_VEC(, Task)   // empty linkage = external
```

Pointer types (typedef first):
```c
typedef void* voidptr;
DEFINE_VEC(static inline, voidptr)
// canon_vec_voidptr v = canon_vec_voidptr_init(buf, 64);
```

#### `DEFINE_VEC(linkage, type)` — Generated Types

| Type | Description |
|---|---|
| `canon_vec_T` | The vector struct (`items`, `len`, `capacity`) |
| `canon_vec_T_iter` | Forward iterator (`vec*` + `index`) |
| `canon_vec_T_slice` | Zero-copy sub-view (`items*` + `len`) |

#### `DEFINE_VEC(linkage, type)` — Generated Functions

| Category | Functions |
|---|---|
| Constructors | `_init`, `_empty`, `_alloc`, `_arena_alloc`, `_free` |
| Queries | `_len`, `_capacity`, `_remaining`, `_is_empty`, `_is_full` |
| Element access | `_get`, `_get_option`, `_get_unchecked`, `_at`, `_set`, `_first`, `_last`, `_data` |
| Modification | `_push`, `_try_push`, `_push_unchecked`, `_pop`, `_pop_option`, `_clear`, `_insert`, `_remove`, `_remove_option` |
| Bulk | `_append_array`, `_extend`, `_fill`, `_swap` |
| Iterator | `_iter_init`, `_iter_next` |
| Slice | `_slice_init`, `_slice_get` |

#### Linkage Comparison

| Linkage | Use in | Binary size | Build style |
|---|---|---|---|
| `static inline` | any `.h` or `.c` | copy per TU | header-only |
| `static` | one `.c` | local to TU | separate compilation |
| `` (empty) | exactly one `.c` | one shared definition | separate compilation |

#### `result_bool_Error` Instantiation

`vec_impl.h` (pulled in by `vec_defn.h`) instantiates `CANON_RESULT(bool, Error)` exactly once using a define guard:
```c
#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
    #define CANON_RESULT_BOOL_ERROR_DEFINED
    CANON_RESULT(bool, Error)
#endif
```
This is shared with `deque_impl.h` — safe to include both in the same translation unit.

> **Known Limitations:**
> - `DEFINE_VEC` must be used at file/global scope, not inside functions.
> - `option_##type` must be instantiated via `CANON_OPTION(type)` before calling `DEFINE_VEC` — missing it produces cryptic compile errors referencing undefined `option_T_some` / `option_T_none`.
> - With empty linkage (extern), define exactly once across all translation units — multiple definitions cause linker errors.
> - With `static inline`, each translation unit gets its own copy — safe but increases binary size per TU.
> - Element type must be a valid C identifier — use `typedef` for pointer types before calling `DEFINE_VEC`.
> - `vec.h` is the preferred entry point for header-only use — it includes `vec_defn.h` and additionally provides `DEFINE_VEC_SLICE` for `slice.h` integration.



### `vec_impl.h`
> Pure implementation logic macros for the Canon-C vec module — no naming decisions, no linkage decisions. Every identifier is received as a parameter from `vec_defn.h`. Do not include this file directly. Use `vec.h` for header-only usage or `vec_decl.h` + `vec_defn.h` for separate compilation.

#### Struct Layout

Three structs are generated per instantiation:
```c
// Vector
typedef struct canon_vec_T_s {
    T*    items;    // caller-owned element buffer (NULL iff capacity == 0)
    usize len;      // current element count (0 <= len <= capacity)
    usize capacity; // maximum elements buffer can hold
} canon_vec_T;

// Forward iterator
typedef struct canon_vec_T_iter_s {
    canon_vec_T* vec;   // parent vec — invalidated by any modification
    usize        index; // current position (0 = start)
} canon_vec_T_iter;

// Zero-copy sub-view
typedef struct canon_vec_T_slice_s {
    T*    items; // pointer into parent vec's buffer (not owned)
    usize len;   // number of elements in this slice
} canon_vec_T_slice;
```

#### Available Implementation Macros

| Macro | Behavior |
|---|---|
| `IMPL_VEC_STRUCTS(...)` | Generates all three struct typedefs |
| `IMPL_VEC_INIT(linkage, VecType, fn, type)` | Wraps caller-owned buffer; validates non-NULL and capacity limit |
| `IMPL_VEC_EMPTY(linkage, VecType, fn)` | Returns zero-initialized vec |
| `IMPL_VEC_ALLOC(linkage, VecType, fn_alloc, fn_empty, fn_init, type)` | `malloc` + `checked_mul` overflow guard; returns `fn_empty()` on failure |
| `IMPL_VEC_ARENA_ALLOC(linkage, VecType, fn_alloc, fn_empty, fn_init, type)` | `arena_alloc` + overflow guard; returns `fn_empty()` on failure |
| `IMPL_VEC_FREE(linkage, VecType, fn)` | `free(items)`, NULLs all fields; NULL-safe |
| `IMPL_VEC_LEN(linkage, VecType, fn)` | Returns `v->len`; 0 if NULL |
| `IMPL_VEC_CAPACITY(linkage, VecType, fn)` | Returns `v->capacity`; 0 if NULL |
| `IMPL_VEC_REMAINING(linkage, VecType, fn)` | Returns `capacity - len`; 0 if NULL |
| `IMPL_VEC_IS_EMPTY(linkage, VecType, fn)` | Returns `true` if NULL or `len == 0` |
| `IMPL_VEC_IS_FULL(linkage, VecType, fn)` | Returns `true` if NULL or `len >= capacity` |
| `IMPL_VEC_GET(linkage, VecType, fn, type)` | Bounds-checked copy into `*out`; false if NULL/OOB |
| `IMPL_VEC_GET_OPTION(...)` | Returns `Some(v->items[i])` or `None` |
| `IMPL_VEC_GET_UNCHECKED(linkage, VecType, fn, type)` | No bounds check; `ensure_msg` debug only |
| `IMPL_VEC_AT(linkage, VecType, fn, type)` | Pointer to element; `ensure_msg` debug only |
| `IMPL_VEC_SET(linkage, VecType, fn, type)` | Bounds-checked write; false if NULL/OOB |
| `IMPL_VEC_FIRST(linkage, VecType, fn, type)` | `&items[0]` or NULL if empty |
| `IMPL_VEC_LAST(linkage, VecType, fn, type)` | `&items[len-1]` or NULL if empty |
| `IMPL_VEC_DATA(linkage, VecType, fn, type)` | Returns `v->items`; NULL if NULL |
| `IMPL_VEC_PUSH(linkage, VecType, fn, type)` | Appends item; `Err(ERR_CAPACITY_EXCEEDED)` if full |
| `IMPL_VEC_TRY_PUSH(linkage, VecType, fn, type)` | Same; returns plain bool |
| `IMPL_VEC_PUSH_UNCHECKED(linkage, VecType, fn, type)` | No check; `ensure_msg` debug only |
| `IMPL_VEC_POP(linkage, VecType, fn, type)` | Removes last; `Err(ERR_INVALID_STATE)` if empty |
| `IMPL_VEC_POP_OPTION(...)` | Delegates to `_pop`; returns `Some` or `None` |
| `IMPL_VEC_CLEAR(linkage, VecType, fn)` | Sets `len = 0`; NULL-safe; does NOT zero memory |
| `IMPL_VEC_INSERT(linkage, VecType, fn, type)` | Shifts right via `mem_move`; `Err(ERR_OUT_OF_RANGE)` if i > len |
| `IMPL_VEC_REMOVE(linkage, VecType, fn, type)` | Shifts left via `mem_move`; `Err(ERR_OUT_OF_RANGE)` if i >= len |
| `IMPL_VEC_REMOVE_OPTION(...)` | Delegates to `_remove`; returns `Some` or `None` |
| `IMPL_VEC_APPEND_ARRAY(linkage, VecType, fn, type)` | `checked_add` overflow guard + `mem_copy`; `Err(ERR_OVERFLOW)` on overflow |
| `IMPL_VEC_EXTEND(linkage, VecType, fn, fn_append, type)` | Delegates to `_append_array` |
| `IMPL_VEC_FILL(linkage, VecType, fn, type)` | Fills `min(count, remaining)` slots; silently stops at capacity |
| `IMPL_VEC_SWAP(linkage, VecType, fn)` | Struct copy on stack; `require_msg` both non-NULL |
| `IMPL_VEC_ITER_INIT(linkage, IterType, fn, VecType)` | Returns `{vec=v, index=0}` |
| `IMPL_VEC_ITER_NEXT(linkage, IterType, fn, type)` | Advances index; copies element into `*out`; false when exhausted |
| `IMPL_VEC_SLICE_INIT(linkage, SliceType, fn, VecType, type)` | Pointer into `v->items[start]`, len = end-start; empty on invalid range |
| `IMPL_VEC_SLICE_GET(linkage, SliceType, fn, type)` | `&s->items[i]`; `ensure_msg` debug only |

#### Required Dependencies
```
core/primitives/types.h
core/primitives/limits.h
core/primitives/contract.h
core/primitives/checked.h
core/memory.h
core/arena.h
semantics/result/result.h
semantics/error.h
<stdlib.h>   (malloc, free — called directly)
```

#### `result_bool_Error` Instantiation

Instantiated exactly once per translation unit via define guard:
```c
#ifndef CANON_RESULT_BOOL_ERROR_DEFINED
    #define CANON_RESULT_BOOL_ERROR_DEFINED
    CANON_RESULT(bool, Error)
#endif
```

Shared with `deque_impl.h` — safe to include both in the same TU.

#### Capacity Limit

All constructors enforce `CANON_VEC_MAX_CAPACITY` (default 1 GB from `limits.h`):
```c
if (capacity > CANON_VEC_MAX_CAPACITY / sizeof(type)) return fn_empty();
```
Override before including `limits.h`:
```c
#define CANON_VEC_MAX_CAPACITY (256 * CANON_MB)
```

> **Known Limitations:**
> - Do not include directly — all users should go through `vec.h` or `vec_defn.h`.
> - `IMPL_VEC_CLEAR` does not zero buffer contents — stale data remains readable. For sensitive data, zero manually before calling clear.
> - `IMPL_VEC_FILL` silently stops at capacity rather than returning an error — not symmetric with push/insert behavior.
> - `IMPL_VEC_ALLOC` and `IMPL_VEC_ARENA_ALLOC` call `malloc` / `arena_alloc` directly rather than through a `mem_alloc` wrapper.
> - `IMPL_VEC_GET_UNCHECKED`, `IMPL_VEC_AT`, `IMPL_VEC_PUSH_UNCHECKED`, and `IMPL_VEC_SLICE_GET` have no release-build protection — violations cause UB silently.
> - `mem_move` is used for insert and remove shifts — element type must be trivially relocatable (no internal self-pointers).
> - `IMPL_VEC_EXTEND` is a thin alias for `IMPL_VEC_APPEND_ARRAY` — they are identical in behavior.


### `vec_mangle.h`
> Name mangling conventions for the Canon-C vec module. Single source of truth for all generated type and function names. Every macro is individually overridable via `#ifndef` guards — override before including to rename the entire API globally.

#### Default Naming Scheme

For a type `T`, the defaults produce:

| Category | What | Default name | Example for `int` |
|---|---|---|---|
| Types | Vector type | `canon_vec_T` | `canon_vec_int` |
| | Struct tag | `canon_vec_T_s` | `canon_vec_int_s` |
| | Iterator type | `canon_vec_T_iter` | `canon_vec_int_iter` |
| | Iterator tag | `canon_vec_T_iter_s` | `canon_vec_int_iter_s` |
| | Slice type | `canon_vec_T_slice` | `canon_vec_int_slice` |
| | Slice tag | `canon_vec_T_slice_s` | `canon_vec_int_slice_s` |
| Constructors | `init` | `canon_vec_T_init` | `canon_vec_int_init` |
| | `empty` | `canon_vec_T_empty` | `canon_vec_int_empty` |
| | `alloc` | `canon_vec_T_alloc` | `canon_vec_int_alloc` |
| | `arena_alloc` | `canon_vec_T_arena_alloc` | `canon_vec_int_arena_alloc` |
| | `free` | `canon_vec_T_free` | `canon_vec_int_free` |
| Queries | `len` | `canon_vec_T_len` | `canon_vec_int_len` |
| | `capacity` | `canon_vec_T_capacity` | `canon_vec_int_capacity` |
| | `remaining` | `canon_vec_T_remaining` | `canon_vec_int_remaining` |
| | `is_empty` | `canon_vec_T_is_empty` | `canon_vec_int_is_empty` |
| | `is_full` | `canon_vec_T_is_full` | `canon_vec_int_is_full` |
| Element access | `get` | `canon_vec_T_get` | `canon_vec_int_get` |
| | `get_option` | `canon_vec_T_get_option` | `canon_vec_int_get_option` |
| | `get_unchecked` | `canon_vec_T_get_unchecked` | `canon_vec_int_get_unchecked` |
| | `at` | `canon_vec_T_at` | `canon_vec_int_at` |
| | `set` | `canon_vec_T_set` | `canon_vec_int_set` |
| | `first` | `canon_vec_T_first` | `canon_vec_int_first` |
| | `last` | `canon_vec_T_last` | `canon_vec_int_last` |
| | `data` | `canon_vec_T_data` | `canon_vec_int_data` |
| Modification | `push` | `canon_vec_T_push` | `canon_vec_int_push` |
| | `try_push` | `canon_vec_T_try_push` | `canon_vec_int_try_push` |
| | `push_unchecked` | `canon_vec_T_push_unchecked` | `canon_vec_int_push_unchecked` |
| | `pop` | `canon_vec_T_pop` | `canon_vec_int_pop` |
| | `pop_option` | `canon_vec_T_pop_option` | `canon_vec_int_pop_option` |
| | `clear` | `canon_vec_T_clear` | `canon_vec_int_clear` |
| | `insert` | `canon_vec_T_insert` | `canon_vec_int_insert` |
| | `remove` | `canon_vec_T_remove` | `canon_vec_int_remove` |
| | `remove_option` | `canon_vec_T_remove_option` | `canon_vec_int_remove_option` |
| Bulk | `append_array` | `canon_vec_T_append_array` | `canon_vec_int_append_array` |
| | `extend` | `canon_vec_T_extend` | `canon_vec_int_extend` |
| | `fill` | `canon_vec_T_fill` | `canon_vec_int_fill` |
| | `swap` | `canon_vec_T_swap` | `canon_vec_int_swap` |
| Iterator | `iter_init` | `canon_vec_T_iter_init` | `canon_vec_int_iter_init` |
| | `iter_next` | `canon_vec_T_iter_next` | `canon_vec_int_iter_next` |
| Slice | `slice_init` | `canon_vec_T_slice_init` | `canon_vec_int_slice_init` |
| | `slice_get` | `canon_vec_T_slice_get` | `canon_vec_int_slice_get` |

#### Customization

Define overrides **before** including any vec header:
```c
// Project namespace prefix
#define MANGLE_VEC_TYPE(type)       myproject_vec_##type
#define MANGLE_VEC_ITER_TYPE(type)  myproject_vec_##type##_iter
#define MANGLE_VEC_SLICE_TYPE(type) myproject_vec_##type##_slice
#include "data/vec/vec.h"
DEFINE_VEC(static inline, int)

myproject_vec_int v = myproject_vec_int_init(buf, 64);

// Rename a single function
#define MANGLE_VEC_PUSH(type) myproject_vec_##type##_push
```

Individual function macros can be overridden selectively — only the macros you define before including are affected. The rest fall back to their `#ifndef`-guarded defaults.

> **Known Limitations:**
> - All macros are guarded with `#ifndef` — overrides must be defined before the first include of any vec header in that translation unit.
> - Overrides apply to all types instantiated after them, not selectively per type — use separate translation units if per-type naming is needed.
> - `MANGLE_VEC_TYPE` and `MANGLE_VEC_STRUCT_TAG` are independent — overriding the type name does not automatically update the struct tag, and vice versa.
> - `vec_mangle.h` has no includes of its own — it is safe to include from any layer.
> - The `fmt` and `range` extension files (`vec_fmt.h`, `vec_range.h`) define their own mangle macros (`MANGLE_VEC_TO_STRINGBUF`, `MANGLE_VEC_EXTEND_FROM_RANGE`) separately — these are not declared in `vec_mangle.h`.

### `vec_fmt.h`
> Optional StringBuf formatting extension for the Canon-C vec module. Adds `to_stringbuf` and `to_stringbuf_cb` to any typed vector instantiated via `DEFINE_VEC`. Not included by `vec.h` by default — only include when you need vector-to-string formatting.

#### Setup

Requires `data/stringbuf.h`. Call `DEFINE_VEC_FMT` after `DEFINE_VEC` for the same type:
```c
#include "data/vec/vec.h"
#include "data/vec/vec_fmt.h"

DEFINE_VEC(static inline, int)
DEFINE_VEC_FMT(static inline, int)
```

#### `canon_vec_T_to_stringbuf` — printf-style formatting

**`void canon_vec_T_to_stringbuf(const canon_vec_T* v, StringBuf* sb, const char* fmt)`**
Iterates all elements and writes each via `stringbuf_printf(sb, fmt, element)`. Output is appended to whatever is already in `sb` — it is NOT cleared first. Does nothing if v, sb, or fmt is NULL.
```c
char out[256];
StringBuf sb = stringbuf_init(out, sizeof(out));

canon_vec_int_to_stringbuf(&v, &sb, "%d ");
// sb now contains "1 2 3 4 "
```

`fmt` must be a valid printf format string for the element type. Mismatched format strings produce undefined behavior — same as printf.

#### `canon_vec_T_to_stringbuf_cb` — callback-style formatting

**`void canon_vec_T_to_stringbuf_cb(const canon_vec_T* v, StringBuf* sb, void (*cb)(StringBuf*, T))`**
Iterates all elements and calls `cb(sb, element)` for each. Callback is responsible for writing to `sb`. Does nothing if v, sb, or cb is NULL.
```c
void fmt_int(StringBuf* sb, int val) {
    stringbuf_printf(sb, "[%d]", val);
}

canon_vec_int_to_stringbuf_cb(&v, &sb, fmt_int);
// sb now contains "[1][2][3][4]"
```

Use the callback variant for custom types, complex formatting, or when a printf format string is insufficient.

#### Choosing Between the Two

| | `to_stringbuf` | `to_stringbuf_cb` |
|---|---|---|
| Format control | printf format string | arbitrary callback |
| Element type | primitive / printf-compatible | any |
| Overhead | `stringbuf_printf` per element | function call per element |
| NULL safety | fmt == NULL → no-op | cb == NULL → no-op |

#### Performance

- Time: O(n) where n = v->len
- Space: O(1) — writes into caller-owned StringBuf, no allocation

#### Name Mangling

Both functions follow the same `#ifndef`-guarded override pattern as the rest of the vec module:
```c
// Override before including vec_fmt.h:
#define MANGLE_VEC_TO_STRINGBUF(type)    myproject_vec_##type##_to_str
#define MANGLE_VEC_TO_STRINGBUF_CB(type) myproject_vec_##type##_to_str_cb
```

> **Known Limitations:**
> - Not included by `vec.h` — must be included explicitly after `vec.h`.
> - `DEFINE_VEC_FMT` must be called after `DEFINE_VEC(linkage, type)` for the same type — calling it before produces undefined references.
> - `to_stringbuf` output is appended to `sb` — clear it manually before calling if a fresh output is needed.
> - `StringBuf` silently truncates if capacity is exceeded — check `stringbuf_remaining()` before calling if overflow matters.
> - `to_stringbuf` accesses `v->items` directly (field name `items`) — if `vec_mangle.h` struct layout is overridden, this file may need updating.
> - The callback in `to_stringbuf_cb` receives elements by value — for large structs, consider a pointer-based wrapper callback.
> - `MANGLE_VEC_TO_STRINGBUF` and `MANGLE_VEC_TO_STRINGBUF_CB` are defined in `vec_fmt.h` itself, not in `vec_mangle.h` — they must be overridden before including `vec_fmt.h` specifically.
> - Depends on `data/stringbuf.h` — not suitable for targets without that layer available.


### `vec_range.h`
> Optional range-fill extension for the Canon-C vec module. Adds `extend_from_range` to any typed vector instantiated via `DEFINE_VEC`. Fills a vec from an `IntRange` — ascending or descending. Not included by `vec.h` by default — only include when you need range-to-vector filling.

#### Setup

Requires `data/range.h`. Call `DEFINE_VEC_RANGE` after `DEFINE_VEC` for the same type:
```c
#include "data/vec/vec.h"
#include "data/vec/vec_range.h"

DEFINE_VEC(static inline, int)
DEFINE_VEC_RANGE(static inline, int)
```

#### Usage

**`result_bool_Error canon_vec_T_extend_from_range(canon_vec_T* v, IntRange r)`**
Iterates the range and appends each value into `v`. Values are cast from `isize` to `T`. Supports ascending and descending ranges.
```c
int buf[16];
canon_vec_int v = canon_vec_int_init(buf, 16);

IntRange asc = range_make(0, 10);   // 0..10 ascending
canon_vec_int_extend_from_range(&v, asc);
// v now contains [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]

IntRange desc = range_make(5, 0);   // 5..0 descending
canon_vec_int_extend_from_range(&v, desc);
// appends [5, 4, 3, 2, 1]
```

#### Return Values
- `Ok(true)` — all range values appended successfully
- `Err(ERR_INVALID_ARG)` — v or v->items is NULL
- `Err(ERR_OVERFLOW)` — v->len + range count overflows `usize`
- `Err(ERR_CAPACITY_EXCEEDED)` — insufficient space in v

#### Range Direction

Direction is inferred automatically from `r.start` and `r.end`:
```c
r.end >= r.start  →  step = +1  (ascending)
r.end <  r.start  →  step = -1  (descending)
```

An empty range (`r.start == r.end`) appends nothing and returns `Ok(true)`.

#### Key-Value Correspondence with Hashmap

When using `hashmap_range.h` and `vec_range.h` together, `extend_from_range` can pre-fill key vecs for batch lookup:
```c
IntRange keys = range_make(0, 100);
canon_vec_int_extend_from_range(&key_vec, keys);
// then use key_vec with hashmap_collect_keys / HASHMAP_FOR_EACH
```

#### Performance

- Time: O(count) where count = abs(r.end - r.start)
- Space: O(1) — no allocation, fills into existing buffer

#### Name Mangling

Follows the same `#ifndef`-guarded override pattern as the rest of the vec module:
```c
// Override before including vec_range.h:
#define MANGLE_VEC_EXTEND_FROM_RANGE(type) myproject_vec_##type##_fill_range
```

Defined in `vec_range.h` itself, not in `vec_mangle.h` — must be overridden before including `vec_range.h` specifically.

> **Known Limitations:**
> - Not included by `vec.h` — must be included explicitly after `vec.h`.
> - `DEFINE_VEC_RANGE` must be called after `DEFINE_VEC(linkage, type)` for the same type.
> - Range values are cast from `isize` to `T` — for unsigned element types, negative range values will wrap. Caller is responsible for ensuring the range fits the element type.
> - Only makes semantic sense for numeric element types (`int`, `u32`, `i64`, etc.) — using with struct types will produce a compile error on the cast.
> - Element count is computed as `abs(r.end - r.start)` via `isize` arithmetic — very large ranges may overflow `isize` before the `checked_add` guard fires.
> - `MANGLE_VEC_EXTEND_FROM_RANGE` is defined in `vec_range.h` itself, not in `vec_mangle.h` — override it before including `vec_range.h`.
> - Depends on `data/range.h` — not suitable for targets without that module available.


### `array.h`
> Fixed-size typed array with compile-time capacity. Buffer lives inside the struct — no heap allocation. Always fully initialized — no length tracking. Generated per `(type, N)` pair via `DEFINE_ARRAY(type, N)`.

#### Setup
```c
DEFINE_ARRAY(int, 8)

// Pointer types — typedef first:
typedef void* voidptr;
DEFINE_ARRAY(voidptr, 4)

// Requires DEFINE_SLICE(type) before using as_slice
// Requires CANON_OPTION(type) before using get_option / first_option / last_option
```

Must be used at file/global scope. Use once per `(type, N)` pair.

#### Constructors

**`array_T_N array_T_N_zero(void)`**
Returns an array with all bytes zeroed. O(N).

**`array_T_N array_T_N_fill(type val)`**
Returns an array with every element set to `val`. O(N).

**`array_T_N array_T_N_from_ptr(const type* src, usize count)`**
Returns an array populated by copying `count` elements from `src`. If `count < N`, remaining elements are zeroed. If `count > N`, only first N are copied. Uses `checked_mul()` for overflow-safe byte size. O(N).

#### Queries

**`usize array_T_N_len(void)`**
Returns N — always. Compile-time constant. O(1).

**`usize array_T_N_size_bytes(void)`**
Returns `N * sizeof(type)`. Uses `checked_mul()` — returns `CANON_USIZE_MAX` on overflow. O(1).

#### Element Access

**`bool array_T_N_get(const array_T_N* a, usize i, type* out)`**
Bounds-checked copy into `*out`. Returns false if `a == NULL`, `out == NULL`, or `i >= N`. O(1).

**`option_T array_T_N_get_option(const array_T_N* a, usize i)`**
Returns `Some(element)` or `None` if OOB or `a == NULL`. O(1).

**`type array_T_N_get_unchecked(const array_T_N* a, usize i)`**
No bounds check — `ensure_msg()` in debug only. UB in release if violated. O(1).

**`bool array_T_N_set(array_T_N* a, usize i, type val)`**
Bounds-checked write. Returns false if `a == NULL` or `i >= N`. O(1).

**`type* array_T_N_at(array_T_N* a, usize i)`**
Returns pointer to element, or `NULL` if OOB. O(1).

**`type* array_T_N_first(array_T_N* a)`**
Returns pointer to first element. `require_msg` if `a == NULL`. O(1).

**`type* array_T_N_last(array_T_N* a)`**
Returns pointer to last element. `require_msg` if `a == NULL`. O(1).

**`option_T array_T_N_first_option(const array_T_N* a)`**
Returns `Some(first)` or `None` if `a == NULL`. O(1).

**`option_T array_T_N_last_option(const array_T_N* a)`**
Returns `Some(last)` or `None` if `a == NULL`. O(1).

#### Mutation

**`void array_T_N_fill_all(array_T_N* a, type val)`**
Sets every element to `val`. NULL-safe. O(N).

**`void array_T_N_copy_from(array_T_N* dst, const array_T_N* src)`**
Copies all N elements from `src` into `dst` via `memcpy`. Both must be non-NULL (`require_msg`). O(N).

#### Comparison

**`bool array_T_N_equal(const array_T_N* a, const array_T_N* b)`**
Returns true if all N elements are byte-equal (`memcmp`). Returns false if either is NULL. O(N).

#### Views (zero-copy)

**`slice_T array_T_N_as_slice(array_T_N* a)`**
Returns a typed `slice_T` view over all N elements. Requires `DEFINE_SLICE(type)`. `require_msg` if `a == NULL`. O(1).

**`slice_T array_T_N_as_slice_const(const array_T_N* a)`**
Const variant of `as_slice`. O(1).

**`bytes_t array_T_N_as_bytes(array_T_N* a)`**
Returns mutable byte view over all `N * sizeof(type)` bytes. Returns empty `bytes_t` on overflow. O(1).

**`cbytes_t array_T_N_as_cbytes(const array_T_N* a)`**
Read-only byte view. Returns empty `cbytes_t` on overflow. O(1).

#### Iteration Macros

**`ARRAY_FOR(type, N, arr_ptr, idx_var)`**
Iterates over all elements by index. Declares `usize idx_var` from 0 to N-1.
```c
ARRAY_FOR(int, 8, &a, i) {
    a.items[i] = (int)i * 2;
}
```

**`ARRAY_FOR_PTR(type, N, arr_ptr, elem_ptr)`**
Iterates over all elements by pointer. Declares `type* elem_ptr`.
```c
ARRAY_FOR_PTR(int, 8, &a, p) {
    *p *= 2;
}
```

#### Struct Layout
```c
typedef struct {
    type items[N];  // N elements, always initialized
} array_T_N;
// sizeof(array_T_N) == N * sizeof(type) (plus compiler alignment padding)
```

#### Quick Start
```c
DEFINE_ARRAY(int, 8)

array_int_8 a = array_int_8_zero();

array_int_8_set(&a, 3, 42);

int val;
array_int_8_get(&a, 3, &val);        // val = 42

option_int opt = array_int_8_get_option(&a, 3);   // Some(42)
option_int oob = array_int_8_get_option(&a, 99);  // None

slice_int sv = array_int_8_as_slice(&a);           // zero-copy view

array_int_8_fill_all(&a, 0);                       // reset all to 0
```

> **Known Limitations:**
> - `equal()` uses `memcmp` — not suitable for types with padding bytes or pointer members. Compare element-by-element for those.
> - `get_unchecked()`, `first()`, `last()` have no release-build bounds protection.
> - `as_slice()` requires `DEFINE_SLICE(type)` to have been called first.
> - `get_option()`, `first_option()`, `last_option()` require `CANON_OPTION(type)` to have been called first.
> - `from_ptr()` does not overflow-check `sizeof(type) * count` beyond `checked_mul()` — returns zeroed array on overflow.
> - Not thread-safe — concurrent modifications require external synchronization. Concurrent reads are safe.


### `bitset.h`
> Fixed-capacity bitset backed by a caller-owned `u64` buffer. O(1) single-bit operations, O(n/64) bulk ops. No allocation inside any function. Bits beyond capacity are always kept zero.

#### Constants & Sizing

**`BITSET_BITS_PER_WORD`** — 64 (bits per backing word)

**`BITSET_WORD_COUNT(n)`** — Number of `u64` words needed to hold `n` bits.
```c
BITSET_WORD_COUNT(64)   // → 1
BITSET_WORD_COUNT(65)   // → 2
BITSET_WORD_COUNT(200)  // → 4
```

**`BITSET_NPOS`** — Sentinel (`CANON_USIZE_MAX`) returned by find functions when no bit is found.

#### Initialization

**`void bitset_init(Bitset* bs, u64* words, usize capacity)`**
Initializes bitset with caller-provided buffer. Clears all bits. O(n/64).
```c
u64 words[BITSET_WORD_COUNT(200)];
Bitset bs;
bitset_init(&bs, words, 200);
```

#### Single-Bit Operations — O(1)

**`void bitset_set(Bitset* bs, usize i)`** — Sets bit `i` to 1.

**`void bitset_clear(Bitset* bs, usize i)`** — Clears bit `i` to 0.

**`void bitset_toggle(Bitset* bs, usize i)`** — Flips bit `i`.

**`bool bitset_test(const Bitset* bs, usize i)`** — Returns true if bit `i` is set. Returns false if `bs == NULL` or `i >= capacity`.

**`void bitset_assign(Bitset* bs, usize i, bool value)`** — Sets bit `i` to `value`.

All single-bit ops use `require_msg` if index is out of range. NULL-safe via early return.

#### Bulk Operations — O(n/64)

**`void bitset_clear_all(Bitset* bs)`** — Clears all bits. NULL-safe.

**`void bitset_set_all(Bitset* bs)`** — Sets all bits within capacity. Padding bits stay zero. NULL-safe.

**`void bitset_not(Bitset* bs)`** — Inverts all bits within capacity. Padding bits stay zero. NULL-safe.

**`void bitset_and(Bitset* bs, const Bitset* other)`** — `bs &= other` in-place. Operates on `min(word_count)` words. Words beyond `other`'s range become 0.

**`void bitset_or(Bitset* bs, const Bitset* other)`** — `bs |= other` in-place. Operates on `min(word_count)` words.

**`void bitset_xor(Bitset* bs, const Bitset* other)`** — `bs ^= other` in-place. Operates on `min(word_count)` words. Padding bits stay zero.

#### Query — O(n/64)

**`usize bitset_count(const Bitset* bs)`** — Returns number of set bits (popcount). Uses `popcount64` per word.

**`bool bitset_is_empty(const Bitset* bs)`** — Returns true if no bits are set.

**`bool bitset_is_full(const Bitset* bs)`** — Returns true if all bits within capacity are set.

**`bool bitset_is_disjoint(const Bitset* bs, const Bitset* other)`** — Returns true if no bits are shared.

**`usize bitset_capacity(const Bitset* bs)`** — Returns total usable bits. NULL-safe — returns 0. O(1).

#### Search — O(n/64)

**`usize bitset_find_first(const Bitset* bs)`** — Index of first set bit, or `BITSET_NPOS`. Uses `ctz64`.

**`usize bitset_find_next(const Bitset* bs, usize prev)`** — Index of next set bit after `prev`, or `BITSET_NPOS`.

**`usize bitset_find_last(const Bitset* bs)`** — Index of last set bit, or `BITSET_NPOS`. Uses `clz64`.

#### View — O(1)

**`bytes_t bitset_as_bytes(const Bitset* bs)`**
Returns a `bytes_t` view over the raw backing words (including padding — always zero). Useful for serialization and hashing. Non-owning — do not free.

#### Iteration Macro

**`BITSET_FOR_EACH(bs_ptr, idx_var)`**
Iterates over all set bit indices.
```c
BITSET_FOR_EACH(&bs, i) {
    printf("bit %zu is set\n", i);
}
```

#### Struct Layout
```c
typedef struct {
    u64*  words;       // caller-owned backing buffer
    usize capacity;    // number of usable bits
    usize word_count;  // number of u64 words
} Bitset;
```

#### Quick Start
```c
u64 words[BITSET_WORD_COUNT(200)];
Bitset bs;
bitset_init(&bs, words, 200);

bitset_set(&bs, 42);
bitset_set(&bs, 199);

bool v = bitset_test(&bs, 42);    // true
usize n = bitset_count(&bs);      // 2
usize f = bitset_find_first(&bs); // 42

BITSET_FOR_EACH(&bs, i) {
    printf("%zu\n", i);  // 42, 199
}
```

> **Known Limitations:**
> - Not thread-safe — caller must synchronize if shared across threads.
> - `and` / `or` / `xor` operate on `min(word_count)` words — results are undefined if capacities differ significantly and you rely on the full range of the larger bitset.
> - `bitset_clear_all` and `bitset_set_all` do not zero frame contents beyond capacity — padding invariant is maintained by `bitset_clear_padding()` internally.
> - All single-bit ops use `require_msg` — they always fire in release builds if index is out of range.
> - `bitset_as_bytes` covers the entire word array including padding bits — always zero by invariant but present in the view.


### `priority_queue.h`
> Fixed-capacity binary min-heap with caller-owned buffer. O(log n) push/pop, O(1) peek. Use a descending comparator for max-heap behavior. Returns `option_type` / `result_bool_Error` for idiomatic Canon-C style.

#### Initialization

**`void pq_init(PriorityQueue* pq, void* buffer, usize capacity, usize elem_size, algo_cmp_fn cmp, void* ctx)`**
Initializes the priority queue with a caller-owned buffer. All fields required — no NULL allowed except `ctx`. O(1).
```c
int buf[64];
PriorityQueue pq;
pq_init(&pq, buf, 64, sizeof(int), algo_cmp_int, NULL);
```

**`void pq_heapify(PriorityQueue* pq, usize len)`**
Builds a valid heap from the first `len` elements already in the buffer. Clamps to `capacity` if `len` exceeds it. O(n log n).

#### Typed Macro Wrapper
```c
DEFINE_PRIORITY_QUEUE(int)

int buf[64];
pq_int h;
pq_int_init(&h, buf, 64, algo_cmp_int, NULL);
```

`DEFINE_PRIORITY_QUEUE(type)` generates a `pq_##type` struct wrapping `PriorityQueue` and typed versions of all functions below.

#### Core Operations (preferred)

**`result_bool_Error pq_push_result(PriorityQueue* pq, const void* elem)`**
**`result_bool_Error pq_T_push_result(pq_T* h, type val)`**
Inserts an element. O(log n).
- `Ok(true)` — inserted
- `Err(ERR_INVALID_ARG)` — `pq` or `elem` is NULL
- `Err(ERR_CAPACITY_EXCEEDED)` — heap is full

**`option_type pq_pop_option(PriorityQueue* pq)`**
**`option_T pq_T_pop_option(pq_T* h)`**
Removes and returns the top element. O(log n).
- `Some(value)` — element removed
- `None` — heap is empty or `pq == NULL`

**`option_type pq_peek_option(const PriorityQueue* pq)`**
**`option_T pq_T_peek_option(const pq_T* h)`**
Returns the top element without removing it. O(1).
- `Some(value)` — top element
- `None` — heap is empty or `pq == NULL`

**`result_bool_Error pq_remove_at_result(PriorityQueue* pq, usize i)`**
**`result_bool_Error pq_T_remove_at_result(pq_T* h, usize i)`**
Removes element at heap index `i`. Runs sift-up then sift-down to restore invariant. O(log n).
- `Ok(true)` — removed
- `Err(ERR_OUT_OF_RANGE)` — `i >= len` or `pq == NULL`

#### Legacy / Compatibility Variants
```c
bool pq_push(PriorityQueue* pq, const void* elem)
bool pq_pop(PriorityQueue* pq, void* out)
bool pq_peek(const PriorityQueue* pq, void* out)
bool pq_remove_at(PriorityQueue* pq, usize i)

// Typed:
bool pq_T_push(pq_T* h, type val)
bool pq_T_pop(pq_T* h, type* out)
bool pq_T_peek(const pq_T* h, type* out)
bool pq_T_remove_at(pq_T* h, usize i)
```
Plain bool variants — delegate to the `result`/`option` versions above.

#### Queries
```c
pq_len(&pq)        // usize — current element count
pq_capacity(&pq)   // usize — fixed maximum
pq_remaining(&pq)  // usize — free slots
pq_is_empty(&pq)   // bool
pq_is_full(&pq)    // bool
```
All NULL-safe — return 0 or true if `pq == NULL`. Typed variants follow the same pattern (`pq_T_len`, etc.).

#### View

**`bytes_t pq_as_bytes(const PriorityQueue* pq)`**
Returns a `bytes_t` view over current heap contents `[data, data + len * elem_size)`. Returns empty `bytes_t` if empty or NULL.

#### Min-heap vs Max-heap
```c
// Min-heap (default) — smallest value at top
pq_init(&pq, buf, 64, sizeof(int), algo_cmp_int, NULL);

// Max-heap — largest value at top
pq_init(&pq, buf, 64, sizeof(int), algo_cmp_int_desc, NULL);
```

#### Struct Layout
```c
typedef struct {
    void*       data;       // caller-owned buffer
    usize       len;        // current element count
    usize       capacity;   // fixed maximum
    usize       elem_size;  // size of each element in bytes
    algo_cmp_fn cmp;        // three-way comparator
    void*       ctx;        // optional context passed to cmp (may be NULL)
} PriorityQueue;

// Typed wrapper:
typedef struct { PriorityQueue _pq; } pq_##type;
```

#### Quick Start
```c
DEFINE_PRIORITY_QUEUE(int)

int buf[16];
pq_int h;
pq_int_init(&h, buf, 16, algo_cmp_int, NULL);

pq_int_push_result(&h, 30);
pq_int_push_result(&h, 10);
pq_int_push_result(&h, 20);

option_int top = pq_int_peek_option(&h);  // Some(10) — min at top
option_int val = pq_int_pop_option(&h);   // Some(10)
```

> **Known Limitations:**
> - No type safety in the untyped `PriorityQueue` API — passing wrong `elem_size` or mismatched types is UB.
> - `pq_swap` falls back to a byte-by-byte loop for elements larger than `CANON_MEM_SWAP_MAX` (default 256 bytes) — avoid very large element types.
> - `pq_heapify` clamps `len` to `capacity` silently — no error returned.
> - Not thread-safe — concurrent modifications require external synchronization.
> - `pq_as_bytes` view is invalidated by any push, pop, or remove operation.
> - The typed `DEFINE_PRIORITY_QUEUE(type)` macro uses `option_type` and `result_bool_Error` internally — `CANON_OPTION(type)` and `CANON_RESULT(bool, Error)` must be instantiated before use.


### `queue.h`
> Bounded FIFO queue — thin zero-overhead wrapper over `canon_deque_##type`. enqueue = push_back, dequeue = pop_front, peek = peek_front. Fixed capacity, no automatic growth.

#### Setup
```c
// Deque must be instantiated first
DEFINE_DEQUE(static inline, int)
DEFINE_QUEUE(static inline, int)

// Pointer types — typedef first:
typedef void* voidptr;
DEFINE_DEQUE(static inline, voidptr)
DEFINE_QUEUE(static inline, voidptr)
```

Must be used at file/global scope. `DEFINE_DEQUE(linkage, type)` must be called before `DEFINE_QUEUE(linkage, type)`.

#### Separate Compilation
```c
// In tasks.h:
#include "data/deque/deque_decl.h"
#include "data/queue.h"
DECLARE_DEQUE(Task)
DECLARE_QUEUE(Task)

// In tasks.c:
#include "data/deque/deque_defn.h"
#include "data/queue.h"
DEFINE_DEQUE(, Task)
DEFINE_QUEUE(, Task)
```

#### Constructor

**`void canon_queue_T_init(canon_queue_T* q, type* buffer, usize capacity)`**
Initializes queue with caller-owned buffer. `require_msg` if `q == NULL`. O(1).
```c
int buf[128];
canon_queue_int q;
canon_queue_int_init(&q, buf, 128);
```

#### Enqueue / Dequeue (Result variants)

**`result_bool_Error canon_queue_T_enqueue(canon_queue_T* q, type item)`**
Adds item to the back of the queue. O(1).
- `Ok(true)` — enqueued
- `Err(ERR_INVALID_ARG)` — `q == NULL` or buffer is NULL
- `Err(ERR_CAPACITY_EXCEEDED)` — queue is full

**`result_bool_Error canon_queue_T_dequeue(canon_queue_T* q, type* out)`**
Removes and returns front item (oldest). O(1).
- `Ok(true)` — dequeued into `*out`
- `Err(ERR_INVALID_ARG)` — `q == NULL`, `out == NULL`, or buffer is NULL
- `Err(ERR_INVALID_STATE)` — queue is empty

#### Dequeue (Option variant)

**`option_T canon_queue_T_dequeue_option(canon_queue_T* q)`**
Removes and returns front item as `Option<T>`. O(1).
- `Some(item)` — dequeued
- `None` — empty or invalid

#### Peek

**`bool canon_queue_T_peek(const canon_queue_T* q, type* out)`**
Returns front item without removing it. Returns false if empty or invalid. Queue unchanged. O(1).

**`option_T canon_queue_T_peek_option(const canon_queue_T* q)`**
Returns front item as `Option<T>` without removing. Queue unchanged. O(1).

#### Queries
```c
canon_queue_T_len(&q)        // usize — current element count
canon_queue_T_capacity(&q)   // usize — fixed maximum
canon_queue_T_remaining(&q)  // usize — free slots
canon_queue_T_is_empty(&q)   // bool
canon_queue_T_is_full(&q)    // bool
```
All NULL-safe — return 0 or true if `q == NULL`.

#### Misc

**`void canon_queue_T_clear(canon_queue_T* q)`**
Resets queue to empty. O(1). Does NOT zero buffer contents. NULL-safe.

#### Type Layout

`canon_queue_T` is a typedef alias for `canon_deque_T` — no new struct. All `canon_deque_T` functions remain usable. `DEFINE_QUEUE` only adds FIFO-named wrappers for clarity.

#### Quick Start
```c
DEFINE_DEQUE(static inline, int)
DEFINE_QUEUE(static inline, int)

int buf[128];
canon_queue_int q;
canon_queue_int_init(&q, buf, 128);

canon_queue_int_enqueue(&q, 10);
canon_queue_int_enqueue(&q, 20);

int val;
canon_queue_int_dequeue(&q, &val);         // val = 10 (FIFO)

option_int next = canon_queue_int_dequeue_option(&q);  // Some(20)
option_int front = canon_queue_int_peek_option(&q);    // None — now empty
```

> **Known Limitations:**
> - `DEFINE_DEQUE(linkage, type)` must be called before `DEFINE_QUEUE(linkage, type)` — missing this causes compile errors.
> - `option_##type` must be instantiated via `CANON_OPTION(type)` before calling `DEFINE_DEQUE` — not enforced automatically.
> - Fixed capacity — no growth. For auto-growing queues, wrap `dynvec.h`.
> - `clear()` does not zero buffer contents — stale data remains readable until overwritten. Zero manually for sensitive data.
> - Not thread-safe — concurrent modifications require external synchronization.
> - Not suitable for double-ended access (use deque directly) or random access by index (use vec).


### `range.h`
> Explicit bounded integer range generator with `[start, end)` semantics. Supports ascending, descending, and stepped iteration. Overflow-safe via `checked_add_isize()`. All operations O(1) except iteration itself.

#### Setup
```c
#include "data/range.h"

// Requires CANON_OPTION(isize) for peek_option / current_option
```

#### Construction

**`range range_make(isize start, isize end, isize step)`**
Creates a range `[start, end)` with given step. `step == 0` is normalized to `+1`. O(1).
```c
range_make(0, 10, 1)   // 0, 1, 2, ..., 9
range_make(10, 0, -1)  // 10, 9, 8, ..., 1
range_make(0, 20, 5)   // 0, 5, 10, 15
range_make(5, 5, 1)    // empty
```

**`range range_upto(isize end)`**
Ascending `[0, end)` with step 1.
```c
range_upto(5)  // 0, 1, 2, 3, 4
```

**`range range_from_to(isize start, isize end)`**
Ascending `[start, end)` with step 1.
```c
range_from_to(5, 10)  // 5, 6, 7, 8, 9
```

**`range range_downfrom(isize start)`**
Descending `[start, 0)` with step -1.
```c
range_downfrom(5)  // 5, 4, 3, 2, 1
```

**`range range_downto(isize start, isize end)`**
Descending `[start, end)` with step -1.
```c
range_downto(10, 5)  // 10, 9, 8, 7, 6
```

#### Queries

**`bool range_is_empty(const range* r)`** — True if exhausted or `r == NULL`. O(1).

**`bool range_has_next(const range* r)`** — True if at least one element remains. Equivalent to `!range_is_empty(r)`. O(1).

**`bool range_is_valid(const range* r)`** — True if `r != NULL` and has remaining elements. O(1).

**`usize range_len(const range* r)`**
Returns exact remaining element count. Pure arithmetic — does NOT consume the range. Returns `CANON_USIZE_MAX` if count overflows `usize`. O(1).
```c
range_len(&range_make(0, 10, 1))   // → 10
range_len(&range_make(0, 10, 2))   // → 5
range_len(&range_make(10, 0, -1))  // → 10
range_len(&range_make(5, 5, 1))    // → 0
```

**`usize range_remaining(const range* r)`** — Alias for `range_len`. O(1).

**`bool range_peek(const range* r, isize* out)`**
Peeks at next value without advancing. Returns false if empty or invalid. Range unchanged. O(1).

**`option_isize range_peek_option(const range* r)`**
Returns `Some(next)` or `None`. Range unchanged. O(1).

**`range_current_option`** — Alias macro for `range_peek_option`.

#### Iteration

**`isize range_next(range* r)`**
Returns next value and advances the iterator. Uses `checked_add_isize()` — saturates to `end` on overflow. O(1).

⚠️ Always check `range_has_next()` before calling, or use `RANGE_FOR`.
```c
range r = range_make(0, 5, 1);
while (range_has_next(&r)) {
    isize val = range_next(&r);  // 0, 1, 2, 3, 4
}
```

**`void range_reset(range* r, isize new_start)`**
Resets `current` to `new_start`. `end` and `step` unchanged. NULL-safe. O(1).

**`void range_skip(range* r, usize n)`**
Skips `n` elements forward via direct arithmetic — O(1), not O(n). Saturates to `end` on overflow or boundary crossing. NULL-safe.
```c
range r = range_make(0, 10, 1);
range_skip(&r, 5);
isize val = range_next(&r);  // val == 5
```

**`range_advance`** — Alias macro for `range_skip`.

#### `RANGE_FOR` Macro
```c
RANGE_FOR(var, r_expr)
```
Clean for-loop syntax. `r_expr` is evaluated exactly once. `var` must be declared before the loop.
```c
int i;
RANGE_FOR(i, range_make(0, 10, 1)) {
    printf("%d ", i);  // 0 1 2 3 4 5 6 7 8 9
}

// Descending
RANGE_FOR(i, range_make(10, 0, -1)) {
    printf("%d ", i);  // 10 9 8 7 6 5 4 3 2 1
}

// Stepped
RANGE_FOR(i, range_make(0, 20, 3)) {
    printf("%d ", i);  // 0 3 6 9 12 15 18
}

// Nested
int j;
RANGE_FOR(i, range_upto(3)) {
    RANGE_FOR(j, range_upto(3)) {
        printf("(%d,%d) ", i, j);
    }
}
```

`break` and `continue` work normally inside the loop body.

#### Struct Layout
```c
typedef struct {
    isize current;  // next value to be returned by range_next()
    isize end;      // exclusive bound
    isize step;     // positive = ascending, negative = descending, never 0
} range;
```

> **Known Limitations:**
> - `range_next()` has a `require_msg` (always-on) for `r == NULL` and an `ensure_msg` (debug-only) for exhausted range — calling on an exhausted range is a logic error.
> - `RANGE_FOR` uses `typeof(var)` for the cast — requires GCC/Clang or C23. Not valid in strict C99.
> - `range_len()` returns `CANON_USIZE_MAX` on overflow — caller must check before using as allocation size.
> - `range_skip()` uses `checked_mul_isize` for the jump — saturates to `end` silently on overflow rather than returning an error.
> - Do not modify `_r` inside a `RANGE_FOR` loop body — behavior is undefined.
> - Empty ranges (`start == end`, or wrong direction for given step) are valid and safe — `range_has_next()` returns false immediately.


### `stack.h`
> Bounded LIFO stack — thin zero-overhead wrapper over `canon_vec_##type`. push = vec_push, pop = vec_pop, peek = vec_last. Fixed capacity, no automatic growth.

#### Setup
```c
// Vec must be instantiated first
DEFINE_VEC(static inline, int)
DEFINE_STACK(static inline, int)

// Pointer types — typedef first:
typedef void* voidptr;
DEFINE_VEC(static inline, voidptr)
DEFINE_STACK(static inline, voidptr)
```

Must be used at file/global scope. `DEFINE_VEC(linkage, type)` must be called before `DEFINE_STACK(linkage, type)`.

#### Separate Compilation
```c
// In tasks.h:
#include "data/vec/vec_decl.h"
#include "data/stack.h"
DECLARE_VEC(Task)
DECLARE_STACK(Task)

// In tasks.c:
#include "data/vec/vec_defn.h"
#include "data/stack.h"
DEFINE_VEC(, Task)
DEFINE_STACK(, Task)
```

#### Constructor

**`void canon_stack_T_init(canon_stack_T* s, type* buffer, usize capacity)`**
Initializes stack with caller-owned buffer. `require_msg` if `s == NULL`. O(1).
```c
int buf[128];
canon_stack_int s;
canon_stack_int_init(&s, buf, 128);
```

#### Push / Pop (Result variants)

**`result_bool_Error canon_stack_T_push(canon_stack_T* s, type item)`**
Pushes item onto the top of the stack. O(1).
- `Ok(true)` — pushed
- `Err(ERR_INVALID_ARG)` — `s == NULL` or buffer is NULL
- `Err(ERR_CAPACITY_EXCEEDED)` — stack is full

**`result_bool_Error canon_stack_T_pop(canon_stack_T* s, type* out)`**
Removes and returns top item (most recently pushed). O(1).
- `Ok(true)` — popped into `*out`
- `Err(ERR_INVALID_ARG)` — `s == NULL`, `out == NULL`, or buffer is NULL
- `Err(ERR_INVALID_STATE)` — stack is empty

#### Pop (Option variant)

**`option_T canon_stack_T_pop_option(canon_stack_T* s)`**
Removes and returns top item as `Option<T>`. O(1).
- `Some(item)` — popped
- `None` — empty or invalid

#### Peek

**`bool canon_stack_T_peek(const canon_stack_T* s, type* out)`**
Returns top item without removing it. Returns false if empty or invalid. Stack unchanged. O(1).

**`option_T canon_stack_T_peek_option(const canon_stack_T* s)`**
Returns top item as `Option<T>` without removing. Stack unchanged. O(1).

#### Queries
```c
canon_stack_T_len(&s)        // usize — current element count
canon_stack_T_capacity(&s)   // usize — fixed maximum
canon_stack_T_remaining(&s)  // usize — free slots
canon_stack_T_is_empty(&s)   // bool
canon_stack_T_is_full(&s)    // bool
```
All NULL-safe — return 0 or true if `s == NULL`.

#### Misc

**`void canon_stack_T_clear(canon_stack_T* s)`**
Resets stack to empty. O(1). Does NOT zero buffer contents. NULL-safe.

#### Type Layout

`canon_stack_T` is a typedef alias for `canon_vec_T` — no new struct. All `canon_vec_T` functions remain usable. `DEFINE_STACK` only adds LIFO-named wrappers for clarity.

#### Quick Start
```c
DEFINE_VEC(static inline, int)
DEFINE_STACK(static inline, int)

int buf[128];
canon_stack_int s;
canon_stack_int_init(&s, buf, 128);

canon_stack_int_push(&s, 10);
canon_stack_int_push(&s, 20);

int val;
canon_stack_int_pop(&s, &val);              // val = 20 (LIFO)

option_int top = canon_stack_int_pop_option(&s);   // Some(10)
option_int peek = canon_stack_int_peek_option(&s); // None — now empty
```

> **Known Limitations:**
> - `DEFINE_VEC(linkage, type)` must be called before `DEFINE_STACK(linkage, type)` — missing this causes compile errors.
> - `option_##type` must be instantiated via `CANON_OPTION(type)` before calling `DEFINE_VEC` — not enforced automatically.
> - Fixed capacity — no growth. For auto-growing stacks, wrap `dynvec.h`.
> - `clear()` does not zero buffer contents — stale data remains readable until overwritten. Zero manually for sensitive data.
> - Not thread-safe — concurrent modifications require external synchronization.
> - Not suitable for FIFO access (use queue.h), double-ended access (use deque.h), or random access by index (use vec directly).


### `stringbuf.h`
> Fixed-capacity incremental string builder. Always null-terminated. Arena-backed or caller-owned buffer. No automatic growth — all appends return `bool` and fail gracefully. Zero hidden state, no global variables.

#### Setup
```c
#include "data/stringbuf.h"
```

No macro instantiation required — `StringBuf` is a concrete struct, not a generated type.

#### Constructors

**`bool stringbuf_init_arena(StringBuf* sb, Arena* arena, usize initial_cap)`**
Allocates buffer from arena. Returns false on failure. `initial_cap` must be > 1 (includes null terminator). O(1).
```c
StringBuf sb;
if (!stringbuf_init_arena(&sb, &arena, 1024)) { /* handle failure */ }
```

**`void stringbuf_init_buffer(StringBuf* sb, char* buffer, usize cap)`**
Wraps a caller-owned buffer. No allocation. `cap` must be > 1. O(1).
```c
char buf[256];
StringBuf path;
stringbuf_init_buffer(&path, buf, sizeof(buf));
```

#### Append Operations

All append functions return `false` on failure and leave the buffer unchanged. NULL-safe on `sb`.

**`bool stringbuf_append(StringBuf* sb, const char* s)`**
Appends a null-terminated C string. NULL `s` is a no-op — returns true. O(strlen(s)).

**`bool stringbuf_append_str(StringBuf* sb, str_t s)`**
Appends a `str_t` view directly — no `strlen` scan needed. O(s.len).

**`bool stringbuf_append_char(StringBuf* sb, char c)`**
Appends a single character. O(1).

**`bool stringbuf_append_fmt(StringBuf* sb, const char* fmt, ...)`**
printf-style formatted append. Two `vsnprintf` passes — measure then write. Returns false on format error or insufficient space. O(n).
```c
stringbuf_append_fmt(&sb, "value=%d", 42);
```

**`bool stringbuf_append_fmt_va(StringBuf* sb, const char* fmt, va_list args)`**
Same as `append_fmt` but accepts an existing `va_list`. Caller manages `va_start`/`va_end`. O(n).

**`bool stringbuf_append_n(StringBuf* sb, const char* s, usize n)`**
Appends at most `n` characters. Stops early at null terminator. NULL `s` is a no-op. O(min(n, strlen(s))).
```c
stringbuf_append_n(&sb, "hello world", 5);  // appends "hello"
```

**`stringbuf_printf`** — Alias macro for `stringbuf_append_fmt`.

#### Access & Views

**`const char* stringbuf_str(const StringBuf* sb)`**
Returns null-terminated C string pointer. Never returns NULL — returns `""` if `sb == NULL` or uninitialized. Do NOT free. O(1).

**`str_t stringbuf_as_str(const StringBuf* sb)`**
Returns a `str_t` non-owning view over current contents `[data, data+len)`. Do NOT free `ptr`. O(1).

**`bytes_t stringbuf_as_bytes(const StringBuf* sb)`**
Returns `bytes_t` view over `[data, data+len)` — does NOT include the null terminator. O(1).

**`bytes_t stringbuf_buffer_bytes(const StringBuf* sb)`**
Returns `bytes_t` view over entire buffer `[data, data+capacity)` — includes used, null terminator, and free space. O(1).

#### Queries
```c
stringbuf_len(&sb)              // usize — current string length (excluding '\0')
stringbuf_capacity(&sb)         // usize — total buffer size (including '\0')
stringbuf_remaining(&sb)        // usize — characters still appendable
stringbuf_is_empty(&sb)         // bool  — true if len == 0
stringbuf_is_full(&sb)          // bool  — true if no more chars can be appended
stringbuf_is_arena_backed(&sb)  // bool  — true if allocated from an arena
```
All NULL-safe — return 0 or false if `sb == NULL`.

#### Mutation

**`void stringbuf_clear(StringBuf* sb)`**
Resets to empty. Sets `len = 0` and writes `'\0'` at index 0. Does NOT free or zero buffer. O(1).

**`void stringbuf_truncate(StringBuf* sb, usize new_len)`**
Truncates to `new_len` characters. No-op if `new_len >= len`. O(1).

#### Struct Layout
```c
typedef struct {
    Arena* arena;    // backing arena (NULL = caller-owned buffer)
    char*  data;     // buffer pointer (always null-terminated when valid)
    usize  len;      // current string length (excluding '\0')
    usize  capacity; // total buffer size including space for '\0'
} StringBuf;
// Invariant: data[len] == '\0' always
// Invariant: len < capacity always
```

#### Performance

| Operation | Time | Notes |
|---|---|---|
| `append` | O(strlen(s)) | |
| `append_str` | O(s.len) | No strlen scan |
| `append_char` | O(1) | |
| `append_fmt` | O(n) | Two vsnprintf passes |
| `append_n` | O(min(n, strlen(s))) | |
| All views | O(1) | Zero-copy |
| All queries | O(1) | |
| `clear` / `truncate` | O(1) | |

#### Quick Start
```c
// Arena-backed (recommended)
StringBuf sb;
stringbuf_init_arena(&sb, &arena, 1024);
stringbuf_append(&sb, "Hello, ");
stringbuf_append_fmt(&sb, "%s!", name);
printf("%s\n", stringbuf_str(&sb));

// Stack-backed (zero allocation)
char buf[256];
StringBuf path;
stringbuf_init_buffer(&path, buf, sizeof(buf));
stringbuf_append(&path, "/home/user/");
stringbuf_append(&path, filename);

// Views
str_t   sv = stringbuf_as_str(&sb);   // borrowed, no copy
bytes_t bv = stringbuf_as_bytes(&sb); // raw byte view
```

> **Known Limitations:**
> - Capacity is fixed — once full, all appends fail. No reserve, grow, or realloc. By design.
> - `capacity` includes the null terminator — usable characters = `capacity - 1`.
> - Arena-backed buffers become invalid after `arena_reset()`.
> - `stringbuf_clear()` does NOT zero buffer contents — stale data remains readable until overwritten.
> - `append_fmt` makes two `vsnprintf` passes — for tight loops prefer `append_str` or `append_n` with pre-formatted strings.
> - `stringbuf_str()` returns `""` for NULL or uninitialized — callers cannot distinguish the two.
> - `as_str` / `as_bytes` views are invalidated by any append, clear, or truncate operation.
> - Not thread-safe — concurrent modifications require external synchronization.


### `any_all.h`
> Predicate testing over sequences — existential and universal quantification. Short-circuits on first match or failure. No allocation, O(1) space.

#### Generic Interface

**`bool algo_any(const void* base, usize len, usize elem_size, algo_pred_fn pred, void* ctx)`**
Returns `true` if any element satisfies the predicate. Short-circuits on first match.
```c
algo_any(arr, 6, sizeof(int), is_negative, NULL)  // → true if any < 0
```

**`bool algo_all(const void* base, usize len, usize elem_size, algo_pred_fn pred, void* ctx)`**
Returns `true` if all elements satisfy the predicate. Short-circuits on first failure.
```c
algo_all(arr, 6, sizeof(int), is_positive, NULL)  // → false if any <= 0
```

Both return `false` immediately if `base == NULL`, `pred == NULL`, `len == 0`, or `elem_size == 0`.

#### Typed Macros

**`ALGO_ANY_TYPED(items, len, Type, pred, ctx)`**
Type-safe ANY — automatically passes `sizeof(Type)`.

**`ALGO_ALL_TYPED(items, len, Type, pred, ctx)`**
Type-safe ALL — automatically passes `sizeof(Type)`.
```c
ALGO_ANY_TYPED(arr, 6, int, is_negative, NULL)
ALGO_ALL_TYPED(arr, 6, int, is_positive, NULL)
```

#### Slice Variants — `DEFINE_ALGO_ANY_ALL(type)`

Requires `DEFINE_SLICE(type)`. Generates:
```c
bool algo_any_slice_##type(slice_##type sv, algo_pred_fn pred, void* ctx)
bool algo_all_slice_##type(slice_##type sv, algo_pred_fn pred, void* ctx)
```
```c
DEFINE_SLICE(int)
DEFINE_ALGO_ANY_ALL(int)

slice_int sv = slice_int_from(arr, 6);
algo_any_slice_int(sv, is_negative, NULL)
algo_all_slice_int(sv, is_positive, NULL)
```

> **Known Limitations:**
> - Both return `false` for empty sequences — no vacuous truth for `algo_all`.
> - `elem_size == 0` triggers `require_msg` — always-on panic.
> - Predicates receive a `const void*` — cast inside the predicate.
> - Not thread-safe if the array is being modified concurrently.


### `filter.h`
> Copies elements matching a predicate into a caller-provided output buffer. Preserves relative order (stable). Truncates safely when output is full. No allocation, O(n) time.

#### Generic Interface

**`usize algo_filter(const void* base, usize len, usize elem_size, algo_pred_fn pred, void* ctx, void* out, usize out_cap)`**
Iterates all input elements. Copies matching ones into `out`. Stops writing when `out_cap` is reached but continues iterating input.
```c
int in[] = {-1, 2, -3, 4, 0, 5};
int out[6];
usize n = algo_filter(in, 6, sizeof(int), is_positive, NULL, out, 6);
// n = 3, out = {2, 4, 5}
```

Returns `0` if `base == NULL`, `pred == NULL`, `out == NULL`, `len == 0`, `elem_size == 0`, or `out_cap == 0`.

#### Typed Macro

**`ALGO_FILTER_TYPED(out, out_cap, in, in_len, Type, pred, ctx)`**
Type-safe filter — automatically passes `sizeof(Type)`. `out_cap` is explicit to avoid the `sizeof(array)/sizeof(array[0])` trap.
```c
usize n = ALGO_FILTER_TYPED(out, 6, in, 6, int, is_positive, NULL);
// n = 3, out = {2, 4, 5}
```

#### Slice Variant — `DEFINE_ALGO_FILTER(type)`

Requires `DEFINE_SLICE(type)`. Generates:
```c
usize algo_filter_slice_##type(slice_##type sv, type* out, usize out_cap, algo_pred_fn pred, void* ctx)
```
```c
DEFINE_SLICE(int)
DEFINE_ALGO_FILTER(int)

slice_int sv = slice_int_from(in, 6);
usize n = algo_filter_slice_int(sv, out, 6, is_positive, NULL);
// n = 3, out = {2, 4, 5}
```

#### Performance

| Operation | Time | Space |
|---|---|---|
| `algo_filter` | O(n) — all elements checked | O(1) |

> **Known Limitations:**
> - Output is truncated silently when `out_cap` is reached — no error returned.
> - `elem_size == 0` triggers `require_msg` — always-on panic.
> - Input and output buffers must not overlap.
> - Not thread-safe if the array is being modified concurrently.


### `find.h`
> Locate the first element in a sequence matching a predicate. Short-circuits on first match. Returns index and/or pointer to match. Slice variants return `option_##type`.

#### Generic Interface

**`bool algo_find(const void* base, usize len, usize elem_size, algo_pred_fn pred, void* ctx, usize* out_index, const void** out_elem)`**
Linear search with short-circuit on first match. Both `out_index` and `out_elem` are optional — pass `NULL` to ignore either.
```c
usize idx;
const void* elem;
bool found = algo_find(arr, 4, sizeof(int), is_negative, NULL, &idx, &elem);
if (found) printf("Found at index %zu: %d\n", idx, *(const int*)elem);
```

Returns `false` immediately if `base == NULL`, `pred == NULL`, `len == 0`, or `elem_size == 0`.

#### Typed Macro

**`ALGO_FIND_TYPED(base, len, Type, pred, ctx, out_index, out_elem_ptr)`**
Type-safe find — avoids `void**` cast at call site. `out_elem_ptr` is `const Type**`.
```c
const int* p = NULL;
usize idx;
if (ALGO_FIND_TYPED(arr, 4, int, is_negative, NULL, &idx, &p)) {
    printf("Found at index %zu: %d\n", idx, *p);
}
```

#### Slice Variants — `DEFINE_ALGO_FIND(type)`

Requires `DEFINE_SLICE(type)` and `CANON_OPTION(type)`. Generates:
```c
option_##type algo_find_slice_##type(slice_##type sv, algo_pred_fn pred, void* ctx)
usize         algo_find_index_slice_##type(slice_##type sv, algo_pred_fn pred, void* ctx)
```
```c
DEFINE_SLICE(int)
CANON_OPTION(int)
DEFINE_ALGO_FIND(int)

slice_int sv = slice_int_from(arr, 4);

option_int opt = algo_find_slice_int(sv, is_negative, NULL);
if (opt.has_value) printf("Found: %d\n", opt.value);

usize idx = algo_find_index_slice_int(sv, is_negative, NULL);
if (idx != CANON_USIZE_MAX) printf("Found at index: %zu\n", idx);
```

#### Performance

| Operation | Time | Short-circuit |
|---|---|---|
| `algo_find` | O(k) best, O(n) worst | ✅ stops at first match |

> **Known Limitations:**
> - `algo_find_index_slice_##type` returns `CANON_USIZE_MAX` when not found — check before use.
> - `out_index` and `out_elem` are not modified on failure.
> - `elem_size == 0` triggers `require_msg` — always-on panic.
> - `CANON_OPTION(type)` must be instantiated before `DEFINE_ALGO_FIND` — missing it produces cryptic compile errors.
> - Not thread-safe if the array is being modified concurrently.


### `fold.h`
> Functional reduction of sequences to single values (fold/reduce). Supports infallible and fallible variants. Accumulator is caller-owned and caller-initialized. No allocation, O(n) time.

#### Typed Macros

**`ALGO_FOLD(acc_ptr, array, len, Type, fold_fn, ctx)`**
Infallible fold — applies `fold_fn` to each element, accumulating into `*acc_ptr`. No-op if any pointer is NULL.
```c
void sum_ints(int* acc, const int* elem, void* ctx) { *acc += *elem; }

int numbers[] = {1, 2, 3, 4, 5};
int total = 0;
ALGO_FOLD(&total, numbers, 5, int, sum_ints, NULL);
// total = 15
```

**`ALGO_FOLD_RESULT(acc_ptr, array, len, Type, fold_fn, ctx)`** *(GNU C / C23 only)*
Fallible fold — stops immediately on first error. Returns `result_bool_Error`.
Disable with `#define CANON_NO_GNU_EXTENSIONS` to use C99 fallback.
```c
result_bool_Error sum_positive(int* acc, const int* elem, void* ctx) {
    if (*elem < 0) return result_bool_Error_err(ERR_INVALID_ARG);
    *acc += *elem;
    return result_bool_Error_ok(true);
}

int vals[] = {1, 2, -3, 4};
int sum = 0;
result_bool_Error r = ALGO_FOLD_RESULT(&sum, vals, 4, int, sum_positive, NULL);
// stopped at index 2, sum = 3
```

#### Vec Convenience Macros

**`ALGO_FOLD_VEC(acc_ptr, vec, Type, fold_fn, ctx)`**
Infallible fold over any container with `.items` and `.len` fields.

**`ALGO_FOLD_RESULT_VEC(acc_ptr, vec, Type, fold_fn, ctx)`**
Fallible fold over any container with `.items` and `.len` fields.
```c
ALGO_FOLD_VEC(&total, my_vec, int, sum_ints, NULL);
```

#### Slice Variants — `DEFINE_ALGO_FOLD(type)`

Requires `DEFINE_SLICE(type)`. Generates:
```c
void             algo_fold_slice_##type(void* acc_ptr, slice_##type sv, void (*fn)(void*, const type*, void*), void* ctx)
result_bool_Error algo_fold_result_slice_##type(void* acc_ptr, slice_##type sv, result_bool_Error (*fn)(void*, const type*, void*), void* ctx)
```
```c
DEFINE_SLICE(int)
DEFINE_ALGO_FOLD(int)

int arr[] = {1, 2, 3, 4, 5};
slice_int sv = slice_int_from(arr, 5);
int total = 0;
algo_fold_slice_int(&total, sv, sum_ints, NULL);
// total = 15
```

#### Performance

| Operation | Time | Space |
|---|---|---|
| `ALGO_FOLD` | O(n) | O(1) |
| `ALGO_FOLD_RESULT` | O(k) best, O(n) worst | O(1) |

> **Known Limitations:**
> - `ALGO_FOLD_RESULT` requires GNU C statement expressions or C23 — use `#define CANON_NO_GNU_EXTENSIONS` for C99 fallback.
> - Accumulator may contain a partial result on error — always check the `Result` before using it.
> - Caller must initialize `*acc_ptr` before calling — functions do not reset it.
> - `CANON_RESULT(bool, Error)` is instantiated automatically inside `fold.h` — do not instantiate it again in the same translation unit.
> - Not thread-safe if the array or accumulator is being modified concurrently.


### `map.h`
> Element-wise transformations over sequences. Supports same-type and cross-type mapping. Output buffer is caller-provided. No allocation, O(n) time.

#### Typed Macros

**`ALGO_MAP_TYPED(out_array, in_array, len, OutType, InType, fn)`**
Maps each element of `in_array` through `fn` into `out_array`. Input and output types may differ. No-op if any pointer is NULL.
```c
void to_double(double* out, const int* in) { *out = (double)(*in); }

int input[] = {1, 2, 3, 4, 5};
double output[5];
ALGO_MAP_TYPED(output, input, 5, double, int, to_double);
// output = {1.0, 2.0, 3.0, 4.0, 5.0}
```

**`ALGO_MAP_INPLACE(array, len, Type, fn)`**
Transforms each element of `array` in place. Original values are overwritten.
```c
void increment(int* elem) { (*elem)++; }

int arr[] = {10, 20, 30};
ALGO_MAP_INPLACE(arr, 3, int, increment);
// arr = {11, 21, 31}
```

#### Vec Convenience Macros

**`ALGO_MAP_VEC(out_vec, in_vec, OutType, InType, fn)`**
Maps between two containers with `.items` and `.len` fields. Processes `min(out_vec.len, in_vec.len)` elements.

**`ALGO_MAP_INPLACE_VEC(vec, Type, fn)`**
In-place map over a container with `.items` and `.len` fields.
```c
ALGO_MAP_VEC(output_vec, input_vec, double, int, to_double);
ALGO_MAP_INPLACE_VEC(my_vec, int, increment);
```

#### Slice Variants — `DEFINE_ALGO_MAP(in_type, out_type)`

Requires `DEFINE_SLICE(in_type)` and `DEFINE_SLICE(out_type)`. Generates:
```c
void algo_map_slice_##in_type##_##out_type(slice_##out_type out_sv, slice_##in_type in_sv, void (*fn)(out_type*, const in_type*))
void algo_map_inplace_slice_##in_type(slice_##in_type sv, void (*fn)(in_type*))
```
```c
DEFINE_SLICE(int)
DEFINE_SLICE(double)
DEFINE_ALGO_MAP(int, double)

int arr[] = {1, 2, 3, 4, 5};
double result[5];
slice_int sv_in = slice_int_from(arr, 5);
slice_double sv_out = slice_double_from(result, 5);

algo_map_slice_int_double(sv_out, sv_in, to_double);
// result = {1.0, 2.0, 3.0, 4.0, 5.0}

// In-place (same type)
DEFINE_ALGO_MAP(int, int)
algo_map_inplace_slice_int(sv_in, increment);
```

#### Performance

| Operation | Time | Space |
|---|---|---|
| `ALGO_MAP_TYPED` | O(n) | O(1) |
| `ALGO_MAP_INPLACE` | O(n) | O(1) |

> **Known Limitations:**
> - `ALGO_MAP_TYPED` does not check if `out_array` has sufficient capacity — caller must ensure it.
> - `ALGO_MAP_INPLACE` overwrites original data — use `ALGO_MAP_TYPED` if the original must be preserved.
> - `ALGO_MAP_VEC` uses `min(out_vec.len, in_vec.len)` — set `out_vec.len` before calling to control how many elements are processed.
> - `DEFINE_ALGO_MAP(in_type, out_type)` generates `algo_map_inplace_slice_##in_type` regardless of `out_type` — calling it twice with the same `in_type` causes duplicate definition errors.
> - Not thread-safe if arrays are being modified concurrently.


### `reverse.h`
> In-place sequence reversal using two-pointer technique. Supports palindrome checking. No allocation, O(n) time, O(1) space.

#### Generic Interface

**`void algo_reverse(void* array, usize len, usize elem_size)`**
Reverses elements in-place. Uses a fixed 256-byte stack buffer for swapping — no heap allocation.
```c
int scores[] = {10, 20, 30, 40, 50};
algo_reverse(scores, 5, sizeof(int));
// scores = {50, 40, 30, 20, 10}
```

Does nothing if `array == NULL` or `len < 2`.

**`bool algo_is_palindrome(const void* array, usize len, usize elem_size, algo_cmp_fn cmp, void* ctx)`**
Returns `true` if the array reads the same forwards and backwards. Non-destructive.
```c
int arr[] = {1, 2, 3, 2, 1};
bool is_pal = algo_is_palindrome(arr, 5, sizeof(int), algo_cmp_int, NULL);
// is_pal = true
```

Returns `true` for `NULL`, empty, or single-element arrays.

#### Typed Macros

**`ALGO_REVERSE_TYPED(array, len, Type)`**
Type-safe in-place reverse — automatically passes `sizeof(Type)`.
```c
ALGO_REVERSE_TYPED(scores, 5, int);
```

**`ALGO_IS_PALINDROME_TYPED(array, len, Type, cmp, ctx)`** *(GNU C / C23 — evaluates to `bool`)*
Type-safe palindrome check. C99 fallback available when `CANON_NO_GNU_EXTENSIONS` is defined.
```c
bool result = ALGO_IS_PALINDROME_TYPED(arr, 5, int, algo_cmp_int, NULL);
```

#### Slice Variants — `DEFINE_ALGO_REVERSE(type)`

Requires `DEFINE_SLICE(type)`. Generates:
```c
void algo_reverse_slice_##type(slice_##type sv)
bool algo_is_palindrome_slice_##type(slice_##type sv, algo_cmp_fn cmp, void* ctx)
```
```c
DEFINE_SLICE(int)
DEFINE_ALGO_REVERSE(int)

int arr[] = {1, 2, 3, 4, 5};
slice_int sv = slice_int_from(arr, 5);

algo_reverse_slice_int(sv);
// arr = {5, 4, 3, 2, 1}

bool is_pal = algo_is_palindrome_slice_int(sv, algo_cmp_int, NULL);
```

#### Swap Buffer
```c
// Default: 256 bytes — handles most types
// Override before including:
#define ALGO_REVERSE_SWAP_BUF_SIZE 512
#include "algo/reverse.h"
```

`elem_size > ALGO_REVERSE_SWAP_BUF_SIZE` triggers `require_msg` — always-on panic.

#### Performance

| Operation | Time | Space |
|---|---|---|
| `algo_reverse` | O(n) — ⌊n/2⌋ swaps | O(1) stack buffer |
| `algo_is_palindrome` | O(n/2) | O(1) |

> **Known Limitations:**
> - `elem_size` must not exceed `ALGO_REVERSE_SWAP_BUF_SIZE` — triggers `require_msg` if violated.
> - `ALGO_IS_PALINDROME_TYPED` uses GNU C statement expressions in its expression form — use `#define CANON_NO_GNU_EXTENSIONS` for C99 fallback.
> - Slice reversal modifies the underlying array — the slice is a non-owning view, not a copy.
> - Not thread-safe if the array is being modified concurrently.


### `search.h`
> Binary search utilities for sorted sequences. O(log n) lookup. Requires input to be sorted with the same comparator used for searching.

#### Generic Interface

**`usize algo_lower_bound(const void* array, usize len, usize elem_size, const void* key, algo_cmp_fn cmp, void* ctx)`**
Returns the index of the first exact match, or `CANON_USIZE_MAX` if not found. Array must be sorted.
```c
int numbers[] = {1, 3, 5, 7, 9, 11};
int key = 7;
usize idx = algo_lower_bound(numbers, 6, sizeof(int), &key, algo_cmp_int, NULL);
// idx = 3
```

Returns `CANON_USIZE_MAX` if `array == NULL`, `key == NULL`, `cmp == NULL`, or `len == 0`.

#### Typed Macro

**`ALGO_LOWER_BOUND_TYPED(array, len, Type, key, cmp, ctx)`**
Type-safe lower bound — automatically passes `sizeof(Type)`.
```c
usize idx = ALGO_LOWER_BOUND_TYPED(numbers, 6, int, &key, algo_cmp_int, NULL);
```

#### Slice Variant — `DEFINE_ALGO_SEARCH(type)`

Requires `DEFINE_SLICE(type)` and `CANON_OPTION(type)`. Generates:
```c
usize      algo_lower_bound_slice_##type(slice_##type sv, const type* key, algo_cmp_fn cmp, void* ctx)
bool       algo_binary_search_slice_##type(slice_##type sv, const type* key, algo_cmp_fn cmp, void* ctx)
```
```c
DEFINE_SLICE(int)
CANON_OPTION(int)
DEFINE_ALGO_SEARCH(int)

slice_int sv = slice_int_from(numbers, 6);
usize idx = algo_lower_bound_slice_int(sv, &key, algo_cmp_int, NULL);
bool found = algo_binary_search_slice_int(sv, &key, algo_cmp_int, NULL);
```

#### Search Variants Explained
```
Array: [1, 2, 2, 2, 5, 7, 9]  searching for key=2
        0  1  2  3  4  5  6

lower_bound(2) = 1  — first index where array[i] >= key
upper_bound(2) = 4  — first index where array[i] > key
equal_range(2) = [1, 4)  — all elements equal to key
binary_search(2) = true  — exact match exists
```

#### Important: Arrays Must Be Sorted

Always use the **same comparator** for sorting and searching:
```c
// ✓ Correct
algo_sort(arr, len, sizeof(int), algo_cmp_int, NULL, tmp);
algo_lower_bound(arr, len, sizeof(int), &key, algo_cmp_int, NULL);

// ✗ Wrong — different comparators produce incorrect results
algo_sort(arr, len, sizeof(int), algo_cmp_int, NULL, tmp);
algo_lower_bound(arr, len, sizeof(int), &key, algo_cmp_int_desc, NULL);
```

#### Performance

| Operation | Time | Space |
|---|---|---|
| `algo_lower_bound` | O(log n) | O(1) |

> **Known Limitations:**
> - Array must be sorted with the same comparator — unsorted input produces incorrect results, not crashes.
> - Returns `CANON_USIZE_MAX` for both "not found" and invalid input — callers cannot distinguish the two.
> - `elem_size == 0` triggers `require_msg` — always-on panic.
> - `cmp == NULL` triggers `require_msg` — always-on panic.
> - Not thread-safe if the array is being modified concurrently.


### `sort.h`
> Stable in-place hybrid sort — insertion sort for small arrays (len < 16), merge sort for large ones (len ≥ 16). Caller provides temp buffer for merge sort. No internal allocation.

#### Generic Interface

**`void algo_sort(void* base, usize len, usize elem_size, algo_cmp_fn cmp, void* ctx, void* temp_buffer)`**
Sorts array in-place. Pass `temp_buffer` of `len * elem_size` bytes for merge sort. Pass `NULL` to fall back to O(n²) insertion sort for any length.
```c
int arr[] = {64, 34, 25, 12, 22, 11, 90};
int tmp[7];
algo_sort(arr, 7, sizeof(int), algo_cmp_int, NULL, tmp);
// arr = {11, 12, 22, 25, 34, 64, 90}
```

Does nothing if `base == NULL`, `len < 2`, `cmp == NULL`, or `elem_size == 0`.

**`bool algo_is_sorted(const void* base, usize len, usize elem_size, algo_cmp_fn cmp, void* ctx)`**
Returns `true` if array is sorted according to `cmp`. Non-destructive.
```c
bool sorted = algo_is_sorted(arr, 7, sizeof(int), algo_cmp_int, NULL);
// sorted = true
```

Returns `true` for `NULL`, empty, or single-element arrays.

#### Typed Macros

**`ALGO_SORT_TYPED(base, len, Type, cmp, ctx)`**
Type-safe sort with automatic stack temp buffer. Uses merge sort for `len <= ALGO_SORT_STACK_TEMP_MAX` (default 128), falls back to insertion sort for larger arrays.
```c
ALGO_SORT_TYPED(arr, 7, int, algo_cmp_int, NULL);
```

**`ALGO_IS_SORTED_TYPED(base, len, Type, cmp, ctx)`**
Type-safe sorted check — evaluates to `bool`. C99-portable.
```c
bool sorted = ALGO_IS_SORTED_TYPED(arr, 7, int, algo_cmp_int, NULL);
```

#### Stack Temp Buffer Limit
```c
// Default: 128 elements
// Override before including:
#define ALGO_SORT_STACK_TEMP_MAX 256
#include "algo/sort.h"
```

For arrays larger than `ALGO_SORT_STACK_TEMP_MAX`, call `algo_sort()` directly with a heap or arena buffer.

#### Slice Variants — `DEFINE_ALGO_SORT(type)`

Requires `DEFINE_SLICE(type)`. Generates:
```c
void algo_sort_slice_##type(slice_##type sv, algo_cmp_fn cmp, void* ctx, type* temp, usize temp_cap)
bool algo_is_sorted_slice_##type(slice_##type sv, algo_cmp_fn cmp, void* ctx)
```

If `temp == NULL` or `temp_cap < sv.len`, falls back to insertion sort.
```c
DEFINE_SLICE(int)
DEFINE_ALGO_SORT(int)

int arr[] = {64, 34, 25, 12, 22, 11, 90};
int tmp[7];
slice_int sv = slice_int_from(arr, 7);

algo_sort_slice_int(sv, algo_cmp_int, NULL, tmp, 7);
// arr = {11, 12, 22, 25, 34, 64, 90}

bool sorted = algo_is_sorted_slice_int(sv, algo_cmp_int, NULL);
```

#### Algorithm Selection

| Condition | Algorithm | Time | Stable |
|---|---|---|---|
| `len < 2` | no-op | O(1) | — |
| `len < 16` | insertion sort | O(n²) worst, O(n) best | ✅ |
| `len ≥ 16`, `temp != NULL` | merge sort | O(n log n) | ✅ |
| `len ≥ 16`, `temp == NULL` | insertion sort fallback | O(n²) | ✅ |

> **Known Limitations:**
> - `ALGO_SORT_TYPED` falls back to insertion sort for arrays larger than `ALGO_SORT_STACK_TEMP_MAX` — call `algo_sort()` directly with a caller-managed buffer for large arrays.
> - `temp_buffer` and `base` must not overlap — undefined behavior if they do.
> - Inner swap uses a fixed 256-byte stack buffer — elements larger than 256 bytes fall back to a byte-by-byte loop.
> - Recursion depth is O(log n) — not suitable for extremely deep recursion-constrained environments.
> - Not thread-safe if the array is being modified concurrently.


### `unique.h`
> Removes consecutive duplicate elements in-place. Single linear pass, O(n) time, O(1) space. Most powerful after sorting — enables full deduplication.

#### Generic Interface

**`usize algo_unique(void* array, usize len, usize elem_size, algo_cmp_fn cmp, void* ctx)`**
Collapses consecutive equal elements into one. Returns the new logical length of the unique prefix. Elements beyond the returned length contain stale data.
```c
int arr[] = {1, 1, 2, 2, 3, 3, 5, 5};
usize new_len = algo_unique(arr, 8, sizeof(int), algo_cmp_int, NULL);
// arr[0..new_len-1] = {1, 2, 3, 5}, new_len = 4
```

Returns `len` immediately if `array == NULL` or `len <= 1`.

#### Full Deduplication Pattern

Sort first, then unique — always use the same comparator:
```c
int arr[] = {5, 2, 3, 2, 1, 3, 5, 5};
int tmp[8];

// Step 1: sort
algo_sort(arr, 8, sizeof(int), algo_cmp_int, NULL, tmp);
// arr = {1, 1, 2, 2, 3, 3, 5, 5, 5}

// Step 2: unique
usize new_len = algo_unique(arr, 8, sizeof(int), algo_cmp_int, NULL);
// arr[0..new_len-1] = {1, 2, 3, 5}, new_len = 4
```

#### Typed Macros

**`ALGO_UNIQUE_TYPED(array, len, Type, cmp, ctx)`** *(GNU C / C23 — returns new length)*
Type-safe unique — automatically passes `sizeof(Type)`.
```c
usize new_len = ALGO_UNIQUE_TYPED(arr, 5, int, algo_cmp_int, NULL);
```

C99 fallback when `CANON_NO_GNU_EXTENSIONS` is defined — updates length via pointer:
```c
usize len = 5;
ALGO_UNIQUE_TYPED(arr, &len, int, algo_cmp_int, NULL);
// len updated in place
```

#### Slice Variant — `DEFINE_ALGO_UNIQUE(type)`

Requires `DEFINE_SLICE(type)`. Generates:
```c
usize algo_unique_slice_##type(slice_##type sv, algo_cmp_fn cmp, void* ctx)
```
```c
DEFINE_SLICE(int)
DEFINE_ALGO_UNIQUE(int)

slice_int sv = slice_int_from(arr, 8);
usize new_len = algo_unique_slice_int(sv, algo_cmp_int, NULL);
// use sv.ptr[0..new_len-1]
```

#### Performance

| Operation | Time | Space |
|---|---|---|
| `algo_unique` | O(n) — single pass | O(1) |

> **Known Limitations:**
> - Removes **consecutive** duplicates only — sort first for full deduplication.
> - Elements beyond the returned length contain stale data — do not read them.
> - `cmp == NULL` triggers `require_msg` — always-on panic.
> - `elem_size == 0` triggers `require_msg` — always-on panic.
> - `ALGO_UNIQUE_TYPED` requires GNU C statement expressions or C23 for the return-value form — use `#define CANON_NO_GNU_EXTENSIONS` for C99 fallback.
> - Not thread-safe if the array is being modified concurrently.











