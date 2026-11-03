#!/usr/bin/env python3
"""
pointer_probe.py -- VERIFY-057's probe: pointer arithmetic outside its object.

C leaves a pointer computed outside its object, or more than one past its
end, undefined, whether or not it is dereferenced (C11 6.5.6p8). Frama-C's
RTE plugin checks it only under the kernel option -warn-invalid-pointer,
which adds a `pointer_value` assertion, \\object_pointer(p), at each such
pointer. The verified units run without it. This probe runs each unit as its
job runs it, with the option added, and proves only those assertions
(-wp-prop pointer_value), with the CI's provers.

    python3 tools/probes/verify-057/pointer_probe.py run UNIT
        Runs WP on UNIT's file with UNIT's preprocessing and memory model,
        -wp-rte -warn-invalid-pointer -wp-prop pointer_value -wp-split,
        Alt-Ergo, Z3 and CVC5 at 120 s, into wp_UNIT.txt. Exits 1 if WP
        writes neither a summary nor "No goal generated".

    python3 tools/probes/verify-057/pointer_probe.py report UNIT
        Reads wp_UNIT.txt and posts one notice: the line and the unproved
        names, or that the unit has no such goal. Report-only: it exits 0
        but for missing output (1). The scoring is VERIFY-057's, in
        docs/deviations.md.

Each unit's file, memory model and preprocessing are its job's, as the
proof workflows (proofs-core.yml, proofs-semantics.yml, proofs-data.yml)
give them at VERIFY-057; the preprocessing as tools/idioms/catalogue.json
reads it from them. frama-c-arena-32 and frama-c-win64, the machine-model
variants, are left out.
"""
import re
import subprocess
import sys

UNITS = {
    'frama-c-checked': ('core/primitives/checked.h', 'Typed', '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-bits': ('core/primitives/bits.h', 'Typed', '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-compare': ('core/primitives/compare.h', 'Typed+Cast', '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-ptr': ('core/primitives/ptr.h', 'Typed+Cast', '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-slice': ('core/slice.h', 'Typed+Cast', '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-memory': ('core/memory.h', 'Typed+Cast', '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-arena': ('core/arena.h', 'Typed+Cast', '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-pool': ('core/pool.h', 'Typed+Cast', '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-region': ('core/region.h', 'Typed+Cast', '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-lifetime': ('core/primitives/lifetime.h', 'Typed', '-I core/primitives -I core -I . -DCANON_LIFETIME_DEBUG -DCANON_LIFETIME_NO_ATOMICS -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-error': ('semantics/error.h', 'Typed', '-I core/primitives -I core -I semantics -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-option': ('vmacros/vdrivers/option_verify.h', 'Typed', '-I core/primitives -I core -I semantics -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-result': ('vmacros/vdrivers/result_verify.h', 'Typed', '-I core/primitives -I core -I semantics -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-borrow': ('semantics/borrow.h', 'Typed+Cast', '-I core/primitives -I core -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-diag': ('semantics/diag.h', 'Typed+Cast', '-I core/primitives -I core -I semantics -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-vec': ('vmacros/vdrivers/vec_verify.h', 'Typed+Cast', '-I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-deque': ('vmacros/vdrivers/deque_verify.h', 'Typed', '-I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-bitset': ('vmacros/vdrivers/bitset_verify.h', 'Typed+Cast', '-I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-priority-queue': ('data/priority_queue.h', 'Typed+Cast', '-I data -I core -I core/primitives -I semantics -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-range': ('vmacros/vdrivers/range_verify.h', 'Typed', '-I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-vec-range': ('vmacros/vdrivers/vec_range_verify.h', 'Typed+Cast', '-CC -I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-vec-fmt': ('vmacros/vdrivers/vec_fmt_verify.h', 'Typed+Cast', '-CC -I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-stringbuf': ('data/stringbuf.h', 'Typed+Cast', '-I core/primitives -I core -I semantics -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-stack': ('vmacros/vdrivers/stack_verify.h', 'Typed+Cast', '-CC -I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-array': ('vmacros/vdrivers/array_verify.h', 'Typed+Cast', '-CC -I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG'),
    'frama-c-hashmap': ('vmacros/vdrivers/hashmap_verify.h', 'Typed+Cast', '-CC -I core/primitives -I core -I semantics -I data -I . -DCANON_NO_REQUIRE -DNDEBUG'),
}

NOTICE_CUT = 4000  # the runner cuts an annotation at 4096 characters


def out_name(unit):
    return f"wp_{unit}.txt"


def run(unit):
    path, model, cpp = UNITS[unit]
    cmd = ["frama-c", "-wp", "-wp-rte", "-warn-invalid-pointer",
           "-wp-prop", "pointer_value",
           "-wp-model", model,
           "-wp-prover", "alt-ergo,z3,cvc5",
           "-wp-timeout", "120",
           "-wp-split",
           f"-cpp-extra-args={cpp}",
           path]
    print("Running:", " ".join(cmd), flush=True)
    with open(out_name(unit), "w") as f:
        r = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT)
    text = open(out_name(unit)).read()
    print(f"frama-c exit: {r.returncode}")
    if not re.search(r"^\[wp\] Proved goals:", text, re.M) and "No goal generated" not in text:
        tail = " / ".join(text.strip().splitlines()[-5:])
        print(f"::error title=pointer-probe {unit}::no summary; last lines: {tail[:NOTICE_CUT]}")
        return 1
    return 0


def report(unit):
    try:
        text = open(out_name(unit)).read()
    except OSError:
        print(f"::error title=pointer-probe {unit}::NO DATA: {out_name(unit)} is missing")
        return 1
    if "No goal generated" in text and not re.search(r"^\[wp\] Proved goals:", text, re.M):
        msg = f"{unit}: no pointer_value goal"
        print(msg)
        print(f"::notice title=pointer-probe {unit}::{msg}")
        return 0
    m = re.findall(r"^\[wp\] Proved goals:\s+(\d+ / \d+)", text, re.M)
    line = m[-1] if m else "?"
    bad = sorted(set(re.findall(r"^\[wp\] \[(?:Failed|Failure|Invalid|Stepout)\] ([A-Za-z0-9_]+)", text, re.M)))
    unproved = sorted(set(re.findall(r"^\[wp\] \[(?:Timeout|Unknown)\] ([A-Za-z0-9_]+)", text, re.M)))
    print(f"{unit}: pointer_value proved {line}")
    for n in unproved:
        print(f"  unproved: {n}")
    for n in bad:
        print(f"  failed/invalid/stepout: {n}")
    names = " ".join(unproved) if unproved else "none"
    msg = f"{unit}: pointer_value proved {line}; unproved {len(unproved)}: {names}"
    if bad:
        msg += f"; failed/invalid/stepout {len(bad)}: {' '.join(bad)}"
    print(f"::notice title=pointer-probe {unit}::{msg[:NOTICE_CUT]}")
    return 0


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("run", "report") or sys.argv[2] not in UNITS:
        print(__doc__)
        print("units:", " ".join(UNITS))
        return 2
    return run(sys.argv[2]) if sys.argv[1] == "run" else report(sys.argv[2])


if __name__ == "__main__":
    sys.exit(main())
