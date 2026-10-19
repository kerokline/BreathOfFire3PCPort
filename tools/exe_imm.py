#!/usr/bin/env python
"""The owning-function place kind: a PSX function's immediates read back as .data bytes.

docs/exe-immediate-places.md; docs/exe-import-engine.md 3.3. The PC function
that reads a row of BOF3.exe's .data no disc places has a PSX twin
(symbols.toml `psx`, or the pair a placed code-pointer word gives,
exe_tables.code_pairs), and the twin's instructions locate or rebuild the row
on the player's disc. Two shapes of row, both read from the disc's code:

  imm      the values themselves are immediates: the twin fills a local array
           with `li` / `sb` (the PlayStation's compiler folded the table into
           code; the PC's left it in .data). The row's elements are the stores
           and the instructions that make their values.
  immref   the twin's immediates form the address of the row's own bytes in its
           overlay: a `lui` and the `addiu` / `ori` / load / store that adds its
           low half (addiu's and a load's sign kept). The row's bytes are the
           file's at that address plus the row's displacement - the case the
           measurement found (docs/exe-immediate-places.md 2): the bytes are
           data on the disc, in reach of no other key (the end of an overlay
           section, or too short for a 16-byte window).

This module is the decoder and the search, and holds no game data: a MIPS
R3000 word decoder for the few instruction classes the kind needs, a
function's extent, a straight-line constant pass, the match of a run of the
PC's bytes to the stores of constants a twin makes, a function's body found
again in another build's copy of its file, and a call / constant fingerprint
for checking a (PC function, PSX function) pair. tools/exe_tables.py writes
the rows (recipes/exe-imm.tsv), applies them and checks them.

An `imm` row's `insns` (offsets from the function's start, hex, no 0x),
comma-separated elements:

    S=V      the store at S (sb / sh / sw: its width and displacement) of the
             value the instruction at V makes (li: addiu or ori from $zero; or a
             lone lui)
    S=H+L    the same with the value of a pair: lui at H, then addiu or ori at L
             on the lui's register (addiu's immediate sign-extended)
    S=z      a store of $zero

and its `at` is the displacement (off the stores' one base register) of the
row's first PC byte, so element S lands at pc + (disp(S) - at). An `immref`
row's `insns` is H+L (the lui, and the instruction on the lui's register that
adds the low half) and its `at` the signed bytes from that address to the
row's first byte. Decoding checks every instruction's opcode and registers;
any other word is refused, loudly.

    python tools/exe_imm.py selftest        # the decoder on synthetic code (no game data)
    python tools/exe_imm.py pair --game <dir> --disc <psx-jp cue> [--out TSV] [--held-out | --write]
                                            # functions no placed word pairs, paired from the
                                            # known ones (docs/install-gap-pairs.md); --write
                                            # feeds them to symbols.toml at the hypothesis tier
"""
import collections
import sys

# ------------------------------------------------------------------ the decoder

OP_SPECIAL, OP_REGIMM, OP_J, OP_JAL = 0x00, 0x01, 0x02, 0x03
OP_BEQ, OP_BNE, OP_BLEZ, OP_BGTZ = 0x04, 0x05, 0x06, 0x07
OP_ADDI, OP_ADDIU, OP_SLTI, OP_SLTIU, OP_ANDI, OP_ORI, OP_XORI, OP_LUI = range(0x08, 0x10)
OP_COP0, OP_COP2 = 0x10, 0x12
LOADS = {0x20: 1, 0x21: 2, 0x22: 4, 0x23: 4, 0x24: 1, 0x25: 2, 0x26: 4}        # lb lh lwl lw lbu lhu lwr
STORES = {0x28: 1, 0x29: 2, 0x2A: 4, 0x2B: 4, 0x2E: 4}                        # sb sh swl sw swr
WHOLE_STORES = {0x28: 1, 0x29: 2, 0x2B: 4}                                    # stores of a whole register's low bytes
F_JR, F_JALR = 0x08, 0x09
NO_RD = {0x08, 0x0C, 0x0D, 0x11, 0x13, 0x18, 0x19, 0x1A, 0x1B}               # jr syscall break mthi mtlo mult(u) div(u)
RA, SP, ZERO = 31, 29, 0
CALLER_SAVED = set(range(1, 16)) | {24, 25, 31}


class Insn:
    """One MIPS word's fields."""
    __slots__ = ("w", "op", "rs", "rt", "rd", "funct", "imm", "simm", "target")

    def __init__(self, w):
        self.w = w
        self.op = w >> 26
        self.rs = (w >> 21) & 31
        self.rt = (w >> 16) & 31
        self.rd = (w >> 11) & 31
        self.funct = w & 63
        self.imm = w & 0xFFFF
        self.simm = self.imm - 0x10000 if self.imm & 0x8000 else self.imm
        self.target = w & 0x3FFFFFF

    def branch_target(self, pc):
        """The target of a conditional branch at pc, else None."""
        if self.op in (OP_BEQ, OP_BNE, OP_BLEZ, OP_BGTZ) or (self.op == OP_REGIMM and self.rt in (0, 1, 16, 17)):
            return (pc + 4 + (self.simm << 2)) & 0xFFFFFFFF
        return None

    def jump_target(self, pc):
        """The target of j / jal at pc, else None."""
        if self.op in (OP_J, OP_JAL):
            return ((pc + 4) & 0xF0000000) | (self.target << 2)
        return None

    def writes(self):
        """The general register this instruction writes, or None."""
        if self.op == OP_SPECIAL:
            if self.funct == F_JALR:
                return self.rd
            return None if self.funct in NO_RD else self.rd
        if OP_ADDI <= self.op <= OP_LUI or self.op in LOADS:
            return self.rt
        if self.op == OP_JAL:
            return RA
        if self.op == OP_REGIMM and self.rt in (16, 17):
            return RA
        if self.op in (OP_COP0, OP_COP2) and self.rs in (0, 2):          # mfc / cfc
            return self.rt
        return None


def value_of(insns):
    """The value a value element's instructions make: [li] or [lui] or [lui, lo].
    Returns (value, register) or raises ValueError naming what is wrong."""
    if len(insns) == 1:
        i = insns[0]
        if i.op in (OP_ADDIU, OP_ADDI) and i.rs == ZERO:
            return i.simm & 0xFFFFFFFF, i.rt
        if i.op == OP_ORI and i.rs == ZERO:
            return i.imm, i.rt
        if i.op == OP_LUI:
            return i.imm << 16, i.rt
        raise ValueError("not li / ori from $zero / lui (word op 0x%02X)" % i.op)
    if len(insns) == 2:
        h, l = insns
        if h.op != OP_LUI:
            raise ValueError("a pair's first instruction is not lui (op 0x%02X)" % h.op)
        if l.rs != h.rt:
            raise ValueError("a pair's low instruction does not read the lui's register")
        if l.op in (OP_ADDIU, OP_ADDI):
            return ((h.imm << 16) + l.simm) & 0xFFFFFFFF, l.rt
        if l.op == OP_ORI:
            return (h.imm << 16) | l.imm, l.rt
        raise ValueError("a pair's low instruction is not addiu / ori (op 0x%02X)" % l.op)
    raise ValueError("%d value instructions" % len(insns))


# ------------------------------------------------------------------ a row's elements

def parse_elements(text):
    """'S=V,S=H+L,S=z' -> [(store off, [value offs])] ([] for z). Raises ValueError."""
    out = []
    for tok in text.split(","):
        s, eq, v = tok.partition("=")
        if not eq or not s or not v:
            raise ValueError("element %r is not S=V, S=H+L or S=z" % tok)
        vs = [] if v == "z" else [int(x, 16) for x in v.split("+")]
        if len(vs) > 2 or any(x < 0 for x in vs) or int(s, 16) < 0:
            raise ValueError("element %r" % tok)
        out.append((int(s, 16), vs))
    if not out:
        raise ValueError("no elements")
    return out


def format_elements(elems):
    return ",".join("%X=%s" % (s, "z" if not vs else "+".join("%X" % v for v in vs)) for s, vs in elems)


