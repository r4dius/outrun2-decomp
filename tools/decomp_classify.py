#!/usr/bin/env python3
"""Sort the absent game functions of the tracker (decomp_status.py) by what
they are, so the remaining work can be ordered:

  network         network code: Demonware glue (strings bd*, demonware, lobby, STUN,
                  Xbox Live, Net_*), calls into the Demonware regions, the network
                  session globals (7F9460.., 7D68AC.., 800A80..), and every absent
                  function reached only from such code
  called-by-port  called directly by ported code: either inlined by the port
                  without citing the address (false positive of the audit) or a
                  real missing call; reviewed by hand (work/decomp/call-audit.md)
  offline         the rest: offline game code still to port

usage: decomp_classify.py [--listing steam.asm] [--out-dir work/decomp]
"""
import argparse, bisect, collections, os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import decomp_status as ds

KEYWORDS = re.compile(r'\bbd[A-Z]|demonware|lobby|stun\.|xboxlive|net_|matchmak|\bdw/|packet|session|consolidate|node', re.I)
NET_GLOBALS = [(0x7f9460, 0x7f9600), (0x7d68a0, 0x7d6900), (0x800a80, 0x800b00)]

def strings_of(exe):
    secs = ds.pe_sections(exe)
    def rd(va, n):
        for name, base, vs, ro, rs in secs:
            if base <= va < base + rs: return exe[ro + va - base:ro + va - base + n]
        return None
    return rd

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--listing', default=os.path.join(ds.ROOT, 'work', 'decomp', 'steam_text.asm'))
    ap.add_argument('--out-dir', default=os.path.join(ds.ROOT, 'work', 'decomp'))
    a = ap.parse_args()
    ins = ds.listing(a.listing)
    exe = open(ds.EXE, 'rb').read()
    steam = open(ds.STEAM, 'rb').read()
    rd = strings_of(steam)
    starts = ds.function_starts(ins, exe)
    size = ds.sizes(starts, ins)
    named, handled, constant, refused, mentioned, tested = ds.scan_port()
    platform = ds.import_callers(starts, ins)
    status = {s: ds.classify(s, named, handled, constant, refused, mentioned, platform) for s in starts}
    owner = lambda addr: starts[bisect.bisect_right(starts, addr) - 1]
    calls = collections.defaultdict(set); callers = collections.defaultdict(set)
    net = set()        # strong: Demonware calls / strings, network manager methods
    weak = set()       # reads a network global (often an offline "is a session active?" test)
    ecx_mgr = None
    for addr, t in ins:
        if addr < starts[0]: continue
        f = owner(addr)
        if addr >= f + size[f]: continue
        if re.match(r'mov\s+ecx,0x(7f9460|7d68ac)$', t): ecx_mgr = (f, addr)
        m = re.match(r'(?:call|jmp)\s+0x([0-9a-f]+)$', t)
        if m:
            v = int(m.group(1), 16)
            if v in status and v != f:
                calls[f].add(v); callers[v].add(f)
                if ds.region(v).startswith('Demonware'): net.add(f)
                if ecx_mgr and ecx_mgr[0] == f and addr - ecx_mgr[1] < 16: net.add(v)   # a network manager method
            ecx_mgr = None
        for h in re.findall(r'0x([0-9a-f]{6})\b', t):
            v = int(h, 16)
            if any(lo <= v < hi for lo, hi in NET_GLOBALS): weak.add(f)
            elif 0x597000 <= v < 0x700000:
                b = rd(v, 64)
                if b:
                    s = b.split(b'\0')[0]
                    if len(s) >= 4 and all(32 <= c < 127 for c in s) and KEYWORDS.search(s.decode()): net.add(f)
    absent = [s for s in starts if status[s] == 'absent' and ds.region(s) == 'game']
    # Reachability with vtables: an immediate that points at a table of code
    # addresses (a vtable stored by a constructor, a callback table) makes every
    # entry of the table reachable from the function using it.
    sset = set(starts)
    def table(v):
        out = []
        for k in range(64):
            b = rd(v + 4 * k, 4)
            if not b or len(b) < 4: break
            w = struct.unpack('<I', b)[0]
            if w not in sset: break
            out.append(w)
        return out
    edges = collections.defaultdict(set)
    for addr, t in ins:
        if addr < starts[0]: continue
        f = owner(addr)
        if addr >= f + size[f]: continue
        for h in re.findall(r'0x([0-9a-f]{6})\b', t):
            v = int(h, 16)
            if v in sset and v != f: edges[f].add(v)
            elif 0x597000 <= v < 0x700000: edges[f].update(table(v))
    pe = struct.unpack_from('<I', exe, 0x3c)[0]
    entry = struct.unpack_from('<I', exe, pe + 24 + 16)[0] + struct.unpack_from('<I', exe, pe + 24 + 28)[0]
    roots = [r for r in ds.metadata_roots() | {owner(entry)} if r in sset]
    def reach(cut):
        seen = set(); todo = list(roots)
        while todo:
            f = todo.pop()
            if f in seen or (cut and f in net and status[f] not in ('implemented', 'handled', 'constant')): continue
            seen.add(f); todo.extend(edges[f] - seen)
        return seen
    offline, everything = reach(True), reach(False)
    if os.environ.get('CLS_DEBUG'):
        pr=[x for x in starts if status[x] in ('implemented','handled','constant') and ds.region(x)=='game']
        print('edges',len(edges),sum(len(v) for v in edges.values()),'root edges',sum(len(edges[r]) for r in roots));print('roots',len(roots),'offline',len(offline),'all',len(everything),'ported',len(pr),'ported reached',sum(1 for x in pr if x in everything),'ported offline',sum(1 for x in pr if x in offline),'ported net-seeded',sum(1 for x in pr if x in net))
        print('unreached ported sample',[hex(x) for x in pr if x not in everything][:20])
    ported = set(s for s in starts if status[s] in ('implemented', 'handled', 'constant'))
    kind = {}
    for s in absent:
        if s in offline: kind[s] = 'called-by-port' if callers[s] & ported else 'offline'
        elif s in everything or s in net or s in weak: kind[s] = 'network'
        else: kind[s] = 'unreached'
    tot = collections.Counter(); byt = collections.Counter()
    for s in absent: tot[kind[s]] += 1; byt[kind[s]] += size[s]
    blocks = collections.defaultdict(lambda: collections.Counter())
    for s in absent: blocks[s >> 12][kind[s]] += size[s]
    out = ['# Game functions missing from the port, by kind\n',
           'Generated by `tools/decomp_classify.py` (the same scan as `progress.md`).\n',
           '| Kind | Functions | Bytes |', '|---|---|---|']
    for k in ('offline', 'called-by-port', 'network', 'unreached'):
        out.append('| %s | %d | %d |' % (k, tot[k], byt[k]))
    out.append('| total | %d | %d |\n' % (len(absent), sum(size[s] for s in absent)))
    out += ['## Per 4 KiB block (bytes)\n', '| Block | offline | called-by-port | network | unreached |', '|---|---|---|---|---|']
    for b in sorted(blocks):
        c = blocks[b]
        out.append('| %06x | %d | %d | %d | %d |' % (b << 12, c['offline'], c['called-by-port'], c['network'], c['unreached']))
    out += ['\n## List\n', '| Function | Bytes | Kind | Called by |', '|---|---|---|---|']
    for s in absent:
        cs = sorted(callers[s])
        out.append('| %06x | %d | %s | %s |' % (s, size[s], kind[s], ' '.join('%06x' % c for c in cs[:6]) + (' …' if len(cs) > 6 else '')))
    os.makedirs(a.out_dir, exist_ok=True)
    with open(os.path.join(a.out_dir, 'missing.md'), 'w', encoding='utf-8') as fh:
        fh.write('\n'.join(out) + '\n')
    print('\n'.join(out[:9]))

if __name__ == '__main__':
    main()
