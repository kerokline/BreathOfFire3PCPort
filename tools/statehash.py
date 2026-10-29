#!/usr/bin/env python
"""Compare runs by the state hash (BOF3X_STATEHASH, src/hook/statehash.cpp):
one hash per 4 KiB page of BOF3.exe's .data, once a logic frame.

    python tools/statehash.py info RUN.sh
    python tools/statehash.py diff A.sh B.sh [--from T] [--to T] [--align recipe]
    python tools/statehash.py check REF.sh REFB.sh NEW.sh [--from T] [--to T]
    python tools/statehash.py bytes A.sh.T.bin B.sh.T.bin [--skip-out FILE] [--fold ADDR]
    python tools/statehash.py selftest

diff   the first tick at which two runs differ, and per page the ticks it
       differed on (first, last, count), with the symbols the page holds.
check  the regression question: REF and REFB are two runs of the reference
       side, NEW the side under test. A page is noise at a tick when the two
       references disagree there; what is reported is NEW against REF on every
       page and tick where the references agree. Exit 1 when anything is.
bytes  two raw dumps (BOF3X_STATEHASH_DUMP) compared byte for byte: the
       differing ranges with their symbols, and with --skip-out a skip list
       (BOF3X_STATEHASH_SKIP) of them to review - never feed it back unread.
       --fold ADDR reads every aligned word of both dumps whose value lies in
       the image placed at ADDR as at 0x400000, as the hash does (below).
selftest  the reader and the fold on synthetic files: a dump with every
       image pointer moved by a constant - into .pool too - compares equal
       folded, and runs hashed with different folded ranges or extents are
       refused.

Runs hashed under different skip lists are refused, except where the run
under test (check's NEW, diff's B) only adds ranges to the reference's:
then every page none of the added ranges touches is compared, and those
pages are named and left out (2026-10-11; --strict refuses anyway; the lists
are found by their header hash among the committed versions of
tools/statehash_skip.txt and --skip-list files).

A BOF3SH3 file (2026-10-10) hashes our pointers in Capcom's pointer slots by
what they name; one beside an older or BOF3X_STATEHASH_RAW=1 file is compared
with a note (docs/state-hash.md section 7).

A BOF3SH4 file (2026-10-28) adds where the game's image lay and the
BOF3X_STATEHASH_FOLD address: every word whose value lies in a folded range
(the image's base when it is not 0x400000, and the fold address) is hashed as
at 0x400000 (docs/state-hash.md section 8). With nothing folded it hashes as
BOF3SH3 bit for bit, so every older file is a run that folded nothing. Runs
that folded different ranges are refused: game data holds values in any such
range, and those hash differently on the two sides.

A BOF3SH5 file (2026-10-29) adds the folded range's extent: 0x587000, the
image to the end of bof3x.exe's .pool (src/hook/exe_layout.h), where a
BOF3SH4 file folded [F, F + 0x53F000), BOF3.exe's SizeOfImage, and left the
pointers into a moved .pool raw (docs/statehash-pool-fold.md). A BOF3SH4 file
is read as extent 0x53F000. Runs that folded the same ranges over different
extents are refused as different ranges are; with nothing folded the extent
changes nothing, and every file compares as before.

Runs are aligned by tick (the logic frames the latch saw, from the first) or,
with --align recipe, by the recipe frame of a scripted run. docs/state-hash.md
has the method and what the hash does not see.
"""
import argparse, array, bisect, os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_exe import read_layout  # noqa: E402  (the `kName = 0x...;` lines of exe_layout.h)

BASE, END = 0x5DA000, 0x93E000
# The image at its own base, and a folded range's extent (statehash.cpp kExeLo,
# kImageExtent): to the end of bof3x.exe's .pool, which moves with the image -
# taken from src/hook/exe_layout.h, as the engine takes it, not written again
_LAYOUT = read_layout(os.path.join(ROOT, 'src', 'hook', 'exe_layout.h'))
IMAGE = _LAYOUT['ImageBase']
IMAGE_EXTENT = _LAYOUT['PoolVa'] + _LAYOUT['PoolSize'] - IMAGE
# A BOF3SH4 file's extent, which its header does not record: BOF3.exe's
# SizeOfImage, the range statehash.cpp folded from 2026-10-28 to 2026-10-29
SH4_EXTENT = 0x53F000
assert IMAGE == 0x400000 and IMAGE_EXTENT == 0x587000, 'exe_layout.h moved: read docs/state-hash.md section 8'
VERSIONS = {b'BOF3SH1\0': 1, b'BOF3SH2\0': 2, b'BOF3SH3\0': 3, b'BOF3SH4\0': 4, b'BOF3SH5\0': 5}


