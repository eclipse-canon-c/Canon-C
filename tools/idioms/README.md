# tools/idioms — a record-independent n(P)

The committed test of the paper's §4.3 predicts that a program *P* written over
Canon-C's vocabulary needs no new discharge argument, provided n(*P*) = 0:
*P* uses no specification shape the verified substrate has not already used.
For that prediction to be able to fail, n(*P*) must be computed **from source
text alone** — never from WP output, the deviation record, or whether an
argument turned out to be needed. Otherwise "outside the catalogue" could be
read off "needed a new argument", and the zero case would hold by construction
(paper §2).

`extract.py` reads C and ACSL sources and nothing else.

## What it extracts

Features are *kinds*, not counts: n(*P*) counts new kinds of shape, not new
instances of known ones.

| layer | what | examples |
|---|---|---|
| A | ACSL constructs written in the sources | built-ins (`\valid`, `\separated`, `\fresh`, …); clause kinds (`requires`, `behavior`, `loop invariant`, `predicate`, …); logic-level pointer-to-integer casts; two contract shapes behind known residual mechanisms — a behavior of a value-returning function that never constrains `\result`, and a pointer-returning function whose contract never constrains `\result` |
| B | C constructs WP models specially | pointer/integer casts, bitwise and shift operators, non-linear arithmetic, indirect calls, unions, bit-fields, variadic definitions and calls, recursion, loops (and loops spelled inside macro bodies), `goto`, floating point, dynamic allocation, `setjmp`/`longjmp`, VLAs, inline assembly, and each libc function called |
| C | substrate idioms | each macro family instantiated (`DEFINE_<FAMILY>_…`) and the kind of each type argument (integer, enum, struct, pointer, floating); when measuring a program, every repository header it includes that no verified unit includes |

## The catalogue

    python3 tools/idioms/extract.py catalogue [--commit HASH]

builds `catalogue.json` from **every Frama-C job in the CI workflow**, each with
its own translation unit and preprocessor flags, so the catalogue covers exactly
what CI verifies (22 jobs, 21 units). Each entry lists the jobs it comes from.
At 6290f4a (CI #1337): 37 ACSL features, 22 C features and 8 family features,
over the 41 repository files the verified units include.

    python3 tools/idioms/extract.py crosscheck

checks that every residual class of §3.2 / Appendix B maps to at least one
catalogue feature (`appendix_b_map.json`), so the extractor can see the shapes
the record says produce residuals.

## Measuring a program

    python3 tools/idioms/extract.py measure [--cpp "FLAGS"] P.c [more.c …]

prints *P*'s features, the ones outside the catalogue, the count per layer, and
n(*P*). Use the preprocessor flags *P* will be verified with.

## Tests

    python3 tools/idioms/tests/test_extract.py

1. an in-vocabulary program (verified `vec<int>`, arena, catalogued ACSL) has
   n(*P*) = 0;
2. an out-of-vocabulary program reports exactly its six planted constructs and
   the three unverified headers its one planted include pulls in, and a union —
   already used by the substrate — is seen but not counted (negative control);
3. every verified unit, measured as if it were *P*, has n = 0;
4. the catalogue rebuilds identically, and the cross-check passes.

## Limits

- **A syntactic proxy.** It cannot see semantic novelty expressed with
  catalogued constructs — a new kind of invariant written with `\forall` and
  `\valid` counts as nothing new. A refutation at n(*P*) = 0 may therefore mean
  the catalogue was too coarse rather than that the hypothesis failed; the
  outcome report has to say which.
- **Clause detection is by line start** inside ACSL blocks, with comments
  removed; the self-test pins its behaviour on the substrate.
- **Shapes that are themselves residual mechanisms are in the catalogue.** For
  example, `pool_alloc_zero`'s contract constrains no result, so the
  opaque-result shape is "inside". That matches how the record files such
  residuals (under the existing class-(e) argument).
- Built with clang 18; the features are coarse enough that other recent clang
  versions should agree, and test 4 checks it.

## Decisions for the pre-registration (not made here)

1. **Which layers define n(*P*).** Recommended: all three, with both the
   verifier-bound and the specification-bound predictions of §4.3 conditioned on
   n(*P*) = 0, and each layer reported separately so a refutation can be traced.
2. **Whether family type-kinds count.** `vec`, `deque` are verified only at an
   integer type, so a program storing structs in a `vec` has n(*P*) ≥ 1
   (`family:vec@struct`) and leaves the zero case untested. Either accept that,
   or first verify `vec` at a struct type in the substrate, which extends the
   catalogue legitimately and is a small prospective test in itself.
3. **Freezing.** Commit `catalogue.json` and this tool with the prediction hash,
   compute n(*P*) once before the first WP run and once on the final sources, and
   decide the test on the final value.
