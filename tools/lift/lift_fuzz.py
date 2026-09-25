#!/usr/bin/env python
"""Differential fuzz: lifted C against the original bytes, run in an emulator.

    python tools/lift/lift_fuzz.py --pe corpus.exe --entries entries.txt \
        --lib lifted.so [--rounds 2000] [--only NAME]

The oracle is the original machine code, executed by Unicorn (QEMU's x86 core)
from the PE's own bytes; the candidate is lift_x86.py's C, compiled for the
host (x86-64 here) and called through ctypes. Each round gives both sides the
same state - random registers, random bytes in every writable section and in
the stack, arguments drawn from boundaries and noise, the x87 control word
0x027F that BOF3.exe runs under - runs one function from its entry to its
return, and compares: the eight registers, the x87 TOP and st(0) if it moved,
every writable byte of the image and the stack window.

This is the shape of the project's differential fuzz (docs/SCAFFOLDING.md,
CloneOriginal): the original runs beside the candidate on identical state.
The difference is the executor. An emulator needs no Windows and no game
process, so this runs anywhere - on the synthetic corpus in CI, on BOF3.exe's
bytes on the owner's machine (its leaf functions first: an import call or a
callee that was not lifted ends the round as "not comparable", not as a pass).
"""
import argparse, ctypes, math, os, random, struct, sys

from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn import x86_const as U

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lift_x86 import Image, read_entries   # noqa: E402

STACK_LO, STACK_HI = 0x00100000, 0x00200000
ESP0 = 0x001F0000          # the call's esp: the return address is at [ESP0]
STACK_WINDOW = (ESP0 - 0x8000, ESP0 + 0x100)
RET_SENTINEL = 0x00080000  # a mapped page the function returns to
MAX_INSNS = 2_000_000
REGS = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
UREG = {r: getattr(U, f'UC_X86_REG_{r.upper()}') for r in REGS}


class Cpu(ctypes.Structure):
    _fields_ = [(r, ctypes.c_uint32) for r in REGS] + [
        ('st', ctypes.c_double * 8), ('top', ctypes.c_uint32), ('cw', ctypes.c_uint16), ('sw', ctypes.c_uint16)]


def f80_to_double(mant, se):
    sign = -1.0 if se & 0x8000 else 1.0
    e = se & 0x7FFF
    if e == 0x7FFF:
        return math.nan if mant & ((1 << 63) - 1) else sign * math.inf
    if mant == 0:
        return sign * 0.0
    try:
        return sign * math.ldexp(float(mant), e - 16383 - 63)
    except OverflowError:
        return sign * math.inf


def same_double(a, b):
    if math.isnan(a) and math.isnan(b):
        return True
    return struct.pack('<d', a) == struct.pack('<d', b)


BOUNDARY = [0, 1, 2, 3, 7, 8, 15, 16, 29, 30, 31, 32, 63, 64, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000,
            0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFF8000]


def arg(rng, pointers=()):
    k = rng.random()
    if pointers and k < 0.2:            # a pointer into a writable section
        va, n = rng.choice(pointers)
        return va + (rng.randrange(0, max(n - 64, 1)) & ~3)
    if k < 0.35:
        return rng.choice(BOUNDARY)
    if k < 0.6:
        return rng.randrange(0, 256)
    if k < 0.75:
        return (-rng.randrange(1, 256)) & 0xFFFFFFFF
    return rng.getrandbits(32)


