#!/usr/bin/env python
"""Rewrite global data sites to their typed symbols.toml names (typed-data
round stage B0, docs/typed-data-round.md section 3).

A global site is an accessor on a data-address constant: an `at::k`
constant, a module constant, or `bof3::addr::Name`, possibly plus a constant
offset (`B(at::kStep)`, `SetW(kTimer, 0)`, `At(at::kTable + 4)`). It becomes
the `[[data]]` entry's typed name only when every rule holds:

1. the address is exactly an entry with a `ctype`, or a constant offset that
   lands on an element of a typed array (`Name[i]`); a dynamic index
   (`B(at::kT + i)`, `W(at::kT + 2 * i)`) only on an array with a `count`
   whose element size is the stride;
2. the accessor's width and signedness are the `ctype`'s. Every accessor is
   read from its own definition (the file's, its includes', or a `using`'s),
   by evaluating its body: `return At(a)[0]`, `memcpy(&v, At(a), sizeof v)`,
   `static_cast<short>(Word(Mem(a)))`, a setter's `memcpy` or delegation. A
   read of the other signedness becomes an explicit `static_cast` of the
   name;
3. the expression keeps its type where it matters: a read whose accessor
   returns a type that promotes differently from the `ctype` (a `U` from a
   u16 load) keeps it as `static_cast<R>(Name)` unless the value is used
   directly (assigned, initialised, returned, compared for equality, tested);
   a setter `SetW(K, v);` becomes `Name = v;` only as a whole statement;
4. a pointer form (`At(K)`, `Mem(K)` not consumed by a load) or an address
   handed on as a value (`Foo(at::kX)`) keeps its accessor and becomes
   `At(Name_addr)` / `Name_addr`.

Where an accessor reaches memory through an out-of-line function (the
harnesses' `Mem`, `sh::Mem`, the seam the start-up fuzzes stand recorders in
behind), and in every `*_fuzz.cpp` / `*_harness.*` file, a site keeps its
accessor and only the constant changes: `B(Name_addr)`, `Mem(Name_addr)`.

Anything else is left raw and listed with its reason (`--left`).

    python tools/typed_convert.py [FILES...] [--left TSV] [--dry-run]
    python tools/typed_convert.py FILES... --gate BASE_BUILD [--build build]

With `--gate`, each converted file's objects are rebuilt and disassembled
against BASE_BUILD's (tools/codegen_same.py's comparison); a site whose
conversion changes the code is stepped down (typed, then typed with full
casts, then the address form where rule 4 allows it) or left raw with the
reason `gate`, found by bisection over the file's sites.

    python tools/typed_convert.py --retire [--dry-run]

removes every module constant (`*_callees.h`, a `namespace at`) whose name no
longer appears in any file that can see it, other than in its own definition.

Addresses and names only; no game data.
"""
from __future__ import annotations

import argparse
import collections
import fnmatch
import os
import re
import subprocess
import sys
import tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import data_census as dc  # noqa: E402

ROOT = dc.ROOT

# --------------------------------------------------------------------------
# Types
# --------------------------------------------------------------------------

INT_TYPES = {
    'unsigned char': (1, False), 'std::uint8_t': (1, False), 'uint8_t': (1, False), 'UC': (1, False),
    'u8': (1, False), 'U8': (1, False),
    'signed char': (1, True), 'char': (1, True), 'std::int8_t': (1, True), 'int8_t': (1, True), 'S8': (1, True),
    'unsigned short': (2, False), 'std::uint16_t': (2, False), 'uint16_t': (2, False), 'U16': (2, False),
    'u16': (2, False),
    'short': (2, True), 'std::int16_t': (2, True), 'int16_t': (2, True), 'S16': (2, True), 'i16': (2, True),
    'unsigned long': (4, False), 'unsigned int': (4, False), 'unsigned': (4, False), 'std::uint32_t': (4, False),
    'uint32_t': (4, False), 'U': (4, False), 'u32': (4, False), 'U32': (4, False), 'std::uintptr_t': (4, False),
    'uintptr_t': (4, False), 'std::size_t': (4, False), 'size_t': (4, False), 'DWORD': (4, False),
    'long': (4, True), 'int': (4, True), 'std::int32_t': (4, True), 'int32_t': (4, True), 'S32': (4, True),
    'I': (4, True), 'i32': (4, True), 'signed': (4, True), 'long int': (4, True),
}
FLOAT_TYPES = {'float': 4}
CANON = {(1, False): 'unsigned char', (1, True): 'signed char', (2, False): 'std::uint16_t',
         (2, True): 'std::int16_t', (4, False): 'std::uint32_t', (4, True): 'std::int32_t'}


def clean_type(t):
    t = ' '.join(t.replace('*', ' * ').replace('&', ' & ').split())
    t = re.sub(r'\b(inline|static|constexpr|volatile|\[\[[^\]]*\]\])\s*', '', t).strip()
    return t.replace(' *', '*').replace(' &', '&')


def int_type(t, aliases=None):
    """(size, signed) of an integer type spelling, None otherwise."""
    t = clean_type(t)
    if t.startswith('const '):
        t = t[6:]
    if aliases and t in aliases:
        t = aliases[t]
    return INT_TYPES.get(t)


def promoted(st):
    """The class a value of (size, signed) has in arithmetic."""
    size, signed = st
    return (4, True) if size < 4 else (4, signed)


# --------------------------------------------------------------------------
# Function definitions, for evaluating accessors
# --------------------------------------------------------------------------

FN_HEAD = re.compile(
    r'(?m)^[ \t]*((?:(?:static|inline|constexpr|\[\[[^\]]*\]\])\s+)*)'
    r'((?:const\s+)?(?:unsigned\s+|signed\s+)?[A-Za-z_][\w:]*(?:<[^<>;{}()]*>)?(?:\s+(?:int|long|short|char))?'
    r'(?:\s*[\*&]+\s*|\s+))'
    r'([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\(([^(){};]*)\)\s*(?:const\s*)?(?:noexcept\s*)?([{;])')


class FnDef:
    __slots__ = ('file', 'name', 'ret', 'params', 'body', 'ns', 'pos')

    def __init__(self, file, name, ret, params, body, ns, pos):
        self.file, self.name, self.ret, self.params, self.body, self.ns, self.pos = \
            file, name, ret, params, body, ns, pos

    def __repr__(self):
        return f'<{self.file.rel}:{self.name}({self.params}) {self.body!r:.60}>'


def parse_params(s):
    s = s.strip()
    if s in ('', 'void'):
        return []
    out = []
    for p in split_top(s, ','):
        p = p.split('=')[0].strip()
        m = re.match(r'(.*?)([A-Za-z_]\w*)\s*$', p)
        if not m or not m.group(1).strip():
            out.append((clean_type(p), ''))
        else:
            out.append((clean_type(m.group(1)), m.group(2)))
    return out


def split_top(s, sep):
    parts, depth, cur, i = [], 0, [], 0
    while i < len(s):
        c = s[i]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
        if depth == 0 and s.startswith(sep, i):
            parts.append(''.join(cur))
            cur = []
            i += len(sep)
            continue
        cur.append(c)
        i += 1
    parts.append(''.join(cur))
    return parts


