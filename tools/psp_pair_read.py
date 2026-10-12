#!/usr/bin/env python
"""Reading aids for stage B of the PSP code diff (docs/psp-code-diff-field-1.md).

    python tools/psp_pair_read.py rescore --psp-jp ISO --psp-eu ISO --diff <scratch>/diff.json --out <scratch>/rescore.json
    python tools/psp_pair_read.py phantom --psp-jp ISO --psp-eu ISO 801b1e3c:cbc68 [...]
    python tools/psp_pair_read.py dump    --psp-jp ISO --psp-eu ISO --diff <scratch>/diff.json --half field \
        --rows 1-454 --out <scratch>/rows/

Additive beside tools/psp_pair.py: it imports that module, rebuilds its pairing
(the same pairs, so the same rows), and changes nothing in it.

  rescore  holds the pairs fixed and rescores every `differs` row with two
           normaliser defects corrected, one at a time and together:
             ds  - a jal's delay slot is processed before the call clobbers the
                   caller-saved registers (psp_pair.extract does it after, so an
                   address half formed in the delay slot is counted as a value);
             res - an overlay's call into its own file's other sections or its
                   resident module (GAME.EMI, BATTLE.EMI) resolves there before
                   the boot image, whose bytes from 0x801a0000 on are zero
                   (.bss): psp_pair.Psx.resolve tries the boot range first.
           Prints, per half and per stage-A row range, how many rows leave
           `differs` under each fix.
  phantom  diffs a PSP function against the overlay functions at a PSX address
           (for the boot-image .bss 'functions' of no instructions that took a
           PSP twin by caller vote).
  dump     per row of a half's `differs` table (in the table's order): the
           annotated disassembly of both sides, and an aligned diff of the two
           functions' streams of calls and non-trivial constants (delay slots
           first, address halves set aside), with each differing chunk's lines.

The dump holds instruction text from the player's discs: it goes to scratch,
never into the tree (CLAUDE.md rule 1). rescore and phantom print addresses,
counts and scores only.
"""
import argparse, collections, difflib, inspect, json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import psp_pair as pp


def _ds_order(lo, hi, space):
    """Instruction addresses with each jal / jalr's delay slot before the call."""
    out, x = [], lo
    while x < hi:
        w = space.word(x)
        if (w >> 26 == 3 or (w >> 26 == 0 and w & 63 == 9)) and x + 4 < hi:
            out += [x + 4, x]
            x += 8
        else:
            out.append(x)
            x += 4
    return out


def _make_extract_ds():
    src = inspect.getsource(pp.extract).replace('def extract(', 'def extract_ds(')
    old = '    for x in range(f.start, f.end, 4):\n        w = space.word(x)'
    assert old in src, 'psp_pair.extract changed shape; update psp_pair_read'
    src = src.replace(old, '    for x in _ds_order(f.start, f.end, space):\n        w = space.word(x)')
    pp._ds_order = _ds_order
    ns = pp.__dict__
    exec(compile(src, 'extract_ds', 'exec'), ns)
    return ns['extract_ds']


