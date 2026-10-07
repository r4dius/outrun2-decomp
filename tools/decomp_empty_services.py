#!/usr/bin/env python3
"""Static audit of silent service holes in the ported code.

A ported function receives its outside world through service objects (structs
of callbacks / pointers named *Services, *Hooks, *Io ...). When a member is
null the function must not silently skip what the original did: it reports
(missing pc, fault, log) or falls back to the bulk translation. This audit
finds the three ways the rule is broken:

  silent-skip   the callee tests a service member and skips the call without
                any report: `if(s.f)s.f(...)`, `s.f?s.f(...):x`,
                `if(!s.f)return;` / `return true;` / `return 0;`
  empty-arg     a call site passes `{}` / `Type{}` for a service parameter
  bare-object   a service object declared without initialiser and passed on
                with none of its members assigned in between

Each hit has a stable key (file, function, kind, expression). Hits listed in
tools/decomp_empty_services_allow.txt (with the reason the skip is the
original's behaviour or an intended option) are accepted; --check fails on
any other hit, so a new silent hole cannot be added unnoticed.

usage: decomp_empty_services.py [--out work/decomp/empty-services.md] [--check]
"""
import argparse, collections, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src')
ALLOW = os.path.join(ROOT, 'tools', 'decomp_empty_services_allow.txt')
SKIP_FILE = re.compile(r'(_tr\.cpp|bulk_tr\.cpp|/tests?/)')
# Service-like aggregate types: what a ported function gets its callbacks through.
SERVICE_TYPE = re.compile(r'^\w*(Services|Service|Hooks|Hook|Callbacks|Io|Binding|Bindings)\w*$')

ID = r'[A-Za-z_]\w*'
OBJ = r'(?:' + ID + r'(?:\s*(?:\.|->)\s*' + ID + r')*)'


def strip_comments(t):
    t = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), t, flags=re.S)
    return re.sub(r'//[^\n]*', '', t)


def sources():
    for d, _, fs in os.walk(SRC):
        for f in fs:
            if f.endswith(('.cpp', '.hpp', '.inc', '.h')):
                p = os.path.join(d, f)
                rel = os.path.relpath(p, ROOT).replace('\\', '/')
                if SKIP_FILE.search(rel):
                    continue
                yield rel, strip_comments(open(p, encoding='utf-8', errors='replace').read())


def func_at(text, pos):
    """Name of the function whose body encloses pos (best effort)."""
    depth, i = 0, pos
    while i > 0:
        i -= 1
        c = text[i]
        if c == '}':
            depth += 1
        elif c == '{':
            if depth == 0:
                head = text[max(0, i - 300):i]
                m = re.search(r'(' + ID + r'(?:::' + ID + r')*)\s*\([^;{}]*\)\s*(?:const\s*)?(?:override\s*)?(?:->\s*[\w:<>]+\s*)?$', head)
                if m and m.group(1) not in ('if', 'for', 'while', 'switch', 'catch', 'return', 'sizeof'):
                    return m.group(1)
            else:
                depth -= 1
    return '?'


def struct_bodies(t):
    for m in re.finditer(r'\bstruct\s+(' + ID + r')\s*(?:final\s*)?(?::[^{;]*)?\{', t):
        depth, i = 0, m.end() - 1
        while i < len(t):
            if t[i] == '{':
                depth += 1
            elif t[i] == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        yield m.group(1), t[m.end():i]


R_FUNC_MEMBER = re.compile(r'std::function\s*<[^;]*>\s*(' + ID + r')\s*(?:\{[^;]*\})?\s*;|\(\s*\*\s*(' + ID + r')\s*\)\s*\([^;]*\)\s*(?:\{\s*\})?\s*;')
R_PTR_MEMBER = re.compile(r'(?:const\s+)?(?:[\w:]+::)?' + ID + r'(?:<[^;<>]*>)?\s*\*\s*(' + ID + r')\s*(?:\{\s*\}|=\s*nullptr)?\s*;')


R_FN_ALIAS = re.compile(r'\busing\s+(' + ID + r')\s*=\s*(?:std::function\s*<|[^;=]*\(\s*\*\s*\)\s*\()|\btypedef\s+[^;]*\(\s*\*\s*(' + ID + r')\s*\)\s*\(')


