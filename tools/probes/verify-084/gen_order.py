#!/usr/bin/env python3
"""
gen_order.py -- writes VERIFY-084's two WP drivers for the 24 comparators of
core/primitives/compare.h, so that each wrapper is the same text with only
the comparator and its type changed.

  compare_order_verify.h  for each comparator, three functions that call it
                          and assert that it is reflexive, antisymmetric and
                          transitive (see the file's header);
  compare_order_false.h   the negative control: for each comparator, a
                          function whose assertion, symmetric, is false
                          whenever the two values compare unequal; and for
                          f32 and f64, one whose assertion, self_equal, that
                          C's x == x holds, is false for a NaN.

Both are written next to this script.

    python3 tools/probes/verify-084/gen_order.py           write both
    python3 tools/probes/verify-084/gen_order.py --check   exit 1 if a file
                                                           differs from what
                                                           it would write
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))

# compare.h's order: unsigned, signed, size, floating point; each ascending,
# then descending.
TYPES = ['u8', 'u16', 'u32', 'u64', 'i8', 'i16', 'i32', 'i64', 'usize', 'isize', 'f32', 'f64']
COMPARATORS = [(t, d) for t in TYPES for d in ('', '_desc')]
NAMES = [f'algo_cmp_{t}{d}' for t, d in COMPARATORS]

LICENCE = """\
/***************************************************************************
 * Copyright (C) 2026 Eclipse Canon-C contributors
 *
 * This program and the accompanying materials are made available under the
 * terms of the MIT License which is available at
 * https://opensource.org/licenses/MIT.
 *
 * AI Disclosure: This file was largely AI-generated.
 * The AI-generated portions may be considered public domain (CC0-1.0)
 * and not subject to the project's licence. The human contributor has
 * reviewed and verified that the code is correct.
 *
 * SPDX-License-Identifier: MIT AND CC0-1.0
 **************************************************************************/
"""

ORDER_HEAD = """\
/* {path}{pad}[VERIFY-084]
 * ============================================================================
 * WP driver: each of compare.h's 24 comparators is a total order.
 * Written by gen_order.py (tools/probes/verify-084/); edit that, not this.
 *
 * algo_cmp_fn's documentation (core/primitives/compare.h) asks a comparator
 * to be reflexive, antisymmetric and transitive. The contract of each of
 * compare.h's comparators states that its result is -1, 0 or 1, and nothing
 * that relates two calls, so WP cannot derive any of the three from it.
 *
 * Here each comparator cmp has three functions that call it and assert the
 * property of its results, for any a, b and c that meet its precondition
 * and any ctx:
 *
 *   order_T_reflexive      reflexive      cmp(a, a) == 0
 *   order_T_antisymmetric  antisymmetric  cmp(b, a) == -cmp(a, b)
 *   order_T_transitive     transitive_lt  cmp(a, b) < 0 && cmp(b, c) < 0
 *                                           ==> cmp(a, c) < 0
 *                          transitive_eq  cmp(a, b) == 0 && cmp(b, c) == 0
 *                                           ==> cmp(a, c) == 0
 *
 * With antisymmetry the two transitive ones give the rest of what a sort
 * needs: cmp(a, b) > 0 && cmp(b, c) > 0 ==> cmp(a, c) > 0, and when
 * cmp(a, b) == 0, cmp(a, c) and cmp(b, c) have the same sign.
 *
 * WP proves them with Frama-C's -inline-calls and -remove-inlined, each
 * given the 24 comparators' names: each call is replaced by the
 * comparator's body, and a _desc comparator's call to its ascending one
 * too, so the assertions are proved of the comparators' code. The kernel
 * keeps each inlined body's contract as a statement contract, which WP has
 * not supported since Frama-C 23 (its manual, 1.6 and 2.1); the reads of
 * the inlined bodies are checked by -wp-rte's run-time checks instead. The
 * comparators then leave the analysed unit; frama-c-compare proves them.
 * Frama-C's RPP plug-in proves such properties by self-composition too,
 * but needs Frama-C 33 (docs/for-future.md).
 * ============================================================================ */
"""

FALSE_HEAD = """\
/* {path}{pad}[VERIFY-084]
 * ============================================================================
 * The negative control of compare_order_verify.h: assertions that are
 * false, which WP must not prove with the same options.
 * Written by gen_order.py (tools/probes/verify-084/); edit that, not this.
 *
 *   order_false_T_symmetric  symmetric   cmp(b, a) == cmp(a, b): false
 *                                        whenever a and b compare unequal
 *   order_false_T_self_equal self_equal  for f32 and f64, that C's x == x
 *                                        holds: false for a NaN
 *
 * If WP proved one, the hypotheses would be contradictory, or WP's model
 * would have no unequal values or, for self_equal, no NaN, and the proofs
 * of compare_order_verify.h would say nothing about those cases.
 * ============================================================================ */