class Run:
    def __init__(self, path, align='tick'):
        d = open(path, 'rb').read()
        if d[:8] not in VERSIONS:
            sys.exit(f'{path}: not a state hash file')
        self.version = VERSIONS[d[:8]]
        self.base, self.pages, self.page, self.skips = struct.unpack_from('<4I', d, 8)
        # BOF3SH2 adds a hash of the skip list's ranges; a BOF3SH1 file has none
        self.skip_hash = struct.unpack_from('<I', d, 24)[0] if self.version >= 2 else None
        # BOF3SH3 adds flags, bit 0: our pointers in Capcom's slots hashed by
        # what they name (docs/state-hash.md section 7); before it, always raw
        flags = struct.unpack_from('<I', d, 28)[0] if self.version >= 3 else 0
        self.normalised = bool(flags & 1)
        # bit 1 (2026-10-11): the run-time slots the battle's messages fill
        # (0x93C2C4's ring, 0x93B8E4) normalised with them; a run without it
        # hashes those words raw and differs there under an overlay
        self.runtime_slots = bool(flags & 2)
        # bit 2 (2026-10-29, the relocatable-image round's stage C2): a word
        # holding the start of a function of ours that the bind table binds to
        # a VA of Capcom's is hashed as that VA (docs/state-hash.md section 8)
        self.bound_code = bool(flags & 4)
        # BOF3SH4 (2026-10-28) adds where the image lay and the fold address;
        # an older file is a run at 0x400000 that folded nothing
        self.image, self.fold = struct.unpack_from('<2I', d, 32) if self.version >= 4 else (IMAGE, 0)
        self.folds = frozenset(b for b in (self.image, self.fold) if b and b != IMAGE)
        # BOF3SH5 (2026-10-29) adds the folded range's extent; a BOF3SH4 file
        # folded BOF3.exe's SizeOfImage, an older one nothing
        self.extent = (struct.unpack_from('<I', d, 40)[0] if self.version >= 5 else
                       SH4_EXTENT if self.version == 4 else None)
        self.path = path
        self.keys = []      # per record: the alignment key
        self.fc = []        # per record: Frame_Counter
        self.recs = []      # per record: [(page, hash)...] changed
        o = self.header = {1: 24, 2: 28, 3: 32, 4: 40, 5: 44}[self.version]
        while o + 16 <= len(d):
            tick, fc, rf, n = struct.unpack_from('<4I', d, o)
            if o + 16 + 8 * n > len(d):
                break       # the game was killed inside a record
            ch = struct.unpack_from(f'<{2 * n}I', d, o + 16)
            self.keys.append(rf if align == 'recipe' else tick)
            self.fc.append(fc)
            self.recs.append(ch)
            o += 16 + 8 * n

    def frames(self):
        """Yield (key, Frame_Counter, state list) per record; the list is reused."""
        state = [0] * self.pages
        for key, fc, ch in zip(self.keys, self.fc, self.recs):
            for i in range(0, len(ch), 2):
                state[ch[i]] = ch[i + 1]
            yield key, fc, state


_syms = None


def symbols():
    """(sorted addresses, names) of every symbols.toml entry inside .data."""
    global _syms
    if _syms is None:
        import tomllib
        with open(os.path.join(ROOT, 'symbols.toml'), 'rb') as f:
            t = tomllib.load(f)
        rows = []
        for kind in t.values():
            if not isinstance(kind, list):
                continue
            for e in kind:
                pc = e.get('pc') if isinstance(e, dict) else None
                if isinstance(pc, str):
                    try:
                        pc = int(pc, 0)
                    except ValueError:
                        continue
                if isinstance(pc, int) and BASE <= pc < END and e.get('name'):
                    rows.append((pc, e['name']))
        rows.sort()
        _syms = ([a for a, _ in rows], [n for _, n in rows])
    return _syms


def names_in(lo, hi, limit=4):
    addrs, names = symbols()
    i, j = bisect.bisect_left(addrs, lo), bisect.bisect_left(addrs, hi)
    out = [f'{names[k]}' for k in range(i, min(j, i + limit))]
    if j - i > limit:
        out.append(f'+{j - i - limit}')
    if not out and i > 0:
        out = [f'({names[i - 1]}+0x{lo - addrs[i - 1]:X})']
    return ' '.join(out)


def at(lo):
    addrs, names = symbols()
    i = bisect.bisect_right(addrs, lo) - 1
    return f'{names[i]}+0x{lo - addrs[i]:X}' if i >= 0 else '?'


