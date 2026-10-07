#!/usr/bin/env python3
"""Per-function port status of OR2006C2C.EXE (decomp tracker).

Every function of the EXE's .text is listed with its size and classified by
what the port's source (src/, switch/source/) does with its address:

  implemented  a C++ function whose name ends with the address (foo_4d66d0)
  handled      a service/case entry for the address (case 0x4d66d0u:, pc==...)
  constant     a case entry that only counts and/or returns a constant: to review
               (some originals are one-line getters, some are silent stubs)
  refused      the port stops on it on purpose (EndUnported, fault, missing, throw)
  mentioned    only named in comments
  platform     not referenced, but calls Windows/DirectX imports itself: the
               native platform layer replaces it (window, D3D device, input,
               files, sound driver, movie)
  absent       not referenced anywhere (reachable from the entry point or the
               event/mode callbacks)
  unreached    not referenced and not reachable that way (dead code, or only
               reached through a computed call)

"verified" means the address also appears in tests/ (golden data measured on
the original, or a difftest of the translated original).

Function starts: direct call targets, plus code pointers stored in .rdata /
.data (event and mode tables, vtables, callbacks). Code is read from the
unpacked Steam build listing (same build, plain code for the protected
functions); data from OR2006C2C.EXE. Library regions are reported apart.

usage: decomp_status.py [--listing steam.asm] [--out-dir work/decomp]
"""
import argparse, collections, csv, os, re, struct, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Base: the unpacked Steam build (same build and addresses as OR2006C2C.EXE; .data
# identical, .text identical except the protection gateway sites, .rdata except
# the IAT and debug directory). OR2_EXE overrides.
EXE = os.environ.get('OR2_EXE', os.environ.get('OR2_STEAM_EXE', r'C:/Users/r4dius/Documents/outrun2/OR2006C2C_unpacked_analysis.exe'))
STEAM = os.environ.get('OR2_STEAM_EXE', r'C:/Users/r4dius/Documents/outrun2/OR2006C2C_unpacked_analysis.exe')
# The FXT EXE (protected, sha256 bdafa88a...) the earlier oracle data was measured on.
FXT = os.environ.get('OR2_FXT_EXE', os.path.join(ROOT, '..', 'OR2006C2C.EXE'))
TEXT_LO, TEXT_HI = 0x401000, 0x595881

# Address regions (from the strings their code references, 2026-10-04).
REGIONS = [
    (0x401000, 0x521c00, 'game'),
    (0x521c00, 0x536000, 'Demonware network (bd*)'),
    (0x536000, 0x543000, 'DXERR (DirectX error strings)'),
    (0x543000, 0x55a000, 'Ogg Vorbis'),
    (0x55a000, 0x580000, 'Demonware network (bd*)'),
    (0x580000, 0x596000, 'CRT Microsoft (VS2005)'),
]
def region(a):
    for lo, hi, name in REGIONS:
        if lo <= a < hi: return name
    return '?'

def listing(path):
    if not os.path.exists(path):
        exe = STEAM
        cmd = ['objdump', '-d', '-Mintel', '--no-show-raw-insn',
               '--start-address=0x%x' % TEXT_LO, '--stop-address=0x%x' % TEXT_HI, exe]
        with open(path, 'w', encoding='utf-8') as f:
            subprocess.run(cmd, stdout=f, check=True)
    ins = []
    rx = re.compile(r'\s+([0-9a-f]+):\t(.*)$')
    for l in open(path, encoding='utf-8', errors='replace'):
        m = rx.match(l.rstrip('\n'))
        if m: ins.append((int(m.group(1), 16), m.group(2).strip()))
    return ins

def pe_sections(d):
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]; opt = struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    out = []
    for i in range(n):
        o = pe + 24 + opt + i * 40
        name = d[o:o + 8].rstrip(b'\0').decode('latin1')
        vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
        out.append((name, base + va, vs, ro, rs))
    return out

