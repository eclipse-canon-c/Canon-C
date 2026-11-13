# Canon-C in AUTOSAR Classic

Canon-C can be used in a project built on AUTOSAR Classic (the Classic
Platform, CP). This page says how, what `integrations/autosar/canon_autosar.h`
adds for it, and what neither it nor Canon-C does. Canon-C is not an AUTOSAR
module, and nothing here is certified or qualified: CI checks the adapter
against stand-ins for the AUTOSAR headers, built from the specifications,
never against an AUTOSAR stack (see "What CI checks").

## The adapter

`integrations/autosar/canon_autosar.h` is opt-in: no other Canon-C header
includes it. It needs on the include path the repository's root, its own
directory, and the project's AUTOSAR headers: `Std_Types.h`, which brings
`Platform_Types.h`, and `Det.h`. It gives three things:

- **Checks of the platform's types** against Canon-C's: the build fails
  where `uint8` .. `uint64`, `sint8` .. `sint64`, `float32` or `float64`
  differs in width or signedness from `u8` .. `u64`, `i8` .. `i64`, `f32` or
  `f64`.
- **Conversions**: `canon_autosar_std_return(Error)` gives `E_OK` for
  `ERR_OK` and `E_NOT_OK` for any other code; `canon_autosar_boolean(bool)`
  and `canon_autosar_bool(boolean)` convert between C's `bool` and
  AUTOSAR's `boolean`.
- **Failed contract checks reported to the Default Error Tracer (DET)**,
  then a safe state the project supplies. See the next section.

Define its settings before including it, the same in every translation unit
that includes it:

| Macro | | Meaning |
|-------|---|---------|
| `CANON_AUTOSAR_MODULE_ID` | required | The `ModuleId` of Canon-C's DET reports. AUTOSAR reserves 0 to 254 for basic software modules; complex drivers use 255 or 2048 to 4095 ([SWS DefaultErrorTracer][det], 8.2.1.1). Without it the build stops with an `#error`: no default is right for every project. |
| `CANON_AUTOSAR_INSTANCE_ID` | default 0 | The `InstanceId`. |
| `CANON_AUTOSAR_API_ID` | default 0 | The `ApiId`: one for every Canon-C function, since the handler is not told which function's check failed. |
| `CANON_AUTOSAR_E_CONTRACT` | default 1 | The `ErrorId`. |
| `CANON_AUTOSAR_NO_64BIT_TYPES` | not defined | Skips the check of `uint64` and `sint64`, for a `Platform_Types.h` older than AUTOSAR 4.1.2, which added them ([R4.3.1 PlatformTypes][pt431]). |

The project supplies one function:

```c
void canon_autosar_safe_state(void);
```

It puts the ECU in its safe state, as the project defines it: a reset, or a
shutdown through the ECU State Manager. It must not return. A program that
installs the adapter's handler does not link without it.

A sketch of the pieces, with a project header that every unit using Canon-C
includes:

```c
/* canon_config.h, the project's */
#define CANON_AUTOSAR_MODULE_ID 2050u   /* the project's choice */
#include "canon_autosar.h"

/* in exactly one .c file */
#define CANON_CONTRACT_IMPL             /* defines the handler pointer */
#include "canon_config.h"

void canon_autosar_safe_state(void)
{
    /* the project's safe state: a reset, for one */
    for (;;) { }
}

/* at start-up, before any other Canon-C call */
canon_autosar_init();
```

## Failed contract checks

Canon-C checks its functions' preconditions at run time with `require()`
(`core/primitives/contract.h`), and calls a handler when one fails. The
handler must not return. By default it writes to `stderr` and calls
`abort()`. `canon_autosar_init()` installs `canon_autosar_contract_handler`
in its place, which:

1. reports the failure with `Det_ReportError(CANON_AUTOSAR_MODULE_ID,
   CANON_AUTOSAR_INSTANCE_ID, CANON_AUTOSAR_API_ID,
   CANON_AUTOSAR_E_CONTRACT)`, AUTOSAR's call for development errors, the
   class its own modules' parameter checks belong to;
2. then calls `canon_autosar_safe_state()`;
3. and, if that returns, loops forever, since a contract handler must not
   return.

The author decided both steps on 2026-11-13 (VERIFY-075): `Det_ReportError`,
not `Det_ReportRuntimeError`, and a safe state the integrator supplies, the
build failing without it, not one the adapter picks.

AUTOSAR's DET stops execution in `Det_ReportError`: "Det_ReportError shall
stop execution" (SWS_Det_00026, [R24-11][det]). With such a DET the safe
state is not reached; it is reached with a DET that returns. Canon-C's
description of the failure, its file, line, function and expression, is
dropped: `Det_ReportError` has no place for it.

Points to keep in mind:

- **Install the handler first.** A check that fails before
  `canon_autosar_init()` reaches the default handler, whose `fprintf` and
  `abort` come from the C library.
- **Canon-C's checks do not follow DevErrorDetect.** They are on unless
  `CANON_NO_REQUIRE` is defined, which `contract.h` keeps for builds whose
  formal proof covers every call site.
- **`contract_set_handler()` is not thread-safe.** Call
  `canon_autosar_init()` once, before other tasks run
  (`docs/thread-safety.md`).
