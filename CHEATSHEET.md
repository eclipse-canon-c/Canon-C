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
bits_rotr(0b10110001, 2)  // → 0b01101100  (8-bit context)
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
> Overflow-safe add, sub, mul for all integer sizes. Returns `true` on success, `false` on overflow. Also includes alignment helpers and min/max/clamp macros.

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

#### Alignment

**`usize align_up(usize value, usize alignment)`**
Rounds `value` up to the next multiple of `alignment`.
```c
align_up(17, 16)  // → 32
align_up(16, 16)  // → 16
```

**`usize align_down(usize value, usize alignment)`**
Rounds `value` down to the nearest multiple of `alignment`.
```c
align_down(17, 16)  // → 16
```

**`bool is_aligned(usize value, usize alignment)`**
Returns `true` if `value` is a multiple of `alignment`.
```c
is_aligned(32, 16)  // → true
is_aligned(17, 16)  // → false
```

**`bool is_ptr_aligned(const void* ptr, usize alignment)`**
Same as `is_aligned` but takes a pointer.
```c
is_ptr_aligned(buf, 64)  // → true if buf address % 64 == 0
```

> **Note:** All alignment functions require `alignment` to be a power of 2. Not checked at runtime — UB if violated.

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

> **Known Limitations:** Macros — arguments may be evaluated more than once; don't put function calls or `i++` inside them.

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
> Named constants for integer limits, alignment, sizes, capacity limits, and growth factors. All compile-time.

#### Integer Limits

| Constant | Value |
|---|---|
| `CANON_U8_MAX` | 255 |
| `CANON_U16_MAX` | 65,535 |
| `CANON_U32_MAX` | 4,294,967,295 |
| `CANON_U64_MAX` | 18,446,744,073,709,551,615 |
| `CANON_I8_MIN` / `CANON_I8_MAX` | -128 / 127 |
| `CANON_I16_MIN` / `CANON_I16_MAX` | -32,768 / 32,767 |
| `CANON_I32_MIN` / `CANON_I32_MAX` | -2,147,483,648 / 2,147,483,647 |
| `CANON_I64_MIN` / `CANON_I64_MAX` | -9.2e18 / 9.2e18 |
| `CANON_USIZE_MAX` | `SIZE_MAX` (platform-dependent) |
| `CANON_ISIZE_MAX` / `CANON_ISIZE_MIN` | `PTRDIFF_MAX/MIN` (platform-dependent) |

#### Alignment Constants

| Constant | Value | Use |
|---|---|---|
| `CANON_DEFAULT_ALIGN` | 8 (64-bit) / 4 (32-bit) | General-purpose allocations |
| `CANON_CACHE_LINE` | 64 bytes | Prevent false sharing between threads |
| `CANON_SIMD_ALIGN` | 16 bytes | SSE / NEON |
| `CANON_SIMD_ALIGN_AVX` | 32 bytes | AVX (256-bit) |
| `CANON_SIMD_ALIGN_AVX512` | 64 bytes | AVX-512 |
| `CANON_PAGE_SIZE` | 4096 bytes | Page-aligned allocations |

#### Size Constants

```c
CANON_KB  // 1,024
CANON_MB  // 1,048,576
CANON_GB  // 1,073,741,824
```
```c
Arena arena = arena_create(buf, 2 * CANON_MB);
```

#### Capacity Limits

| Constant | Default | Overridable |
|---|---|---|
| `CANON_VEC_MAX_CAPACITY` | 1 GB | ✅ |
| `CANON_STRING_MAX_SIZE` | 16 MB | ✅ |
| `CANON_ARENA_MAX_SIZE` | 1 GB | ✅ |

Override before including the header:
```c
#define CANON_VEC_MAX_CAPACITY (512 * CANON_MB)
#include "limits.h"
```

#### Small Buffer Thresholds

```c
CANON_SSO_THRESHOLD  // 23 bytes — inline string storage
CANON_SVO_THRESHOLD  // 8 elements — inline vector storage
```

#### Growth Factor

```c
CANON_GROWTH_FACTOR_NUM    // 3
CANON_GROWTH_FACTOR_DENOM  // 2
// → 1.5x growth per resize

CANON_MIN_ALLOCATION  // 32 bytes — minimum initial allocation
```
```c
usize new_cap = old_cap * CANON_GROWTH_FACTOR_NUM / CANON_GROWTH_FACTOR_DENOM;
```

#### Pointer Tagging

```c
CANON_PTR_TAG_BITS   // 3 (64-bit) / 2 (32-bit)
CANON_PTR_TAG_MASK   // mask for tag bits
CANON_PTR_ADDR_MASK  // mask for address bits (strips tag)
```
```c
uintptr_t tagged   = ((uintptr_t)ptr & CANON_PTR_ADDR_MASK) | tag;
void*     untagged = (void*)(tagged & CANON_PTR_ADDR_MASK);
usize     tag      = tagged & CANON_PTR_TAG_MASK;
```

#### Platform Info

```c
CANON_POINTER_SIZE   // sizeof(void*) — 4 or 8
CANON_BITS_PER_BYTE  // CHAR_BIT — always 8
CANON_POINTER_BITS   // 32 or 64
```

