#!/usr/bin/env python3
"""Find bytes of the original game EXE inside a built port binary.

Usage: leak_check.py EXE BINARY [--nm NM] [--allow FILE] [--min 48] [--fail-at 512] [--report FILE]

EXE is a decompressed OR2006C2C.EXE (the player's; never committed). BINARY is
a built port binary (Switch ELF, host executable, Mac/PS5 ELF). Every run of
at least --min bytes that also appears in the EXE image is reported; with --nm
the runs are attributed to the binary's symbols (nearest preceding symbol
marked '?' when no symbol contains the run).

Runs in symbols matched by --allow (third-party libraries the EXE also links:
Ogg Vorbis, zlib, FFmpeg...) are ignored. The check fails (exit 1) when any
other single run reaches --fail-at bytes: a table copied from the EXE instead
of being read from the player's image (system/exe_image.hpp). Smaller runs are
listed: short constant tables written as part of ported functions.
"""
import argparse
import bisect
import re
import struct
import subprocess
import sys
from collections import defaultdict

WINDOW = 32
STRIDE = 16


def pe_image(data):
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional = struct.unpack_from('<H', data, pe + 20)[0]
    base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
    size = struct.unpack_from('<I', data, pe + 24 + 56)[0]
    image = bytearray(size)
    for i in range(count):
        o = pe + 24 + optional + i * 40
        vsize, rva, rsize, raw = struct.unpack_from('<IIII', data, o + 8)
        n = min(rsize, size - rva)
        image[rva:rva + n] = data[raw:raw + n]
    return base, bytes(image)


def interesting(w):
    return len(set(w)) >= 8


def symbols(nm, binary):
    out = subprocess.run([nm, '-S', '--defined-only', binary], capture_output=True, text=True, check=True).stdout
    syms = []
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 4:
            try:
                syms.append((int(p[0], 16), int(p[1], 16), p[3]))
            except ValueError:
                pass
    syms.sort()
    return syms


def elf_segments(binary):
    raw = open(binary, 'rb').read(1 << 20)
    if raw[:4] != b'\x7fELF' or raw[4] != 2:
        return None
    phoff, = struct.unpack_from('<Q', raw, 0x20)
    phentsize, phnum = struct.unpack_from('<HH', raw, 0x36)
    segs = []
    for i in range(phnum):
        o = phoff + i * phentsize
        ptype, = struct.unpack_from('<I', raw, o)
        off, vaddr, _, filesz = struct.unpack_from('<QQQQ', raw, o + 8)
        if ptype == 1:
            segs.append((off, filesz, vaddr))
    return segs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('exe')
    ap.add_argument('binary')
    ap.add_argument('--nm')
    ap.add_argument('--allow')
    ap.add_argument('--min', type=int, default=48)
    ap.add_argument('--fail-at', type=int, default=512)
    ap.add_argument('--report')
    a = ap.parse_args()
    allow = []
    if a.allow:
        for line in open(a.allow):
            line = line.strip()
            if line and not line.startswith('#'):
                allow.append(re.compile(line))
    base, image = pe_image(open(a.exe, 'rb').read())
    index = {}
    for e in range(0, len(image) - WINDOW, STRIDE):
        w = image[e:e + WINDOW]
        if interesting(w):
            index.setdefault(w, e)
    blob = open(a.binary, 'rb').read()
    runs = []
    o, end = 0, len(blob) - WINDOW
    while o < end:
        e = index.get(blob[o:o + WINDOW])
        if e is None:
            o += 1
            continue
        s, t = o, e
        while s > 0 and t > 0 and blob[s - 1] == image[t - 1]:
            s -= 1
            t -= 1
        f, g = o + WINDOW, e + WINDOW
        while f < len(blob) and g < len(image) and blob[f] == image[g]:
            f += 1
            g += 1
        if f - s >= a.min:
            runs.append((s, f - s, base + t))
        o = f
    segs = elf_segments(a.binary)
    syms = symbols(a.nm, a.binary) if a.nm else []
    addrs = [x[0] for x in syms]
    per = defaultdict(int)
    lines, failures, allowed = [], [], 0
    for off, n, va in runs:
        name, bare = '?', ''
        if segs and syms:
            mid = off + n // 2   # the run may start in zero padding before its table
            vaddr = next((v + mid - so for so, sz, v in segs if so <= mid < so + sz), None)
            if vaddr is not None:
                k = bisect.bisect_right(addrs, vaddr) - 1
                if k >= 0 and syms[k][0] <= vaddr < syms[k][0] + max(syms[k][1], 1):
                    name = bare = syms[k][2]
                elif k >= 0:
                    name = '%s+0x%X?' % (syms[k][2], vaddr - syms[k][0])
                    bare = syms[k][2]   # unsized data after a library table belongs to that library
        if bare and any(p.search(bare) for p in allow):
            allowed += n
            continue
        per[name] += n
        lines.append('file+0x%X len %d = EXE 0x%X [%s]' % (off, n, va, name))
        if n >= a.fail_at:
            failures.append(lines[-1])
    total = sum(per.values())
    summary = ['%d bytes of the EXE image found in %s outside the allowed libraries (%d allowed)' % (total, a.binary, allowed)]
    summary += ['%10d  %s' % (n, s) for s, n in sorted(per.items(), key=lambda x: -x[1])]
    text = '\n'.join(summary + [''] + lines) + '\n'
    if a.report:
        open(a.report, 'w').write(text)
    print('\n'.join(summary[:40]))
    if failures:
        print('\nFAIL: %d run(s) of %d bytes or more (tables to read from the EXE image):' % (len(failures), a.fail_at))
        print('\n'.join(failures))
        return 1
    print('\nOK: no run of %d bytes or more' % a.fail_at)
    return 0


if __name__ == '__main__':
    sys.exit(main())
