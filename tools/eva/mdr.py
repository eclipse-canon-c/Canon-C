#!/usr/bin/env python3
"""
mdr.py -- MdR's SARIF of 14 · Eva's runs, for GitHub's code scanning
(VERIFY-082).

MdR, Frama-C's Markdown Report plug-in, writes the property statuses of an
analysis as SARIF. tools/eva/eva.py has it write one file for each program
at each machine model, from the run its pins compare
(eva_out/MODEL/PROGRAM.sarif, with -mdr-no-print-libc: none of Frama-C's
libc's own properties). An alarm is a result under the alarm's name as
rule, its status as the kind (open for Unknown, fail for Invalid). Any
other property is a result under the rule user-spec of the kind pass,
whatever its status, and of the level none unless it is invalid: Frama-C
29's MdR does not give it its kind (VERIFY-082's run 1: of 293540
results, the 30 alarms open and the others pass, none open at any of the
pins' 744 preconditions and assertions).

merge uploads, for code scanning, the properties Eva's pins hold open
(tools/eva/README.md, "Pins", says why each holds):

- an alarm, as MdR's open result at its file and line under its name: MdR
  and Report's table must agree on it;
- an open assertion, or a call whose precondition Eva leaves open, as
  Report's table, from which the pins are made, gives it: its file, line
  and status. MdR's result for it would say pass.

One property at several models or programs is one result, which names
them. Each result is at level warning for Unknown, error for Invalid, and
of SARIF's default kind, fail: MdR gives an Unknown status the level none,
and code scanning does not show a result of level none.

Each result carries its own fingerprint, from its rule, its file, the text
of its line and the property. Code scanning tells alerts apart by the
fingerprint. The upload computes one from the line alone, which would make
two properties on one line one alert; finding ours, it keeps ours, with a
warning for each (VERIFY-082's run 1: the two alarms of ptr.c's line 38
two alerts). It does not change when the line moves, nor when the models
or the programs do.

MdR's other open results are counted, not uploaded.

    python3 tools/eva/mdr.py merge [DIR] [OUT]
        DIR (default eva_out): eva.py run's output, a directory per model.
        OUT (default eva.sarif): one SARIF run for code scanning, written
        only when every program has its SARIF at every model. Prints each
        result; the notices go to notices_mdr.txt.

A program with no SARIF at a model, or a pinned alarm with no open result
in MdR's SARIF, is a problem: reported, and failing the job once ENFORCE
is set (VERIFY-082's runs 1 and 2 report).
"""
import collections
import glob
import hashlib
import json
import os
import sys
from urllib.parse import unquote

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True
import eva  # noqa: E402  (the programs, the models and the summaries)

ENFORCE = False   # VERIFY-082 runs 1 and 2: report-only

# SARIF's kinds for a status Eva leaves open: Unknown, Invalid, Inconsistent.
OPEN_KINDS = ('open', 'fail', 'review')
NOTICES = 'notices_mdr.txt'
CUT = 4000        # the runner cuts an annotation at 4096 characters