class Harness:
    def __init__(self, img, lib, cw=0x027F):
        self.img, self.cw = img, cw
        self.lib = ctypes.CDLL(lib)
        self.lib.rt_init.restype = ctypes.c_int
        if self.lib.rt_init():
            raise SystemExit('rt_init: could not reserve the 4 GiB guest space')
        self.lib.rt_memory.restype = ctypes.c_void_p
        self.lib.rt_cpu.restype = ctypes.POINTER(Cpu)
        self.lib.rt_call.argtypes = [ctypes.c_uint32]
        self.lib.rt_last_failure.restype = ctypes.c_char_p
        self.lib.rt_fldcw.argtypes = [ctypes.c_uint16]
        self.M = self.lib.rt_memory()
        self.R = self.lib.rt_cpu().contents
        # Writable ranges: the image's writable sections and the stack window.
        self.writable = [(va, n) for name, va, n, ch in img.sections if ch & 0x80000000]
        self.compare = self.writable + [(STACK_WINDOW[0], STACK_WINDOW[1] - STACK_WINDOW[0])]

    def mput(self, va, data):
        ctypes.memmove(self.M + va, data, len(data))

    def mget(self, va, n):
        return ctypes.string_at(self.M + va, n)

    def new_uc(self):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        size = (self.img.size + 0xFFF) & ~0xFFF
        uc.mem_map(self.img.base, size)
        uc.mem_write(self.img.base, bytes(self.img.mem))
        uc.mem_map(STACK_LO, STACK_HI - STACK_LO)
        uc.mem_map(RET_SENTINEL, 0x1000)
        self.count = 0

        def hook(uc_, addr, size_, user):
            self.count += 1
            if self.count > MAX_INSNS:
                uc_.emu_stop()
        uc.hook_add(UC_HOOK_CODE, hook)
        return uc

    def round(self, entry, rng):
        """One round. Returns None on a match, 'skip: ...' when the original
        itself faults (not comparable), or a description of the mismatch."""
        state = {r: rng.getrandbits(32) for r in REGS}
        state['esp'] = ESP0
        data = [(va, bytes(rng.getrandbits(8) for _ in range(n))) for va, n in self.writable]
        lo, hi = STACK_WINDOW
        stack = bytearray(rng.getrandbits(8) for _ in range(hi - lo))
        args = [arg(rng, self.writable) for _ in range(6)]
        struct.pack_into('<I', stack, ESP0 - lo, RET_SENTINEL)
        for i, a in enumerate(args):
            struct.pack_into('<I', stack, ESP0 - lo + 4 + 4 * i, a)
        for k in ('ecx', 'edx'):             # fastcall's register arguments
            if rng.random() < 0.5:
                state[k] = arg(rng, self.writable)

        # -- the original, in the emulator
        uc = self.new_uc()
        for va, b in data:
            uc.mem_write(va, b)
        uc.mem_write(lo, bytes(stack))
        for r in REGS:
            uc.reg_write(UREG[r], state[r])
        uc.reg_write(U.UC_X86_REG_FPCW, self.cw)
        try:
            uc.emu_start(entry, RET_SENTINEL)
        except UcError as e:
            return f'skip: the original faults ({e})'
        if self.count > MAX_INSNS:
            return 'skip: the original did not return'
        want = {r: uc.reg_read(UREG[r]) for r in REGS}
        sw = uc.reg_read(U.UC_X86_REG_FPSW)
        want_top = (sw >> 11) & 7
        want_st0 = f80_to_double(*uc.reg_read(U.UC_X86_REG_FP0 + want_top)) if want_top != 0 else None
        want_mem = [uc.mem_read(va, n) for va, n in self.compare]

        # -- the lifted C, on the host
        for va, n in self.writable:
            ctypes.memset(self.M + va, 0, n)
        self.mput(self.img.base, bytes(self.img.mem))
        for va, b in data:
            self.mput(va, b)
        self.mput(lo, bytes(stack))
        for r in REGS:
            setattr(self.R, r, state[r])
        self.R.top = 0
        self.R.sw = 0
        self.lib.rt_fldcw(0x027F)
        if self.lib.rt_call(entry):
            return f'lifted code failed: {self.lib.rt_last_failure().decode()}'
        got = {r: getattr(self.R, r) for r in REGS}

        diffs = [f'{r} {got[r]:#x} want {want[r]:#x}' for r in REGS if got[r] != want[r]]
        if self.R.top != want_top:
            diffs.append(f'x87 TOP {self.R.top} want {want_top}')
        elif want_st0 is not None:
            got_st0 = self.R.st[self.R.top]
            if not same_double(got_st0, want_st0):
                diffs.append(f'st(0) {got_st0!r} want {want_st0!r}')
        for (va, n), w in zip(self.compare, want_mem):
            g = self.mget(va, n)
            if g != bytes(w):
                i = next(i for i in range(n) if g[i] != w[i])
                diffs.append(f'memory at {va + i:#x}: {g[i]:#04x} want {w[i]:#04x}')
                break
        if diffs:
            return '; '.join(diffs) + f' (args {", ".join(f"{a:#x}" for a in args[:4])})'
        return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--pe', required=True)
    ap.add_argument('--entries', required=True)
    ap.add_argument('--lib', required=True, help='lifted C + lift_rt.c, built as a shared library')
    ap.add_argument('--rounds', type=int, default=2000)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--only', action='append', help='fuzz only these names (repeatable)')
    ap.add_argument('--cw', type=lambda s: int(s, 0), default=0x027F,
                    help="the oracle's x87 control word (BOF3.exe's is 0x027F)")
    a = ap.parse_args()

    img = Image(a.pe)
    entries = read_entries(a.entries)
    h = Harness(img, os.path.abspath(a.lib), a.cw)
    rng = random.Random(a.seed)
    total_bad = total_skip = 0
    for va, name in sorted(entries.items(), key=lambda kv: kv[1]):
        if a.only and name not in a.only:
            continue
        ok = bad = skip = 0
        first = None
        for _ in range(a.rounds):
            r = h.round(va, rng)
            if r is None:
                ok += 1
            elif r.startswith('skip'):
                skip += 1
            else:
                bad += 1
                first = first or r
        total_bad += bad
        total_skip += skip
        line = f'{name:24} {va:#x}  {ok:6} match  {bad:5} mismatch  {skip:5} skipped'
        print(line + (f'\n    first: {first}' if first else ''))
    print(f'lift_fuzz: {total_bad} mismatching rounds in total, {total_skip} skipped')
    return 1 if total_bad else 0


if __name__ == '__main__':
    sys.exit(main())