def brace_end(code, i):
    depth = 0
    n = len(code)
    while i < n:
        c = code[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return n


def collect_defs(f):
    defs = []
    for m in FN_HEAD.finditer(f.code):
        ret = clean_type(m.group(2))
        name = m.group(3)
        if ret in ('return', 'else', 'new', 'delete', 'case', 'goto', 'using', 'typedef', 'namespace'):
            continue
        if name in ('if', 'while', 'for', 'switch', 'return', 'sizeof', 'static_assert'):
            continue
        params = parse_params(m.group(4))
        if m.group(5) == '{':
            end = brace_end(f.code, m.end() - 1)
            body = ' '.join(f.code[m.end():end - 1].split())
        else:
            body = None   # a declaration
        defs.append(FnDef(f, name.split('::')[-1], ret, params, body, f.ns_at(m.start()), m.start()))
    return defs


# --------------------------------------------------------------------------
# Semantic values
#   ('addr', off)                      the address parameter plus off
#   ('ptr', pointee, seam)             a pointer to that address
#   ('ref', type, seam)                an lvalue of `type` there
#   ('val', size, signed, type, seam)  a value loaded from there
#   ('fval', type, seam)               a float loaded from there
#   ('ptrval', pointee, seam)          a pointer loaded from there
#   ('write', size, vkind, seam)       a store of the value parameter there
#   ('v', kind)                        the value parameter (kind int / ptrint / ptr / other)
#   ('const',)                         a literal
# --------------------------------------------------------------------------

def seam_of(s):
    return bool(s and len(s) > 1 and s[-1] is True)


class Evaluator:
    def __init__(self, files):
        self.files = files
        self.by_rel = {f.rel: f for f in files}
        self.defs = {}
        for f in files:
            self.defs[f.rel] = collect_defs(f)
        self.usings = {}
        self.aliases = {}
        for f in files:
            self.usings[f.rel] = {m.group(2): m.group(1) for m in re.finditer(
                r'\busing\s+((?:\w+::)*\w+)::(\w+)\s*;', f.code)}
            self.aliases[f.rel] = {m.group(1): clean_type(m.group(2)) for m in re.finditer(
                r'\busing\s+(\w+)\s*=\s*([^;]+);', f.code)}
            for m in re.finditer(r'\btypedef\s+([^;]+?)\s+(\w+)\s*;', f.code):
                self.aliases[f.rel][m.group(2)] = clean_type(m.group(1))
        self.cache = {}

    def closure(self, f):
        return [f] + dc.include_closure(f, self.by_rel)

    def type_alias(self, f, t):
        t = clean_type(t)
        const = t.startswith('const ')
        base = t[6:] if const else t
        for g in self.closure(f):
            a = self.aliases[g.rel].get(base)
            if a and a != base:
                return ('const ' if const else '') + a
        return t

    def itype(self, f, t):
        t = self.type_alias(f, t)
        return int_type(t)

    def lookup(self, f, qual, name, nargs, argkinds):
        """FnDefs named `name` visible from f (qualified by `qual`)."""
        ns = None
        if qual:
            q = qual.rstrip(':').split('::')[-1]
            for g in self.closure(f):
                if q in g.aliases:
                    q = g.aliases[q].split('::')[-1]
                    break
            ns = q
        else:
            for g in self.closure(f):
                u = self.usings[g.rel].get(name)
                if u:
                    ns = u.split('::')[-1]
                    break
        cands = []
        for g in self.closure(f):
            for d in self.defs[g.rel]:
                if d.name != name or len(d.params) < nargs:
                    continue
                if len(d.params) > nargs:
                    continue
                if ns is not None and ns not in d.ns.split('::'):
                    continue
                cands.append(d)
        good = []
        for d in cands:
            ok = True
            for (pt, _), k in zip(d.params, argkinds):
                isptr = '*' in pt or pt.endswith('&')
                if k == 'ptr' and not isptr:
                    ok = False
                if k == 'int' and isptr:
                    ok = False
            if ok:
                good.append(d)
        # a definition beats a declaration of the same signature
        withbody = [d for d in good if d.body is not None]
        return withbody or good

    # ---- expression evaluation ------------------------------------------

    def ev(self, f, e, env, depth=0):
        if depth > 12:
            return None
        e = e.strip()
        while e.startswith('(') and self._closes_at_end(e):
            e = e[1:-1].strip()
        if e in env:
            return env[e]
        if re.fullmatch(r'(0[xX][0-9A-Fa-f]+|\d+)[uUlL]*', e):
            return ('const',)
        m = re.fullmatch(r'(reinterpret_cast|static_cast)\s*<\s*([^<>]+?)\s*>\s*\((.*)\)', e, re.S)
        if m and self._closes_at_end(e[e.index('('):]):
            return self.cast(f, m.group(1), m.group(2), self.ev(f, m.group(3), env, depth + 1))
        if e.startswith('*') and not e.startswith('**'):
            inner = self.ev(f, e[1:], env, depth + 1)
            return self.deref(inner)
        m = re.fullmatch(r'(.*)\[\s*0\s*\]', e, re.S)
        if m and self._balanced(m.group(1)):
            return self.deref(self.ev(f, m.group(1), env, depth + 1))
        m = re.fullmatch(r'(::)?((?:\w+::)*)(\w+)\s*\((.*)\)', e, re.S)
        if m and self._closes_at_end(e[e.index('(', len(m.group(1) or '') + len(m.group(2)) + len(m.group(3))):]):
            name, qual = m.group(3), m.group(2)
            if name in ('Address', 'Key', 'Addr', 'KeyOf') and not qual:
                a = self.ev(f, m.group(4), env, depth + 1)
                if a and a[0] == 'v' and a[1] == 'ptr':
                    return ('v', 'ptrint')
                return None
            args = split_top(m.group(4), ',') if m.group(4).strip() else []
            sems = [self.ev(f, a, env, depth + 1) for a in args]
            return self.call(f, qual, name, sems, depth + 1)
        m = re.fullmatch(r'(\w+)\s*&\s*(0[xX][fF]+)[uU]?', e)
        if m and m.group(1) in env and env[m.group(1)][0] == 'v':
            width = (len(m.group(2)) - 2) // 2
            return ('v', 'int', width)
        return None

    @staticmethod
    def _closes_at_end(e):
        if not e.startswith('('):
            return False
        depth = 0
        for i, c in enumerate(e):
            if c in '([{':
                depth += 1
            elif c in ')]}':
                depth -= 1
                if depth == 0:
                    return i == len(e) - 1
        return False

    @staticmethod
    def _balanced(e):
        depth = 0
        for c in e:
            if c in '([{':
                depth += 1
            elif c in ')]}':
                depth -= 1
                if depth < 0:
                    return False
        return depth == 0

    def deref(self, s):
        if s and s[0] == 'ptr' and s[1] != 'void':
            return ('ref', s[1], s[2])
        return None

    def cast(self, f, kind, t, s):
        if s is None:
            return None
        t = self.type_alias(f, t)
        tc = clean_type(t)
        if tc.endswith('*'):
            pointee = clean_type(tc[:-1])
            if pointee.startswith('const '):
                pointee = pointee[6:]
            if s[0] == 'addr':
                return ('ptr', pointee, False)
            if s[0] == 'ptr':
                return ('ptr', pointee, s[2])
            if s[0] == 'val' and s[1] == 4:
                return ('ptrval', pointee, s[4])
            if s[0] == 'ptrval':
                return ('ptrval', pointee, s[2])
            if s[0] == 'v' and s[1] == 'ptr':
                return s
            return None
        it = int_type(tc)
        if it is None:
            return None
        if s[0] == 'addr':
            return s if it[0] == 4 else None
        if s[0] == 'ref':
            st = self.itype(f, s[1])
            if st is None:
                return None
            s = ('val', st[0], st[1], s[1], s[2])
        if s[0] == 'val':
            size, signed = s[1], s[2]
            if it[0] < size:
                return ('val', it[0], it[1], tc, s[4])
            if it[0] == size:
                return ('val', size, it[1], tc, s[4])
            return ('val', size, signed, tc, s[4])
        if s[0] == 'ptr' and it[0] == 4 and kind == 'reinterpret_cast':
            return None
        if s[0] == 'v':
            if s[1] == 'ptr' and kind == 'reinterpret_cast' and it[0] == 4:
                return ('v', 'ptrint')
            if s[1] == 'int':
                return ('v', 'int', min(it[0], s[2] if len(s) > 2 else 4))
            if s[1] == 'ptrint' and it[0] == 4:
                return s
            return None
        if s[0] == 'ptrval' and it[0] == 4:
            return None
        return None

    def call(self, f, qual, name, sems, depth):
        if any(s is None for s in sems[:1]):
            return None
        kinds = []
        for s in sems:
            if s is None:
                kinds.append('?')
            elif s[0] in ('ptr', 'ptrval') or (s[0] == 'v' and s[1] == 'ptr'):
                kinds.append('ptr')
            elif s[0] in ('addr', 'val', 'ref', 'const') or (s[0] == 'v' and s[1] in ('int', 'ptrint')):
                kinds.append('int')
            else:
                kinds.append('?')
        defs = self.lookup(f, qual, name, len(sems), kinds)
        if not defs:
            return None
        d = defs[0]
        if d.body is None:
            # an out-of-line function: only the harnesses' Mem-like seam
            # (pointer from address) is understood
            if len(d.params) == 1 and sems[0][0] == 'addr' and clean_type(d.ret) in (
                    'unsigned char*', 'std::uint8_t*'):
                return ('ptr', 'unsigned char', True)
            return None
        env = {}
        for (pt, pn), s in zip(d.params, sems):
            if not pn:
                return None
            if s is None:
                s = ('opaque',)
            if s[0] == 'v' or s[0] == 'const' or s[0] == 'opaque':
                # a value argument: what the callee sees is its parameter
                st = self.itype(d.file, pt)
                if '*' in pt:
                    s = ('v', 'ptr')
                elif st is not None:
                    w = s[2] if (s[0] == 'v' and len(s) > 2) else 4
                    s = ('v', 'int', min(w, st[0])) if s[0] != 'v' or s[1] == 'int' else s
                elif clean_type(pt) in ('double', 'float'):
                    s = ('v', 'other')
                else:
                    s = ('v', 'other')
            elif s[0] in ('val', 'ref') and not ('*' in pt):
                # a loaded value handed on as an argument (a cast function)
                st = self.itype(d.file, pt)
                if st is None:
                    return None
                if s[0] == 'ref':
                    rt = self.itype(f, s[1])
                    if rt is None:
                        return None
                    s = ('val', rt[0], rt[1], s[1], s[2])
                s = self.cast(d.file, 'static_cast', pt, s)
            env[pn] = s
        return self.body(d, env, depth)

    def body(self, d, env, depth):
        b = d.body
        f = d.file
        m = re.fullmatch(r'return (.*);', b, re.S)
        if m and ';' not in m.group(1):
            r = self.ev(f, m.group(1), env, depth + 1)
            if r is None:
                return None
            ret = self.type_alias(f, d.ret)
            if r[0] == 'val':
                return self.cast(f, 'static_cast', ret, r) if not ret.endswith('&') else None
            if r[0] == 'ref':
                if ret.endswith('&'):
                    rt = clean_type(ret[:-1])
                    if self.itype(f, rt) == self.itype(f, r[1]):
                        return ('ref', rt, r[2])
                    return None
                return self.cast(f, 'static_cast', ret, r)
            if r[0] == 'fval':
                return r if clean_type(ret) in ('float',) else None
            if r[0] == 'ptr':
                rt = clean_type(ret)
                if rt.endswith('*'):
                    pointee = clean_type(rt[:-1])
                    if pointee.startswith('const '):
                        pointee = pointee[6:]
                    return ('ptr', pointee, r[2])
                return None
            if r[0] == 'ptrval':
                rt = clean_type(ret)
                if rt.endswith('*'):
                    pointee = clean_type(rt[:-1])
                    if pointee.startswith('const '):
                        pointee = pointee[6:]
                    return ('ptrval', pointee, r[2])
                return None
            if r[0] == 'addr':
                return r if (int_type(self.type_alias(f, ret)) or (0,))[0] == 4 else None
            if r[0] == 'write':
                return r if clean_type(ret) == 'void' else None
            return None
        # T v; std::memcpy(&v, P, sizeof v); return v;
        m = re.fullmatch(r'((?:const\s+)?[\w:]+(?:\s+(?:int|long|short|char))?) (\w+); (?:std::|__builtin_)?memcpy\(&\2, (.*), '
                         r'(?:sizeof \2|sizeof\(\2\)|sizeof \(\2\)|\d+)\); return \2;', b, re.S)
        if m:
            p = self.ev(f, m.group(3), env, depth + 1)
            if not p or p[0] != 'ptr':
                return None
            t = self.type_alias(f, m.group(1))
            r = None
            st = int_type(t)
            if st:
                r = ('val', st[0], st[1], clean_type(t), p[2])
            elif clean_type(t) == 'float':
                r = ('fval', 'float', p[2])
            if r is None:
                return None
            ret = self.type_alias(f, d.ret)
            if r[0] == 'val':
                return self.cast(f, 'static_cast', ret, r)
            if r[0] == 'fval' and clean_type(ret) == 'float':
                return r
            return None
        # std::memcpy(P, &v, sizeof v);
        m = re.fullmatch(r'(?:std::|__builtin_)?memcpy\((.*), &(\w+), (?:sizeof \2|sizeof\(\2\)|sizeof \(\2\)|(\d+))\);', b, re.S)
        if m:
            p = self.ev(f, m.group(1), env, depth + 1)
            v = env.get(m.group(2))
            if not p or p[0] != 'ptr' or not v or v[0] != 'v':
                return None
            pt = dict((n, t) for t, n in d.params).get(m.group(2))
            st = self.itype(f, pt) if pt else None
            if st is None:
                return None
            width = int(m.group(3)) if m.group(3) else st[0]
            if width != st[0]:
                return None
            return ('write', width, v[1], p[2])
        # const auto w = static_cast<T>(v [& mask]); std::memcpy(P, &w, sizeof w);
        m = re.fullmatch(r'const ([\w:]+) (\w+) = (static_cast<[^<>]+>\(.*?\)); (?:std::|__builtin_)?memcpy\((.*), &\2, '
                         r'(?:sizeof \2|sizeof\(\2\)|sizeof \(\2\)|(\d+))\);', b, re.S)
        if m:
            p = self.ev(f, m.group(4), env, depth + 1)
            v = self.ev(f, m.group(3), env, depth + 1)
            if not p or p[0] != 'ptr' or not v or v[0] != 'v':
                return None
            tm = re.match(r'static_cast<([^<>]+)>', m.group(3))
            st = self.itype(f, tm.group(1))
            if st is None:
                return None
            width = int(m.group(5)) if m.group(5) else st[0]
            if width != st[0] or (len(v) > 2 and v[2] < width):
                return None
            return ('write', width, v[1], p[2])
        # LV = static_cast<T>(v);   (*At(a), At(a)[0], B(a))
        m = re.fullmatch(r'(.*?) = (.*);', b, re.S)
        if m and ';' not in m.group(2) and '==' not in m.group(1):
            lv = self.ev(f, m.group(1), env, depth + 1)
            v = self.ev(f, m.group(2), env, depth + 1)
            if lv and lv[0] == 'ref' and v and v[0] == 'v' and v[1] == 'int':
                st = self.itype(f, lv[1])
                if st and (len(v) <= 2 or v[2] >= st[0]):
                    return ('write', st[0], 'int', lv[2])
            return None
        # F(P, value);  a setter handing on to another
        m = re.fullmatch(r'((?:\w+::)*)(\w+)\((.*)\);', b, re.S)
        if m:
            args = split_top(m.group(3), ',')
            sems = [self.ev(f, a, env, depth + 1) for a in args]
            r = self.call(f, m.group(1), m.group(2), sems, depth + 1)
            if r and r[0] == 'write':
                return r
            return None
        return None

    # ---- an accessor applied at a site -----------------------------------

    def at_site(self, f, qual, name, argsems):
        key = (f.rel, qual, name, tuple(argsems))
        if key not in self.cache:
            self.cache[key] = self.call(f, qual, name, list(argsems), 0)
        return self.cache[key]


# --------------------------------------------------------------------------
# Data entries
# --------------------------------------------------------------------------

class Entries:
    def __init__(self, path):
        with open(path, 'rb') as fh:
            t = tomllib.load(fh)
        self.data = sorted(t.get('data', []), key=lambda e: e['pc'])
        self.by_pc = {e['pc']: e for e in self.data}
        self.starts = [e['pc'] for e in self.data]

    @staticmethod
    def elem_size(e):
        ct = e.get('ctype')
        if not ct:
            return 0
        if '*' in ct:
            return 4
        st = int_type(ct)
        if st:
            return st[0]
        if ct in FLOAT_TYPES:
            return FLOAT_TYPES[ct]
        return 0

    def find(self, v):
        """(entry, offset) covering address v, or (None, None)."""
        if v in self.by_pc:
            return self.by_pc[v], 0
        import bisect
        # entries may nest (Party_Zenny inside Cond_Flags): the covering one
        # that starts last
        i = bisect.bisect_right(self.starts, v) - 1
        while i >= 0 and v - self.data[i]['pc'] < 0x40000:
            e = self.data[i]
            size = self.elem_size(e) * e.get('count', 1)
            if size and v < e['pc'] + size:
                return e, v - e['pc']
            i -= 1
        return None, None


# --------------------------------------------------------------------------
# Sites
# --------------------------------------------------------------------------

HARNESS = re.compile(r'(_fuzz\.|_harness|fuzz_)')
BAD_BARE_NAME = re.compile(r'(End|Last|Limit|Top|Hi|Lo|Max|Min|Stop|Size|Count|Stride|Mask|Bytes|Len|Shift)$')
KEYWORDS = frozenset('if while for switch return sizeof alignof decltype static_cast reinterpret_cast const_cast '
                     'case else do new delete throw static_assert defined'.split())


class Site:
    """One reference to a data constant and what becomes of it."""
    __slots__ = ('f', 'ref_s', 'ref_e', 'value', 'line', 'tiers', 'tier', 'reason', 'entry', 'off',
                 'span', 'acc', 'kind', 'cname')

    def __init__(self, f, ref_s, ref_e, value, cname):
        self.f, self.ref_s, self.ref_e, self.value, self.cname = f, ref_s, ref_e, value, cname
        self.line = dc.line_of(f.starts, ref_s)
        self.tiers = []     # [(span_s, span_e, parts, label)], best first
        self.tier = 0
        self.reason = None
        self.entry = None
        self.off = None
        self.span = None
        self.acc = '-'
        self.kind = '-'

    def choice(self):
        if self.reason is not None or self.tier >= len(self.tiers):
            return None
        return self.tiers[self.tier]


def prev_token(code, i):
    j = i - 1
    while j >= 0 and code[j] in ' \t\r\n':
        j -= 1
    if j < 0:
        return '', j
    if code[j].isalnum() or code[j] == '_':
        k = j
        while k >= 0 and (code[k].isalnum() or code[k] == '_'):
            k -= 1
        return code[k + 1:j + 1], k + 1
    two = code[max(0, j - 1):j + 1]
    if two in ('==', '!=', '<=', '>=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '&&', '||', '<<', '>>',
               '->', '++', '--', '::'):
        return two, j - 1
    return code[j], j


def next_token(code, i):
    j = i
    n = len(code)
    while j < n and code[j] in ' \t\r\n':
        j += 1
    if j >= n:
        return '', j
    if code[j].isalnum() or code[j] == '_':
        k = j
        while k < n and (code[k].isalnum() or code[k] == '_'):
            k += 1
        return code[j:k], j
    three = code[j:j + 3]
    if three in ('<<=', '>>='):
        return three, j
    two = code[j:j + 2]
    if two in ('==', '!=', '<=', '>=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '&&', '||', '<<', '>>',
               '->', '++', '--', '::'):
        return two, j
    return code[j], j


def enclosing_open(code, pos, limit=4000):
    """The innermost unmatched '(' or '[' before pos, and the argument index."""
    depth = 0
    arg = 0
    i = pos - 1
    lo = max(0, pos - limit)
    while i >= lo:
        c = code[i]
        if c in ')]}':
            depth += 1
        elif c in '([{':
            if depth == 0:
                return i, c, arg
            depth -= 1
        elif c == ',' and depth == 0:
            arg += 1
        elif c == ';' and depth == 0:
            return None, None, None
        i -= 1
    return None, None, None


def callee_before(code, open_pos):
    """(qual, name, start) of the identifier call whose '(' is at open_pos."""
    m = re.search(r'((?:::)?(?:[A-Za-z_]\w*::)*)([A-Za-z_]\w*)\s*$', code[max(0, open_pos - 200):open_pos])
    if not m:
        return None
    start = open_pos - (len(code[max(0, open_pos - 200):open_pos]) - m.start())
    # a member call (`x.B(`, `p->B(`) is not ours
    j = start - 1
    while j >= 0 and code[j] in ' \t':
        j -= 1
    if j >= 0 and (code[j] == '.' or code[j - 1:j + 1] == '->'):
        return None
    return m.group(1), m.group(2), start


def mask_casts(s):
    """Blank the angle brackets of casts and templates so a top-level scan
    does not read them as operators."""
    out = list(s)
    for m in re.finditer(r'\b(?:static_cast|reinterpret_cast|const_cast|[A-Za-z_]\w*_cast)\s*<', s):
        depth, i = 0, m.end() - 1
        while i < len(s):
            if s[i] == '<':
                depth += 1
            elif s[i] == '>':
                depth -= 1
                if depth == 0:
                    for k in range(m.end() - 1, i + 1):
                        out[k] = ' '
                    break
            i += 1
    return ''.join(out)


def top_level_ops(s):
    """The operators of s at paren depth 0 (casts masked)."""
    s = mask_casts(s)
    ops, depth, i = [], 0, 0
    while i < len(s):
        c = s[i]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
        elif depth == 0 and c in '+-*/%<>&|^?=!~':
            two = s[i:i + 2]
            if two in ('<<', '>>', '&&', '||', '==', '!=', '<=', '>=', '->', '++', '--'):
                ops.append((i, two))
                i += 2
                continue
            ops.append((i, c))
        i += 1
    return ops


class Converter:
    def __init__(self, toml='symbols.toml'):
        os.chdir(ROOT)
        self.syms = dc.Symbols(toml)
        self.entries = Entries(toml)
        self.files = dc.load_sources()
        self.by_rel = {f.rel: f for f in self.files}
        self.genv = dc.collect_constants(self.files, self.syms.names)
        self.defs_by_name = collections.defaultdict(list)
        for f in self.files:
            for name, value, ns, line in f.consts:
                self.defs_by_name[name].append((f, value, ns, line))
        self.ev = Evaluator(self.files)
        self.envs = {}

    def env(self, f):
        if f.rel not in self.envs:
            e = dict(self.syms.names)
            e.update({x['name'] + '_addr': x['pc'] for x in self.entries.data if x.get('ctype')})
            e.update(self.genv)
            for g in reversed(dc.include_closure(f, self.by_rel)):
                e.update({k: v for k, v in g.locals.items() if v is not None})
            e.update({k: v for k, v in f.locals.items() if v is not None})
            self.envs[f.rel] = e
        return self.envs[f.rel]

    # ---- finding the references ------------------------------------------

    def refs(self, f):
        """(start, end, value, name) for every data-constant reference in f
        that the census counts: at:: references, module constants in an
        accessor's argument, `bof3::addr::Name`."""
        code = f.code
        out = []
        for m in dc.AT_REF.finditer(code):
            if f.in_at_namespace(m.start()):
                continue
            value, cands = dc.resolve_at(f, m.group(1), m.group(2), m.start(), self.by_rel, self.defs_by_name)
            if not cands:
                continue
            if value is None:
                if any(self.syms.in_data(c[1]) for c in cands):
                    out.append((m.start(), m.end(), None, m.group(0)))
                continue
            if not self.syms.in_data(value):
                continue
            out.append((m.start(), m.end(), value, m.group(0)))
        taken = {(s, e) for s, e, _, _ in out}
        # module constants (outside a namespace at) whose value is a data address
        for m in re.finditer(r'(?<![\w.>:])((?:\w+::)*)(k[A-Z0-9_]\w*)\b(?!\s*[\[(.{])', code):
            if dc.is_at_prefix(m.group(1)) or (m.start(), m.end()) in taken:
                continue
            if f.in_at_namespace(m.start()):
                continue
            # skip a definition (`kX = ...`, `kA = 1, kB = 2`): a constant is
            # never assigned
            if re.match(r'\s*=(?!=)', code[m.end():]):
                continue
            v = self.module_value(f, m.group(1), m.group(2))
            if not self.syms.in_data(v):
                continue
            out.append((m.start(), m.end(), v, m.group(0)))
        out.sort()
        return out

    def module_value(self, f, qual, name):
        """The value of a module constant as f sees it: its own definition or
        its includes' (no guess from elsewhere in src/, where the same name
        may be an enumerator or another module's); qualified, only by a
        namespace that defines the name."""
        if qual:
            q = qual.rstrip(':').split('::')[-1]
            for g in [f] + dc.include_closure(f, self.by_rel):
                if q in g.aliases:
                    q = g.aliases[q].split('::')[-1]
                    break
            vals = {v for g in self.files for n, v, ns, _ in g.mconsts + g.consts
                    if n == name and ns.split('::')[-1] == q and v is not None}
            return vals.pop() if len(vals) == 1 else None
        if f.locals.get(name) is not None:
            return f.locals[name]
        for g in dc.include_closure(f, self.by_rel):
            if g.locals.get(name) is not None:
                return g.locals[name]
        return None

    # ---- one site -----------------------------------------------------------

    def analyse(self, f, s, e, value, cname):
        site = Site(f, s, e, value, cname)
        code = f.code
        if value is None:
            site.reason = 'ambiguous'
            return site
        ls = code.rfind('\n', 0, s) + 1
        pl = code.rfind('\n', 0, max(0, ls - 1)) + 1
        if code[ls:s].lstrip().startswith('#') or code[pl:max(pl, ls - 1)].rstrip().endswith('\\'):
            site.reason = 'macro'
            return site
        entry, off = self.entries.find(value)
        site.entry, site.off = entry, off
        if entry is None:
            site.reason = 'no-entry'
            return site
        if not entry.get('ctype'):
            site.reason = 'untyped-entry'
            return site
        esize = self.entries.elem_size(entry)
        if esize == 0:
            site.reason = 'untyped-entry'
            return site
        harness = bool(HARNESS.search(f.rel))
        # the address expression around the reference, and the call it is the first argument of
        op, ch, argi = enclosing_open(code, s)
        call = None
        if op is not None and ch == '(':
            cb = callee_before(code, op)
            if cb and cb[1] not in KEYWORDS:
                call = cb
        if call is not None and argi == 0:
            r = dc.balanced_args(code, op)
            if r is None:
                call = None
            else:
                args, close = r
                arg_s = op + 1
                arg_e = arg_s + len(args[0])
        else:
            call = None
        addr_tiers = self.addr_tiers(site, entry, off, esize)
        if call is None:
            # an address handed on as a value
            site.kind = 'bare'
            if BAD_BARE_NAME.search(cname.split('::')[-1]):
                site.reason = 'name'
                return site
            return self.address_only(site, addr_tiers, cname, off)
        qual, name, cstart = call
        site.acc = name
        vocab = name in dc.ACCESSORS
        form = self.addr_form(f, code[arg_s:arg_e], s - arg_s, e - arg_s)
        if form is None:
            # an expression we do not read as REF + offset (+ index): the
            # constant alone may still be named, as an address
            site.kind = 'expr'
            return self.address_only(site, addr_tiers, cname, off)
        coff, idx = form          # constant offset, (index_s, index_e, stride) or None
        total_off = off + coff
        sem = self.ev.at_site(f, qual, name, (('addr', 0),) + tuple(
            ('opaque',) for _ in range(len(args) - 1)))
        if sem is not None and len(args) > 1 and sem[0] != 'write':
            sem = None            # a helper with more arguments: not an accessor we read
        wr0 = None
        if sem is not None and sem[0] == 'write':
            if len(args) != 2:
                sem = None
            else:
                wr0 = (arg_s + len(args[0]) + 1, close - 1, 2)
        site.kind = sem[0] if sem else 'unknown'
        # a load through the pointer (Word(At(K)), At(K)[i], *At(K),
        # static_cast<short>(W(K))) is climbed to
        span_s, span_e = cstart, close
        chain = [(span_s, span_e, sem, wr0, 0, idx)]
        cur, cs, ce, extra, dyn = sem, span_s, span_e, 0, idx
        for _ in range(4):
            nxt = self.climb(f, cs, ce, cur, dyn)
            if nxt is None:
                break
            cs, ce, cur, add, dyn2, wr = nxt
            extra += add
            if dyn2 is not None:
                dyn = dyn2
            chain.append((cs, ce, cur, wr, extra, dyn))
        seam = any(seam_of(c[2]) for c in chain if c[2])
        if harness or seam:
            site.kind = 'seam' if seam else 'harness'
            site.tiers = addr_tiers
            if not site.tiers:
                site.reason = 'inside-entry'
            return site
        # the outermost typed form first
        typed, why = [], None
        for cs, ce, sm, wr, ex, dy in reversed(chain):
            if sm is None:
                continue
            t = self.typed(site, entry, esize, total_off + ex, dy, cs, ce, sm, wr)
            if isinstance(t, str):
                why = why or t
                continue
            typed = t
            break
        pointer_form = sem is not None and sem[0] in ('ptr', 'addr')
        if typed:
            site.span = (typed[0][0], typed[0][1])
            site.tiers = typed + (addr_tiers if pointer_form else [])
            return site
        if pointer_form or (sem is None and not vocab):
            # a pointer form nobody loads through here, or the address handed
            # to a helper: the constant is renamed, the helper stays
            site.kind = 'ptr' if pointer_form else 'helper'
            return self.address_only(site, addr_tiers, cname, off)
        site.reason = why or ('accessor' if sem is None else 'type')
        return site

    def address_only(self, site, addr_tiers, cname, off):
        if 'at::' not in cname and off:
            site.reason = 'inside-entry'
            return site
        site.tiers = addr_tiers
        if not site.tiers:
            site.reason = 'inside-entry'
        return site

    def addr_tiers(self, site, entry, off, esize):
        if off % esize != 0 or (off and 'count' not in entry):
            return []
        name = entry['name'] + '_addr'
        rep = name if off == 0 else f'({name} + {off:#x})'
        # bare `(at::kX)` needs no extra parentheses
        return [(site.ref_s, site.ref_e, [rep], 'addr')]

    def addr_form(self, f, arg, rs, re_):
        """(constant offset, index) of an address argument `REF [+ c] [+ i * s]`
        relative to the reference's value; None if it is anything else."""
        # the reference must be at top level
        pre = arg[:rs]
        if pre.count('(') != pre.count(')'):
            # inside parentheses: allow only `(REF + c)` wholly parenthesised
            st = arg.strip()
            if st.startswith('(') and Evaluator._closes_at_end(st):
                lead = len(arg) - len(arg.lstrip()) + 1
                inner = st[1:-1]
                return self.addr_form(f, inner, rs - lead, re_ - lead)
            return None
        masked = mask_casts(arg)
        ops = top_level_ops(arg)
        if any(o not in ('+', '-', '*') for _, o in ops):
            return None
        # split into signed terms at top-level + and -
        terms, last, sign = [], 0, '+'
        for pos, o in ops:
            if o in '+-':
                # a unary sign (after an operator or at start) belongs to its term
                if masked[last:pos].strip() == '':
                    continue
                terms.append((sign, last, pos))
                sign, last = o, pos + 1
        terms.append((sign, last, len(arg)))
        coff, idx, seen = 0, None, False
        env = self.env(f)
        for sg, a, b in terms:
            t = arg[a:b]
            if a <= rs and re_ <= b:
                if t.strip() != arg[rs:re_] or sg != '+':
                    return None
                seen = True
                continue
            v = dc.eval_expr(t, env)
            if v is not None:
                if self.syms.in_data(v) and abs(v) > 0x10000:
                    return None       # a second address in the argument
                coff += v if sg == '+' else -v
                continue
            if sg != '+' or idx is not None:
                return None
            # a dynamic index: X, X * s, s * X
            facs = split_top(mask_casts(t), '*')
            if len(facs) == 1:
                ts = a + (len(t) - len(t.lstrip()))
                idx = (ts, a + len(t.rstrip()), 1)
                continue
            # find the constant factor(s)
            pos_list, p = [], a
            raw_facs = []
            for fc in facs:
                raw_facs.append((p, p + len(fc)))
                p += len(fc) + 1
            consts, dyns = 1, []
            for fs, fe in raw_facs:
                v = dc.eval_expr(arg[fs:fe], env)
                if v is not None:
                    consts *= v
                else:
                    dyns.append((fs, fe))
            if len(dyns) != 1:
                return None
            fs, fe = dyns[0]
            fs += len(arg[fs:fe]) - len(arg[fs:fe].lstrip())
            fe -= len(arg[fs:fe]) - len(arg[fs:fe].rstrip())
            idx = (fs, fe, consts)
        if not seen:
            return None
        coff &= 0xFFFFFFFF
        if coff >= 0x80000000:
            coff -= 0x100000000
        if idx is not None:
            idx = (idx[0], idx[1], idx[2])
        return coff, idx

    def climb(self, f, cs, ce, sem, idx):
        """Extend the span one level outward when that level loads or stores
        through what the span computes: (s, e, sem, offset, index, write)."""
        code = f.code
        if sem is None:
            return None
        # postfix [c] / [i]
        t, j = next_token(code, ce)
        if t == '[' and sem[0] == 'ptr' and sem[1] != 'void':
            depth, k = 0, j
            while k < len(code):
                if code[k] == '[':
                    depth += 1
                elif code[k] == ']':
                    depth -= 1
                    if depth == 0:
                        break
                k += 1
            inner = code[j + 1:k]
            v = dc.eval_expr(inner, self.env(f))
            st = self.ev.itype(f, sem[1])
            if st is None:
                return None
            if v is not None:
                return (cs, k + 1, ('ref', sem[1], sem[2]), v * st[0], None, None)
            if idx is not None:
                return None
            ins = j + 1 + (len(inner) - len(inner.lstrip()))
            ine = j + 1 + len(inner.rstrip())
            return (cs, k + 1, ('ref', sem[1], sem[2]), 0, (ins, ine, st[0]), None)
        # prefix *
        p, pj = prev_token(code, cs)
        if p == '*' and sem[0] == 'ptr' and sem[1] != 'void':
            q, _ = prev_token(code, pj)
            if q in ('', '(', ',', '=', 'return', '{', ';', '?', ':', '!', '+', '-', '==', '!=', '&&', '||', '[') \
                    or q in ('==', '!=', '+=', '-=', '|=', '&=', '^='):
                return (pj, ce, ('ref', sem[1], sem[2]), 0, None, None)
        # static_cast<T>( span )
        op, ch, argi = enclosing_open(code, cs)
        if op is None or ch != '(' or argi != 0:
            return None
        r = dc.balanced_args(code, op)
        if r is None:
            return None
        args, close = r
        arg_s = op + 1
        a0 = code[arg_s:arg_s + len(args[0])]
        rest = code[ce:arg_s + len(args[0])]
        add = 0
        if rest.strip():
            m = re.fullmatch(r'\s*([+-])\s*(.+?)\s*', rest, re.S)
            if not m or sem[0] != 'ptr' or idx is not None:
                return None
            v = dc.eval_expr(m.group(2), self.env(f))
            if v is None:
                return None
            add = v if m.group(1) == '+' else -v
            if a0[:cs - arg_s].strip():
                return None
        elif code[arg_s:cs].strip():
            return None
        m = re.search(r'\b(static_cast|reinterpret_cast)\s*<\s*([^<>]+?)\s*>\s*$', code[max(0, op - 200):op])
        if m:
            if len(args) != 1 or add:
                return None
            cst = op - (len(code[max(0, op - 200):op]) - m.start())
            s2 = self.ev.cast(f, m.group(1), m.group(2), sem)
            if s2 is None:
                return None
            return (cst, close, s2, 0, None, None)
        cb = callee_before(code, op)
        if not cb or cb[1] in KEYWORDS:
            return None
        qual, name, cstart = cb
        sems = [sem if not add else sem] + [('opaque',)] * (len(args) - 1)
        s2 = self.ev.at_site(f, qual, name, tuple(sems))
        if s2 is None:
            return None
        if len(args) != 1 and not (s2[0] == 'write' and len(args) == 2):
            return None
        if s2[0] == 'write':
            return (cstart, close, s2, add, None, (arg_s + len(args[0]) + 1, close - 1, len(args)))
        if s2[0] in ('ptr', 'addr'):
            return None
        return (cstart, close, s2, add, None, None)

    def typed(self, site, entry, esize, off, dyn, cs, ce, sem, wr):
        """The typed replacement tiers for span [cs, ce) computing `sem`, or a
        reason string."""
        f = site.f
        code = f.code
        ct = entry['ctype']
        cst = int_type(ct)
        is_ptr_entry = '*' in ct
        if off % esize != 0:
            return 'inside-entry'
        if (off or dyn) and 'count' not in entry:
            return 'inside-entry' if off else 'index'
        if dyn is not None and dyn[2] != esize:
            return 'index'
        i = off // esize
        count = entry.get('count')
        if count is not None and dyn is None and not (0 <= i < count):
            return 'inside-entry'
        name = entry['name']
        if count is not None:
            if dyn is not None:
                lead = f'{i:#x} + ' if i else ''
                x_parts = [name + '[' + lead, (dyn[0], dyn[1]), ']']
            else:
                x_parts = [f'{name}[{i if i < 10 else hex(i)}]']
        else:
            x_parts = [name]
        k = sem[0]
        if k == 'write':
            width, vkind = sem[1], sem[2]
            if wr is None:
                return 'accessor'
            if width != esize:
                return 'width'
            if vkind == 'int' and (cst is None):
                return 'type'
            if vkind == 'ptrint' and not is_ptr_entry:
                return 'pointer-in-integer'
            if vkind not in ('int', 'ptrint'):
                return 'type'
            if wr[2] != 2:
                return 'accessor'
            if not self.is_statement(code, cs, ce):
                return 'context'
            vs, ve = wr[0], wr[1]
            # trim the value's whitespace
            while vs < ve and code[vs] in ' \t\r\n':
                vs += 1
            while ve > vs and code[ve - 1] in ' \t\r\n':
                ve -= 1
            parts = x_parts + [' = ', (vs, ve)]
            return [(cs, ce, parts, 'write')]
        if k == 'ref':
            rt = self.ev.itype(f, sem[1])
            if rt is None or cst is None:
                return 'type'
            if rt[0] != esize:
                return 'width'
            lv = self.lvalue_use(code, cs, ce)
            if rt[1] == cst[1]:
                return [(cs, ce, x_parts, 'typed')]
            # the other signedness
            if lv is None:
                return [(cs, ce, [f'static_cast<{CANON[rt]}>('] + x_parts + [')'], 'typed')]
            if lv in ('=', '|=', '&=', '^=', '+=', '-=', '++', '--', '<<='):
                return [(cs, ce, x_parts, 'typed')]
            return 'signed'
        if k == 'val':
            size, signed, rtype = sem[1], sem[2], sem[3]
            if cst is None:
                return 'type'
            if size != esize:
                return 'width'
            inner = x_parts
            base_st = cst
            if signed != cst[1]:
                inner = [f'static_cast<{CANON[(size, signed)]}>('] + x_parts + [')']
                base_st = (size, signed)
            rt = self.ev.itype(f, rtype) or int_type(rtype)
            if rt is None:
                return 'type'
            full = inner if promoted(base_st) == promoted(rt) else \
                [f'static_cast<{CANON[rt] if rt[0] == 4 else CANON[rt]}>('] + inner + [')']
            if full is inner:
                return [(cs, ce, inner, 'typed')]
            tiers = []
            if self.value_use_direct(code, cs, ce):
                tiers.append((cs, ce, inner, 'typed'))
            tiers.append((cs, ce, full, 'typed-cast'))
            return tiers
        if k == 'fval':
            if ct != 'float':
                return 'type'
            return [(cs, ce, x_parts, 'typed')]
        if k == 'ptrval':
            if not is_ptr_entry:
                return 'pointer-in-integer'
            pointee = clean_type(ct[:-1].strip())
            if pointee.startswith('const '):
                return 'type'
            if pointee == sem[1] or (pointee in ('unsigned char', 'std::uint8_t') and sem[1] in (
                    'unsigned char', 'std::uint8_t')):
                return [(cs, ce, x_parts, 'typed')]
            if pointee == 'void':
                return [(cs, ce, [f'static_cast<{sem[1]}*>('] + x_parts + [')'], 'typed')]
            return 'type'
        if k in ('ptr', 'addr'):
            return 'ptr'
        return 'accessor'

    @staticmethod
    def is_statement(code, cs, ce):
        t, _ = next_token(code, ce)
        if t != ';':
            return False
        p, pj = prev_token(code, cs)
        if p in (';', '{', '}', 'else', 'do'):
            return True
        if p == ':':
            q, _ = prev_token(code, pj)
            # a label or `case X:` / `default:`; not the ternary's
            ls = code.rfind('\n', 0, pj) + 1
            return bool(re.match(r'\s*(case\b|default\b|\w+\s*:)', code[ls:pj + 1]))
        if p == ')':
            # the body of an if / while / for without braces
            depth, k = 0, pj
            while k >= 0:
                if code[k] == ')':
                    depth += 1
                elif code[k] == '(':
                    depth -= 1
                    if depth == 0:
                        break
                k -= 1
            kw, _ = prev_token(code, k)
            return kw in ('if', 'while', 'for')
        return False

    @staticmethod
    def lvalue_use(code, cs, ce):
        t, _ = next_token(code, ce)
        if t in ('=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>=', '++', '--'):
            return t
        p, _ = prev_token(code, cs)
        if p in ('++', '--'):
            return p
        if p == '&':
            q, _ = prev_token(code, _)
            if q in ('', '(', ',', '=', 'return', '{', '?', ':'):
                return 'addr'
        return None

    @staticmethod
    def value_use_direct(code, cs, ce):
        """The value is converted or tested directly: its type cannot matter."""
        t, _ = next_token(code, ce)
        p, pj = prev_token(code, cs)
        if p in ('==', '!='):
            return t in (')', ';', ',', '&&', '||', '?', ']')
        if t in ('==', '!='):
            return p in ('(', ',', '=', 'return', '&&', '||', '!', '?', ':', '{', ';')
        if t not in (')', ';', ',', '&&', '||', '?', ']', '}'):
            return False
        if p == '=':
            # not `auto x = ...`
            ls = max(code.rfind(';', 0, pj), code.rfind('{', 0, pj), code.rfind('}', 0, pj)) + 1
            return not re.search(r'\bauto\b', code[ls:pj]) and t in (';', ',', ')')
        if p == 'return':
            return t == ';'
        if p in ('&&', '||', '!'):
            return True
        if p == '(' and t == ')':
            kw, _ = prev_token(code, pj)
            return kw in ('if', 'while', 'switch')
        if p == '[' and t == ']':
            return True
        return False

    # ---- running a file -----------------------------------------------------

    def sees_symbols(self, f):
        return any('bof3/symbols.gen.h' in g.includes for g in [f] + dc.include_closure(f, self.by_rel))

    def sites(self, f):
        out = []
        header = self.sees_symbols(f)
        for s, e, v, name in self.refs(f):
            st = self.analyse(f, s, e, v, name)
            if not header and st.reason is None:
                st.reason = 'no-symbols-header'
            out.append(st)
        # a reference inside another site's typed span is consumed by it when
        # the outer one is typed (its address argument); nested writes keep
        # their value span rendered
        return out

    @staticmethod
    def render(f, sites, active):
        """f's text with the chosen tier of every active site applied. An edit
        whose span holds another's (a write's value, a dynamic index) renders
        that one inside its hole; an edit overlapping another otherwise is
        dropped."""
        edits = []
        for st in sites:
            if st in active:
                ch = st.choice()
                if ch is not None:
                    edits.append(ch)
        edits.sort(key=lambda x: (x[0], -x[1]))

        def holes(ed):
            return [p for p in ed[2] if isinstance(p, tuple)]

        kept = []
        for ed in edits:
            ok = True
            for k in kept:
                if ed[0] < k[1] and k[0] < ed[1]:
                    if any(h[0] <= ed[0] and ed[1] <= h[1] for h in holes(k)):
                        continue
                    ok = False
                    break
            if ok:
                kept.append(ed)
        raw = f.raw

        gap = re.compile(r'^([^\n/]*?)( {2,})//')

        def plain(chunk, state):
            # a comment aligned after the code keeps its column: the spaces
            # before it absorb what the edits on its line grew or shrank by
            if state[0]:
                nl = chunk.find('\n')
                head = chunk if nl < 0 else chunk[:nl]
                m = gap.match(head)
                if m:
                    n = max(1, len(m.group(2)) - state[0])
                    chunk = m.group(1) + ' ' * n + chunk[m.end(2):]
                    state[0] = 0
                if nl >= 0:
                    state[0] = 0
            return chunk

        def emit(a, b, pool, state):
            inside = [e for e in pool if a <= e[0] and e[1] <= b]
            top = [e for e in inside if not any(o is not e and o[0] <= e[0] and e[1] <= o[1] for o in inside)]
            top.sort(key=lambda x: x[0])
            out, i = [], a
            for ed in top:
                out.append(plain(raw[i:ed[0]], state))
                sub = [x for x in inside if x is not ed and ed[0] <= x[0] and x[1] <= ed[1]]
                rep = ''.join(emit(p[0], p[1], sub, [0]) if isinstance(p, tuple) else p for p in ed[2])
                out.append(rep)
                if '\n' not in rep and '\n' not in raw[ed[0]:ed[1]]:
                    state[0] += len(rep) - (ed[1] - ed[0])
                i = ed[1]
            out.append(plain(raw[i:b], state))
            return ''.join(out)

        return emit(0, len(raw), kept, [0])


# --------------------------------------------------------------------------
# The gate
# --------------------------------------------------------------------------

def shutil_which(n):
    import shutil
    return shutil.which(n)


class Gate:
    def __init__(self, base, build):
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import codegen_same as cg
        self.cg = cg
        self.objdump = cg.find_objdump()
        self.base, self.build = base, build
        from pathlib import Path
        self.base_objs = cg.objects(Path(base))
        self.new_objs = cg.objects(Path(build))
        self.base_cache = {}
        self.ninja = shutil_which('ninja') or shutil_which('ninja.exe')
        if not self.ninja:
            sys.exit('typed_convert: --gate needs ninja on PATH (the preset\'s generator)')

    def objs_for(self, rel):
        keys = [k for k in self.new_objs if k.endswith('/' + rel + '.obj')]
        return keys

    def compile(self, keys):
        # ninja (the preset's generator) builds an object by its path
        targets = ['CMakeFiles/' + k for k in keys]
        r = subprocess.run([self.ninja, '-C', self.build] + targets, capture_output=True, text=True)
        return r.returncode == 0, r.stdout + r.stderr

    def same(self, keys):
        for k in keys:
            if k not in self.base_objs:
                return False
            if k not in self.base_cache:
                self.base_cache[k] = self.cg.disasm(self.objdump, self.base_objs[k])
            new = self.cg.disasm(self.objdump, self.new_objs[k])
            if new != self.base_cache[k]:
                return False
        return True

    def test(self, rel, text, keys):
        path = os.path.join(ROOT, rel)
        with open(path, 'w', encoding='utf-8', newline='') as fh:
            fh.write(text)
        ok, log = self.compile(keys)
        if not ok:
            return False
        return self.same(keys)


def gate_file(conv, gate, f, sites, verbose=False):
    """Choose each site's tier so f's objects compile to the baseline's."""
    keys = gate.objs_for(f.rel)
    if not keys:
        print(f'  {f.rel}: no object; converting nothing', file=sys.stderr)
        for st in sites:
            if st.reason is None:
                st.reason = 'no-object'
        return Converter.render(f, sites, set())
    live = [st for st in sites if st.choice() is not None]

    def attempt(active):
        return gate.test(f.rel, Converter.render(f, sites, set(active)), keys)

    tests = [0]

    def ok(active):
        tests[0] += 1
        return attempt(active)

    accepted = []
    pending = list(live)
    rounds = 0
    while pending and rounds < 6:
        rounds += 1
        if ok(accepted + pending):
            accepted += pending
            pending = []
            break
        bad = find_bad(ok, accepted, pending)
        good = [x for x in pending if x not in bad]
        accepted += good
        pending = []
        for st in bad:
            st.tier += 1
            if st.tier < len(st.tiers):
                pending.append(st)
            else:
                st.reason = 'gate'
    for st in pending:
        st.reason = 'gate'
    text = Converter.render(f, sites, set(accepted))
    if not gate.test(f.rel, text, keys):
        # should not happen: fall back to the original
        print(f'  {f.rel}: accepted set fails; reverting the file', file=sys.stderr)
        for st in accepted:
            st.reason = 'gate'
        text = f.raw
        gate.test(f.rel, text, keys)
    if verbose:
        print(f'  {f.rel}: {len(accepted)} kept, {sum(1 for s in live if s.reason == "gate")} gate, '
              f'{tests[0]} builds', file=sys.stderr)
    return text


def find_bad(ok, base, cand):
    """The members of cand that make ok(base + subset) fail, assuming the
    failures are independent."""
    if not cand:
        return []
    if len(cand) == 1:
        return [] if ok(base + cand) else list(cand)
    mid = len(cand) // 2
    a, b = cand[:mid], cand[mid:]
    if ok(base + a):
        bad_a = []
        good_a = a
    else:
        bad_a = find_bad(ok, base, a)
        good_a = [x for x in a if x not in bad_a]
    if ok(base + good_a + b):
        return bad_a
    return bad_a + find_bad(ok, base + good_a, b)


# --------------------------------------------------------------------------
# Retiring module constants
# --------------------------------------------------------------------------

# a constexpr statement of k-constants, possibly over several lines, and the
# comment after it on its last line
CONST_STMT = re.compile(r'(?m)^([ \t]*)((?:inline\s+)?(?:static\s+)?constexpr\s+[\w:]+(?:\s+(?:int|long|short|char))?)\s+'
                        r'(k\w*\s*(?:=[^;{}]*?|\{[^{};,]*\}))\s*;[ \t]*(?://[ \t]?([^\n]*))?$')
NOTE = re.compile(r'^[ 	]*// (k\w+): now \w+ \(symbols\.toml\)')


def retire(conv, dry_run=False, since=None):
    """Retire each data-address constant whose name nothing that can see it
    still spells, code or comment, outside its own definition: the files
    that include its file (and the file itself), and any other file's line
    that names its module beside it. Only constants of a `*_callees.h` or a
    `.cpp` whose value is a typed entry's address, and with `since`, only
    those something used at that revision (the constants this stage left
    unused, not ones already dead); never a name that reads as a bound or a
    size (`kTableEnd`, `kRowCount`), whose value only happens to be an
    entry's address. The definition becomes the note `// kX: now Name
    (symbols.toml).`, keeping its comment, so a comment above it still
    describes something."""
    files = conv.files
    old_text = {}

    def text_at(rel):
        if rel not in old_text:
            r = subprocess.run(['git', 'show', f'{since}:{rel}'], capture_output=True, cwd=ROOT)
            old_text[rel] = strip_notes(r.stdout.decode('utf-8', 'replace').replace('\r\n', '\n')) \
                if r.returncode == 0 else ''
        return old_text[rel]
    by_rel = conv.by_rel
    includers = collections.defaultdict(set)
    for f in files:
        for g in dc.include_closure(f, by_rel):
            includers[g.rel].add(f.rel)
    typed_pcs = {e['pc']: e for e in conv.entries.data if e.get('ctype')}
    removed = []

    def strip_notes(text):
        return '\n'.join(l for l in text.split('\n') if not NOTE.match(l))

    plain = {f.rel: strip_notes(f.raw) for f in files}
    for f in files:
        if not (f.rel.endswith('_callees.h') or f.rel.endswith('.cpp')):
            continue
        raw = f.raw
        seers = {f.rel} | includers[f.rel]
        stem = re.sub(r'(_callees|_fuzz)?\.\w+$', '', os.path.basename(f.rel))
        env = None
        edits = []
        for m in CONST_STMT.finditer(raw):
            body = m.group(3)
            if '//' in body or '/*' in body:
                continue          # a comment inside the statement: left alone
            brace = re.fullmatch(r'\s*(k\w*)\s*\{([^{};,]*)\}\s*', body)
            if brace:
                decls = [(brace.group(1), brace.group(2))]
            else:
                decls = dc.split_decls(body)
                if len(decls) != len(split_top(body, ',')):
                    continue
            if env is None:
                env = conv.env(f)
            stmt_text = m.group(0)
            gone = []
            for name, rhs in decls:
                v = dc.eval_expr(rhs, env)
                if v is None or v not in typed_pcs:
                    continue
                pat = re.compile(r'(?<![\w])' + re.escape(name) + r'(?![\w])')
                uses = 0
                for r in seers:
                    n = len(pat.findall(plain[r]))
                    if r == f.rel:
                        n -= len(pat.findall(stmt_text))
                    uses += n
                if uses == 0:
                    for g in files:
                        if g.rel in seers or name not in plain[g.rel]:
                            continue
                        for l2 in plain[g.rel].split('\n'):
                            if pat.search(l2) and stem in l2:
                                uses += 1
                if uses == 0 and BAD_BARE_NAME.search(name):
                    uses = 1
                if uses == 0 and since:
                    # used at `since`: in a file that sees this one, beyond its definition
                    was = 0
                    for r in seers:
                        n = len(pat.findall(text_at(r)))
                        if r == f.rel:
                            n -= 1
                        was += n
                    if was <= 0:
                        uses = 1
                if uses == 0:
                    gone.append((name, v))
            if not gone:
                continue
            indent, head, comment = m.group(1), m.group(2), m.group(4)
            names_gone = {n for n, _ in gone}
            for name, v in gone:
                removed.append((f.rel, dc.line_of(f.starts, m.start()), name, v, typed_pcs[v]['name']))
            keep = [(n, r) for n, r in decls if n not in names_gone]
            out = [f'{indent}// {n}: now {typed_pcs[v]["name"]} (symbols.toml).' for n, v in gone]
            if keep:
                parts = [f'{n} = {" ".join(r.split())}' for n, r in keep]
                one = f'{indent}{head} ' + ', '.join(parts) + ';'
                if len(one) > 110 and len(parts) > 1:
                    cont = ' ' * (len(indent) + len(head) + 1)
                    one = f'{indent}{head} ' + (',\n' + cont).join(parts) + ';'
                if comment:
                    one += '  // ' + comment.strip()
                out.append(one)
            elif comment:
                out[-1] += ' ' + comment.strip()
            edits.append((m.start(), m.end(), '\n'.join(out)))
        if edits and not dry_run:
            text = raw
            for a, b, t in reversed(edits):
                text = text[:a] + t + text[b:]
            with open(os.path.join(ROOT, f.rel), 'w', encoding='utf-8', newline='') as fh:
                fh.write(text)
    return removed


# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------

LEFT_HDR = ['file', 'line', 'address', 'entry', 'accessor', 'constant', 'reason']


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('files', nargs='*', help='files (or globs) under src/ to convert; default: every file')
    ap.add_argument('--toml', default='symbols.toml')
    ap.add_argument('--left', help='append the sites left raw to this TSV (written fresh with --left-new)')
    ap.add_argument('--left-new', action='store_true')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--gate', metavar='BASE_BUILD', help='verify each file against this baseline build')
    ap.add_argument('--build', default='build')
    ap.add_argument('--retire', action='store_true', help='remove module constants nothing uses any more')
    ap.add_argument('--since', help='with --retire: only constants something used at this git revision')
    ap.add_argument('--report', action='store_true', help='print each site and its outcome')
    ap.add_argument('-v', '--verbose', action='store_true')
    a = ap.parse_args()

    conv = Converter(a.toml)
    if a.retire:
        removed = retire(conv, a.dry_run, a.since)
        for r in removed:
            print(f'retire\t{r[0]}:{r[1]}\t{r[2]}\t{r[3]:#x}\t{r[4]}')
        print(f'typed_convert: {len(removed)} module constants removed', file=sys.stderr)
        return 0

    pats = a.files or ['src/**']
    targets = [f for f in conv.files if any(fnmatch.fnmatch(f.rel, p) or f.rel == p for p in pats)]
    gate = Gate(os.path.abspath(a.gate), os.path.abspath(a.build)) if a.gate else None
    stats = collections.Counter()
    left = []
    for f in targets:
        sites = conv.sites(f)
        if not sites:
            continue
        if gate and not a.dry_run:
            text = gate_file(conv, gate, f, sites, a.verbose)
        else:
            text = Converter.render(f, sites, set(sites))
            if not a.dry_run and text != f.raw:
                with open(os.path.join(ROOT, f.rel), 'w', encoding='utf-8', newline='') as fh:
                    fh.write(text)
        for st in sites:
            ch = st.choice()
            if st.reason is not None or ch is None:
                reason = st.reason or 'none'
                stats['left:' + reason] += 1
                ent = st.entry['name'] if st.entry else '-'
                if st.entry and st.off:
                    ent += f'+{st.off:#x}'
                left.append((f.rel, st.line, f'{st.value:#x}' if st.value is not None else '-', ent, st.acc,
                             st.cname, reason))
            else:
                stats[ch[3]] += 1
                if ch[3] == 'addr':
                    stats['addr:' + st.kind] += 1
                stats['converted'] += 1
            if a.report:
                print(f'{f.rel}:{st.line}\t{st.cname}\t{st.acc}\t{st.kind}\t'
                      f'{(ch[3] + " " + "".join(p if isinstance(p, str) else "<..>" for p in ch[2])) if ch and st.reason is None else "LEFT " + str(st.reason)}')
    if a.left:
        new = a.left_new or not os.path.exists(a.left)
        with open(a.left, 'w' if new else 'a', encoding='utf-8', newline='\n') as fh:
            if new:
                fh.write('# tools/typed_convert.py: global sites left raw in stage B0, with the reason. '
                         'Addresses and names only.\n')
                fh.write('#' + '\t'.join(LEFT_HDR) + '\n')
            for row in left:
                fh.write('\t'.join(str(x) for x in row) + '\n')
    for k, v in sorted(stats.items()):
        print(f'{k}\t{v}', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
