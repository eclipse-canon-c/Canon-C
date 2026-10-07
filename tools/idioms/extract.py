#!/usr/bin/env python3
"""Record-independent idiom extraction for n(P) (paper §2 and §4.3).

n(P) counts the specification shapes a program P uses that the verified
substrate has never used. For the committed test of §4.3 to be falsifiable,
n(P) must be computed from source text alone — never from WP output, the
deviation record, or whether an argument turned out to be needed. This tool
reads C and ACSL sources and nothing else.

Three layers of features:

  A  ACSL constructs written in the sources: built-ins (\\valid, \\separated,
     ...), clause kinds (requires, behavior, loop invariant, ...), and logic-level
     pointer-to-integer casts.
  B  C constructs that WP models specially: pointer/integer casts, indirect
     calls, unions, variadic definitions and calls, recursion, loops (and loops
     spelled inside macro bodies), goto, floating point, dynamic allocation,
     setjmp/longjmp, VLAs, bit-fields, inline assembly, and the libc functions
     called.
  C  Substrate idioms: macro families instantiated (DEFINE_<FAMILY>_...), with
     the kind of each type argument, and — when measuring a program — every
     repository header it includes that no verified unit includes.

Subcommands:

  extract.py catalogue [--out FILE]
      Build the catalogue from every Frama-C job in the CI workflow, using each
      job's own translation unit and preprocessor flags, so the catalogue covers
      exactly what CI verifies.

  extract.py measure --catalogue FILE [--cpp "FLAGS"] SOURCE...
      Report P's features outside the catalogue, per layer, and n(P).

  extract.py crosscheck [--catalogue FILE]
      Check that every residual class of §3.2 (Appendix B) maps to at least one
      catalogue feature, so the extractor can see what the record says matters.

Feature identities are deliberately coarse (a construct kind, not a count), so
n(P) counts *new kinds of shape*, not new instances of known ones.
"""
import argparse, json, os, re, shutil, subprocess, sys
from collections import defaultdict

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
WORKFLOW = os.path.join(REPO, ".github", "workflows", "cmake-multi-platform.yml")
TOOL_VERSION = "1"

# ------------------------------------------------------------------ workflow
def verified_units():
    """(job, [files], [cpp flags]) for every frama-c-* job in the CI workflow."""
    lines = open(WORKFLOW, encoding="utf-8").read().split("\n")
    jobs = [(i, re.match(r"^  ([a-z0-9-]+):\s*$", l).group(1))
            for i, l in enumerate(lines) if re.match(r"^  ([a-z0-9-]+):\s*$", l)]
    units = []
    for k, (i, name) in enumerate(jobs):
        if not name.startswith("frama-c-"):
            continue
        blk = lines[i:(jobs[k + 1][0] if k + 1 < len(jobs) else len(lines))]
        for j, l in enumerate(blk):
            if re.match(r"^\s*frama-c(\s|$)", l) and ("-wp" in " ".join(blk[j:j + 4])):
                cmd = []
                for l2 in blk[j:j + 60]:
                    cmd.append(l2.strip().rstrip("\\").strip())
                    if not l2.rstrip().endswith("\\"):
                        break
                c = " ".join(cmd)
                cpp = re.search(r'-cpp-extra-args="([^"]*)"', c)
                files = re.findall(r'\s((?:core|data|semantics|util|algo|vmacros)/[\w/.-]+\.[ch])\b', c)
                if files:
                    units.append((name, files, cpp.group(1).split() if cpp else []))
                break
    return units

# ------------------------------------------------------------------ clang
def clang_flags(cpp):
    out, it = [], iter(cpp)
    for tok in it:
        if tok == "-I":
            out.append("-I" + next(it))
        else:
            out.append(tok)
    return out

CLANG = os.environ.get("CLANG", "clang")

def require_clang():
    if shutil.which(CLANG) is None:
        raise SystemExit("extract.py: '%s' not found. It needs clang (any recent version), e.g.\n"
                         "    sudo apt install clang        (Debian/Ubuntu)\n"
                         "    sudo dnf install clang        (Fedora)\n"
                         "or point CLANG at a binary: CLANG=clang-18 python3 tools/idioms/extract.py ..." % CLANG)

