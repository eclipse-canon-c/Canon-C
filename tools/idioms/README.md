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
At 2d36ca1 (CI #1375): 42 ACSL features, 22 C features and 8 family features,
over the 41 repository files the verified units include. The first build, at
6290f4a (CI #1337), had 37 ACSL features; VERIFY-039 G11's repair of option's
and result's drivers wrote `\valid_function` and the `terminates` clause (39
at 37ad1f2, CI #1362), and G13's repair of memcmp's callers wrote `\dangling`
(40 at 7624d8c, CI #1367). G14's repair of the contract handler took its
loop's invariant out and wrote `terminates \false`: both clauses moved between
units, and no kind was added (40 at 64f4bd1, CI #1368). G18's repair of the
allocation wrappers wrote `allocates` and `frees`, and stated
`pool_alloc_zero`'s result, the only pointer result a contract left
unconstrained, so that shape left the catalogue (41 at 806bdeb, CI #1369).
G19's and G20's repair of `region_end` wrote its hook rule as a predicate in
an `axiomatic` block, the first a verified unit reads (42 at 642ad8a,
CI #1372). VERIFY-040 round 2 stated memory.h's `regions_overlap` by
`\separated`, so the built-in joined `frama-c-memory`'s list, and no
kind was added. VERIFY-041's `frama-c-vec-range`, the first job preprocessed
with `-CC` (whose comment flags `clang_flags` drops, clang rejecting them
without `-E`), added the family feature `vec@other+integer`, as that record's
P9 predicted (43 files and 9 family features at 42a89ad, CI #1382).

    python3 tools/idioms/extract.py crosscheck

checks that every residual class of §3.2 / Appendix B maps to at least one
catalogue feature (`appendix_b_map.json`), so the extractor can see the shapes
the record says produce residuals.

## Measuring a program

    python3 tools/idioms/extract.py measure [--cpp "FLAGS"] P.c [more.c …]

prints *P*'s features, the ones outside the catalogue, the count per layer, and
n(*P*). Use the preprocessor flags *P* will be verified with. *P* may lie
outside the repository, with headers of its own beside it; run the command
from the repository root. Until the paper's v7 review (2026-10-22) it could
not: the scope test read repository paths only, so a program outside the tree
lost its C features (layer B) without a warning, and with that fixed, its own
functions, and its own headers' functions, would have been read as libc's.
`measure` now takes as the program's every file its sources include that is
neither a repository file nor found in a system directory. Test 5 checks it.

## Tests

    python3 tools/idioms/tests/test_extract.py

1. an in-vocabulary program (verified `vec<int>`, arena, catalogued ACSL) has
   n(*P*) = 0;
2. an out-of-vocabulary program reports exactly its six planted constructs and
   the three unverified headers its one planted include pulls in, and a union —
   already used by the substrate — is seen but not counted (negative control);
3. every verified unit, measured as if it were *P*, has n = 0;
4. the catalogue rebuilds identically, and the cross-check passes;
5. a program copied outside the repository gives what it gives inside it, and
   its own functions, and those of a header of its own, are not counted as
   libc functions.

## Limits

- **A syntactic proxy.** It cannot see semantic novelty expressed with
  catalogued constructs — a new kind of invariant written with `\forall` and
  `\valid` counts as nothing new. A refutation at n(*P*) = 0 may therefore mean
  the catalogue was too coarse rather than that the hypothesis failed; the
  outcome report has to say which.
- **Clause detection is by line start** inside ACSL blocks, with comments
  removed; the self-test pins its behaviour on the substrate.
- **Shapes that are themselves residual mechanisms are in the catalogue.** For
  example, `ptr_is_aligned`'s `nonnull` behaviour constrains no result, so
  the shape of a behaviour that leaves the result unconstrained is "inside",
  and `mem_is_aligned`'s two residuals rest on it. That matches how the
  record files such residuals, under an existing argument (VERIFY-039's block
  on a clause the specification omits).
- **Macro text.** The extractor reads the annotations in the text of the
  macros a unit's sources define, whether or not the unit's preprocessing keeps
  them: under the default `-C` WP never sees them. Since VERIFY-039 G11's repair
  of the macro bodies (37ad1f2), bitset's, range's and the queue's units were
  listed for `\valid_function` and `terminates` before any clause WP verifies
  there wrote them. Each unit writes them itself since G11's later repairs:
  the queue's at result (`Bool`, `Error`), bitset's and range's at their
  copies of option.
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
   decide the test on the final value. A repair of the substrate's contracts can
   add kinds, as VERIFY-039 G11's added two, so the catalogue frozen has to be
   built from the substrate commit *P* is verified against.
4. **Kinds the verifier never proves on a feasible path.** G11's repair wrote
   `\valid_function`, which WP 29 does not implement: it turns the assertion
   into `false`, proved only where the path's hypotheses are inconsistent. Under
   the repaired contracts, so far the home drivers' (the macro bodies and the
   `DECLARE_*` declarations still require nothing of the pointer), a program
   that writes it, or calls one of option's or result's combinators on its
   calling branch, gets goals no prover discharges where the path is feasible;
   the record's arguments, once VERIFY-014's and VERIFY-015's blocks are
   restated, cover only the substrate's own run-time checks. The rebuilt
   catalogue counts the kind as inside; the pre-registration decides whether it
   is, and excludes it explicitly if not.
