#!/usr/bin/env python
"""Pair PSX functions with PC ones through the tables both builds kept.

    python tools/psx_pair.py areas     -> analysis/area_pairs.json
    python tools/psx_pair.py tables    -> analysis/table_matches.json

`areas`: the boot EXE's 200-entry area descriptor table (0x801802EC) has a PC
twin at 0x667590 (docs/attract-remaining.md section 5). Each descriptor's +0x3C
is a handler array and +0x40 an init function on both sides; the PSX side is the
sibling's names/area_records.toml. Pairs them slot for slot, then measures
whether each overlay's order survives on the PC and whether the gaps between
paired anchors hold as many function starts on both sides.

`tables`: every run of code pointers in the PC data and in the PSX images (boot
EXE plus the sibling's 406 overlay captures), matched by shape - each distinct
target numbered by first appearance, nulls and bare returns marked - as whole
tables or as a window of a longer run.

Reads the sibling checkout (--sibling). Output is derived from copyrighted game
code: it lives under analysis/ and is never committed (CLAUDE.md rule 1).
"""
import argparse, base64, bisect, collections, json, struct, tomllib
import pe_hidden

PC_AREA_TABLE = 0x667590   # 200 pointers to 0x44-byte area descriptors
PSX_BOOT_LO = 0x80093800   # SLPS_009.90 code, after its 0x800-byte header
PSX_BOOT_TEXT = 0x163800
AREAS = 200


def psx_area_records(sib):
    recs = tomllib.load(open(sib + 'names/area_records.toml', 'rb'))['record']
    section, slots = {}, collections.defaultdict(dict)
    for r in recs:
        section[r['area']] = r['section']
        if r['kind'].startswith('handler['):
            slots[r['area']][int(r['kind'][8:-1])] = int(r['entry'], 16)
        elif r['kind'] == 'init':
            slots[r['area']]['init'] = int(r['entry'], 16)
    return section, slots


def cmd_areas(a):
    img = pe_hidden.Image(a.exe)
    rd = img.dword
    section, psx = psx_area_records(a.sibling)
    pairs, disagree = [], []
    for k in range(AREAS):
        p = rd(PC_AREA_TABLE + 4 * k)
        h, init = (rd(p + 0x3C), rd(p + 0x40)) if p else (0, 0)
        n = 0
        while h and img.in_text(rd(h + 4 * n)):
            n += 1
        handlers = sorted(j for j in psx.get(k, {}) if j != 'init')
        if n != len(handlers) or bool(init) != ('init' in psx.get(k, {})):
            disagree.append(dict(area=k, pc_handlers=n, psx_handlers=len(handlers),
                                 pc_init=init, psx_init=psx.get(k, {}).get('init')))
            continue
        for j in handlers:
            pairs.append(dict(area=k, section=section[k], slot=j, psx=psx[k][j], pc=rd(h + 4 * j)))
        if init:
            pairs.append(dict(area=k, section=section[k], slot='init', psx=psx[k]['init'], pc=init))

    back = collections.defaultdict(set)
    keys = collections.defaultdict(set)
    for r in pairs:
        back[r['pc']].add((r['section'], r['psx']))
        keys[(r['section'], r['psx'])].add(r['pc'])
    print(f'{AREAS - len(disagree)} of {AREAS} areas agree on handler count and init; '
          f'{len(pairs)} pairs, {len(back)} distinct PC functions; '
          f'{sum(1 for v in keys.values() if len(v) > 1)} PSX entries with more than one PC twin; '
          f'{sum(1 for v in back.values() if len(v) > 1)} PC functions shared by several PSX entries')
    for d in disagree:
        print('  disagrees:', d)

    # Order and gap counts, over anchors no other PSX entry shares.
    caps = {c['source_md5']: c for c in json.load(open(a.sibling + 'analysis/overlay_captures_all.json'))}
    funcs = {f['entry'] for f in json.load(open(a.funcs))['functions']}
    hidden = {h['entry'] for h in json.load(open(a.hidden))}
    pc_starts = sorted(funcs | hidden | set(back))
    by = collections.defaultdict(set)
    for r in pairs:
        if len(back[r['pc']]) == 1:
            by[r['section']].add((r['psx'], r['pc']))
    ordered = total = 0
    gaps = collections.Counter()
    for s, v in by.items():
        if len(v) < 2:
            continue
        v = sorted(v)
        total += 1
        ordered += [b for _, b in v] == sorted(b for _, b in v)
        if s not in caps:
            continue
        c = caps[s]
        ps = sorted({int(x, 16) for key in ('static_discovery_entry_pcs', 'header_entry_pcs',
                                            'engine_entry_pcs') for x in c.get(key, [])}
                    | {p for p, _ in v})
        for (a1, b1), (a2, b2) in zip(v, v[1:]):
            if b2 > b1:
                n_psx = bisect.bisect_left(ps, a2) - bisect.bisect_left(ps, a1)
                n_pc = bisect.bisect_left(pc_starts, b2) - bisect.bisect_left(pc_starts, b1)
                gaps[n_pc - n_psx] += 1
    print(f'{ordered} of {total} overlays keep their PSX order on the PC; '
          f'{gaps[0]} of {sum(gaps.values())} anchor gaps hold equal start counts; '
          f'PC minus PSX elsewhere: {dict(sorted((k, n) for k, n in gaps.items() if k))}')
    json.dump(dict(pairs=pairs, disagree=disagree), open(a.out or 'analysis/area_pairs.json', 'w'), indent=0)


