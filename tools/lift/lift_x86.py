#!/usr/bin/env python
"""A crude x86 -> C lifter: the prototype of IDEAS.md I3 (docs/lifter-feasibility.md).

    python tools/lift/lift_x86.py --pe corpus.exe --entries entries.txt --out lifted.c

Reads an i386 PE with a fixed image base and a list of function entries (one
hex address per line, optionally followed by a name), decodes each function by
recursive descent from its entry, and emits one C function per entry over the
runtime in lift_rt.h: registers and flags as locals, x87 as a stack of doubles,
guest memory as the flat array M. The output is not meant to be read; it is
meant to be CORRECT and to compile for any 64-bit host. What "correct" means is
decided by tools/lift/lift_fuzz.py, against the original bytes run in an
emulator.

What it refuses rather than guesses (CLAUDE.md rule 4 applies to generated
code too): an instruction it has no semantics for, a segment override (SEH),
an indirect jump that is not a recognised table, a call into the import table.
Each becomes rt_fail() at that address, reached only if the path runs, and is
counted in the report so a gap is never silent.

Reads nothing but the file it is given. Point it at BOF3.exe only on your own
machine; its output is derived from the binary and is game data (CLAUDE.md
rule 1) - never commit it.
"""
import argparse, collections, struct, sys

import capstone
from capstone import x86_const as X

# --- PE ----------------------------------------------------------------------


class Image:
    def __init__(self, path):
        data = open(path, 'rb').read()
        pe = struct.unpack_from('<I', data, 0x3C)[0]
        if data[pe:pe + 4] != b'PE\0\0':
            raise SystemExit(f'{path}: not a PE')
        machine, nsec = struct.unpack_from('<HH', data, pe + 4)
        if machine != 0x14C:
            raise SystemExit(f'{path}: not i386')
        opt = pe + 24
        self.base = struct.unpack_from('<I', data, opt + 28)[0]
        self.size = struct.unpack_from('<I', data, opt + 56)[0]
        optsize = struct.unpack_from('<H', data, pe + 20)[0]
        imp_rva, imp_size = struct.unpack_from('<II', data, opt + 96 + 8 * 12)  # IAT directory
        self.iat = (self.base + imp_rva, self.base + imp_rva + imp_size) if imp_size else (0, 0)
        self.sections = []
        self.mem = bytearray(self.size)
        self.mem[:0x1000] = data[:0x1000]
        sh = opt + optsize
        for i in range(nsec):
            name, vsize, rva, rsize, rptr = struct.unpack_from('<8sIIII', data, sh + 40 * i)
            chars = struct.unpack_from('<I', data, sh + 40 * i + 36)[0]
            n = min(rsize, vsize)
            self.mem[rva:rva + n] = data[rptr:rptr + n]
            self.sections.append((name.rstrip(b'\0').decode(), self.base + rva, max(vsize, rsize), chars))

    def section_of(self, va):
        for s in self.sections:
            if s[1] <= va < s[1] + s[2]:
                return s
        return None

    def is_code(self, va):
        s = self.section_of(va)
        return bool(s and s[3] & 0x20000000)   # IMAGE_SCN_MEM_EXECUTE

    def u32(self, va):
        return struct.unpack_from('<I', self.mem, va - self.base)[0]

    def bytes_at(self, va, n):
        o = va - self.base
        return bytes(self.mem[o:o + n])


# --- operands ----------------------------------------------------------------

R32 = {X.X86_REG_EAX: 'eax', X.X86_REG_ECX: 'ecx', X.X86_REG_EDX: 'edx', X.X86_REG_EBX: 'ebx',
       X.X86_REG_ESP: 'esp', X.X86_REG_EBP: 'ebp', X.X86_REG_ESI: 'esi', X.X86_REG_EDI: 'edi'}
R16 = {X.X86_REG_AX: 'eax', X.X86_REG_CX: 'ecx', X.X86_REG_DX: 'edx', X.X86_REG_BX: 'ebx',
       X.X86_REG_SP: 'esp', X.X86_REG_BP: 'ebp', X.X86_REG_SI: 'esi', X.X86_REG_DI: 'edi'}
