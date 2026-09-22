#!/usr/bin/env python3
"""
tokeneer_rules.py — a second argument budget, from another ecosystem.

Extracts every user rule from the SPARK 2005 Tokeneer sources distributed in
AdaCore's public SPARK repository, normalises each rule up to identifier
renaming, and accumulates distinct rules over the packages that carry any,
in dependency-depth order with the main program last. Reproduces Table 6 of
the paper (§4.4). Also groups the 14 justification pragmas of the maintained SPARK 2014 port
(testsuite/gnatprove/tests/tokeneer) by reason: Table 7.

Usage:
    python3 tools/tokeneer_rules.py [path-to-spark2014-checkout]

If no path is given, performs a blobless partial clone of
https://github.com/AdaCore/spark2014 into ./_spark2014 and sparse-checks-out
docs/case_study/tokeneer only (a few MB of blobs).

What a "user rule" is: a lemma in an .rlu file given to the SPARK 2005
Simplifier for a VC it could not discharge, written by a person and reviewed
rather than proved. The same epistemic object as a discharge argument here,
with one mechanical difference: it closes the goal rather than annotating it
open. The sameness relation is syntactic shape modulo renaming, which is
coarser than the paper's per-block judgment; a semantic reading merges some
rules, not enough to change the shape of the curve.

Known limits, stated: the package order is dependency depth from `with`
clauses, not the chronological proof order (not recorded in the archive);
the curve does not saturate under any order we tried, since the last unit
adds 8 of 92 distinct rules under all of them.
"""
import re, glob, os, sys, subprocess, functools, collections

REPO = "https://github.com/AdaCore/spark2014"
SUBDIR = "docs/case_study/tokeneer"
SUBDIR14 = "testsuite/gnatprove/tests/tokeneer"

def checkout(root):
    if not os.path.isdir(os.path.join(root, ".git")):
        subprocess.run(["git", "clone", "-q", "--filter=blob:none", "--no-checkout", "--depth", "1", REPO, root], check=True)
        subprocess.run(["git", "-C", root, "sparse-checkout", "init", "--cone"], check=True)
        subprocess.run(["git", "-C", root, "sparse-checkout", "set", SUBDIR, SUBDIR14], check=True)
        subprocess.run(["git", "-C", root, "checkout", "-q", "HEAD"], check=True)
    return os.path.join(root, SUBDIR)

KW = {'may_be_deduced_from', 'may_be_replaced_by', 'are_interchangeable', 'may_be_deduced',
      'and', 'or', 'not', 'for_all', 'for_some', 'mod', 'div', 'true', 'false',
      'element', 'update', 'if', 'goal'}

def shape(body):
    """Canonical form of a rule up to identifier renaming."""
    names = []
    def sub(m):
        w = m.group(0)
        if w in KW or w.startswith('fld_') or w.startswith('upf_'):
            return w
        if w not in names:
            names.append(w)
        return 'V%d' % names.index(w)
    return re.sub(r'[A-Za-z_][A-Za-z0-9_]*', sub, body)

