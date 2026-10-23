#!/usr/bin/env python
"""Map the US (SLUS_004.22) function starts of a splat-based decompilation onto the JP (SLPS_009.90) overlays, then onto the PC.

    python -I twinmap.py --decomp <decomp root> --x <dir with us/ and jp/ overlays> \
        --repo C:/Users/kerok/Documents/GitHub/BreathOfFire3PCPort --out analysis/region-twins

Method: both regions were compiled by the same compiler from nearly the same source, so a function's
instruction words agree once address halves (lui/addiu/ori and memory offsets off a lui'd base),
jal/j targets are masked. Each US function (boundaries from the decompilation's splat.yaml) is searched as a
masked word string in the masked JP section, monotonically. Outputs:

    us_jp.tsv   target, us, size, kind(c/asm), name, jp, status(exact/none/gap)
    pc_map.tsv  pc, jp, us, target, kind, how
"""
import argparse, collections, glob, hashlib, json, os, re, struct, sys
import yaml

TARGETS = {
    'emi/battle/battle/03': ('BATTLE.EMI', 3), 'emi/battle/battle/15': ('BATTLE.EMI', 15),
    'emi/etc/game/00': ('GAME.EMI', 0), 'emi/etc/game/01': ('GAME.EMI', 1),
    'emi/etc/start/08': ('START.EMI', 8), 'emi/etc/start/09': ('START.EMI', 9),
    'emi/etc/shop/00': ('SHOP.EMI', 0), 'emi/etc/sisyou/00': ('SISYOU.EMI', 0),
    'emi/etc/commu00/00': ('COMMU00.EMI', 0),
    'emi/world00/area030/04': ('AREA030.EMI', 4), 'emi/world00/area030/05': ('AREA030.EMI', 5),
    'emi/scenario/sce10eff/00': ('SCE10EFF.EMI', 0), 'emi/scenario/scena00/00': ('SCENA00.EMI', 0),
    'emi/scenario/scena16/00': ('SCENA16.EMI', 0),
    'emi/world00/area008/13': ('AREA008.EMI', 13), 'emi/world00/area016/13': ('AREA016.EMI', 13),
    'emi/world00/area024/14': ('AREA024.EMI', 14), 'emi/world00/area026/13': ('AREA026.EMI', 13),
    'emi/world00/area027/13': ('AREA027.EMI', 13), 'emi/world00/area028/13': ('AREA028.EMI', 13),
    'emi/world00/area032/13': ('AREA032.EMI', 13),
    'exe/slus_004_22': ('EXE', None),
}


def emi_sections(path):
    blob = open(path, 'rb').read()
    assert blob[8:16] == b'MATH_TBL', path
    count, = struct.unpack_from('<I', blob, 0)
    out, pos = [], 0x800
    for i in range(count):
        size, dest = struct.unpack_from('<II', blob, 0x10 + 16 * i)
        out.append((i, dest, blob[pos:pos + size]))
        pos += ((size + 0x7FF) >> 11) * 0x800
    return out


def exe_image(path):
    blob = open(path, 'rb').read()
    dest, = struct.unpack_from('<I', blob, 0x18)
    return dest, blob[0x800:]


def section(xdir, region, name, idx):
    if name == 'EXE':
        return exe_image(os.path.join(xdir, region, 'SLPS_009.90' if region == 'jp' else 'SLUS_004.22'))
    for i, dest, data in emi_sections(os.path.join(xdir, region, name)):
        if i == idx:
            return dest, data
    raise SystemExit('no section %d in %s' % (idx, name))


def mask_words(data):
    """Masked instruction words: address halves and call targets zeroed, the rest kept."""
    n = len(data) // 4
    words = struct.unpack_from('<%dI' % n, data, 0)
    out = bytearray(n * 4)
    tainted = [False] * 32
    for i, w in enumerate(words):
        op = w >> 26
        rs, rt = (w >> 21) & 31, (w >> 16) & 31
        m = w
        if op in (2, 3):                       # j / jal
            m = op << 26
        elif op == 15:                         # lui
            imm = w & 0xFFFF
            if 0x8000 <= imm <= 0x8020 or imm == 0x1F80:
                m = w & 0xFFFF0000
                tainted[rt] = True
            else:
                tainted[rt] = False
        elif op in (9, 13) and tainted[rs]:    # addiu / ori off a lui'd base
            m = w & 0xFFFF0000
            tainted[rt] = (rt == rs) or True
        elif op in (8, 9, 10, 11, 12, 13, 14):  # other immediates: dest no longer an address
            tainted[rt] = tainted[rs] if op in (9, 13) else False
        elif op >= 32 and op <= 46 or op in (49, 50, 57, 58):  # loads / stores
            if tainted[rs]:
                m = w & 0xFFFF0000
            if op <= 37 or op in (49, 50):     # a load overwrites rt
                tainted[rt] = False
        elif op == 0:                          # special: dest rd; addu/or carry an address base on
            rd = (w >> 11) & 31
            fn = w & 63
            if fn in (0x20, 0x21, 0x25, 0x22, 0x23):
                tainted[rd] = tainted[rs] or tainted[rt]
            else:
                tainted[rd] = False
        tainted[0] = False
        struct.pack_into('<I', out, i * 4, m)
    return bytes(out)


