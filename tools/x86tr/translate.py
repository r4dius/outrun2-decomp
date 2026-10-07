#!/usr/bin/env python3
"""Mechanical x86 -> C++ translation of OR2006C2C.EXE functions.

Input: an objdump Intel-syntax listing of the EXE's .text (see disasm()).
Output: one C++ file with one function per translated PC function:
    void F_<addr>(or2x86::Cpu& c)
Registers live in locals (spilled around calls), memory is the flat guest
memory of x86rt.hpp, and every instruction is translated on its own, so the
result does what the original instruction stream does. Anything the
translator cannot express is reported (the function is left out), never
approximated.

usage: translate.py text.asm out.cpp ADDR [ADDR...]   (or @file with addresses)
"""
import re, sys, bisect, struct, subprocess, os

EXE = os.environ.get('OR2_EXE', 'OR2006C2C.EXE')   # the Steam build (plain code)

# ---------------------------------------------------------------- listing
class Ins:
    __slots__ = ('a', 'n', 'mn', 'ops', 'text', 'pfx', 'b')
def load_listing(path):
    return parse_listing(open(path, encoding='utf-8', errors='replace'))
def objdump_range(lo, hi):
    """Disassemble [lo, hi) of the EXE from lo (any section, .rld included):
    a jump target the linear listing missed (the protection's junk bytes
    desynchronise objdump's sweep) is decoded from its real start."""
    exe = EXE
    cmd = ['objdump', '-D', '-M', 'intel', '--start-address=0x%x' % lo, '--stop-address=0x%x' % hi]
    if os.name == 'nt':
        m = re.match(r'^([A-Za-z]):[/\\](.*)$', exe)
        exe = '/mnt/%s/%s' % (m.group(1).lower(), m.group(2).replace('\\', '/'))
        cmd = ['wsl'] + cmd
    out = subprocess.run(cmd + [exe], capture_output=True, text=True, errors='replace').stdout
    return parse_listing(out.splitlines())
def parse_listing(lines):
    ins = []
    cur = None
    for l in lines:
        m = re.match(r'\s+([0-9a-f]+):\t((?:[0-9a-f]{2} )+)\s*(?:\t(.*))?$', l.rstrip('\n'))
        if not m:
            continue
        a = int(m.group(1), 16); nb = len(m.group(2).split())
        if m.group(3) is None:          # continuation of the previous instruction's bytes
            if cur is not None and cur.a + cur.n == a:
                cur.n += nb; cur.b += [int(x, 16) for x in m.group(2).split()]
            continue
        i = Ins(); i.a = a; i.n = nb; i.b = [int(x, 16) for x in m.group(2).split()]; i.text = m.group(3).strip()
        t = i.text
        i.pfx = []
        while True:
            mm = re.match(r'(rep|repz|repe|repnz|repne|lock|data16|ds|es|cs|ss|fs|gs)\s+(.*)', t)
            if not mm: break
            i.pfx.append(mm.group(1)); t = mm.group(2)
        mm = re.match(r'(\S+)\s*(.*)$', t)
        i.mn = mm.group(1); rest = mm.group(2)
        rest = re.sub(r'\s*<[^>]*>', '', rest).strip()
        i.ops = split_ops(rest) if rest else []
        ins.append(i); cur = i
    return ins