R8L = {X.X86_REG_AL: 'eax', X.X86_REG_CL: 'ecx', X.X86_REG_DL: 'edx', X.X86_REG_BL: 'ebx'}
R8H = {X.X86_REG_AH: 'eax', X.X86_REG_CH: 'ecx', X.X86_REG_DH: 'edx', X.X86_REG_BH: 'ebx'}
ST = {getattr(X, f'X86_REG_ST{i}'): i for i in range(8)}
REGS = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
UT = {1: 'u8', 2: 'u16', 4: 'u32', 8: 'u64'}
ST_T = {1: 's8', 2: 's16', 4: 's32', 8: 's64'}
MASK = {1: '0xFFu', 2: '0xFFFFu', 4: '0xFFFFFFFFu'}
SIGN = {1: '0x80u', 2: '0x8000u', 4: '0x80000000u'}

CC = {
    'o': 'of', 'no': '!of', 'b': 'cf', 'c': 'cf', 'nae': 'cf', 'ae': '!cf', 'nb': '!cf', 'nc': '!cf',
    'e': 'zf', 'z': 'zf', 'ne': '!zf', 'nz': '!zf', 'be': '(cf|zf)', 'na': '(cf|zf)',
    'a': '(!cf&&!zf)', 'nbe': '(!cf&&!zf)', 's': 'sf', 'ns': '!sf', 'p': 'pf', 'pe': 'pf',
    'np': '!pf', 'po': '!pf', 'l': '(sf!=of)', 'nge': '(sf!=of)', 'ge': '(sf==of)', 'nl': '(sf==of)',
    'le': '(zf||sf!=of)', 'ng': '(zf||sf!=of)', 'g': '(!zf&&sf==of)', 'nle': '(!zf&&sf==of)',
}


class Unsupported(Exception):
    pass


