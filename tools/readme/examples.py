#!/usr/bin/env python3
"""examples.py: the README's examples, built (VERIFY-109). Run from the
repository root; 17 · README examples runs it.

Usage, `my_types.h` and `main.c`, is written out as the README shows it,
built with each compiler at the project's warning flags, and run: it must
print "first even: 8" and exit 0, as its text says.

Each example's "WITH Canon-C" block is compiled inside its scaffold, a file
of this folder whose line `// block:` is the block's first line. The
scaffold gives what the block leaves to the program around it: the Option
and Result types the README says the examples assume (`my_types.h` here),
stand-ins for the program's own names, and, for statements the block shows
at file scope, a function to hold them. It gives no header of Canon-C's:
each block includes what it uses. The block's opening comments and
directives go to `/*@DIRECTIVES@*/`; the rest to `/*@FILE@*/`, at file
scope, or to `/*@BODY@*/`, in the function, or, where the scaffold names a
line of the block with `// split:`, the part before that line to the one
and the part from it to the other. A scaffold's `// flags:` line adds
compiler flags, each for a part the block leaves to "...", said below it.
`#line` directives make a diagnostic name the README's line. Each block is
compiled, not linked, at -O0 and -O2. The "WITHOUT Canon-C" blocks are
plain C to compare with, and are not built.

    CANON_README_CC  the compilers, by name (gcc clang)
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.getcwd()
HERE = os.path.join(ROOT, 'tools', 'readme')
README = os.path.join(ROOT, 'README.md')
WARN = ['-std=c99', '-Wall', '-Wextra', '-Wpedantic', '-Werror']
USAGE_OUT = 'first even: 8\n'


def blocks(path):
    """The README's ```c blocks: (line of the block's first line, lines)."""
    lines = open(path, encoding='utf-8').read().split('\n')
    out, i = [], 0
    while i < len(lines):
        if lines[i].strip() == '```c':
            j = i + 1
            while j < len(lines) and not lines[j].startswith('```'):
                j += 1
            out.append((i + 2, lines[i + 1:j]))
            i = j
        i += 1
    return out


def scaffolds():
    """Each scaffold: its block's first line, split line, flags and text."""
    out = {}
    for name in sorted(os.listdir(HERE)):
        if not name.endswith('.c'):
            continue
        text = open(os.path.join(HERE, name), encoding='utf-8').read()
        meta = dict(re.findall(r'^// (block|split|flags): (.*)$', text, re.M))
        if 'block' not in meta:
            sys.exit(f'FAIL: tools/readme/{name} names no block')
        out[meta['block'].strip()] = (name, meta.get('split', '').strip(),
                                      meta.get('flags', '').split(), text)
    return out


def split(body):
    """The block's opening comments and directives, and the rest."""
    n = 0
    while n < len(body):
        s = body[n].strip()
        if not (s == '' or s.startswith('#') or (s.startswith('/*') and s.endswith('*/'))):
            break
        n += 1
    return n


def place(name, text, parts):
    """The scaffold with each part in place, between #line directives."""
    out = []
    for k, line in enumerate(text.split('\n'), 1):
        marker = re.fullmatch(r'/\*@(DIRECTIVES|FILE|BODY)@\*/', line.strip())
        if not marker:
            out.append(line)
            continue
        first, lines = parts.get(marker.group(1), (None, []))
        if lines:
            out.append(f'#line {first} "README.md"')
            out.extend(lines)
        out.append(f'#line {k + 1} "tools/readme/{name}"')
    return '\n'.join(out)


def run(cmd, cwd=None):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    return p.returncode, p.stdout, p.stderr