def service_types(files):
    """Service structs: named like one (Services, Hooks...) or holding callbacks.
    Returns (type names, member names that are callbacks or service pointers)."""
    names, members = set(), set()
    aliases = set()
    for _, t in files:
        aliases |= {a or b for a, b in R_FN_ALIAS.findall(t)}
    aliases.discard('')
    r_alias = re.compile(r'(?:[\w:]+::)?\b(' + '|'.join(sorted(aliases)) + r')\s+(' + ID + r')\s*(?:\{\s*\}|=\s*nullptr)?\s*;') if aliases else None
    for _, t in files:
        for name, body in struct_bodies(t):
            calls = {a or b for a, b in R_FUNC_MEMBER.findall(body)}
            if r_alias:
                calls |= {m.group(2) for m in r_alias.finditer(body)}
            if SERVICE_TYPE.match(name) or calls:
                names.add(name)
                members |= calls
                if SERVICE_TYPE.match(name):
                    members |= set(R_PTR_MEMBER.findall(body))
    members.discard('')
    return names, members


def split_args(s):
    out, depth, cur = [], 0, ''
    for c in s:
        if c in '([{<':
            depth += 1
        elif c in ')]}>':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def balanced(text, start):
    """text[start] == '(' -> index after the matching ')'."""
    depth = 0
    for i in range(start, min(len(text), start + 4000)):
        if text[i] == '(':
            depth += 1
        elif text[i] == ')':
            depth -= 1
            if depth == 0:
                return i + 1
    return -1


def service_params(files, types):
    """function name -> {param index: type} for parameters of service type."""
    out = collections.defaultdict(dict)
    r = re.compile(r'\b(' + ID + r')\s*\(')
    tnames = re.compile(r'\b(?:' + '|'.join(sorted(types)) + r')\s*[&*]')
    for rel, t in files:
        if not rel.endswith(('.hpp', '.h')):
            continue                              # the declarations: headers
        for m in r.finditer(t):
            if not tnames.search(t, m.end(), m.end() + 2000):
                continue
            name = m.group(1)
            if name in ('if', 'for', 'while', 'switch', 'return', 'sizeof', 'catch'):
                continue
            end = balanced(t, m.end() - 1)
            if end < 0:
                continue
            args = split_args(t[m.end():end - 1])
            for i, a in enumerate(args):
                pm = re.match(r'^(?:const\s+)?(?:[\w:]+::)?(' + ID + r')\s*[&*]\s*' + ID + r'?\s*(?:=.*)?$', a)
                if pm and pm.group(1) in types:
                    out[name][i] = pm.group(1)
    return out


def scan(files, types, members, params):
    hits = []
    def add(rel, t, pos, kind, expr):
        line = t.count('\n', 0, pos) + 1
        hits.append((rel, line, func_at(t, pos), kind, re.sub(r'\s+', '', expr)))
    silent = [
        re.compile(r'if\s*\(\s*(' + OBJ + r')\s*\)\s*\{?\s*(?:\(void\)\s*)?\1\s*\('),
        re.compile(r'if\s*\(\s*(' + OBJ + r')\s*!=\s*nullptr\s*\)\s*\{?\s*(?:\(void\)\s*)?\1\s*\('),
        re.compile(r'(' + OBJ + r')\s*\?\s*\1\s*\('),
        re.compile(r'if\s*\(\s*!\s*(' + OBJ + r')\s*\)\s*\{?\s*return\s*(?:true|0|0u|nullptr)?\s*;'),
    ]
    for rel, t in files:
        for r in silent:
            for m in r.finditer(t):
                obj = m.group(1)
                if '.' not in obj and '->' not in obj:
                    continue                      # a plain local / pointer test, not a service member
                if re.split(r'\.|->', obj.replace(' ', ''))[-1] not in members:
                    continue                      # a state flag, not a callback or service pointer
                # the guarded statement, and an else branch right after it
                j = t.find(';', m.end())
                b = m.group(0).find('{')
                if b >= 0:                        # if(X){...}: the block's closing brace
                    depth, j = 0, m.start() + b
                    while j < len(t):
                        depth += {'{': 1, '}': -1}.get(t[j], 0)
                        if depth == 0:
                            break
                        j += 1
                k = j + 1
                while 0 < k < len(t) and t[k] in ' \t\r\n}':
                    k += 1
                stmt = t[m.start():j + 1] + (t[k:t.find(';', k) + 1] if t.startswith('else', k) else '')
                if 'service_hole(' in stmt:
                    continue                      # reported: driving::service_hole() on the null branch
                add(rel, t, m.start(), 'silent-skip', obj)
        # empty-arg: calls of functions with service parameters given {} / Type{}
        for name, idx in params.items():
            if not re.search(r'_[0-9a-f]{6}$', name) and len(name) < 14:
                continue                          # short generic names collide with lambdas and locals
            for m in re.finditer(r'\b' + re.escape(name) + r'\s*\(', t):
                end = balanced(t, m.end() - 1)
                if end < 0:
                    continue
                args = split_args(t[m.end():end - 1])
                for i, ty in idx.items():
                    if i < len(args) and re.match(r'^(?:[\w:]*' + ty + r')?\s*\{\s*\}$|^(?:[\w:]*' + ty + r')\s*\(\s*\)$', args[i]):
                        add(rel, t, m.start(), 'empty-arg', name + '#' + str(i))
        # bare-object: `Type v;` / `Type v{};` passed on without member assignment
        for m in re.finditer(r'\b(?:[\w:]+::)?(' + ID + r')\s+(' + ID + r')\s*(?:\{\s*\})?\s*;', t):
            if m.group(1) not in types:
                continue
            v = m.group(2)
            rest = t[m.end():m.end() + 6000]
            # the enclosing scope ends at the first unmatched '}'
            depth, stop = 0, len(rest)
            for i, c in enumerate(rest):
                if c == '{':
                    depth += 1
                elif c == '}':
                    if depth == 0:
                        stop = i; break
                    depth -= 1
            body = rest[:stop]
            if re.search(r'\b' + v + r'\s*(?:\.|->)\s*\w+\s*(?:\[[^\]]*\])?\s*=[^=]', body):
                continue
            if re.search(r'\b' + v + r'\s*=\s*[^=]', body) or re.search(r'&\s*' + v + r'\b|\b' + v + r'\s*\)\s*;', body) is None:
                continue
            add(rel, t, m.start(), 'bare-object', m.group(1) + ' ' + v)
    return hits