def splat_functions(path):
    cfg = yaml.safe_load(open(path))
    rows = []
    for seg in cfg['segments']:
        if not isinstance(seg, dict) or seg.get('type') != 'code' or 'subsegments' not in seg:
            continue
        base, vram = seg['start'], seg['vram']
        for sub in seg['subsegments']:
            if isinstance(sub, list) and len(sub) >= 3:
                start, kind, name = sub[0], sub[1], sub[2]
                rows.append((vram + start - base, kind, name))
    rows.sort()
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--decomp', required=True); ap.add_argument('--x', required=True)
    ap.add_argument('--repo', required=True); ap.add_argument('--out', required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    us_jp = []
    stats = collections.Counter()
    jp_sections = {}
    for target, (name, idx) in TARGETS.items():
        splat = os.path.join(a.decomp, 'config', target, 'splat.yaml')
        if not os.path.exists(splat):
            print('no splat for', target); continue
        us_base, us_data = section(a.x, 'us', name, idx)
        jp_base, jp_data = section(a.x, 'jp', name, idx)
        jp_sections[(name, idx)] = (jp_base, jp_data)
        us_m, jp_m = mask_words(us_data), mask_words(jp_data)
        rows = splat_functions(splat)
        # matched = a MATCH body in the decompilation's src tree (the splat kinds lag the tree)
        matched = {}
        for cpath in glob.glob(os.path.join(a.decomp, 'src', target, '*.c')):
            txt = open(cpath, encoding='utf-8', errors='replace').read()
            for m in re.finditer(r'^MATCH\s[^\n]*?\bfunc_([0-9A-Fa-f]{8})\s*\(', txt, re.M):
                matched[int(m.group(1), 16)] = os.path.relpath(cpath, a.decomp).replace('\\', '/')
        rows = [(addr, ('c' if addr in matched else ('asm' if k == 'c' else k)), (matched.get(addr, nm) if addr in matched else nm)) for addr, k, nm in rows]
        funcs = [(addr, kind, nm) for addr, kind, nm in rows if kind in ('c', 'asm', 'hasm')]
        ends = {}
        for i, (addr, kind, nm) in enumerate(rows):
            nxt = rows[i + 1][0] if i + 1 < len(rows) else us_base + len(us_data)
            ends[addr] = nxt
        pos = 0
        found = []
        for addr, kind, nm in funcs:
            off, end = addr - us_base, ends[addr] - us_base
            pat = us_m[off:end]
            # trim trailing nops (padding) from the pattern
            while len(pat) >= 8 and pat[-4:] == b'\0\0\0\0':
                pat = pat[:-4]
            jp_off, st = None, 'none'
            if len(pat) >= 8:
                p = jp_m.find(pat, pos)
                while p != -1 and p % 4:
                    p = jp_m.find(pat, p + 1)
                if p == -1:
                    p = jp_m.find(pat)
                    while p != -1 and p % 4:
                        p = jp_m.find(pat, p + 1)
                    if p != -1:
                        st = 'exact-outoforder'
                else:
                    st = 'exact'
                if p != -1:
                    jp_off = p
                    if st == 'exact':
                        pos = p + len(pat)
            found.append([target, addr, end - off, kind, nm, jp_off, st])
        # gap pass: an unmatched function between two exact neighbours whose JP gap has the same size
        for i, row in enumerate(found):
            if row[6] != 'none':
                continue
            prev = next((r for r in reversed(found[:i]) if r[6] == 'exact'), None)
            nxt = next((r for r in found[i + 1:] if r[6] == 'exact'), None)
            if prev and nxt:
                # expected JP start = prev.jp + (us - prev.us) if the two gaps agree in size
                us_gap = nxt[1] - prev[1]
                jp_gap = nxt[5] - prev[5]
                if us_gap == jp_gap:
                    row[5], row[6] = prev[5] + (row[1] - prev[1]), 'gap-same'
                else:
                    row[5], row[6] = prev[5] + (row[1] - prev[1]), 'gap-shifted(%+d)' % (jp_gap - us_gap)
        for row in found:
            if row[5] is not None:
                row[5] = jp_base + row[5]
            stats[(target, row[6].split('(')[0])] += 1
        us_jp.extend(found)
        n = len(found); ex = sum(1 for r in found if r[6] == 'exact')
        print('%-26s %4d functions, %4d exact, %3d c-files' % (target, n, ex, sum(1 for r in found if r[3] == 'c')))
    with open(os.path.join(a.out, 'us_jp.tsv'), 'w') as f:
        f.write('target\tus\tsize\tkind\tname\tjp\tstatus\n')
        for t, us, sz, k, nm, jp, st in us_jp:
            f.write('%s\t0x%08X\t%d\t%s\t%s\t%s\t%s\n' % (t, us, sz, k, nm, '0x%08X' % jp if jp else '', st))

    # ---- PC side -------------------------------------------------------------------------
    jp_to_us = {}
    for t, us, sz, k, nm, jp, st in us_jp:
        if jp and st.startswith(('exact', 'gap-same')):
            jp_to_us.setdefault(jp, []).append((t, us, k, nm, st))
    pc_rows = []
    # A takeover's marker: `// original 0xPC (... PSX 0xJP ...)`. The PSX address is not always
    # right after the paren (`(area 98 +0x34[5]; PSX 0x801F5B50)`, `(+0x3C[10], PSX ...)`,
    # `0x56C130: chapter 17's vtable slot 0 (PSX ...)`), so take the first on the marker's line.
    rx = re.compile(r'//\s*original\s+0x([0-9A-Fa-f]{6})\b[^\n]*?\bPSX\s+0x(80[0-9A-Fa-f]{6})')
    for path in glob.glob(os.path.join(a.repo, 'src', '**', '*.cpp'), recursive=True) + \
            glob.glob(os.path.join(a.repo, 'src', '**', '*.h'), recursive=True):
        rel = os.path.relpath(path, a.repo).replace(chr(92), chr(47))
        area = None
        for ln, line in enumerate(open(path, encoding='utf-8', errors='replace'), 1):
            # The area files head each area's block with `// ---- area N (`, and a marker may say
            # `(area 98 ...)` itself; the area decides which AREAnnn overlay the twin lives in.
            ma = re.match(r'//\s*-{3,}\s*area\s+(\d+)\b', line, re.I)
            if ma:
                area = int(ma.group(1))
            m = rx.search(line)
            if m:
                mb = re.search(r'\(area\s+(\d+)\b', line)
                where = ('AREA%03d.EMI' % int(mb.group(1))) if mb else (('AREA%03d.EMI' % area) if area is not None else None)
                pc_rows.append((int(m.group(1), 16), int(m.group(2), 16), 'src:%s:%d' % (rel, ln), where))
    md5s = {hashlib.md5(d).hexdigest(): (nm, i) for (nm, i), (b, d) in jp_sections.items()}
    for p in json.load(open(os.path.join(a.repo, 'analysis', 'pairs_propagated.json'))):
        sec = p['section']
        where = 'boot' if sec == 'boot' else md5s.get(sec)
        pc_rows.append((p['pc'], p['psx'], 'pairs:%s:%s' % (p['how'], where if where else sec[:8]), where))
    seen = set()
    with open(os.path.join(a.out, 'pc_map.tsv'), 'w') as f:
        f.write('pc\tjp\tus\ttarget\tkind\tname\tstatus\thow\n')
        for pc, jp, how, where in sorted(pc_rows, key=lambda r: r[:3]):
            if (pc, jp, how.split(':')[0]) in seen:
                continue
            seen.add((pc, jp, how.split(':')[0]))
            hits = jp_to_us.get(jp, [])
            # A propagated pair was found in one JP section. The area overlays put a different
            # function at the same address in every area, so a pair from AREA016 may only match
            # the US AREA016 target; a pair from a section the decompilation has not got (an
            # unresolved md5) matches nothing. The boot block ('boot') is the executable and GAME.EMI.
            if how.startswith('pairs:'):
                sec = how.rsplit(':', 1)[1]
                if sec == 'boot':
                    # The sibling's boot image holds the executable and the resident field
                    # overlay (GAME.EMI section 0) together.
                    hits = [h for h in hits if TARGETS[h[0]][0] in ('EXE', 'GAME.EMI')]
                elif where:
                    hits = [h for h in hits if TARGETS[h[0]] == where]
                else:
                    hits = []
            elif where:
                # A citation from an area file: an AREAnnn twin must be that area's own overlay;
                # twins in the shared overlays (GAME, the executable) stay.
                hits = [h for h in hits if not TARGETS[h[0]][0].startswith('AREA') or TARGETS[h[0]][0] == where]
            if hits:
                for t, us, k, nm, st in hits:
                    f.write('0x%06X\t0x%08X\t0x%08X\t%s\t%s\t%s\t%s\t%s\n' % (pc, jp, us, t, k, nm, st, how))
            else:
                f.write('0x%06X\t0x%08X\t\t\t\t\t\t%s\n' % (pc, jp, how))
    n_src = sum(1 for r in pc_rows if r[2].startswith('src'))
    hit_src = sum(1 for r in pc_rows if r[2].startswith('src') and r[1] in jp_to_us)
    hit_c = sum(1 for r in pc_rows if r[2].startswith('src') and any(h[2] == 'c' for h in jp_to_us.get(r[1], [])))
    print('PC: %d src citations, %d with a US twin in the decompilation\'s targets, %d of those matched C' % (n_src, hit_src, hit_c))
    n_p = sum(1 for r in pc_rows if r[2].startswith('pairs'))
    hit_p = sum(1 for r in pc_rows if r[2].startswith('pairs') and r[1] in jp_to_us)
    hit_pc = sum(1 for r in pc_rows if r[2].startswith('pairs') and any(h[2] == 'c' for h in jp_to_us.get(r[1], [])))
    print('PC: %d propagated pairs, %d with a US twin, %d matched C' % (n_p, hit_p, hit_pc))


if __name__ == '__main__':
    main()
