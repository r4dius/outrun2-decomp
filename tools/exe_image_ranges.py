#!/usr/bin/env python3
"""Emit src/system/exe_image_ranges.inc: every PC range of OR2006C2C.EXE the
port reads, with its SHA-256 in the supported Steam build.

The port carries no byte of the original executable. At first launch it maps
the player's decompressed EXE and checks these hashes (system/exe_image.cpp);
the hashes identify the data without containing it.

Usage: exe_image_ranges.py <decompressed Steam OR2006C2C.EXE> src/system/exe_image_ranges.inc [TRACE...]

TRACE files come from a run with OR2_EXE_TRACE=<file> (ranges read through
exe_image_bytes by tables built at load, e.g. the sound tables); keep the
merged trace in tools/exe_image_trace.txt.

The list is the union of the ranges named by the table generators
(extract_exe_ranges.py, generate_*_tables.py, the pack builders) and
tools/exe_image_ranges.txt (ranges read by hand-ported code). Pointer tables
(course descriptors, stage-17 tables, font descriptors) are followed in the
reference image to name the ranges they point at.
"""
import hashlib
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import extract_exe_ranges  # noqa: E402
import generate_camera_tables  # noqa: E402
import generate_shader_tables  # noqa: E402

STEAM_SHA256 = '7c4e8ec6fcc54e04bfd82e6165e789dfe462e898dcde6f1107d7cc72799fa93d'
IMAGE_BASE = 0x400000


def pe_image(data):
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional = struct.unpack_from('<H', data, pe + 20)[0]
    size = struct.unpack_from('<I', data, pe + 24 + 56)[0]
    image = bytearray(size)
    for i in range(count):
        o = pe + 24 + optional + i * 40
        _, rva, rsize, raw = struct.unpack_from('<IIII', data, o + 8)
        n = min(rsize, size - rva, max(0, len(data) - raw))
        image[rva:rva + n] = data[raw:raw + n]
    # Import address table: zeroed as in system/exe_image.cpp (tools differ).
    image[0x196000:0x196300] = bytes(0x300)
    return bytes(image)


def u32(image, va):
    return struct.unpack_from('<I', image, va - IMAGE_BASE)[0]


def ranges(image, traces=()):
    out = []
    add = lambda va, size, why: out.append((va, size, why))
    for base, end, why in extract_exe_ranges.RANGES:
        add(base, end - base, why)
    for _, va, size in generate_camera_tables.TABLES:
        add(va, size, 'camera tables (484EE0)')
    for _, va, size in generate_shader_tables.RANGES:
        add(va, size, 'renderer tables')
    add(0x5B2F68, 30 * 4, 'vehicle layout pointers')
    add(0x5B0CB8, 30 * 0x128, 'vehicle draw layouts')
    add(0x64D120, 30 * 4, 'vehicle colour list ids')
    add(0x599808, 410 * 0x18, 'event descriptors')
    add(0x59BE78, 128 * 0x14, 'event functions')
    add(0x5995B4, 37 * 0x10, 'mode table')
    add(0x6A54E0, 66 * 4, 'primary course descriptor table')
    add(0x6A55E8, 77 * 4, 'secondary course descriptor table')
    for i in range(66):
        d = u32(image, 0x6A54E0 + i * 4)
        add(d, 0x98, 'course descriptor')
        add(u32(image, d + 0x68), 0x60, 'stage-17 table +68')
        add(u32(image, d + 0x6C), 0x15E, 'stage-17 table +6C')
    add(0x5E3140, 0x2650 + 18 * 4, 'driving parameters')
    add(0x5E3050, 4 * 15 * 4, 'driving parameter selectors')
    add(0x5E94D8, 648, 'torque table 0')
    add(0x5E9760, 648, 'torque table 1')
    add(0x5E9DF0, 1024, 'brake table')
    add(0x76F7B8, 40, 'font descriptor pointers')
    for i in range(10):
        d = u32(image, 0x76F7B8 + i * 4)
        add(d, 32, 'font descriptor')
        token, metrics, kern, width, height, first, dx, dy, stride = struct.unpack_from('<IIIhhiiii', image, d - IMAGE_BASE)
        count = stride if kern else 128 - first
        add(metrics, count * 2, 'font glyph map')
        if kern:
            add(kern, count * count, 'font kerning')
    # Tables declared with OR2_EXE_BYTES and translated-code byte ranges.
    src = HERE.parent / 'src'
    for path in sorted(list(src.rglob('*.cpp')) + list(src.rglob('*.hpp')) + list(src.rglob('*.inc'))):
        text = path.read_text(errors='replace')
        for va, size in re.findall(r'OR2_EXE_(?:BYTES|COPY)\([^,()]+(?:\[\d+\])*,0x([0-9A-Fa-f]+)u,0x([0-9A-Fa-f]+)u\)', text):
            add(int(va, 16), int(size, 16), path.name)
        if 'TranslatedCodeData' in text:
            for va, size in re.findall(r'\{0x([0-9a-f]+)u,(\d+),nullptr\}', text):
                add(int(va, 16), int(size), path.name)
    for trace in traces:
        for line in open(trace):
            va, size = line.split()
            add(int(va, 16), int(size, 16), 'trace')
    extra = HERE / 'exe_image_ranges.txt'
    for line in extra.read_text().splitlines():
        line = line.split('#', 1)[0].strip()
        if line:
            va, size = line.split()[:2]
            add(int(va, 16), int(size, 0), 'exe_image_ranges.txt')
    return out


def merge(items):
    items = sorted((va, va + size) for va, size, _ in items if size > 0)
    merged = []
    for a, b in items:
        if merged and a <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], b)
        else:
            merged.append([a, b])
    return merged


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    data = open(sys.argv[1], 'rb').read()
    if hashlib.sha256(data).hexdigest() != STEAM_SHA256:
        raise SystemExit('expected the decompressed Steam OR2006C2C.EXE (sha256 %s)' % STEAM_SHA256)
    image = pe_image(data)
    merged = merge(ranges(image, sys.argv[3:]))
    lines = ['// Generated by tools/exe_image_ranges.py: PC ranges of OR2006C2C.EXE read by the port',
             '// and their SHA-256 in the Steam build. Hashes only, no data of the original.']
    total = 0
    for a, b in merged:
        digest = hashlib.sha256(image[a - IMAGE_BASE:b - IMAGE_BASE]).digest()
        lines.append('{0x%08Xu,0x%Xu,{{%s}}},' % (a, b - a, ','.join('0x%02x' % x for x in digest)))
        total += b - a
    open(sys.argv[2], 'w', newline='\n').write('\n'.join(lines) + '\n')
    print('%d ranges, %d bytes checked' % (len(merged), total))


if __name__ == '__main__':
    main()