def run_clang(files, cpp):
    require_clang()
    base = [CLANG, "-x", "c", "-std=gnu99", "-w"] + clang_flags(cpp)
    src = files[0] if len(files) == 1 else None
    if src is None:   # several files: include them all from one stub TU
        stub = os.path.join("/tmp", "nP_stub.c")
        open(stub, "w").write("".join('#include "%s"\n' % os.path.join(REPO, f) for f in files))
        src = stub
    ast = subprocess.run(base + ["-fsyntax-only", "-Xclang", "-ast-dump=json", src],
                         cwd=REPO, capture_output=True, text=True)
    if ast.returncode != 0:
        raise SystemExit("clang failed on %s:\n%s" % (files, ast.stderr[-2000:]))
    dep = subprocess.run(base + ["-M", src], cwd=REPO, capture_output=True, text=True)
    if dep.returncode != 0:
        raise SystemExit("clang -M failed on %s:\n%s" % (files, dep.stderr[-2000:]))
    deps = [d for d in dep.stdout.replace("\\\n", " ").split()[1:]]
    return json.loads(ast.stdout), deps

def repo_rel(path):
    p = os.path.normpath(os.path.join(REPO, path)) if not os.path.isabs(path) else os.path.normpath(path)
    return os.path.relpath(p, REPO) if p.startswith(REPO + os.sep) else None

# ------------------------------------------------------------------ layer A
ACSL_CLAUSES = ["requires", "ensures", "assigns", "assumes", "behavior",
                "complete behaviors", "disjoint behaviors", "terminates", "exits",
                "decreases", "loop invariant", "loop assigns", "loop variant",
                "predicate", "logic", "lemma", "axiomatic", "inductive", "ghost",
                "calls", "allocates", "frees", "global invariant", "type invariant",
                "check", "admit", "reads"]

CLAUSE_RE = re.compile(r"^(?:check\s+|admit\s+)?(" + "|".join(
    c.replace(" ", r"\s+") for c in sorted(ACSL_CLAUSES, key=len, reverse=True)) + r")\b")

def acsl_blocks(text):
    """ACSL annotation bodies, with comments inside them removed."""
    blocks = re.findall(r"/\*@(.*?)\*/", text, re.S) + re.findall(r"//@([^\n]*)", text)
    return [re.sub(r"//[^\n]*", "", b) for b in blocks]

def acsl_features(text):
    feats = set()
    for block in acsl_blocks(text):
        for b in re.findall(r"\\[A-Za-z_]+", block):
            feats.add("acsl-builtin:" + b)
        for line in block.split("\n"):
            line = line.strip().lstrip("@").strip()
            m = CLAUSE_RE.match(line)
            if m:
                feats.add("acsl-clause:" + " ".join(m.group(1).split()))
                if line.startswith(("check ", "admit ")):
                    feats.add("acsl-clause:" + line.split()[0])
        flat = " ".join(block.split())
        if re.search(r"\(\s*(?:uintptr_t|intptr_t|usize|size_t|unsigned long)\s*\)\s*\(?\s*[A-Za-z_][\w>.-]*", flat) and \
           re.search(r"uintptr_t|unsigned long\)", flat):
            feats.add("acsl-logic:pointer-to-integer cast")
    return feats

CONTRACT_DECL = re.compile(r"/\*@(.*?)\*/\s*((?:static\s+|inline\s+|extern\s+|const\s+)*[A-Za-z_][\w\s]*?[\s\*]+)([A-Za-z_]\w*)\s*\(", re.S)