def function_starts(ins, exe_bytes):
    addrs = [a for a, _ in ins]
    text = dict(ins)
    starts = set()
    for a, t in ins:
        m = re.match(r'call\s+0x([0-9a-f]+)$', t)
        if m:
            v = int(m.group(1), 16)
            if TEXT_LO <= v < TEXT_HI and v in text: starts.add(v)
    # code pointers in .rdata/.data: an instruction start that follows padding or a ret
    prev = {}
    for i in range(1, len(ins)): prev[ins[i][0]] = ins[i - 1][1]
    # 16-aligned code right after int3 padding: a function reached only by jumps or pointers
    for i in range(1, len(ins)):
        a, t = ins[i]
        if a & 0xf == 0 and ins[i - 1][1].startswith('int3') and not t.startswith('int3'): starts.add(a)
    for name, va, vs, ro, rs in pe_sections(exe_bytes):
        if name not in ('.rdata', '.data'): continue
        blob = exe_bytes[ro:ro + rs]
        for off in range(0, len(blob) - 3, 4):
            v = struct.unpack_from('<I', blob, off)[0]
            if TEXT_LO <= v < TEXT_HI and v in text and v not in starts:
                p = prev.get(v, '')
                if p.startswith('int3') or p.startswith('ret') or p.startswith('jmp') or v & 0xf == 0:
                    starts.add(v)
    return sorted(starts)

def import_callers(starts, ins):
    """Functions that call an import (IAT 596000..5964FF) directly."""
    out = set(); i = 0
    import bisect
    for a, t in ins:
        if re.search(r'(call|jmp)\s+DWORD PTR ds:0x5960[0-9a-f]{2}\b|(call|jmp)\s+DWORD PTR ds:0x5961[0-9a-f]{2}\b|(call|jmp)\s+DWORD PTR ds:0x596[2-4][0-9a-f]{2}\b', t):
            k = bisect.bisect_right(starts, a) - 1
            if k >= 0: out.add(starts[k])
    return out

def sizes(starts, ins):
    """Bytes from the start to the next start, without the int3 padding."""
    pad = set(a for a, t in ins if t.startswith('int3'))
    out = {}
    for i, s in enumerate(starts):
        e = starts[i + 1] if i + 1 < len(starts) else TEXT_HI
        n = e
        while n - 1 > s and (n - 1) in pad: n -= 1
        out[s] = n - s
    return out