class Ctx:
    def __init__(self, a):
        self.psx, self.AF, self.jp, self.B, self.m, self.full, self.j2e, self.eu, self.E = pp.build(a, log=lambda s: None)
        self.plat = pp.platform_layer(self.m, self.psx, self.jp, self.B)
        self.extract_ds = _make_extract_ds()
        self.orig_resolve = self.psx.resolve

    def resolve_fixed(self, space, t):
        psx = self.psx
        sp = psx.spaces[space]
        if space != 'boot' and sp.lo <= t < sp.hi:
            return (space, t)
        if space != 'boot':
            src = psx.caps[space]['source_file']
            fam = src.split('/')[1]
            resident = 'BIN/BATTLE/BATTLE.EMI' if fam in pp.BATTLE_FAMILIES else 'BIN/ETC/GAME.EMI'
            for f in (src, resident):
                for mm in psx.by_file[f]:
                    o = psx.spaces[mm]
                    if mm != space and o.lo <= t < o.hi and (mm, t) in self.AF:
                        return (mm, t)
        return self.orig_resolve(space, t)

    def rescore(self, r, b, ds, res):
        m, psx = self.m, self.psx
        fa0, fb0 = m.A[r], m.B[b]
        ex = self.extract_ds if ds else pp.extract
        rs = self.resolve_fixed if res else self.orig_resolve
        fa = pp.Func(fa0.key, fa0.space, fa0.start, fa0.end)
        fb = pp.Func(fb0.key, fb0.space, fb0.start, fb0.end)
        ex(fa, psx.spaces[fa0.space], False, lambda t, sp=fa0.space: rs(sp, t) or ('?', t),
           lambda x, sp=fa0.space: rs(sp, x) in self.AF)
        fa.calls = [m.rep_a.get(c, c) for c in fa.calls]
        fa.callargs = [(m.rep_a.get(c, c), g) for c, g in fa.callargs]
        ex(fb, self.jp.space, True, lambda t: t, lambda x: x in self.B)
        fb.calls = [m.rep_b.get(c, c) for c in fb.calls]
        fb.stubs = fb0.stubs
        m.A[r], m.B[b] = fa, fb
        try:
            return pp.diff_pair(m, psx, self.jp, r, b, self.plat)
        finally:
            m.A[r], m.B[b] = fa0, fb0


def table_rows(diff, half):
    """A half's `differs` rows in the doc table's order (psp_pair.write_md)."""
    d = [x for x in json.load(open(diff))['rows'] if x['half'] == half and x['verdict'] == 'differs']
    d.sort(key=lambda x: (not x['name'], -x['score'], x['file'], x['psx']))
    return d


def key_of(s):
    sp, ad = s.split(':')
    return (sp, int(ad, 16))


def cmd_rescore(a):
    c = Ctx(a)
    out = []
    for half in ('field', 'battle'):
        for i, x in enumerate(table_rows(a.diff, half), 1):
            k = key_of(x['psx'])
            r, b = c.m.rep_a.get(k, k), int(x['psp_jp'], 16)
            rec = dict(half=half, row=i, psx=x['psx'], score=x['score'])
            for tag, ds, res in (('base', False, False), ('ds', True, False), ('res', False, True), ('both', True, True)):
                kinds, det, sc = c.rescore(r, b, ds, res)
                rec[tag], rec[tag + '_v'] = sc, pp.verdict(kinds, sc)
                if tag == 'both':
                    rec['det'] = det
            out.append(rec)
    json.dump(out, open(a.out, 'w'), indent=0)
    for half in ('field', 'battle'):
        sel = [r for r in out if r['half'] == half]
        # the field half's table is read in two shares (CD2 rows 1..454, CD3 the rest)
        for lo, hi in ((1, 454), (455, 10 ** 6), (1, 10 ** 6)) if len(sel) > 454 else ((1, 10 ** 6),):
            s = [r for r in sel if lo <= r['row'] <= hi]
            if s:
                print('%-6s rows %d..%d (%d): base reproduces stage A on %d; leave differs under ds %d, res %d, both %d'
                      % (half, lo, min(hi, len(sel)), len(s), sum(1 for r in s if r['base'] == r['score']),
                         *(sum(1 for r in s if r[t + '_v'] != 'differs') for t in ('ds', 'res', 'both'))))
    print('wrote', a.out)


def cmd_phantom(a):
    c = Ctx(a)
    for arg in a.pairs:
        ad, b = (int(v, 16) for v in arg.split(':'))
        hits = [k for k in c.AF if k[1] == ad and k[0] != 'boot']
        if not hits:
            print('%x: no overlay function at this address' % ad)
        for k in hits:
            r = c.m.rep_a.get(k, k)
            if r not in c.m.A:
                print('%x %s: not a class representative' % (ad, c.psx.file_of(k[0])))
                continue
            kinds, det, sc = pp.diff_pair(c.m, c.psx, c.jp, r, b, c.plat)
            print('%x %s n=%d vs PSP %x n=%d (its own twin: %s): %s, score %d; %s' % (
                ad, c.psx.file_of(k[0]), c.AF[k].n, b, c.B[b].n,
                '%x' % c.m.a2b[r] if r in c.m.a2b else 'unpaired', pp.verdict(kinds, sc), sc, det))


