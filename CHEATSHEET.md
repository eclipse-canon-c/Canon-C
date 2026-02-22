# Canon-C Cheatsheet

> Quick reference for all Canon-C modules.

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