def load(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def where(art):
    """The repository file an artifactLocation names, or None. Frama-C
    writes a file under its current directory, the repository's root, as
    relative to PWD, and its own files (libc) as relative to FRAMAC_SHARE."""
    uri = unquote(art.get('uri') or '')
    base = art.get('uriBaseId')
    if base and base != 'PWD':
        return None
    if uri.startswith('file://'):
        uri = uri[len('file://'):]
    if os.path.isabs(uri):
        uri = os.path.relpath(uri, ROOT)
    path = os.path.normpath(uri)
    if path.startswith('..') or not os.path.isfile(os.path.join(ROOT, path)):
        return None
    return path


def region(reg):
    """The region's lines and columns, those SARIF allows (1 and up), an
    end before its start left out."""
    out = {k: reg[k] for k in ('startLine', 'startColumn', 'endLine', 'endColumn')
           if isinstance(reg.get(k), int) and reg[k] >= 1}
    if out.get('endLine', out.get('startLine', 0)) < out.get('startLine', 0):
        out.pop('endLine')
        out.pop('endColumn', None)
    if ('endColumn' in out and out.get('endLine', out.get('startLine')) == out.get('startLine')
            and out['endColumn'] <= out.get('startColumn', 1)):
        out.pop('endColumn')
    return out


def results_of(doc):
    """MdR's results, its rules' descriptions and its tool's driver."""
    results, rules, driver = [], {}, {}
    for run in doc.get('runs') or []:
        tool = (run.get('tool') or {}).get('driver') or {}
        driver = driver or tool
        for comp in [tool] + list(run.get('taxonomies') or []) + [run.get('resources') or {}]:
            for r in list(comp.get('rules') or []) + list(comp.get('taxa') or []):
                if isinstance(r, dict) and r.get('id'):
                    rules.setdefault(r['id'], ((r.get('shortDescription') or {}).get('text') or '').strip())
        results.extend(run.get('results') or [])
    return results, rules, driver


def place(res):
    """A result's file, line and region, or None outside the repository."""
    locs = res.get('locations') or []
    if not locs:
        return None
    phys = locs[0].get('physicalLocation') or {}
    path = where(phys.get('artifactLocation') or {})
    reg = phys.get('region') or {}
    line = reg.get('startLine')
    if path is None or not isinstance(line, int) or line < 1:
        return None
    return path, line, region(reg)


def rule_of(item):
    """The rule of MdR's result for a property a pin holds: the alarm's name
    for an alarm, user-spec for a user annotation or a precondition."""
    if item['class'] == 'alarm' and not item['kind'].startswith('user '):
        return item['kind']
    return 'user-spec'


def source_line(path, line, cache={}):
    """The text of a file's line, its spaces collapsed."""
    if path not in cache:
        with open(os.path.join(ROOT, path), encoding='utf-8', errors='replace') as f:
            cache[path] = f.read().split('\n')
    lines = cache[path]
    return ' '.join(lines[line - 1].split()) if 0 < line <= len(lines) else ''


def fingerprint(key, seen):
    """primaryLocationLineHash: a hash of the rule, the file, the text of
    the line and the property, and how many results before this one had it."""
    rule, path, line, status, kind, prop = key
    h = hashlib.sha256('\0'.join((rule, path, source_line(path, line), status, kind, prop))
                       .encode('utf-8')).hexdigest()[:16]
    seen[h] += 1
    return f'{h}:{seen[h]}'


def models_text(models):
    ms = [m for m in eva.MODELS if m in models]
    if len(ms) == len(eva.MODELS):
        return f'at all {len(ms)} machine models'
    return 'at ' + (ms[0] if len(ms) == 1 else ', '.join(ms[:-1]) + ' and ' + ms[-1])


def merge(where_dir, out):
    summaries = {}
    for p in sorted(glob.glob(os.path.join(where_dir, '**', '*.json'), recursive=True)):
        try:
            s = load(p)
        except (OSError, ValueError):
            continue
        if isinstance(s, dict) and 'program' in s and 'model' in s:
            summaries[(s['model'], s['program'])] = (s, p[:-len('.json')] + '.sarif')
    problems, files = [], 0
    kinds = collections.Counter()           # MdR's results, by kind
    levels = collections.Counter()          # and by level
    others = collections.Counter()          # its open results no pin holds, by model
    others_kind = collections.Counter()
    outside = 0
    alarms = alarms_matched = specs = specs_placed = 0
    rules_all, driver = {}, {}
    merged = {}
    for model in eva.MODELS:
        for program in eva.PROGRAMS:
            if (model, program) not in summaries:
                problems.append(f'{program} @ {model}: no summary')
                continue
            s, sarif = summaries[(model, program)]
            try:
                doc = load(sarif)
            except FileNotFoundError:
                problems.append(f'{program} @ {model}: no SARIF')
                continue
            except (OSError, ValueError) as e:
                problems.append(f'{program} @ {model}: SARIF not read: {e}')
                continue
            files += 1
            results, rules, drv = results_of(doc)
            for k, v in rules.items():
                rules_all.setdefault(k, v)
            driver = driver or drv
            pool = collections.defaultdict(list)    # MdR's open results, by place
            spec_places = set()                      # where MdR has a user-spec result
            for res in results:
                kind = res.get('kind') or 'fail'
                kinds[kind] += 1
                levels[res.get('level') or 'warning'] += 1
                pl = place(res)
                if pl is not None and res.get('ruleId') == 'user-spec':
                    spec_places.add(pl[:2])
                if kind not in OPEN_KINDS:
                    continue
                if pl is None:
                    outside += 1
                    continue
                pool[pl[:2]].append({'res': res, 'region': pl[2], 'used': False})
            for item in s.get('alarm_list', []) + s.get('program_calls', []) + s.get('library_calls', []):
                rule = rule_of(item)
                at = (item['file'], item['line'])
                if rule == 'user-spec':
                    # Report's table: MdR's SARIF gives an ACSL property the kind pass.
                    specs += 1
                    specs_placed += at in spec_places
                    region, source, mdr_text = {'startLine': item['line']}, "Report's table", ''
                else:
                    alarms += 1
                    cands = pool.get(at, [])
                    pick = next((c for c in cands if not c['used'] and c['res'].get('ruleId') == rule), None)
                    if pick is None:
                        have = sorted({c['res'].get('ruleId') or '?' for c in cands})
                        problems.append(f"{program} @ {model}: no open result of rule {rule} at "
                                        f"{item['file']}:{item['line']} for {item['kind']}: "
                                        f"{item['property'][:120]}"
                                        + (f" (MdR has {', '.join(have)} there)" if have else ''))
                        continue
                    pick['used'] = True
                    alarms_matched += 1
                    region, source = pick['region'], "MdR's SARIF, as Report's table"
                    mdr_text = (pick['res'].get('message') or {}).get('text') or ''
                key = (rule, item['file'], item['line'], item['status'], item['kind'], item['property'])
                e = merged.setdefault(key, {
                    'region': region, 'function': item['function'], 'models': set(),
                    'programs': set(), 'count': collections.Counter(), 'source': source,
                    'mdr': mdr_text})
                e['models'].add(model)
                e['programs'].add(program)
                e['count'][(model, program)] += 1
            for cands in pool.values():
                for c in cands:
                    if not c['used']:
                        others[model] += 1
                        others_kind[c['res'].get('kind') or 'fail'] += 1

    results, seen = [], collections.Counter()
    for key in sorted(merged, key=lambda k: (k[1], k[2], k[0], k[4], k[5])):
        rule, path, line, status, kind, prop = key
        e = merged[key]
        n = max(e['count'].values())
        programs = sorted(e['programs'])
        text = (f"Eva: {status}: {kind}: {prop[:300]}" + (f' ({n} on this line)' if n > 1 else '')
                + f". Function {e['function']}; program{'s' if len(programs) > 1 else ''} "
                + ', '.join(f'{p}.c' for p in programs) + '; ' + models_text(e['models']) + '.')
        results.append({
            'ruleId': rule,
            'level': 'error' if status.startswith('Invalid') else 'warning',
            'message': {'text': text},
            'locations': [{'physicalLocation': {'artifactLocation': {'uri': path},
                                                'region': e['region'] or {'startLine': line}}}],
            'partialFingerprints': {'primaryLocationLineHash': fingerprint(key, seen)},
            'properties': {'status': status, 'function': e['function'],
                           'models': [m for m in eva.MODELS if m in e['models']],
                           'programs': programs, 'source': e['source'], 'mdrMessage': e['mdr']},
        })
    rule_counts = collections.Counter(r['ruleId'] for r in results)
    rules = [{'id': r, 'shortDescription': {'text': (rules_all.get(r) or r).rstrip('.') + '.'},
              'defaultConfiguration': {'level': 'warning'}} for r in sorted(rule_counts)]
    drv = {'name': driver.get('name') or 'frama-c',
           'informationUri': driver.get('informationUri') or 'https://frama-c.com', 'rules': rules}
    if driver.get('version') and 'omitted' not in driver['version']:
        drv['version'] = driver['version']
    doc = {'$schema': 'https://json.schemastore.org/sarif-2.1.0.json', 'version': '2.1.0',
           'runs': [{'tool': {'driver': drv}, 'results': results,
                     'properties': {'from': f"14 · Eva's runs, {len(eva.PROGRAMS)} programs at "
                                            f"{len(eva.MODELS)} machine models: the alarms from MdR's SARIF, "
                                            f"the rest from Report's table (tools/eva/mdr.py, VERIFY-082)"}}]}
    expected = len(eva.MODELS) * len(eva.PROGRAMS)
    complete = files == expected
    if complete:
        with open(out, 'w', encoding='utf-8') as f:
            json.dump(doc, f, indent=1, ensure_ascii=False)
            f.write('\n')
    elif os.path.exists(out):
        os.remove(out)

    print(f'{"level":<8} {"rule":<18} {"place":<52} {"models":>6}  text')
    for r in results:
        loc = r['locations'][0]['physicalLocation']
        at = f"{loc['artifactLocation']['uri']}:{loc['region'].get('startLine')}"
        print(f"{r['level']:<8} {r['ruleId']:<18} {at:<52} {len(r['properties']['models']):>6}  "
              f"{r['message']['text'][:200]}")
    for p in problems:
        print('problem: ' + p)
    warnings = sum(1 for r in results if r['level'] == 'warning')
    lines = [
        ('mdr', f"mdr: MdR's SARIF for {files} of {expected} runs ({len(eva.PROGRAMS)} programs at "
                f"{len(eva.MODELS)} machine models): {sum(kinds.values())} results, "
                + ', '.join(f'{k} {kinds[k]}' for k in sorted(kinds)) + '; levels '
                + ', '.join(f'{k} {levels[k]}' for k in sorted(levels))),
        ('mdr pins', f"mdr pins: the alarms, {alarms_matched} of {alarms}, have MdR's open result at "
                     f"their file and line, under their name; the assertions and preconditions, "
                     f"{specs}, are as Report's table holds them, MdR's SARIF giving an ACSL "
                     f"property the kind pass (MdR has a user-spec result at the place of "
                     f"{specs_placed})"),
        ('mdr upload', f"mdr upload: {len(results)} results for code scanning, {warnings} warning(s), "
                       f"{len(results) - warnings} error(s); rules "
                       + ', '.join(f'{r} {n}' for r, n in sorted(rule_counts.items()))
                       + ('' if complete else f'; {out} not written: a run has no SARIF')),
        ('mdr others', f"mdr others: MdR's open results no pin holds, not uploaded: {sum(others.values())} ("
                       + ', '.join(f'{k} {v}' for k, v in sorted(others_kind.items())) + '); '
                       + '; '.join(f'{m} {others[m]}' for m in eva.MODELS)
                       + (f'; {outside} open results outside the repository' if outside else '')),
    ]
    if problems:
        lines.append(('mdr problems', f'mdr problems: {len(problems)}'
                      + ('' if ENFORCE else ', report-only') + ': ' + '; '.join(problems)))
    with open(NOTICES, 'w', encoding='utf-8') as f:
        for title, msg in lines:
            f.write(f'::notice title={title}::{msg[:CUT]}\n')
    for _, msg in lines:
        print(msg)
    return 1 if (ENFORCE and problems) else 0


def main(argv):
    if len(argv) >= 2 and argv[1] == 'merge' and len(argv) <= 4:
        return merge(argv[2] if len(argv) > 2 else 'eva_out', argv[3] if len(argv) > 3 else 'eva.sarif')
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