class Func:
    """One function: recursive descent from its entry, then C."""

    def __init__(self, lifter, entry, name):
        self.L, self.entry, self.name = lifter, entry, name
        self.insns = {}          # va -> capstone insn
        self.tables = {}         # va of the indirect jmp -> [targets]
        self.fails = []          # (va, reason) of code emitted as rt_fail

    # -- decoding
    def decode(self):
        L = self.L
        work, seen = [self.entry], set()
        while work:
            va = work.pop()
            while va not in seen:
                if va != self.entry and va in L.entries:
                    break          # fell into another function: a tail call, emitted as one
                if not L.img.is_code(va):
                    raise Unsupported(f'flow leaves code at {va:#x}')
                seen.add(va)
                insn = next(L.md.disasm(L.img.bytes_at(va, 16), va, 1), None)
                if insn is None:
                    raise Unsupported(f'undecodable bytes at {va:#x}')
                self.insns[va] = insn
                m = insn.mnemonic
                nxt = va + insn.size
                if m in ('ret', 'retf', 'hlt', 'int3', 'ud2'):
                    break
                if m == 'jmp':
                    op = insn.operands[0]
                    if op.type == X.X86_OP_IMM:
                        t = op.imm
                        if t not in L.entries or t == self.entry:
                            work.append(t)
                    elif op.type == X.X86_OP_MEM and op.mem.index != 0 and op.mem.base == 0 \
                            and op.mem.scale == 4 and op.mem.segment == 0:
                        targets = self.table_targets(insn)
                        self.tables[va] = targets
                        work.extend(t for t in targets if t not in L.entries)
                    break
                if m.startswith('j') or m in ('loop', 'loope', 'loopne'):
                    work.append(insn.operands[0].imm)
                va = nxt

    def table_targets(self, insn):
        """A jmp [table + reg*4]: the entries, bounded by a preceding cmp reg, n / ja when found."""
        L, mem = self.L, insn.operands[0].mem
        table, idx = mem.disp & 0xFFFFFFFF, mem.index
        bound = None
        # Look back a few instructions in address order for cmp idx, imm.
        prev = sorted(a for a in self.insns if a < insn.address)[-8:]
        for a in reversed(prev):
            p = self.insns[a]
            if p.mnemonic == 'cmp' and len(p.operands) == 2 and p.operands[0].type == X.X86_OP_REG \
                    and p.operands[0].reg == idx and p.operands[1].type == X.X86_OP_IMM:
                bound = p.operands[1].imm + 1
                break
        targets, n = [], 0
        while n < (bound if bound else 1024):
            s = L.img.section_of(table + 4 * n)
            if s is None:
                break
            t = L.img.u32(table + 4 * n)
            if not L.img.is_code(t):
                if bound:
                    raise Unsupported(f'jump table {table:#x} entry {n} is not code')
                break
            targets.append(t)
            n += 1
        if not targets:
            raise Unsupported(f'jump table at {table:#x}: no entries')
        L.stats['jump tables'] += 1
        if not bound:
            L.stats['jump tables bounded by scan'] += 1
        return sorted(set(targets))

    # -- emission
    def emit(self):
        L = self.L
        out = [f'// {self.name} at {self.entry:#010x}: {len(self.insns)} instructions',
               f'void fn_{self.entry:08X}(void) {{',
               '    u32 ' + ', '.join(f'{r} = R.{r}' for r in REGS) + ';',
               '    u8 cf = 0, zf = 0, sf = 0, of = 0, pf = 0;',
               '    (void)cf; (void)zf; (void)sf; (void)of; (void)pf;']
        order = sorted(self.insns)
        if order[0] != self.entry:
            out.append(f'    goto L_{self.entry:08X};')
        for i, va in enumerate(order):
            insn = self.insns[va]
            out.append(f'L_{va:08X}: /* {insn.mnemonic} {insn.op_str} */')
            try:
                body = self.insn_c(insn)
            except Unsupported as e:
                self.fails.append((va, str(e)))
                L.stats['emitted as rt_fail'] += 1
                body = [f'SAVE(); rt_fail("{c_str(str(e))}", {va:#x}u); return;']
            out.extend('    ' + b for b in body)
            L.mnemonics[insn.mnemonic] += 1
            # Fall-through out of the decoded set: the next address belongs to
            # another function (shared tail) or was never reached.
            nxt = va + insn.size
            m = insn.mnemonic
            ends = m in ('ret', 'retf', 'jmp', 'hlt', 'int3', 'ud2')
            if not ends and (i + 1 == len(order) or order[i + 1] != nxt):
                if nxt in L.entries:
                    out.append(f'    SAVE(); fn_{nxt:08X}(); return;   /* falls into the next function */')
                elif nxt in self.insns:
                    out.append(f'    goto L_{nxt:08X};')
                else:
                    out.append(f'    SAVE(); rt_fail("falls off the decoded code", {nxt:#x}u); return;')
        out.append('}')
        return '\n'.join(out)

    # operand helpers
    def addr(self, op):
        m = op.mem
        if m.segment != 0:
            raise Unsupported('segment override')
        parts = []
        if m.base:
            parts.append(R32[m.base])
        if m.index:
            parts.append(f'{R32[m.index]}*{m.scale}' if m.scale != 1 else R32[m.index])
        if m.disp or not parts:
            parts.append(f'{m.disp & 0xFFFFFFFF:#x}u')
        return '(u32)(' + ' + '.join(parts) + ')'

    def rd(self, op, size=None):
        size = size or op.size
        if op.type == X.X86_OP_IMM:
            v = op.imm & ((1 << (8 * size)) - 1) if size < 8 else op.imm & 0xFFFFFFFFFFFFFFFF
            return f'{v:#x}u'
        if op.type == X.X86_OP_REG:
            r = op.reg
            if r in R32: return R32[r]
            if r in R16: return f'(u16){R16[r]}'
            if r in R8L: return f'(u8){R8L[r]}'
            if r in R8H: return f'(u8)({R8H[r]} >> 8)'
            raise Unsupported(f'register {self.L.md.reg_name(r)}')
        if op.type == X.X86_OP_MEM:
            return f'LD{8 * size}({self.addr(op)})'
        raise Unsupported('operand')

    def wr(self, op, val, size=None):
        size = size or op.size
        if op.type == X.X86_OP_REG:
            r = op.reg
            if r in R32: return f'{R32[r]} = (u32)({val});'
            if r in R16: return f'{R16[r]} = ({R16[r]} & 0xFFFF0000u) | (u16)({val});'
            if r in R8L: return f'{R8L[r]} = ({R8L[r]} & 0xFFFFFF00u) | (u8)({val});'
            if r in R8H: return f'{R8H[r]} = ({R8H[r]} & 0xFFFF00FFu) | ((u32)(u8)({val}) << 8);'
            raise Unsupported(f'register {self.L.md.reg_name(r)}')
        if op.type == X.X86_OP_MEM:
            return f'ST{8 * size}({self.addr(op)}, ({UT[size]})({val}));'
        raise Unsupported('write operand')

    def szf(self, r, size):
        return [f'zf = ({r}) == 0; sf = (({r}) & {SIGN[size]}) != 0; pf = PARITY({r});']

    def insn_c(self, insn):
        m, ops, va = insn.mnemonic, insn.operands, insn.address
        L = self.L
        nxt = va + insn.size
        if insn.prefix[0] in (0xF2, 0xF3) and not m.startswith('rep'):
            m = ('repne ' if insn.prefix[0] == 0xF2 else 'rep ') + m
        # -- moves
        if m == 'mov':
            return [self.wr(ops[0], self.rd(ops[1], ops[0].size))]
        if m == 'movzx':
            return [self.wr(ops[0], f'(u32){self.rd(ops[1])}')]
        if m == 'movsx':
            return [self.wr(ops[0], f'(u32)(s32)({ST_T[ops[1].size]}){self.rd(ops[1])}')]
        if m == 'lea':
            return [self.wr(ops[0], self.addr(ops[1]))]
        if m == 'xchg':
            a, b = self.rd(ops[0]), self.rd(ops[1])
            return [f'{{ {UT[ops[0].size]} t_ = {a};', self.wr(ops[0], b), self.wr(ops[1], 't_'), '}']
        if m == 'push':
            v = self.rd(ops[0], 4) if ops[0].type != X.X86_OP_IMM else f'{ops[0].imm & 0xFFFFFFFF:#x}u'
            return [f'{{ u32 v_ = {v}; esp -= 4; ST32(esp, v_); }}']
        if m == 'pop':
            return [f'{{ u32 v_ = LD32(esp); esp += 4;', self.wr(ops[0], 'v_'), '}']
        if m == 'leave':
            return ['esp = ebp; ebp = LD32(esp); esp += 4;']
        if m in ('cdq',):
            return ['edx = (u32)((s32)eax >> 31);']
        if m == 'cwde':
            return ['eax = (u32)(s32)(s16)eax;']
        if m == 'cbw':
            return ['eax = (eax & 0xFFFF0000u) | (u16)(s16)(s8)eax;']
        if m == 'cwd':
            return ['edx = (edx & 0xFFFF0000u) | (((s16)eax < 0) ? 0xFFFFu : 0u);']
        if m == 'nop':
            return []
        if m == 'bswap':
            return [self.wr(ops[0], f'__builtin_bswap32({self.rd(ops[0])})')]
        if m == 'sahf':
            return ['sf = (eax >> 15) & 1; zf = (eax >> 14) & 1; pf = (eax >> 10) & 1; cf = (eax >> 8) & 1;']
        if m == 'lahf':
            return ['eax = (eax & 0xFFFF00FFu) | ((u32)((sf << 7) | (zf << 6) | (pf << 2) | 2 | cf) << 8);']
        # -- integer arithmetic
        if m in ('add', 'adc', 'sub', 'sbb', 'cmp'):
            s = ops[0].size
            a, b = self.rd(ops[0]), self.rd(ops[1], s)
            carry = ' + cf' if m in ('adc', 'sbb') else ''
            if m in ('add', 'adc'):
                body = [f'{{ u32 a_ = {a}, b_ = {b}; u64 t_ = (u64)a_ + b_{carry}; u32 r_ = (u32)t_ & {MASK[s]};',
                        f'  cf = (t_ >> {8 * s}) & 1; of = (((a_ ^ r_) & (b_ ^ r_)) & {SIGN[s]}) != 0;']
            else:
                body = [f'{{ u32 a_ = {a}, b_ = {b}; u32 r_ = (a_ - b_{carry.replace("+", "-")}) & {MASK[s]};',
                        f'  cf = (u64)a_ < (u64)b_{carry}; of = (((a_ ^ b_) & (a_ ^ r_)) & {SIGN[s]}) != 0;']
            body += ['  ' + x for x in self.szf('r_', s)]
            if m != 'cmp':
                body.append('  ' + self.wr(ops[0], 'r_'))
            return body + ['}']
        if m in ('and', 'or', 'xor', 'test'):
            s = ops[0].size
            op = {'and': '&', 'test': '&', 'or': '|', 'xor': '^'}[m]
            body = [f'{{ u32 r_ = ({self.rd(ops[0])} {op} {self.rd(ops[1], s)}) & {MASK[s]}; cf = 0; of = 0;']
            body += ['  ' + x for x in self.szf('r_', s)]
            if m != 'test':
                body.append('  ' + self.wr(ops[0], 'r_'))
            return body + ['}']
        if m in ('inc', 'dec'):
            s = ops[0].size
            d = '+' if m == 'inc' else '-'
            ovf = SIGN[s] if m == 'inc' else f'({SIGN[s]} - 1u)'
            body = [f'{{ u32 a_ = {self.rd(ops[0])}; u32 r_ = (a_ {d} 1u) & {MASK[s]}; of = a_ == {ovf};']
            return body + ['  ' + x for x in self.szf('r_', s)] + ['  ' + self.wr(ops[0], 'r_'), '}']
        if m == 'neg':
            s = ops[0].size
            body = [f'{{ u32 a_ = {self.rd(ops[0])}; u32 r_ = (0u - a_) & {MASK[s]}; cf = a_ != 0; of = a_ == {SIGN[s]};']
            return body + ['  ' + x for x in self.szf('r_', s)] + ['  ' + self.wr(ops[0], 'r_'), '}']
        if m == 'not':
            return [self.wr(ops[0], f'~{self.rd(ops[0])}')]
        if m in ('shl', 'sal', 'shr', 'sar', 'rol', 'ror'):
            s = ops[0].size
            bits = 8 * s
            cnt = self.rd(ops[1], 1) if len(ops) > 1 else '1u'
            a = self.rd(ops[0])
            body = [f'{{ u32 n_ = ({cnt}) & 31u; if (n_) {{ u32 a_ = {a} & {MASK[s]}; u32 r_;']
            if m in ('shl', 'sal'):
                body += [f'  r_ = (u32)((u64)a_ << n_) & {MASK[s]}; cf = n_ <= {bits} ? (a_ >> ({bits} - n_)) & 1 : 0;',
                         f'  of = ((r_ & {SIGN[s]}) != 0) != cf;']
            elif m == 'shr':
                body += [f'  r_ = n_ < 32 ? (a_ >> n_) : 0; cf = (a_ >> (n_ - 1)) & 1; of = (a_ & {SIGN[s]}) != 0;']
            elif m == 'sar':
                body += [f'  s32 sa_ = ({ST_T[s]})a_; r_ = (u32)(sa_ >> (n_ < {bits} ? n_ : {bits} - 1)) & {MASK[s]};',
                         f'  cf = (u32)(sa_ >> (n_ - 1 < {bits} ? n_ - 1 : {bits} - 1)) & 1; of = 0;']
            elif m == 'rol':
                body += [f'  u32 k_ = n_ % {bits}; r_ = ((a_ << k_) | (a_ >> (({bits} - k_) % {bits}))) & {MASK[s]};',
                         f'  cf = r_ & 1; of = (((r_ & {SIGN[s]}) != 0) != cf);']
            else:
                body += [f'  u32 k_ = n_ % {bits}; r_ = ((a_ >> k_) | (a_ << (({bits} - k_) % {bits}))) & {MASK[s]};',
                         f'  cf = (r_ & {SIGN[s]}) != 0; of = ((r_ >> ({bits} - 1)) ^ (r_ >> ({bits} - 2))) & 1;']
            if m not in ('rol', 'ror'):
                body += ['  ' + x for x in self.szf('r_', s)]
            return body + ['  ' + self.wr(ops[0], 'r_'), '} }']
        if m in ('shld', 'shrd'):
            if ops[0].size != 4:
                raise Unsupported(f'{m} on 16 bits')
            d, src, cnt = self.rd(ops[0]), self.rd(ops[1]), self.rd(ops[2], 1)
            if m == 'shld':
                calc = 'r_ = (d_ << n_) | (s_ >> (32 - n_)); cf = (d_ >> (32 - n_)) & 1;'
            else:
                calc = 'r_ = (d_ >> n_) | (s_ << (32 - n_)); cf = (d_ >> (n_ - 1)) & 1;'
            body = [f'{{ u32 n_ = ({cnt}) & 31u; if (n_) {{ u32 d_ = {d}, s_ = {src}, r_; {calc}',
                    '  of = ((r_ ^ d_) >> 31) & 1;']
            return body + ['  ' + x for x in self.szf('r_', 4)] + ['  ' + self.wr(ops[0], 'r_'), '} }']
        if m == 'imul':
            if len(ops) == 1:
                s = ops[0].size
                if s != 4:
                    raise Unsupported('imul r/m8, r/m16')
                return [f'{{ s64 p_ = (s64)(s32)eax * (s32){self.rd(ops[0])}; eax = (u32)p_; edx = (u32)((u64)p_ >> 32);',
                        '  cf = of = p_ != (s64)(s32)p_; }']
            a, b = (self.rd(ops[0]), self.rd(ops[1])) if len(ops) == 2 else (self.rd(ops[1]), self.rd(ops[2], ops[0].size))
            s = ops[0].size
            return [f'{{ s64 p_ = (s64)({ST_T[s]}){a} * ({ST_T[s]}){b}; cf = of = p_ != (s64)({ST_T[s]})p_;',
                    '  ' + self.wr(ops[0], '(u32)p_'), '}']
        if m == 'mul':
            s = ops[0].size
            if s == 4:
                return [f'{{ u64 p_ = (u64)eax * {self.rd(ops[0])}; eax = (u32)p_; edx = (u32)(p_ >> 32); cf = of = edx != 0; }}']
            if s == 1:
                return [f'{{ u32 p_ = (u32)(u8)eax * {self.rd(ops[0])}; eax = (eax & 0xFFFF0000u) | (u16)p_; cf = of = (p_ >> 8) != 0; }}']
            raise Unsupported('mul r/m16')
        if m in ('div', 'idiv'):
            s = ops[0].size
            if s != 4:
                raise Unsupported(f'{m} r/m{8 * s}')
            if m == 'div':
                return [f'{{ u64 n_ = ((u64)edx << 32) | eax; u32 d_ = {self.rd(ops[0])};',
                        f'  if (!d_ || n_ / d_ > 0xFFFFFFFFu) {{ SAVE(); rt_fail("#DE", {va:#x}u); return; }}',
                        '  eax = (u32)(n_ / d_); edx = (u32)(n_ % d_); }']
            return [f'{{ s64 n_ = (s64)(((u64)edx << 32) | eax); s32 d_ = (s32){self.rd(ops[0])};',
                    f'  if (!d_ || (n_ == INT64_MIN && d_ == -1) || n_ / d_ != (s32)(n_ / d_)) {{ SAVE(); rt_fail("#DE", {va:#x}u); return; }}',
                    '  eax = (u32)(s32)(n_ / d_); edx = (u32)(s32)(n_ % d_); }']
        if m.startswith('set') and m[3:] in CC:
            return [self.wr(ops[0], f'({CC[m[3:]]}) ? 1u : 0u')]
        if m.startswith('cmov') and m[4:] in CC:
            return [f'if ({CC[m[4:]]}) {{ {self.wr(ops[0], self.rd(ops[1]))} }}']
        # -- string ops, direction flag assumed clear (the ABI's)
        if m in ('rep stosd', 'rep stosb', 'stosd', 'stosb', 'rep movsd', 'rep movsb', 'movsd', 'movsb', 'rep stosw', 'rep movsw'):
            w = {'d': 4, 'b': 1, 'w': 2}[m[-1]]
            one = (f'ST{8 * w}(edi, ({UT[w]})eax); edi += {w};' if 'stos' in m
                   else f'ST{8 * w}(edi, LD{8 * w}(esi)); esi += {w}; edi += {w};')
            if m.startswith('rep'):
                return [f'while (ecx) {{ {one} --ecx; }}']
            return [one]
        # -- control flow
        if m == 'jmp':
            op = ops[0]
            if op.type == X.X86_OP_IMM:
                t = op.imm
                if t in L.entries and t != self.entry:
                    L.stats['tail calls'] += 1
                    return [f'SAVE(); fn_{t:08X}(); return;']
                return [f'goto L_{t:08X};']
            if va in self.tables:
                cases = ' '.join(f'case {t:#x}u: goto L_{t:08X};' if t not in L.entries or t == self.entry
                                 else f'case {t:#x}u: SAVE(); fn_{t:08X}(); return;' for t in self.tables[va])
                return [f'switch ({self.rd(op, 4)}) {{ {cases}',
                        f'  default: SAVE(); rt_fail("jump table target not decoded", {va:#x}u); return; }}']
            L.stats['indirect tail jumps'] += 1
            return [f'{{ u32 t_ = {self.rd(op, 4)}; SAVE(); rt_dispatch(t_); return; }}']
        if m.startswith('j') and m[1:] in CC:
            t = ops[0].imm
            if t in L.entries and t != self.entry:
                L.stats['conditional tail calls'] += 1
                return [f'if ({CC[m[1:]]}) {{ SAVE(); fn_{t:08X}(); return; }}']
            return [f'if ({CC[m[1:]]}) goto L_{t:08X};']
        if m in ('jecxz', 'jcxz'):
            return [f'if (!ecx) goto L_{ops[0].imm:08X};']
        if m == 'loop':
            return [f'if (--ecx) goto L_{ops[0].imm:08X};']
        if m == 'call':
            op = ops[0]
            pre = f'esp -= 4; ST32(esp, {nxt:#x}u); SAVE();'
            if op.type == X.X86_OP_IMM:
                t = op.imm
                if t == nxt:
                    return [f'esp -= 4; ST32(esp, {nxt:#x}u);   /* call $+5: pushes its own address */']
                L.calls.add(t)
                return [f'{pre} fn_{t:08X}(); LOAD();']
            if op.type == X.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0 \
                    and L.img.iat[0] <= (op.mem.disp & 0xFFFFFFFF) < L.img.iat[1]:
                raise Unsupported(f'import call through {op.mem.disp & 0xFFFFFFFF:#x}')
            L.stats['indirect calls'] += 1
            return [f'{{ u32 t_ = {self.rd(op, 4)}; {pre} rt_dispatch(t_); LOAD(); }}']
        if m == 'ret':
            n = ops[0].imm if ops else 0
            return [f'esp += {4 + n}u; SAVE(); return;']
        if m in ('int3', 'hlt', 'ud2'):
            return [f'SAVE(); rt_fail("{m}", {va:#x}u); return;']
        # -- x87
        if m.startswith('f'):
            return self.x87(insn)
        raise Unsupported(f'no semantics for {m}')

    def x87(self, insn):
        m, ops = insn.mnemonic, insn.operands
        sts = [ST[o.reg] for o in ops if o.type == X.X86_OP_REG and o.reg in ST]
        mem = next((o for o in ops if o.type == X.X86_OP_MEM), None)

        def mload(o, integer=False):
            a = self.addr(o)
            if integer:
                return {2: f'(double)(s16)LD16({a})', 4: f'(double)(s32)LD32({a})', 8: f'(double)(s64)LD64({a})'}[o.size]
            return {4: f'(double)LDF32({a})', 8: f'LDF64({a})'}.get(o.size) or self.unsupported_x87(m, o)

        if m in ('fld', 'fild'):
            if mem:
                return [f'{{ double v_ = {mload(mem, m == "fild")}; FPUSH(v_); }}']
            return [f'{{ double v_ = FST({sts[0]}); FPUSH(v_); }}']
        if m in ('fld1', 'fldz', 'fldpi'):
            v = {'fld1': '1.0', 'fldz': '0.0', 'fldpi': '3.141592653589793'}[m]
            return [f'FPUSH({v});']
        if m in ('fst', 'fstp'):
            pop = ' FPOP();' if m == 'fstp' else ''
            if mem:
                if mem.size == 4: return [f'STF32({self.addr(mem)}, FST(0));{pop}']
                if mem.size == 8: return [f'STF64({self.addr(mem)}, FST(0));{pop}']
                self.unsupported_x87(m, mem)
            return [f'FST({sts[0]}) = FST(0);{pop}']
        if m in ('fist', 'fistp'):
            pop = ' FPOP();' if m == 'fistp' else ''
            s = mem.size
            return [f'ST{8 * s}({self.addr(mem)}, ({UT[s]})rt_fist(FST(0), {8 * s}));{pop}']
        arith = {'fadd': '+', 'fsub': '-', 'fsubr': 'r-', 'fmul': '*', 'fdiv': '/', 'fdivr': 'r/'}
        base = m[:-1] if m.endswith('p') and m[:-1] in arith else m
        ibase = base[2:] if base.startswith('fi') and 'f' + base[2:] in arith else None
        if base in arith or ibase:
            op = arith[base] if base in arith else arith['f' + ibase]
            rev = op.startswith('r')
            op = op[-1]

            def f(dst, src):   # dst = dst op src, or src op dst for the reversed forms
                return f'{dst} = {src} {op} {dst};' if rev else f'{dst} = {dst} {op} {src};'
            if mem:
                return [f'{{ double m_ = {mload(mem, bool(ibase))}; {f("FST(0)", "m_")} }}']
            if m.endswith('p'):
                i = sts[0] if sts else 1
                return [f'{{ {f(f"FST({i})", "FST(0)")} FPOP(); }}']
            if len(sts) == 2:
                d, s_ = sts
                return [f'{{ {f(f"FST({d})", f"FST({s_})")} }}']
            return [f'{{ {f("FST(0)", f"FST({sts[0]})")} }}']
        if m == 'fchs':
            return ['FST(0) = -FST(0);']
        if m == 'fabs':
            return ['FST(0) = __builtin_fabs(FST(0));']
        if m == 'fsqrt':
            return ['FST(0) = __builtin_sqrt(FST(0));']
        if m == 'frndint':
            return ['FST(0) = rt_frndint(FST(0));']
        if m == 'fxch':
            i = sts[-1] if sts else 1
            return [f'{{ double t_ = FST(0); FST(0) = FST({i}); FST({i}) = t_; }}']
        if m in ('fcom', 'fcomp', 'fcompp', 'fucom', 'fucomp', 'fucompp', 'ficom', 'ficomp', 'ftst'):
            pops = 2 if m.endswith('pp') else 1 if m.endswith('p') else 0
            if m == 'ftst':
                src = '0.0'
            elif mem:
                src = mload(mem, m.startswith('fi'))
            else:
                src = f'FST({sts[-1] if sts else 1})'
            return [f'FCOM(FST(0), {src});' + ' FPOP();' * pops]
        if m in ('fcomi', 'fcomip', 'fucomi', 'fucomip'):
            i = sts[-1]
            pop = ' FPOP();' if m.endswith('p') else ''
            return [f'{{ double a_ = FST(0), b_ = FST({i}); int u_ = !(a_ == a_ && b_ == b_);',
                    '  zf = u_ || a_ == b_; pf = u_; cf = u_ || a_ < b_; of = sf = 0; }' + pop]
        if m == 'fnstsw':
            if mem:
                return [f'ST16({self.addr(mem)}, FNSTSW());']
            return ['eax = (eax & 0xFFFF0000u) | FNSTSW();']
        if m == 'fnstcw':
            return [f'ST16({self.addr(mem)}, R.cw);']
        if m == 'fldcw':
            return [f'rt_fldcw(LD16({self.addr(mem)}));']
        if m in ('fwait', 'wait'):
            return []
        raise Unsupported(f'no semantics for {m}')

    def unsupported_x87(self, m, o):
        raise Unsupported(f'{m} on a {o.size}-byte operand (80-bit x87 values)')