- **Interrupts.** `Det_ReportError` may be called from interrupt context
  ([SWS DefaultErrorTracer][det]). If Canon-C runs in an interrupt service
  routine, `canon_autosar_safe_state()` must be safe to call there.

## Types

| AUTOSAR | Canon-C | |
|---------|---------|---|
| `uint8`, `uint16`, `uint32`, `uint64` | `u8`, `u16`, `u32`, `u64` | `uint8_t` .. `uint64_t` in Canon-C |
| `sint8`, `sint16`, `sint32`, `sint64` | `i8`, `i16`, `i32`, `i64` | `int8_t` .. `int64_t` in Canon-C |
| `float32`, `float64` | `f32`, `f64` | IEEE 754 binary32 and binary64 in both |
| `boolean` | `bool` | convert with `canon_autosar_boolean` and `canon_autosar_bool` |
| `Std_ReturnType` | `Error` | `canon_autosar_std_return`: `ERR_OK` is `E_OK`, every other code `E_NOT_OK` |

The adapter checks values, not types. `uint32` and `u32` hold the same
values, but can be different C types of the same width, `unsigned long` and
`unsigned int` for instance, and a pointer to one is then not a pointer to
the other: convert values between them, not pointers.

`boolean` has had two forms. Before AUTOSAR R22-11 it is an unsigned
integer, `unsigned char` in the specification's annex, with `TRUE` 1 and
`FALSE` 0 ([R4.3.1][pt431]); since R22-11 it is C99's `_Bool`, with `TRUE`
`true` and `FALSE` `false` ([R22-11][pt2211]). The conversions are right for
both, and CI tests both.

`canon_autosar_std_return` keeps only success and failure: `Std_ReturnType`
has `E_OK` and `E_NOT_OK`, and the codes `0x02` to `0x3F` a module may define
for itself ([SWS StandardTypes][std]); Canon-C's `Error` codes are not mapped
onto those.

## What Canon-C does not do

- **No Canon-C header is an AUTOSAR module.** None uses `Compiler.h`'s
  abstraction macros (`FUNC`, `P2VAR` and the rest) or `MemMap.h`, and none
  has a module ID, version information or a DevErrorDetect switch of its
  own. They declare in plain C99.
- **Memory sections.** Canon-C's functions are `static inline`: a function
  that is not inlined is emitted in the translation unit that includes its
  header. Which section it lands in follows that unit's compiler and its
  options, and, on a compiler that places code by `#pragma`, the section in
  force where the header is included: check the compiler's manual. Canon-C's
  one global object, the contract handler's pointer, is defined where
  `CANON_CONTRACT_IMPL` is.
- **MISRA C:2012.** The library is checked with Cppcheck's MISRA addon in CI,
  every finding pinned and argued (`docs/misra-campaign.md`); the adapter is
  checked with it (see below). Cppcheck's addon is not a qualified MISRA
  checker.
- **No certification or qualification** for ISO 26262 or any other
  standard; see `docs/verification.md` for what CI checks and what its proofs
  rest on.

## What CI checks

- **`autosar_test` and `autosar_bool_test`**
  (`test/integrations/autosar/autosar_test.c`) run in every job that runs
  the tests: Linux on x86-64, AArch64, i386 and s390x, with glibc and musl;
  macOS; Windows with MSVC and MinGW-w64's GCC. The first gives `boolean`
  AUTOSAR 4.3.1's form and sets every DET ID; the second gives it R22-11's
  and leaves the IDs at their defaults. Each checks
  `canon_autosar_std_return` for `ERR_OK` and every other code
  `semantics/error.h` names, `canon_autosar_boolean` for `true` and `false`,
  and `canon_autosar_bool` for `FALSE`, `TRUE` and every other value of an
  `unsigned char`; and that a failed `require()` reaches DET once, with the
  configured IDs, then the safe state, which ends the test.
- **The stand-ins.** `test/integrations/autosar/stubs/` holds a
  `Platform_Types.h`, a `Std_Types.h` and a `Det.h` with the definitions the
  adapter uses, as the specifications give them. They are not an AUTOSAR
  implementation, and CI has no AUTOSAR stack: a project's own headers, DET
  and ECU are not checked here.
- **The bare-metal targets** (Cortex-M0+, Cortex-M4, RV32, RV64) compile the
  test at -O0, -Os and -O2 (`2 · Platforms`, the embedded job).
- **MISRA C:2012**: the misra job reads the adapter through
  `test/integrations/autosar/misra_adapter.h`, which gives it a `ModuleId`.
- **Cppcheck and clang-tidy** read the test with the adapter; CompCert
  compiles and runs it.

[det]: https://www.autosar.org/fileadmin/standards/R24-11/CP/AUTOSAR_CP_SWS_DefaultErrorTracer.pdf
[pt431]: https://www.autosar.org/fileadmin/standards/R4.3.1/CP/AUTOSAR_SWS_PlatformTypes.pdf
[pt2211]: https://www.autosar.org/fileadmin/standards/R22-11/CP/AUTOSAR_SWS_PlatformTypes.pdf
[std]: https://www.autosar.org/fileadmin/standards/R20-11/CP/AUTOSAR_SWS_StandardTypes.pdf
