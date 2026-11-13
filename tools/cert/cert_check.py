#!/usr/bin/env python3
# Copyright (C) 2026 Eclipse Canon-C contributors
# SPDX-License-Identifier: MIT AND CC0-1.0
# AI Disclosure: This file was largely AI-generated; see the repository's
# licence notes. The human contributor has reviewed it.
"""cert_check.py -- the static-analysis job's CERT C step (VERIFY-077).

Reads what clang-tidy prints when it runs CERT C's checks (cert-*-c) over
the test files, and reports what they find in the library's code:

  - a finding whose location is in one of the library's directories
    (core/, semantics/, data/, algo/, util/, integrations/); or
  - a finding in a test that a note traces to a macro one of those
    directories defines ("expanded from macro"): the library's code, as
    the test expands it.

The tests' own code is not checked, as the author decided on 2026-11-13;
its findings are counted, and printed, but do not fail the step. A
compiler error anywhere fails it: the checks did not read that file.

Usage:  cert_check.py ROOT TIDY_OUTPUT
        ROOT        the repository's root, as the paths in TIDY_OUTPUT name it
        TIDY_OUTPUT clang-tidy's standard output and error, together

Exit status: 0 when the library's code has no finding, 1 otherwise.
"""

import os
import re
import sys

LIBRARY = ("core", "semantics", "data", "algo", "util", "integrations")
DIAG = re.compile(r"^(?P<path>[^\s:][^:]*):(?P<line>\d+):(?P<col>\d+): "
                  r"(?P<kind>error|warning|note): (?P<msg>.*)$")
CHECK = re.compile(r"\s\[(?P<checks>[^\]]+)\]$")


def relative(root, path):
    """The path relative to the root, or None when it lies outside it."""
    full = os.path.normpath(path if os.path.isabs(path) else os.path.join(root, path))
    rel = os.path.relpath(full, root)
    return None if rel.startswith("..") else rel


def in_library(rel):
    return rel is not None and rel.split(os.sep, 1)[0] in LIBRARY


def parse(root, text):
    """Yield each diagnostic as (rel, line, col, msg, notes); a note is
    (rel, line, msg)."""
    current = None
    for raw in text.splitlines():
        m = DIAG.match(raw)
        if not m:
            continue
        rel = relative(root, m.group("path"))
        if m.group("kind") == "note":
            if current is not None:
                current[4].append((rel, int(m.group("line")), m.group("msg")))
            continue
        if current is not None:
            yield current
        current = (rel, int(m.group("line")), int(m.group("col")), m.group("msg"), [])
    if current is not None:
        yield current


def main(argv):
    if len(argv) != 3:
        sys.stderr.write(__doc__)
        return 2
    root = os.path.normpath(os.path.abspath(argv[1]))
    with open(argv[2], encoding="utf-8", errors="replace") as f:
        text = f.read()

    library, tests, broken = {}, {}, {}
    for rel, line, col, msg, notes in parse(root, text):
        m = CHECK.search(msg)
        checks = m.group("checks") if m else ""
        names = [c for c in checks.split(",") if c and c != "-warnings-as-errors"]
        if not names or any(n.startswith("clang-diagnostic-error") for n in names):
            broken.setdefault((rel, line, col, msg), (rel, line, msg))
            continue
        if not any(n.startswith("cert-") for n in names):
            continue                      # another check's diagnostic: not this step's
        text_msg = CHECK.sub("", msg)
        tag = "[" + ",".join(names) + "]"
        macros = [(nrel, nline) for nrel, nline, nmsg in notes
                  if "expanded from macro" in nmsg and in_library(nrel)]
        if in_library(rel):
            library.setdefault((rel, line, text_msg, tag), set())
        elif macros:
            # The library's code as a test expands it: one finding at the
            # macro's line, however many tests reach it.
            nrel, nline = macros[0]
            library.setdefault((nrel, nline, text_msg, tag), set()).add(f"{rel}:{line}")
        else:
            tests.setdefault((rel, line, col, msg), True)

    for rel, line, msg in sorted(broken.values(), key=lambda d: (str(d[0]), d[1])):
        print(f"::error file={rel},line={line}::clang-tidy could not read this file: {msg}")
    for (rel, line, msg, tag), via in sorted(library.items()):
        where = f" (in the expansion at {', '.join(sorted(via))})" if via else ""
        print(f"::error file={rel},line={line}::{msg} {tag}{where}")
        print(f"  {rel}:{line}: {msg} {tag}{where}")
    print(f"CERT C: {len(library)} finding(s) in the library's code; "
          f"{len(tests)} in the tests' own code, which this step does not check")
    print(f"::notice title=cert-c::clang-tidy's CERT C checks: {len(library)} finding(s) "
          f"in the library's code; the tests' own code is not checked")
    return 1 if (library or broken) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
