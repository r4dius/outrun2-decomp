#!/usr/bin/env python3
"""Translate the PC shader bytecode of the hash-pinned EXE into GLSL and C++.

The PC renderer creates every vertex and pixel shader from D3D9 bytecode
blobs stored in .rdata 61E700..625C40: vs_1_1 fragments linked by 40E140 and
standalone vs_1_1 shaders, ps_1_4 fragments linked by 40B200 and standalone
ps_1_1 shaders (special/effect shaders). A linked shader is the version token
followed by the concatenated bodies of its blobs, so any shader the PC can
create is a sequence of these blob bodies.

This tool emits, for every blob, one function per target:
  * GLSL: switch/shaders/pc_{vs,ps14,ps11}_uber_{vsh,fsh}.glsl, "ubershaders"
    whose main() runs the blob functions listed in a uniform program block
    (deko3d has no runtime shader compiler; uam compiles these at build time);
  * C++: src/platform/pc_shader_programs.cpp, the same functions over a CPU
    register state (host software renderer and translation tests).
plus the blob table used to split a created shader back into blob ids.

Semantics follow the D3D9 SM1 instruction definitions (register write masks,
source swizzles/modifiers, destination shift/saturate, co-issued pairs,
rcp/rsq special values, lit/dst, matrix macros, ps_1_1 texture addressing
texbeml/texm3x3pad/texm3x3vspec, ps_1_4 texld with _dz/_dw).

Usage: generate_pc_shaders.py OR2006C2C.EXE <repo root>
"""
import hashlib
import os
import struct
import sys

EXPECTED_SHA256 = 'bdafa88a5abdd2a9743f6bdcc5e2189288c0203790412fce10b9bb649933482f'
BLOB_RANGE = (0x61E700, 0x625C40)
VERSIONS = {0xFFFE0101: 'vs', 0xFFFF0101: 'ps11', 0xFFFF0104: 'ps14'}

OP = {0: 'nop', 1: 'mov', 2: 'add', 3: 'sub', 4: 'mad', 5: 'mul', 6: 'rcp', 7: 'rsq', 8: 'dp3', 9: 'dp4',
      10: 'min', 11: 'max', 12: 'slt', 13: 'sge', 16: 'lit', 17: 'dst', 18: 'lrp',
      20: 'm4x4', 21: 'm4x3', 22: 'm3x4', 23: 'm3x3', 24: 'm3x2', 31: 'dcl',
      0x42: 'tex', 0x44: 'texbeml', 0x49: 'texm3x3pad', 0x4D: 'texm3x3vspec',
      0x50: 'cnd', 0x51: 'def', 0x58: 'cmp'}


class Image:
    def __init__(self, data):
        self.d = data
        pe = struct.unpack_from('<I', data, 0x3C)[0]
        n = struct.unpack_from('<H', data, pe + 6)[0]
        opt = struct.unpack_from('<H', data, pe + 20)[0]
        self.secs = [struct.unpack_from('<8sIIII', data, pe + 24 + opt + i * 40) for i in range(n)]

    def u32(self, va):
        r = va - 0x400000
        for name, vs, rva, rs, raw in self.secs:
            if rva <= r < rva + rs:
                return struct.unpack_from('<I', self.d, raw + r - rva)[0]
        raise ValueError(hex(va))


def blobs(img):
    out = []
    va = BLOB_RANGE[0]
    while va < BLOB_RANGE[1]:
        t = img.u32(va)
        if t in VERSIONS:
            toks = [t]
            k = va + 4
            while True:
                x = img.u32(k)
                toks.append(x)
                k += 4
                if x == 0xFFFF:
                    break
            out.append((va, toks))
        va += 4
    return out


def reg_type(p):
    return ((p >> 28) & 7) | ((p >> 8) & 0x18)


def parse(toks):
    ins = []
    i = 1
    while i < len(toks) - 1:
        tok = toks[i]
        op = tok & 0xFFFF
        if op == 0xFFFE:
            i += 1 + ((tok >> 16) & 0x7FFF)
            continue
        if op not in OP:
            raise ValueError('unsupported opcode %d' % op)
        if op == 0x51:
            j = i + 6
        else:
            j = i + 1
            while j < len(toks) - 1 and (toks[j] & 0x80000000):
                j += 1
        ins.append((OP[op], toks[i + 1:j], bool(tok & 0x40000000)))
        i = j
    return ins