def split_ops(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch == '[': depth += 1
        if ch == ']': depth -= 1
        if ch == ',' and depth == 0 and not cur.endswith('st('):
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    if cur.strip(): out.append(cur.strip())
    return out

# ---------------------------------------------------------------- EXE bytes
_exe = None
def exe_u8(a):
    v = exe_u32(a & ~3)
    return None if v is None else (v >> (8 * (a & 3))) & 0xff
def exe_u32(a):
    global _exe
    if _exe is None:
        D = open(EXE, 'rb').read()
        pe = struct.unpack_from('<I', D, 0x3c)[0]; n = struct.unpack_from('<H', D, pe + 6)[0]; oh = struct.unpack_from('<H', D, pe + 20)[0]
        secs = []
        for k in range(n):
            o = pe + 24 + oh + k * 40; vs, va, rs, ro = struct.unpack_from('<IIII', D, o + 8)
            secs.append((0x400000 + va, max(vs, rs), ro, rs))
        _exe = (D, secs)
    D, secs = _exe
    for va, sz, ro, rs in secs:
        if va <= a < va + sz:
            off = a - va
            return struct.unpack_from('<I', D, ro + off)[0] if off + 4 <= rs else None
    return None

# ---------------------------------------------------------------- operands
R32 = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
R16 = {'ax': 'eax', 'cx': 'ecx', 'dx': 'edx', 'bx': 'ebx', 'sp': 'esp', 'bp': 'ebp', 'si': 'esi', 'di': 'edi'}
R8L = {'al': 'eax', 'cl': 'ecx', 'dl': 'edx', 'bl': 'ebx'}
R8H = {'ah': 'eax', 'ch': 'ecx', 'dh': 'edx', 'bh': 'ebx'}
SIZES = {'BYTE': 8, 'WORD': 16, 'DWORD': 32, 'QWORD': 64, 'TBYTE': 80, 'XMMWORD': 128, 'FWORD': 48}
class Unsupported(Exception):
    pass
class Op:
    __slots__ = ('kind', 'reg', 'w', 'imm', 'addr', 'seg')
def parse_op(s):
    o = Op(); o.seg = None
    s = s.strip()
    if s in R32: o.kind = 'r'; o.reg = s; o.w = 32; return o
    if s in R16: o.kind = 'r'; o.reg = R16[s]; o.w = 16; return o
    if s in R8L: o.kind = 'r'; o.reg = R8L[s]; o.w = 8; return o
    if s in R8H: o.kind = 'rh'; o.reg = R8H[s]; o.w = 8; return o
    m = re.match(r'^st(?:\((\d)\))?$', s)
    if m: o.kind = 'st'; o.imm = int(m.group(1) or 0); o.w = 80; return o
    m = re.match(r'^xmm(\d)$', s)
    if m: o.kind = 'x'; o.imm = int(m.group(1)); o.w = 128; return o
    m = re.match(r'^(-?0x[0-9a-f]+|-?\d+)$', s)
    if m: o.kind = 'i'; o.imm = int(s, 0) & 0xffffffff; o.w = 0; return o
    m = re.match(r'^(?:(BYTE|WORD|DWORD|QWORD|TBYTE|XMMWORD|FWORD) PTR )?(?:([a-z]s):)?(\[.*\]|0x[0-9a-f]+)$', s)
    if m:
        o.kind = 'm'; o.w = SIZES[m.group(1)] if m.group(1) else 0; o.seg = m.group(2)
        body = m.group(3)
        if o.seg == 'gs':
            raise Unsupported('segment gs')
        o.addr = addr_expr(body[1:-1] if body.startswith('[') else body)
        if o.seg == 'fs':                      # thread block: FS base + offset
            o.addr = 'u32(c.fs_base+%s)' % o.addr
        return o
    raise Unsupported('operand ' + s)
def addr_expr(e):
    terms = re.findall(r'[+-]?[^+-]+', e.replace(' ', ''))
    parts = []; disp = 0
    for t in terms:
        sign = -1 if t.startswith('-') else 1
        t = t.lstrip('+-')
        m = re.match(r'^([a-z]+)\*(\d)$', t)
        if m and m.group(1) in R32:
            parts.append('%s*%su' % (m.group(1), m.group(2))); continue
        if t in R32:
            parts.append(t); continue
        if re.match(r'^(0x[0-9a-f]+|\d+)$', t):
            disp += sign * int(t, 0); continue
        raise Unsupported('address ' + e)
    disp &= 0xffffffff
    if disp or not parts: parts.append('0x%xu' % disp)
    return 'u32(' + '+'.join(parts) + ')'

# ---------------------------------------------------------------- functions
class Listing:
    def __init__(self, ins):
        self.ins = ins; self.addrs = [i.a for i in ins]; self.by = {i.a: i for i in ins}
        self.entries = set()
        for i in ins:
            if i.mn == 'call' and len(i.ops) == 1 and re.match(r'^0x[0-9a-f]+$', i.ops[0]):
                self.entries.add(int(i.ops[0], 16))
    def at(self, a):
        i = self.by.get(a)
        if i is None and (0x401000 <= a < 0x596000 or 0x98e000 <= a < 0x145a000):
            for j in objdump_range(a, a + 0x400):
                if j.a not in self.by: self.by[j.a] = j
            i = self.by.get(a)
        return i
JCC = {'jo': 'f.of', 'jno': '!f.of', 'jb': 'f.cf', 'jae': '!f.cf', 'je': 'f.zf', 'jne': '!f.zf',
       'jbe': '(f.cf||f.zf)', 'ja': '(!f.cf&&!f.zf)', 'js': 'f.sf', 'jns': '!f.sf', 'jp': 'f.pf', 'jnp': '!f.pf',
       'jl': '(f.sf!=f.of)', 'jge': '(f.sf==f.of)', 'jle': '(f.zf||f.sf!=f.of)', 'jg': '(!f.zf&&f.sf==f.of)'}
SETCC = {'set' + k[1:]: v for k, v in JCC.items()}
SSE = {'movss', 'movaps', 'movups', 'xorps', 'andps', 'orps', 'andnps', 'addss', 'subss', 'mulss', 'divss', 'minss',
       'maxss', 'sqrtss', 'comiss', 'ucomiss', 'cvtsi2ss', 'cvttss2si', 'cvtss2si'}
CODE_LO, CODE_HI = 0x401000, 0x596000
# Protection gateways (tools/x86tr/gateways.txt): a jmp/call into the
# protection that saves every register, runs its noise, executes the game
# instruction it took out of .text (a push, a register load), and continues at
# a fixed .text address. Each entry is that measured effect.
GW = {}
def load_gateways():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'gateways.txt')
    if not os.path.exists(path): return
    for l in open(path):
        if l.startswith('#') or not l.strip(): continue
        p = l.split()
        eff = dict(e.split('=') for e in p[3:])
        GW[int(p[0], 16)] = (int(p[1], 16), int(p[2]), {k: int(v, 16) for k, v in eff.items()})
# The base is the unpacked Steam build (same build, the protected functions in
# plain code): no gateway is substituted. OR2_TR_GATEWAYS=1 restores the old
# mode for a listing of the protected OR2006C2C.EXE (OR2_TR_PLAIN is accepted
# and has no effect).
if os.environ.get('OR2_TR_GATEWAYS'): load_gateways()
def fold_step(k, i):
    """Constant folding over one instruction for the protection's computed
    addresses (mov r,[A] ; op r,[B] ; ... ; xchg [esp],r). k maps a register
    (or 'cf') to a known 32-bit value; anything not understood forgets the
    destination. Returns a computed code address stored to [esp+n], or None."""
    mn, ops = i.mn, i.ops
    def val(o):
        if o in R32: return k.get(o)
        m = re.match(r'^(?:DWORD PTR )?ds:(0x[0-9a-f]+)$', o)
        if m: return exe_u32(int(m.group(1), 16))
        if re.match(r'^(0x[0-9a-f]+)$', o): return int(o, 16)
        return None
    if mn == 'clc': k['cf'] = 0; return None
    if mn == 'stc': k['cf'] = 1; return None
    if mn == 'xchg' and len(ops) == 2 and re.match(r'^DWORD PTR \[esp(\+0x[0-9a-f]+)?\]$', ops[0]) and ops[1] in R32:
        v = k.get(ops[1]); k.pop(ops[1], None)
        return v if v is not None and CODE_LO <= v < CODE_HI else None
    if not ops or ops[0] not in R32:
        if mn in ('call',): k.clear()
        return None
    d = ops[0]; a = k.get(d); b = val(ops[1]) if len(ops) > 1 else None
    M = 0xffffffff; r = None
    if mn == 'mov': r = b
    elif mn in ('add', 'sub', 'xor', 'and', 'or') and a is not None and b is not None:
        r = {'add': a + b, 'sub': a - b, 'xor': a ^ b, 'and': a & b, 'or': a | b}[mn] & M
        k['cf'] = int((a + b) > M) if mn == 'add' else int(a < b) if mn == 'sub' else 0
    elif mn in ('adc', 'sbb') and a is not None and b is not None and k.get('cf') is not None:
        c = k['cf']
        r = (a + b + c) & M if mn == 'adc' else (a - b - c) & M
        k['cf'] = int(a + b + c > M) if mn == 'adc' else int(a < b + c)
    elif mn in ('rol', 'ror') and a is not None and len(ops) == 2 and ops[1] == 'cl' and k.get('ecx') is not None:
        n = k['ecx'] & 31
        r = ((a << n) | (a >> (32 - n))) & M if mn == 'rol' else ((a >> n) | (a << (32 - n))) & M
    elif mn == 'not' and a is not None: r = (~a) & M
    elif mn == 'neg' and a is not None: r = (-a) & M
    if r is None: k.pop(d, None)
    else: k[d] = r
    return None