def contract_patterns(text):
    """Contract shapes behind known residual mechanisms (class (e) cascades).

    - a behavior of a value-returning function that never constrains \\result:
      under it, callers learn nothing (the empty ptr.h behaviors VERIFY-023
      filled);
    - a pointer-returning function whose contract never constrains \\result:
      the opaque-result shape (VERIFY-023, VERIFY-036 F6).
    """
    feats = set()
    for block, rtype, name in CONTRACT_DECL.findall(text):
        block = re.sub(r"//[^\n]*", "", block)
        flat = " ".join(block.replace("\\\n", " ").split())
        flat = re.sub(r"\\let\s+\w+\s*=\s*[^;]*;", " ", flat)   # a \let's ';' does not end the clause
        if "*" in rtype and not re.search(r"ensures[^;]*\\result", flat):
            feats.add("acsl-pattern:pointer result left unconstrained")
        if re.fullmatch(r"(?:static\s+|inline\s+|extern\s+)*void\s*", rtype):
            continue                      # a void function's behaviors only frame
        sections = re.split(r"\bbehavior\s+\w+\s*:", flat)
        for sec in sections[1:]:
            sec = re.split(r"\b(?:complete|disjoint)\s+behaviors\b", sec)[0]
            if "assumes" in sec and not re.search(r"ensures[^;]*\\result", sec):
                feats.add("acsl-pattern:behavior leaves the result unconstrained")
    return feats

# ------------------------------------------------------------------ layers B and C
ALLOC = {"malloc", "calloc", "realloc", "free", "aligned_alloc"}
SETJMP = {"setjmp", "longjmp", "_setjmp", "_longjmp", "sigsetjmp", "siglongjmp"}