def walk(runs, lo, hi):
    """Step the runs together by key; yield (key, [state...]) where all have it."""
    its = [r.frames() for r in runs]
    cur = [next(it, None) for it in its]
    while all(c is not None for c in cur):
        k = max(c[0] for c in cur)
        for n, it in enumerate(its):
            while cur[n] is not None and cur[n][0] < k:
                cur[n] = next(it, None)
        if any(c is None for c in cur):
            return
        if any(c[0] != k for c in cur):
            continue
        if hi is not None and k > hi:
            return
        if k >= lo:
            yield k, [c[2] for c in cur]
        cur = [next(it, None) for it in its]


def report(per_page, run, total, what):
    if not per_page:
        print(f'{what}: identical on all {total} compared ticks')
        return
    first = min(v[0] for v in per_page.values())
    print(f'{what}: {len(per_page)} pages differ over {total} compared ticks; the first at tick {first}')
    print('page       first    last   ticks  symbols')
    for p, (f, l, c) in sorted(per_page.items(), key=lambda kv: (kv[1][0], kv[0])):
        a = run.base + p * run.page
        print(f'0x{a:06X} {f:7d} {l:7d} {c:7d}  {names_in(a, a + run.page)}')


SKIP_FILE = 'tools/statehash_skip.txt'
_lists = None


def parse_skips(text):
    """A skip list's (address, length) ranges in file order, read as
    statehash.cpp's LoadSkips reads them: '#' starts a comment, a blank line is
    skipped, every other line is ADDRESS LENGTH."""
    out = []
    for line in text.splitlines():
        words = line.split('#', 1)[0].split()
        if words:
            out.append((int(words[0], 0), int(words[1], 0)))
    return out


def skip_hash(ranges):
    """The header's skip list hash: FNV-1a over each range's address and
    length as little-endian u32s, 0 for an empty list (statehash.cpp)."""
    if not ranges:
        return 0
    h = 2166136261
    for at_, ln in ranges:
        for w in (at_, ln):
            for b in range(4):
                h = ((h ^ ((w >> (8 * b)) & 0xFF)) * 16777619) & 0xFFFFFFFF
    return h


def known_lists(extra=()):
    """{(count, hash): ranges} for every skip list the tool can name: the
    working tree's tools/statehash_skip.txt, every committed version of it
    (git log), and the files given with --skip-list."""
    global _lists
    if _lists is None:
        import subprocess
        texts = []
        try:
            texts.append(open(os.path.join(ROOT, SKIP_FILE), encoding='utf-8').read())
        except OSError:
            pass
        try:
            revs = subprocess.run(['git', '-C', ROOT, 'log', '--format=%H', '--', SKIP_FILE],
                                  capture_output=True, text=True, check=True).stdout.split()
            for rev in revs:
                texts.append(subprocess.run(['git', '-C', ROOT, 'show', f'{rev}:{SKIP_FILE}'],
                                            capture_output=True, encoding='utf-8').stdout)
        except (OSError, subprocess.CalledProcessError):
            pass
        _lists = {}
        for t in texts:
            r = parse_skips(t)
            _lists.setdefault((len(r), skip_hash(r)), r)
    for path in extra:
        r = parse_skips(open(path, encoding='utf-8').read())
        _lists[(len(r), skip_hash(r))] = r
    return _lists


