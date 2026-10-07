#!/usr/bin/env python3
"""Final check of the port (the "empty port" problem, non-working functions).

Static part (always):
  1. tools/decomp_empty_services.py   silent service skips, {} services
  2. tools/decomp_service_gaps.py     service calls no dispatcher answers
  3. refusals                         `throw ...("... not ported")`, EndUnported / missing(...)
                                      in service switches (a run that reaches them stops)
  4. silent constants                 `case 0xADDRu:return <const>;` answers with no comment:
                                      a PC function answered by a constant must say why
Run-time part (--logs FILE...): host run logs scanned for "[service] hole", "[bulk] first
call" (translated fallback reached), "not ported", latches and unhandled callbacks.

usage: decomp_final_check.py [--out work/decomp/final-check.md] [--logs log1 log2 ...]
"""
import argparse, collections, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src')
SKIP = re.compile(r'(_tr\.cpp|bulk_tr\.cpp|/tests?/)')


def sources():
    for d, _, fs in os.walk(SRC):
        for f in fs:
            if f.endswith(('.cpp', '.hpp', '.inc')):
                p = os.path.join(d, f)
                rel = os.path.relpath(p, ROOT).replace('\\', '/')
                if not SKIP.search(rel):
                    yield rel, open(p, encoding='utf-8', errors='replace').read().splitlines()


R_REFUSE = re.compile(r'(throw\s+\w*(Unported|Undefined)\w*\s*[({]|missing\s*\(\s*(pc|0x[0-9a-f]+u?)\s*,|throw\s+std::\w+\s*\(\s*"[^"]*not ported)', re.I)
R_UNREACH = re.compile(r'throw\s+\w*Unreachable\w*\s*[({]|never reach|not reached by|unreachable on the PC|no PC code reaches', re.I)
R_CONST = re.compile(r'case\s+0x([0-9a-f]{6})u\s*:\s*(?:\{\s*)?return\s+(0u|0|1u|1|true|false|~0u|0xffffffffu)\s*;\s*(?:\}\s*)?$', re.I)


def run(cmd):
    r = subprocess.run([sys.executable] + cmd, cwd=ROOT, capture_output=True, text=True)
    return (r.stdout.strip().splitlines() or [''])[-1]


def main():
    a = argparse.ArgumentParser()
    a.add_argument('--out', default=os.path.join(ROOT, 'work', 'decomp', 'final-check.md'))
    a.add_argument('--logs', nargs='*', default=[])
    o = a.parse_args()
    lines = []
    lines.append('# Final port check\n')
    lines.append('## Static\n')
    lines.append('- empty services: ' + run(['tools/decomp_empty_services.py']))
    lines.append('- service gaps: ' + run(['tools/decomp_service_gaps.py']))
    refusals, consts, unreach = [], [], []
    for rel, text in sources():
        for i, l in enumerate(text, 1):
            if R_REFUSE.search(l):
                refusals.append((rel, i, l.strip()[:160]))
            if R_UNREACH.search(l):
                unreach.append((rel, i, l.strip()[:160]))
            m = R_CONST.search(l)
            if m and '//' not in l:
                consts.append((rel, i, l.strip()[:120]))
    unported = [r for r in refusals if re.search(r'not ported|neither native|no LAN session|not reproduced|not bound', r[2], re.I)]
    lines.append('- refusals (a run that reaches them stops): %d, of them %d for unported code (the others guard data bounds)' % (len(refusals), len(unported)))
    lines.append('- paths claimed unreachable (a run that takes one stops; verify by playing every mode to its end): %d' % len(unreach))
    lines.append('- constant answers without a comment: %d\n' % len(consts))
    if o.logs:
        lines.append('## Run time\n')
        holes, bulk, notp, latch, unh = collections.Counter(), collections.Counter(), collections.Counter(), collections.Counter(), collections.Counter()
        stalls, ends, taken = collections.Counter(), {}, collections.Counter()
        trc = collections.Counter()   # [trcount] (OR2_TR_COUNT): translated functions run, by run log
        benign = re.compile(r'^(454670|45acb0|59[45][0-9a-f]{3})$')   # CRT helpers and the two known protected-gate stubs
        for f in o.logs:
            for l in open(f, encoding='utf-8', errors='replace'):
                if '[trcount]' in l:
                    for pc, n in re.findall(r'\b([0-9a-f]{6})=(\d+)', l.split('[trcount]', 1)[1]):
                        if not benign.match(pc): trc['%s (%s)' % (pc, os.path.basename(f))] += int(n)
                    continue
                m = re.search(r'\[flow\] mode \d+ -> (\d+)', l)
                if m:
                    ends[os.path.basename(f)] = m.group(1)
                    continue
                if '[stall]' in l:
                    stalls['%s: %s' % (os.path.basename(f), l.strip()[:150])] += 1
                    continue
                if 'no PC code reaches' in l or 'Unreachable' in l: taken[l.strip()[:160]] += 1
                # display faults the HUD / race end keep going after (GAD_PUB soft faults, 4998C0 notes)
                if 'missing (first frame):' in l and l.split('missing (first frame):', 1)[1].strip():
                    notp['%s: HUD draws skipped:%s' % (os.path.basename(f), l.split('missing (first frame):', 1)[1].rstrip()[:120])] += 1
                    continue
                if ' FAULT ' in l: latch[l.strip()[:160]] += 1; continue
                if '[service] hole' in l: holes[l.strip()[:160]] += 1
                elif '[bulk] first call' in l: bulk[l.strip()[:120]] += 1
                elif 'not ported' in l and 'event406' not in l: notp[l.strip()[:160]] += 1
                elif 'latched' in l and 'latched=0' not in l: latch[l.strip()[:160]] += 1
                elif l.startswith('unhandled') and l.strip().endswith(':') is False: unh[l.strip()[:160]] += 1
        if ends:
            lines.append('### last game mode of each run\n')
            for k in sorted(ends): lines.append('- %s: mode %s' % (k, ends[k]))
            lines.append('')
        for title, c in (('stalls ([stall]: unported screen / no progress)', stalls), ('claimed-unreachable paths taken', taken), ('service holes', holes), ('translated fallback reached', bulk), ('translated functions run ([trcount])', trc), ('not ported', notp),
                         ('latches', latch), ('unhandled callbacks', unh)):
            lines.append('### %s (%d distinct)\n' % (title, len(c)))
            for k, n in c.most_common(60):
                lines.append('- %s (x%d)' % (k, n))
            lines.append('')
    lines.append('## Refusals for unported code\n')
    for r in unported:
        lines.append('- `%s:%d` %s' % r)
    lines.append('\n## Paths claimed unreachable\n')
    for r in unreach:
        lines.append('- `%s:%d` %s' % r)
    lines.append('\n## All refusals by file\n')
    by = collections.Counter(r[0] for r in refusals)
    for f, n in by.most_common():
        lines.append('- `%s`: %d' % (f, n))
    lines.append('\n<details><summary>all refusals</summary>\n')
    for r in refusals:
        lines.append('- `%s:%d` %s' % r)
    lines.append('\n</details>\n\n## Constant answers without a comment\n')
    for r in consts:
        lines.append('- `%s:%d` %s' % r)
    os.makedirs(os.path.dirname(o.out), exist_ok=True)
    open(o.out, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    for l in lines[:8]:
        if l.startswith('-'): print(l)
    print('report:', o.out)


if __name__ == '__main__':
    main()
