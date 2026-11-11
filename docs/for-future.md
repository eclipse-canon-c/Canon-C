# For the future

Things checked and worth doing, which are not done yet, each with the
reason it waits. When one is taken up, it moves to a record in
`docs/deviations.md`, and its entry here says so.

## Relational properties: Frama-C's RPP plug-in

Some properties relate two calls of a function rather than one: a
comparator of `core/primitives/compare.h` must give opposite answers for
swapped arguments, `cmp(a, b) == -cmp(b, a)`, which every sort that uses it
relies on. A WP contract speaks of one call, so it cannot state this
directly. RPP proves such properties by self-composition (Blatter,
Kosmatov, Le Gall, Prevosto, Petiot).

Why it waits: its only opam release, 0.0.4, needs Frama-C 33 or later, and
its page calls it an early prototype; the proofs here run on Frama-C 29.
Until the project moves to a Frama-C that RPP supports, such a property is
proved through a wrapper function that makes both calls, with an ordinary
contract.

Sources: [RPP's page](https://frama-c.com/fc-plugins/rpp.html) and [its
opam package](https://ocaml.org/p/frama-c-rpp).

## Verasco: an analyzer proved sound

Verasco is a static analyzer for C proved sound in Coq (Jourdan,
Laporte, Blazy, Leroy and Pichardie, POPL 2015): a program it analyses
without an alarm has no run-time error in CompCert's semantics of C. It is
not run here, and is not usable on this code today:

- its page offers version 1.3, which needs Coq 8.4;
- it reuses parts of CompCert, so it is not free software, and may be used
  for evaluation, research and education but not commercially;
- it accepts the subset of C99 that CompCert accepts, and its 2014
  release note says it handles neither recursion nor dynamic allocation;
- it does not read ACSL, so the library's contracts would not help it.

Sources: [its page](https://compcert.inria.fr/verasco/), [the release
note](https://gallium.inria.fr/blog/papers-verasco/) and [the
paper](https://hal.archives-ouvertes.fr/hal-01078386).

## SparrowBerry: a proved checker of an analyzer's results

SparrowBerry is a checker proved in Coq for the results of Sparse
Sparrow, a static analyzer for C (Cho, Kang, Choi, Hur and Yi, Seoul
National University). The analyzer itself is not proved. SparrowBerry
checks each result it gives, and a result that passes is, by the Coq
proof, a correct abstract semantics of the program. It is the other way to
trust a large tool: prove a small checker of its results instead of the
tool. It is free software (GPLv3), but it is not run here, and is not
usable on this code today:

- its one release, 2.0.1 (2013), needs Coq 8.4pl2;
- it checks the results of a 2013 research version of Sparrow, not those
  of the academic Sparrow maintained today, which is designed to be sound
  but is not proved, as Eva is not, and whose README describes it as
  detecting buffer overruns;
- neither reads ACSL, so the library's contracts would not help them.

Sources: [its page](https://ropas.snu.ac.kr/sparrowberry) and [Sparrow's
repository](https://github.com/prosyslab/sparrow).

## Foundational verification (issue #12)

Foundational verification proves the code in a proof assistant
against a formal semantics of C. VST's semantics is the one CompCert
preserves, so VST with CompCert would give a result from the source to
the machine code. VST and RefinedC are in Coq, AutoCorres in
Isabelle/HOL. Issue #12 records what it would take, starting with
`ptr.h`, `memory.h` and `slice.h`. It is not scheduled.

## MISRA C:2023 and MISRA C:2025

The editions of MISRA C after MISRA C:2012 and its amendments: MISRA C:2023
gathers 2012 and its amendments in one document, and MISRA C:2025 is the
latest. (MISRA C++:2023 is a different standard, for C++, and does not apply
here.) The project targets MISRA C:2012: its checker in CI, Cppcheck's MISRA
addon, is written for that edition, and it is still widely cited.

Why it waits: a checker for the newer editions is needed first. If a
commercial analyser that checks them (Astrée or the LDRA tool suite, both
asked on 2026-11-10) can be run on Canon-C, it would give the gap.

Sources: [Astrée's compliance list](https://www.absint.com/astree/compliance.htm),
[MISRA C on Wikipedia](https://en.wikipedia.org/wiki/MISRA_C) and [LDRA on
MISRA C:2025](https://ldra.com/ldra-blog/misra-c-2025-the-whys-and-wherefores-of-whats-new/).

## Package managers

Canon-C is reached today from its repository: a user copies the headers, or
puts the repository's root on the include path (README, "Usage"). A
package manager would fetch it by name and version. The ones its users are likely to look in:

- for C and C++ in general, vcpkg and Conan, whose recipes live in their
  own repositories (vcpkg's registry, ConanCenter);
- for embedded projects, PlatformIO's registry, Espressif's component
  registry (ESP-IDF) and a Zephyr module, each described by a file in this
  repository (`library.json`, `idf_component.yml`, `zephyr/module.yml`);
- for systems, Debian and Ubuntu, and Homebrew, whose packages their own
  maintainers keep.

Why it waits: most of them build from a CMake target, which the project's
`CMakeLists.txt` does not offer yet; it builds the tests only. That target,
`Canon-C::canon`, with install rules, `find_package(CanonC)` and a
pkg-config file, comes first, checked in CI by a small project that uses
it. It waits for a stable release, as the managers do (decided with the
author, 2026-11-11): its names and the installed layout are promises to
users and to the managers' recipes, best made once the directories are
settled, and `integrations/`, planned for AUTOSAR, will need an install
decision of its own. Each manager then points at that release's tag, and
its recipe is kept up to date there at every release.

What a check for the target found (2026-11-11):

- **The headers need only the repository's root on the include path.** Of
  the 113 headers in `core/`, `semantics/`, `data/`, `algo/` and `util/`,
  98 compile alone that way (gcc, `-std=c99 -Wall -Wextra -Wpedantic
  -Werror`). The other 15 refuse by design, with an `#error`: `algo/`'s
  nine `_impl.h` and `data/hashmap/hashmap_impl.h`, which only their
  module's header includes, and five more of `data/hashmap/`'s, which need
  `HASHMAP_KEY_TYPE` (or, for `hashmap_fmt.h`, `HASHMAP_FMT_KEY_FN`)
  defined first.
- **Those five directories, headers only, are what an install would
  copy.** No header in them includes from `vmacros/` (the proofs' drivers),
  `canary/`, `tools/` or `test/`.
- **Through `add_subdirectory` or FetchContent, the target should come
  alone.** Canon-C's tests, and the options that build them (warnings as
  errors, sanitizers, fuzzing), belong to Canon-C as the top project only;
  today `add_subdirectory` adds Canon-C's test programs to the user's
  build.

Sources: [vcpkg](https://github.com/microsoft/vcpkg),
[ConanCenter](https://github.com/conan-io/conan-center-index),
[PlatformIO's registry](https://registry.platformio.org),
[Espressif's component registry](https://components.espressif.com) and
[Zephyr's modules](https://docs.zephyrproject.org/latest/develop/modules.html).

## Concurrency: Mthread, DeadlockF, Conc2seq

Frama-C's plug-ins for concurrent C. The library is single-threaded apart
from the lifetime ids' counter (`core/primitives/lifetime.h`), whose atomic
forms are not analysed today.

Why it waits: decided with the author (2026-11-10), for later. Mthread is
not publicly distributed.

Sources: [Frama-C's plug-ins](https://frama-c.com/html/kernel-plugin.html).