def same_headers(runs, strict=False, skip_lists=()):
    """Refuse runs that were not hashed alike: a page holding a range one run
    skipped and another did not differs at every tick, which `check` would call
    noise and never compare (round fourteen's review, item 3).

    One exception (2026-10-11, docs/state-hash.md section 4): when the lists
    differ only by ranges the run under test (the last) adds - the reference
    side shares one list and every range of it is in the new one - every page none of the added ranges touches hashes alike under both
    lists and is compared; the pages they touch are left out and named. The
    lists are found by their header hash among the committed versions of the
    skip list and the --skip-list files. --strict refuses as before.
    Returns the set of page indices to leave out."""
    first = runs[0]
    for r in runs[1:]:
        for what in ('base', 'pages', 'page'):
            if getattr(r, what) != getattr(first, what):
                sys.exit(f'{r.path}: {what} is {getattr(r, what)}, {first.path} has {getattr(first, what)} - '
                         'the runs were not hashed alike; record a new pair')
        if r.folds != first.folds:
            sys.exit(f'{r.path} folded {fold_text(r)} and {first.path} {fold_text(first)}: game data holds values '
                     'in such a range, hashed as at 0x400000 on one side and raw on the other - the runs were not '
                     'hashed alike; record both with the same BOF3X_STATEHASH_FOLD (docs/state-hash.md section 8)')
        if r.folds and r.extent != first.extent:
            # the same bases folded over different lengths: a word between the
            # two ends (a pointer into a moved .pool, or data there) is folded
            # on one side and raw on the other
            sys.exit(f'{r.path} folded {fold_text(r)} and {first.path} {fold_text(first)}: the ranges differ in '
                     'extent (a BOF3SH4 file folded 0x53F000 bytes, BOF3SH5 to .pool\'s end) - the runs were not '
                     'hashed alike; record both with one build (docs/state-hash.md section 8)')
    notes(runs)
    keys = [(r.skips, r.skip_hash) for r in runs]
    if any(r.skip_hash is None for r in runs):
        # a BOF3SH1 file: the lists are compared by their length only, as before
        keys = [(r.skips, None) for r in runs]
    if len(set(keys)) == 1:
        return set()
    a, b = next((x, y) for x, y in zip(runs, runs[1:] + runs[:1]) if (x.skips, x.skip_hash) != (y.skips, y.skip_hash))
    if strict or any(r.skip_hash is None for r in runs):
        why = '--strict' if strict else 'a BOF3SH1 file has no list hash'
        sys.exit(f"{b.path}: its skip list ({b.skips} ranges, hash {b.skip_hash or 0:08X}) is not {a.path}'s "
                 f'({a.skips}, {a.skip_hash or 0:08X}) and {why}: the runs were not hashed alike; record a new pair')
    lists = known_lists(skip_lists)
    missing = [r for r in runs if (r.skips, r.skip_hash) not in lists]
    if missing:
        m = missing[0]
        sys.exit(f'{m.path}: its skip list ({m.skips} ranges, hash {m.skip_hash:08X}) is no committed version of '
                 f"{SKIP_FILE} and no --skip-list file, and the runs' lists differ: record a new pair, or name the "
                 'list with --skip-list')
    sets = [set(lists[k]) for k in keys]
    # the run under test is the last (check's NEW, diff's B); the others are
    # the reference side and must share one list, every range of it in NEW's
    wide = len(runs) - 1
    older = list(range(wide))
    if len({frozenset(sets[i]) for i in older}) != 1 or not sets[0] < sets[wide]:
        sys.exit(f'{runs[older[0]].path} and {runs[wide].path}: the skip lists differ by more than added ranges '
                 '(a range removed or changed): the runs were not hashed alike; record a new pair')
    added = sorted(sets[wide] - sets[older[0]])
    out = {}
    for at_, ln in added:
        for pg in range((at_ - first.base) // first.page, (at_ + ln - 1 - first.base) // first.page + 1):
            out.setdefault(pg, []).append((at_, ln))
    print(f'note: {runs[wide].path} was hashed under {len(sets[wide])} skip ranges and '
          + ', '.join(runs[i].path for i in older) + f' under {len(sets[older[0]])}, all of them in it; '
          f'the {len(added)} added ranges touch {len(out)} pages, left out here (every other page hashes alike '
          'under both lists; --strict refuses instead):')
    for pg, rs in sorted(out.items()):
        a0 = first.base + pg * first.page
        print(f'  0x{a0:06X}  ' + ', '.join(f'0x{x:06X} {n} ({at(x)})' for x, n in rs))
    return set(out)


def fold_text(r):
    if not r.folds:
        return 'nothing (the image at 0x400000)'
    return (' and '.join(f'0x{b:08X}..0x{b + r.extent:08X}' for b in sorted(r.folds))
            + f' (the image at 0x{r.image:08X}, extent 0x{r.extent:X})')


def notes(runs):
    """The notes for runs compared although hashed by different rules (these
    sat after same_headers' return from 2026-10-11 to 2026-10-28, and were
    never printed)."""
    first = runs[0]
    known = [r.skip_hash is not None for r in runs]
    if first.skips and any(known) and not all(known):
        print('note: a run here predates the skip list hash (BOF3SH1): the lists are compared by their length only')
    norm = [r.normalised for r in runs]
    if any(norm) and not all(norm):
        # compared, not refused: the two hash alike on every page that holds no
        # pointer of ours in one of Capcom's slots - all of them without a
        # language overlay - so a reference of before 2026-10-10 still serves
        raw = ', '.join(r.path for r in runs if not r.normalised)
        print(f'note: {raw} hashed our pointers raw (BOF3SH2, or BOF3X_STATEHASH_RAW=1) and the others by what they '
              'name: identical without a language overlay; under one the overlay\'s tables differ from tick 1 '
              '(docs/state-hash.md section 7) - record the reference again rather than read those pages')
    bc = [r.bound_code for r in runs]
    if all(norm) and any(bc) and not all(bc):
        old = ', '.join(r.path for r in runs if not r.bound_code)
        print(f'note: {old} hashed a word holding a function of ours raw and the others as the code VA it is bound to '
              '(stage C2): identical where no such word is stored - hosted, a word our code wrote as our function\'s '
              'address; in bof3x.exe every code word (docs/state-hash.md section 8)')
    rt = [r.runtime_slots for r in runs]
    if all(norm) and any(rt) and not all(rt):
        old = ', '.join(r.path for r in runs if not r.runtime_slots)
        print(f'note: {old} hashed the battle message words (0x93C2C4, 0x93B8E4) raw and the others by what they '
              'name: identical without a language overlay; under one, pages 0x93B000 and 0x93C000 differ from the '
              'first battle message (docs/state-hash.md section 7, measured 3)')


def cmd_info(a):
    r = Run(a.run)
    if not r.recs:
        sys.exit(f'{a.run}: a header and no whole record')
    h = 'no skip list hash (BOF3SH1)' if r.skip_hash is None else f'skip list hash {r.skip_hash:08X}'
    h += ', our pointers ' + ('hashed by what they name' if r.normalised else
                              'raw (BOF3X_STATEHASH_RAW=1)' if r.version >= 3 else f'raw (BOF3SH{r.version})')
    if r.normalised:
        h += ', the battle message words ' + ('with them' if r.runtime_slots else 'raw (before 2026-10-11)')
        h += ', bound code words ' + ('as their VAs' if r.bound_code else 'raw (before 2026-10-29)')
    h += ', folded ' + fold_text(r) + ('' if r.version >= 4 else f' (BOF3SH{r.version})')
    print(f'{a.run}: {len(r.recs)} records, ticks {r.keys[0]}..{r.keys[-1]}, Frame_Counter {r.fc[0]}..{r.fc[-1]}, '
          f'{r.pages} pages of 0x{r.page:X} from 0x{r.base:X}, {r.skips} skip ranges, {h}')
    busy = {}
    for ch in r.recs[1:]:
        for i in range(0, len(ch), 2):
            busy[ch[i]] = busy.get(ch[i], 0) + 1
    print(f'{len(busy)} pages changed at least once after the first record')


def cmd_diff(a):
    ra, rb = Run(a.a, a.align), Run(a.b, a.align)
    out = same_headers([ra, rb], a.strict, a.skip_list)
    per, total = {}, 0
    for k, (sa, sb) in walk([ra, rb], a.lo, a.hi):
        total += 1
        if sa != sb:
            for p in range(ra.pages):
                if sa[p] != sb[p] and p not in out:
                    v = per.get(p)
                    per[p] = (k, k, 1) if v is None else (v[0], k, v[2] + 1)
    report(per, ra, total, 'diff')
    return 1 if per else 0


def cmd_check(a):
    ref, refb, new = Run(a.ref, a.align), Run(a.refb, a.align), Run(a.new, a.align)
    out = same_headers([ref, refb, new], a.strict, a.skip_list)
    noise, per, total = {}, {}, 0
    for k, (sr, sb, sn) in walk([ref, refb, new], a.lo, a.hi):
        total += 1
        if sr == sb == sn:
            continue
        for p in range(ref.pages):
            if p in out:
                continue
            if sr[p] != sb[p]:
                v = noise.get(p)
                noise[p] = (k, k, 1) if v is None else (v[0], k, v[2] + 1)
            elif sr[p] != sn[p]:
                v = per.get(p)
                per[p] = (k, k, 1) if v is None else (v[0], k, v[2] + 1)
    print(f'{total} ticks compared; the references disagree on {len(noise)} pages '
          f'({sum(1 for v in noise.values() if v[2] == total)} of them at every tick)')
    always = sorted(p for p, v in noise.items() if v[2] == total)
    if always and total > 1:
        # never compared at all: say which, so a page that wants a skip range is seen
        print('WARNING: pages the references never agree on, so NEW is never compared there: '
              + ', '.join(f'0x{ref.base + p * ref.page:06X}' for p in always))
    if a.noise:
        report(noise, ref, total, 'noise (reference against reference)')
    report(per, ref, total, 'check (new against the reference, where the references agree)')
    return 1 if per else 0


def fold_dump(d, fold, extent=None):
    """A raw dump with every aligned word whose value lies in the image placed
    at `fold` read as at 0x400000 (statehash.cpp's Fold); and, failing that, a
    word whose low 24 bits do - an ordering-table link into the moved image,
    its length byte on top - with those 24 bits folded and the top byte kept
    (Fold24). The image is `extent` bytes, to .pool's end; the self-test
    passes BOF3SH4's 0x53F000 to show what that missed. The dump keeps the raw
    bytes, the hash does this."""
    w = array.array('I', d)
    if sys.byteorder != 'little':
        w.byteswap()
    lo, hi, n = fold, fold + (IMAGE_EXTENT if extent is None else extent), 0
    for i, v in enumerate(w):
        if lo <= v < hi:
            w[i] = v - fold + IMAGE
            n += 1
        elif v >> 24 and lo <= (v & 0xFFFFFF) < hi and (v & 0xFFFFFF) - fold + IMAGE <= 0xFFFFFF:
            w[i] = (v & 0xFF000000) | ((v & 0xFFFFFF) - fold + IMAGE)
            n += 1
    if sys.byteorder != 'little':
        w.byteswap()
    return w.tobytes(), n


def check_fold(f):
    if f is None:
        return
    if (f % 0x10000 or not f or f + IMAGE_EXTENT > 0x80000000
            or (f != IMAGE and f < IMAGE + IMAGE_EXTENT and f + IMAGE_EXTENT > IMAGE)):
        sys.exit(f'--fold 0x{f:X}: not a 64 KiB-aligned base whose image ends at or below 0x80000000, clear of '
                 f'0x{IMAGE:X}..0x{IMAGE + IMAGE_EXTENT:X} (the image to .pool\'s end)')


def cmd_bytes(a):
    da, db = open(a.a, 'rb').read(), open(a.b, 'rb').read()
    if len(da) != END - BASE or len(db) != END - BASE:
        sys.exit('a dump is not the size of .data')
    check_fold(a.fold)
    if a.fold is not None and a.fold != IMAGE:
        da, na = fold_dump(da, a.fold)
        db, nb = fold_dump(db, a.fold)
        print(f'folded 0x{a.fold:08X}..0x{a.fold + IMAGE_EXTENT:08X} as at 0x400000: {na} words of {a.a}, {nb} of {a.b}')
    return compare_dumps(da, db, a)


def compare_dumps(da, db, a):
    ranges, i, n = [], 0, len(da)
    while i < n:
        if da[i] != db[i]:
            j = i + 1
            # one range across gaps of equal bytes shorter than --gap
            last = i
            while j < n and j - last <= a.gap:
                if da[j] != db[j]:
                    last = j
                j += 1
            ranges.append((BASE + i, last - i + 1))
            i = last + 1
        else:
            i += 1
    print(f'{len(ranges)} differing ranges, {sum(l for _, l in ranges)} bytes')
    out = open(a.skip_out, 'w') if getattr(a, 'skip_out', None) else None
    for addr, ln in ranges:
        o = addr - BASE
        sa, sb = da[o:o + min(ln, 8)].hex(), db[o:o + min(ln, 8)].hex()
        print(f'0x{addr:06X} {ln:6d}  {sa:16s} {sb:16s}  {at(addr)}')
        if out:
            out.write(f'0x{addr:06X} {ln}  # {at(addr)}\n')
    return 1 if ranges else 0


def cmd_selftest(a):
    """The reader and the fold on synthetic files (statehash.cpp's own
    self-test, BOF3X_SELFTEST_ONLY, covers the hash itself)."""
    import random, tempfile
    ok = True

    def expect(what, got, want):
        nonlocal ok
        print(f'  {"ok  " if got == want else "FAIL"} {what}: {got}')
        ok &= got == want

    rnd = random.Random(0x5A1)
    n = (END - BASE) // 4
    F = 0x70000000
    w0 = array.array('I', bytes(END - BASE))
    wf = array.array('I', bytes(END - BASE))
    # bof3x.exe's .pool (exe_layout.h kPoolVa..kOwnVa), past BOF3.exe's SizeOfImage
    pool, pool_size = _LAYOUT['PoolVa'], _LAYOUT['PoolSize']
    kinds = [0, 0, 0, 0, 0]
    for i in range(0, n, 7):    # one word in seven set, the rest zero
        k = rnd.randrange(6)
        if k < 2:       # a pointer into the image, in a slot or stored at run time: moved
            v = IMAGE + (rnd.randrange(IMAGE_EXTENT) & ~3)
            w0[i], wf[i] = v, v - IMAGE + F
        elif k == 5:    # a pointer into .pool (a draw item), stored at run time: moved with the image
            v = pool + (rnd.randrange(pool_size) & ~3)
            w0[i], wf[i] = v, v - IMAGE + F
        elif k == 2:    # data in the moved image's range: unmoved
            w0[i] = wf[i] = F + rnd.randrange(IMAGE_EXTENT)
        elif k == 3:    # a number that reads as an address in the image: unmoved
            w0[i] = wf[i] = IMAGE + rnd.randrange(IMAGE_EXTENT)
        else:
            w0[i] = wf[i] = rnd.getrandbits(32)
        kinds[(0, 0, 1, 2, 3, 4)[k]] += 1
    if sys.byteorder != 'little':
        w0.byteswap(); wf.byteswap()
    d0, df = w0.tobytes(), wf.tobytes()
    print(f'synthetic .data: {kinds[0]} image pointers and {kinds[4]} .pool pointers moved by 0x{F - IMAGE:08X}, '
          f'{kinds[1]} data words in the moved range, {kinds[2]} image-shaped numbers and {kinds[3]} others unmoved')
    q = argparse.Namespace(gap=3)
    import contextlib, io
    with contextlib.redirect_stdout(io.StringIO()):
        raw = compare_dumps(d0, df, q)
        f0, _ = fold_dump(d0, F)
        ff, _ = fold_dump(df, F)
        folded = compare_dumps(f0, ff, q)
        o0, _ = fold_dump(d0, F, SH4_EXTENT)
        of, _ = fold_dump(df, F, SH4_EXTENT)
        folded_sh4 = compare_dumps(o0, of, q)
        df2 = bytearray(df); df2[0x1234] ^= 1
        ff2, _ = fold_dump(bytes(df2), F)
        changed = compare_dumps(f0, ff2, q)
    expect('the moved dump against the unmoved, raw: differs', raw, 1)
    expect(f'the same pair, both folded at the moved base over 0x{IMAGE_EXTENT:X}: equal', folded, 0)
    expect(f"the same pair folded over BOF3SH4's 0x{SH4_EXTENT:X} (the .pool pointers raw): differs", folded_sh4, 1)
    expect('a byte changed in the moved dump, folded: differs', changed, 1)

    # ordering-table links (a length byte over a 24-bit address) into an image
    # below 16 MB, every other one into .pool
    F24 = 0xA00000
    l0 = array.array('I', bytes(END - BASE))
    lf = array.array('I', bytes(END - BASE))
    pool_links = 0
    for i in range(0, n, 5):
        if i % 2:
            off = pool - IMAGE + (rnd.randrange(pool_size) & ~3)
            pool_links += 1
        else:
            off = rnd.randrange(IMAGE_EXTENT) & ~3
        top = rnd.randrange(1, 16) << 24
        l0[i], lf[i] = top | (IMAGE + off), top | (F24 + off)
    if sys.byteorder != 'little':
        l0.byteswap(); lf.byteswap()
    with contextlib.redirect_stdout(io.StringIO()):
        links_raw = compare_dumps(l0.tobytes(), lf.tobytes(), q)
        g0, _ = fold_dump(l0.tobytes(), F24)
        gf, _ = fold_dump(lf.tobytes(), F24)
        links_folded = compare_dumps(g0, gf, q)
        h0, _ = fold_dump(l0.tobytes(), F24, SH4_EXTENT)
        hf, _ = fold_dump(lf.tobytes(), F24, SH4_EXTENT)
        links_sh4 = compare_dumps(h0, hf, q)
        lf2 = bytearray(lf.tobytes()); lf2[4 * 5 * 3] ^= 0x10   # the low byte of a link into .pool
        gf2, _ = fold_dump(bytes(lf2), F24)
        links_changed = compare_dumps(g0, gf2, q)
    print(f'synthetic links: {len(range(0, n, 5))}, {pool_links} of them into .pool')
    expect('ordering-table links into the moved image, raw: differ', links_raw, 1)
    expect('the same links folded at 0xA00000 (24 bits, the length byte kept): equal', links_folded, 0)
    expect(f"the same links folded over BOF3SH4's 0x{SH4_EXTENT:X} (the .pool links raw): differ", links_sh4, 1)
    expect('a byte changed in a moved link into .pool, folded: differs', links_changed, 1)

    def sh(path, version, image=IMAGE, fold=0, extent=IMAGE_EXTENT):
        with open(path, 'wb') as f:
            f.write(b'BOF3SH%d\0' % version + struct.pack('<4I', BASE, 4, 0x1000, 0) + struct.pack('<2I', 0, 3))
            if version >= 4:
                f.write(struct.pack('<2I', image, fold))
            if version >= 5:
                f.write(struct.pack('<I', extent))
            f.write(struct.pack('<4I', 0, 1, 0, 4) + struct.pack('<8I', 0, 7, 1, 8, 2, 9, 3, 10))
            f.write(struct.pack('<4I', 1, 2, 1, 1) + struct.pack('<2I', 2, 11))
    with tempfile.TemporaryDirectory() as t:
        paths = {}
        for name, args in (('sh3', (3,)), ('sh4', (4,)), ('moved', (4, F, 0)), ('folding', (4, IMAGE, F)),
                           ('other', (4, IMAGE, 0x60000000)), ('sh5', (5,)), ('moved5', (5, F, 0)),
                           ('folding5', (5, IMAGE, F)), ('other5', (5, IMAGE, 0x60000000))):
            paths[name] = os.path.join(t, name + '.sh')
            sh(paths[name], *args)
        runs = {k: Run(v) for k, v in paths.items()}
        expect('a BOF3SH4 run at 0x400000 folds', [hex(b) for b in sorted(runs['sh4'].folds)], [])
        expect('a moved run folds', [hex(b) for b in sorted(runs['moved'].folds)], [hex(F)])
        expect('a run at 0x400000 folding the moved base folds', [hex(b) for b in sorted(runs['folding'].folds)], [hex(F)])
        expect('a BOF3SH5 run at 0x400000 folds', [hex(b) for b in sorted(runs['sh5'].folds)], [])
        expect('the extents read: BOF3SH3, BOF3SH4, BOF3SH5',
               [None if runs[x].extent is None else hex(runs[x].extent) for x in ('sh3', 'sh4', 'sh5')],
               [None, hex(SH4_EXTENT), hex(IMAGE_EXTENT)])
        expect('the records read alike across versions',
               runs['sh3'].recs == runs['sh4'].recs == runs['moved'].recs == runs['sh5'].recs == runs['moved5'].recs,
               True)

        def compared(x, y):
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    same_headers([runs[x], runs[y]])
                return 'compared'
            except SystemExit:
                return 'refused'
        expect('BOF3SH3 against BOF3SH4 at 0x400000', compared('sh3', 'sh4'), 'compared')
        expect('the moved run against a run at 0x400000 folding its base', compared('folding', 'moved'), 'compared')
        expect('the moved run against a run that folded nothing', compared('sh4', 'moved'), 'refused')
        expect('two runs folding different ranges', compared('folding', 'other'), 'refused')
        expect('BOF3SH3 against BOF3SH5 at 0x400000', compared('sh3', 'sh5'), 'compared')
        expect('BOF3SH4 against BOF3SH5 at 0x400000, nothing folded (the extents differ)', compared('sh4', 'sh5'),
               'compared')
        expect('a moved BOF3SH5 run against a BOF3SH5 run folding its base', compared('folding5', 'moved5'), 'compared')
        expect('a moved BOF3SH5 run against a BOF3SH4 run folding its base (the extents differ)',
               compared('folding', 'moved5'), 'refused')
        expect('a moved BOF3SH4 run against a BOF3SH5 run folding its base (the extents differ)',
               compared('folding5', 'moved'), 'refused')
        expect('two BOF3SH5 runs folding different ranges', compared('folding5', 'other5'), 'refused')
    print('selftest ' + ('passed' if ok else 'FAILED'))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)

    def window(p):
        p.add_argument('--from', dest='lo', type=int, default=0)
        p.add_argument('--to', dest='hi', type=int, default=None)
        p.add_argument('--align', choices=['tick', 'recipe'], default='tick')
        p.add_argument('--strict', action='store_true',
                       help='refuse runs hashed under different skip lists, even when one only adds ranges')
        p.add_argument('--skip-list', action='append', default=[], metavar='FILE',
                       help='a skip list to know by its header hash, besides the committed versions of ' + SKIP_FILE)

    p = sub.add_parser('info'); p.add_argument('run'); p.set_defaults(fn=cmd_info)
    p = sub.add_parser('diff'); p.add_argument('a'); p.add_argument('b'); window(p); p.set_defaults(fn=cmd_diff)
    p = sub.add_parser('check'); p.add_argument('ref'); p.add_argument('refb'); p.add_argument('new'); window(p)
    p.add_argument('--noise', action='store_true', help='list the noise pages too'); p.set_defaults(fn=cmd_check)
    p = sub.add_parser('bytes'); p.add_argument('a'); p.add_argument('b'); p.add_argument('--skip-out')
    p.add_argument('--gap', type=int, default=3, help='a range spans runs of equal bytes shorter than this (default 3: up to 2)')
    p.add_argument('--fold', type=lambda x: int(x, 0), default=None, metavar='ADDR',
                   help='read every aligned word in the image placed at ADDR as at 0x400000, in both dumps')
    p.set_defaults(fn=cmd_bytes)
    p = sub.add_parser('selftest'); p.set_defaults(fn=cmd_selftest)
    a = ap.parse_args()
    sys.exit(a.fn(a) or 0)


if __name__ == '__main__':
    main()
