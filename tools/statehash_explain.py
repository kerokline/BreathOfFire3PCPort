#!/usr/bin/env python
"""Name the bytes behind a state-hash difference on one page, after the fact
(docs/state-hash.md 5a; known-defects D420).

A hash file says a page differed at a tick, not which bytes. When the run that
differed cannot be made to differ again (noise from the machine's use), replay
the same build with BOF3X_STATEHASH_DUMP at that tick and its neighbours: the
replay's page, hashed here, equals the reference's recorded hash (the
self-check), and a change to it that gives the other run's recorded hash is
what that run held.

    python tools/statehash_explain.py REF.sh NEW.sh DUMP_STEM --page 0x7DE000 --tick T

DUMP_STEM is the replay's hash file: the dumps are DUMP_STEM.<tick>.bin. Tried,
in order: one byte of the page at any value; one or two key bytes of Key_State
at 0x80 (the real keyboard); the bytes of a neighbouring tick's dump (T-2 to
T+2, where dumped) put in place, by differing run of bytes and every
combination of up to 18 runs (something a tick early or late).

The page is hashed as src/hook/statehash.cpp hashes it for a run with no
language layer and nothing folded: the skip list's ranges zeroed, a bound code
word (a place under one of bind.cpp's top bytes) as its VA. A page that needs
more fails the self-check, and the tool says so and stops.
"""
import argparse
import itertools
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import statehash as S  # noqa: E402

PAGE = 0x1000
MASK = (1 << 64) - 1
GOLD = 0x9E3779B97F4A7C15
TOPS = (0x7C, 0x70, 0x7E, 0x7B, 0x7D)   # src/hook/bind.cpp kTops
CODE = (0x401000, 0x5DA000)
KEY_STATE, KEY_COUNT = 0x7DE828, 256      # symbols.toml Key_State


def hash_page(p):
    h = GOLD
    for (w,) in struct.iter_unpack('<Q', bytes(p)):
        h = ((h ^ w) * GOLD) & MASK
        h ^= h >> 29
    return (h ^ (h >> 32)) & 0xFFFFFFFF


def normalised(raw, page, skips):
    p = bytearray(raw)
    for at, n in skips:
        lo, hi = max(at, page), min(at + n, page + PAGE)
        if lo < hi:
            p[lo - page:hi - page] = bytes(hi - lo)
    for at in range(0, PAGE, 4):
        w = struct.unpack_from('<I', p, at)[0]
        if (w >> 24) in TOPS and CODE[0] <= (w & 0xFFFFFF) < CODE[1]:
            struct.pack_into('<I', p, at, w & 0xFFFFFF)
    return p