def scan_port():
    srcs = []
    for top in ('src', os.path.join('switch', 'source')):
        for dp, _, fs in os.walk(os.path.join(ROOT, top)):
            for f in fs:
                if f.endswith(('.cpp', '.hpp', '.inc', '.h')):
                    srcs.append(os.path.join(dp, f))
    named = collections.defaultdict(set)
    handled = collections.defaultdict(set)
    constant = collections.defaultdict(set)
    refused = collections.defaultdict(set)
    mentioned = collections.defaultdict(set)
    r_named = re.compile(r'\b[A-Za-z]\w*?_([0-9a-f]{6})\s*\(')   # a function, not a counter named after it
    r_case = re.compile(r'(?:case\s+|(?:pc|pc_entry|callback|token|entry)\s*==\s*)0x0*([0-9a-f]{6})u?\b')
    r_const = re.compile(r'case\s+0x0*([0-9a-f]{6})u?\s*:\s*(?:\{\s*)?(?:\+\+[\w.\[\]>-]+;\s*)*return\s+(?:0x)?[0-9a-f]+u?\s*;\s*(?:\}\s*)?(?://.*)?$')
    r_refuse = re.compile(r'EndUnported|fault\(|not ported|missing_?\s*=|throw\s|_missing_\w*\s*=|unported', re.I)
    r_hex = re.compile(r'(?<![0-9A-Fa-f])(?:0x0*)?([0-9A-Fa-f]{6})(?![0-9A-Fa-f])')
    for p in srcs:
        rel = os.path.relpath(p, ROOT).replace('\\', '/')
        for line in open(p, encoding='utf-8', errors='replace'):
            for m in r_named.finditer(line): named[int(m.group(1), 16)].add(rel)
            for m in r_case.finditer(line): handled[int(m.group(1), 16)].add(rel)
            m = r_const.search(line)
            if m: constant[int(m.group(1), 16)].add(rel)
            if r_refuse.search(line):
                for m in r_hex.finditer(line):
                    v = int(m.group(1), 16)
                    if TEXT_LO <= v < TEXT_HI: refused[v].add(rel)
            for m in r_hex.finditer(line):
                v = int(m.group(1), 16)
                if TEXT_LO <= v < TEXT_HI: mentioned[v].add(rel)
    # Early modules (driving, chassis, collision) name their functions without the
    # address; tools/decomp_aliases.csv maps them (address,name).
    defined = set()
    r_def = re.compile(r'\b([A-Za-z_]\w*)\s*\(')
    for p in srcs:
        for line in open(p, encoding='utf-8', errors='replace'):
            defined.update(r_def.findall(line))
    for line in open(os.path.join(ROOT, 'tools', 'decomp_aliases.csv'), encoding='utf-8').read().splitlines()[1:]:
        addr, name = line.split(',')
        if name in defined: named[int(addr, 16)].add('decomp_aliases.csv:' + name)
    tested = set()
    for dp, _, fs in os.walk(os.path.join(ROOT, 'tests')):
        for f in fs:
            if not f.endswith(('.cpp', '.hpp', '.py', '.inc', '.h', '.sh')): continue
            for line in open(os.path.join(dp, f), encoding='utf-8', errors='replace'):
                for m in r_hex.finditer(line):
                    v = int(m.group(1), 16)
                    if TEXT_LO <= v < TEXT_HI: tested.add(v)
    return named, handled, constant, refused, mentioned, tested

def metadata_roots():
    """Callbacks of the event function table (128 x 0x14 at 59BE78) and the mode
    table (37 x 0x10 at 5995B4), read in the EXE like src/platform/exe_packs.cpp."""
    d = open(EXE, 'rb').read()
    def va_bytes(va, size):
        for _, base, vs, ro, rs in pe_sections(d):
            if base <= va and va + size <= base + rs: return d[ro + va - base:ro + va - base + size]
        raise ValueError('address %x not in the file' % va)
    roots = set()
    for va, size in ((0x59BE78, 128 * 0x14), (0x5995B4, 37 * 0x10)):
        data = va_bytes(va, size)
        for o in range(0, size, 4):
            v = struct.unpack_from('<I', data, o)[0]
            if TEXT_LO <= v < TEXT_HI: roots.add(v)
    return roots

def reachable(starts, size, ins, exe_bytes):
    """Functions the original can reach from its entry point and the event/mode
    callbacks: direct calls/jumps and code addresses used as immediates."""
    import bisect
    pe = struct.unpack_from('<I', exe_bytes, 0x3c)[0]
    entry = struct.unpack_from('<I', exe_bytes, pe + 24 + 16)[0] + struct.unpack_from('<I', exe_bytes, pe + 24 + 28)[0]
    sset = set(starts)
    edges = collections.defaultdict(set)
    rx = re.compile(r'0x([0-9a-f]{6})\b')
    for a, t in ins:
        for m in rx.finditer(t):
            v = int(m.group(1), 16)
            if v in sset:
                k = bisect.bisect_right(starts, a) - 1
                if k >= 0 and a < starts[k] + max(size[starts[k]], 1): edges[starts[k]].add(v)
    # code addresses stored in .rdata/.data (vtables, callback tables) are roots too
    data_refs = set()
    for name, va, vs, ro, rs in pe_sections(exe_bytes):
        if name not in ('.rdata', '.data'): continue
        blob = exe_bytes[ro:ro + rs]
        for off in range(0, len(blob) - 3, 4):
            v = struct.unpack_from('<I', blob, off)[0]
            if v in sset: data_refs.add(v)
    seen = set(); todo = [r for r in metadata_roots() | data_refs | {entry} if r in sset]
    k = bisect.bisect_right(starts, entry) - 1
    if k >= 0: todo.append(starts[k])
    while todo:
        f = todo.pop()
        if f in seen: continue
        seen.add(f); todo.extend(edges[f] - seen)
    return seen