def decode_row(read, func, at, elems):
    """The bytes a row's elements rebuild: [(offset from the row's pc, bytes)],
    one piece per element. `read(addr)` gives the 32-bit word at a disc address
    (or None off the file). Raises ValueError, naming the element, when a word is
    not what the row says: a store that is not sb / sh / sw, a value instruction
    of another kind, a value made in another register than the one stored, a
    value that does not fit the store's width, two elements over one byte, or
    stores off different base registers."""
    out, seen, base = [], set(), None
    for s, vs in elems:
        w = read(func + s)
        if w is None:
            raise ValueError("+0x%X: off the file" % s)
        st = Insn(w)
        if st.op not in WHOLE_STORES:
            raise ValueError("+0x%X: not sb / sh / sw (op 0x%02X)" % (s, st.op))
        if base is not None and st.rs != base:
            raise ValueError("+0x%X: stores off $%d, the row's other stores off $%d" % (s, st.rs, base))
        base = st.rs
        width = WHOLE_STORES[st.op]
        if vs:
            words = [read(func + v) for v in vs]
            if None in words:
                raise ValueError("+0x%X: a value instruction off the file" % s)
            try:
                value, reg = value_of([Insn(x) for x in words])
            except ValueError as e:
                raise ValueError("+0x%X: %s" % (s, e))
            if reg != st.rt or reg == ZERO:
                raise ValueError("+0x%X: the value is made in $%d, the store stores $%d" % (s, reg, st.rt))
        else:
            if st.rt != ZERO:
                raise ValueError("+0x%X: an element of $zero stores $%d" % (s, st.rt))
            value = 0
        if width < 4:
            v = value if value < 0x80000000 else value - 0x100000000
            if not -(1 << (8 * width - 1)) <= v < (1 << (8 * width)):
                raise ValueError("+0x%X: value does not fit %d byte(s)" % (s, width))
        off = st.simm - at
        if off < 0:
            raise ValueError("+0x%X: lands before the row" % s)
        span = set(range(off, off + width))
        if span & seen:
            raise ValueError("+0x%X: overlaps another element" % s)
        seen |= span
        out.append((off, (value & ((1 << (8 * width)) - 1)).to_bytes(width, "little")))
    return sorted(out)


MEMORY = set(LOADS) | set(STORES) | {0x32, 0x3A}                              # + lwc2 / swc2 (the GTE's)


def parse_ref(text):
    """'H+L' -> (H, L). Raises ValueError."""
    h, plus, l = text.partition("+")
    if not plus or not h or not l or "+" in l:
        raise ValueError("an immref's insns %r is not H+L" % text)
    h, l = int(h, 16), int(l, 16)
    if h < 0 or l < 0 or h == l:
        raise ValueError("an immref's insns %r" % text)
    return h, l


def decode_ref(read, func, hi, lo):
    """The address an immref row's pair forms: lui at func + hi, and at func + lo
    an addiu / ori on the lui's register or a load / store based on it. Raises
    ValueError when either word is not that."""
    wh, wl = read(func + hi), read(func + lo)
    if wh is None or wl is None:
        raise ValueError("+0x%X / +0x%X: off the file" % (hi, lo))
    h, l = Insn(wh), Insn(wl)
    if h.op != OP_LUI:
        raise ValueError("+0x%X: not lui (op 0x%02X)" % (hi, h.op))
    if l.rs != h.rt or h.rt == ZERO:
        raise ValueError("+0x%X: does not read the lui's register $%d" % (lo, h.rt))
    if l.op in (OP_ADDIU, OP_ADDI) or l.op in MEMORY:
        return ((h.imm << 16) + l.simm) & 0xFFFFFFFF
    if l.op == OP_ORI:
        return (h.imm << 16) | l.imm
    raise ValueError("+0x%X: not addiu / ori / a load or store (op 0x%02X)" % (lo, l.op))


def blob_reader(blob, base):
    """read(addr) over one file section's bytes: its 32-bit word, None off it."""
    def read(a):
        o = a - base
        if 0 <= o <= len(blob) - 4 and not o & 3:
            return int.from_bytes(blob[o:o + 4], "little")
        return None
    return read


# ------------------------------------------------------------------ the search side

def extent(read, start, limit=0x4000):
    """The end (exclusive) of the function at `start`: the first `jr $ra` (and its
    delay slot) past every forward branch target seen, a `j` within the next 4 KB
    counted as a branch. None when the code runs off the file or past `limit`."""
    far, pc = start, start
    while pc < start + limit:
        w = read(pc)
        if w is None:
            return None
        i = Insn(w)
        t = i.branch_target(pc)
        if t is None and i.op == OP_J:
            t = i.jump_target(pc)
            if not start <= t < pc + 0x1000:
                t = None
        if t is not None and t > far:
            far = t
        if i.op == OP_SPECIAL and i.funct == F_JR and i.rs == RA and pc + 8 > far:
            return pc + 8
        pc += 4
    return None


def scan(read, start, end):
    """A straight-line pass over [start, end): every constant a register is given
    (li, ori, lui, and lui's pairs), and every store of a constant or $zero. The
    state is dropped at each branch target and a call clears the caller-saved
    registers after its delay slot. Returns (stores, refs): stores [(store off,
    base register, displacement, width, value, [value offs])] - offsets from
    `start`, [] for $zero; refs [(address, [offs])] - the lui-based addresses a
    pair or a load / store forms (where the function's own data is)."""
    targets = set()
    for pc in range(start, end, 4):
        w = read(pc)
        if w is None:
            break
        i = Insn(w)
        t = i.branch_target(pc)
        if t is None and i.op == OP_J:
            t = i.jump_target(pc)
        if t is not None and start <= t < end:
            targets.add(t)
    reg = {}                # register -> (kind, value, [offs]): "c" a constant, "u" lui alone, "h" lui + an index
    stores, refs = [], []
    clear_after = None
    for pc in range(start, end, 4):
        w = read(pc)
        if w is None:
            break
        if pc in targets:
            reg.clear()
        i = Insn(w)
        off = pc - start
        wr = i.writes()
        new = None
        if i.op == OP_LUI:
            new = ("u", i.imm << 16, [off])
        elif i.op in (OP_ADDIU, OP_ADDI, OP_ORI):
            src = reg.get(i.rs)
            if i.rs == ZERO:
                new = ("c", (i.simm & 0xFFFFFFFF) if i.op != OP_ORI else i.imm, [off])
            elif src and src[0] == "u":
                v = ((src[1] + i.simm) & 0xFFFFFFFF) if i.op != OP_ORI else (src[1] | i.imm)
                new = ("c", v, src[2] + [off])
                refs.append((v, src[2] + [off]))
        elif i.op == OP_SPECIAL and i.funct in (0x21, 0x25) and wr:       # addu / or: a move or lui + index
            a, b = reg.get(i.rs), reg.get(i.rt)
            if i.rt == ZERO and a:
                new = a
            elif i.rs == ZERO and b:
                new = b
            elif a and a[0] == "u" and i.rt != ZERO:
                new = ("h", a[1], a[2])
            elif b and b[0] == "u" and i.rs != ZERO:
                new = ("h", b[1], b[2])
        if i.op in MEMORY:
            base = reg.get(i.rs)
            if base and base[0] in ("u", "h"):
                refs.append(((base[1] + i.simm) & 0xFFFFFFFF, base[2] + [off]))
        if i.op in WHOLE_STORES:
            if i.rt == ZERO:
                stores.append((off, i.rs, i.simm, WHOLE_STORES[i.op], 0, []))
            else:
                v = reg.get(i.rt)
                if v and v[0] in ("c", "u"):
                    stores.append((off, i.rs, i.simm, WHOLE_STORES[i.op], v[1], list(v[2])))
        if wr is not None and wr != ZERO:
            if new is None:
                reg.pop(wr, None)
            else:
                reg[wr] = new
        if clear_after is not None and clear_after == pc:
            for r in CALLER_SAVED:
                reg.pop(r, None)
            clear_after = None
        if i.op == OP_JAL or (i.op == OP_SPECIAL and i.funct == F_JALR):
            clear_after = pc + 4
    return stores, refs


def store_bytes(stores):
    """{(base register, displacement of a byte): (byte, store off, value offs, store's
    displacement, width)} from scan's stores, the bytes of a displacement two stores
    write left out (which one the code keeps depends on its path)."""
    out, twice = {}, set()
    for s, base, disp, width, value, vs in stores:
        b = (value & ((1 << (8 * width)) - 1)).to_bytes(width, "little")
        for k in range(width):
            key = (base, disp + k)
            if key in out:
                twice.add(key)
            out[key] = (b[k], s, vs, disp, width)
    for key in twice:
        del out[key]
    return out