class Walker:
    """Walks clang's JSON AST, tracking clang's delta-encoded source files."""
    def __init__(self, scope):
        self.scope = scope          # set of repo-relative files whose code counts
        self.cur = None
        self.feats = set()
        self.calls = defaultdict(set)   # caller -> callees (for recursion)
        self.funcs_in_scope = set()
        self.variadic = set()
        self.decl_file = {}

    def _upd(self, loc):
        if not isinstance(loc, dict):
            return None, False
        macro = "spellingLoc" in loc or "expansionLoc" in loc
        spell = exp = None
        for key in ("spellingLoc", "expansionLoc"):
            sub = loc.get(key)
            if isinstance(sub, dict) and "file" in sub:
                self.cur = sub["file"]
            if key == "spellingLoc":
                spell = self.cur
            else:
                exp = self.cur
        if "file" in loc:
            self.cur = loc["file"]
        return (spell, exp, self.cur), macro

    def node_files(self, node):
        files, macro = set(), False
        for part in (node.get("loc"), (node.get("range") or {}).get("begin"), (node.get("range") or {}).get("end")):
            f, m = self._upd(part)
            if f:
                files |= {x for x in f if x}
            macro = macro or m
        return files, macro

    def in_scope(self, files):
        return any(repo_rel(f) in self.scope for f in files if f)

    def walk(self, node, func=None, func_scoped=False):
        if not isinstance(node, dict):
            return
        files, macro = self.node_files(node)
        kind = node.get("kind")
        if kind == "FunctionDecl":
            func = node.get("name")
            func_scoped = self.in_scope(files) and any(c.get("kind") == "CompoundStmt" for c in node.get("inner", []))
            if func_scoped:
                self.funcs_in_scope.add(func)
            if node.get("variadic"):
                self.variadic.add(func)
                if func_scoped:
                    self.feats.add("c:variadic definition")
        elif kind == "RecordDecl" and self.in_scope(files):
            if node.get("tagUsed") == "union":
                self.feats.add("c:union")
            for c in node.get("inner", []):
                if c.get("kind") == "FieldDecl" and c.get("isBitfield"):
                    self.feats.add("c:bit-field")
        if func_scoped:
            self.body_node(node, kind, func, macro)
        for c in node.get("inner", []):
            self.walk(c, func, func_scoped)

    def body_node(self, node, kind, func, macro):
        ty = node.get("type") or {}
        t = ty.get("qualType", "") + " " + ty.get("desugaredQualType", "")
        if re.search(r"\b(float|double)\b", t):
            self.feats.add("c:floating point")
        if kind in ("CStyleCastExpr", "ImplicitCastExpr"):
            ck = node.get("castKind")
            if ck == "PointerToIntegral":
                self.feats.add("c:pointer-to-integer cast")
            elif ck == "IntegralToPointer":
                self.feats.add("c:integer-to-pointer cast")
        elif kind in ("ForStmt", "WhileStmt", "DoStmt"):
            self.feats.add("c:loop")
            if macro:
                self.feats.add("c:loop in a macro body")
        elif kind == "BinaryOperator" or kind == "CompoundAssignOperator":
            op = node.get("opcode", "")
            if op in ("&", "|", "^", "<<", ">>", "&=", "|=", "^=", "<<=", ">>="):
                self.feats.add("c:bitwise or shift operator")
            elif op in ("*", "/", "%", "*=", "/=", "%="):
                operands = node.get("inner", [])
                if len(operands) == 2 and not any(self.is_constant(o) for o in operands):
                    self.feats.add("c:nonlinear arithmetic")
        elif kind == "UnaryOperator" and node.get("opcode") == "~":
            self.feats.add("c:bitwise or shift operator")
        elif kind == "GotoStmt":
            self.feats.add("c:goto")
        elif kind == "GCCAsmStmt":
            self.feats.add("c:inline assembly")
        elif kind == "VarDecl" and "[" in t and re.search(r"\[[^\]0-9]", t):
            self.feats.add("c:variable-length array")
        elif kind == "CallExpr":
            callee = self.direct_callee(node)
            if callee is None:
                self.feats.add("c:indirect call")
            else:
                self.calls[func].add(callee)
                if callee in ALLOC:
                    self.feats.add("c:dynamic allocation")
                if callee in SETJMP:
                    self.feats.add("c:setjmp/longjmp")

    @staticmethod
    def is_constant(e):
        while e.get("kind") in ("ImplicitCastExpr", "ParenExpr", "CStyleCastExpr") and e.get("inner"):
            e = e["inner"][0]
        return e.get("kind") in ("IntegerLiteral", "CharacterLiteral", "UnaryExprOrTypeTraitExpr") or \
               (e.get("kind") == "DeclRefExpr" and (e.get("referencedDecl") or {}).get("kind") == "EnumConstantDecl")

    @staticmethod
    def direct_callee(call):
        inner = call.get("inner", [])
        if not inner:
            return None
        e = inner[0]
        while e.get("kind") in ("ImplicitCastExpr", "ParenExpr") and e.get("inner"):
            e = e["inner"][0]
        if e.get("kind") == "DeclRefExpr":
            ref = e.get("referencedDecl", {})
            if ref.get("kind") == "FunctionDecl":
                return ref.get("name")
        return None

    def finish(self, system_funcs):
        # variadic calls, libc calls, recursion
        for caller, callees in self.calls.items():
            for c in callees:
                if c in self.variadic:
                    self.feats.add("c:variadic call")
                if c in system_funcs and c not in ALLOC and c not in SETJMP:
                    self.feats.add("libc:" + c)     # allocation and setjmp have their own features
        graph = {f: {c for c in cs if c in self.funcs_in_scope} for f, cs in self.calls.items() if f in self.funcs_in_scope}
        if has_cycle(graph):
            self.feats.add("c:recursion")
        return self.feats

def has_cycle(g):
    WHITE, GREY, BLACK = 0, 1, 2
    col = defaultdict(int)
    def dfs(u):
        col[u] = GREY
        for v in g.get(u, ()):
            if col[v] == GREY or (col[v] == WHITE and dfs(v)):
                return True
        col[u] = BLACK
        return False
    sys.setrecursionlimit(100000)
    return any(col[u] == WHITE and dfs(u) for u in list(g))

def system_functions(ast):
    """Functions declared only in non-repo (system or Frama-C) headers."""
    names, cur = {}, None
    for n in ast.get("inner", []):
        loc = n.get("loc", {})
        for key in ("spellingLoc", "expansionLoc"):
            if isinstance(loc.get(key), dict) and "file" in loc[key]:
                cur = loc[key]["file"]
        if "file" in loc:
            cur = loc["file"]
        rng = n.get("range", {})
        for part in (rng.get("begin", {}), rng.get("end", {})):
            for key in ("spellingLoc", "expansionLoc"):
                if isinstance(part.get(key), dict) and "file" in part[key]:
                    cur = part[key]["file"]
            if "file" in part:
                cur = part["file"]
        if n.get("kind") == "FunctionDecl":
            repo = repo_rel(cur) is not None if cur else False
            nm = n.get("name")
            names[nm] = names.get(nm, False) or repo
    return {nm for nm, repo in names.items() if not repo}

