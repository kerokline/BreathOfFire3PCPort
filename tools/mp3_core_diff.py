"""Diff minimp3.h's decoding core against dr_mp3.h's, function by function.

dr_mp3 carries minimp3's decoder renamed (drmp3_*, drmp3dec_*, drmp3_uint8, ...)
beside its own stream layer. This puts both on one naming, drops comments and
whitespace, splits each into top-level items (functions, tables, typedefs) and
reports which are token-identical and a unified diff of each that is not.
docs/mp3-decoder-choice.md has the reading of its output.

    python tools/mp3_core_diff.py <minimp3.h> <dr_mp3.h> [--out core.diff]

Neither header is carried in the repository; point it at checkouts of
github.com/lieff/minimp3 and github.com/mackron/dr_libs.
"""
import argparse
import difflib
import re
import sys

STD_INT = {f'{s}int{n}_t' for s in ('', 'u') for n in (8, 16, 32, 64)}
DR_INT = {f'drmp3_{s}int{n}': f'{s}int{n}_t' for s in ('', 'u') for n in (8, 16, 32, 64)}
DR_EXACT = {
    'drmp3dec': 'mp3dec', 'drmp3_bs_get_bits': 'get_bits', 'drmp3_bs': 'bs',
    'DRMP3_INLINE': 'INLINE', 'DRMP3_API': '', 'size_t': 'int',
    'DRMP3_COPY_MEMORY': 'memcpy', 'DRMP3_MOVE_MEMORY': 'memmove',
    'DRMP3_ZERO_MEMORY': 'ZERO', 'DRMP3_ASSERT': 'assert',
}
PREFIXES = ('drmp3dec_', 'drmp3d_', 'drmp3_', 'DRMP3_', 'mp3dec_', 'mp3d_', 'minimp3_', 'MINIMP3_')


def norm_ident(tok, dr):
    if dr:
        if tok in DR_INT:
            return DR_INT[tok]
        if tok in DR_EXACT:
            return DR_EXACT[tok]
        if tok.startswith('g_drmp3_'):
            return 'g_' + tok[len('g_drmp3_'):]
    else:
        if tok == 'inline':
            return 'INLINE'
        # minimp3's typedefs end in _t (mp3dec_t, bs_t, L3_gr_info_t); dr_mp3's do not
        if tok.endswith('_t') and tok not in STD_INT and tok not in ('size_t', 'float32x4_t'):
            tok = tok[:-2]
    for p in PREFIXES:
        if tok.startswith(p) and len(tok) > len(p):
            return tok[len(p):]
    return tok


def normalise(text, dr):
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    out = []
    for line in text.split('\n'):
        line = re.sub(r'[A-Za-z_]\w*', lambda m: norm_ident(m.group(0), dr), line)
        line = re.sub(r'\s+', ' ', line).strip()
        if line and not line.startswith('#'):
            out.append(line)
    return out


def items(lines):
    """Top-level items keyed by the name the first lines declare."""
    groups, cur, depth = [], [], 0
    for line in lines:
        cur.append(line)
        depth += line.count('{') - line.count('}')
        if depth == 0 and line.endswith((';', '}')):
            groups.append(cur)
            cur = []
    if cur:
        groups.append(cur)
    keyed = {}
    for g in groups:
        m = re.search(r'(\w+)\s*(\[[^\]]*\]\s*)*(\(|=|\[)', ' '.join(g[:2]))
        key = m.group(1) if m else ' '.join(g[:2])[:60]
        t = re.match(r'\}\s*(\w+)\s*;$', g[-1])
        if g[0].startswith('typedef') and t:   # an anonymous struct goes by its typedef name
            key = 'typedef ' + t.group(1)
        k, n = key, 1
        while k in keyed:
            n += 1
            k = f'{key}#{n}'
        keyed[k] = g
    return keyed


def regions(mini_text, dr_text):
    mini = mini_text.split('\n')
    start = next(i for i, l in enumerate(mini) if 'MINIMP3_IMPLEMENTATION' in l and l.startswith('#if'))
    dr = dr_text.split('\n')
    # dr_mp3's decoder runs from its header macros to drmp3dec_f32_to_s16's body; the
    # SIMD set-up 40 lines above the macros is minimp3's too
    first = next(i for i, l in enumerate(dr) if l.startswith('#define DRMP3_HDR_SIZE'))
    last = max(i for i, l in enumerate(dr) if l.startswith('DRMP3_API void drmp3dec_f32_to_s16'))
    last = next(i for i in range(last, len(dr)) if dr[i] == '}')
    return '\n'.join(mini[start:]), '\n'.join(dr[first - 40:last + 1])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('minimp3')
    ap.add_argument('dr_mp3')
    ap.add_argument('--out', help='write the report here instead of stdout')
    a = ap.parse_args()
    mini_text, dr_text = regions(open(a.minimp3, encoding='utf-8').read(),
                                 open(a.dr_mp3, encoding='utf-8').read())
    A, B = items(normalise(mini_text, False)), items(normalise(dr_text, True))
    same = [k for k in A if k in B and A[k] == B[k]]
    diff = [k for k in A if k in B and A[k] != B[k]]
    rep = [f'identical: {len(same)}', f'differing: {len(diff)}',
           f'only minimp3: {[k for k in A if k not in B]}',
           f'only dr_mp3: {[k for k in B if k not in A]}', '']
    for k in diff:
        rep.append(f'===== {k}')
        rep.extend(difflib.unified_diff(A[k], B[k], 'minimp3', 'dr_mp3', n=1, lineterm=''))
    text = '\n'.join(rep) + '\n'
    if a.out:
        open(a.out, 'w', encoding='utf-8').write(text)
        print('\n'.join(rep[:4]))
    else:
        sys.stdout.write(text)


if __name__ == '__main__':
    main()