def match_run(want, nonzero, bytes_):
    """Every (base register, at) that rebuilds `want` (the PC's bytes of a run, a
    bytes object) from the stores: each byte `nonzero` names (offsets into want)
    covered by a store's byte of the same value, and each store whose bytes are
    used lying wholly inside the run with every byte equal. Returns
    [(base, at, [(store off, [value offs])])], the elements in PC order."""
    if not nonzero:
        return []
    x0 = nonzero[0]
    out = []
    for (base, p), (b, *_rest) in bytes_.items():
        if b != want[x0]:
            continue
        at = p - x0
        used, ok = {}, True
        for x in nonzero:
            e = bytes_.get((base, at + x))
            if e is None or e[0] != want[x]:
                ok = False
                break
            used[e[1]] = e
        if not ok:
            continue
        for s, (b2, s2, vs, disp, width) in list(used.items()):
            for k in range(width):
                x = disp - at + k
                e = bytes_.get((base, disp + k))
                if not 0 <= x < len(want) or e is None or e[1] != s or e[0] != want[x]:
                    ok = False
        if ok:
            # the stores of zero bytes between the first and last non-zero byte, whole and equal
            for x in range(nonzero[0], nonzero[-1] + 1):
                e = bytes_.get((base, at + x))
                if e is None or e[1] in used or e[0] != want[x]:
                    continue
                if all(0 <= e[3] - at + k <= nonzero[-1] and bytes_.get((base, e[3] + k), (None, None))[1] == e[1]
                       and bytes_[(base, e[3] + k)][0] == want[e[3] - at + k] for k in range(e[4])):
                    used[e[1]] = e
            elems = sorted(((s, vs) for s, (_, _, vs, disp, _) in used.items()), key=lambda e: used[e[0]][3])
            out.append((base, at, elems))
    return out


# ------------------------------------------------------------------ one function in another build's file

def body_signature(read, start, end):
    """[(word, mask)] for [start, end): the bits a build's own layout leaves alone -
    a j / jal's target, a lui's immediate and the immediate of an instruction
    based on a lui's register (an address's low half) masked out, every other
    bit kept (branch offsets are relative)."""
    out, hi = [], set()
    for pc in range(start, end, 4):
        w = read(pc)
        if w is None:
            return None
        i = Insn(w)
        if i.op in (OP_J, OP_JAL):
            mask = 0xFC000000
        elif i.op == OP_LUI:
            mask = 0xFFFF0000
        elif OP_ADDI <= i.op <= OP_XORI or i.op in MEMORY:
            mask = 0xFFFF0000 if i.rs in hi else 0xFFFFFFFF
        else:
            mask = 0xFFFFFFFF
        out.append((w & mask, mask))
        wr = i.writes()
        if i.op == OP_LUI:
            hi.add(i.rt)
        elif i.op == OP_SPECIAL and i.funct == 0x21 and (i.rs in hi or i.rt in hi) and wr:
            hi.add(wr)
        elif wr is not None and not ((OP_ADDI <= i.op <= OP_XORI) and i.rs in hi and wr == i.rs):
            hi.discard(wr)
    return out


def find_body(sig, blob, base):
    """Every address in one file section where `sig` (body_signature's) matches
    word for word. The first fully kept word anchors the search."""
    k0 = next((k for k, (_, m) in enumerate(sig) if m == 0xFFFFFFFF), None)
    if k0 is None:
        return []
    pat = sig[k0][0].to_bytes(4, "little")
    out, o = [], blob.find(pat)
    while o >= 0:
        st = o - 4 * k0
        if not o & 3 and st >= 0 and st + 4 * len(sig) <= len(blob) and all(
                int.from_bytes(blob[st + 4 * k:st + 4 * k + 4], "little") & m == w for k, (w, m) in enumerate(sig)):
            out.append(base + st)
        o = blob.find(pat, o + 1)
    return out


# ------------------------------------------------------------------ checking a (PC function, PSX function) pair

def psx_fingerprint(read, start):
    """(calls, constants) of the PSX function at `start`, or None when its extent
    is not found: the jal targets in order, and the set of the immediates of
    addiu / ori / slti / sltiu / andi / xori off a register other than $sp, but 0
    and 1 (a stack frame and a flag say nothing)."""
    end = extent(read, start)
    if end is None:
        return None
    calls, consts = [], set()
    for pc in range(start, end, 4):
        i = Insn(read(pc))
        if i.op == OP_JAL:
            calls.append(i.jump_target(pc))
        elif OP_ADDI <= i.op <= OP_XORI and SP not in (i.rs, i.rt):
            v = i.simm & 0xFFFFFFFF if i.op in (OP_ADDI, OP_ADDIU, OP_SLTI, OP_SLTIU) else i.imm
            if v not in (0, 1):
                consts.add(v)
    return calls, consts


def pc_fingerprint(text, va, start, end):
    """(calls, constants) of the PC function [start, end) in .text (`text` the
    section's bytes at `va`): the direct call targets, and the set of the
    immediates other than branch targets below 0x10000 or above 0xFFFF0000 (a
    small negative), but 0 and 1. Needs capstone (the owner's tools, not a
    player's setup)."""
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    calls, consts = [], set()
    for ins in md.disasm(text[start - va:end - va], start):
        if ins.mnemonic == "call" and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
            calls.append(ins.operands[0].imm & 0xFFFFFFFF)
            continue
        if ins.mnemonic.startswith("j"):
            continue
        for op in ins.operands:
            if op.type == capstone.x86.X86_OP_IMM:
                v = op.imm & 0xFFFFFFFF
                if (v < 0x10000 or v >= 0xFFFF0000) and v not in (0, 1):
                    consts.add(v)
    return calls, consts


def pair_agrees(pc_fp, psx_fp, psx_to_pc):
    """Whether a PC function and a PSX function are one function by their
    fingerprints, and why: every PSX call to a function `psx_to_pc` pairs
    (resident twins at the evidence tier) is a call the PC makes, every PC call
    to such a function is matched, and either one call is matched or the
    constant sets overlap by half (Jaccard 0.5) or more. Measured on the
    evidence-tier pairs (docs/exe-immediate-places.md 3)."""
    pcalls, pconsts = pc_fp
    xcalls, xconsts = psx_fp
    mapped = [psx_to_pc[c] for c in xcalls if c in psx_to_pc]
    known = set(psx_to_pc.values())
    matched = sum(1 for c in mapped if c in pcalls)
    if matched != len(mapped):
        return False, "a PSX call the PC does not make"
    if sorted(c for c in pcalls if c in known) != sorted(mapped):
        return False, "a PC call the PSX does not make"
    union = pconsts | xconsts
    j = len(pconsts & xconsts) / len(union) if union else 0.0
    if matched:
        return True, "%d call(s) agree, constants %.2f" % (matched, j)
    if j >= 0.5:
        return True, "constants %.2f" % j
    return False, "no call to compare, constants %.2f" % j


# ------------------------------------------------------------------ selftest (synthetic code, no game data)

def enc_i(op, rs, rt, imm):
    return (op << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF)


def enc_r(rs, rt, rd, funct):
    return (rs << 21) | (rt << 16) | (rd << 11) | funct


