#!/usr/bin/env python
"""Pair the sibling's named PSX functions with their twins in the PSP's BOOT.BIN.

    python tools/psp_pair.py pair --psp-jp ISO [--psp-eu ISO] [--check-eu] --out <scratch>/pairs.json
    python tools/psp_pair.py diff --psp-jp ISO [--psp-eu ISO] --out <scratch>/diff.json [--md <scratch>/table.md]

docs/psp-code-diff.md has the measurements; `diff` is described at diff_pair.

The PSP ELF is a native compile of the PSX boot EXE and its overlays by a
different compiler (docs/psp-stallion.md 3.1): registers, scheduling and
delay slots differ, so PSX and PSP words never match as they stand. The
pairing therefore works on what a compiler keeps:

  - constants: the immediates of li / andi / ori / xori / slti / sltiu /
    addiu that are not address halves (on the PSP the relocation table says
    which halves are addresses; on the PSX a lui of 0x8000..0x8020 or 0x1F80),
    a lui / ori pair as one 32-bit value, a division reciprocal as its divisor;
  - field offsets: load / store offsets on a base that is not sp and not a
    tracked global address, with the access width;
  - callees and code pointers, in order of first use, once paired (an overlay's
    call resolves into its own file, then the resident BATTLE.EMI and GAME.EMI;
    a call clobbers registers only after its delay slot);
  - globals: the data addresses a function reads or writes (an indexed access
    counts as its array's base), once a PSX address has a learned PSP twin.

It runs in rounds until nothing moves:
  1. seed: functions whose constants and field offsets are unique and equal;
  2. data map: paired functions' global lists aligned, two agreeing votes;
  3. callee alignment: a pair's callee lists aligned through known pairs;
  4. caller vote: an unpaired callee whose paired callers' twins all call one
     unpaired PSP function;
  5. gaps: the PSP links each family of overlays (BATTLE, BMAGIC, BOSS, ETC,
     PLCHAR, SCENARIO, the WORLDs) in the PSX's file order, so the paired
     functions of a family in that order are anchors, and between two anchors
     equal counts pair by position, others by an order-keeping alignment;
  6. similarity: weighted (rarity) multiset similarity, mutual best with a
     margin; last, 'similarity-weak' (0.3, 1.3x the runner-up), tagged.
A pair whose paired callees contradict it on both sides is dropped at the end.

Equal copies (the same words, at the same load address on the PSX) are one
class on each side: AREA overlays repeat helpers, the PSP repeats some too.

PSP-JP is paired against the sibling's PSX-JP. PSP-EU is mapped from PSP-JP
by unique relocation-masked words, then by order between those anchors.
--check-eu also pairs PSX -> PSP-EU independently and counts agreement: a
consistency measure, not a precision one (both sides share the method).

Function starts. PSX: the sibling's analysis/functions.tsv (without its
'low' and 'data' rows, blobs that would stretch the boot EXE's code range over
GAME.EMI) and symbols.toml for the boot EXE; the captures' header and engine entries; every jal target;
every stored word pointing at a stack-frame prologue. The captures'
static-discovery lists are not used: they hold case labels, and so does
names/functions.toml (24 names), which pair through their containing
function. PSP: jal targets and .data code pointers. On both, a function ends
at the first `jr ra` (or tail `j`) that no forward branch passes - past a
jump table, at a `jr ra` followed by a stack-frame prologue - and the code
after it starts a new function unless it is padding.

Reads the sibling checkout (--sibling) and the player's PSP disc images
through tools/psx_disc.py. Output is addresses, counts and the sibling's
names only - never code bytes - and lives in scratch (CLAUDE.md rule 1).
"""
import argparse, base64, bisect, collections, json, math, os, struct, sys, tomllib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

SP, FP, ZERO, RA = 29, 30, 0, 31
JR_RA = 0x03E00008
PSX_BOOT_FILE_LO = 0x80093800   # SLPS_009.90 after its 0x800-byte header
CALLER_SAVED = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 31)
LOADS = {0x20: 1, 0x21: 2, 0x22: 4, 0x23: 4, 0x24: 1, 0x25: 2, 0x26: 4}
STORES = {0x28: 1, 0x29: 2, 0x2A: 4, 0x2B: 4, 0x2E: 4}
COPMEM = {0x31, 0x32, 0x35, 0x36, 0x39, 0x3A, 0x3D, 0x3E}     # lwc1/lwc2(lv.s)/.../swc2(sv.s)/sv.q


def simm(w):
    v = w & 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def find_sibling():
    here = os.path.dirname(os.path.abspath(__file__))
    for _ in range(8):
        cand = os.path.join(here, 'BreathOfFire3Recomp')
        if os.path.isfile(os.path.join(cand, 'symbols.toml')):
            return cand + os.sep
        here = os.path.dirname(here)
    return None


# ---------------------------------------------------------------- code spaces

class Space:
    """One address space of code: lo, bytes, and (PSP) relocation facts."""

    def __init__(self, name, lo, data):
        self.name, self.lo, self.data = name, lo, data
        self.hi = lo + (len(data) & ~3)
        self.addr_hi = {}   # PSP: address of a lui -> segment base to add (HI16 relocation)
        self.addr_lo = set()  # PSP: addresses whose low half is an address part (LO16)
        self.r26 = set()    # PSP: j / jal words with an R_MIPS_26

    def word(self, x):
        return struct.unpack_from('<I', self.data, x - self.lo)[0]


def branch_target(x, w):
    op = w >> 26
    if op in (4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17) or op == 1:
        return x + 4 + simm(w) * 4
    if op in (0x10, 0x11, 0x12, 0x13) and ((w >> 21) & 31) == 8:   # bc0..3 / bvf / bvt
        return x + 4 + simm(w) * 4
    return None


def is_jr_reg(w):
    return w >> 26 == 0 and w & 63 == 8 and (w >> 21) & 31 != RA


def written(w):
    """General registers an instruction writes (approximate, for clobbering)."""
    op = w >> 26
    rs, rt, rd = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
    if op == 0:
        f = w & 63
        if f in (8, 12, 13, 0x18, 0x19, 0x1A, 0x1B, 0x11, 0x13):
            return ()
        return (rd,)
    if op == 1:
        return (RA,) if rt in (0x10, 0x11, 0x12, 0x13) else ()
    if op == 3:
        return CALLER_SAVED
    if 8 <= op <= 0x0F or op in LOADS:
        return (rt,)
    if op in (0x10, 0x11, 0x12, 0x13):
        return (rt,) if rs in (0, 2, 3) else ()
    if op in (0x1C, 0x1F):
        return (rt, rd)
    return ()


