#!/usr/bin/env python
"""For each non-exact US function in us_jp.tsv, align its masked words with the JP words at the mapped
address (difflib on word lists) and report how many words differ: a region difference census.

    python -I regdiff.py --map analysis/region-twins --x <dir with us/ and jp/> --twinmap <dir of region_twin_map.py>
"""
import argparse, csv, difflib, importlib.util, os, struct, sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--map', required=True); ap.add_argument('--x', required=True); ap.add_argument('--twinmap', required=True)
    a = ap.parse_args()
    spec = importlib.util.spec_from_file_location('twinmap', os.path.join(a.twinmap, 'region_twin_map.py'))
    tm = importlib.util.module_from_spec(spec); spec.loader.exec_module(tm)
    rows = list(csv.DictReader(open(os.path.join(a.map, 'us_jp.tsv')), delimiter='\t'))
    cache = {}
    out = open(os.path.join(a.map, 'region_diff.tsv'), 'w')
    out.write('target\tus\tjp\tsize\tkind\tstatus\tus_words\tjp_words_aligned\tdiff_words\tfirst_diff_us\n')
    for r in rows:
        if r['status'] == 'exact' or not r['jp']:
            continue
        t = r['target']
        if t not in cache:
            name, idx = tm.TARGETS[t]
            ub, ud = tm.section(a.x, 'us', name, idx); jb, jd = tm.section(a.x, 'jp', name, idx)
            cache[t] = (ub, tm.mask_words(ud), jb, tm.mask_words(jd))
        ub, um, jb, jm = cache[t]
        us, jp, size = int(r['us'], 16), int(r['jp'], 16), int(r['size'])
        uo, jo = us - ub, jp - jb
        uw = list(struct.unpack_from('<%dI' % (size // 4), um, uo))
        # JP window: same size plus slack both ways
        slack = 64
        j0 = max(0, jo - slack); j1 = min(len(jm), jo + size + slack)
        jw = list(struct.unpack_from('<%dI' % ((j1 - j0) // 4), jm, j0))
        sm = difflib.SequenceMatcher(None, uw, jw, autojunk=False)
        blocks = sm.get_matching_blocks()
        matched = sum(b.size for b in blocks)
        # the JP span the matching blocks cover
        covered = [b for b in blocks if b.size]
        jp_span = (covered[-1].b + covered[-1].size - covered[0].b) if covered else 0
        diff = len(uw) - matched
        first = next((b.a + b.size for b in covered if b.a + b.size < len(uw)), 0) if covered else 0
        out.write('%s\t%s\t%s\t%d\t%s\t%s\t%d\t%d\t%d\t0x%08X\n' % (t, r['us'], r['jp'], size, r['kind'], r['status'], len(uw), jp_span, diff, us + first * 4))
    out.close()
    print('wrote', os.path.join(a.map, 'region_diff.tsv'))


if __name__ == '__main__':
    main()