def selftest():
    """The decoder, the extent, the scan and the match on a synthetic function
    assembled here: a frame, a local array of three bytes, a halfword, a word
    from a lui / ori pair and a byte of $zero filled on the stack, a negative
    halfword by addiu (its sign kept), a lui-addressed load, a call, a branch
    past the first jr $ra. Returns a list of problems."""
    errs = []
    v0, v1, a0, t0, at = 2, 3, 4, 8, 1
    base = 0x80100000
    code = [
        enc_i(OP_ADDIU, SP, SP, -0x28),        # 00 frame
        enc_i(OP_ADDIU, ZERO, v0, 3),          # 04 li v0, 3
        enc_i(0x28, SP, v0, 0x10),             # 08 sb v0, 0x10(sp)
        enc_i(OP_ORI, ZERO, v1, 0xFE),         # 0C ori v1, zero, 0xFE
        enc_i(0x28, SP, v1, 0x11),             # 10 sb v1, 0x11(sp)
        enc_i(0x28, SP, ZERO, 0x12),           # 14 sb zero, 0x12(sp)
        enc_i(OP_ADDIU, ZERO, v0, -2),         # 18 li v0, -2 (0xFFFE as a halfword)
        enc_i(0x29, SP, v0, 0x14),             # 1C sh v0, 0x14(sp)
        enc_i(OP_LUI, ZERO, t0, 0x1234),       # 20 lui t0, 0x1234
        enc_i(OP_ORI, t0, t0, 0x5678),         # 24 ori t0, t0, 0x5678
        enc_i(0x2B, SP, t0, 0x18),             # 28 sw t0, 0x18(sp)
        enc_i(OP_LUI, ZERO, at, 0x801F),       # 2C lui at, 0x801F
        enc_i(0x24, at, a0, 0x4300),           # 30 lbu a0, 0x4300(at)
        enc_i(OP_BEQ, a0, ZERO, 3),            # 34 beq a0, zero, +3 -> 0x44
        enc_i(OP_ADDIU, ZERO, v1, 6),          # 38 (delay) li v1, 6
        enc_r(RA, 0, 0, F_JR),                 # 3C jr ra (not the end: a branch goes past it)
        enc_i(0x28, SP, v1, 0x21),             # 40 (delay) sb v1, 0x21(sp)
        enc_i(OP_ADDIU, ZERO, v0, 7),          # 44 (the branch's target: the state dropped) li v0, 7
        (OP_JAL << 26) | (0x80150000 >> 2 & 0x3FFFFFF),   # 48 jal
        enc_i(0x28, SP, v0, 0x13),             # 4C (delay) sb v0, 0x13(sp): v0 still 7 in the slot
        enc_i(0x28, SP, v0, 0x20),             # 50 sb v0, 0x20(sp): v0 cleared by the call - not a constant
        enc_i(OP_ADDIU, SP, SP, 0x28),         # 54
        enc_r(RA, 0, 0, F_JR),                 # 58 jr ra: the end
        0,                                     # 5C nop (delay)
        enc_i(OP_ADDIU, ZERO, v0, 9),          # 60 the next function
    ]
    words = {base + 4 * k: w for k, w in enumerate(code)}
    read = words.get
    end = extent(read, base)
    if end != base + 0x60:
        errs.append("extent: 0x%X, not 0x60" % ((end or 0) - base))
    stores, refs = scan(read, base, end)
    got = sorted((s, disp, width, value) for s, _, disp, width, value, _ in stores)
    want = [(0x08, 0x10, 1, 3), (0x10, 0x11, 1, 0xFE), (0x14, 0x12, 1, 0), (0x1C, 0x14, 2, 0xFFFFFFFE),
            (0x28, 0x18, 4, 0x12345678), (0x40, 0x21, 1, 6), (0x4C, 0x13, 1, 7)]
    if got != want:
        errs.append("scan's stores: %s" % got)
    if [r[0] for r in refs] != [0x12345678, 0x801F4300]:
        errs.append("scan's refs: %s" % [hex(r[0]) for r in refs])
    # the PC's run: bytes 03 FE 00 07 FE FF 00 00 78 56 34 12 at the array's displacement 0x10
    run = bytes([3, 0xFE, 0, 7, 0xFE, 0xFF, 0, 0, 0x78, 0x56, 0x34, 0x12])
    nz = [k for k, b in enumerate(run) if b]
    m = match_run(run, nz, store_bytes(stores))
    if len(m) != 1 or m[0][:2] != (SP, 0x10):
        errs.append("match: %s" % [(b, a) for b, a, _ in m])
    else:
        elems = m[0][2]
        text = format_elements(elems)
        if text != "8=4,10=C,14=z,4C=44,1C=18,28=20+24":
            errs.append("elements: %s" % text)
        pieces = decode_row(read, base, 0x10, parse_elements(text))
        rebuilt = bytearray(len(run))
        for o, b in pieces:
            rebuilt[o:o + len(b)] = b
        if bytes(rebuilt) != run:
            errs.append("decode_row does not rebuild the run")
        # the $zero store included explicitly decodes to zero
        if decode_row(read, base, 0x12, parse_elements("14=z")) != [(0, b"\0")]:
            errs.append("a $zero element")
    # refusals: each is a ValueError
    for text, why in (("C=4", "a store that is not a store"), ("8=C", "a value in another register"),
                      ("8=24", "a pair's low half alone, from t0 (not $zero)"), ("28=24+20", "a pair out of order"),
                      ("8=4,8=4", "two elements over one byte"), ("1C=18,1C=18", "overlap"),
                      ("8=z", "a $zero element on a store of v0")):
        try:
            decode_row(read, base, 0x10, parse_elements(text))
            errs.append("decode_row accepted %s (%s)" % (text, why))
        except ValueError:
            pass
    for text in ("", "8", "8=", "=4", "8=1+2+3", "8=-1"):
        try:
            parse_elements(text)
            errs.append("parse_elements accepted %r" % text)
        except ValueError:
            pass
    # a value too wide for its store: li 0x1FF stored by sb
    wide = {base: enc_i(OP_ADDIU, ZERO, v0, 0x1FF), base + 4: enc_i(0x28, SP, v0, 0)}
    try:
        decode_row(wide.get, base, 0, parse_elements("4=0"))
        errs.append("decode_row accepted a value wider than its store")
    except ValueError:
        pass
    # a run two placements fit is ambiguous: match_run returns both
    twin = {base + 4 * k: w for k, w in enumerate([enc_i(OP_ADDIU, ZERO, v0, 5), enc_i(0x28, SP, v0, 0),
                                                   enc_i(0x28, SP, v0, 8), enc_r(RA, 0, 0, F_JR), 0])}
    st2, _ = scan(twin.get, base, extent(twin.get, base))
    if len(match_run(bytes([5]), [0], store_bytes(st2))) != 2:
        errs.append("match_run: an ambiguous run did not give two placements")
    # immref: the lui at 0x2C and the load at 0x30 form 0x801F4300; the load's sign is kept
    if decode_ref(read, base, *parse_ref("2C+30")) != 0x801F4300:
        errs.append("decode_ref: the lui / lbu pair")
    neg = {base: enc_i(OP_LUI, ZERO, at, 0x8020), base + 4: enc_i(OP_ADDIU, at, at, -0x10),
           base + 8: enc_i(OP_ORI, at, v0, 0x10)}
    if decode_ref(neg.get, base, 0, 4) != 0x801FFFF0:
        errs.append("decode_ref: addiu's sign")
    for h, l, why in ((0x30, 0x2C, "lo before hi: not a lui"), (0x2C, 0x34, "a branch"),
                      (0x20, 0x30, "a load off another register")):
        try:
            decode_ref(read, base, h, l)
            errs.append("decode_ref accepted %X+%X (%s)" % (h, l, why))
        except ValueError:
            pass
    for text in ("2C", "2C+", "+30", "2C+2C", "2C+30+34"):
        try:
            parse_ref(text)
            errs.append("parse_ref accepted %r" % text)
        except ValueError:
            pass
    # the same function in another build's file: the lui's immediate, the load's low
    # half and the jal's target moved, everything else equal
    sig = body_signature(read, base, end)
    other = list(code)
    other[11] = enc_i(OP_LUI, ZERO, at, 0x8020)        # lui at, 0x8020
    other[12] = enc_i(0x24, at, a0, -0x7C00)           # lbu a0, -0x7C00(at)
    other[18] = (OP_JAL << 26) | (0x80151230 >> 2 & 0x3FFFFFF)
    blob = bytes(16) + b"".join(w.to_bytes(4, "little") for w in other) + bytes(8)
    if find_body(sig, blob, 0x80200000) != [0x80200010]:
        errs.append("find_body: %s" % [hex(x) for x in find_body(sig, blob, 0x80200000)])
    other[5] = enc_i(0x28, SP, ZERO, 0x13)             # a kept field changed: no match
    blob = bytes(16) + b"".join(w.to_bytes(4, "little") for w in other) + bytes(8)
    if find_body(sig, blob, 0x80200000):
        errs.append("find_body matched a body with a changed store")
    # the pair check
    psx_to_pc = {0x80150000: 0x401000, 0x80151000: 0x402000}
    fp = psx_fingerprint(read, base)
    if fp is None or fp[0] != [0x80150000] or fp[1] != {3, 0xFE, 0xFFFFFFFE, 0x5678, 6, 7}:
        errs.append("psx_fingerprint: %s" % (fp,))
    if not pair_agrees(([0x401000], {3}), fp, psx_to_pc)[0]:
        errs.append("pair_agrees refused the matching call")
    if pair_agrees(([0x402000], {3}), fp, psx_to_pc)[0]:
        errs.append("pair_agrees took a call to another known function")
    if pair_agrees(([0x401000, 0x402000], {3}), fp, psx_to_pc)[0]:
        errs.append("pair_agrees took a PC call the PSX does not make")
    if not pair_agrees(([], {3, 0xFE, 6, 7}), ([], {3, 0xFE, 6, 7, 9}), {})[0]:
        errs.append("pair_agrees refused agreeing constants")
    if pair_agrees(([], {3}), ([], {4, 5}), {})[0]:
        errs.append("pair_agrees took disagreeing constants")
    return errs


# ------------------------------------------------------------------ pairing the functions no placed word pairs

# docs/install-gap-pairs.md. The code-pointer pairs (exe_tables.feed) reach no
# effect, magic or boss function: their tables are unplaced. These methods pair
# them from the pairs already known (symbols.toml's `psx`), on the psx-jp disc,
# each pair accepted only through pair_agrees (the gate) or, for a table slot,
# an equal call list that contradicts no known pair:
#
#   magic        Magic_Rows (PC) against its copy in the battle overlay, found by
#                the PC's file ids less 0x105 at the same stride; row N's entry
#                to row N's entry, in the file the row's id names (the disc's BIN/
#                files in LBA order)
#   kinds        Effect_KindHandlers (PC) against its psx copy, kind N's handler to
#                kind N's; a handler in an overlay address that many files share
#                takes the file whose state table agrees best (score_dispatch)
#   tables       a paired dispatcher's state table (a .data jump table, or a
#                stack table of imm32 stores on the PC) against the table the twin's
#                lui pair addresses: state N's function to state N's
#   calls        two paired functions' call lists of equal length: the n-th call
#                to the n-th call, each callee through the gate
#   tablesearch  an unpaired PC dispatcher's table among every run of PSX code
#                words, by the entries already paired; the twin the function whose
#                lui pair addresses that table
#   order        between two pairs adjacent in PC order whose twins are in one
#                section in the same order: as many functions on both sides,
#                position by position through the gate; or, where the counts
#                differ (or the next pair is another file's), in an overlay only,
#                the order-keeping alignment, each pair the one gate pass of its
#                row and its column