def one_byte(p, want):
    """Every (offset, value) whose single change gives `want`. The hash runs word by word, so the words
    before the changed one are hashed once and the rest for all 2,040 candidates of a word together."""
    import numpy as np
    gold, s29, s32, low = np.uint64(GOLD), np.uint64(29), np.uint64(32), np.uint64(0xFFFFFFFF)
    words = np.frombuffer(bytes(p), dtype='<u8')
    vals = np.arange(256, dtype=np.uint64)
    hits = []
    with np.errstate(over='ignore'):
        h = gold
        for k in range(PAGE // 8):
            c = np.concatenate([(words[k] & ~(np.uint64(0xFF) << np.uint64(8 * b))) | (vals << np.uint64(8 * b))
                                for b in range(8)])
            v = (h ^ c) * gold
            v ^= v >> s29
            for w in words[k + 1:]:
                v = (v ^ w) * gold
                v ^= v >> s29
            out = (v ^ (v >> s32)) & low
            hits += [(k * 8 + int(i) // 256, int(i) % 256) for i in np.nonzero(out == np.uint64(want))[0]]
            h = (h ^ words[k]) * gold
            h ^= h >> s29
    return [(o, v) for o, v in hits if p[o] != v]


def keys(p, page, want):
    if not page <= KEY_STATE < page + PAGE:
        return None
    base = KEY_STATE - page
    for n in (1, 2):
        for combo in itertools.combinations(range(KEY_COUNT), n):
            q = bytearray(p)
            for k in combo:
                q[base + k] = 0x80
            if hash_page(q) == want:
                return combo
    return None


def runs_of(cur, oth):
    runs = []
    for i in range(PAGE):
        if cur[i] != oth[i]:
            if runs and i - runs[-1][1] <= 4:
                runs[-1][1] = i + 1
            else:
                runs.append([i, i + 1])
    return runs


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('ref', help='the reference run\'s hash file')
    ap.add_argument('new', help='the run that differed')
    ap.add_argument('dumps', help='the replay\'s hash file; its dumps are <this>.<tick>.bin')
    ap.add_argument('--page', type=lambda v: int(v, 0), required=True)
    ap.add_argument('--tick', type=int, required=True)
    ap.add_argument('--skip-list', default=os.path.join(S.ROOT, 'tools', 'statehash_skip.txt'))
    a = ap.parse_args()
    ref, new = S.Run(a.ref), S.Run(a.new)
    skips = S.parse_skips(open(a.skip_list, encoding='utf-8').read())
    if ref.skip_hash != S.skip_hash(skips):
        sys.exit(f'{a.ref} was hashed under another skip list than {a.skip_list}: name the right one with --skip-list')
    pi = (a.page - ref.base) // ref.page
    rec = {k: (sa[pi], sb[pi]) for k, (sa, sb) in S.walk([ref, new], a.tick - 2, a.tick + 2)}
    if a.tick not in rec:
        sys.exit(f'tick {a.tick} is not in both runs')
    want_ref, want_new = rec[a.tick]
    if want_ref == want_new:
        sys.exit(f'the runs agree on page 0x{a.page:06X} at tick {a.tick}')
    pages = {}
    for t in range(a.tick - 2, a.tick + 3):
        path = f'{a.dumps}.{t}.bin'
        if os.path.exists(path):
            d = open(path, 'rb').read()
            pages[t] = normalised(d[a.page - S.BASE:a.page - S.BASE + PAGE], a.page, skips)
    if a.tick not in pages:
        sys.exit(f'{a.dumps}.{a.tick}.bin: no dump at the tick (BOF3X_STATEHASH_DUMP)')
    cur = pages[a.tick]
    if hash_page(cur) != want_ref:
        sys.exit(f'self-check failed: the dump\'s page hashes {hash_page(cur):08X}, the reference recorded {want_ref:08X} - '
                 'the replay is not the reference\'s build and settings, or the page holds a normalised slot')
    print(f'page 0x{a.page:06X} tick {a.tick}: reference {want_ref:08X}, the other run {want_new:08X}; the dump hashes as the reference')
    found = False
    for off, v in one_byte(cur, want_new):
        found = True
        print(f'  one byte: 0x{a.page + off:06X} {S.at(a.page + off)} = 0x{v:02X} (the reference holds 0x{cur[off]:02X})')
    combo = None if found else keys(cur, a.page, want_new)
    if combo:
        found = True
        print('  Key_State: DirectInput key ' + ' and '.join(f'0x{k:02X}' for k in combo) + ' down')
    for t, oth in sorted(pages.items()):
        if t == a.tick or found:
            continue
        runs = runs_of(cur, oth)
        if len(runs) > 18:
            print(f'  tick {t}: {len(runs)} differing runs of bytes, too many to combine')
            continue
        for n in range(1, len(runs) + 1):
            for c in itertools.combinations(runs, n):
                q = bytearray(cur)
                for lo, hi in c:
                    q[lo:hi] = oth[lo:hi]
                if hash_page(q) == want_new:
                    found = True
                    print(f'  as at tick {t}: ' + ', '.join(f'0x{a.page + lo:06X}..0x{a.page + hi:06X} {S.at(a.page + lo)}' for lo, hi in c))
    if not found:
        print('  not explained by one byte, two keys or a neighbouring tick\'s bytes')
    return 0 if found else 1


if __name__ == '__main__':
    sys.exit(main())