# ---------------------------------------------------------------- dump

def cmd_dump(a):
    import capstone
    c = Ctx(a)
    m, psx, jp, B, plat = c.m, c.psx, c.jp, c.B, c.plat
    md = capstone.Cs(capstone.CS_ARCH_MIPS, capstone.CS_MODE_MIPS32 | capstone.CS_MODE_LITTLE_ENDIAN)
    name = lambda k: psx.names.get(k, ('',))[0]

    def psx_call(space, t):
        k = c.resolve_fixed(space, t)
        r = m.rep_a.get(k, k) if k else None
        return k, r

    def events(f, space, is_psp):
        ev, regs = [], {}
        for x in _ds_order(f.start, f.end, space):
            w = space.word(x)
            op, rs, rt, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
            if op == 3:
                if is_psp:
                    t = (w & 0x3FFFFFF) << 2
                    r = m.rep_b.get(t, t)
                    tok = 'PLAT' if (jp.is_stub(t) or t in plat or r in plat) else (
                        name(m.b2a[r]) or '%x' % r) if r in m.b2a else '?'
                else:
                    t = ((x + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
                    k, r = psx_call(space.name, t)
                    tok = 'PLAT' if r in psx.lib else (name(k) or name(r) or '%x' % m.a2b[r]) if r in m.a2b else '?'
                ev.append((x, 'call ' + tok))
                for q in pp.CALLER_SAVED:
                    regs.pop(q, None)
                continue
            if op == 0x0F:
                isaddr = (x in space.addr_hi) if is_psp else (0x8000 <= imm <= 0x8020 or imm == 0x1F80)
                if isaddr:
                    regs[rt] = ('A',)
                else:
                    regs[rt] = ('K', imm << 16, len(ev))
                    ev.append((x, imm << 16))
                continue
            if op in (0x08, 0x09, 0x0D) and rs in regs and regs[rs][0] == 'A':
                regs[rt] = ('A',)
                continue
            if is_psp and x in space.addr_lo:
                regs.pop(rt, None)
                continue
            if op in (0x09, 0x0D) and rs in regs and regs[rs][0] == 'K':
                v = (regs[rs][1] | imm) if op == 0x0D else (regs[rs][1] + pp.simm(w)) & 0xFFFFFFFF
                i = regs[rs][2]
                ev[i] = (ev[i][0], pp.const_norm(v))
                regs[rt] = ('K', v, i)
                continue
            if 0x08 <= op <= 0x0E:
                if pp.SP not in (rs, rt):
                    v = imm if op in (0x0C, 0x0D, 0x0E) else pp.simm(w)
                    if v not in pp.TRIVIAL and not (is_psp and pp.psx_ram_offset(v)):
                        ev.append((x, v & 0xFFFFFFFF))
                regs.pop(rt, None)
                continue
            if op in pp.LOADS:
                regs.pop(rt, None)
                continue
            for q in pp.written(w):
                regs.pop(q, None)
        return ev

    def tok(e):
        if isinstance(e, str):
            return e
        s = e - (1 << 32) if e >= 0x80000000 else e
        return 'K %d' % s if -1000 < s < 1000 else 'K %#x' % e

    def equiv(u, v):
        if u == v:
            return True
        if isinstance(u, int) and isinstance(v, int):
            p = u - (1 << 32) if u >= 0x80000000 else u
            q = v - (1 << 32) if v >= 0x80000000 else v
            return abs(p - q) == 1 or p == -q or (p - q) & 0xFFFF == 0
        return False

    def line(space, x, is_psp):
        w = space.word(x)
        ins = list(md.disasm(w.to_bytes(4, 'little'), x))
        t = ('%s %s' % (ins[0].mnemonic, ins[0].op_str)) if ins else '.word'
        note, op = '', w >> 26
        if op in (2, 3):
            t_ = ((x + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            if is_psp:
                r = m.rep_b.get(t_, t_)
                k = m.b2a.get(r)
                note = 'STUB' if jp.is_stub(t_) else ('PLAT ' if (t_ in plat or r in plat) else '') + (
                    'psx %s:%x %s' % (psx.file_of(k[0]).replace('BIN/', ''), k[1], name(k)) if k else
                    'unpaired n=%d' % (B[r].n if r in B else 0))
            else:
                k, r = psx_call(space.name, t_)
                note = ('LIB ' if r in psx.lib else '') + (name(k) or '') + (
                    ' -> psp %x' % m.a2b[r] if r in m.a2b else ' unpaired')
            if op == 2:
                note = 'j ' + note
        elif is_psp and (x in space.addr_lo or x in space.addr_hi):
            note = 'ADDR'
        return '  %08x  %-36s %s' % (x, t, note)

    lo, hi = (int(v) for v in a.rows.split('-'))
    os.makedirs(a.out, exist_ok=True)
    for i, x in enumerate(table_rows(a.diff, a.half), 1):
        if not lo <= i <= hi:
            continue
        k = key_of(x['psx'])
        r, b = m.rep_a.get(k, k), int(x['psp_jp'], 16)
        fa, fb = m.A[r], m.B[b]
        spa = psx.spaces[fa.space]
        ea, eb = events(fa, spa, False), events(fb, jp.space, True)
        ta, tb = [tok(e) for _, e in ea], [tok(e) for _, e in eb]
        sm = difflib.SequenceMatcher(a=ta, b=tb, autojunk=False)
        kinds, det, sc = c.rescore(r, b, True, True)
        out = ['row %d  %s %s  name=%s  PSP-JP %s EU %s  paired by %s  n=%d/%d  score %d (both fixes: %d)' % (
            i, x['psx'], x['file'], x['name'], x['psp_jp'], x['psp_eu'], x['how'], x['n_psx'], x['n_psp'],
            x['score'], sc), 'stage A: ' + x['detail'], 'both fixes: ' + det, '', '== event diff']
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == 'equal':
                continue
            A_, B_ = [e for _, e in ea[i1:i2]], [e for _, e in eb[j1:j2]]
            if len(A_) == len(B_) and all(equiv(p, q) for p, q in zip(A_, B_)):
                continue
            out.append('[%s] PSX %s: %s' % (tag, ' '.join('%x' % y for y, _ in ea[i1:i2][:1]) or '-', ', '.join(ta[i1:i2]) or '-'))
            out.append('       PSP %s: %s' % (' '.join('%x' % y for y, _ in eb[j1:j2][:1]) or '-', ', '.join(tb[j1:j2]) or '-'))
        out += ['', '== PSX %x..%x' % (fa.start, fa.end)] + [line(spa, y, False) for y in range(fa.start, fa.end, 4)]
        out += ['', '== PSP %x..%x' % (fb.start, fb.end)] + [line(jp.space, y, True) for y in range(fb.start, fb.end, 4)]
        open(os.path.join(a.out, '%03d.txt' % i), 'w').write('\n'.join(out) + '\n')
    print('wrote rows %d..%d to %s' % (lo, hi, a.out))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('rescore', 'phantom', 'dump'):
        p = sub.add_parser(name)
        p.add_argument('--psp-jp', required=True)
        p.add_argument('--psp-eu')
        p.add_argument('--sibling', default=pp.find_sibling())
        if name in ('rescore', 'dump'):
            p.add_argument('--diff', required=True, help="psp_pair.py diff's JSON")
            p.add_argument('--out', required=True)
        if name == 'dump':
            p.add_argument('--half', choices=('field', 'battle'), required=True)
            p.add_argument('--rows', default='1-100000')
        if name == 'phantom':
            p.add_argument('pairs', nargs='+', help='PSXADDR:PSPADDR in hex')
    a = ap.parse_args()
    a.check_eu = False
    if not a.sibling:
        sys.exit('no sibling checkout found; pass --sibling')
    if not a.sibling.endswith(('/', '\\')):
        a.sibling += os.sep
    dict(rescore=cmd_rescore, phantom=cmd_phantom, dump=cmd_dump)[a.cmd](a)


if __name__ == '__main__':
    main()