> **Known Limitations:**
> - `CANON_CACHE_LINE` and `CANON_PAGE_SIZE` are hardcoded constants, not queried at runtime — they're correct for most x86/ARM64 systems but not guaranteed on all platforms.
> - `CANON_GB` can overflow on 32-bit systems since `usize` would be 32-bit, making 1GB the maximum representable value.
> - Pointer tagging assumes natural alignment — if you're working with packed structs or manually misaligned pointers, `CANON_PTR_TAG_BITS` may not be reliable.

---

### `ptr.h`
> Named pointer arithmetic and alignment. All operations on `void*` — no type punning. NULL-safe unless noted.

#### Integer Alignment

**`usize align_up(usize n, usize align)`**
Rounds `n` up to the next multiple of `align`.
```c
align_up(13, 8)  // → 16
align_up(16, 8)  // → 16
```

**`usize align_down(usize n, usize align)`**
Rounds `n` down to the nearest multiple of `align`.
```c
align_down(13, 8)  // → 8
```

**`usize align_padding(usize n, usize align)`**
Returns bytes needed to align `n` — 0 if already aligned.
```c
align_padding(13, 8)  // → 3  (13 + 3 = 16)
align_padding(16, 8)  // → 0
```

**`bool is_aligned(usize n, usize align)`**
Returns `true` if `n` is a multiple of `align`.
```c
is_aligned(16, 8)  // → true
is_aligned(13, 8)  // → false
```

**`bool is_power_of_two(usize n)`**
Returns `true` if `n` is a power of two. Returns `false` for 0.
```c
is_power_of_two(16)  // → true
is_power_of_two(15)  // → false
```

#### Pointer Alignment

**`void* ptr_align_up(void* p, usize align)`**
**`void* ptr_align_down(void* p, usize align)`**
Rounds pointer up / down to the nearest `align`-byte boundary. Returns `NULL` if `p` is `NULL`.
```c
void* aligned = ptr_align_up(raw, 16);
```

**`bool ptr_is_aligned(const void* p, usize align)`**
Returns `true` if pointer address is a multiple of `align`. Returns `false` for `NULL`.

**`usize ptr_align_padding(const void* p, usize align)`**
Returns bytes until the next aligned address. Returns `0` for `NULL`.

#### Pointer Arithmetic

**`void* ptr_offset(void* p, usize n)`**
**`const void* ptr_offset_const(const void* p, usize n)`**
Advances pointer forward by `n` bytes. Returns `NULL` if `p` is `NULL`.
```c
void* next = ptr_offset(buffer, slot_size);
```

**`void* ptr_retreat(void* p, usize n)`**
Moves pointer backward by `n` bytes. Returns `NULL` if `p` is `NULL`.

**`isize ptr_diff(const void* to, const void* from)`**
Signed byte distance from `from` to `to` (`to - from`). Positive if `to > from`.
```c
isize used = ptr_diff(arena->current, arena->start);
```

**`usize ptr_span(const void* to, const void* from)`**
Unsigned byte distance from `from` to `to`. Panics if `to < from`.

#### Bounds Checking

**`bool ptr_in_range(const void* p, const void* region_start, const void* region_end)`**
Returns `true` if `p` is within `[region_start, region_end)`. Returns `false` if any argument is `NULL`.
```c
bool safe = ptr_in_range(user_ptr, buf, buf + buf_size);
```

**`bool ptr_range_in_range(const void* p, usize len, const void* region_start, const void* region_end)`**
Returns `true` if `[p, p+len)` lies fully within `[region_start, region_end)`. Overflow-safe.

#### Element Access

**`void* ptr_elem(void* base, usize index, usize elem_size)`**
**`const void* ptr_elem_const(const void* base, usize index, usize elem_size)`**
Returns pointer to element `index` in an untyped array. Equivalent to `&((T*)base)[index]`.
```c
void* slot = ptr_elem(pool->buffer, 3, sizeof(MyStruct));
```

#### Null Safety

**`void* ptr_or(void* p, void* fallback)`**
Returns `p` if non-NULL, otherwise returns `fallback`.

**`bool ptr_is_valid(const void* p)`**
Returns `true` if `p` is not `NULL`. Explicit alternative to implicit bool conversion.

#### Macros

**`PTR_OFFSET_OF(type, member)`**
Byte offset of `member` within `type`. Portable replacement for `offsetof`.
```c
usize off = PTR_OFFSET_OF(MyStruct, field);
```

**`PTR_CONTAINER_OF(ptr, type, member)`**
Recovers the enclosing struct pointer from a pointer to one of its members. Used for intrusive data structures.
```c
MyItem* item = PTR_CONTAINER_OF(node_ptr, MyItem, node);
```

> **Known Limitations:** All alignment functions require `align` to be a power of two and greater than 0 — enforced via `require_msg` at runtime. `ptr_elem` multiplies `index * elem_size` without overflow checking — use `checked_mul` first if indices are untrusted.

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
Returns a heap-allocated copy as a plain C string. Caller must `free()` the result. Returns a heap-allocated `""` on NULL or empty input — never returns NULL itself.

#### Performance

| Operation | Time | Notes |
|---|---|---|
| `append` | Amortized O(n) | O(len + n) on realloc |
| `append_char` | Amortized O(1) | O(len) on realloc |
| `append_fmt` | O(n) | Two `vsnprintf` passes |
| `append_n` | Amortized O(n) | O(len + n) on realloc |
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