def main():
    tok = sys.argv[1] if len(sys.argv) > 1 else checkout(os.path.abspath("_spark2014"))
    s05 = os.path.join(tok, "SPARK2005")

    # ---- rules
    rules = []  # (package, rule body)
    for f in sorted(glob.glob(os.path.join(s05, "**", "*.rlu"), recursive=True)):
        rel = os.path.relpath(f, s05).split(os.sep)          # core/<pkg>/<proc>.rlu or core/<pkg>.rlu
        pkg = rel[1] if len(rel) > 2 else rel[-1][:-4]
        pkg = pkg.replace('cert_', 'cert')
        txt = open(f, encoding='latin-1').read()
        txt = re.sub(r'/\*.*?\*/', '', txt, flags=re.S)
        txt = re.sub(r'--.*', '', txt)
        for m in re.finditer(r'([a-z_]+)\((\d+)\)\s*:\s*(.*?)\.\s*(?=\n|$)', txt, flags=re.S):
            rules.append((pkg, re.sub(r'\s+', ' ', m.group(3)).strip()))

    # ---- dependency depth from with-clauses (specs and bodies)
    deps = {}
    for f in glob.glob(os.path.join(s05, "core", "*.ad?")):
        p = os.path.basename(f)[:-4]
        txt = open(f, encoding='latin-1').read()
        deps.setdefault(p, set()).update(
            w.lower().split('.')[0] for w in re.findall(r'^\s*with\s+([\w.]+)', txt, flags=re.M))
    pk = set(deps)

    @functools.lru_cache(None)
    def depth(p, stack=()):
        ds = [d for d in deps.get(p, ()) if d in pk and d != p and d not in stack]
        return 0 if not ds else 1 + max(depth(d, stack + (p,)) for d in ds)

    pkgs = sorted({p for p, _ in rules}, key=lambda p: (depth(p) if p in pk else 99, p))

    # ---- cumulative distinct curve
    print(f"{'k':>2} {'package':12s} {'depth':>5} {'rules':>5} {'new':>4} {'A(k)':>5}   prf_-rules")
    seen = set()
    for k, p in enumerate(pkgs, 1):
        prs = [b for q, b in rules if q == p]
        new = [x for x in dict.fromkeys(shape(b) for b in prs) if x not in seen]
        seen |= set(new)
        prf = sum(1 for b in prs if 'prf_' in b)
        print(f"{k:2d} {p:12s} {depth(p) if p in pk else '-':>5} {len(prs):5d} {len(new):4d} {len(seen):5d}   {prf}/{len(prs)}")
    kinds = collections.Counter(
        'deduced_from' if 'may_be_deduced_from' in b else
        'replaced_by' if 'may_be_replaced_by' in b else
        'interchangeable' if 'are_interchangeable' in b else 'other' for _, b in rules)
    print(f"\ntotal rules {len(rules)}; distinct bodies {len(set(b for _, b in rules))}; "
          f"distinct up to renaming {len(seen)}; rules mentioning a prf_ proof function "
          f"{sum(1 for _, b in rules if 'prf_' in b)}")
    print("rule kinds:", dict(kinds))

    # ---- the SPARK 2014 port (maintained testsuite copy): justifications by reason (Table 7)
    t14 = os.path.join(os.path.dirname(os.path.dirname(tok.rstrip("/"))), "..", SUBDIR14) if "case_study" in tok else tok
    t14 = os.path.normpath(os.path.join(tok, "..", "..", "..", SUBDIR14)) if "case_study" in tok else tok
    deps14 = {}
    for f in glob.glob(os.path.join(t14, "*.ad?")):
        q = os.path.basename(f)[:-4]; t = open(f, encoding='latin-1').read()
        deps14.setdefault(q, set()).update(w.lower().split('.')[0] for w in re.findall(r'^\s*with\s+([\w.]+)', t, flags=re.M))
    pk14 = set(deps14)
    @functools.lru_cache(None)
    def depth14(q, stack=()):
        ds = [d for d in deps14.get(q, ()) if d in pk14 and d != q and d not in stack]
        return 0 if not ds else 1 + max(depth14(d, stack + (q,)) for d in ds)
    just = []
    for f in sorted(glob.glob(os.path.join(t14, "*.ad?"))):
        q = os.path.basename(f)[:-4]; t = open(f, encoding='latin-1').read()
        for m in re.finditer(r'pragma\s+Annotate\s*\(\s*GNATprove\s*,\s*(False_Positive|Intentional)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"', t, flags=re.S):
            reason = m.group(3)
            key = "'Image of enum/integer is a short string starting at index 1" if re.search(r'Image of .* short strings starting at index 1', reason) else reason
            just.append((q, m.group(1), key))
    print(f"\nSPARK 2014 (testsuite copy): {len(just)} justified checks, {len(set(k for *_, k in just))} distinct reasons")
    print(f"{'k':>2} {'package':22s} {'depth':>5} {'checks':>6} {'new':>4} {'A(k)':>5}")
    seen14 = set()
    for k, q in enumerate(sorted({q for q, *_ in just}, key=lambda q: (depth14(q), q)), 1):
        js = [j for j in just if j[0] == q]
        new = [x for x in dict.fromkeys(j[2] for j in js) if x not in seen14]; seen14 |= set(new)
        print(f"{k:2d} {q:22s} {depth14(q):5d} {len(js):6d} {len(new):4d} {len(seen14):5d}  {'; '.join(new)}")

if __name__ == '__main__':
    main()