def load_allow():
    allow = {}
    if os.path.exists(ALLOW):
        for line in open(ALLOW, encoding='utf-8'):
            line = line.rstrip('\n')
            if not line.strip() or line.lstrip().startswith('#'):
                continue
            key, _, why = line.partition(' # ')
            allow[key.strip()] = why.strip()
    return allow


def key(h):
    return '%s|%s|%s|%s' % (h[0], h[2], h[3], h[4])


def main():
    a = argparse.ArgumentParser()
    a.add_argument('--out', default=os.path.join(ROOT, 'work', 'decomp', 'empty-services.md'))
    a.add_argument('--check', action='store_true', help='exit 1 on any hit not in the allow list')
    a.add_argument('--list', action='store_true', help='print the keys of the unaccepted hits')
    o = a.parse_args()
    files = list(sources())
    types, members = service_types(files)
    params = service_params(files, types)
    hits = scan(files, types, members, params)
    allow = load_allow()
    new = [h for h in hits if key(h) not in allow]
    seen = {key(h) for h in hits}
    stale = [k for k in allow if k not in seen]
    by = collections.Counter(h[3] for h in new)
    # allow entries whose reason starts with "known hole": reported at run time, still to port
    known = [h for h in hits if allow.get(key(h), '').lower().startswith('known hole')]
    if o.out:
        os.makedirs(os.path.dirname(o.out), exist_ok=True)
        with open(o.out, 'w', encoding='utf-8') as f:
            f.write('# Silent service holes\n\n')
            f.write('Generated by tools/decomp_empty_services.py: %d service types, %d hits, %d accepted (allow list, %d of them known holes still to port), %d open.\n\n'
                    % (len(types), len(hits), len(hits) - len(new), len(known), len(new)))
            if known:
                f.write('## known holes (%d, reported by service_hole at run time)\n\n' % len(known))
                for h in sorted(known):
                    f.write('- `%s:%d` %s: `%s` (%s)\n' % (h[0], h[1], h[2], h[4], allow[key(h)]))
                f.write('\n')
            for kind in ('silent-skip', 'empty-arg', 'bare-object'):
                rows = [h for h in new if h[3] == kind]
                f.write('## %s (%d open)\n\n' % (kind, len(rows)))
                for h in sorted(rows):
                    f.write('- `%s:%d` %s: `%s`\n' % (h[0], h[1], h[2], h[4]))
                f.write('\n')
            if stale:
                f.write('## allow-list entries without a hit (%d)\n\n' % len(stale))
                for k in stale:
                    f.write('- `%s`\n' % k)
    print("empty services: %d hits, %d accepted (%d known holes), %d open (%s), %d stale allow entries"
          % (len(hits), len(hits) - len(new), len(known), len(new), ", ".join('%s %d' % kv for kv in sorted(by.items())) or 'none', len(stale)))
    if o.list:
        for h in sorted(new):
            print('%s  # %s:%d' % (key(h), h[0], h[1]))
    if o.check and new:
        for h in sorted(new):
            print('open: %s:%d %s %s %s' % (h[0], h[1], h[2], h[3], h[4]), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
