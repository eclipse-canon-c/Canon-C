# Canon-C Verification Record

This document records the formal-verification state of Canon-C's
header-only modules: which headers carry ACSL contracts, which goals
Frama-C WP discharges automatically, and which goals require manual
justification (cross-referenced to `docs/deviations.md`).

## Summary

| Header     | Proved / Total | Automatic % | Unproved | Deviation entry  |
|------------|----------------|-------------|----------|------------------|
| checked.h  | 1539 / 1541    | 99.87%      |        2 | VERIFY-001, -002 |
| bits.h     |  746 / 761     | 98.03%      |       15 | VERIFY-003, -004 |
| compare.h  |  208 / 208     | 100.00%     |        0 | VERIFY-005       |
| ptr.h      | 1729 / 1739    | 99.43%      |       10 | VERIFY-006       |
| **Total**  | **4222 / 4249** | **99.36%** |  **27**  |                  |

All 27 unproved goals carry written manual-proof arguments in
`docs/deviations.md`. All proof runs use the triple-prover
configuration **Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1** with
`-wp-timeout 120`.

MC/DC coverage is 100% on all four verified headers (64/64 on
checked.h, 52/52 on bits.h, 8/8 on compare.h, 42/42 on ptr.h) as
measured by GCC 14 `-fcondition-coverage`.

---

## checked.h

**Scope.** Overflow-safe arithmetic for the Canon-C primitive integer
types (u8, u16, u32, u64, usize, and signed variants). Every function
carries a full ACSL contract with `no_overflow` / `overflow` behaviors
that are `complete` and `disjoint`, enabling `-wp-rte` to prove absence
of runtime errors on the fallback path.

**Provers.** Alt-Ergo 2.6.3 + Z3 4.15.2 + CVC5 1.2.1.
**Memory model.** Typed (default).

**Result.** 1539 / 1541 goals discharged automatically. CVC5
contributes 3 goals that neither Alt-Ergo nor Z3 can close. The
remaining 2 goals are manually discharged under VERIFY-001.

**Reproduction.**
```bash
frama-c -wp -wp-rte \
  -wp-prover alt-ergo,z3,cvc5 \
  -wp-timeout 120 \
  -wp-split \
  -cpp-extra-args="-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG" \
  core/primitives/checked.h
```

**Expected output:**