def type_kinds(ast):
    """typedef name -> kind of its desugared type."""
    kinds = {}
    for n in ast.get("inner", []):
        if n.get("kind") == "TypedefDecl":
            t = n.get("type", {})
            sugared, desugared = t.get("qualType", ""), t.get("desugaredQualType", "")
            k = classify_type(sugared)
            kinds[n.get("name")] = k if k in ("struct", "union", "enum", "pointer") else classify_type(desugared or sugared)
    return kinds

INTEGER = {"int", "unsigned", "signed", "char", "short", "long", "_Bool", "bool",
           "unsigned int", "unsigned char", "unsigned short", "unsigned long",
           "long long", "unsigned long long", "signed char"}

def classify_type(q):
    q = q.replace("const ", "").replace("volatile ", "").strip()
    if "*" in q:
        return "pointer"
    if q.startswith("struct"):
        return "struct"
    if q.startswith("union"):
        return "union"
    if q.startswith("enum"):
        return "enum"
    if re.search(r"\b(float|double)\b", q):
        return "floating"
    if q in INTEGER or re.fullmatch(r"(unsigned |signed )?(char|short|int|long( long)?)( int)?", q):
        return "integer"
    return "other"

def family_features(scope_files, kinds):
    feats = set()
    for f in scope_files:
        try:
            text = open(os.path.join(REPO, f), encoding="utf-8", errors="ignore").read()
        except OSError:
            continue
        text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)      # comments are not code
        text = re.sub(r"//[^\n]*", "", text)
        logical = re.sub(r"\\\n", " ", text)
        for line in logical.split("\n"):
            if line.lstrip().startswith("#"):
                continue
            for fam, part, args in re.findall(r"\bDEFINE_([A-Z]+)_([A-Z_]+)\s*\(([^)]*)\)", line):
                a = [x.strip() for x in args.split(",") if x.strip()]
                if part == "FUNCTIONS" and a:
                    a = a[1:]                      # first argument is the linkage
                tk = "+".join(kinds.get(x, classify_type(x)) for x in a)
                feats.add("family:" + fam.lower())
                feats.add("family:%s@%s" % (fam.lower(), tk))
    return feats

# ------------------------------------------------------------------ one unit
def features_of(files, cpp, scope_files=None):
    ast, deps = run_clang(files, cpp)
    closure = sorted({r for r in (repo_rel(d) for d in deps) if r})
    scope = set(scope_files) if scope_files is not None else set(closure)
    w = Walker(scope)
    w.walk(ast)
    feats = w.finish(system_functions(ast))
    for f in scope:
        try:
            txt = open(os.path.join(REPO, f), encoding="utf-8", errors="ignore").read()
            feats |= acsl_features(txt) | contract_patterns(txt)
        except OSError:
            pass
    feats |= family_features(scope, type_kinds(ast))
    return feats, closure

def layer(feat):
    if feat.startswith("acsl"):
        return "A"
    if feat.startswith(("c:", "libc:")):
        return "B"
    return "C"

# ------------------------------------------------------------------ commands
def cmd_catalogue(args):
    cat = {"A": defaultdict(list), "B": defaultdict(list), "C": defaultdict(list)}
    closure_all, units = set(), []
    for name, files, cpp in verified_units():
        feats, closure = features_of(files, cpp)
        units.append({"job": name, "files": files, "cpp": cpp, "features": len(feats)})
        closure_all |= set(closure)
        for f in feats:
            cat[layer(f)][f].append(name)
        print("  %-24s %3d features, %2d repo files in closure" % (name, len(feats), len(closure)), file=sys.stderr)
    head = args.commit or subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=REPO,
                                         capture_output=True, text=True).stdout.strip()
    clang = subprocess.run([CLANG, "--version"], capture_output=True, text=True).stdout.split("\n")[0]
    out = {"meta": {"tool_version": TOOL_VERSION, "substrate_commit": head or "unknown",
                    "clang": clang, "workflow": os.path.relpath(WORKFLOW, REPO)},
           "verified_units": units,
           "verified_closure": sorted(closure_all),
           "A": {k: sorted(set(v)) for k, v in sorted(cat["A"].items())},
           "B": {k: sorted(set(v)) for k, v in sorted(cat["B"].items())},
           "C": {k: sorted(set(v)) for k, v in sorted(cat["C"].items())}}
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(out, fh, indent=1)
        fh.write("\n")
    print("catalogue: %d A, %d B, %d C features over %d units -> %s" %
          (len(out["A"]), len(out["B"]), len(out["C"]), len(units), args.out), file=sys.stderr)