def c_str(s):
    return s.replace('\\', '\\\\').replace('"', "'")


class Lifter:
    def __init__(self, img, entries):
        self.img = img
        self.entries = entries                 # va -> name
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.stats = collections.Counter()
        self.mnemonics = collections.Counter()
        self.calls = set()

    def run(self):
        funcs, bodies, failed = [], [], {}
        for va in sorted(self.entries):
            f = Func(self, va, self.entries[va])
            try:
                f.decode()
            except Unsupported as e:
                failed[va] = str(e)
                continue
            bodies.append(f.emit())
            funcs.append(f)
        lifted = {f.entry for f in funcs}
        missing = sorted(self.calls - lifted)
        out = ['// GENERATED by tools/lift/lift_x86.py. Do not edit; do not commit if lifted from BOF3.exe.',
               '#include <stdint.h>',
               '#include "lift_rt.h"',
               '#define SAVE() (' + ', '.join(f'R.{r} = {r}' for r in REGS) + ')',
               '#define LOAD() (' + ', '.join(f'{r} = R.{r}' for r in REGS) + ')',
               '']
        out += [f'void fn_{va:08X}(void);' for va in sorted(lifted)]
        # A direct call to a function that was not lifted still has to link.
        for va in missing:
            out.append(f'static void fn_{va:08X}(void) {{ rt_fail("call to a function that was not lifted", {va:#x}u); }}')
        out.append('')
        out += bodies
        out.append('')
        out.append('typedef struct { u32 addr; void (*fn)(void); } Entry;')
        out.append('const Entry lift_table[] = {')
        out += [f'    {{{va:#x}u, fn_{va:08X}}},' for va in sorted(lifted)]
        out.append('};')
        out.append(f'const u32 lift_table_n = {len(lifted)};')
        return '\n'.join(out) + '\n', funcs, failed, missing


