# CERT C and CWE

What Canon-C checks against the SEI CERT C Coding Standard, how that maps to
ISO/IEC TS 17961 and to MITRE's CWE, and what is not checked. CI runs
clang-tidy's CERT checks for C on the library's code; nothing here is a
certification, and a check covers only what its tool can see of its rule.

## What CI checks

The static-analysis job (`3 · Static analysis & coverage`, on Linux) runs
clang-tidy 18's 19 checks for CERT's C rules and recommendations,
`cert-*-c`, over the test files, the same files as its other clang-tidy
step. `tools/cert/cert_check.py` sorts what they find:

- **The library's code** fails the step: a finding in a header of `core/`,
  `semantics/`, `data/`, `algo/`, `util/` or `integrations/`, or in a test
  where a macro one of those headers defines expands (counted once, at the
  macro's line).
- **The tests' own code** is not checked. Its findings are counted in the
  step's log and do not fail it: the author decided on 2026-11-13 that the
  CERT checks apply to the library. Most are `cert-err33-c` on the test
  harness's `printf` and `fprintf`, whose results it does not read.

The step's notice says how many findings the library's code has: none since
VERIFY-077. To run it locally, configure a build with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, run `clang-tidy -p BUILD
--checks='-*,cert-*-c'` over `test/**/*.c` (the job leaves out
`dynstring_test.c` and `dynvec_test.c`) into a file, and give that file to
`tools/cert/cert_check.py` with the repository's root.

## The checks

Each check, the CERT C guideline it checks, and how CERT's own page relates
that guideline to ISO/IEC TS 17961:2013 (C Secure Coding Rules) and to CWE.
CERT's pages list each TS 17961 item with an unspecified relationship, and
give the CWE relationship shown; an empty cell means CERT's page lists none.

| clang-tidy | CERT C | | ISO/IEC TS 17961 | CWE (relationship, per CERT) |
|------------|--------|---|------------------|------------------------------|
| `cert-con36-c` | [CON36-C][con36]. Wrap functions that can spuriously wake up in a loop | rule | | |
| `cert-dcl03-c` | [DCL03-C][dcl03]. Use a static assertion to test the value of a constant expression | recommendation | | |
| `cert-dcl16-c` | [DCL16-C][dcl16]. Use "L," not "l," to indicate a long value | recommendation | | |
| `cert-dcl37-c` | [DCL37-C][dcl37]. Do not declare or define a reserved identifier | rule | [resident] | |
| `cert-env33-c` | [ENV33-C][env33]. Do not call system() | rule | [syscall] | CWE-88 (partial overlap); CWE-676 (rule subset of CWE) |
| `cert-err33-c` | [ERR33-C][err33]. Detect and handle standard library errors | rule | [liberr] | CWE-252, CWE-253 (partial overlap); CWE-391 (rule subset of CWE) |
| `cert-err34-c` | [ERR34-C][err34]. Detect errors when converting a string to a number | rule | | CWE-676 (rule subset of CWE); CWE-758 (partial overlap) |
| `cert-exp42-c` | [EXP42-C][exp42]. Do not compare padding data | rule | [padcomp] | |
| `cert-fio38-c` | [FIO38-C][fio38]. Do not copy a FILE object | rule | [filecpy] | |
| `cert-flp30-c` | [FLP30-C][flp30]. Do not use floating-point variables as loop counters | rule | | |
| `cert-flp37-c` | [FLP37-C][flp37]. Do not use object representations to compare floating-point values | rule | | |
| `cert-msc24-c` | [MSC24-C][msc24]. Do not use deprecated or obsolescent functions | recommendation | | CWE-20, -73, -79, -89, -91, -94, -114, -120, -192, -197, -367, -464, -601, -676 (no relationship given) |
| `cert-msc30-c` | [MSC30-C][msc30]. Do not use the rand() function for generating pseudorandom numbers | rule | | CWE-327, CWE-330, CWE-338, CWE-676 (rule subset of CWE) |
| `cert-msc32-c` | [MSC32-C][msc32]. Properly seed pseudorandom number generators | rule | | CWE-327, CWE-330 (rule subset of CWE); CWE-331 (exact) |
| `cert-msc33-c` | [MSC33-C][msc33]. Do not pass invalid data to the asctime() function | rule | | |
| `cert-pos44-c` | [POS44-C][pos44]. Do not use signals to terminate threads | rule | | |
| `cert-pos47-c` | [POS47-C][pos47]. Do not use threads that can be canceled asynchronously | rule | | |
| `cert-sig30-c` | [SIG30-C][sig30]. Call only asynchronous-safe functions within signal handlers | rule | [asyncsig] | CWE-479 (exact) |
| `cert-str34-c` | [STR34-C][str34]. Cast characters to unsigned char before converting to larger integer sizes | rule | [signconv] | CWE-704 (no relationship given) |

Sixteen rules and three recommendations; seven map to TS 17961, eight to
CWE. A check finds what its pattern can see: `cert-err33-c`, for one, flags
an unread result of a listed library function, not every unhandled error.

## What the checks found

VERIFY-077 ran them first. In the library's code they found six things,
since handled:

- **Reserved identifiers** (DCL37-C, MISRA C:2012 rule 21.2). Four functions
  at file scope began with an underscore, which C reserves there:
  `_hm_home`, `_hm_wrap` and `_hm_normalize_hash` in
  `data/hashmap/hashmap_impl.h`, and `_stringbuf_append_bytes` in
  `data/stringbuf.h`. Each lost its underscore. A fifth,
  `_hm_lifetime_next_id_`, built only with `CANON_LIFETIME_DEBUG`, which the
  step does not build, was found by a search of the headers and renamed
  `hm_lifetime_next_id_`. Cppcheck's MISRA addon had reported none of them.
- **`_POSIX_C_SOURCE`** (DCL37-C). `util/time.h` defines it, as POSIX asks
  an application that wants `clock_gettime` to do; MISRA-DEV-011 argues it
  for rule 21.1, and its lines carry `NOLINT(cert-dcl37-c)` citing that
  record.
- **An unread `snprintf` result** (ERR33-C). `DIAG_PUSH_FMT` in
  `semantics/diag.h` ignored it. It now pushes an empty message when
  `snprintf` fails, rather than whatever the buffer held; `diag_test`
  checks that on glibc, where a `%lc` that the "C" locale cannot encode
  makes it fail.

## What is not checked

- **CERT C's other guidelines.** No tool here checks them.
- **Cppcheck's CERT addon.** Open-source Cppcheck's `cert.py` addon was
  removed after its 2.7 release, its makers moving CERT checking to the
  commercial Cppcheck Premium. CI runs Cppcheck 2.13 and 2.19, which do not
  have it.
- **The tests' own code**, as above.
- **The library's code no test reaches**: a macro no test expands, and the
  configurations the step does not build, such as `CANON_LIFETIME_DEBUG`
  and `CANON_NO_REQUIRE`.
- **Other tools.** CodeQL, Coverity, PVS-Studio and SonarQube are planned
  (the README's badges): the project plans to run them. None has run.

See also `docs/misra-campaign.md` for MISRA C:2012, which overlaps CERT C in
places, and `docs/verification.md` for what CI checks overall.

[con36]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/concurrency-con/con36-c
[dcl03]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/recommendations/declarations-and-initialization-dcl/dcl03-c
[dcl16]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/recommendations/declarations-and-initialization-dcl/dcl16-c
[dcl37]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/declarations-and-initialization-dcl/dcl37-c
[env33]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/environment-env/env33-c
[err33]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/error-handling-err/err33-c
[err34]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/error-handling-err/err34-c
[exp42]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/expressions-exp/exp42-c
[fio38]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/input-output-fio/fio38-c
[flp30]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/floating-point-flp/flp30-c
[flp37]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/floating-point-flp/flp37-c
[msc24]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/recommendations/miscellaneous-msc/msc24-c
[msc30]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/miscellaneous-msc/msc30-c
[msc32]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/miscellaneous-msc/msc32-c
[msc33]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/miscellaneous-msc/msc33-c
[pos44]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/posix-pos/pos44-c
[pos47]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/posix-pos/pos47-c
[sig30]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/signals-sig/sig30-c
[str34]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/characters-and-strings-str/str34-c
