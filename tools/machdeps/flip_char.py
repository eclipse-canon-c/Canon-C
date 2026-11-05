#!/usr/bin/env python3
"""tools/machdeps/flip_char.py IN.yaml OUT.yaml signed|unsigned

VERIFY-060, phase B's control. Writes a copy of the machine model IN with
only the signedness of char changed: the field char_is_unsigned, and the
compiler macro __CHAR_UNSIGNED__ among custom_defs, added for unsigned and
removed for signed. The copy is made line by line, not by a YAML library,
so that `diff IN OUT` shows the change and nothing else. Exits non-zero if
IN does not have the one field and the one custom_defs section it expects.
"""
import sys


def main():
    if len(sys.argv) != 4 or sys.argv[3] not in ('signed', 'unsigned'):
        sys.exit(__doc__)
    src, dst, want = sys.argv[1:4]
    with open(src, encoding='utf-8') as fh:
        lines = fh.read().split('\n')
    out = []
    fields = sections = 0
    for line in lines:
        if line.startswith('char_is_unsigned:'):
            out.append('char_is_unsigned: ' + ('true' if want == 'unsigned' else 'false'))
            fields += 1
            continue
        if line.strip().startswith('__CHAR_UNSIGNED__:'):
            continue
        out.append(line)
        if line == 'custom_defs:':
            sections += 1
            if want == 'unsigned':
                out.append("    __CHAR_UNSIGNED__: '1'")
    if fields != 1 or sections != 1:
        sys.exit(f'flip_char.py: {src}: {fields} char_is_unsigned field(s), '
                 f'{sections} custom_defs section(s); expected one of each')
    with open(dst, 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(out))


if __name__ == '__main__':
    main()
