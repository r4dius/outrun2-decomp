#!/usr/bin/env python3
"""Translation set for a group of roots: the roots plus every direct callee
the port does not implement (functions.csv status absent / unreached /
mentioned / refused), transitively. Ported callees (implemented / handled /
constant / platform) stay external: the translated code calls them through
the module's service, i.e. the port's own implementation.

usage: closure.py steam_raw.asm functions.csv 0xROOT... > list.txt
Prints the addresses (one per line) and, on stderr, the external callees."""
import csv, re, sys
listing, table = sys.argv[1], sys.argv[2]
roots = [int(x, 16) for x in sys.argv[3:]]
status = {}
for r in csv.DictReader(open(table, encoding='utf-8')):
    try: status[int(r['address'], 16)] = r['status']
    except (ValueError, KeyError): pass
starts = sorted(status)
import bisect
calls = {}
rx = re.compile(r'^\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} )*\s*(call|jmp)\s+0x([0-9a-f]+)\s*$')
for line in open(listing, encoding='utf-8', errors='replace'):
    m = rx.match(line)
    if not m: continue
    a, v = int(m.group(1), 16), int(m.group(3), 16)
    if v not in status: continue
    k = bisect.bisect_right(starts, a) - 1
    if k < 0: continue
    f = starts[k]
    if f != v: calls.setdefault(f, set()).add(v)
PORTED = ('implemented', 'handled', 'constant', 'platform')
CRT_LO = 0x580000
seen, todo, external = set(), list(roots), set()
while todo:
    f = todo.pop()
    if f in seen: continue
    seen.add(f)
    for v in calls.get(f, ()):
        if v in seen: continue
        if status.get(v) in PORTED or v >= CRT_LO and v not in (0x582194,): external.add(v)
        else: todo.append(v)
for f in sorted(seen): print('0x%x' % f)
sys.stderr.write('externals: ' + ' '.join('%x' % v for v in sorted(external)) + '\n')