def bounds(space, starts):
    """[start, end) of each start: to the first unpassed jr ra / tail j, or the next start."""
    out = {}
    for i, s in enumerate(starts):
        nxt = starts[i + 1] if i + 1 < len(starts) else space.hi
        nxt = min(nxt, space.hi)
        x, maxbr, switch, end = s, s, False, nxt
        while x + 4 <= nxt:
            w = space.word(x)
            t = branch_target(x, w)
            if t is not None and s <= t < nxt:
                maxbr = max(maxbr, t)
            op = w >> 26
            if op == 2:
                t = ((x + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
                if s <= t < nxt and t > x:
                    maxbr = max(maxbr, t)
                elif not (s <= t < nxt) and maxbr <= x + 4 and not switch:
                    end = x + 8; break
            if is_jr_reg(w):
                switch = True
            if w == JR_RA and maxbr <= x + 4 and not switch:
                end = x + 8; break
            # past a jump table the targets are unknown: a stack-frame prologue right
            # after the return still starts the next function
            if w == JR_RA and switch and x + 12 <= nxt and space.word(x + 8) >> 16 == 0x27BD \
                    and space.word(x + 8) & 0x8000:
                end = x + 8; break
            x += 4
        out[s] = min(end, nxt)
    return out


def sweep(space, starts):
    """Add a start after each function end that is not padding (unreferenced code)."""
    starts = sorted(set(s for s in starts if space.lo <= s < space.hi))
    for _ in range(6):
        b = bounds(space, starts)
        new = set()
        for s in starts:
            e = b[s]
            while e + 4 <= space.hi and space.word(e) == 0:
                e += 4
            if e + 4 <= space.hi and e not in b:
                j = bisect.bisect_left(starts, e)
                if j < len(starts) and starts[j] == e:
                    continue
                new.add(e)
        if not new:
            return starts, b
        starts = sorted(set(starts) | new)
    return starts, bounds(space, starts)


# ---------------------------------------------------------------- features

def _magic():
    """Reciprocal multipliers a compiler uses for x / d, back to d."""
    out = {}
    for d in range(3, 10001):
        if d & (d - 1) == 0:
            continue
        for sh in range(0, 12):
            m = -(-(1 << (32 + sh)) // d)
            if m < (1 << 32):
                out.setdefault(m, d)
    return out


MAGIC = _magic()


def const_norm(v):
    """A 32-bit constant; a division reciprocal becomes its divisor (the PSP may divide)."""
    return MAGIC.get(v, v)


class Func:
    __slots__ = ('key', 'space', 'start', 'end', 'n', 'consts', 'fields', 'calls', 'refs', 'fptrs', 'stubs', 'mhash', 'callargs', 'mults', 'gte')

    def __init__(self, key, space, start, end):
        self.key, self.space, self.start, self.end = key, space, start, end


def extract(f, space, is_psp, resolve, is_code_addr):
    """Constants, field offsets, ordered callees and ordered globals of one function."""
    consts, fields = collections.Counter(), collections.Counter()
    calls, refs, fptrs, callargs, mults = [], [], [], [], []
    gte = 0              # PSX: GTE (cop2) instructions; PSP: VFPU ones
    regs = {}            # reg -> ('A', address) | ('K', value)
    mw = []              # the words, intra-function jumps relative: equal copies hash equal.
    # Copies must be equal in their address fields too (same callees, same data): on the
    # PSP the image is unrelocated, so equal words are equal targets; on the PSX two
    # overlays are copies only at the same load address.
    pend = {}            # reg -> address formed but not yet used as a base
    n = 0

    def flush(r):
        a = pend.pop(r, None)
        if a is not None:
            if is_code_addr(a):
                fptrs.append(resolve(a))
            else:
                refs.append(a)

    clobber_at = None    # a call clobbers the caller-saved registers after its delay slot
    for x in range(f.start, f.end, 4):
        if clobber_at is not None and x >= clobber_at:
            for r in CALLER_SAVED:
                flush(r); regs.pop(r, None)
            clobber_at = None
        w = space.word(x)
        if w == 0:
            continue
        n += 1
        op = w >> 26
        if (not is_psp and op in (0x12, 0x32, 0x3A)) or (is_psp and op in (0x18, 0x19, 0x1B, 0x34, 0x36, 0x37, 0x3C, 0x3E, 0x3F)):
            gte += 1
        rs, rt = (w >> 21) & 31, (w >> 16) & 31
        imm = w & 0xFFFF
        if op == 2 and f.start <= (((x + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)) < f.end:
            mw.append((w & 0xFC000000, (w & 0x3FFFFFF) - (f.start >> 2 & 0x3FFFFFF)))
        else:
            mw.append(w)
        if op == 3:
            t = ((x + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            calls.append(resolve(t))
            # the call's constant arguments (a0..a3, the delay slot's li included): a
            # platform call's arguments are the platform's, set aside by the diff
            args = [regs[r][1] for r in (4, 5, 6, 7) if r in regs and regs[r][0] == 'K' and regs[r][1]]
            if x + 8 <= f.end:
                d = space.word(x + 4)
                if d >> 26 in (0x09, 0x0D) and (d >> 21) & 31 == ZERO and 4 <= (d >> 16) & 31 <= 7 and d & 0xFFFF:
                    args.append(d & 0xFFFF if d >> 26 == 0x0D else simm(d))
            callargs.append((calls[-1], tuple(args)))
        elif op == 2:
            t = ((x + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
            if not (f.start <= t < f.end):
                calls.append(resolve(t))
        if op == 0x0F:   # lui
            if is_psp:
                base = space.addr_hi.get(x)
                isaddr = base is not None
                val = (imm << 16) + (base or 0)
            else:
                isaddr = 0x8000 <= imm <= 0x8020 or imm == 0x1F80
                val = imm << 16
            flush(rt)
            if isaddr:
                regs[rt] = ('A', val)
            else:
                regs[rt] = ('K', val)
                consts[val] += 1
            continue
        if op in (0x08, 0x09, 0x0D) and rs in regs and regs[rs][0] == 'A' and \
                (not is_psp or x in space.addr_lo or op in (0x08, 0x09)):   # base + member offset
            a = regs[rs][1] + (imm if op == 0x0D else simm(w))
            a &= 0xFFFFFFFF
            if rt != rs:
                flush(rt)
            pend.pop(rs, None) if rt == rs else None
            regs[rt] = ('A', a)
            pend[rt] = a
            continue
        if op in (0x08, 0x09) and (rs == SP or rt == SP):
            if rt in regs:
                flush(rt); regs.pop(rt, None)
            continue
        if op in (0x09, 0x0D) and rs in regs and regs[rs][0] == 'K' and regs[rs][1] & 0xFFFF == 0 and regs[rs][1]:
            hi = regs[rs][1]                     # lui K; ori/addiu: one 32-bit constant
            full = (hi | imm) if op == 0x0D else (hi + simm(w)) & 0xFFFFFFFF
            consts[hi] -= 1
            if not consts[hi]:
                del consts[hi]
            consts[const_norm(full)] += 1
            flush(rt)
            regs[rt] = ('K', full)
            continue
        if 0x08 <= op <= 0x0E:
            v = imm if op in (0x0C, 0x0D, 0x0E) else simm(w)
            if v:                    # a zero is a move or a clear, however compiled
                consts[v] += 1
            flush(rt)
            if rs == ZERO:
                regs[rt] = ('K', v)
            else:
                regs.pop(rt, None)
            continue
        if op in LOADS or op in STORES or op in COPMEM:
            off = simm(w) if op not in (0x32, 0x36, 0x3A, 0x3E) or not is_psp else simm(w & 0xFFFC)
            width = LOADS.get(op) or STORES.get(op) or 4
            if rs in regs and regs[rs][0] == 'A' and (not is_psp or rs != SP):
                a = (regs[rs][1] + off) & 0xFFFFFFFF
                pend.pop(rs, None)
                refs.append(a)
            elif rs not in (SP, FP):
                fields[(width, off)] += 1
            if op in STORES and rt in pend:
                flush(rt)
            if op in LOADS:
                flush(rt); regs.pop(rt, None)
            continue
        if op == 0 and w & 63 in (0x18, 0x19):          # mult by a constant: the PSX shifts and adds instead
            for r in (rs, rt):
                if r in regs and regs[r][0] == 'K' and consts.get(regs[r][1]):
                    consts[regs[r][1]] -= 1
                    if not consts[regs[r][1]]:
                        del consts[regs[r][1]]
                    mults.append(regs[r][1])
        if op == 0 and w & 63 in (0x20, 0x21, 0x25):   # add / addu / or: base + index
            rd = (w >> 11) & 31
            ra_, rb_ = regs.get(rs), regs.get(rt)
            base = ra_[1] if ra_ and ra_[0] == 'A' else rb_[1] if rb_ and rb_[0] == 'A' else None
            if base is not None and rd not in (ZERO, SP):
                for r in (rs, rt):
                    pend.pop(r, None)
                flush(rd)
                regs[rd] = ('A', base)      # an element of the array at base
                continue
        if op == 3:
            clobber_at = x + 8
        for r in (written(w) if op != 3 else ()):
            if r:
                flush(r); regs.pop(r, None)
        if op == 0:   # an address register used as an operand escapes
            for r in (rs, rt):
                if r in pend:
                    flush(r)
    for r in list(pend):
        flush(r)
    f.n, f.consts, f.fields, f.calls, f.refs, f.fptrs = n, consts, fields, calls, refs, fptrs
    f.callargs, f.mults = callargs, mults
    f.gte = gte
    f.mhash = hash((0 if is_psp else space.lo, tuple(mw)))


# ---------------------------------------------------------------- the PSX side

class Psx:
    def __init__(self, sib):
        self.sib = sib
        boot = open(sib + 'disc/SLPS_009.90', 'rb').read()[0x800:]
        self.spaces = {'boot': Space('boot', PSX_BOOT_FILE_LO, boot)}
        self.caps = {}
        self.names = {}       # (space, addr) -> (name, origin)
        self.lib = set()      # (space, addr): Psy-Q library and runtime
        boot_starts = set()
        for line in open(sib + 'analysis/functions.tsv'):
            p = line.split('\t')
            # 'low' rows are blobs (the overlay region, zero fill): kept out, or the boot
            # EXE's code range would swallow GAME.EMI's
            if line.startswith('0x') and len(p) > 3 and p[3] not in ('data', 'low'):
                boot_starts.add(int(p[0], 16))
        for f in tomllib.load(open(sib + 'symbols.toml', 'rb'))['func']:
            boot_starts.add(f['pc'])
            self.names[('boot', f['pc'])] = (f['name'], 'symbols.toml')
            if f['note'].startswith('Psy-Q') or f['name'] in ('BootEntry',):
                self.lib.add(('boot', f['pc']))
        self.code_lo = min(s for s in boot_starts if s >= 0x80100000)
        self.code_hi = max(boot_starts) + 0x400
        # the Psy-Q libraries are linked as one block at the end of the boot EXE: from
        # the first function the sibling notes as Psy-Q library code on, all of it
        self.lib_lo = min(f['pc'] for f in tomllib.load(open(sib + 'symbols.toml', 'rb'))['func']
                          if f['note'].startswith('Psy-Q L'))
        ov_starts = collections.defaultdict(set)
        for f in tomllib.load(open(sib + 'names/functions.toml', 'rb'))['function']:
            self.names[(f['overlay'], f['pc'])] = (f['name'], 'names/functions.toml')
        self.by_file = collections.defaultdict(list)
        for c in json.load(open(sib + 'analysis/overlay_captures_all.json')):
            if c['source_file'].startswith('LOGO'):
                continue
            lo = int(c['load_addr'], 16) if isinstance(c['load_addr'], str) else c['load_addr']
            m = c['source_md5']
            self.spaces[m] = Space(m, lo, base64.b64decode(c['bytes_b64']))
            self.caps[m] = c
            self.by_file[c['source_file']].append(m)
            # The captures' static_discovery lists hold case labels as well as starts, so
            # they are not used: a named label inside a function pairs through that function.
            for key in ('header_entry_pcs', 'engine_entry_pcs'):
                v = c.get(key, [])
                if isinstance(v, str):
                    v = [s.strip(" '") for s in v.strip('[]').split(',') if s.strip(" '")]
                ov_starts[m].update(int(s, 16) for s in v)
        self.starts = {'boot': boot_starts}
        self.starts.update(ov_starts)
        # every jal target, resolved to its space; and every stored word that points
        # at a stack-frame prologue in its own space (handler and state tables)
        for name, sp in self.spaces.items():
            lo, hi = (self.code_lo, self.code_hi) if name == 'boot' else (sp.lo, sp.hi)
            st = self.starts.setdefault(name, set())
            for x in range(sp.lo, sp.hi - 3, 4):
                w = sp.word(x)
                if lo <= w < hi and w & 3 == 0 and sp.lo <= w < sp.hi and sp.word(w) >> 16 == 0x27BD                         and sp.word(w) & 0x8000:
                    st.add(w)
                if not lo <= x < hi:
                    continue
                if w >> 26 == 3:
                    t = ((x + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
                    r = self.resolve(name, t)
                    if r:
                        self.starts.setdefault(r[0], set()).add(r[1])

    def order(self, k):
        """A function's place in its family's link order (see Matcher)."""
        if k[0] == 'boot':
            return ('boot', ('', 0, k[1]))
        c = self.caps[k[0]]
        fam = c['source_file'].split('/')[1]
        fam = 'WORLD' if fam.startswith('WORLD') else fam
        return (fam, (c['source_file'], c['source_index'], k[1]))

    def file_of(self, space):
        if space == 'boot':
            return 'SLPS_009.90'
        c = self.caps[space]
        return '%s#%d' % (c['source_file'], c['source_index'])

    def resolve(self, space, t):
        sp = self.spaces[space]
        if space != 'boot' and sp.lo <= t < sp.hi:
            return (space, t)
        if self.code_lo <= t < self.code_hi:
            return ('boot', t)
        if space != 'boot':
            src = self.caps[space]['source_file']
            fam = src.split('/')[1]
            # the same file's other sections, then the resident modules: BATTLE.EMI
            # under the battle families, GAME.EMI under all
            # GAME.EMI stays resident under a battle as well
            resident = ('BIN/BATTLE/BATTLE.EMI', 'BIN/ETC/GAME.EMI') if fam in ('BATTLE', 'BMAGIC', 'BOSS')                 else ('BIN/ETC/GAME.EMI',)
            for f in (src,) + resident:
                for m in self.by_file[f]:
                    o = self.spaces[m]
                    if m != space and o.lo <= t < o.hi:
                        return (m, t)
        return None

    def functions(self):
        out = {}
        for name, sp in self.spaces.items():
            st = sorted(s for s in self.starts.get(name, ()) if sp.lo <= s < sp.hi
                        and (name != 'boot' or self.code_lo <= s < self.code_hi))
            if not st:
                continue
            st, b = sweep(sp, st)
            for s in st:
                if name == 'boot' and s >= self.code_hi:
                    continue
                out[(name, s)] = Func((name, s), name, s, b[s])
        self.functions_keys = set(out)
        self.lib |= set(k for k in out if k[0] == 'boot' and k[1] >= self.lib_lo)
        # a name that is not a start lies inside a function: say which
        self.inside = {}
        by_space = collections.defaultdict(list)
        for k in out:
            by_space[k[0]].append(k[1])
        for v in by_space.values():
            v.sort()
        for k in self.names:
            if k in out:
                continue
            v = by_space.get(k[0], [])
            i = bisect.bisect_right(v, k[1]) - 1
            if i >= 0 and k[1] < out[(k[0], v[i])].end:
                self.inside[k] = (k[0], v[i])
        for k, f in out.items():
            def resolve(t, sp=f.space):
                return self.resolve(sp, t) or ('?', t)

            def is_code(a, sp=f.space):
                return self.resolve(sp, a) in out
            extract(f, self.spaces[f.space], False, resolve, is_code)
        return out


# ---------------------------------------------------------------- the PSP side

class Psp:
    def __init__(self, path):
        if path.lower().endswith('.iso'):
            import psx_disc
            d = psx_disc.Disc(path)
            data = d.read('PSP_GAME/SYSDIR/BOOT.BIN')
        else:
            data = open(path, 'rb').read()
        self.raw = data
        assert data[:4] == b'\x7fELF'
        phoff, = struct.unpack_from('<I', data, 28)
        _, off0, va0, _, fs0, _, _, _ = struct.unpack_from('<8I', data, phoff)
        _, off1, va1, _, fs1, ms1, _, _ = struct.unpack_from('<8I', data, phoff + 32)
        self.seg = [va0, va1]
        img = data[off0:off0 + fs0]
        e_shoff, = struct.unpack_from('<I', data, 32)
        es, n, si = struct.unpack_from('<HHH', data, 46)
        shs = [struct.unpack_from('<10I', data, e_shoff + i * es) for i in range(n)]
        so = shs[si][4]
        nm = lambda s: data[so + s[0]:data.index(b'\0', so + s[0])].decode()
        sec = {nm(s): s for s in shs}
        self.text_end = sec['.text'][3] + sec['.text'][5]
        self.stub_lo = self.text_end
        self.stub_hi = sec['.lib.ent.top'][3]
        self.space = Space('psp', 0, img[:self.stub_hi])
        self.data_ptrs = set()
        for s in shs:
            if s[1] != 0x700000A0:
                continue
            target = shs[s[7]] if s[7] < len(shs) else None
            tname = nm(target) if target else ''
            for i in range(s[5] // 8):
                ro, info = struct.unpack_from('<II', data, s[4] + i * 8)
                typ, ob, ab = info & 0xFF, (info >> 8) & 0xFF, (info >> 16) & 0xFF
                loc = ro + self.seg[ob]
                if tname == '.text':
                    if typ == 5:
                        self.space.addr_hi[loc] = self.seg[ab]
                    elif typ == 6:
                        self.space.addr_lo.add(loc)
                    elif typ == 4:
                        self.space.r26.add(loc)
                elif typ == 2 and tname == '.data' and ab == 0:
                    v, = struct.unpack_from('<I', img, loc)
                    if v < self.text_end:
                        self.data_ptrs.add(v)

    def is_stub(self, a):
        return self.stub_lo <= a < self.stub_hi

    def functions(self):
        sp = self.space
        st = set(self.data_ptrs)
        for x in range(0, self.text_end, 4):
            w = sp.word(x)
            if w >> 26 == 3:
                t = (w & 0x3FFFFFF) << 2
                if t < self.text_end:
                    st.add(t)
        st, b = sweep(Space('t', 0, sp.data[:self.text_end]), sorted(st | {0}))
        out = {}
        keys = set(st)
        for s in st:
            f = Func(s, 'psp', s, b[s])
            extract(f, sp, True, lambda t: t, lambda a: a in keys)
            f.stubs = sum(1 for c in f.calls if self.is_stub(c))
            out[s] = f
        return out

    def masked_hash(self, f):
        """Words of f with every relocated field zeroed (j/jal 26 bits, hi/lo 16)."""
        sp = self.space
        h = []
        for x in range(f.start, f.end, 4):
            w = sp.word(x)
            if x in sp.r26:
                w &= 0xFC000000
            elif x in sp.addr_hi or x in sp.addr_lo:
                w &= 0xFFFF0000
            h.append(w)
        return hash(tuple(h))


# ---------------------------------------------------------------- matching

def align_dp(sc, floor):
    """Order-keeping alignment of a similarity matrix: the monotone matching that
    maximises the sum of (sim - floor / 2) over pairs with sim >= floor."""
    na, nb = len(sc), len(sc[0]) if sc else 0
    best = [[0.0] * (nb + 1) for _ in range(na + 1)]
    for i in range(na - 1, -1, -1):
        row, nxt = best[i], best[i + 1]
        for j in range(nb - 1, -1, -1):
            v = max(nxt[j], row[j + 1])
            s = sc[i][j]
            if s >= floor:
                v = max(v, s - floor / 2 + nxt[j + 1])
            row[j] = v
    out, i, j = [], 0, 0
    while i < na and j < nb:
        s = sc[i][j]
        if s >= floor and abs(best[i][j] - (s - floor / 2 + best[i + 1][j + 1])) < 1e-12:
            out.append((i, j)); i += 1; j += 1
        elif best[i][j] == best[i + 1][j]:
            i += 1
        else:
            j += 1
    return out


def lis(xs):
    """Indices of a longest strictly increasing subsequence of xs."""
    tails, tails_i, prev = [], [], [None] * len(xs)
    for i, x in enumerate(xs):
        j = bisect.bisect_left(tails, x)
        if j == len(tails):
            tails.append(x); tails_i.append(i)
        else:
            tails[j], tails_i[j] = x, i
        prev[i] = tails_i[j - 1] if j else None
    out, i = [], tails_i[-1] if tails_i else None
    while i is not None:
        out.append(i); i = prev[i]
    return out[::-1]


def copies(side):
    groups = collections.defaultdict(list)
    for k, f in side.items():
        if f.n >= 6:
            groups[(f.mhash, f.n)].append(k)
    rep = {}
    for ks in groups.values():
        if len(ks) > 1:
            ks.sort(key=lambda k: (k[0] != 'boot', k) if isinstance(k, tuple) else (False, k))
            for k in ks:
                rep[k] = ks[0]
    return rep


TRIVIAL = {0, 1, -1, 2, 4, 8, 0xFF, 0xFFFF, 0x10, 3}


class Matcher:
    def __init__(self, A, B, order=None):
        # order(k) -> (sequence, position): the PSP links a family of overlays (the battle
        # magic, the bosses, the areas...) in the PSX's file order, so gaps are filled
        # along a family, across file boundaries; by default a space is its own sequence.
        self.order = order or (lambda k: (k[0], k[1]))
        # Equal copies (the same words, 6 or more instructions) are one class on each
        # side; the matcher sees one representative and full() expands the result.
        self.rep_a, self.rep_b = copies(A), copies(B)
        for side, rep in ((A, self.rep_a), (B, self.rep_b)):
            for f in side.values():
                f.calls = [rep.get(c, c) for c in f.calls]
                f.fptrs = [rep.get(c, c) for c in f.fptrs]
        self.fullA, self.fullB = A, B
        A = {k: f for k, f in A.items() if self.rep_a.get(k, k) == k}
        B = {k: f for k, f in B.items() if self.rep_b.get(k, k) == k}
        self.A, self.B = A, B          # dict key -> Func (A = PSX, B = PSP)
        self.a2b, self.b2a, self.how = {}, {}, {}
        self.gmap = {}                 # PSX data address -> PSP data address
        self.callers_a = collections.defaultdict(set)
        self.callers_b = collections.defaultdict(set)
        for k, f in A.items():
            for c in f.calls:
                self.callers_a[c].add(k)
        for k, f in B.items():
            for c in f.calls:
                self.callers_b[c].add(k)
        df = collections.Counter()
        for side in (A, B):
            for f in side.values():
                for t in set(self.static_tokens(f)):
                    df[t] += 1
        N = len(A) + len(B)
        self.idf = {t: math.log(N / c) for t, c in df.items()}
        self.ga_sorted = []

    @staticmethod
    def static_tokens(f):
        out = [('c', v) for v, c in f.consts.items() for _ in range(c)]
        out += [('f',) + k for k, c in f.fields.items() for _ in range(c)]
        return out

    def add(self, a, b, how):
        if a in self.a2b or b in self.b2a:
            return False
        self.a2b[a], self.b2a[b], self.how[a] = b, a, how
        return True

    # -- tokens with the current maps applied
    def map_ref_a(self, a):
        if a in self.gmap:
            return self.gmap[a]
        i = bisect.bisect_right(self.ga_sorted, a) - 1
        if i >= 0:
            base = self.ga_sorted[i]
            if a - base < 0x40:
                return self.gmap[base] + (a - base)
        return None

    def tokens_a(self, f):
        t = collections.Counter(self.static_tokens(f))
        for c in f.calls:
            t[('k', self.a2b.get(c, '?'))] += 1
        for c in f.fptrs:
            t[('p', self.a2b.get(c, '?'))] += 1
        for r in f.refs:
            m = self.map_ref_a(r)
            t[('g', m if m is not None else '?')] += 1
        return t

    def tokens_b(self, f):
        t = collections.Counter(self.static_tokens(f))
        for c in f.calls:
            t[('k', c if c in self.b2a else '?')] += 1
        for c in f.fptrs:
            t[('p', c if c in self.b2a else '?')] += 1
        gb = self.gb_set
        for r in f.refs:
            t[('g', r if r in gb else '?')] += 1
        return t

    def w(self, t):
        if t[0] in ('k', 'p', 'g'):
            return 0.3 if t[1] == '?' else 4.0
        if t[0] == 'c' and t[1] in TRIVIAL:
            return 0.3
        return self.idf.get(t, 3.0)

    def sim(self, ta, tb, fa, fb):
        num = den = 0.0
        for t in set(ta) | set(tb):
            x, y = ta.get(t, 0), tb.get(t, 0)
            w = self.w(t)
            num += w * min(x, y)
            den += w * max(x, y)
        s = num / den if den else 0.0
        r = (fb.n + 1) / (fa.n + 1)
        if r > 2.5 or r < 0.4:
            s *= 0.5
        return s

    def refresh(self):
        self.ga_sorted = sorted(self.gmap)
        self.gb_set = set(self.gmap.values())

    # -- round 1: unique exact signatures
    def seed(self):
        def sig(f):
            c = tuple(sorted(f.consts.items()))
            fl = tuple(sorted(f.fields.items()))
            return (c, fl, len(f.calls))

        def informative(f):
            return sum(1 for v in f.consts if v not in TRIVIAL) + len(f.fields) >= 4

        sa, sb = collections.defaultdict(list), collections.defaultdict(list)
        for k, f in self.A.items():
            if informative(f):
                sa[sig(f)].append(k)
        for k, f in self.B.items():
            if informative(f):
                sb[sig(f)].append(k)
        n = 0
        for s, ks in sa.items():
            if len(ks) == 1 and len(sb.get(s, ())) == 1:
                fa, fb = self.A[ks[0]], self.B[sb[s][0]]
                r = (fb.n + 1) / (fa.n + 1)
                if 0.4 <= r <= 2.5:
                    n += self.add(ks[0], sb[s][0], 'signature')
        return n

    # -- alignment of two ordered lists through a known equivalence
    @staticmethod
    def gaps(la, lb, same):
        """Positions between agreeing anchors where both gaps are of equal length."""
        anchors = [(-1, -1)]
        j0 = 0
        for i, x in enumerate(la):
            for j in range(j0, len(lb)):
                if same(x, lb[j]):
                    anchors.append((i, j)); j0 = j + 1
                    break
        anchors.append((len(la), len(lb)))
        out = []
        for (i0, j0), (i1, j1) in zip(anchors, anchors[1:]):
            if i1 - i0 == j1 - j0 and i1 - i0 > 1:
                out += [(la[i], lb[j0 + (i - i0)]) for i in range(i0 + 1, i1)]
        return out

    def propagate_calls(self):
        n = 0
        for a, b in list(self.a2b.items()):
            fa, fb = self.A[a], self.B[b]
            la = list(dict.fromkeys(c for c in fa.calls + fa.fptrs if c in self.A))
            lb = list(dict.fromkeys(c for c in fb.calls + fb.fptrs if c in self.B))
            for x, y in self.gaps(la, lb, lambda x, y: self.a2b.get(x) == y):
                if x in self.a2b or y in self.b2a:
                    continue
                ga, gb = self.A[x], self.B[y]
                s = self.sim(self.tokens_a(ga), self.tokens_b(gb), ga, gb)
                small = ga.n < 12 and gb.n < 12 and 0.5 <= (gb.n + 1) / (ga.n + 1) <= 2
                if s >= 0.35 or (small and s >= 0.2):
                    n += self.add(x, y, 'callee-align')
        return n

    def caller_vote(self):
        """An unpaired PSX callee whose paired callers' twins all call one unpaired PSP function."""
        n = 0
        for k, callers in list(self.callers_a.items()):
            if k in self.a2b or k not in self.A:
                continue
            paired = [self.a2b[c] for c in callers if c in self.a2b]
            if len(paired) < 2:
                continue
            votes = collections.Counter()
            for b in paired:
                for y in set(self.B[b].calls):
                    if y in self.B and y not in self.b2a:
                        votes[y] += 1
            if not votes:
                continue
            (y, v), = votes.most_common(1)
            second = votes.most_common(2)[1][1] if len(votes) > 1 else 0
            if v >= max(2, 0.6 * len(paired)) and v > second:
                n += self.add(k, y, 'caller-vote')
        return n

    def learn_globals(self):
        votes = collections.defaultdict(collections.Counter)
        for a, b in self.a2b.items():
            la = list(dict.fromkeys(self.A[a].refs))
            lb = list(dict.fromkeys(self.B[b].refs))
            if len(la) == len(lb):
                pairs = list(zip(la, lb))
            else:
                pairs = self.gaps(la, lb, lambda x, y: self.gmap.get(x) == y)
            for x, y in pairs:
                votes[x][y] += 1
        n = 0
        taken = collections.Counter()
        for x, c in votes.items():
            y, k = c.most_common(1)[0]
            if k >= 2 and k >= 0.6 * sum(c.values()):
                taken[y] += 1
        for x, c in votes.items():
            y, k = c.most_common(1)[0]
            if k >= 2 and k >= 0.6 * sum(c.values()) and taken[y] == 1 and x not in self.gmap and y not in self.gb_set:
                self.gmap[x] = y
                n += 1
        self.refresh()
        return n

    def fuzzy(self, thresh, margin, ratio=None, how='similarity'):
        """Mutual best by weighted similarity among candidates sharing a rare or mapped token."""
        inv = collections.defaultdict(set)
        tb = {}
        for k, f in self.B.items():
            if k in self.b2a:
                continue
            tb[k] = self.tokens_b(f)
            for t in tb[k]:
                if (t[0] in ('k', 'p', 'g') and t[1] != '?') or (t[0] in ('c', 'f') and self.idf.get(t, 0) > 5.0):
                    inv[t].add(k)
        best_a = {}
        for k, f in self.A.items():
            if k in self.a2b:
                continue
            ta = self.tokens_a(f)
            cand = collections.Counter()
            for t in ta:
                for y in inv.get(t, ()):
                    cand[y] += 1
            scored = sorted(((self.sim(ta, tb[y], f, self.B[y]), y) for y, _ in cand.most_common(60)), reverse=True)
            if scored:
                s1 = scored[0][0]
                s2 = scored[1][0] if len(scored) > 1 else 0.0
                best_a[k] = (s1, s2, scored[0][1])
        best_b = {}
        for k, (s1, s2, y) in best_a.items():
            cur = best_b.get(y)
            if cur is None or s1 > cur[0]:
                best_b[y] = (s1, cur[0] if cur else 0.0, k)
            else:
                best_b[y] = (cur[0], max(cur[1], s1), cur[2])
        n = 0
        for y, (s1, s2b, k) in best_b.items():
            s1a, s2a, _ = best_a[k]
            s2 = max(s2a, s2b)
            big = self.A[k].n >= 40 and self.B[y].n >= 40
            if ratio is not None:
                ok = s1 >= thresh and s1 >= ratio * s2 and best_a[k][2] == y
            else:
                ok = (s1 >= thresh and s1 - s2 >= margin) or (big and s1 >= 0.25 and s1 >= 4 * s2)
            if ok:
                n += self.add(k, y, how)
        return n

    def gapfill(self):
        """Pair inside the gaps between paired neighbours: the PSP keeps a module's order.

        Along each family (order()), the paired functions in link order whose twins also rise
        (a longest increasing run) are anchors. Between two anchors, equal counts pair by
        position when each pair is not dissimilar; otherwise an order-keeping alignment
        within the window."""
        bkeys = sorted(self.B)
        n = 0
        by_space = collections.defaultdict(list)
        for k in self.fullA:
            by_space[self.order(k)[0]].append(k)
        for space, ks in by_space.items():
            ks.sort(key=self.order)
            seq = [(k, self.a2b.get(self.rep_a.get(k, k))) for k in ks]
            idx = [i for i, (k, b) in enumerate(seq) if b is not None]
            run = lis([seq[i][1] for i in idx])
            anchors = [idx[j] for j in run]
            for i0, i1 in zip(anchors, anchors[1:]):
                b0, b1 = seq[i0][1], seq[i1][1]
                if b1 - b0 > 0x20000:
                    continue
                pa = [seq[i][0] for i in range(i0 + 1, i1) if seq[i][1] is None]
                pa = [k for k in dict.fromkeys(self.rep_a.get(k, k) for k in pa) if k not in self.a2b and k in self.A]
                lo_i, hi_i = bisect.bisect_right(bkeys, b0), bisect.bisect_left(bkeys, b1)
                pb = [y for y in bkeys[lo_i:hi_i] if y not in self.b2a]
                if not pa or not pb or len(pb) > 80 or len(pa) > 120:
                    continue
                ta = {k: self.tokens_a(self.A[k]) for k in pa}
                tb = {y: self.tokens_b(self.B[y]) for y in pb}
                sc = [[self.sim(ta[k], tb[y], self.A[k], self.B[y]) for y in pb] for k in pa]
                # equal counts pair by position when no pair is dissimilar; in a gap of four
                # or fewer the anchors carry it and only the sizes must agree
                floor = 0.0 if len(pa) <= 4 else 0.2
                if len(pa) == len(pb) and all(sc[i][i] >= floor and 0.4 <= (self.B[pb[i]].n + 1) / (self.A[pa[i]].n + 1) <= 2.5
                                              for i in range(len(pa))):
                    for k, y in zip(pa, pb):
                        n += self.add(k, y, 'gap-order')
                    continue
                for i, j in align_dp(sc, 0.3):
                    n += self.add(pa[i], pb[j], 'gap-align')
        return n

    def run(self, log=print):
        log('seed: %d unique signatures' % self.seed())
        self.refresh()
        for rnd in range(400):
            g = self.learn_globals()
            c = self.propagate_calls()
            c += self.caller_vote()
            if c or g:
                log('round %d: %d data addresses learned, %d callee-aligned' % (rnd, g, c))
                continue
            f = self.gapfill()
            if f:
                log('round %d: %d in gaps between paired neighbours' % (rnd, f))
                continue
            f = self.fuzzy(0.55, 0.12)
            if not f:
                f = self.fuzzy(0.42, 0.15)
            if not f:   # last resort, tagged so a reader weighs it: mutual best, 1.3x the runner-up
                f = self.fuzzy(0.3, None, ratio=1.3, how='similarity-weak')
            log('round %d: %d by similarity' % (rnd, f))
            if not f:
                break
        self.dispute()

    def full(self):
        """Every PSX function (copies included) -> (PSP twin, how, number of PSP copies)."""
        ncopy = collections.Counter(self.rep_b.values())
        out = {}
        for k in self.fullA:
            r = self.rep_a.get(k, k)
            if r in self.a2b:
                b = self.a2b[r]
                out[k] = (b, self.how[r] + ('' if r == k else ' (copy)'), max(1, ncopy.get(b, 0)))
            elif r in self.how:
                out[k] = (None, self.how[r], 0)
        return out

    def dispute(self):
        """Drop pairs whose paired callees contradict them on both sides."""
        bad = []
        for a, b in self.a2b.items():
            ka = [self.a2b[c] for c in self.A[a].calls if c in self.a2b]
            kb = [c for c in self.B[b].calls if c in self.b2a]
            if len(ka) >= 3 and len(kb) >= 3 and not set(ka) & set(kb):
                bad.append(a)
        for a in bad:
            b = self.a2b.pop(a)
            self.b2a.pop(b)
            self.how[a] = 'disputed'
        return len(bad)


# ---------------------------------------------------------------- commands

def key_str(k):
    return '%s:%08x' % (k[0], k[1]) if isinstance(k, tuple) else '%08x' % k


def jp_to_eu(jp, B, eu, E):
    """PSP-JP function -> PSP-EU twin: unique relocation-masked words, then equal gaps by order."""
    hj, he = collections.defaultdict(list), collections.defaultdict(list)
    for k, f in B.items():
        hj[jp.masked_hash(f)].append(k)
    for k, f in E.items():
        he[eu.masked_hash(f)].append(k)
    j2e = {hj[h][0]: he[h][0] for h in hj if len(hj[h]) == 1 and len(he.get(h, ())) == 1}
    bj, be = sorted(B), sorted(E)
    xs = sorted(j2e)
    run = [xs[i] for i in lis([j2e[x] for x in xs])]
    j2e = {x: j2e[x] for x in run}
    exact = len(j2e)
    for x0, x1 in zip(run, run[1:]):
        ga = bj[bisect.bisect_right(bj, x0):bisect.bisect_left(bj, x1)]
        gb = be[bisect.bisect_right(be, j2e[x0]):bisect.bisect_left(be, j2e[x1])]
        if len(ga) == len(gb):
            for x, y in zip(ga, gb):
                if B[x].n == E[y].n:
                    j2e[x] = y
    return j2e, exact


def build(a, log=print):
    psx = Psx(a.sibling)
    A = psx.functions()
    log('PSX: %d functions in %d spaces (boot EXE and %d overlay captures)' % (len(A), len(psx.spaces), len(psx.spaces) - 1))
    jp = Psp(a.psp_jp)
    B = jp.functions()
    log('PSP-JP: %d functions (%d .data code pointers)' % (len(B), len(jp.data_ptrs)))
    m = Matcher(A, B, psx.order)
    m.run(log=log)
    full = m.full()
    j2e, eu, E = {}, None, None
    if a.psp_eu:
        eu = Psp(a.psp_eu)
        E = eu.functions()
        j2e, exact = jp_to_eu(jp, B, eu, E)
        log('PSP-JP to PSP-EU: %d of %d functions (%d by unique masked words, the rest by order)'
            % (len(j2e), len(B), exact))
    return psx, A, jp, B, m, full, j2e, eu, E


def twin_of(psx, full, k):
    """(PSP-JP twin, how, the function it was paired through) for a PSX key or a named label."""
    via = psx.inside.get(k, k)
    t = full.get(via)
    if t is None:
        return None, 'unpaired' if via in psx.functions_keys else 'no function', via
    return t[0], t[1] if t[0] is not None else t[1] or 'unpaired', via


def coverage(psx, full):
    named = list(psx.names)
    game = [k for k in named if k not in psx.lib]
    lib = [k for k in named if k in psx.lib]
    pg = [k for k in game if twin_of(psx, full, k)[0] is not None]
    pl = [k for k in lib if twin_of(psx, full, k)[0] is not None]
    allp = sum(1 for v in full.values() if v[0] is not None)
    return dict(named=len(named), game=len(game), game_paired=len(pg), lib=len(lib), lib_paired=len(pl),
                labels=len(psx.inside), all_psx=len(psx.functions_keys), all_paired=allp)


def cmd_pair(a):
    psx, A, jp, B, m, full, j2e, eu, E = build(a)
    cov = coverage(psx, full)
    print('named PSX functions: %(named)d; game %(game)d, paired %(game_paired)d; Psy-Q library %(lib)d, '
          'paired %(lib_paired)d; %(labels)d names are labels inside a function' % cov)
    print('all PSX functions: %(all_paired)d of %(all_psx)d paired' % cov)
    out = dict(coverage=cov, gmap=len(m.gmap), functions=[])
    if a.check_eu and E is not None:
        m2 = Matcher(A, E, psx.order)
        m2.run(log=lambda s: None)
        f2 = m2.full()
        both = [k for k, v in full.items() if v[0] is not None and f2.get(k, (None,))[0] is not None and v[0] in j2e]
        agree = sum(1 for k in both if j2e[full[k][0]] == f2[k][0])
        print('independent PSX->PSP-EU pairing: of %d functions paired both ways, %d agree with JP->EU (%.2f%%)'
              % (len(both), agree, 100.0 * agree / max(1, len(both))))
        out['eu_check'] = dict(both=len(both), agree=agree)
    keys = set(k for k, v in full.items() if v[0] is not None) | set(psx.names)
    for k in sorted(keys, key=lambda k: (k[0] != 'boot', k[0], k[1])):
        b, how, via = twin_of(psx, full, k)
        f = A.get(via)
        out['functions'].append(dict(
            psx=key_str(k), name=psx.names.get(k, ('',))[0], lib=k in psx.lib,
            file=psx.file_of(k[0]), inside=None if via == k else key_str(via),
            psp_jp=None if b is None else '%08x' % b, psp_eu=None if b is None or b not in j2e else '%08x' % j2e[b],
            how=how, n_psx=f.n if f else 0, n_psp=B[b].n if b is not None else 0))
    json.dump(out, open(a.out, 'w'), indent=0)
    print('wrote', a.out)


# ---------------------------------------------------------------- the diff

# The data whose consumers the round asked about first (docs/psp-only-round.md 3,
# CD). PSX-JP addresses: tables.toml (exp_table), psp_stallion.py JP_TABLES
# (consumables, 14-byte records), region_diff sections of BPLCHAR/RYUD00 #3 and
# BPLD012 #3 / #7 (Ryu's form data; the arenas' load area, up to RYUD's).
PRIORITY = [
    ('exp_table', 0x801CC068, 0x801CC068 + 5544),
    ('consumables', 0x801C995C, 0x801C995C + 92 * 14),
    ('RYUD form data', 0x8007280C, 0x8007280C + 0x1B000),
    ('arena load area', 0x80033800, 0x8007280C),
]
# Constants the PSP's widescreen work introduced (docs/psp-widescreen.md 3a): the
# widened culls' bounds as compiled (an slti's bound is the kept bound + 1), the
# side message box placements, and the full-frame rectangle's -32 / 384 / 352.
WIDESCREEN = {-96, 417, -231, 552, -81, 402, -32, 384, 352, 113, 258}


def fmt_c(c, limit=8):
    items = sorted(c.items(), key=lambda kv: str(kv[0]))
    out = []
    for v, n in items[:limit]:
        if isinstance(v, tuple):
            t = 'w%d%+d' % v
        elif -1000 < v < 1000:
            t = '%d' % v
        else:
            t = '%#x' % (v & 0xFFFFFFFF)
        out.append(t + ('x%d' % n if n > 1 else ''))
    return ' '.join(out) + (' ...' if len(items) > limit else '')


def psx_ram_offset(v):
    """A PSP constant that is a PSX RAM address less 0x80000000: the PSP keeps the EMI
    sections at their PSX offsets in one buffer (0x33800 for 0x80033800, the arenas'
    load area), so these are addresses, not values."""
    return isinstance(v, int) and 0x10000 <= v < 0x200000 and v & 0xFF == 0


def soft(v):
    """A constant a compiler is free to make or unmake: a bit mask (2^k - 1) or a power
    of two (Allegrex's ext / ins / seb / min / max take them as fields or not at all)."""
    if not isinstance(v, int) or v <= 0:
        return False
    return v & (v + 1) == 0 or v & (v - 1) == 0


def diff_pair(m, psx, jp, a, b, platform_b):
    """One pair's differences: kinds, a short detail line, and a score.

    Compared as sets after normalising what the two compilers do differently
    (measured on the pairs psp-stallion.md reads as identical, all 'same' here):
      - constants as a set (the PSP loads a constant once where the PSX reloads it),
        the trivial ones apart (0xFF, 1, -1...), masks and powers of two not scored,
        c against c + 1, c - 1 or -c cancelled (a bound moved by one, a subtraction), and
        against its own low 16 or (for a negative) 8 bits (0xBF and -65 clear one bit),
        and against a power-of-two multiple (one reciprocal serves x / 25 and x / 100),
        multipliers dropped (the PSX shifts and adds), a PSP constant that is a PSX
        RAM offset dropped, and the constant arguments of a platform call on either side
        dropped on both;
      - paired callees as a set; a difference only in small callees (40
        instructions or fewer, which one compiler inlines) is listed, not scored;
      - globals, scored only when each side has a mapped one the other lacks;
      - field offsets are listed, never scored: the PSP's base registers shift them.
    Calls into the PSX's Psy-Q library and into the PSP's platform layer are set
    aside, and so is a PSX function using the GTE once it differs."""
    fa, fb = m.A[a], m.B[b]
    kinds, det = [], []
    plat_a = lambda c: c in psx.lib
    plat_b = lambda c: jp.is_stub(c) or c in platform_b or m.rep_b.get(c, c) in platform_b

    pargs = set(v for t, args in fa.callargs if plat_a(t) for v in args) | \
        set(v for t, args in fb.callargs if plat_b(t) for v in args)

    arefs = set(fa.refs)

    def norm(f, psp):
        return set(v for v, n in f.consts.items() if v not in TRIVIAL and v not in pargs
                   and not (psp and (psx_ram_offset(v) or (0x10000 <= v < 0x200000 and (v | 0x80000000) in arefs))))
    ca, cb = norm(fa, False), norm(fb, True)
    minus, plus = ca - cb, cb - ca
    for v in sorted(minus, key=str):
        if not isinstance(v, int):
            continue
        for w in sorted(plus, key=str):
            if not isinstance(w, int):
                continue
            ratio = max(abs(v), abs(w)) // max(1, min(abs(v), abs(w)))
            if (abs(v - w) == 1 or v == -w or (v - w) & 0xFFFF == 0 or
                                       (v > 0 and w > 0 and max(v, w) % min(v, w) == 0 and ratio & (ratio - 1) == 0) or
                                       ((v < 0 or w < 0) and min(abs(v), abs(w)) <= 0xFF and (v - w) & 0xFF == 0)):
                minus.discard(v); plus.discard(w)
                break
    score = 0
    if minus or plus:
        det.append('const -[%s] +[%s]' % (fmt_c(collections.Counter(minus)), fmt_c(collections.Counter(plus))))
        hard = [v for v in minus | plus if not soft(v) and v not in WIDESCREEN]
        if plus & WIDESCREEN:
            kinds.append('widescreen')
        if hard:
            kinds.append('consts')
            score += len(hard)
    fm, fp = set(fa.fields) - set(fb.fields), set(fb.fields) - set(fa.fields)
    if fm or fp:
        det.append('field -[%s] +[%s]' % (fmt_c(collections.Counter(fm)), fmt_c(collections.Counter(fp))))
    la = [c for c in fa.calls if plat_a(c)]
    lb = [c for c in fb.calls if plat_b(c)]
    ka = set(m.a2b[c] for c in fa.calls if c in m.a2b and not plat_a(c))
    kb = set(c for c in fb.calls if c in m.b2a and not plat_b(c))
    ua = len(set(c for c in fa.calls if c not in m.a2b and not plat_a(c)))
    ub = len(set(c for c in fb.calls if c not in m.b2a and not plat_b(c)))
    if ka != kb or ua != ub:
        def nm(y):
            return psx.names.get(m.b2a.get(y), ('',))[0] or '%x' % y
        det.append('calls -[%s] +[%s]%s' % (' '.join(sorted(nm(c) for c in ka - kb)),
                                            ' '.join(sorted(nm(c) for c in kb - ka)),
                                            ' unpaired %d/%d' % (ua, ub) if ua or ub else ''))
        big = [c for c in ka ^ kb if m.B[c].n > 40]
        if big or abs(ua - ub) > 1:
            kinds.append('calls')
            score += 2 * len(big) + max(0, abs(ua - ub) - 1)
        else:
            kinds.append('inline')
    if bool(la) != bool(lb):
        kinds.append('platform')
    if fa.gte:
        kinds.append('gte')
    ra = set(m.map_ref_a(r) for r in fa.refs) - {None}
    rb = set(r for r in fb.refs if r in m.gb_set)
    if ra - rb and rb - ra:
        kinds.append('globals')
        det.append('globals %d only PSX, %d only PSP' % (len(ra - rb), len(rb - ra)))
        score += min(len(ra - rb), len(rb - ra))
    return kinds, '; '.join(det), score


def verdict(kinds, score):
    """same / minor (score 1) / differs (2 or more) / widescreen / platform."""
    if 'widescreen' in kinds and (score < 2 or 'gte' in kinds):
        return 'widescreen'
    if score == 0:
        return 'platform' if 'platform' in kinds else 'same'
    if 'gte' in kinds or ('platform' in kinds and kinds.count('calls') and 'consts' not in kinds):
        return 'platform'
    return 'differs' if score >= 2 else 'minor'


BATTLE_FAMILIES = ('BATTLE', 'BMAGIC', 'BOSS')
BATTLE_PREFIXES = ('Battle', 'BattleResult', 'Magic', 'Effect', 'Enemy', 'EnemyAI', 'Boss', 'Attack', 'Escape',
                   'Formation', 'Examine', 'HUD', 'BattleMenu', 'BattleBanner', 'Skill', 'AutoBattle', 'Defend')


def half_of(psx, k, callers):
    """'battle' (battle and effects) or 'field' (field, menus, items, system): a battle
    family's overlay, a battle name, or a boot function only battle code calls."""
    if k[0] != 'boot':
        return 'battle' if psx.order(k)[0] in BATTLE_FAMILIES else 'field'
    if psx.names.get(k, ('',))[0].split('_')[0] in BATTLE_PREFIXES:
        return 'battle'
    fams = set(psx.order(c)[0] for c in callers.get(k, ()))
    return 'battle' if fams and fams <= set(BATTLE_FAMILIES) else 'field'


def platform_layer(m, psx, jp, B):
    """The PSP's platform layer: functions that call the system's stubs, the twins of
    the PSX's library functions, and - to a fixed point - unpaired PSP functions that
    call into either."""
    plat = set(m.a2b[k] for k in psx.lib if k in m.a2b) | set(k for k, f in B.items() if f.stubs)
    while True:
        add = set(k for k, f in B.items() if k not in plat and m.rep_b.get(k, k) not in m.b2a
                  and any(c in plat for c in f.calls))
        if not add:
            return plat
        plat |= add


def cmd_diff(a):
    psx, A, jp, B, m, full, j2e, eu, E = build(a)
    platform_b = platform_layer(m, psx, jp, B)
    callers = collections.defaultdict(set)
    for k, f in A.items():
        for c in f.calls:
            callers[c].add(k)
    ncopies = collections.Counter(m.rep_a.values())
    label_of = {}
    for x, y in psx.inside.items():
        label_of.setdefault(y, psx.names[x][0] + ' (label)')
    rows, seen = [], set()
    for k in sorted(A, key=psx.order):
        r = m.rep_a.get(k, k)
        if r not in m.a2b or r in seen:
            continue
        seen.add(r)
        b = m.a2b[r]
        kinds, det, score = diff_pair(m, psx, jp, r, b, platform_b)
        prio = [label for label, lo, hi in PRIORITY if any(lo <= x < hi for x in A[r].refs)]
        if 'consumables' in prio and 87 in A[r].consts:
            prio.append('the constant 87')
        name = psx.names.get(r, ('',))[0] or label_of.get(r, '')
        rows.append(dict(psx=key_str(r), file=psx.file_of(r[0]), name=name, lib=r in psx.lib,
                         copies=max(0, ncopies.get(r, 1) - 1), psp_jp='%08x' % b,
                         psp_eu='%08x' % j2e[b] if b in j2e else None, how=m.how[r], n_psx=A[r].n, n_psp=B[b].n,
                         kinds=kinds, score=score, verdict=verdict(kinds, score), detail=det, priority=prio,
                         half=half_of(psx, r, callers)))
    tally = collections.Counter((x['half'], x['verdict']) for x in rows)
    print('pairs (each copy class once): %d' % len(rows))
    for h in ('battle', 'field'):
        print('  %-6s %s' % (h, ', '.join('%s %d' % (v, tally[(h, v)]) for v in ('same', 'minor', 'differs', 'widescreen', 'platform'))))
    named = [x for x in rows if x['name'] and not x['lib']]
    print('named game pairs: %d; differs %d' % (len(named), sum(1 for x in named if x['verdict'] == 'differs')))
    for label, _, _ in PRIORITY + [('the constant 87', 0, 0)]:
        hits = [x for x in rows if label in x['priority']]
        print('priority %-18s %3d paired readers; differs %d' % (label, len(hits), sum(1 for x in hits if x['verdict'] == 'differs')))
    json.dump(dict(rows=rows), open(a.out, 'w'), indent=0)
    print('wrote', a.out)
    if a.md:
        write_md(rows, a.md)


def md_row(x):
    size = (x['n_psp'] + 1) / (x['n_psx'] + 1)
    flag = ' (size %.1fx)' % size if not 0.6 <= size <= 1.7 else ''
    det = x['detail'].replace('|', '/')
    return '| `%s` %s%s | %s | `%s` | `%s` | %s | %d/%d%s | %d | %s |' % (
        x['psx'].split(':')[1], x['file'].replace('BIN/', ''), ' +%d copies' % x['copies'] if x['copies'] else '',
        x['name'] or '', x['psp_jp'], x['psp_eu'] or '-', x['how'], x['n_psx'], x['n_psp'], flag, x['score'],
        det if len(det) <= 160 else det[:157] + '...')


HEAD = ['| PSX (file) | name | PSP-JP | PSP-EU | paired by | instr. PSX/PSP | score | differs in |',
        '|---|---|---|---|---|---|---|---|']


def write_md(rows, path):
    """The doc's tables: the priority readers, then the differing pairs by half,
    named ones first, then by score."""
    out = ['#### The priority data\'s paired readers (every verdict)\n']
    out.append('| data | PSX (file) | name | PSP-JP | PSP-EU | verdict | score | differs in |')
    out.append('|---|---|---|---|---|---|---|---|')
    for label, _, _ in PRIORITY + [('the constant 87', 0, 0)]:
        for x in rows:
            if label in x['priority']:
                out.append('| %s | `%s` %s | %s | `%s` | `%s` | %s | %d | %s |' % (
                    label, x['psx'].split(':')[1], x['file'].replace('BIN/', ''), x['name'], x['psp_jp'],
                    x['psp_eu'] or '-', x['verdict'], x['score'], x['detail'].replace('|', '/')[:160]))
    out.append('')
    for h, title in (('battle', 'Battle and effects'), ('field', 'Field, menus, items and system')):
        d = [x for x in rows if x['half'] == h and x['verdict'] == 'differs']
        d.sort(key=lambda x: (not x['name'], -x['score'], x['file'], x['psx']))
        out.append('#### %s: %d differing pairs\n' % (title, len(d)))
        out += HEAD
        out += [md_row(x) for x in d]
        out.append('')
    open(path, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print('wrote', path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('pair')
    p.add_argument('--psp-jp', required=True)
    p.add_argument('--psp-eu')
    p.add_argument('--check-eu', action='store_true', help='also pair PSX->EU independently and measure agreement')
    p.add_argument('--out', required=True)
    p.add_argument('--sibling', default=find_sibling())
    p = sub.add_parser('diff')
    p.add_argument('--psp-jp', required=True)
    p.add_argument('--psp-eu')
    p.add_argument('--out', required=True)
    p.add_argument('--md', help='write the doc tables here')
    p.add_argument('--sibling', default=find_sibling())
    a = ap.parse_args()
    a.check_eu = getattr(a, 'check_eu', False)
    if not a.sibling:
        sys.exit('no sibling checkout found; pass --sibling')
    if not a.sibling.endswith(('/', '\\')):
        a.sibling += os.sep
    if a.cmd == 'pair':
        cmd_pair(a)
    elif a.cmd == 'diff':
        cmd_diff(a)


if __name__ == '__main__':
    main()