def read_entries(path):
    entries = {}
    for line in open(path):
        line = line.split('#')[0].strip()
        if not line:
            continue
        parts = line.split()
        va = int(parts[0], 16)
        entries[va] = parts[1] if len(parts) > 1 else f'sub_{va:X}'
    return entries


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--pe', required=True)
    ap.add_argument('--entries', required=True, help='hex address [name] per line')
    ap.add_argument('--out', required=True)
    a = ap.parse_args()

    img = Image(a.pe)
    L = Lifter(img, read_entries(a.entries))
    text, funcs, failed, missing = L.run()
    with open(a.out, 'w', newline='\n') as f:
        f.write(text)
    n_insn = sum(len(f.insns) for f in funcs)
    print(f'lift_x86: {len(funcs)} of {len(L.entries)} functions lifted, {n_insn} instructions, '
          f'{text.count(chr(10))} lines of C -> {a.out}')
    for k, v in sorted(L.stats.items()):
        print(f'  {k}: {v}')
    for va, why in sorted(failed.items()):
        print(f'  NOT LIFTED {va:#x} {L.entries[va]}: {why}')
    for f in funcs:
        for va, why in f.fails:
            print(f'  rt_fail in {f.name} at {va:#x}: {why}')
    if missing:
        print(f'  called but not in the entry list: {", ".join(f"{v:#x}" for v in missing)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