"""


WIDTH = 79   # the drivers' lines, as vmacros/vdrivers/'s headers have them


def head(template, path):
    pad = ' ' * max(1, WIDTH - len('/* ') - len(path) - len('[VERIFY-084]'))
    return template.format(path=path, pad=pad)


def requires(t, names):
    return ''.join(f'    requires \\valid_read((char *){n} + (0 .. sizeof({t}) - 1));\n'
                   for n in names)


def section(t):
    title = f' {t} '
    rule = '\u2500' * (WIDTH - len('/* ') - 2 - len(title) - len(' */'))
    return '/* ' + '\u2500' * 2 + title + rule + ' */\n\n'


def signature(name, params):
    """void name(params), the parameters wrapped under the first if the line
    would be longer than WIDTH."""
    one = f'void {name}({", ".join(params)})'
    if len(one) <= WIDTH:
        return one + '\n'
    lines, cur = [], f'void {name}('
    indent = ' ' * len(cur)
    for i, p in enumerate(params):
        piece = p + (', ' if i < len(params) - 1 else ')')
        if len(cur) + len(piece.rstrip()) > WIDTH and cur.strip() != f'void {name}(':
            lines.append(cur.rstrip())
            cur = indent
        cur += piece
    lines.append(cur.rstrip())
    return '\n'.join(lines) + '\n'


def order_wrappers(t, d):
    cmp, w = f'algo_cmp_{t}{d}', f'order_{t}{d}'
    return (
        f'/*@\n{requires(t, "a")}'
        f'    assigns  \\nothing;\n */\n'
        + signature(f'{w}_reflexive', ['const void *a', 'void *ctx']) +
        f'{{\n'
        f'    int aa = {cmp}(a, a, ctx);\n'
        f'    /*@ assert reflexive: aa == 0; */\n'
        f'    (void)aa;\n'
        f'}}\n\n'
        f'/*@\n{requires(t, "ab")}'
        f'    assigns  \\nothing;\n */\n'
        + signature(f'{w}_antisymmetric', ['const void *a', 'const void *b', 'void *ctx']) +
        f'{{\n'
        f'    int ab = {cmp}(a, b, ctx);\n'
        f'    int ba = {cmp}(b, a, ctx);\n'
        f'    /*@ assert antisymmetric: ba == -ab; */\n'
        f'    (void)ab;\n'
        f'    (void)ba;\n'
        f'}}\n\n'
        f'/*@\n{requires(t, "abc")}'
        f'    assigns  \\nothing;\n */\n'
        + signature(f'{w}_transitive',
                    ['const void *a', 'const void *b', 'const void *c', 'void *ctx']) +
        f'{{\n'
        f'    int ab = {cmp}(a, b, ctx);\n'
        f'    int bc = {cmp}(b, c, ctx);\n'
        f'    int ac = {cmp}(a, c, ctx);\n'
        f'    /*@ assert transitive_lt: ab < 0 && bc < 0 ==> ac < 0; */\n'
        f'    /*@ assert transitive_eq: ab == 0 && bc == 0 ==> ac == 0; */\n'
        f'    (void)ab;\n'
        f'    (void)bc;\n'
        f'    (void)ac;\n'
        f'}}\n\n')


def false_wrappers(t, d):
    cmp, w = f'algo_cmp_{t}{d}', f'order_false_{t}{d}'
    text = (
        f'/*@\n{requires(t, "ab")}'
        f'    assigns  \\nothing;\n */\n'
        + signature(f'{w}_symmetric', ['const void *a', 'const void *b', 'void *ctx']) +
        f'{{\n'
        f'    int ab = {cmp}(a, b, ctx);\n'
        f'    int ba = {cmp}(b, a, ctx);\n'
        f'    /*@ assert symmetric: ba == ab; */\n'
        f'    (void)ab;\n'
        f'    (void)ba;\n'
        f'}}\n\n')
    if t in ('f32', 'f64') and not d:
        text += (
            f'/*@\n{requires(t, "a")}'
            f'    assigns  \\nothing;\n */\n'
            + signature(f'order_false_{t}_self_equal', ['const void *a']) +
            f'{{\n'
            f'    {t} x = *(const {t} *)a;\n'
            f'    int eq = (x == x);\n'
            f'    /*@ assert self_equal: eq == 1; */\n'
            f'    (void)eq;\n'
            f'}}\n\n')
    return text


def driver(path, template, guard, wrappers):
    out = [LICENCE, '\n', head(template, path), '\n',
           f'#ifndef {guard}\n#define {guard}\n\n',
           '#include "core/primitives/compare.h"\n\n']
    for t in TYPES:
        out.append(section(t))
        for d in ('', '_desc'):
            out.append(wrappers(t, d))
    out.append(f'#endif /* {guard} */\n')
    return ''.join(out)


def files(where=HERE):
    rel = os.path.relpath(where, ROOT)
    return {
        os.path.join(where, 'compare_order_verify.h'):
            driver(f'{rel}/compare_order_verify.h', ORDER_HEAD,
                   'CANON_VDRIVER_COMPARE_ORDER_VERIFY_H', order_wrappers),
        os.path.join(where, 'compare_order_false.h'):
            driver(f'{rel}/compare_order_false.h', FALSE_HEAD,
                   'CANON_VDRIVER_COMPARE_ORDER_FALSE_H', false_wrappers),
    }


def main(argv):
    check = argv[1:] == ['--check']
    if argv[1:] and not check:
        print(__doc__)
        return 2
    bad = 0
    for path, text in files().items():
        if check:
            try:
                with open(path, encoding='utf-8') as f:
                    same = f.read() == text
            except OSError:
                same = False
            if not same:
                print(f'{os.path.relpath(path, ROOT)}: not as gen_order.py writes it')
                bad += 1
        else:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(text)
            print(f'wrote {os.path.relpath(path, ROOT)}')
    if check and not bad:
        print('gen_order.py --check: both files as it writes them')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