def discover(L, entry):
    """Instructions of the function at entry: everything reachable through
    jumps without entering another function's entry, plus the code addresses
    the protection computes and transfers to through the stack. Returns
    (body, labels, jump tables, computed targets)."""
    body = {}; labels = set(); tables = {}; computed = set(); work = [entry]
    while work:
        a = work.pop(); k = {}
        while True:
            if a in body: break
            i = L.at(a)
            if i is None: raise Unsupported('no instruction at %x' % a)
            body[a] = i
            if a in GW:                              # protection gateway: a jump to its measured target
                t = GW[a][0]
                if t != entry and t in L.entries: break
                labels.add(t)
                if t not in body: a = t; k = {}; continue
                break
            t = fold_step(k, i)
            if t is not None:
                computed.add(t)
                if t not in L.entries: labels.add(t); work.append(t)
            nxt = a + i.n
            mn = i.mn
            if mn in ('ret', 'int3', 'hlt', 'ud2'): break
            if mn in JCC or mn in ('jecxz', 'loop'):
                t = int(i.ops[0], 16); labels.add(t)
                if t != entry and t in L.entries: raise Unsupported('conditional tail jump %x' % t)
                work.append(t); a = nxt; labels.add(nxt); k = {}; continue
            if mn == 'jmp':
                op = i.ops[0]
                if re.match(r'^0x[0-9a-f]+$', op):
                    t = int(op, 16)
                    if t != entry and t in L.entries: break       # tail call
                    labels.add(t)
                    # keep folding along an unconditional jump (the computed
                    # sequences are split by jumps)
                    if t not in body: a = t; continue
                    break
                m = re.match(r'^DWORD PTR \[([a-z]+)\*4\+(0x[0-9a-f]+)\]$', op)
                if m:
                    tables[a] = (m.group(1), int(m.group(2), 16))
                    break
                break                                  # indirect tail jump (import thunk, vtable thunk)
            a = nxt
    return body, labels, tables, computed