def fbits(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


class Gen:
    """Emits one blob as GLSL (glsl=True) or C++ statements."""

    def __init__(self, kind, glsl):
        self.kind = kind
        self.glsl = glsl
        self.defs = {}

    def regref(self, p):
        t = reg_type(p)
        n = p & 0x7FF
        g = self.glsl
        k = self.kind
        if k == 'vs':
            if t == 0: return ('R[%d]' if g else 's.R[%d]') % n
            if t == 1: return ('V[%d]' if g else 's.V[%d]') % n
            if t == 2: return ('C[%d]' if g else 's.C[%d]') % n
            if t == 4: return ['oPos', 'oFog', 'oPts'][n] if g else 's.' + ['oPos', 'oFog', 'oPts'][n]
            if t == 5: return ('oD[%d]' if g else 's.oD[%d]') % n
            if t == 6: return ('oT[%d]' if g else 's.oT[%d]') % n
        else:
            if t == 0: return ('R[%d]' if g else 's.R[%d]') % n
            if t == 1: return ('VC[%d]' if g else 's.VC[%d]') % n
            if t == 2:
                if n in self.defs: return self.defs[n]
                return ('PC[%d]' if g else 's.PC[%d]') % n
            if t == 3: return ('T[%d]' if g else 's.T[%d]') % n
        raise ValueError('register type %d in %s' % (t, k))

    def swz(self, e, sw):
        comps = [(sw >> (2 * i)) & 3 for i in range(4)]
        if comps == [0, 1, 2, 3]:
            return e
        if self.glsl:
            return '(%s).%s' % (e, ''.join('xyzw'[c] for c in comps))
        return 'swz(%s,%d,%d,%d,%d)' % (e, *comps)

    def v4(self, x):
        return ('vec4(%s)' if self.glsl else 'v4(%s)') % x

    def src(self, p):
        e = self.swz(self.regref(p), (p >> 16) & 0xFF)
        m = (p >> 24) & 0xF
        h, one, two = self.v4('0.5'), self.v4('1.0'), self.v4('2.0')
        if m == 0: return e
        if m == 1: return '(-(%s))' % e if self.glsl else 'neg(%s)' % e
        if self.glsl:
            return {2: '((%s)-%s)' % (e, h), 3: '(-((%s)-%s))' % (e, h), 4: '((%s)*2.0-%s)' % (e, one),
                    5: '(-((%s)*2.0-%s))' % (e, one), 6: '(%s-(%s))' % (one, e), 7: '((%s)*2.0)' % e,
                    8: '(-((%s)*2.0))' % e}[m]
        return {2: 'sub(%s,%s)' % (e, h), 3: 'neg(sub(%s,%s))' % (e, h), 4: 'sub(mul(%s,%s),%s)' % (e, two, one),
                5: 'neg(sub(mul(%s,%s),%s))' % (e, two, one), 6: 'sub(%s,%s)' % (one, e), 7: 'mul(%s,%s)' % (e, two),
                8: 'neg(mul(%s,%s))' % (e, two)}[m]

    def fn(self, name, *args):
        if self.glsl:
            table = {'add': '(%s+%s)', 'sub': '(%s-%s)', 'mul': '(%s*%s)', 'mad': '(%s*%s+%s)'}
            if name in table: return table[name] % args
            return '%s(%s)' % (name, ','.join(args))
        return '%s(%s)' % (name, ','.join(args))

    def value(self, op, dst, srcs):
        s = [self.src(p) for p in srcs]
        g = self.glsl
        if op == 'mov': return s[0]
        if op in ('add', 'sub', 'mul', 'mad'): return self.fn(op, *s)
        if op == 'dp3': return ('vec4(dot((%s).xyz,(%s).xyz))' % (s[0], s[1])) if g else 'dp3(%s,%s)' % (s[0], s[1])
        if op == 'dp4': return ('vec4(dot(%s,%s))' % (s[0], s[1])) if g else 'dp4(%s,%s)' % (s[0], s[1])
        if op == 'min': return self.fn('min', *s)
        if op == 'max': return self.fn('max', *s)
        if op == 'slt': return ('vec4(lessThan(%s,%s))' % (s[0], s[1])) if g else 'slt(%s,%s)' % (s[0], s[1])
        if op == 'sge': return ('vec4(greaterThanEqual(%s,%s))' % (s[0], s[1])) if g else 'sge(%s,%s)' % (s[0], s[1])
        if op == 'rcp': return 'd3d_rcp((%s).w)' % s[0] if g else 'd3d_rcp(%s.w)' % s[0]
        if op == 'rsq': return 'd3d_rsq((%s).w)' % s[0] if g else 'd3d_rsq(%s.w)' % s[0]
        if op == 'lit': return 'd3d_lit(%s)' % s[0]
        if op == 'dst': return 'd3d_dst(%s,%s)' % (s[0], s[1])
        if op == 'lrp': return ('mix(%s,%s,%s)' % (s[2], s[1], s[0])) if g else 'lrp(%s,%s,%s)' % (s[0], s[1], s[2])
        if op == 'cnd': return ('mix(%s,%s,vec4(greaterThan(%s,vec4(0.5))))' % (s[2], s[1], s[0])) if g else 'cnd(%s,%s,%s)' % (s[0], s[1], s[2])
        if op == 'cmp': return ('mix(%s,%s,vec4(greaterThanEqual(%s,vec4(0.0))))' % (s[2], s[1], s[0])) if g else 'cmp(%s,%s,%s)' % (s[0], s[1], s[2])
        raise ValueError(op)

    def matrix(self, op, dst, srcs):
        rows = {'m4x4': (4, 4), 'm4x3': (4, 3), 'm3x4': (3, 4), 'm3x3': (3, 3), 'm3x2': (3, 2)}[op]
        a = self.src(srcs[0])
        parts = []
        for r in range(rows[1]):
            base = srcs[1]
            row = (base & ~0x7FF) | ((base & 0x7FF) + r)
            b = self.src(row)
            if rows[0] == 4:
                parts.append(('dot(%s,%s)' % (a, b)) if self.glsl else 'dp4(%s,%s).x' % (a, b))
            else:
                parts.append(('dot((%s).xyz,(%s).xyz)' % (a, b)) if self.glsl else 'dp3(%s,%s).x' % (a, b))
        while len(parts) < 4:
            parts.append('0.0' if self.glsl else '0.f')
        mask = {4: 0xF, 3: 0x7, 2: 0x3}[rows[1]]
        return self.v4(','.join(parts)), mask

    def write(self, dst, expr, mask_override=None):
        """Statements storing expr to the destination parameter."""
        mask = (dst >> 16) & 0xF
        if mask_override is not None:
            mask &= mask_override
        mod = (dst >> 20) & 0xF
        shift = (dst >> 24) & 0xF
        shift = shift - 16 if shift >= 8 else shift
        e = expr
        if shift:
            f = 2.0 ** shift
            e = ('((%s)*%r)' % (e, f)) if self.glsl else 'mul(%s,v4(%rf))' % (e, f)
        if mod & 1:
            e = ('clamp(%s,0.0,1.0)' % e) if self.glsl else 'sat(%s)' % e
        r = self.regref(dst)
        if self.glsl:
            if mask == 0xF:
                return ['%s=%s;' % (r, e)]
            comps = ''.join('xyzw'[i] for i in range(4) if mask >> i & 1)
            return ['%s.%s=(%s).%s;' % (r, comps, e, comps)]
        return ['wmask(%s,%s,%d);' % (r, e, mask)]

    def blob(self, toks):
        ins = parse(toks)
        # def constants (ps) are shader-wide; substitute them.
        for op, ps, co in ins:
            if op == 'def':
                n = ps[0] & 0x7FF
                vals = ','.join('%r' % fbits(x) + ('' if self.glsl else 'f') for x in ps[1:5])
                self.defs[n] = self.v4(vals)
        lines = []
        pending = []  # co-issue pairs: evaluate both, then write both
        tmp = [0]

        def flush():
            for st in pending:
                lines.append(st)
            pending.clear()

        for idx, (op, ps, co) in enumerate(ins):
            if op in ('nop', 'dcl', 'def'):
                continue
            if op in ('tex', 'texbeml', 'texm3x3pad', 'texm3x3vspec'):
                lines.extend(self.texture(op, ps))
                continue
            if op.startswith('m') and op[1:2].isdigit():
                expr, m = self.matrix(op, ps[0], ps[1:])
                name = 't%d' % tmp[0]; tmp[0] += 1
                lines.append(('vec4 %s=%s;' % (name, expr)) if self.glsl else 'V4 %s=%s;' % (name, expr))
                lines.extend(self.write(ps[0], name, m))
                continue
            expr = self.value(op, ps[0], ps[1:])
            name = 't%d' % tmp[0]; tmp[0] += 1
            decl = ('vec4 %s=%s;' % (name, expr)) if self.glsl else 'V4 %s=%s;' % (name, expr)
            nxt_co = idx + 1 < len(ins) and ins[idx + 1][2]
            if co:
                lines.append(decl)
                pending.extend(self.write(ps[0], name))
                flush()
            elif nxt_co:
                lines.append(decl)
                pending.extend(self.write(ps[0], name))
            else:
                flush()
                lines.append(decl)
                lines.extend(self.write(ps[0], name))
        flush()
        return lines

    def texture(self, op, ps):
        g = self.glsl
        d = ps[0]
        n = d & 0x7FF
        if self.kind == 'ps14':
            # texld rN, src: sample stage N at src (optionally divided by z or w).
            s = ps[1]
            m = (s >> 24) & 0xF
            coord = self.swz(self.regref(s), (s >> 16) & 0xFF)
            if m == 9:
                coord = ('vec4((%s).xy/(%s).z,0.0,1.0)' % (coord, coord)) if g else 'proj_z(%s)' % coord
            elif m == 10:
                coord = ('vec4((%s).xy/(%s).w,0.0,1.0)' % (coord, coord)) if g else 'proj_w(%s)' % coord
            e = ('sample_stage(%d,%s)' % (n, coord)) if g else 's.sample(%d,%s)' % (n, coord)
            return self.write(d, e)
        # ps_1_1: t registers.
        if op == 'tex':
            e = ('sample_stage(%d,TC[%d])' % (n, n)) if g else 's.sample(%d,s.TC[%d])' % (n, n)
            return ['T[%d]=%s;' % (n, e)] if g else ['s.T[%d]=%s;' % (n, e)]
        m = ps[1] & 0x7FF
        if op == 'texbeml':
            if g:
                return ['T[%d]=sample_bump(%d,TC[%d],T[%d]);' % (n, n, n, m)]
            return ['s.T[%d]=s.sample_bump(%d,s.TC[%d],s.T[%d]);' % (n, n, n, m)]
        if op == 'texm3x3pad':
            if g:
                return ['M3[%d]=dot(TC[%d].xyz,T[%d].xyz);' % (n, n, m)]
            return ['s.M3[%d]=dp3(s.TC[%d],s.T[%d]).x;' % (n, n, m)]
        if op == 'texm3x3vspec':
            if g:
                return ['T[%d]=sample_vspec(%d,vec3(M3[%d],M3[%d],dot(TC[%d].xyz,T[%d].xyz)),vec3(TC[%d].w,TC[%d].w,TC[%d].w));'
                        % (n, n, n - 2, n - 1, n, m, n - 2, n - 1, n)]
            return ['s.T[%d]=s.sample_vspec(%d,s.M3[%d],s.M3[%d],dp3(s.TC[%d],s.T[%d]).x,v4(s.TC[%d].w,s.TC[%d].w,s.TC[%d].w,0.f));'
                    % (n, n, n - 2, n - 1, n, m, n - 2, n - 1, n)]
        raise ValueError(op)


def dcl_table(toks):
    """(usage, usage index) -> input register for a vs blob."""
    out = []
    for op, ps, co in parse(toks):
        if op == 'dcl':
            out.append((ps[0] & 0x1F, (ps[0] >> 16) & 0xF, ps[1] & 0x7FF))
    return out


GLSL_COMMON = r'''
vec4 d3d_rcp(float v){float f=v==1.0?1.0:(v==0.0?uintBitsToFloat(0x7f800000u):1.0/v);return vec4(f);}
vec4 d3d_rsq(float v){v=abs(v);float f=v==1.0?1.0:(v==0.0?uintBitsToFloat(0x7f800000u):inversesqrt(v));return vec4(f);}
vec4 d3d_lit(vec4 s){vec4 d=vec4(1.0,0.0,0.0,1.0);float p=clamp(s.w,-127.9961,127.9961);
    if(s.x>0.0){d.y=s.x;if(s.y>0.0)d.z=pow(s.y,p);}return d;}
vec4 d3d_dst(vec4 a,vec4 b){return vec4(1.0,a.y*b.y,a.z,b.w);}
'''


def emit(img, root):
    all_blobs = blobs(img)
    kinds = {'vs': [], 'ps11': [], 'ps14': []}
    for va, toks in all_blobs:
        kinds[VERSIONS[toks[0]]].append((va, toks))
    # ---- GLSL ----
    head = '// Generated by tools/generate_pc_shaders.py from the hash-pinned EXE. Do not edit.\n#version 460\n'
    vs = [head, '// The program words follow the constants in one uniform buffer (binding 0,',
          '// 0x1100 bytes): read through a separate binding they reached the',
          '// console GPU wrong (2026-09-30 diagnostic mode 7).',
          'layout(std140,binding=0) uniform VsConstants{vec4 C[256];vec4 P[4];};',
          'layout(std140,binding=2) uniform VsFix{vec4 clip_fix;};']
    for k in range(16):
        vs.append('layout(location=%d) in vec4 in_v%d;' % (k, k))
    vs += ['layout(location=0) out vec4 out_d0;', 'layout(location=1) out vec4 out_d1;']
    for k in range(8):
        vs.append('layout(location=%d) out vec4 out_t%d;' % (2 + k, k))
    vs += ['layout(location=10) out float out_fog;',
           'vec4 R[12];vec4 V[16];vec4 oPos;vec4 oFog;vec4 oPts;vec4 oD[2];vec4 oT[8];', GLSL_COMMON]
    for i, (va, toks) in enumerate(kinds['vs']):
        body = Gen('vs', True).blob(toks)
        vs.append('void f%d(){ // %X\n    %s\n}' % (i, va, '\n    '.join(body)))
    vs.append('void run(int id){switch(id){')
    for i in range(len(kinds['vs'])):
        vs.append('case %d:f%d();break;' % (i, i))
    vs.append('default:break;}}')
    vs.append('''void main(){
    for(int k=0;k<12;++k)R[k]=vec4(0.0);
    V[0]=in_v0;V[1]=in_v1;V[2]=in_v2;V[3]=in_v3;V[4]=in_v4;V[5]=in_v5;V[6]=in_v6;V[7]=in_v7;
    V[8]=in_v8;V[9]=in_v9;V[10]=in_v10;V[11]=in_v11;V[12]=in_v12;V[13]=in_v13;V[14]=in_v14;V[15]=in_v15;
    oPos=vec4(0.0);oFog=vec4(1.0);oPts=vec4(0.0);oD[0]=vec4(0.0);oD[1]=vec4(0.0);
    for(int k=0;k<8;++k)oT[k]=vec4(0.0);
    // Blob list as floats (P, after the constants), unrolled with constant
    // indices: integer words and dynamic indexing reached the console GPU
    // wrong (2026-09-30 diagnostic modes 7/8). -1 ends the list.
    if(P[0].x>=0.0)run(int(P[0].x));if(P[0].y>=0.0)run(int(P[0].y));if(P[0].z>=0.0)run(int(P[0].z));if(P[0].w>=0.0)run(int(P[0].w));if(P[1].x>=0.0)run(int(P[1].x));if(P[1].y>=0.0)run(int(P[1].y));if(P[1].z>=0.0)run(int(P[1].z));if(P[1].w>=0.0)run(int(P[1].w));if(P[2].x>=0.0)run(int(P[2].x));if(P[2].y>=0.0)run(int(P[2].y));if(P[2].z>=0.0)run(int(P[2].z));if(P[2].w>=0.0)run(int(P[2].w));if(P[3].x>=0.0)run(int(P[3].x));if(P[3].y>=0.0)run(int(P[3].y));if(P[3].z>=0.0)run(int(P[3].z));if(P[3].w>=0.0)run(int(P[3].w));
    // D3D9 -> deko3d clip space. With the device's upper-left origin clip +y
    // is the top of the screen, as in D3D9 (hardware: the +y-up SUMO_FE and
    // glyph quads show upright), so y is not flipped. D3D9 pixel centres:
    // x + 1/w, y - 1/h (clip_fix.xy, times w), as Wine's position fixup.
    gl_Position=vec4(oPos.x+clip_fix.x*oPos.w,oPos.y-clip_fix.y*oPos.w,oPos.z,oPos.w);
    // Console diagnostic 6 (clip_fix.w): position = v0 * c64..c67 without the blob program.
    if(clip_fix.w==6.0)gl_Position=vec4(dot(in_v0,C[64]),dot(in_v0,C[65]),dot(in_v0,C[66]),dot(in_v0,C[67]));
    out_d0=clamp(oD[0],0.0,1.0);out_d1=clamp(oD[1],0.0,1.0);
    if(clip_fix.w==7.0){gl_Position=vec4(dot(in_v0,C[64]),dot(in_v0,C[65]),dot(in_v0,C[66]),dot(in_v0,C[67]));
        out_d0=vec4(P[0].x/80.0,P[0].y/80.0,P[0].z/80.0,1.0);}
    if(clip_fix.w==8.0){gl_Position=vec4(dot(in_v0,C[64]),dot(in_v0,C[65]),dot(in_v0,C[66]),dot(in_v0,C[67]));
        out_d0=vec4(clamp(oPos.w/200.0,0.0,1.0),clamp(abs(oPos.x/oPos.w),0.0,1.0),clamp(oPos.z/oPos.w,0.0,1.0),1.0);}
    out_t0=oT[0];out_t1=oT[1];out_t2=oT[2];out_t3=oT[3];out_t4=oT[4];out_t5=oT[5];out_t6=oT[6];out_t7=oT[7];
    out_fog=clamp(oFog.x,0.0,1.0);
}''')
    ps_common = [head,
                 'layout(std140,binding=0) uniform PsConstants{vec4 PC[8];};',
                 'layout(std140,binding=1) uniform PsProgram{ivec4 program[4];ivec4 info;ivec4 stage_kind[2];vec4 bump[8];vec4 lum[2];};',
                 'layout(std140,binding=2) uniform PsFixed{vec4 fog_colour;vec4 fog_params;vec4 alpha_test;};',
                 'layout(location=0) in vec4 in_d0;', 'layout(location=1) in vec4 in_d1;']
    for k in range(8):
        ps_common.append('layout(location=%d) in vec4 in_t%d;' % (2 + k, k))
    ps_common += ['layout(location=10) in float in_fog;', 'layout(location=0) out vec4 out_colour;']
    for k in range(6):
        ps_common.append('layout(binding=%d) uniform sampler2D tex2d%d;' % (k, k))
        ps_common.append('layout(binding=%d) uniform samplerCube texcube%d;' % (6 + k, k))
    sampler = ['vec4 sample_stage(int n,vec4 c){']
    for k in range(6):
        sampler.append('    if(n==%d)return stage_kind[%d][%d]==1?texture(texcube%d,c.xyz):texture(tex2d%d,c.xy);' % (k, k >> 2, k & 3, k, k))
    sampler.append('    return vec4(0.0);}')
    epilogue = '''    vec4 c=R[0];
    // Fixed-function stages after the pixel shader: alpha test, fog.
    // Console diagnostic (alpha_test.w, Minus button): 2 = output alpha as grey, 3 = vertex diffuse alpha, 1 = no alpha test.
    if(alpha_test.w>=7.0){out_colour=vec4(in_d0.rgb,1.0);return;}
    if(alpha_test.w>=5.0){out_colour=vec4(1.0,0.0,0.0,1.0);return;}
    if(alpha_test.w==2.0){out_colour=vec4(vec3(clamp(c.a,0.0,1.0)),1.0);return;}
    if(alpha_test.w==3.0){out_colour=vec4(vec3(clamp(in_d0.a,0.0,1.0)),1.0);return;}
    if(alpha_test.x!=0.0&&alpha_test.w!=1.0){float a=floor(clamp(c.a,0.0,1.0)*255.0+0.5);float ref=alpha_test.y;int f=int(alpha_test.z);
        bool pass=f==8||(f==2&&a<ref)||(f==3&&a==ref)||(f==4&&a<=ref)||(f==5&&a>ref)||(f==6&&a!=ref)||(f==7&&a>=ref);
        if(!pass)discard;}
    if(fog_params.x!=0.0){float fz=gl_FragCoord.z;float fw=1.0/gl_FragCoord.w;float d=fog_params.w!=0.0?fw:fz;float f;
        int mode=int(fog_params.x);
        if(mode==3)f=(fog_colour.w-d)/max(fog_colour.w-fog_params.y,1e-20);
        else if(mode==1)f=exp(-fog_params.z*d);
        else if(mode==2)f=exp(-(fog_params.z*d)*(fog_params.z*d));
        else f=in_fog;
        f=clamp(f,0.0,1.0);c.rgb=mix(fog_colour.rgb,c.rgb,f);}
    out_colour=c;'''
    for kind, regs in (('ps14', 'vec4 R[6];vec4 T[6];vec4 VC[2];'), ('ps11', 'vec4 R[2];vec4 T[4];vec4 TC[4];vec4 VC[2];float M3[4];')):
        src = list(ps_common) + [regs, GLSL_COMMON] + sampler
        if kind == 'ps11':
            src.append('''vec4 sample_bump(int n,vec4 tc,vec4 t){
    vec4 m=bump[n];vec2 uv=tc.xy+vec2(m.x*t.x+m.z*t.y,m.y*t.x+m.w*t.y);
    vec4 c=sample_stage(n,vec4(uv,0.0,1.0));vec4 l=lum[n>>1];float s=(n&1)==0?l.x:l.z;float o=(n&1)==0?l.y:l.w;
    c.rgb*=clamp(t.z*s+o,0.0,1.0);return c;}
vec4 sample_vspec(int n,vec3 nrm,vec3 eye){vec3 r=2.0*nrm*dot(nrm,eye)/dot(nrm,nrm)-eye;return sample_stage(n,vec4(r,1.0));}''')
        for i, (va, toks) in enumerate(kinds[kind]):
            body = Gen(kind, True).blob(toks)
            src.append('void f%d(){ // %X\n    %s\n}' % (i, va, '\n    '.join(body)))
        src.append('void run(int id){switch(id){')
        for i in range(len(kinds[kind])):
            src.append('case %d:f%d();break;' % (i, i))
        src.append('default:break;}}')
        init = ('for(int k=0;k<6;++k){R[k]=vec4(0.0);}T[0]=in_t0;T[1]=in_t1;T[2]=in_t2;T[3]=in_t3;T[4]=in_t4;T[5]=in_t5;'
                if kind == 'ps14' else
                'R[0]=vec4(0.0);R[1]=vec4(0.0);for(int k=0;k<4;++k){T[k]=vec4(0.0);M3[k]=0.0;}TC[0]=in_t0;TC[1]=in_t1;TC[2]=in_t2;TC[3]=in_t3;')
        src.append('void main(){\n    %s\n    VC[0]=in_d0;VC[1]=in_d1;\n    // Unrolled with constant indices (dynamic indexing misbehaved on the\n    // console GPU, 2026-09-30); a negative entry ends the list.\n    if(program[0].x>=0)run(program[0].x);if(program[0].y>=0)run(program[0].y);if(program[0].z>=0)run(program[0].z);if(program[0].w>=0)run(program[0].w);if(program[1].x>=0)run(program[1].x);if(program[1].y>=0)run(program[1].y);if(program[1].z>=0)run(program[1].z);if(program[1].w>=0)run(program[1].w);if(program[2].x>=0)run(program[2].x);if(program[2].y>=0)run(program[2].y);if(program[2].z>=0)run(program[2].z);if(program[2].w>=0)run(program[2].w);if(program[3].x>=0)run(program[3].x);if(program[3].y>=0)run(program[3].y);if(program[3].z>=0)run(program[3].z);if(program[3].w>=0)run(program[3].w);\n%s\n}' % (init, epilogue))
        with open(os.path.join(root, 'switch/shaders/pc_%s_uber_fsh.glsl' % kind), 'w', newline='\n') as f:
            f.write('\n'.join(src) + '\n')
    with open(os.path.join(root, 'switch/shaders/pc_vs_uber_vsh.glsl'), 'w', newline='\n') as f:
        f.write('\n'.join(vs) + '\n')
    # ---- C++ ----
    cpp = ['// Generated by tools/generate_pc_shaders.py from the hash-pinned EXE. Do not edit.',
           '#include "system/exe_image.hpp"', '#include <cstring>', '#include "platform/pc_shader_programs.hpp"', 'namespace outrun::platform {', 'namespace {',
           'using namespace pc_shader;']
    tables = []
    for kind, state in (('vs', 'VsState'), ('ps14', 'Ps14State'), ('ps11', 'Ps11State')):
        names = []
        for i, (va, toks) in enumerate(kinds[kind]):
            body = Gen(kind, False).blob(toks)
            cpp.append('void %s_%d(%s& s){ // %X\n    %s\n}' % (kind, i, state, va, '\n    '.join(body) if body else '(void)s;'))
            names.append('%s_%d' % (kind, i))
        tables.append((kind, state, names))
    cpp.append('}')
    for kind, state, names in tables:
        cpp.append('const PcShaderBlobFn<%s> PcShaderBlobFns_%s[%d]={%s};' % (state, kind, len(names), ','.join(names)))
    blob_lines = []
    token_lines = []
    offset = 0
    for kind in ('vs', 'ps11', 'ps14'):
        for i, (va, toks) in enumerate(kinds[kind]):
            body = toks[1:-1]
            blob_lines.append('    {0x%XU,0x%08XU,%dU,%dU,%dU},' % (va, toks[0], i, offset, len(body)))
            token_lines.append(','.join('0x%XU' % t for t in body))
            offset += len(body)
    # Tokens are not emitted: they are copied from the player's EXE image.
    cpp.append('std::uint32_t PcShaderBlobTokens[%d]{};' % max(offset, 1))
    cpp.append('const PcShaderBlob PcShaderBlobs[%d]={' % len(blob_lines))
    cpp.extend(blob_lines)
    cpp.append('};')
    cpp.append('const std::size_t PcShaderBlobCount=%d;' % len(blob_lines))
    cpp.append('const std::size_t PcShaderBlobCounts[3]={%d,%d,%d}; // vs, ps11, ps14' % (
        len(kinds['vs']), len(kinds['ps11']), len(kinds['ps14'])))
    cpp.append("// Shader bytecode tokens: copied from the player's EXE image at each blob\n// address (version token excluded, END excluded) when the image is installed.\nvoid build_shader_blob_tokens(){\n    for(std::size_t i=0;i<PcShaderBlobCount;++i){\n        const auto& b=PcShaderBlobs[i];\n        std::memcpy(PcShaderBlobTokens+b.token_offset,exe_image_bytes(b.va+4u,b.token_count*4u),b.token_count*4u);\n    }\n}\nOR2_EXE_BIND(build_shader_blob_tokens);")
    cpp.append('}')
    with open(os.path.join(root, 'src/platform/pc_shader_programs_generated.cpp'), 'w', newline='\n') as f:
        f.write('\n'.join(cpp) + '\n')
    print('blobs: vs %d, ps11 %d, ps14 %d' % (len(kinds['vs']), len(kinds['ps11']), len(kinds['ps14'])))


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    data = open(sys.argv[1], 'rb').read()
    if hashlib.sha256(data).hexdigest() != EXPECTED_SHA256:
        raise SystemExit('unexpected OR2006C2C.EXE hash')
    emit(Image(data), sys.argv[2])


if __name__ == '__main__':
    main()
