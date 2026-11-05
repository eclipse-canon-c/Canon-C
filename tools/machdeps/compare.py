#!/usr/bin/env python3
"""tools/machdeps/compare.py COMMITTED_DIR GENERATED_DIR

VERIFY-060, phase A2: the drift check. Every committed model
(COMMITTED_DIR/machdep_<name>.yaml) is compared with the one generate.sh has
just made from the same compiler (GENERATED_DIR/machdep_<name>.yaml), as YAML
data, so that the order of the macros in custom_defs does not matter. Every
field that differs is printed. A generated model with no committed copy is
reported as new.

Exit status: the number of committed models that differ from, or are missing
in, the generated set.
"""
import glob
import os
import sys

import yaml


def load(path):
    with open(path, encoding='utf-8') as f:
        return yaml.safe_load(f)


def main():
    committed_dir, generated_dir = sys.argv[1], sys.argv[2]
    committed = {os.path.basename(p): p for p in glob.glob(os.path.join(committed_dir, 'machdep_*.yaml'))}
    generated = {os.path.basename(p): p for p in glob.glob(os.path.join(generated_dir, 'machdep_*.yaml'))}
    bad = 0
    for name in sorted(committed):
        if name not in generated:
            print(f"MISSING: {name} is committed, and was not generated")
            bad += 1
            continue
        a, b = load(committed[name]), load(generated[name])
        diff = sorted(k for k in set(a) | set(b) if a.get(k) != b.get(k))
        if not diff:
            print(f"SAME: {name} regenerates the same ({len(a)} fields)")
            continue
        bad += 1
        print(f"DIFFERS: {name}: {len(diff)} field(s)")
        for k in diff:
            va, vb = a.get(k), b.get(k)
            if isinstance(va, dict) and isinstance(vb, dict):
                keys = sorted(x for x in set(va) | set(vb) if va.get(x) != vb.get(x))
                print(f"   {k}: {len(keys)} entr(y/ies) differ")
                for x in keys[:20]:
                    print(f"      {x}: committed {va.get(x)!r}, generated {vb.get(x)!r}")
            else:
                print(f"   {k}: committed {va!r}, generated {vb!r}")
    for name in sorted(set(generated) - set(committed)):
        print(f"NEW: {name} was generated, and is not committed")
    print(f"compare.py: {len(committed)} committed, {bad} differ or missing")
    return bad


if __name__ == '__main__':
    sys.exit(main())