class Fn:
    def __init__(self, L, entry, known):
        self.L = L; self.entry = entry; self.known = known
        self.body, self.labels, self.tables, self.computed = discover(L, entry)
        self.out = []; self.used_x87 = False
    def find_bound(self, order, k, reg, depth=0):
        """Highest index of a switch on `reg` before position k: the guard
        'cmp reg,N' (N an immediate or a register loaded with one), or an
        index byte table 'movzx reg,BYTE PTR [x+idx]' guarded on x."""
        for j in range(k - 1, max(k - 40, -1), -1):
            p = self.body[order[j]]
            if p.mn == 'cmp' and len(p.ops) == 2 and p.ops[0] == reg:
                if re.match(r'^0x[0-9a-f]+$', p.ops[1]): return int(p.ops[1], 16)
                if p.ops[1] in R32:
                    for j2 in range(j - 1, max(j - 40, -1), -1):
                        q = self.body[order[j2]]
                        if q.mn == 'mov' and len(q.ops) == 2 and q.ops[0] == p.ops[1] and re.match(r'^0x[0-9a-f]+$', q.ops[1]):
                            return int(q.ops[1], 16)
                return None
            m2 = re.match(r'^BYTE PTR \[([a-z]+)\+(0x[0-9a-f]+)\]$', p.ops[1]) if p.mn == 'movzx' and len(p.ops) == 2 and p.ops[0] == reg else None
            if m2 and depth == 0:
                nb = self.find_bound(order, j, m2.group(1), 1)
                if nb is None: return None
                idx = int(m2.group(2), 16)
                return max(exe_u8(idx + n) for n in range(nb + 1))
            if p.ops and p.ops[0] == reg and p.mn not in ('cmp', 'test', 'push'): return None   # reg rewritten
        return None
    def emit(self):
        L = self.L
        order = sorted(self.body)
        # Jump tables: the bound is the 'cmp reg,N ; ja' guard before the jmp.
        pending = list(self.tables)
        while pending:
            a = pending.pop()
            reg, tab = self.tables[a][:2]
            order = sorted(self.body); k = order.index(a); bound = None
            bound = self.find_bound(order, k, reg)
            if bound is None: raise Unsupported('jump table without bound at %x' % a)
            targets = [exe_u32(tab + 4 * n) for n in range(bound + 1)]
            self.tables[a] = (reg, tab, targets)
            for t in targets:
                self.labels.add(t)
                if t not in self.body:
                    b2, l2, t2, c2 = discover(L, t)
                    for x in b2:
                        self.body.setdefault(x, b2[x])
                    self.labels |= l2; self.computed |= c2
                    for x in t2:
                        if x not in self.tables: self.tables[x] = t2[x]; pending.append(x)
        order = sorted(self.body)
        lines = []
        for a in order:
            i = self.body[a]
            if a in self.labels or a == self.entry: lines.append('L_%x:' % a)
            try:
                code = self.ins(i)
            except Unsupported as e:
                raise Unsupported('%x %s: %s' % (a, i.text, e))
            lines.append('    {' + code + '}  // %x %s' % (a, i.text))
            # fall-through into an address outside the body cannot happen: every
            # path ends in ret/jmp; a fall-through that leaves the body is a
            # call to a no-return function followed by padding.
            if a + i.n not in self.body and i.mn not in ('ret', 'jmp', 'int3', 'hlt', 'ud2'):
                lines.append('    c.trap(c,0x%xu,"fell out of the function");' % (a + i.n))
        head = ['void F_%x(Cpu& c){' % self.entry,
                '    u32 eax=c.eax,ecx=c.ecx,edx=c.edx,ebx=c.ebx,esp=c.esp,ebp=c.ebp,esi=c.esi,edi=c.edi;Fl f{};',
                '    (void)f;const u32 esp0=esp,ret0=ld32(c,esp);(void)ret0;']
        if self.used_x87: head.append('    Fpu& x=c.fpu;')
        head.append('    goto L_%x;                // the body is in address order; execution starts at the entry' % self.entry)
        return '\n'.join(head + lines + ['}'])
    # ---- operand access
    def rd(self, o, w=None):
        w = w or o.w
        if o.kind == 'r':
            return o.reg if w == 32 else '(%s&0x%xu)' % (o.reg, (1 << w) - 1)
        if o.kind == 'rh': return '((%s>>8)&0xffu)' % o.reg
        if o.kind == 'i': return '0x%xu' % (o.imm & ((1 << w) - 1) if w and w < 32 else o.imm)
        if o.kind == 'm':
            return 'ld%d(c,%s)' % (w, o.addr)
        raise Unsupported('read ' + o.kind)
    def wr(self, o, v, w=None):
        w = w or o.w
        if o.kind == 'r':
            if w == 32: return '%s=%s;' % (o.reg, v)
            m = (1 << w) - 1
            return '%s=(%s&0x%xu)|((%s)&0x%xu);' % (o.reg, o.reg, 0xffffffff ^ m, v, m)
        if o.kind == 'rh': return '%s=(%s&0xffff00ffu)|(((%s)&0xffu)<<8);' % (o.reg, o.reg, v)
        if o.kind == 'm': return 'st%d(c,%s,%s);' % (w, o.addr, v)
        raise Unsupported('write ' + o.kind)
    def ops(self, i):
        os_ = [parse_op(s) for s in i.ops]
        w = max([o.w for o in os_ if o.kind in ('r', 'rh', 'm')] + [0])
        for o in os_:
            if o.kind == 'm' and not o.w: o.w = w
        return os_, w
    def spill(self):
        return 'c.eax=eax;c.ecx=ecx;c.edx=edx;c.ebx=ebx;c.esp=esp;c.ebp=ebp;c.esi=esi;c.edi=edi;'
    def reload(self):
        return 'eax=c.eax;ecx=c.ecx;edx=c.edx;ebx=c.ebx;esp=c.esp;ebp=c.ebp;esi=c.esi;edi=c.edi;'
    def call_to(self, target_expr, ret, direct=None):
        if direct is None:
            # the operand is read before the return address is pushed ([esp+n] targets)
            return ('{const u32 t=%s;esp-=4;st32(c,esp,0x%xu);' % (target_expr, ret) + self.spill() +
                    'if(Fn g=c.lookup(t))g(c);else c.external(c,t);}' + self.reload())
        s = 'esp-=4;st32(c,esp,0x%xu);' % ret + self.spill()
        if direct in self.known:
            s += 'F_%x(c);' % direct
        else:
            s += 'c.external(c,0x%xu);' % direct
        return s + self.reload()
    def ins(self, i):
        mn = i.mn; a = i.a; nxt = a + i.n
        if mn == 'nop' or mn == 'xchg' and i.ops in (['ax', 'ax'], ['eax', 'eax']): return ''
        if mn.startswith('f') or mn in ('wait', 'fwait'):
            self.used_x87 = True
            return self.x87(i)
        if a in GW:
            t, delta, eff = GW[a]
            s = ''
            for r in R32:
                if r in eff: s += '%s=0x%xu;' % (r, eff[r])
            push = eff.get('push')
            if push is not None and push != 0xcccccccc:
                if delta != -4: raise Unsupported('gateway push with esp delta %d' % delta)
                s += 'esp-=4;st32(c,esp,0x%xu);' % push
            elif delta:
                s += 'esp=u32(esp+%d);' % delta
            if t in self.body: return s + 'goto L_%x;' % t
            s += self.spill() + (('F_%x(c);' % t) if t in self.known else 'c.external(c,0x%xu);' % t)
            return s + 'return;'
        if mn in ('jmp',):
            op = i.ops[0]
            if re.match(r'^0x[0-9a-f]+$', op):
                t = int(op, 16)
                if t in self.body: return 'goto L_%x;' % t
                # tail call: the callee returns to our caller
                s = self.spill()
                s += ('F_%x(c);' % t) if t in self.known else 'c.external(c,0x%xu);' % t
                return s + 'return;'
            if a in self.tables:
                reg, tab, targets = self.tables[a]
                cases = ''.join('case %d:goto L_%x;' % (n, t) for n, t in enumerate(targets))
                return 'switch(%s){%sdefault:c.trap(c,0x%xu,"jump table index");}' % (reg, cases, a)
            # indirect tail jump: the target returns to our caller
            o = parse_op(op)
            if o.kind == 'm' and not o.w: o.w = 32
            return ('{const u32 t=%s;' % self.rd(o, 32)) + self.spill() + 'if(Fn g=c.lookup(t))g(c);else c.external(c,t);return;}'
        if mn in JCC:
            return 'if(%s)goto L_%x;' % (JCC[mn], int(i.ops[0], 16))
        if mn == 'jecxz':
            return 'if(ecx==0)goto L_%x;' % int(i.ops[0], 16)
        if mn == 'ret':
            n = int(i.ops[0], 0) if i.ops else 0
            # A real return pops the caller's address at the entry depth; any
            # other `ret` is a computed jump of the protection.
            # (helpers like the CRT's __SEH_prolog return to the caller with a different ESP)
            s = '{const u32 t=ld32(c,esp);esp+=%du;if(t==ret0){%sreturn;}' % (4 + n, self.spill())
            s += 'switch(t){'
            for t in sorted(self.computed):
                if t in self.body: s += 'case 0x%xu:goto L_%x;' % (t, t)
                else: s += 'case 0x%xu:esp-=4;st32(c,esp,t);%s%sreturn;' % (t, self.spill(), ('F_%x(c);' % t) if t in self.known else 'c.external(c,0x%xu);' % t)
            return s + 'default:c.trap(c,0x%xu,"ret to an unknown address");}}' % a
        if mn == 'call':
            op = i.ops[0]
            if re.match(r'^0x[0-9a-f]+$', op):
                return self.call_to(None, nxt, int(op, 16))
            o = parse_op(op)
            if o.kind == 'm' and not o.w: o.w = 32
            return self.call_to(self.rd(o, 32), nxt)
        if mn in ('int3', 'hlt', 'ud2'):
            return 'c.trap(c,0x%xu,"%s");' % (a, mn)
        if mn in SSE:
            return self.sse(i)
        if i.pfx and any(p in ('rep', 'repz', 'repe', 'repnz', 'repne') for p in i.pfx) or mn in ('stos', 'movs', 'lods', 'scas', 'cmps'):
            return self.string(i)
        os_, w = self.ops(i)
        if w == 64 and mn not in ('movq', 'movlps', 'movhps'): raise Unsupported('64-bit integer operand')
        if mn == 'mov':
            return self.wr(os_[0], self.rd(os_[1], w))
        if mn in ('movzx', 'movsx'):
            sw = os_[1].w; dw = os_[0].w
            v = self.rd(os_[1], sw)
            if mn == 'movsx': v = 'u32(sx<%d>(%s))' % (sw, v)
            return self.wr(os_[0], v, dw)
        if mn == 'lea':
            return self.wr(os_[0], os_[1].addr, os_[0].w)
        BIN = {'add': 'add', 'sub': 'sub', 'cmp': 'sub', 'adc': 'add', 'sbb': 'sub'}
        if mn in BIN:
            carry = ',f.cf' if mn in ('adc', 'sbb') else ''
            e = '%s<%d>(f,%s,%s%s)' % (BIN[mn], w, self.rd(os_[0], w), self.rd(os_[1], w), carry)
            if mn == 'cmp': return '(void)%s;' % e
            return '{const u32 r=%s;%s}' % (e, self.wr(os_[0], 'r', w))
        LOG = {'and': '&', 'or': '|', 'xor': '^', 'test': '&'}
        if mn in LOG:
            if mn == 'xor' and i.ops[0] == i.ops[1] and os_[0].kind == 'r':
                return '%sf.cf=f.of=false;f.zf=true;f.sf=false;f.pf=true;' % self.wr(os_[0], '0u', w)
            e = 'logic<%d>(f,%s%s%s)' % (w, self.rd(os_[0], w), LOG[mn], self.rd(os_[1], w))
            if mn == 'test': return '(void)%s;' % e
            return '{const u32 r=%s;%s}' % (e, self.wr(os_[0], 'r', w))
        if mn in ('inc', 'dec', 'neg'):
            return '{const u32 r=%s<%d>(f,%s);%s}' % (mn, w, self.rd(os_[0], w), self.wr(os_[0], 'r', w))
        if mn == 'not':
            return self.wr(os_[0], '~%s' % self.rd(os_[0], w), w)
        if mn in ('shl', 'sal', 'shr', 'sar', 'rol', 'ror'):
            fn = 'shl' if mn == 'sal' else mn
            cnt = self.rd(os_[1], 8) if len(os_) > 1 else '1u'
            return '{const u32 r=%s<%d>(f,%s,%s);%s}' % (fn, w, self.rd(os_[0], w), cnt, self.wr(os_[0], 'r', w))
        if mn in ('shld', 'shrd'):
            if w != 32: raise Unsupported('16-bit ' + mn)
            return '{const u32 r=%s(f,%s,%s,%s);%s}' % (mn, self.rd(os_[0]), self.rd(os_[1], 32), self.rd(os_[2], 8), self.wr(os_[0], 'r', 32))
        if mn == 'imul':
            if len(os_) == 1 and w == 8:
                return '{const i32 p=i32(i8(eax))*i32(i8(%s));eax=(eax&0xffff0000u)|(u32(p)&0xffffu);f.cf=f.of=p!=i32(i8(p));}' % self.rd(os_[0], 8)
            if len(os_) == 1:
                if w != 32: raise Unsupported('imul ' + str(w))
                return ('{const i64 p=i64(i32(eax))*i64(i32(%s));eax=u32(p);edx=u32(u64(p)>>32);f.cf=f.of=p!=i64(i32(eax));}'
                        % self.rd(os_[0]))
            src = os_[1] if len(os_) == 2 else os_[1]
            b = self.rd(os_[2], w) if len(os_) == 3 else self.rd(os_[0], w)
            return '{const u32 r=imul2<%d>(f,%s,%s);%s}' % (w, self.rd(src, w), b, self.wr(os_[0], 'r', w))
        if mn == 'mul':
            if w != 32: raise Unsupported('mul ' + str(w))
            return '{const u64 p=u64(eax)*u64(%s);eax=u32(p);edx=u32(p>>32);f.cf=f.of=edx!=0;}' % self.rd(os_[0])
        if mn in ('div', 'idiv'):
            if w != 32: raise Unsupported(mn + ' ' + str(w))
            if mn == 'div':
                return ('{const u32 d=%s;const u64 n=(u64(edx)<<32)|eax;if(!d||n/d>0xffffffffull)c.trap(c,0x%xu,"#DE");'
                        'eax=u32(n/d);edx=u32(n%%d);}' % (self.rd(os_[0]), a))
            return ('{const i64 d=i32(%s);const i64 n=i64((u64(edx)<<32)|eax);if(!d||(n==INT64_MIN&&d==-1))c.trap(c,0x%xu,"#DE");'
                    'const i64 q=n/d;if(q>0x7fffffffll||q<-0x80000000ll)c.trap(c,0x%xu,"#DE");eax=u32(q);edx=u32(n%%d);}' % (self.rd(os_[0]), a, a))
        if mn == 'cdq': return 'edx=u32(i32(eax)>>31);'
        if mn == 'cwde': return 'eax=u32(i32(i16(eax)));'
        if mn == 'cbw': return 'eax=(eax&0xffff0000u)|(u32(i16(i8(eax)))&0xffffu);'
        if mn == 'push':
            o = os_[0]
            if o.kind == 'i':
                return 'esp-=4;st32(c,esp,0x%xu);' % o.imm
            if o.w != 32: raise Unsupported('push %d' % o.w)
            return '{const u32 v=%s;esp-=4;st32(c,esp,v);}' % self.rd(o)
        if mn == 'pop':
            o = os_[0]
            if o.w != 32: raise Unsupported('pop %d' % o.w)
            if o.kind == 'r': return '%s=ld32(c,esp);%s' % (o.reg, '' if o.reg == 'esp' else 'esp+=4;')
            return '{const u32 v=ld32(c,esp);esp+=4;%s}' % self.wr(o, 'v')
        if mn == 'leave': return 'esp=ebp;ebp=ld32(c,esp);esp+=4;'
        if mn == 'pushf':
            return 'esp-=4;st32(c,esp,(f.cf?1u:0u)|2u|(f.pf?4u:0u)|(f.zf?0x40u:0u)|(f.sf?0x80u:0u)|0x200u|(f.of?0x800u:0u));'
        if mn == 'popf':
            return '{const u32 v=ld32(c,esp);esp+=4;f.cf=v&1u;f.pf=v&4u;f.zf=v&0x40u;f.sf=v&0x80u;f.of=v&0x800u;}'
        if mn == 'clc': return 'f.cf=false;'
        if mn == 'stc': return 'f.cf=true;'
        if mn == 'cmc': return 'f.cf=!f.cf;'
        if mn in SETCC:
            return self.wr(os_[0], '(%s)?1u:0u' % SETCC[mn], 8)
        if mn == 'xchg':
            x, y = os_
            return '{const u32 t=%s;%s%s}' % (self.rd(x, w), self.wr(x, self.rd(y, w), w), self.wr(y, 't', w))
        if mn == 'bswap': return '%s=__builtin_bswap32(%s);' % (os_[0].reg, os_[0].reg)
        if mn == 'sahf':
            return 'f.cf=eax&0x100u;f.pf=eax&0x400u;f.zf=eax&0x4000u;f.sf=eax&0x8000u;'
        if mn == 'cld': return ''
        if mn == 'lahf':
            return 'eax=(eax&0xffff00ffu)|(((f.sf?0x80u:0u)|(f.zf?0x40u:0u)|(f.pf?4u:0u)|2u|(f.cf?1u:0u))<<8);'
        if mn.startswith('cmov') and ('j' + mn[4:]) in JCC:
            return 'if(%s){%s}' % (JCC['j' + mn[4:]], self.wr(os_[0], self.rd(os_[1], w), w))
        if mn in ('rcr', 'rcl'):
            if w != 32: raise Unsupported(mn + ' %d' % w)
            cnt = self.rd(os_[1], 8) if len(os_) > 1 else '1u'
            return '{const u32 r=%s(f,%s,%s);%s}' % (mn, self.rd(os_[0]), cnt, self.wr(os_[0], 'r', 32))
        if mn == 'pusha':
            return '{const u32 s=esp;const u32 v[8]{eax,ecx,edx,ebx,s,ebp,esi,edi};for(int k=0;k<8;++k){esp-=4;st32(c,esp,v[k]);}}'
        if mn == 'popa':
            return 'edi=ld32(c,esp);esi=ld32(c,esp+4);ebp=ld32(c,esp+8);ebx=ld32(c,esp+16);edx=ld32(c,esp+20);ecx=ld32(c,esp+24);eax=ld32(c,esp+28);esp+=32;'
        if mn == 'stmxcsr': return 'st32(c,%s,c.mxcsr);' % os_[0].addr
        if mn == 'ldmxcsr': return 'c.mxcsr=ld32(c,%s);' % os_[0].addr
        if mn == 'std': raise Unsupported('direction flag set')
        if mn == 'cpuid':
            # A Pentium 4 without SSE2 reporting (the CRT then keeps its x87 paths)
            return ('if(eax==0){eax=1;ebx=0x756e6547u;edx=0x49656e69u;ecx=0x6c65746eu;}'
                    'else{eax=0xf29u;ebx=0x00010800u;ecx=0;edx=0x0383fbffu;}')
        if mn == 'bt':
            return 'f.cf=(%s>>(%s&%du))&1u;' % (self.rd(os_[0], w), self.rd(os_[1], w), w - 1)
        raise Unsupported('instruction ' + mn)
    def sse(self, i):
        mn = i.mn
        os_ = [parse_op(s) for s in i.ops]
        def lane(o):                       # scalar 32-bit source
            if o.kind == 'x': return 'c.xmm[%d][0]' % o.imm
            if o.kind == 'm': return 'ld32(c,%s)' % o.addr
            if o.kind == 'r' and o.w == 32: return o.reg
            raise Unsupported('sse operand')
        def vec(o):                        # 128-bit source as 4 lanes
            if o.kind == 'x': return ['c.xmm[%d][%d]' % (o.imm, k) for k in range(4)]
            if o.kind == 'm': return ['ld32(c,%s+%du)' % (o.addr, 4 * k) for k in range(4)]
            raise Unsupported('sse vector operand')
        d = os_[0]
        if mn == 'movss':
            s = os_[1]
            if d.kind == 'x' and s.kind == 'm':
                return 'c.xmm[%d][0]=ld32(c,%s);c.xmm[%d][1]=c.xmm[%d][2]=c.xmm[%d][3]=0;' % (d.imm, s.addr, d.imm, d.imm, d.imm)
            if d.kind == 'x' and s.kind == 'x': return 'c.xmm[%d][0]=c.xmm[%d][0];' % (d.imm, s.imm)
            if d.kind == 'm' and s.kind == 'x': return 'st32(c,%s,c.xmm[%d][0]);' % (d.addr, s.imm)
        if mn in ('movaps', 'movups'):
            s = os_[1]
            if d.kind == 'x':
                v = vec(s)
                return '{const u32 t0=%s,t1=%s,t2=%s,t3=%s;c.xmm[%d][0]=t0;c.xmm[%d][1]=t1;c.xmm[%d][2]=t2;c.xmm[%d][3]=t3;}' % tuple(v + [d.imm] * 4)
            if d.kind == 'm' and s.kind == 'x':
                return ''.join('st32(c,%s+%du,c.xmm[%d][%d]);' % (d.addr, 4 * k, s.imm, k) for k in range(4))
        if mn in ('xorps', 'andps', 'orps', 'andnps'):
            if d.kind != 'x': raise Unsupported('sse logic dest')
            v = vec(os_[1]); op = {'xorps': '^', 'andps': '&', 'orps': '|', 'andnps': '&'}[mn]
            n = '~' if mn == 'andnps' else ''
            return '{const u32 t[4]={%s};for(int k=0;k<4;++k)c.xmm[%d][k]=(%sc.xmm[%d][k])%st[k];}' % (','.join(v), d.imm, n, d.imm, op)
        AR = {'addss': 'sse_add', 'subss': 'sse_sub', 'mulss': 'sse_mul', 'divss': 'sse_div', 'minss': 'sse_min', 'maxss': 'sse_max'}
        if mn in AR:
            return 'c.xmm[%d][0]=%s(c.xmm[%d][0],%s);' % (d.imm, AR[mn], d.imm, lane(os_[1]))
        if mn == 'sqrtss': return 'c.xmm[%d][0]=sse_sqrt(%s);' % (d.imm, lane(os_[1]))
        if mn in ('comiss', 'ucomiss'): return 'sse_comi(f,c.xmm[%d][0],%s);' % (d.imm, lane(os_[1]))
        if mn == 'cvtsi2ss': return 'c.xmm[%d][0]=sse_cvtsi2ss(i32(%s));' % (d.imm, lane(os_[1]))
        if mn in ('cvttss2si', 'cvtss2si'):
            return '%s=%s(%s);' % (d.reg, 'sse_' + mn, lane(os_[1]))
        raise Unsupported('sse ' + mn)
    def string(self, i):
        mn = i.mn; rep = [p for p in i.pfx if p.startswith('rep')]
        os_ = [parse_op(s) for s in i.ops]
        w = max(o.w for o in os_)
        n = w // 8
        if mn == 'stos': step = 'st%d(c,edi,%s);edi+=%du;' % (w, 'eax' if w == 32 else '(eax&0x%xu)' % ((1 << w) - 1), n)
        elif mn == 'movs': step = 'st%d(c,edi,ld%d(c,esi));edi+=%du;esi+=%du;' % (w, w, n, n)
        elif mn == 'lods': step = '%s;esi+=%du;' % ('eax=ld32(c,esi)' if w == 32 else 'eax=(eax&0x%xu)|ld%d(c,esi)' % (0xffffffff ^ ((1 << w) - 1), w), n)
        elif mn == 'scas': step = '(void)sub<%d>(f,eax,ld%d(c,edi));edi+=%du;' % (w, w, n)
        elif mn == 'cmps': step = '(void)sub<%d>(f,ld%d(c,esi),ld%d(c,edi));esi+=%du;edi+=%du;' % (w, w, w, n, n)
        else: raise Unsupported('string ' + mn)
        if not rep: return step
        r = rep[0]
        if mn in ('stos', 'movs', 'lods') or r == 'rep':
            return 'while(ecx){%secx--;}' % step
        cond = '!f.zf' if r in ('repz', 'repe') else 'f.zf'
        return 'while(ecx){%secx--;if(%s)break;}' % (step, cond)
    # ---- x87 (fpu.hpp)
    def x87(self, i):
        mn = i.mn
        os_ = []
        for s in i.ops:
            o = parse_op(s)
            os_.append(o)
        def mem(o):
            if o.kind != 'm': raise Unsupported('x87 memory operand')
            return o
        def sti(o): return o.imm
        a = i.a
        # loads
        if mn == 'fld':
            o = os_[0]
            if o.kind == 'st': return 'x.push(x.st(%d));' % o.imm
            if o.w == 32: return 'x.push(x87_from_f32(ld32(c,%s)));' % o.addr
            if o.w == 64: return 'x.push(x87_from_f64(ld64(c,%s)));' % o.addr
            if o.w == 80: return 'x.push(x87_from_f80(c,%s));' % o.addr
        if mn == 'fild':
            o = os_[0]
            if o.w == 16: return 'x.push(xv(i16(ld16(c,%s))));' % o.addr
            if o.w == 32: return 'x.push(xv(i32(ld32(c,%s))));' % o.addr
            if o.w == 64: return 'x.push(x87_from_i64(i64(ld64(c,%s))));' % o.addr
        if mn == 'fldz': return 'x.push(0);'
        if mn in ('fldpi', 'fldl2e', 'fldl2t', 'fldlg2', 'fldln2'): return 'x.push(fpu_const_%s());' % mn[3:]
        if mn in ('fprem', 'fprem1'): return 'x.prem(%s);' % ('true' if mn == 'fprem1' else 'false')
        if mn == 'fxam': return 'x.examine();'
        if mn == 'f2xm1': return 'x.st(0)=fpu_f2xm1(x.st(0));'
        if mn == 'fyl2x': return '{const auto r=fpu_fyl2x(x.st(0),x.st(1));x.pop();x.st(0)=r;}'
        if mn == 'fld1': return 'x.push(1);'
        if mn in ('fst', 'fstp'):
            o = os_[0]; pop = 'x.pop();' if mn == 'fstp' else ''
            if o.kind == 'st': return 'x.st(%d)=x.st(0);%s' % (o.imm, pop)
            if o.w == 32: return 'st32(c,%s,x87_to_f32(x.st(0)));%s' % (o.addr, pop)
            if o.w == 64: return 'st64(c,%s,x87_to_f64(x.st(0)));%s' % (o.addr, pop)
            if o.w == 80: return 'x87_to_f80(c,%s,x.st(0));%s' % (o.addr, pop)
        if mn in ('fist', 'fistp'):
            o = os_[0]; pop = 'x.pop();' if mn == 'fistp' else ''
            if o.w == 16: return 'st16(c,%s,u32(x87_to_int<i16>(x,x.st(0))));%s' % (o.addr, pop)
            if o.w == 32: return 'st32(c,%s,u32(x87_to_int<i32>(x,x.st(0))));%s' % (o.addr, pop)
            if o.w == 64: return 'st64(c,%s,u64(x87_to_int<i64>(x,x.st(0))));%s' % (o.addr, pop)
        if mn == 'fxch':
            k = os_[0].imm if os_ else 1
            return 'std::swap(x.st(0),x.st(%d));' % k
        if mn in ('fchs',): return 'x.st(0)=-x.st(0);'
        if mn in ('fabs',): return 'x.st(0)=std::fabs(x.st(0));'
        if mn == 'fsqrt': return 'x.st(0)=x87_sqrt_op(x.st(0));'
        if mn in ('fsin', 'fcos'): return 'x.st(0)=fpu_%s(x.st(0));' % mn[1:]
        if mn == 'fsincos': return '{const auto v=x.st(0);x.st(0)=fpu_sin(v);x.push(fpu_cos(v));}'
        if mn == 'fptan': return 'x.st(0)=fpu_tan(x.st(0));x.push(1);'
        if mn == 'fpatan': return '{const auto r=fpu_atan2(x.st(1),x.st(0));x.pop();x.st(0)=r;}'
        if mn == 'frndint': return 'x.st(0)=x87_frndint(x,x.st(0));'
        if mn == 'fscale': return 'x.st(0)=x87_fscale(x.st(0),x.st(1));'
        if mn in ('fnstcw', 'fstcw'): return 'st16(c,%s,x.cw);' % mem(os_[0]).addr
        if mn == 'fldcw': return 'x.load_cw(ld16(c,%s));' % mem(os_[0]).addr
        if mn in ('fnstsw', 'fstsw'):
            o = os_[0]
            if o.kind == 'r': return 'eax=(eax&0xffff0000u)|x.status();c.fsw_al=true;'
            return 'st16(c,%s,x.status());' % o.addr
        if mn in ('fwait', 'wait', 'fnclex', 'fclex'): return ''
        if mn == 'ffree': return ''
        if mn == 'fninit': return 'x.init();'
        # arithmetic
        AR = {'fadd': 'x87_add(%s,%s)', 'fmul': 'x87_mul(%s,%s)', 'fsub': 'x87_sub(%s,%s)', 'fsubr': 'x87_sub(%s,%s)',
              'fdiv': 'x87_div(%s,%s)', 'fdivr': 'x87_div(%s,%s)'}
        base = mn[:-1] if mn.endswith('p') and mn[:-1] in AR else mn
        if mn.startswith('fi') and mn[0:2] == 'fi' and ('f' + mn[2:]) in AR:
            base = 'f' + mn[2:]
            o = os_[0]
            v = 'xv(i16(ld16(c,%s)))' % o.addr if o.w == 16 else 'xv(i32(ld32(c,%s)))' % o.addr
            l, r = ('x.st(0)', v) if not base.endswith('r') else (v, 'x.st(0)')
            return 'x.st(0)=%s;' % (AR[base] % (l, r))
        if base in AR:
            pop = mn.endswith('p')
            rev = base.endswith('r')
            if not os_:                             # faddp with no operand = st(1),st
                os_ = [parse_op('st(1)'), parse_op('st')]
            if len(os_) == 1:
                o = os_[0]
                if o.kind == 'm':
                    v = 'x87_from_f32(ld32(c,%s))' % o.addr if o.w == 32 else 'x87_from_f64(ld64(c,%s))' % o.addr
                    l, r = ('x.st(0)', v) if not rev else (v, 'x.st(0)')
                    return 'x.st(0)=%s;' % (AR[base] % (l, r))
                os_ = [parse_op('st'), o]
            # Register forms are decoded from the bytes (objdump's naming of
            # the DC/DE subtract and divide forms is not reliable).
            op0, modrm = i.b[0], i.b[1]
            if modrm < 0xc0 or op0 not in (0xd8, 0xdc, 0xde): raise Unsupported('x87 register form')
            k = modrm & 7; reg = (modrm >> 3) & 7
            FN = {0: 'x87_add', 1: 'x87_mul', 4: 'x87_sub', 5: 'x87_sub', 6: 'x87_div', 7: 'x87_div'}
            if reg not in FN: raise Unsupported('x87 register form reg')
            if op0 == 0xd8:                           # st0 = st0 op st(k); /5 /7 reversed
                l, r = ('x.st(0)', 'x.st(%d)' % k) if reg in (0, 1, 4, 6) else ('x.st(%d)' % k, 'x.st(0)')
                return 'x.st(0)=%s(%s,%s);' % (FN[reg], l, r)
            # DC/DE: st(k) = st(k) op st0 for /0 /1 /5 /7; st0 op st(k) for /4 /6
            l, r = ('x.st(%d)' % k, 'x.st(0)') if reg in (0, 1, 5, 7) else ('x.st(0)', 'x.st(%d)' % k)
            return 'x.st(%d)=%s(%s,%s);%s' % (k, FN[reg], l, r, 'x.pop();' if op0 == 0xde else '')
        CMP = ('fcom', 'fcomp', 'fcompp', 'fucom', 'fucomp', 'fucompp', 'ficom', 'ficomp', 'ftst', 'fcomi', 'fcomip', 'fucomi', 'fucomip')
        if mn in CMP:
            if mn == 'ftst': return 'x.compare(x.st(0),0);'
            if mn.startswith('fi'):
                o = os_[0]
                v = 'xv(i16(ld16(c,%s)))' % o.addr if o.w == 16 else 'xv(i32(ld32(c,%s)))' % o.addr
            elif not os_: v = 'x.st(1)'
            else:
                o = os_[-1]
                if o.kind == 'st': v = 'x.st(%d)' % o.imm
                elif o.w == 32: v = 'x87_from_f32(ld32(c,%s))' % o.addr
                else: v = 'x87_from_f64(ld64(c,%s))' % o.addr
            if mn.endswith('i') or mn.endswith('ip'):
                s = 'x.compare_flags(f,x.st(0),%s);' % v
            else:
                s = 'x.compare(x.st(0),%s);' % v
            if mn.endswith('pp'): s += 'x.pop();x.pop();'
            elif mn.endswith('p'): s += 'x.pop();'
            return s
        raise Unsupported('x87 ' + mn)