def shape(slots):
    m, out = {}, []
    for kind, v in slots:
        out.append(kind if kind < 0 else m.setdefault(v, len(m)))
    return tuple(out)


def pc_tables(img):
    out = []
    for s in img.secs:
        if s['name'] == '.text':
            continue
        blob = img.data[s['raw']:s['raw'] + s['rsz']]
        i = 0
        while i + 4 <= len(blob):
            run, j = [], i
            while j + 4 <= len(blob):
                x, = struct.unpack_from('<I', blob, j)
                if not (img.in_text(x) or (x == 0 and run)):
                    break
                run.append(x)
                j += 4
            while run and run[-1] == 0:
                run.pop()
            if sum(1 for x in run if x) >= 3:
                slots = [(-1, 0) if x == 0 else (-2, 0) if img.data[img.off(x)] == 0xC3 else (0, x)
                         for x in run]
                out.append(dict(va=s['va'] + i, slots=slots))
                i = j
            else:
                i += 4
    return out


def psx_tables(sib):
    boot = open(sib + 'disc/SLPS_009.90', 'rb').read()[0x800:]
    images = [('BOOT', PSX_BOOT_LO, boot)]
    for c in json.load(open(sib + 'analysis/overlay_captures_all.json')):
        images.append((f"{c['source_file']}#{c['source_index']}", int(c['load_addr'], 16)
                       if isinstance(c['load_addr'], str) else c['load_addr'],
                       base64.b64decode(c['bytes_b64'])))
    out = []
    for name, lo, d in images:
        def word(x):
            for base, blob in ((lo, d), (PSX_BOOT_LO, boot)):
                if base <= x < base + len(blob) - 7:
                    return struct.unpack_from('<II', blob, x - base)
            return None

        def code(x):
            return not x & 3 and (lo <= x < lo + len(d) or PSX_BOOT_LO <= x < PSX_BOOT_LO + PSX_BOOT_TEXT)

        i = 0
        while i + 4 <= len(d):
            run, j = [], i
            while j + 4 <= len(d):
                x, = struct.unpack_from('<I', d, j)
                if not (code(x) or (x == 0 and run)):
                    break
                run.append(x)
                j += 4
            while run and run[-1] == 0:
                run.pop()
            if sum(1 for x in run if x) >= 3:
                slots = [(-1, 0) if x == 0 else (-2, 0) if word(x) == (0x03E00008, 0) else (0, x) for x in run]
                out.append(dict(img=name, va=lo + i, slots=slots))
                i = j
            else:
                i += 4
    return out


def cmd_tables(a):
    img = pe_hidden.Image(a.exe)
    pcs, pss = pc_tables(img), psx_tables(a.sibling)
    rows = []
    for t in pcs:
        sig, n = shape(t['slots']), len(t['slots'])
        hits = [(u['img'], u['va'] + 4 * o, len(u['slots']))
                for u in pss for o in range(len(u['slots']) - n + 1)
                if shape(u['slots'][o:o + n]) == sig]
        info = sum(1 for k in range(n) if sig[k] < 0 or sig[k] in sig[:k])  # null, ret or repeat
        rows.append(dict(pc=t['va'], n=n, info=info, hits=hits))
    one = [r for r in rows if len(r['hits']) == 1 and r['info'] >= 2]
    print(f'{len(pcs)} PC tables, {len(pss)} PSX tables; {len(one)} PC tables with exactly one '
          f'PSX twin and at least two null / return / repeated slots:')
    for r in sorted(one, key=lambda r: -r['info']):
        img_, va, m = r['hits'][0]
        print(f"  0x{r['pc']:06X} ({r['n']}, {r['info']}) <-> {img_} 0x{va:08X} (run of {m})")
    json.dump(rows, open(a.out or 'analysis/table_matches.json', 'w'), indent=0)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--exe', default='bof3/BOF3.exe')
    ap.add_argument('--funcs', default='analysis/pc_funcs.json')
    ap.add_argument('--hidden', default='analysis/pc_hidden.json')
    ap.add_argument('--sibling', default='../BreathOfFire3Recomp/')
    ap.add_argument('--out')
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('areas').set_defaults(fn=cmd_areas)
    sub.add_parser('tables').set_defaults(fn=cmd_tables)
    a = ap.parse_args()
    a.fn(a)


if __name__ == '__main__':
    main()