def cmd_measure(args):
    cat = json.load(open(args.catalogue, encoding="utf-8"))
    known = set(cat["A"]) | set(cat["B"]) | set(cat["C"])
    cpp = args.cpp.split() if args.cpp else ["-I", "core/primitives", "-I", "core", "-I", "semantics", "-I", "data", "-I", ".",
                                            "-DCANON_NO_REQUIRE", "-DNDEBUG"]
    srcs = [os.path.abspath(f) for f in args.sources]
    own = [repo_rel(f) or f for f in srcs]
    _, deps = run_clang(srcs, cpp)
    closure = {r for r in (repo_rel(d) for d in deps) if r}       # repository files only
    unverified = sorted(closure - set(cat["verified_closure"]) - set(own))
    feats, _ = features_of(srcs, cpp, scope_files=set(own) | set(unverified))
    feats |= {"unverified header:" + h for h in unverified}
    new = sorted(f for f in feats if f not in known)
    report = {"sources": own, "catalogue_commit": cat["meta"]["substrate_commit"],
              "features": sorted(feats), "outside_catalogue": new,
              "n_by_layer": {L: sum(1 for f in new if layer(f) == L) for L in "ABC"},
              "n": len(new)}
    print(json.dumps(report, indent=1))

APPENDIX_B = os.path.join(os.path.dirname(os.path.abspath(__file__)), "appendix_b_map.json")

def cmd_crosscheck(args):
    """Every residual class of §3.2 / Appendix B must be visible to the extractor."""
    cat = json.load(open(args.catalogue, encoding="utf-8"))
    known = set(cat["A"]) | set(cat["B"]) | set(cat["C"])
    amap = json.load(open(APPENDIX_B, encoding="utf-8"))
    bad = 0
    for cls, entry in amap["classes"].items():
        present = [f for f in entry["features"] if f in known]
        missing = [f for f in entry["features"] if f not in known]
        status = "ok" if present else "NOT VISIBLE"
        bad += 0 if present else 1
        print("  (%s) %-30s %s: %d of %d mapped features in the catalogue%s" % (
            cls, entry["name"], status, len(present), len(entry["features"]),
            ("; absent: " + ", ".join(missing)) if missing else ""))
    unmapped = sorted(known - {f for e in amap["classes"].values() for f in e["features"]})
    print("  catalogue features mapped to no class (allowed): %d" % len(unmapped))
    if bad:
        raise SystemExit("crosscheck: %d class(es) invisible to the extractor" % bad)
    print("crosscheck: every class maps to at least one catalogue feature")

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("catalogue")
    c.add_argument("--out", default=os.path.join(REPO, "tools", "idioms", "catalogue.json"))
    c.add_argument("--commit", default=None, help="substrate commit to record (default: git HEAD)")
    m = sub.add_parser("measure")
    m.add_argument("--catalogue", default=os.path.join(REPO, "tools", "idioms", "catalogue.json"))
    m.add_argument("--cpp", default=None)
    m.add_argument("sources", nargs="+")
    x = sub.add_parser("crosscheck")
    x.add_argument("--catalogue", default=os.path.join(REPO, "tools", "idioms", "catalogue.json"))
    a = ap.parse_args()
    {"catalogue": cmd_catalogue, "measure": cmd_measure, "crosscheck": cmd_crosscheck}[a.cmd](a)

if __name__ == "__main__":
    main()