def main():
    ccs = os.environ.get('CANON_README_CC', 'gcc clang').split()
    versions = []
    for cc in ccs:
        st, out, err = run([cc, '--version'])
        versions.append((out or err).split('\n')[0].strip())
        print(f'{cc}: {versions[-1]}')
    fails = 0
    usage_ok = compiled = compiles = 0
    all_blocks = blocks(README)
    tmp = tempfile.mkdtemp(prefix='canon-readme-')

    # Usage, as the README shows it, built and run.
    usage = {}
    for first, body in all_blocks:
        m = re.match(r'/\* (my_types\.h|main\.c)\b', body[0] if body else '')
        if m:
            usage.setdefault(m.group(1), []).append((first, body))
    print('=== Usage: my_types.h and main.c, built and run ===')
    if sorted(usage) != ['main.c', 'my_types.h'] or any(len(v) != 1 for v in usage.values()):
        print(f'FAIL: Usage needs one my_types.h and one main.c block; found {sorted(usage)}')
        return 1
    udir = os.path.join(tmp, 'usage')
    os.mkdir(udir)
    for fname, [(first, body)] in usage.items():
        with open(os.path.join(udir, fname), 'w', encoding='utf-8') as f:
            f.write('\n'.join(body) + '\n')
    for cc in ccs:
        exe = os.path.join(udir, f'main-{cc}')
        st, out, err = run([cc] + WARN + ['-I', ROOT, '-I', '.', 'main.c', '-o', exe], cwd=udir)
        if st != 0:
            print(f'FAIL: Usage, {cc}: does not build'); print(err.rstrip()); fails += 1
            continue
        st, out, err = run([exe])
        if st == 0 and out == USAGE_OUT and err == '':
            print(f'  {cc}: built, ran: {out.strip()!r}, exit 0')
            usage_ok += 1
        else:
            print(f'FAIL: Usage, {cc}: exit {st}, out {out!r}, err {err!r}'); fails += 1

    # Each WITH block in its scaffold, compiled.
    print("=== The examples' WITH Canon-C blocks, compiled in their scaffolds ===")
    sc = scaffolds()
    used = set()
    withs = [(f, b) for f, b in all_blocks if b and b[0].startswith('/* WITH Canon-C')]
    for first, body in withs:
        key = body[0].strip()
        if key not in sc:
            print(f'FAIL: README.md:{first}: no scaffold names this block: {key}'); fails += 1
            continue
        used.add(key)
        name, at, flags, text = sc[key]
        n = split(body)
        rest_first, rest = first + n, body[n:]
        parts = {'DIRECTIVES': (first, body[:n])}
        if at:
            hits = [i for i, l in enumerate(rest) if l.strip() == at]
            if len(hits) != 1:
                print(f'FAIL: {name}: its split line is in the block {len(hits)} times'); fails += 1
                continue
            parts['FILE'] = (rest_first, rest[:hits[0]])
            parts['BODY'] = (rest_first + hits[0], rest[hits[0]:])
        elif '/*@BODY@*/' in text:
            parts['BODY'] = (rest_first, rest)
        else:
            parts['FILE'] = (rest_first, rest)
        src = os.path.join(tmp, name)
        with open(src, 'w', encoding='utf-8') as f:
            f.write(place(name, text, parts))
        results = []
        for cc in ccs:
            for opt in ('-O0', '-O2'):
                st, out, err = run([cc] + WARN + flags + [opt, '-I', ROOT, '-I', HERE,
                                                          '-c', src, '-o', src + '.o'])
                if st != 0:
                    print(f'FAIL: README.md:{first} ({name}), {cc} {opt}:'); print(err.rstrip())
                    fails += 1
                results.append(st == 0)
        compiled += sum(results)
        compiles += len(results)
        print(f'  README.md:{first} {name}: {sum(results)} of {len(results)} compiled'
              + (f' (with {" ".join(flags)})' if flags else ''))
    for key, (name, _, _, _) in sorted(sc.items()):
        if key not in used:
            print(f'FAIL: tools/readme/{name}: no block of the README is its: {key}'); fails += 1

    names = ' and '.join(ccs)
    summary = (f'Usage built with {names} and run, "{USAGE_OUT.strip()}" and exit 0: '
               f'{usage_ok} of {len(ccs)}; {len(withs)} examples compiled in their scaffolds '
               f'with {names} at -O0 and -O2: {compiled} of {compiles}; {fails} failed')
    print(summary)
    if os.environ.get('GITHUB_ACTIONS'):
        print(f'::notice title=readme::{summary}')   # VERIFY-050
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