PAIR_RES_HI = 0x80196800                       # exe_tables.FEED_RESIDENT's end: below it, the boot executable
PAIR_BOOT = ("BOOT", None)
PAIR_GAME = ("ETC/GAME.EMI", 0)
PAIR_OVERLAYS = ("BMAGIC", "BOSS", "SCENARIO", "WORLD00", "WORLD01", "WORLD02", "WORLD03", "WORLD04")
# the sections a family's code calls into before the boot executable's (the battle overlay is loaded over its low
# part), and after the GAME.EMI field code
PAIR_BEFORE = {"BMAGIC": (("BATTLE/BATTLE.EMI", 15), ("BATTLE/BATTLE.EMI", 3)),
               "BOSS": (("BATTLE/BATTLE.EMI", 15), ("BATTLE/BATTLE.EMI", 3)),
               "BATTLE": (("BATTLE/BATTLE.EMI", 15), ("BATTLE/BATTLE.EMI", 3))}
PAIR_AFTER = {f: (("ETC/GAME.EMI", 1),) for f in ("WORLD00", "WORLD01", "WORLD02", "WORLD03", "WORLD04", "SCENARIO")}
PAIR_GATE_ONLY = ("magic", "kinds", "tables", "calls", "tablesearch", "order", "order-align")


class Pairing:
    """The pairing state over one disc (psx-jp) and BOF3.exe. `pc` {PC function: ((file, section), PSX address,
    method, why)}; seeds are symbols.toml's `psx` fields, method "seed"."""

    def __init__(self, exe, srcs, funcs, data, held_out=False):
        import bisect as _b
        import capstone
        self._b = _b
        self.cs = capstone
        self.x86 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.x86.detail = True
        import exe_tables as ET
        self.ET = ET
        self.va, _, raw, rsz = ET.pe_sections(exe)[".text"]
        self.text = exe[raw:raw + rsz]
        dva, _, draw, drsz = ET.pe_sections(exe)[".data"]
        self.dlo, self.data_bytes = dva, exe[draw:draw + drsz]
        self.srcs = srcs
        self.byfile = collections.defaultdict(list)
        for k in srcs:
            self.byfile[k[0]].append(k)
        self.fileof = {f.rsplit("/", 1)[-1]: f for f in self.byfile}
        self.funcs, self.data = funcs, data
        self.fstarts = sorted(funcs)
        self.dstarts = sorted(data)
        self.p2p = {e["psx"]: p for p, e in funcs.items() if "psx" in e and "psx_overlay" not in e
                    and 0x80010000 <= e["psx"] < PAIR_RES_HI and e.get("status") in ("evidence", "verified")}
        self.read, self.fpx_, self.fpc_, self.sig_, self.jt_ = {}, {}, {}, {}, {}
        self.pc, self.psx, self.ref, self.located = {}, {}, {}, {}
        self.hits = collections.defaultdict(collections.Counter)
        self.cross = collections.defaultdict(collections.Counter)
        self.differ, self.seen = [], set()
        self.runs = None
        self.seed(held_out)

    # -------------------------------------------------- reading both sides
    def rd(self, k):
        if k not in self.read:
            b, bl = self.srcs[k]
            self.read[k] = blob_reader(bl, b)
        return self.read[k]

    def inside(self, k, a):
        b, bl = self.srcs[k]
        return b <= a < b + len(bl)

    def is_text(self, v):
        return self.va <= v < self.va + len(self.text)

    def pcw(self, a):
        o = a - self.dlo
        return int.from_bytes(self.data_bytes[o:o + 4], "little") if 0 <= o <= len(self.data_bytes) - 4 else None

    def fend(self, t):
        i = self._b.bisect_right(self.fstarts, t)
        return min(self.fstarts[i] if i < len(self.fstarts) else t + 0x400, t + 0x2000)

    def pcdis(self, t):
        e = self.fend(t)
        return list(self.x86.disasm(self.text[t - self.va:e - self.va], t))

    def resolve(self, a, ctx):
        """The section a code address means seen from section ctx: ctx's own file, the family's battle overlay,
        the boot executable, GAME.EMI#0, the family's field overlay; None when none holds it."""
        fam = ctx[0].split("/")[0] if ctx else None
        if ctx is not None:
            for k in self.byfile[ctx[0]]:
                if self.inside(k, a):
                    return k
        for k in PAIR_BEFORE.get(fam, ()):
            if k in self.srcs and self.inside(k, a):
                return k
        if 0x80010000 <= a < PAIR_RES_HI:
            return PAIR_BOOT
        if self.inside(PAIR_GAME, a):
            return PAIR_GAME
        for k in PAIR_AFTER.get(fam, ()):
            if k in self.srcs and self.inside(k, a):
                return k
        return None

    def fpx(self, k, x):
        if (k, x) not in self.fpx_:
            self.fpx_[k, x] = psx_fingerprint(self.rd(k), x)
        return self.fpx_[k, x]

    def fpc(self, p):
        if p not in self.fpc_:
            self.fpc_[p] = pc_fingerprint(self.text, self.va, p, self.fend(p))
        return self.fpc_[p]

    def gate(self, p, k, x):
        fp = self.fpx(k, x)
        if fp is None:
            return False, "no extent"
        return pair_agrees(self.fpc(p), fp, self.p2p)

    def sig(self, k, x):
        if (k, x) not in self.sig_:
            r = self.rd(k)
            e = extent(r, x)
            s = body_signature(r, x, e) if e else None
            self.sig_[k, x] = tuple(s) if s else None
        return self.sig_[k, x]

    def shape(self, p, k, x):
        """(agree, contradictions, [(PC callee, section, PSX callee)] not yet paired) of the two call lists
        aligned position by position; None when their lengths differ."""
        fp = self.fpx(k, x)
        if fp is None:
            return None
        C, X = self.fpc(p)[0], [(self.resolve(t, k), t) for t in fp[0]]
        if len(C) != len(X):
            return None
        m, minv, agree, bad, new = {}, {}, 0, 0, []
        for c, (xk, xt) in zip(C, X):
            kp = self.pc.get(c)
            kx = self.psx.get((xk, xt)) if xk else None
            if (kp is not None and kp[1] != xt) or (kx is not None and kx != c) \
                    or m.setdefault(c, xt) != xt or minv.setdefault(xt, c) != c:
                bad += 1
            elif kp is not None or kx is not None:
                agree += 1
            elif xk is not None:
                new.append((c, xk, xt))
        return agree, bad, new

    def pc_table(self, T):
        """The code pointers of the PC table at T: its [[data]] count, else up to the next symbol (nulls kept,
        trailing nulls dropped)."""
        e = self.data.get(T)
        n = e["count"] if e and e.get("count") else 64
        j = self._b.bisect_right(self.dstarts, T)
        nxt = self.dstarts[j] if j < len(self.dstarts) else T + 4 * n
        out = []
        for i in range(n):
            w = self.pcw(T + 4 * i)
            if (T + 4 * i >= nxt and i) or w is None or not (self.is_text(w) or w == 0):
                break
            out.append(w)
        while out and not out[-1]:
            out.pop()
        return out

    def dispatch(self, p):
        """(table, [PC states]) of a dispatcher: the first `jmp` / `call [reg * 4 + T]` whose table starts with
        code, or the stack table of imm32 code addresses it calls through; None for neither."""
        if p in self.jt_:
            return self.jt_[p]
        X = self.cs.x86
        tabs, st, through = [], {}, False
        for ins in self.pcdis(p):
            ops = ins.operands
            if ins.mnemonic in ("jmp", "call") and ops and ops[0].type == X.X86_OP_MEM:
                m = ops[0].mem
                if m.base == X.X86_REG_ESP and m.scale == 4:
                    through = True
                elif m.scale == 4 and m.disp:
                    tabs.append(m.disp & 0xFFFFFFFF)
            if ins.mnemonic == "mov" and len(ops) == 2 and ops[0].type == X.X86_OP_MEM and ops[1].type == X.X86_OP_IMM:
                m, v = ops[0].mem, ops[1].imm & 0xFFFFFFFF
                if m.base == X.X86_REG_ESP and m.index == 0 and self.is_text(v) and m.disp >= 0 and m.disp % 4 == 0:
                    st[m.disp // 4] = v
        out = None
        for T in tabs:
            w = self.pcw(T)
            if w is not None and self.is_text(w):
                out = (T, self.pc_table(T))
                break
        if out is None and through and st and sorted(st) == list(range(len(st))):
            out = ("stack", [st[i] for i in range(len(st))])
        self.jt_[p] = out
        return out

    def psx_table(self, k, x, n):
        """(T', [n words]) the first table of code words (nulls allowed, the first not) a lui pair of the PSX
        function at x addresses, or None."""
        r = self.rd(k)
        e = extent(r, x)
        if e is None:
            return None
        seen = set()
        import exe_tables as ET
        for addr, offs in scan(r, x, e)[1]:
            if addr in seen:
                continue
            seen.add(addr)
            kk = self.resolve(addr, k)
            if kk is None:
                continue
            ws = [self.rd(kk)(addr + 4 * i) for i in range(n)]
            if ws and None not in ws and ws[0] and all(ET.code_word(w, False) or w == 0 for w in ws):
                return addr, ws
        return None

    # -------------------------------------------------- the pairs
    def seed_key(self, e):
        a = e["psx"]
        if "psx_overlay" in e:
            n, _, s = e["psx_overlay"].partition("#")
            f = self.fileof.get(n)
            k = (f, int(s)) if f and s else None
            return k if k in self.srcs and self.inside(k, a) else None
        if 0x80010000 <= a < PAIR_RES_HI:
            return PAIR_BOOT
        if self.inside(PAIR_GAME, a):
            return PAIR_GAME
        return None

    def locate(self, p, a):
        """The section of a `psx` with no psx_overlay above the boot executable: the one section holding a
        whose function passes the gate (GAME.EMI#1 and BATTLE.EMI#3 tried first; several only when their
        bodies are one body)."""
        pref = [k for k in (("ETC/GAME.EMI", 1), ("BATTLE/BATTLE.EMI", 3)) if k in self.srcs and self.inside(k, a)]
        cs = pref + sorted(k for k in self.srcs if k != PAIR_BOOT and k not in pref and self.inside(k, a))
        ok = [k for k in cs if self.gate(p, k, a)[0]]
        if not ok or any(self.sig(k, a) != self.sig(ok[0], a) for k in ok[1:]):
            return None
        return ok[0]

    def seed(self, held_out):
        """symbols.toml's `psx` pairs. held_out: seed only the boot executable's evidence tier and score every
        derivation against the rest (the precision measurement)."""
        for p, e in self.funcs.items():
            if "psx" not in e:
                continue
            k = self.seed_key(e)
            if k is None and "psx_overlay" not in e:
                k = self.locate(p, e["psx"])
                if k is not None:
                    self.located[p] = k
            elif k == PAIR_GAME and "psx_overlay" not in e and self.gate(p, k, e["psx"])[0]:
                self.located[p] = k        # GAME.EMI's: sym_twins reads a psx with no psx_overlay in the boot image
            if k is None:
                continue
            t = "feed" if e.get("psx_status") == "hypothesis" else "evidence"
            self.ref[p] = (k, e["psx"], t)
            if held_out and not (k == PAIR_BOOT and t == "evidence"):
                continue
            self.pc[p] = (k, e["psx"], "seed", "")
            self.psx.setdefault((k, e["psx"]), p)

    def score(self, p, k, x, method, why):
        k0, x0, t = self.ref[p]
        if x0 == x and (k0 == k or PAIR_BOOT in (k0, k)):
            v = "agree"
        elif x0 == x:
            v = "same address, other file"
        elif k0 != k and self.sig(k0, x0) is not None and self.sig(k0, x0) == self.sig(k, x):
            v = "a copy"
        else:
            v = "differ"
            self.differ.append((method, t, p, k0, x0, k, x, why))
        self.hits[method][t + " " + v] += 1

    def add(self, p, k, x, method, why):
        """True when the pair is new. A derivation of a function already paired is scored: against its seed
        (precision), or against the earlier derivation (consistency)."""
        if p in self.pc:
            k0, x0, m0, _ = self.pc[p]
            key = (p, k, x, method)
            if key not in self.seen:
                self.seen.add(key)
                if m0 == "seed":
                    self.score(p, k, x, method, why)
                else:
                    v = "agree" if (k0, x0) == (k, x) else (
                        "a copy" if self.sig(k0, x0) is not None and self.sig(k0, x0) == self.sig(k, x) else "differ")
                    self.cross["%s by %s" % (m0, method)][v] += 1
            return False
        if (k, x) in self.psx or p not in self.funcs:
            return False
        if p in self.ref:
            self.score(p, k, x, method, why)
        self.pc[p] = (k, x, method, why)
        self.psx[(k, x)] = p
        return True

    def slot_ok(self, s, k, w):
        g, why = self.gate(s, k, w)
        if g:
            return True
        sh = self.shape(s, k, w)
        return bool(sh and sh[0] + len(sh[2]) >= 2 and not sh[1])

    def score_dispatch(self, p, k, x):
        """(score, contradictions, [(pc, section, psx)]) of PC function p at PSX x in k: the gate on p, and for
        a dispatcher each state of its table that is already paired the same way or passes slot_ok."""
        g, _ = self.gate(p, k, x)
        sc, bad, out = int(g), 0, [(p, k, x)] if g else []
        dp = self.dispatch(p)
        if not dp:
            sh = self.shape(p, k, x)
            if sh and sh[0] + len(sh[2]) >= 1 and not sh[1]:
                sc += 1
                if not g:
                    out.append((p, k, x))
            return sc, bad, out
        tab = self.psx_table(k, x, len(dp[1]))
        if not tab:
            return sc, bad + 1, out
        if not g:
            out.append((p, k, x))
        for s, w in zip(dp[1], tab[1]):
            if not s or not w:
                continue
            kk = self.resolve(w, k)
            if kk is None:                         # another overlay's: chosen when the table is walked
                continue
            kx = self.psx.get((kk, w))
            if kx is not None or s in self.pc:
                if kx == s or (s in self.pc and self.pc[s][1] == w and kx is None):
                    sc += 1
                else:
                    bad += 1
                continue
            if self.slot_ok(s, kk, w):
                sc += 1
                out.append((s, kk, w))
            sh = self.shape(s, kk, w)
            if sh and sh[1]:
                bad += 1
        return sc, bad, out

    def choose(self, p, x, ctx):
        """The section for PC function p at PSX x seen from ctx (None: any file holding x), by score_dispatch:
        the best scores 2 or more with at most a third as many contradictions; a tie only between files whose
        bodies at x are one body. Returns the accepted [(pc, section, psx)]."""
        k = self.resolve(x, ctx) if ctx else None
        cs = [k] if k else [kk for kk in self.srcs if kk != PAIR_BOOT and self.inside(kk, x)]
        res = []
        for kk in cs:
            sc, bad, out = self.score_dispatch(p, kk, x)
            res.append((sc - 2 * bad, sc, bad, kk, out))
        if not res:
            return []
        res.sort(key=lambda r: (r[0], r[1]), reverse=True)
        best = res[0]
        if best[1] < 2 or best[2] > best[1] // 3:
            return []
        tied = [r for r in res if r[:2] == best[:2]]
        if len(tied) > 1 and (self.sig(best[3], x) is None or any(self.sig(r[3], x) != self.sig(best[3], x) for r in tied[1:])):
            return []
        return best[4]

    # -------------------------------------------------- the methods
    def m_magic(self, ids):
        import struct as S
        rows_at = [(p, e) for p, e in self.data.items() if e["name"] == "Magic_Rows"]
        if not rows_at:
            return 0
        T, e = rows_at[0]
        nrow = e["count"] // 2
        pc_rows = [(self.pcw(T + 8 * i) & 0xFFFF, self.pcw(T + 8 * i + 4)) for i in range(nrow)]
        want = [(f - 0x105) if f != 0xFFFF else None for f, _ in pc_rows]
        first = next(i for i, v in enumerate(want) if v is not None)
        copies = set()
        for k, (b, bl) in self.srcs.items():
            o = bl.find(S.pack("<H", want[first]))
            while o >= 0:
                base = o - 8 * first
                if base >= 0 and base + 8 * nrow <= len(bl) and all(
                        v is None or S.unpack_from("<H", bl, base + 8 * j)[0] == v for j, v in enumerate(want)):
                    copies.add(bytes(bl[base:base + 8 * nrow]))
                o = bl.find(S.pack("<H", want[first]), o + 1)
        if len(copies) != 1:
            return 0
        raw = copies.pop()
        n = 0
        for i, (pf, p) in enumerate(pc_rows):
            fid, w = S.unpack_from("<H", raw, 8 * i)[0], S.unpack_from("<I", raw, 8 * i + 4)[0]
            if pf == 0xFFFF or fid != pf - 0x105 or fid not in ids:
                continue
            ks = [k for k in self.byfile.get(ids[fid], ()) if self.inside(k, w)]
            if len(ks) == 1:
                n += self.add(p, ks[0], w, "magic", "Magic_Rows row %d: %s" % (i, ids[fid]))
        return n

    def m_kinds(self):
        import struct as S
        at = [(p, e) for p, e in self.data.items() if e["name"] == "Effect_KindHandlers" and "psx" in e]
        if not at:
            return 0
        T, e = at[0]
        ks = [k for k in self.byfile["ETC/GAME.EMI"] if self.inside(k, e["psx"])]
        if not ks:
            return 0
        b, bl = self.srcs[ks[0]]
        n = 0
        for kind in range(e["count"]):
            h = self.pcw(T + 4 * kind)
            w = S.unpack_from("<I", bl, e["psx"] + 4 * kind - b)[0]
            for p, k, x in self.choose(h, w, None):
                n += self.add(p, k, x, "kinds", "Effect_KindHandlers[0x%X]" % kind)
        return n

    def m_tables(self):
        n = 0
        for p, (k, x, m, _) in list(self.pc.items()):
            dp = self.dispatch(p)
            if not dp:
                continue
            tab = self.psx_table(k, x, len(dp[1]))
            if not tab:
                continue
            tn = dp[0] if dp[0] == "stack" else "0x%06X" % dp[0]
            for i, (s, w) in enumerate(zip(dp[1], tab[1])):
                if not s or not w:
                    continue
                why = "state %d of 0x%06X's table %s" % (i, p, tn)
                kk = self.resolve(w, k)
                if kk is not None:
                    if self.slot_ok(s, kk, w):
                        n += self.add(s, kk, w, "tables", why)
                else:
                    for s2, k2, x2 in self.choose(s, w, k):
                        n += self.add(s2, k2, x2, "tables", why)
        return n

    def m_calls(self):
        n = 0
        for p, (k, x, m, _) in list(self.pc.items()):
            sh = self.shape(p, k, x)
            if not sh or sh[1]:
                continue
            for c, xk, xt in sh[2]:
                g, why = self.gate(c, xk, xt)
                if g:
                    n += self.add(c, xk, xt, "calls", "a call of 0x%06X (%s)" % (p, why))
        return n

    def code_runs(self):
        """[(section, address, words)]: every run of four or more aligned PSX code words."""
        if self.runs is None:
            import struct as S
            self.runs = []
            for k, (b, bl) in self.srcs.items():
                if k == PAIR_BOOT or len(bl) < 4:
                    continue
                ws = S.unpack_from("<%dI" % (len(bl) // 4), bl, 0)
                i = 0
                while i < len(ws):
                    j = i
                    while j < len(ws) and 0x80010000 <= ws[j] < 0x80200000 and not ws[j] & 3:
                        j += 1
                    if j - i >= 4:
                        self.runs.append((k, b + 4 * i, ws[i:j]))
                    i = j + 1
        return self.runs

    def owner_of(self, k, T):
        """The PSX functions of k's file whose lui pair addresses T (a function start: after a jr $ra and its
        delay slot)."""
        import struct as S
        hi, lo = (T + 0x8000) >> 16 & 0xFFFF, T & 0xFFFF
        out = set()
        for kk in self.byfile[k[0]]:
            b, bl = self.srcs[kk]
            if len(bl) < 4:
                continue
            ws = S.unpack_from("<%dI" % (len(bl) // 4), bl, 0)
            for i, w in enumerate(ws):
                if w >> 26 != OP_LUI or w & 0xFFFF != hi:
                    continue
                r = (w >> 16) & 31
                if not any(v & 0xFFFF == lo and (v >> 21) & 31 == r and v >> 26 in (OP_ADDIU, 0x23, OP_ORI)
                           for v in ws[i + 1:i + 12]):
                    continue
                rd = self.rd(kk)
                for back in range(0, 0x400, 4):
                    st = b + 4 * i - back
                    e = extent(rd, st)
                    if e and st <= b + 4 * i < e and rd(st - 8) == 0x03E00008:
                        out.add((kk, st))
                        break
        return out

    def m_tablesearch(self):
        n = 0
        for p in self.fstarts:
            if p in self.pc:
                continue
            dp = self.dispatch(p)
            if not dp or len(dp[1]) < 4:
                continue
            states = dp[1]
            known = [(i, self.pc[s][1]) for i, s in enumerate(states) if s and s in self.pc]
            if len(known) < 3:
                continue
            best = []
            for k, a, ws in self.code_runs():
                for off in range(len(ws) - len(states) + 1):
                    ag = sum(1 for i, x in known if ws[off + i] == x)
                    if ag < max(3, 0.75 * len(known)):
                        continue
                    if any(ws[off + i] != x and (self.pc[states[i]][0], ws[off + i]) in self.psx for i, x in known):
                        continue
                    best.append((ag, k, a + 4 * off, tuple(ws[off:off + len(states)])))
            if not best:
                continue
            top = max(b[0] for b in best)
            best = [b for b in best if b[0] == top]
            if len({b[3] for b in best}) != 1:
                continue
            best.sort(key=lambda b: (b[1][0] not in ("BATTLE/BATTLE.EMI", "ETC/GAME.EMI"), b[1]))
            _, k, t, _ = best[0]
            own = self.owner_of(k, t)
            if len(own) == 1:
                kk, st = own.pop()
                n += self.add(p, kk, st, "tablesearch", "its table %s at 0x%08X in %s (%d of %d paired entries agree)" % (
                    dp[0] if dp[0] == "stack" else "0x%06X" % dp[0], t, k[0], top, len(known)))
        return n

    def walk(self, k, a, b=None, most=40):
        """The PSX function starts after the one at a, walking extents over zero words: up to b (None when the
        walk does not land on b), or up to `most` until the walk leaves the code."""
        r = self.rd(k)
        e = extent(r, a)
        out = []
        while e is not None and (b is None or e < b) and len(out) < most:
            while (b is None or e < b) and r(e) == 0:
                e += 4
            if b is not None and e >= b:
                break
            nx = extent(r, e)
            if nx is None:
                break
            out.append(e)
            e = nx
        if b is not None:
            return out if e is not None and e <= b else None
        return out

    def m_order(self):
        n = 0
        items = sorted(self.pc.items())
        for (p1, v1), (p2, v2) in zip(items, items[1:]):
            k = v1[0]
            tail = k != v2[0]
            if k == PAIR_BOOT or (not tail and not 0 < v2[1] - v1[1] <= 0x4000):
                continue
            mids = self.fstarts[self._b.bisect_right(self.fstarts, p1):self._b.bisect_left(self.fstarts, p2)]
            if not mids or len(mids) > 30:
                continue
            xs = self.walk(k, v1[1]) if tail else self.walk(k, v1[1], v2[1])
            if not xs:
                continue
            between = "between 0x%06X and 0x%06X" % (p1, p2)
            if not tail and len(xs) == len(mids) and len(mids) <= 12:
                for p, x in zip(mids, xs):
                    g, why = self.gate(p, k, x)
                    if g:
                        n += self.add(p, k, x, "order", "%s (%s)" % (between, why))
                continue
            if len(xs) > 40 or k[0].split("/")[0] not in PAIR_OVERLAYS:
                continue
            ok = [[self.gate(p, k, x)[0] for x in xs] for p in mids]
            L = [[0] * (len(xs) + 1) for _ in range(len(mids) + 1)]
            for i in range(len(mids) - 1, -1, -1):
                for j in range(len(xs) - 1, -1, -1):
                    L[i][j] = max(L[i + 1][j], L[i][j + 1], L[i + 1][j + 1] + 1 if ok[i][j] else 0)
            i = j = 0
            while i < len(mids) and j < len(xs):
                if ok[i][j] and L[i][j] == L[i + 1][j + 1] + 1:
                    if sum(ok[i]) == 1 and sum(row[j] for row in ok) == 1:
                        n += self.add(mids[i], k, xs[j], "order-align", "%s, the one gate pass by order" % between)
                    i, j = i + 1, j + 1
                elif L[i + 1][j] >= L[i][j + 1]:
                    i += 1
                else:
                    j += 1
        return n

    def run(self, ids, report=print):
        for rnd in range(12):
            got = collections.Counter()
            got["magic"] += self.m_magic(ids)
            got["kinds"] += self.m_kinds()
            got["tables"] += self.m_tables()
            got["calls"] += self.m_calls()
            got["tablesearch"] += self.m_tablesearch()
            got["order"] += self.m_order()
            report("pair: round %d: %s" % (rnd, ", ".join("%s +%d" % kv for kv in sorted(got.items()) if kv[1])))
            if not sum(got.values()):
                break


def bin_ids(disc):
    """{PSX file id: the sources' file name}: the disc's BIN/ files in LBA order (the order the sibling's
    analysis/file_ids.json lists; 883 of 883 equal on psx-jp, measured 2026-10-18)."""
    fs = sorted((lba, p) for p, (lba, size) in disc.files.items() if p.startswith("BIN/"))
    return {i: p[4:] for i, (lba, p) in enumerate(fs)}


def pair_load(game, disc_path, held_out=False):
    import os, tomllib
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import psx_disc, exe_twins, exe_tables, tables
    disc = psx_disc.Disc(disc_path)
    _, _, builds = tables.load()
    if tables.disc_build(disc, builds)["id"] != exe_tables.FEED_BUILD:
        raise SystemExit("pair needs the %s disc (symbols.toml's psx addresses are its)" % exe_tables.FEED_BUILD)
    with open(os.path.join(exe_tables.ROOT, "symbols.toml"), "rb") as f:
        t = tomllib.load(f)
    funcs = {e["pc"]: e for e in t["func"]}
    data = {e["pc"]: e for e in t.get("data", [])}
    with open(os.path.join(game, "BOF3.exe"), "rb") as f:
        exe = f.read()
    srcs = {(f, s): (b, blob) for f, s, b, blob in exe_twins.sources(disc)}
    P = Pairing(exe, srcs, funcs, data, held_out)
    P.run(bin_ids(disc))
    return P


def pair_write(P, today):
    """symbols.toml: each new pair of a gate-accepted method on a [[func]] with no `psx` gets psx, psx_overlay
    (not for the boot executable), psx_status = "hypothesis" and psx_evidence; each seed `locate` found a file
    for gets its psx_overlay. Inserted after the entry's name (or psx) line, keyed by its pc. Returns
    (pairs written, overlays written)."""
    import os
    import exe_tables
    path = os.path.join(exe_tables.ROOT, "symbols.toml")
    with open(path, "rb") as f:
        raw = f.read()
    nl = b"\r\n" if b"\r\n" in raw else b"\n"
    lines = raw.split(nl)
    starts, cur, block = {}, False, None
    for j, l in enumerate(lines):
        if l.startswith(b"[["):
            cur, block = l.strip() == b"[[func]]", j
        elif cur and l.startswith(b"pc = 0x"):
            starts[int(l[5:].strip(), 16)] = block
    ov = lambda k: "" if k == PAIR_BOOT else "%s#%d" % (k[0].rsplit("/", 1)[-1], k[1])
    todo = {}
    for p, (k, x, m, why) in P.pc.items():
        if m in PAIR_GATE_ONLY and "psx" not in P.funcs[p]:
            new = ["psx = 0x%08X" % x]
            if ov(k):
                new.append('psx_overlay = "%s"' % ov(k))
            new += ['psx_status = "hypothesis"',
                    'psx_evidence = "exe_imm.py pair %s, %s: %s (docs/install-gap-pairs.md)"' % (today, m, why.replace('"', "'"))]
            todo[p] = new
    nov = 0
    for p, k in P.located.items():
        if p not in todo and ov(k):
            todo[p] = ['psx_overlay = "%s"' % ov(k)]
            nov += 1
    ins = {}
    for p, new in todo.items():
        j = starts[p] + 1
        after = None
        while j < len(lines) and not lines[j].startswith(b"[[") and lines[j].strip():
            if lines[j].startswith(b"name = ") or lines[j].startswith(b"psx = "):
                after = j
            j += 1
        if after is None:
            raise SystemExit("symbols.toml: the [[func]] at 0x%06X has no name line" % p)
        ins[after] = [x.encode("utf-8") for x in new]
    out = []
    for j, l in enumerate(lines):
        out.append(l)
        out += ins.get(j, [])
    with open(path, "wb") as f:
        f.write(nl.join(out))
    return len(todo) - nov, nov


def pair_report(P, out=None):
    by = collections.Counter(v[2] for v in P.pc.values())
    print("pair: %s" % ", ".join("%s %d" % kv for kv in sorted(by.items())))
    print("pair: %d psx without a psx_overlay located by the gate" % len(P.located))
    for m in sorted(P.hits):
        print("pair: precision %-12s %s" % (m, ", ".join("%s %d" % kv for kv in sorted(P.hits[m].items()))))
    for m in sorted(P.cross):
        print("pair: consistency %-28s %s" % (m, ", ".join("%s %d" % kv for kv in sorted(P.cross[m].items()))))
    for m, t, p, k0, x0, k, x, why in P.differ:
        print("pair: differs (%s, %s): 0x%06X has psx 0x%08X (%s), the derivation 0x%08X (%s): %s" % (
            m, t, p, x0, k0[0], x, k[0], why))
    if out:
        with open(out, "w", newline="\n") as f:
            f.write("pc\tfile\tsection\tpsx\tmethod\twhy\n")
            for p, (k, x, m, why) in sorted(P.pc.items()):
                if m != "seed":
                    f.write("0x%06X\t%s\t%s\t0x%08X\t%s\t%s\n" % (p, k[0], "-" if k[1] is None else k[1], x, m, why))


FIXTURES = __file__.rsplit(".", 1)[0] + "_fixtures.tsv"


def fixtures(paths):
    """tools/exe_imm_fixtures.tsv against the held discs (`paths`: disc paths; a
    build none of them is skipped and said so). Returns (problems, rows run)."""
    import os
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import psx_disc, exe_twins, tables
    _, _, builds = tables.load()
    held = {}
    for p in paths:
        d = psx_disc.Disc(p)
        held[tables.disc_build(d, builds)["id"]] = {(f, s): (b, blob) for f, s, b, blob in exe_twins.sources(d)}
    errs, ran = [], 0
    with open(FIXTURES, encoding="utf-8") as f:
        lines = [l.rstrip("\n").split("\t") for l in f if not l.startswith("#")]
    cols = lines[0]
    for c in lines[1:]:
        r = dict(zip(cols, c))
        bid, key, func = r["build"], (r["file"], int(r["section"])), int(r["func"], 16)
        if bid not in held or (r["kind"] == "body" and "psx-jp" not in held):
            print("fixtures: %s not held, skipped" % bid)
            continue
        ran += 1
        where = "%s %s %s#%s 0x%08X" % (r["kind"], bid, key[0], key[1], func)
        if r["kind"] == "scan":
            read = blob_reader(held[bid][key][1], held[bid][key][0])
            end = extent(read, func)
            if end is None or end - func != int(r["extent"]):
                errs.append("%s: extent %s, the fixture says %s" % (where, end and end - func, r["extent"]))
                continue
            stores, refs = scan(read, func, end)
            if (len(stores), len(refs)) != (int(r["stores"]), int(r["refs"])):
                errs.append("%s: %d stores, %d refs, the fixture says %s, %s" % (where, len(stores), len(refs), r["stores"], r["refs"]))
            if decode_ref(read, func, *parse_ref(r["pair"])) != int(r["addr"], 16):
                errs.append("%s: the pair %s decodes elsewhere" % (where, r["pair"]))
        elif r["kind"] == "body":
            jr = blob_reader(held["psx-jp"][key][1], held["psx-jp"][key][0])
            sig = body_signature(jr, func, extent(jr, func))
            n = len(find_body(sig, held[bid][key][1], held[bid][key][0]))
            if n != int(r["extent"]):
                errs.append("%s: found %d times, the fixture says %s" % (where, n, r["extent"]))
        else:
            errs.append("%s: kind" % where)
    return errs, ran


def main():
    if sys.argv[1:2] == ["fixtures"]:
        errs, ran = fixtures(sys.argv[2:])
        for e in errs:
            print("error: " + e)
        print("exe_imm fixtures: %d rows run, %d problem(s)" % (ran, len(errs)))
        return 1 if errs else 0
    if sys.argv[1:2] == ["pair"]:
        import argparse, datetime
        ap = argparse.ArgumentParser(prog="exe_imm.py pair")
        ap.add_argument("--game", required=True)
        ap.add_argument("--disc", required=True, help="the psx-jp disc (symbols.toml's psx addresses are its)")
        ap.add_argument("--out", help="a TSV of every derived pair")
        ap.add_argument("--held-out", action="store_true",
                        help="seed only the boot executable's evidence-tier pairs; score the rest (precision)")
        ap.add_argument("--write", action="store_true", help="write the pairs into symbols.toml")
        a = ap.parse_args(sys.argv[2:])
        if a.held_out and a.write:
            raise SystemExit("--held-out pairs are a measurement; they are not written")
        P = pair_load(a.game, a.disc, a.held_out)
        pair_report(P, a.out)
        if a.write:
            print("symbols.toml: %d pairs, %d psx_overlay fields written" % pair_write(P, datetime.date.today().isoformat()))
        return 0
    if sys.argv[1:] == ["selftest"]:
        errs = selftest()
        for e in errs:
            print("error: " + e)
        print("exe_imm selftest: %d problem(s)" % len(errs))
        return 1 if errs else 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