def main():
    listing, out = sys.argv[1], sys.argv[2]
    want = []
    for x in sys.argv[3:]:
        if x.startswith('@'):
            want += [int(t, 16) for t in open(x[1:]).read().split()]
        else:
            want.append(int(x, 16))
    L = Listing(load_listing(listing))
    # Function boundaries are the call targets only: an entry known from a
    # table (vtable, callback) is translated as its own function, but code
    # that jumps to it keeps it as an inner label (a copy).
    # Translate to a fixed point: a function that fails is dropped, and its
    # callers then call it through c.external (still exact).
    # A failure does not depend on which other functions are translated, so
    # one pass finds them; the second pass emits with the final set.
    known = set(want); bodies = {}; failed = {}
    for a in sorted(want):
        try:
            Fn(L, a, known).emit()
        except Unsupported as e:
            failed[a] = str(e)
    known -= set(failed)
    for a in sorted(known):
        bodies[a] = Fn(L, a, known).emit()
    split = int(os.environ.get('X86TR_SPLIT', '0'))
    head = '// Generated by tools/x86tr/translate.py from OR2006C2C.EXE. Do not edit.\n'
    inc = '#include "x86rt.hpp"\n#include <utility>\n#include <cstdint>\n'
    decl = ''.join('void F_%x(Cpu& c);\n' % a for a in sorted(bodies))
    table = ('struct Entry{u32 pc;Fn fn;};\nextern const Entry translated_table[]={\n' +
             ''.join('    {0x%xu,F_%x},\n' % (a, a) for a in sorted(bodies)) + '    {0,nullptr}};\n')
    if not split:
        with open(out, 'w') as f:
            f.write(head + inc + 'namespace or2x86 {\n' + decl)
            for a in sorted(bodies): f.write(bodies[a] + '\n')
            f.write(table + '}\n')
    else:
        # out is a stem: <out>_decl.hpp, <out>_table.cpp, <out>_000.cpp ... (split files)
        with open(out + '_decl.hpp', 'w') as f:
            f.write(head + '#pragma once\n' + inc + 'namespace or2x86 {\n' + decl + '}\n')
        with open(out + '_table.cpp', 'w') as f:
            f.write(head + '#include "%s_decl.hpp"\nnamespace or2x86 {\n' % os.path.basename(out) + table + '}\n')
        keys = sorted(bodies); per = (len(keys) + split - 1) // split
        for n in range(split):
            with open(out + '_%03d.cpp' % n, 'w') as f:
                f.write(head + '#include "%s_decl.hpp"\nnamespace or2x86 {\n' % os.path.basename(out))
                for a in keys[n * per:(n + 1) * per]: f.write(bodies[a] + '\n')
                f.write('}\n')
    print('translated %d, failed %d' % (len(bodies), len(failed)))
    reasons = {}
    for a, r in failed.items():
        k = re.sub(r'^[0-9a-f]+ ', '', r)
        k = re.sub(r'0x[0-9a-f]+', 'N', k.split(': ', 1)[-1])
        reasons[k] = reasons.get(k, 0) + 1
    with open(out + '.failed.txt', 'w') as f:
        for a in sorted(failed): f.write('%x %s\n' % (a, failed[a]))
    for k, n in sorted(reasons.items(), key=lambda t: -t[1])[:40]:
        print('%6d %s' % (n, k))
if __name__ == '__main__':
    main()
