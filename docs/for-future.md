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

## Concurrency: Mthread, DeadlockF, Conc2seq

Frama-C's plug-ins for concurrent C. The library is single-threaded apart
from the lifetime ids' counter (`core/primitives/lifetime.h`), whose atomic
forms are not analysed today.

Why it waits: decided with the author (2026-11-10), for later. Mthread is
not publicly distributed.

Sources: [Frama-C's plug-ins](https://frama-c.com/html/kernel-plugin.html).
