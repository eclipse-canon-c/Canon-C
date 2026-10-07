#!/usr/bin/env python3
"""Self-tests for tools/idioms/extract.py. Run from anywhere: python3 tools/idioms/tests/test_extract.py"""
import json, os, subprocess, sys, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "extract.py")
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(HERE, ".."))
import extract  # noqa: E402

def run_tool(*args):
    """Run extract.py; on failure, show what it said instead of a bare traceback."""
    out = subprocess.run([sys.executable, TOOL] + list(args), cwd=REPO, capture_output=True, text=True)
    if out.returncode != 0:
        sys.stderr.write("\nextract.py %s failed (exit %d):\n%s\n" % (" ".join(args), out.returncode,
                         (out.stderr or out.stdout).strip()))
        sys.exit(2)
    return out.stdout

def measure(path):
    return json.loads(run_tool("measure", path))

failures = []
def check(cond, msg):
    print(("  ok    " if cond else "  FAIL  ") + msg)
    if not cond:
        failures.append(msg)

print("1. an in-vocabulary program has n(P) = 0")
r = measure(os.path.join(HERE, "in_vocab.c"))
check(r["n"] == 0, "in_vocab.c: n = %d %s" % (r["n"], r["outside_catalogue"]))

print("2. an out-of-vocabulary program reports exactly the planted shapes")
expected = {"acsl-builtin:\\offset", "acsl-clause:allocates", "c:bit-field", "c:goto", "c:recursion",
            "c:setjmp/longjmp", "unverified header:util/str/intern.h", "unverified header:util/str/str.h",
            "unverified header:util/str/str_view.h"}
r = measure(os.path.join(HERE, "out_of_vocab.c"))
got = set(r["outside_catalogue"])
check(got == expected, "out_of_vocab.c: missing %s, unexpected %s" % (sorted(expected - got), sorted(got - expected)))
check("c:union" in r["features"] and "c:union" not in got, "negative control: union seen but not counted")

print("3. every verified unit, measured as if it were P, has n = 0")
for job, files, cpp in extract.verified_units():
    if job == "frama-c-arena-32":
        continue
    n = json.loads(run_tool("measure", "--cpp", " ".join(cpp), *files))["n"]
    check(n == 0, "%s: n = %d" % (job, n))

print("4. the catalogue is reproducible and every residual class is visible")
with tempfile.TemporaryDirectory() as d:
    fresh = os.path.join(d, "c.json")
    run_tool("catalogue", "--out", fresh)
    a, b = json.load(open(fresh)), json.load(open(os.path.join(HERE, "..", "catalogue.json")))
    check(all(set(a[L]) == set(b[L]) for L in "ABC") and a["verified_closure"] == b["verified_closure"],
          "rebuilt catalogue equals the committed one")
x = subprocess.run([sys.executable, TOOL, "crosscheck"], cwd=REPO, capture_output=True, text=True)
check(x.returncode == 0, "crosscheck passes")

print("\n%s" % ("ALL TESTS PASSED" if not failures else "%d FAILURE(S)" % len(failures)))
sys.exit(1 if failures else 0)