def classify(a, named, handled, constant, refused, mentioned, platform):
    if a in named: return 'implemented'
    if a in handled and a not in constant: return 'handled'
    if a in constant: return 'constant'
    if a in refused: return 'refused'
    if a in platform: return 'platform'
    if a in mentioned: return 'mentioned'
    return 'absent'

ORDER = ['implemented', 'handled', 'constant', 'refused', 'platform', 'mentioned', 'absent', 'unreached']
LABEL = {'implemented': 'ported (function)', 'handled': 'ported (service)', 'constant': 'constant / counter (to review)',
         'refused': 'refused (stops on purpose)', 'platform': 'Windows platform (replaced)', 'mentioned': 'mentioned only', 'absent': 'missing (reached by the game)',
         'unreached': 'missing, never reached (dead code or indirect call)'}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--listing', default=os.path.join(ROOT, 'work', 'decomp', 'steam_text.asm'))
    ap.add_argument('--out-dir', default=os.path.join(ROOT, 'work', 'decomp'))
    a = ap.parse_args()
    ins = listing(a.listing)
    exe_bytes = open(EXE, 'rb').read()
    starts = function_starts(ins, exe_bytes)
    size = sizes(starts, ins)
    named, handled, constant, refused, mentioned, tested = scan_port()
    platform = import_callers(starts, ins)
    reach = reachable(starts, size, ins, exe_bytes)
    os.makedirs(a.out_dir, exist_ok=True)
    # Verification base: a function whose bytes are the same in the Steam build
    # (the base) and in the FXT EXE keeps the checks measured on the FXT EXE;
    # the others (protection gateway sites in the FXT EXE) are re-checked on Steam.
    steam_b = open(STEAM, 'rb').read()
    fxt_b = open(FXT, 'rb').read() if os.path.exists(FXT) else None
    def va_reader(d):
        secs = pe_sections(d)
        def rd(a, n):
            for name, va, vs, ro, rs in secs:
                if va <= a < va + rs: return d[ro + a - va:ro + min(a - va + n, rs)]
            return b''
        return rd
    rs_, rf_ = va_reader(steam_b), (va_reader(fxt_b) if fxt_b else None)
    # Functions whose FXT gate sites were reviewed against the Steam code
    # (tools/decomp_steam_review.csv: address, reason).
    reviewed = set()
    rp = os.path.join(ROOT, 'tools', 'decomp_steam_review.csv')
    if os.path.exists(rp):
        for line in open(rp, encoding='utf-8').read().splitlines()[1:]:
            if line.strip(): reviewed.add(int(line.split(',')[0], 16))
    def base(s):
        if not rf_: return 'steam'
        if rs_(s, size[s]) == rf_(s, size[s]): return 'steam=fxt'
        return 'steam!=fxt reviewed' if s in reviewed else 'steam!=fxt'
    rows = []
    for s in starts:
        st = classify(s, named, handled, constant, refused, mentioned, platform)
        if st == 'absent' and s not in reach: st = 'unreached'
        # A native name or service wins over the translated *_tr.cpp copies of the function.
        native = lambda fs: sorted(f for f in (fs or []) if not f.endswith('_tr.cpp'))
        files = (native(named.get(s)) or native(handled.get(s)) or sorted(named.get(s) or handled.get(s)
                 or refused.get(s) or mentioned.get(s) or []))
        rows.append((s, size[s], region(s), st, s in tested, files[0] if files else '', base(s)))
    with open(os.path.join(a.out_dir, 'functions.csv'), 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['address', 'bytes', 'region', 'status', 'verified', 'file', 'base'])
        for s, n, r, st, t, fl, b in rows: w.writerow(['%06x' % s, n, r, st, int(t), fl, b])
    # totals per region and status
    tot = collections.defaultdict(lambda: collections.Counter())
    byt = collections.defaultdict(lambda: collections.Counter())
    ver = collections.Counter(); verb = collections.Counter()
    recheck = collections.Counter()
    for s, n, r, st, t, fl, b in rows:
        tot[r][st] += 1; byt[r][st] += n
        if t and st in ('implemented', 'handled', 'constant'):
            ver[r] += 1; verb[r] += n
            if b == 'steam!=fxt': recheck[r] += 1
    # game blocks of 64 KiB with their dominant port file
    blocks = collections.defaultdict(lambda: [collections.Counter(), collections.Counter(), collections.Counter()])
    for s, n, r, st, t, fl, b in rows:
        if r != 'game': continue
        b = blocks[s >> 16]
        b[0][st] += 1; b[1][st] += n
        if fl: b[2][fl.split('/')[-1]] += 1
    out = []
    out.append('# Port progress per function\n')
    out.append('Generated by `tools/decomp_status.py` (the numbers change with every port; run the script again).\n')
    out.append('A function is "ported" when the port has a function or a service for its address. '
               'It is "verified" when its address also appears in the tests (data measured on the original, or a difftest).\n')
    out.append('**Base: the decompressed Steam EXE** (OR2006C2C_unpacked_analysis.exe, sha256 7c4e8ec6...). It is the same build '
               'as the protected FXT EXE (sha256 bdafa88a...), at the same addresses: identical .data, identical .text except at the '
               'protection gates, identical .rdata except the IAT and the debug directory. The `base` column of functions.csv '
               'says for each function whether its bytes are identical in both EXEs (`steam=fxt`: the checks measured on the FXT '
               'hold for Steam) or not (`steam!=fxt`: to check again on Steam; `steam!=fxt reviewed`: gates read against the Steam '
               'code, list in tools/decomp_steam_review.csv).\n')
    for r in dict.fromkeys(x[2] for x in REGIONS):
        if r not in tot: continue
        n = sum(tot[r].values()); nb = sum(byt[r].values())
        done = tot[r]['implemented'] + tot[r]['handled']; doneb = byt[r]['implemented'] + byt[r]['handled']
        out.append('\n## %s: %d functions, %d bytes\n' % (r, n, nb))
        out.append('Ported: **%.1f %%** of the functions, **%.1f %%** of the bytes. Verified: %.1f %% of the functions (%.1f %% of the bytes), '
                   '%d of them to check again on Steam.\n'
                   % (100.0 * done / n, 100.0 * doneb / max(nb, 1), 100.0 * ver[r] / n, 100.0 * verb[r] / max(nb, 1), recheck[r]))
        out.append('| Status | Functions | Bytes |\n|---|---|---|')
        for st in ORDER:
            out.append('| %s | %d | %d |' % (LABEL[st], tot[r][st], byt[r][st]))
    out.append('\n## Game, per 64 KiB address block\n')
    out.append('| Block | Functions | Ported | Bytes ported | Missing | Most frequent port file |\n|---|---|---|---|---|---|')
    for k in sorted(blocks):
        c, b, f = blocks[k]
        n = sum(c.values()); nb = sum(b.values())
        done = c['implemented'] + c['handled']; doneb = b['implemented'] + b['handled']
        top = f.most_common(1)[0][0] if f else ''
        out.append('| %06x | %d | %.0f %% | %.0f %% | %d | %s |' % (k << 16, n, 100.0 * done / n, 100.0 * doneb / max(nb, 1), c['absent'], top))
    out.append('\n"Missing" counts only the functions the game reaches.')
    verdicts = os.path.join(a.out_dir, 'bulk-verdicts.md')   # work/decomp/bulk_final.py
    if os.path.exists(verdicts):
        out.append(open(verdicts, encoding='utf-8').read().rstrip('\n'))
    with open(os.path.join(a.out_dir, 'progress.md'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(out) + '\n')
    print('\n'.join(out))

if __name__ == '__main__':
    main()
