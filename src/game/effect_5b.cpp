// Round thirteen group E5B (docs/effect_5b.md): the 48 functions of
// analysis/round13_cut.tsv's group E5B and two starts the cut does not list
// (sub-kind 0x13's states 1 and 3, 0x5003A0 and 0x5004A0), each read with
// capstone to its last instruction. Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps by +1 through
// EffectKind18_States, whose entries 11..15, 19 and 79 are the dispatchers here,
// each by +2 through its own table (none bounded by a compare). What each
// sub-kind is, as far as the code says:
//
//   0x0D  four VRAM columns animated by Gpu_SetDrawMove and a glow over a cell
//         (story flag 0xD for column 0, area 48's count 0x92BEE7 for the rest;
//         sound 0x20D)
//   0x0B  eighteen ground tiles that rise with story flag 0x11 and fade out,
//         leaving 0x9039F4 = 4 (sound 0x202)
//   0x0C  sub-kind 0x0D's columns again (other VRAM, story flags by column, no
//         glow; sound 0x201)
//   0x0E  a gate at a fixed place (cells (1, 21), (1, 22)) by row 9's flag 0xC,
//         lowered and raised as the leader comes and goes (sounds 0x200, 0x201)
//   0x13  a wall panel of four variants by row 9's flags and a flickering line
//   0x0F  gates of eight variants (+0x36), lowered by Cond_ByteFE or the
//         leader, their cells 0x52 / 0xA1 in AreaMap_Bytes
//   0x4F  the same gates for the other axis, opened by Cond_ByteFE only
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. No divergence: each is a
// faithful replacement. Where the original jumps through a state table past its
// end or reads one of its byte arrays past its extent by a byte of the record,
// ours aborts with a message (docs/effect_5b.md section 7).
#include "game/effect_5b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5b_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5b::at;
using U = std::uint32_t;
using I = std::int32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<I>(v)); }
// movsx word
I SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
unsigned char& B(U a) { return *At(a); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

using Handler = scenario_harness::Handler;

// Prim_VertexScratch's four SVECTORs (0x9037A0, A8, B0, B8): word k of vertex v.
constexpr U kScratch = 0x9037A0;
U VW(unsigned v, unsigned k) { return Word(At(kScratch + 8 * v + 2 * k)); }
I VS(unsigned v, unsigned k) { return static_cast<std::int16_t>(VW(v, k)); }
void SetV(unsigned v, unsigned k, U value) { SetWord(At(kScratch + 8 * v + 2 * k), value); }
const short* Vertex(unsigned v) { return reinterpret_cast<const short*>(At(kScratch + 8 * v)); }

// cdq; xor eax, edx; sub eax, edx (INT_MIN stays itself)
I Abs(I v) {
    const U sign = static_cast<U>(v >> 31);
    return static_cast<I>((static_cast<U>(v) ^ sign) - sign);
}
// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: an elevation's low word halved
// toward zero
I Half(long e) {
    const I w = static_cast<std::int16_t>(static_cast<U>(e));
    return (w - (w >> 31)) >> 1;
}
// cdq; sub eax, edx; sar eax, 1 on a whole dword
I Halve(I v) { return static_cast<I>(static_cast<U>(v) - static_cast<U>(v >> 31)) >> 1; }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, a data dword or a null.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5b.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// One of the image's byte arrays by an index the record supplies (unchecked by
// the original, which reads the next array or table past its extent).
unsigned Byte(const char* who, U array, unsigned extent, U index) {
    if (index >= extent)
        bof3::Fatal("%s: index %d past the %u bytes of 0x%X - the original reads what follows "
                    "(docs/effect_5b.md section 7)",
                    who, static_cast<int>(index), extent, (unsigned)array);
    return B(array + index);
}

// --- the CRT's _ftol 0x5B9550 on st(0) (truncation set in a copy of the control
// word for one fistp to 64 bits, the word put back), the low dword -------------

// fld dword [p]; call _ftol
U FldFtol(const void* p) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[p])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [p] "r"(p)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
// fild dword [i]; fadd dword [p]; call _ftol (the original's `fild; fld st(0);
// fadd [p]` and `fild; fadd [p]`)
U FiAddFtol(I i, const void* p) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "fildl %[i]\n\t"
        "fadds (%[p])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [i] "m"(i), [p] "r"(p)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
// fld dword [p]; fisub dword [i]; call _ftol (the original's `fld [p]; fsub
// st(1)` over a fild'ed i, and `fild i; fsubr [p]`: p - i, one rounding)
U FldFiSubFtol(const void* p, I i) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[p])\n\t"
        "fisubl %[i]\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [i] "m"(i), [p] "r"(p)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
// fld dword [from]; fst dword [a]; fstp dword [b]: a copy through the FPU (a
// signalling NaN comes out quiet, as in the original)
void FpuCopy2(const void* from, void* a, void* b) {
    __asm__ volatile(
        "flds (%[from])\n\t"
        "fsts (%[a])\n\t"
        "fstps (%[b])\n\t"
        :
        : [from] "r"(from), [a] "r"(a), [b] "r"(b)
        : "st", "memory");
}
// fild dword (a sign-extended word); fstp dword
void StoreFloat(unsigned char* to, I v) {
    const float f = static_cast<float>(v);
    std::memcpy(to, &f, 4);
}
float F(U a) {
    float f;
    std::memcpy(&f, At(a), 4);
    return f;
}

unsigned char* Prim() { return Gfx_PacketNext; }
unsigned char* Flags() { return At(at::kStoryFlags); }
unsigned char* Row9() { return At(at::kCondRow9); }

// The four-vertex projection every quad here makes of Prim_VertexScratch into a
// primitive whose vertices are `stride` apart from +8 (0x10 textured, 0xC flat).
void Project4(unsigned char* prim, unsigned stride) {
    long p;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(prim + 8),
                               reinterpret_cast<float*>(prim + 8 + stride), reinterpret_cast<float*>(prim + 8 + 2 * stride),
                               reinterpret_cast<float*>(prim + 8 + 3 * stride), &p);
}

// AreaMap_Bytes + z * width + x (width the area header's byte 0), as the
// original's 32-bit sums: no bound, as Capcom's.
unsigned char* Cell(I x, I z) {
    const U width = AreaMap_Header[0];
    return const_cast<unsigned char*>(AreaMap_Bytes) + (static_cast<U>(z) * width + static_cast<U>(x));
}

// The record's cell words (+0x36 x, +0x3A z) as 16.16 against the leader's point:
// with +8 set the gate lies along x, so the leader's z against the centre of
// the row after the gate's (within `across`) and its x against the gate's
// first or second cell (within `along`); with +8 clear the axes swap.
bool GateNearXZ(I x, I z, bool along_x, I across, I along) {
    if (along_x) {
        const U row = (static_cast<U>(z + 1) << 16) | 0x8000u;
        if (Abs(static_cast<I>(UL(At(at::kLeaderZ)) - row)) > across) return false;
        const I d = static_cast<I>(UL(At(at::kLeaderX)) - (static_cast<U>(x) << 16));
        return Abs(d) <= along || Abs(static_cast<I>(static_cast<U>(d) - 0x10000u)) <= along;
    }
    const U column = (static_cast<U>(x + 1) << 16) | 0x8000u;
    if (Abs(static_cast<I>(UL(At(at::kLeaderX)) - column)) > across) return false;
    const I d = static_cast<I>(UL(At(at::kLeaderZ)) - (static_cast<U>(z) << 16));
    return Abs(d) <= along || Abs(static_cast<I>(static_cast<U>(d) - 0x10000u)) <= along;
}
bool GateNear(const unsigned char* s, I across, I along) {
    return GateNearXZ(SW(s + 0x36), SW(s + 0x3A), s[8] != 0, across, along);
}

// Sub-kind 0x0E's fixed gate: the leader within `x` of 0x28000 and within `z`
// of 0x150000 or 0x160000.
bool Sub0ENear(I x, I z) {
    const U lx = UL(At(at::kLeaderX)), lz = UL(At(at::kLeaderZ));
    if (Abs(static_cast<I>(lx - 0x28000u)) > x) return false;
    return Abs(static_cast<I>(lz - 0x150000u)) <= z || Abs(static_cast<I>(lz - 0x160000u)) <= z;
}

// Sub-kind 0x0E's two cells (1, 21) and (1, 22): AreaMap_Bytes + 21w + 1 and + 22w + 1.
void Sub0ECells(unsigned char value) {
    *Cell(1, 21) = value;
    *Cell(1, 22) = value;
}

// The gate's two cells: (x, z) and, with +8 set, (x + 1, z), else (x, z + 1)
// (the original adds the byte +8 to x and +8 == 0 to z).
void GateCells(unsigned char value) {
    unsigned char* s = S();
    *Cell(SW(s + 0x36), SW(s + 0x3A)) = value;
    s = S();
    const unsigned b8 = s[8];
    *Cell(SW(s + 0x36) + static_cast<I>(b8), SW(s + 0x3A) + (b8 == 0 ? 1 : 0)) = value;
}

// Sub-kind 0x4F's two cells: (x, z) and (x, z + 1).
void Gate4FCells(unsigned char value) {
    unsigned char* s = S();
    *Cell(SW(s + 0x36), SW(s + 0x3A)) = value;
    s = S();
    *Cell(SW(s + 0x36), SW(s + 0x3A) + 1) = value;
}

// A VRAM move of an 8 x 0x78 column: the rect at (frame + 1) * 8 + `from`,
// 0x160, onto (`to`, 0x160); committed to slot 6.
void MoveColumn(U from, U to) {
    unsigned char rect[8];
    SetWord(rect, from);
    SetWord(rect + 2, 0x160);
    SetWord(rect + 4, 8);
    SetWord(rect + 6, 0x78);
    SH_CALL(Gpu_SetDrawMove)(Prim(), rect, to, 0x160);
    SH_CALL(Gfx_CommitPrim)(6, 0x18);
}

// +9 up one; when past `last`, +9 0 (when `clear`) and +2 to `next` (or up one
// when next is negative).
void Step(unsigned last, bool clear, int next) {
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (S()[9] <= last) return;
    if (clear) S()[9] = 0;
    if (next < 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    else S()[2] = static_cast<unsigned char>(next);
}

void Gate0FPrepare(const char* who, bool sub4F);

}  // namespace

// ===========================================================================
// Sub-kind 0x0D (EffectKind18_States[13])
// ===========================================================================

// original 0x4FF320 (EffectKind18_States[13], hidden in E5A's 0x4FF150): jmp
// [EffectKind18Sub0D_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub0D_Run(void) {
    Dispatch("EffectKind18Sub0D_Run", AddressOf(EffectKind18Sub0D_States), EffectKind18Sub0D_States_count);
}

// original 0x4FF340 (state 0): +9 0; column 0 with story flag 0xD set: frame 3
// moved and +2 3; else the column's frame 0 and +2 1.
extern "C" void __cdecl EffectKind18Sub0D_Start(void) {
    S()[9] = 0;
    if (Word(S() + 0x36) == 0 && SH_CALL(Flags_Test)(Flags(), 0xD)) {
        SH_CALL(EffectKind18Sub0D_MoveFrame)(0, 3);
        S()[2] = 3;
        return;
    }
    SH_CALL(EffectKind18Sub0D_MoveFrame)(SW(S() + 0x36), 0);
    S()[2] = 1;
}

// original 0x4FF3A0 (state 1): column 0 with story flag 0xD, or another column
// with area 48's count not 0: sound 0x20D, +9 0, +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub0D_Wait(void) {
    // +0x36 read again after the test, as the original reads Sprite_Current again
    const bool lit = (Word(S() + 0x36) == 0 && SH_CALL(Flags_Test)(Flags(), 0xD) != 0) ||
                     (Word(S() + 0x36) != 0 && B(at::kCount48) != 0);
    if (!lit) return;
    SH_CALL(Sound_PlayEffect)(0x20D);
    S()[9] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4FF400 (state 2): the frame _OpenFrames[+9 >> 1] moved; +9 up, past
// 4: +9 0 and +2 up; the glow.
extern "C" void __cdecl EffectKind18Sub0D_Open(void) {
    unsigned char* const s = S();
    const unsigned frame = Byte("EffectKind18Sub0D_Open", at::kSub0DOpenFrames, at::kSub0DArray, s[9] >> 1);
    SH_CALL(EffectKind18Sub0D_MoveFrame)(SW(s + 0x36), frame);
    Step(4, true, -1);
    SH_CALL(EffectKind18Sub0D_DrawGlow)(SW(S() + 0x36));
}

// original 0x4FF460 (state 3): the light stays while its condition holds
// (column 0: story flag 0xD; another: area 48's count not 0), else +9 0 and +2
// up; +9 up, past 0x40: +9 0 and, on an odd Rand, +2 5; the glow.
extern "C" void __cdecl EffectKind18Sub0D_Idle(void) {
    bool off;
    if (Word(S() + 0x36) == 0) {
        const unsigned char set = SH_CALL(Flags_Test)(Flags(), 0xD);
        if (set == 0) off = true;
        else if (Word(S() + 0x36) == 0) off = false;
        else off = B(at::kCount48) == 0;
    } else {
        off = B(at::kCount48) == 0;
    }
    if (off) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (S()[9] > 0x40) {
        S()[9] = 0;
        if (SH_CALL(Rand)() & 1) S()[2] = 5;
    }
    SH_CALL(EffectKind18Sub0D_DrawGlow)(SW(S() + 0x36));
}

// original 0x4FF4F0 (state 4): the frame _ShutFrames[+9 / 3] moved; +9 up, past
// 3: +2 1. No glow.
extern "C" void __cdecl EffectKind18Sub0D_Shut(void) {
    unsigned char* const s = S();
    const unsigned frame = Byte("EffectKind18Sub0D_Shut", at::kSub0DShutFrames, at::kSub0DArray, s[9] / 3u);
    SH_CALL(EffectKind18Sub0D_MoveFrame)(SW(s + 0x36), frame);
    Step(3, false, 1);
}

// original 0x4FF540 (state 5): the frame _FlashFrames[+9 >> 1] moved; +9 up,
// past 6: +9 0 and +2 3; the glow.
extern "C" void __cdecl EffectKind18Sub0D_Flash(void) {
    unsigned char* const s = S();
    const unsigned frame = Byte("EffectKind18Sub0D_Flash", at::kSub0DFlashFrames, at::kSub0DArray, s[9] >> 1);
    SH_CALL(EffectKind18Sub0D_MoveFrame)(SW(s + 0x36), frame);
    Step(6, true, 3);
    SH_CALL(EffectKind18Sub0D_DrawGlow)(SW(S() + 0x36));
}

// original 0x4FF5A0 (column, frame): Gpu_SetDrawMove of the rect ((frame + 1) *
// 8 + column * 64 + 0x240, 0x160, 8, 0x78) onto (column * 64 + 0x240, 0x160),
// committed to slot 6 (0x18 bytes). Each product through the original's
// halving of a doubled value (exact).
extern "C" void __cdecl EffectKind18Sub0D_MoveFrame(int column, unsigned frame) {
    const U left = static_cast<U>(Halve(static_cast<I>(static_cast<U>(column) << 7)));
    const U offset = static_cast<U>(Halve(static_cast<I>((frame + 1) << 4)));
    MoveColumn(offset + left + 0x240, left + 0x240);
}

// original 0x4FF610 (column): the column's point (_GlowX / Z - 0x80 cells, y
// -_GlowY * 8) through Gte_RotTransPers into MapView_ScreenXY; culled unless
// -60 <= x <= 380, -150 <= y <= 300 (.rdata, fcomp) and the depth not 0. Then
// a draw mode (tpage 0xB5, dtd 1) linked at the cell; a diamond about the
// point in Prim_VertexScratch, its half-widths _GlowSize * 1125 / depth across
// and (|sin(Camera_Angles) * size >> 13| + size) * 1125 / depth up and down
// (x87, _ftol); four semi-transparent POLY_G3 fans from the centre (colour
// 0x50 on column 0's red and blue, (area 48's count + 1) * column << 4 on red
// and green; the rim black), each linked (0x34); a closing draw mode (dtd 0)
// linked.
extern "C" void __cdecl EffectKind18Sub0D_DrawGlow(int column) {
    const char* const who = "EffectKind18Sub0D_DrawGlow";
    const U i = static_cast<U>(column);
    const unsigned gx = Byte(who, at::kSub0DGlowX, at::kSub0DArray, i);
    const unsigned gz = B(at::kSub0DGlowZ + i);
    const unsigned gy = B(at::kSub0DGlowY + i);
    SetV(0, 0, (gx - 0x80u) << 7);
    SetV(0, 1, (gz - 0x80u) << 7);
    SetV(0, 2, (0u - gy) << 3);
    const U x = static_cast<U>(gx) << 16;
    const U z = static_cast<U>(gz) << 16;
    long p;
    const long depth = SH_CALL(Gte_RotTransPers)(Vertex(0), reinterpret_cast<unsigned long*>(MapView_ScreenXY), &p);
    const unsigned char* const sx = At(0x903820);
    const unsigned char* const sy = At(0x903824);
    if (!(F(0x903820) >= F(at::kCullLeft))) return;
    if (F(0x903820) > F(at::kCullRight)) return;
    if (!(F(0x903824) >= F(at::kCullTop))) return;
    if (F(0x903824) > F(at::kCullBottom)) return;
    if (depth == 0) return;
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 1, 0xB5, 0);
    SH_CALL(MapView_LinkPrimAt)(x, z, 1, 0xC);
    const U cx = FldFtol(sx);
    SetV(2, 0, cx);
    SetV(0, 0, cx);
    const I d = static_cast<I>(depth);
    const I across = static_cast<I>(B(at::kSub0DGlowSize + i) * 1125u) / d;
    SetV(1, 0, FiAddFtol(across, sx));
    SetV(3, 0, FldFiSubFtol(sx, across));
    const U cy = FldFtol(sy);
    SetV(3, 1, cy);
    SetV(1, 1, cy);
    {
        const I angle = Camera_Angles[0];
        const U size = B(at::kSub0DGlowSize + i);
        const I sine = SH_CALL(Math_Sin)(angle);
        const U reach = static_cast<U>(Abs(static_cast<I>(static_cast<U>(sine) * size) >> 13)) + size;
        const I down = static_cast<I>(reach * 1125u) / d;
        SetV(2, 1, FiAddFtol(down, sy));
    }
    {
        const I angle = Camera_Angles[0];
        const U size = B(at::kSub0DGlowSize + i);
        const I sine = SH_CALL(Math_Sin)(angle);
        const U reach = static_cast<U>(Abs(static_cast<I>(static_cast<U>(sine) * size) >> 13)) + size;
        const I up = static_cast<I>(reach * 1125u) / d;
        SetV(0, 1, FldFiSubFtol(sy, up));
    }
    const unsigned char centre = i == 0 ? 0x50 : 0;
    for (unsigned k = 1; k < 5; ++k) {
        unsigned char* const prim = Prim();
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x10));
        FpuCopy2(prim + 0x10, prim + 0x30, prim + 0x20);
        std::memcpy(prim + 8, sx, 4);
        std::memcpy(prim + 0xC, sy, 4);
        const unsigned from = k - 1, to = k & 3;
        StoreFloat(prim + 0x18, VS(from, 0));
        StoreFloat(prim + 0x1C, VS(from, 1));
        StoreFloat(prim + 0x28, VS(to, 0));
        StoreFloat(prim + 0x2C, VS(to, 1));
        const auto green = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(B(at::kCount48) + 1) * static_cast<unsigned char>(i)) << 4);
        prim[4] = static_cast<unsigned char>(green | centre);
        prim[6] = centre;
        prim[5] = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(B(at::kCount48) + 1) * static_cast<unsigned char>(i)) << 4);
        prim[0x26] = 0;
        prim[0x25] = 0;
        prim[0x24] = 0;
        prim[0x16] = 0;
        prim[0x15] = 0;
        prim[0x14] = 0;
        SH_CALL(MapView_LinkPrimAt)(x, z, 1, 0x34);
    }
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 0, 0xB5, 0);
    SH_CALL(MapView_LinkPrimAt)(x, z, 1, 0xC);
}

// ===========================================================================
// Sub-kind 0x0B (EffectKind18_States[11])
// ===========================================================================

// original 0x4FF970 (EffectKind18_States[11], hidden in 0x4FF610): jmp
// [EffectKind18Sub0B_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub0B_Run(void) {
    Dispatch("EffectKind18Sub0B_Run", AddressOf(EffectKind18Sub0B_States), EffectKind18Sub0B_States_count);
}

// original 0x4FF990 (state 0): story flag 0x11 set: sound 0x202, the height
// +0x3C -0x180, the colour +0xB 0x80, +9 0, +2 up and the tiles drawn at -0x180.
extern "C" void __cdecl EffectKind18Sub0B_Start(void) {
    if (!SH_CALL(Flags_Test)(Flags(), 0x11)) return;
    SH_CALL(Sound_PlayEffect)(0x202);
    SetUL(S() + 0x3C, 0xFFFFFE80u);
    S()[0xB] = 0x80;
    S()[9] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub0B_DrawTiles)(-0x180, 0x800000);
}

// original 0x4FF9F0 (state 1): the tiles at +0x3C (colour 0x80); +0x3C up 4;
// above -0x100: +2 up.
extern "C" void __cdecl EffectKind18Sub0B_Rise(void) {
    SH_CALL(EffectKind18Sub0B_DrawTiles)(Long(S() + 0x3C), 0x800000);
    unsigned char* const s = S();
    SetUL(s + 0x3C, UL(s + 0x3C) + 4);
    if (Long(S() + 0x3C) > -0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4FFA30 (state 2): the tiles at +0x3C, colour (+0xB & 0xF8) << 16;
// +0x3C up 4, +0xB down 4; above -0x80: 0x9039F4 = 4 and a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind18Sub0B_Fade(void) {
    unsigned char* s = S();
    const U colour = static_cast<U>(s[0xB] & 0xF8u) << 16;
    SH_CALL(EffectKind18Sub0B_DrawTiles)(Long(s + 0x3C), colour);
    s = S();
    SetUL(s + 0x3C, UL(s + 0x3C) + 4);
    S()[0xB] = static_cast<unsigned char>(S()[0xB] - 4);
    if (Long(S() + 0x3C) <= -0x80) return;
    B(at::kTailState) = 4;
    SH_CALL(Effect_Release)();
}

// original 0x4FFA90 (height, colour): the eighteen tiles of _Tiles (cell x, cell
// z, row dy): each a POLY_FT4 a cell square (x * 0x80 - 0x40C0 .. - 0x3FC0, the
// same in z, y the low word of height), Gte_RotTransPers4, Gte_PrimDepths4_10,
// Prim_SetTexture(colour | 0xB600B100, prim, 1), linked at the cell with its dy
// (0x48).
extern "C" void __cdecl EffectKind18Sub0B_DrawTiles(int height, unsigned long colour) {
    const U texture = static_cast<U>(colour) | 0xB600B100u;
    const U y = static_cast<U>(height);
    for (U t = at::kSub0BTiles + 1; static_cast<I>(t) < static_cast<I>(at::kSub0BTilesEnd); t += 3) {
        unsigned char* const prim = Prim();
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        const U x = static_cast<U>(B(t - 1)) << 7;
        SetV(3, 2, y);
        SetV(2, 2, y);
        SetV(2, 0, x - 0x40C0);
        SetV(0, 0, x - 0x40C0);
        const U z = static_cast<U>(B(t)) << 7;
        SetV(3, 0, x - 0x3FC0);
        SetV(1, 0, x - 0x3FC0);
        SetV(3, 1, z - 0x3FC0);
        SetV(2, 1, z - 0x3FC0);
        SetV(1, 1, z - 0x40C0);
        SetV(0, 1, z - 0x40C0);
        SetV(1, 2, y);
        SetV(0, 2, y);
        Project4(prim, 0x10);
        SH_CALL(Gte_PrimDepths4_10)(prim);
        SH_CALL(Prim_SetTexture)(texture, prim, 1);
        SH_CALL(MapView_LinkPrimAt)(static_cast<U>(B(t - 1)) << 16, static_cast<U>(B(t)) << 16, B(t + 1), 0x48);
    }
}

// ===========================================================================
// Sub-kind 0x0C (EffectKind18_States[12])
// ===========================================================================

namespace {
unsigned Sub0CFlag(const char* who) {
    return Byte(who, at::kSub0CFlags, at::kSub0CFlagCount, static_cast<U>(SW(S() + 0x36)));
}
}  // namespace

// original 0x4FFBB0 (EffectKind18_States[12], hidden in 0x4FFA90): jmp
// [EffectKind18Sub0C_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub0C_Run(void) {
    Dispatch("EffectKind18Sub0C_Run", AddressOf(EffectKind18Sub0C_States), EffectKind18Sub0C_States_count);
}

// original 0x4FFBD0 (state 0): +9 0; the column's story flag (_Flags[+0x36])
// set: frame 3 and +2 3; clear: frame 0 and +2 1.
extern "C" void __cdecl EffectKind18Sub0C_Start(void) {
    S()[9] = 0;
    if (SH_CALL(Flags_Test)(Flags(), Sub0CFlag("EffectKind18Sub0C_Start"))) {
        SH_CALL(EffectKind18Sub0C_MoveFrame)(3);
        S()[2] = 3;
        return;
    }
    SH_CALL(EffectKind18Sub0C_MoveFrame)(0);
    S()[2] = 1;
}

// original 0x4FFC30 (state 1): the column's flag set: sound 0x201, +9 0, +2 up.
extern "C" void __cdecl EffectKind18Sub0C_Wait(void) {
    if (!SH_CALL(Flags_Test)(Flags(), Sub0CFlag("EffectKind18Sub0C_Wait"))) return;
    SH_CALL(Sound_PlayEffect)(0x201);
    S()[9] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4FFC70 (state 2): the frame _OpenFrames[+9 >> 1]; +9 up, past 4:
// +9 0 and +2 up.
extern "C" void __cdecl EffectKind18Sub0C_Open(void) {
    SH_CALL(EffectKind18Sub0C_MoveFrame)(
        Byte("EffectKind18Sub0C_Open", at::kSub0COpenFrames, at::kSub0DArray, S()[9] >> 1));
    Step(4, true, -1);
}

// original 0x4FFCC0 (state 3): the flag clear: +9 0 and +2 up; +9 up, past 0x40:
// +9 0 and, on an odd Rand, +2 5.
extern "C" void __cdecl EffectKind18Sub0C_Idle(void) {
    if (!SH_CALL(Flags_Test)(Flags(), Sub0CFlag("EffectKind18Sub0C_Idle"))) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (S()[9] <= 0x40) return;
    S()[9] = 0;
    if (SH_CALL(Rand)() & 1) S()[2] = 5;
}

// original 0x4FFD30 (state 4): the frame _ShutFrames[+9 / 3]; +9 up, past 3: +2 1.
extern "C" void __cdecl EffectKind18Sub0C_Shut(void) {
    SH_CALL(EffectKind18Sub0C_MoveFrame)(
        Byte("EffectKind18Sub0C_Shut", at::kSub0CShutFrames, at::kSub0DArray, S()[9] / 3u));
    Step(3, false, 1);
}

// original 0x4FFD80 (state 5): the frame _FlashFrames[+9 >> 1]; +9 up, past 6:
// +9 0 and +2 3.
extern "C" void __cdecl EffectKind18Sub0C_Flash(void) {
    SH_CALL(EffectKind18Sub0C_MoveFrame)(
        Byte("EffectKind18Sub0C_Flash", at::kSub0CFlashFrames, at::kSub0DArray, S()[9] >> 1));
    Step(6, true, 3);
}

// original 0x4FFDD0 (frame): Gpu_SetDrawMove of the rect ((frame + 1) * 8 + 0x1C0,
// 0x160, 8, 0x78) onto (_Columns[+0x36] + 0x1C0, 0x160), committed to slot 6.
extern "C" void __cdecl EffectKind18Sub0C_MoveFrame(unsigned frame) {
    const U offset = static_cast<U>(Halve(static_cast<I>((frame + 1) << 4)));
    const U to = Byte("EffectKind18Sub0C_MoveFrame", at::kSub0CColumns, at::kSub0CColumnCount,
                      static_cast<U>(SW(S() + 0x36))) + 0x1C0u;
    MoveColumn(offset + 0x1C0, to);
}

// ===========================================================================
// Sub-kind 0x0E (EffectKind18_States[14])
// ===========================================================================

// original 0x4FFE40 (EffectKind18_States[14], hidden in 0x4FFDD0): jmp
// [EffectKind18Sub0E_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub0E_Run(void) {
    Dispatch("EffectKind18Sub0E_Run", AddressOf(EffectKind18Sub0E_States), EffectKind18Sub0E_States_count);
}

// original 0x4FFE60 (state 0): the lowering +0x30 0; row 9's flag 0xC clear: the
// gate's cells 0x50 and +2 5; set: the cells 0xA1, +2 up and, with the leader
// at the gate, +0x30 0xFF00 and +2 3. The gate drawn.
extern "C" void __cdecl EffectKind18Sub0E_Start(void) {
    SetWord(S() + 0x30, 0);
    if (!SH_CALL(Flags_Test)(Row9(), 0xC)) {
        Sub0ECells(0x50);
        S()[2] = 5;
        SH_CALL(EffectKind18Sub0E_DrawGate)();
        return;
    }
    Sub0ECells(0xA1);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    if (Sub0ENear(0x8000, 0x10000)) {
        SetWord(S() + 0x30, 0xFF00);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x4FFF80 (state 1; EffectKind18Sub0E_Raise calls it): Cond_ByteFE not
// 0, or the leader at the gate: sound 0x200 (Field_Request 0 only) and +2 2 -
// each test on its own. The gate drawn.
extern "C" void __cdecl EffectKind18Sub0E_Wait(void) {
    if (Cond_ByteFE != 0) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        S()[2] = 2;
    }
    if (Sub0ENear(0x8000, 0x10000)) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x500020 (state 2): +0x30 down 0x20; at -0x100 or below (s16): +2 up.
// A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub0E_Lower(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) - 0x20u);
    if (SW(S() + 0x30) <= -0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x500040 (state 3): the leader away from the gate (0x18000 either
// way): +2 4; Cond_ByteFE not 0: +2 3. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub0E_Open(void) {
    if (!Sub0ENear(0x18000, 0x18000)) S()[2] = 4;
    if (Cond_ByteFE != 0) S()[2] = 3;
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x5000B0 (state 4): +0x30 up 0x20; still below 0 (s16): the draw.
// Else EffectKind18Sub0E_Wait (which draws); +2 left at 2 by it: done; else +2
// 1, sound 0x201 (Field_Request 0 only) and the draw again.
extern "C" void __cdecl EffectKind18Sub0E_Raise(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + 0x20u);
    if (SW(S() + 0x30) < 0) {
        SH_CALL(EffectKind18Sub0E_DrawGate)();
        return;
    }
    SH_CALL(EffectKind18Sub0E_Wait)();
    unsigned char* const r = S();
    if (r[2] == 2) return;
    r[2] = 1;
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x201);
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x500100 (state 5): row 9's flag 0xC set: the cells 0xA1 and +2 1. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub0E_Closed(void) {
    if (SH_CALL(Flags_Test)(Row9(), 0xC)) {
        Sub0ECells(0xA1);
        S()[2] = 1;
    }
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x500160: under Draw_PassFlags bit 2, a draw mode (tpage 0x95)
// committed to slot 6, then a POLY_FT4 standing at x -0x3F40 from z +0x30 -
// 0x34C0 to +0x30 - 0x35C0, its foot 0x40 and its head 0x180 above half the
// ground's elevation at each end (AreaMap_Elevation), Gte_RotTransPers4,
// Gte_PrimDepths4_10, Prim_SetTexture(0x25500123, prim, 1), slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub0E_DrawGate(void) {
    if ((Draw_PassFlags & 4) == 0) return;
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const prim = Prim();
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    const unsigned char* const s = S();
    SetV(3, 0, 0xC0C0);
    SetV(2, 0, 0xC0C0);
    SetV(1, 0, 0xC0C0);
    SetV(0, 0, 0xC0C0);
    const U near = Word(s + 0x30) - 0x34C0u;
    SetV(2, 1, near);
    SetV(0, 1, near);
    const U z0 = static_cast<U>(static_cast<std::int16_t>(near) + 0x4000) << 9;
    const U far = Word(s + 0x30) - 0x35C0u;
    SetV(3, 1, far);
    SetV(1, 1, far);
    const long e1 = SH_CALL(AreaMap_Elevation)(0x18000, static_cast<long>(z0));
    SetV(0, 2, static_cast<U>(0x40 - Half(e1)));
    const long e2 = SH_CALL(AreaMap_Elevation)(0x18000, static_cast<long>(z0));
    const U z1 = static_cast<U>(VS(1, 1) + 0x4000) << 9;
    const U x1 = static_cast<U>(VS(1, 0) + 0x4000) << 9;
    SetV(2, 2, static_cast<U>(0x180 - Half(e2)));
    const long e3 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
    SetV(1, 2, static_cast<U>(0x40 - Half(e3)));
    const long e4 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
    SetV(3, 2, static_cast<U>(0x180 - Half(e4)));
    Project4(prim, 0x10);
    SH_CALL(Gte_PrimDepths4_10)(prim);
    SH_CALL(Prim_SetTexture)(0x25500123, prim, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

// ===========================================================================
// Sub-kind 0x13 (EffectKind18_States[19])
// ===========================================================================

namespace {
unsigned Sub13(const char* who, U array) { return Byte(who, array, at::kSub13Count, S()[0xB]); }
}  // namespace

// original 0x500300 (EffectKind18_States[19], hidden in 0x500160): jmp
// [EffectKind18Sub13_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub13_Run(void) {
    Dispatch("EffectKind18Sub13_Run", AddressOf(EffectKind18Sub13_States), EffectKind18Sub13_States_count);
}

// original 0x500320 (state 0): +0xB = the byte +0x36 (the variant); the point
// +0x34 / +0x38 the variant's cell (_Cells, << 16); its row-9 flag (_Flags)
// set: +2 up and a jump into state 1; clear: +2 3 and a tail jump to state 3.
extern "C" void __cdecl EffectKind18Sub13_Start(void) {
    const char* const who = "EffectKind18Sub13_Start";
    unsigned char* s = S();
    s[0xB] = s[0x36];
    s = S();
    SetUL(s + 0x34, static_cast<U>(Byte(who, at::kSub13Cells, 2 * at::kSub13Count, 2u * s[0xB])) << 16);
    s = S();
    SetUL(s + 0x38, static_cast<U>(B(at::kSub13Cells + 1 + 2u * s[0xB])) << 16);
    if (SH_CALL(Flags_Test)(Row9(), Sub13(who, at::kSub13Flags))) {
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        SH_CALL(EffectKind18Sub13_Shown)();
        return;
    }
    S()[2] = 3;
    SH_CALL(EffectKind18Sub13_Hidden)();
}

// original 0x5003A0 (state 1; 0x500320 jumps here): the variant's flag clear: +9
// 9 and +2 up. The panel with face +0xB + 4 * (row 9's _Faces flag) + 1 at full
// height 0x40, and a tail jump to the line.
extern "C" void __cdecl EffectKind18Sub13_Shown(void) {
    const char* const who = "EffectKind18Sub13_Shown";
    if (!SH_CALL(Flags_Test)(Row9(), Sub13(who, at::kSub13Flags))) {
        S()[9] = 9;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    const U face = SH_CALL(Flags_Test)(Row9(), Sub13(who, at::kSub13Faces)) & 0xFFu;
    SH_CALL(EffectKind18Sub13_DrawPanel)(S()[0xB] + face * 4 + 1, 0x40);
    SH_CALL(EffectKind18Sub13_DrawLine)();
}

// original 0x500420 (state 2): +9 down one, at 0: +2 up. The bare panel (face 0,
// 0x40), the face panel at +9 * 8, and a tail jump to the line.
extern "C" void __cdecl EffectKind18Sub13_Fall(void) {
    const char* const who = "EffectKind18Sub13_Fall";
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] == 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub13_DrawPanel)(0, 0x40);
    const int height = static_cast<int>(static_cast<unsigned>(S()[9]) << 3);
    const U face = SH_CALL(Flags_Test)(Row9(), Sub13(who, at::kSub13Faces)) & 0xFFu;
    SH_CALL(EffectKind18Sub13_DrawPanel)(S()[0xB] + face * 4 + 1, height);
    SH_CALL(EffectKind18Sub13_DrawLine)();
}

// original 0x5004A0 (state 3; 0x500320 jumps here): the variant's flag set: +9 9
// and +2 up. The bare panel (face 0, 0x40).
extern "C" void __cdecl EffectKind18Sub13_Hidden(void) {
    if (SH_CALL(Flags_Test)(Row9(), Sub13("EffectKind18Sub13_Hidden", at::kSub13Flags))) {
        S()[9] = 9;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub13_DrawPanel)(0, 0x40);
}

// original 0x5004F0 (state 4): +9 down one, at 0: +2 1. A draw mode (tpage 0xB5)
// linked at the point; the face panel at 0x40; a semi-transparent POLY_F4 over
// the panel's vertices, grey +9 * 0x1E (Gte_RotTransPers4, Gte_PrimDepths4_0C),
// linked (0x38); then the line.
extern "C" void __cdecl EffectKind18Sub13_Rise(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] == 0) S()[2] = 1;
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 0, 0xB5, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
    const U face = SH_CALL(Flags_Test)(Row9(), Sub13("EffectKind18Sub13_Rise", at::kSub13Faces)) & 0xFFu;
    SH_CALL(EffectKind18Sub13_DrawPanel)(S()[0xB] + face * 4 + 1, 0x40);
    unsigned char* const prim = Prim();
    SH_CALL(Gpu_SetPolyF4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const auto grey = static_cast<unsigned char>(S()[9] * 0x1E);
    prim[6] = grey;
    prim[5] = grey;
    prim[4] = grey;
    Project4(prim, 0xC);
    SH_CALL(Gte_PrimDepths4_0C)(prim);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0x38);
    SH_CALL(EffectKind18Sub13_DrawLine)();
}

// original 0x500610 (texture, height): a POLY_FT4 standing at x +0x36 * 0x80 -
// 0x3FC0 between z +0x3A * 0x80 + height - 0x4008 and - height - 0x3FF8; its
// head 0x48 - height and its foot height + 0x40 off half the ground's
// elevation at each end; Gte_RotTransPers4, Gte_PrimDepths4_10,
// Prim_SetTexture(texture + 0x275000F3, prim, 1), linked at the point (0x48).
extern "C" void __cdecl EffectKind18Sub13_DrawPanel(unsigned long texture, int height) {
    const unsigned char* const s = S();
    const U h = static_cast<U>(height);
    const U x = (Word(s + 0x36) << 7) - 0x3FC0u;
    SetV(3, 0, x);
    SetV(2, 0, x);
    SetV(1, 0, x);
    SetV(0, 0, x);
    const U zn = (Word(s + 0x3A) << 7) + h - 0x4008u;
    const U xi = static_cast<U>(static_cast<std::int16_t>(x) + 0x4000) << 9;
    const U zi = static_cast<U>(static_cast<std::int16_t>(zn) + 0x4000) << 9;
    SetV(2, 1, zn);
    SetV(0, 1, zn);
    const U zf = (Word(s + 0x3A) << 7) - h - 0x3FF8u;
    SetV(3, 1, zf);
    SetV(1, 1, zf);
    const long e1 = SH_CALL(AreaMap_Elevation)(static_cast<long>(xi), static_cast<long>(zi));
    SetV(0, 2, 0x48u - static_cast<U>(Half(e1)) - h);
    const long e2 = SH_CALL(AreaMap_Elevation)(static_cast<long>(xi), static_cast<long>(zi));
    const U z1 = static_cast<U>(VS(1, 1) + 0x4000) << 9;
    const U x1 = static_cast<U>(VS(1, 0) + 0x4000) << 9;
    SetV(2, 2, h - static_cast<U>(Half(e2)) + 0x40u);
    const long e3 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
    SetV(1, 2, 0x48u - static_cast<U>(Half(e3)) - h);
    const long e4 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
    SetV(3, 2, h - static_cast<U>(Half(e4)) + 0x40u);
    unsigned char* const prim = Prim();
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    Project4(prim, 0x10);
    SH_CALL(Gte_PrimDepths4_10)(prim);
    SH_CALL(Prim_SetTexture)(static_cast<U>(texture) + 0x275000F3u, prim, 1);
    const unsigned char* const r = S();
    SH_CALL(MapView_LinkPrimAt)(UL(r + 0x34), UL(r + 0x38), 1, 0x48);
}

// original 0x5007B0: a draw mode (tpage 0x95, dtd 1) linked at the point; a
// semi-transparent LINE_F2, grey 0x80 on odd frames, from Prim_VertexScratch's
// first two vertices lifted to 2 * (Frame_Counter % 48) + 4 above half the
// ground's elevation (Gte_RotTransPers3 with the third into MapView_ScreenXY,
// Gte_StoreDepthF3), linked (0x20); a closing draw mode (dtd 0) linked.
extern "C" void __cdecl EffectKind18Sub13_DrawLine(void) {
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 1, 0x95, 0);
    const unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
    unsigned char* const prim = Prim();
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const auto grey = static_cast<unsigned char>((Frame_Counter & 1) << 7);
    prim[6] = grey;
    prim[5] = grey;
    prim[4] = grey;
    const U z0 = static_cast<U>(VS(0, 1) + 0x4000) << 9;
    const U x0 = static_cast<U>(VS(0, 0) + 0x4000) << 9;
    const long e1 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x0), static_cast<long>(z0));
    const I h1 = Half(e1);
    const U lift1 = (Frame_Counter % 0x30u) * 2 + 4;
    const U x1 = static_cast<U>(VS(1, 0) + 0x4000) << 9;
    SetV(0, 2, lift1 - static_cast<U>(h1));
    const U z1 = static_cast<U>(VS(1, 1) + 0x4000) << 9;
    const long e2 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
    const I h2 = Half(e2);
    const U lift2 = (Frame_Counter % 0x30u) * 2 + 4;
    SetV(1, 2, lift2 - static_cast<U>(h2));
    long p;
    SH_CALL(Gte_RotTransPers3)(Vertex(0), Vertex(1), Vertex(2), reinterpret_cast<float*>(prim + 8),
                               reinterpret_cast<float*>(prim + 0x14), MapView_ScreenXY, &p);
    float depth;
    SH_CALL(Gte_StoreDepthF3)(reinterpret_cast<float*>(prim + 0x10), reinterpret_cast<float*>(prim + 0x1C), &depth);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0x20);
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 0, 0x95, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
}

// ===========================================================================
// Sub-kinds 0x0F and 0x4F (EffectKind18_States[15], [79]): the gates
// ===========================================================================

namespace {

// _Start's set-up common to sub-kinds 0x0F and 0x4F, from the variant +0x36 (v)
// and the flag index +0x3A: +0xA (0x0F: the byte +0x3A + 1; 0x4F: 8 - the byte
// v), +0xB = _Flags[+0x3A], the cell words from _Cells[v], +8 = (v == 0), +0x3E
// = -_Heights[v] - half the ground's elevation at the point, +0x32 =
// _Heights[v], +0x2E = _Textures[v], +0x30 = 0.
void Gate0FPrepare(const char* who, bool sub4F) {
    unsigned char* s = S();
    const I v = SW(s + 0x36);
    if (sub4F) s[0xA] = static_cast<unsigned char>(8 - static_cast<unsigned char>(v));
    else s[0xA] = static_cast<unsigned char>(s[0x3A] + 1);
    s = S();
    s[0xB] = static_cast<unsigned char>(Byte(who, at::kGateFlags, at::kGateFlagCount, static_cast<U>(SW(s + 0x3A))));
    const unsigned cell_x = Byte(who, at::kGateCells, 2 * at::kGateCount, 2u * static_cast<U>(v));
    SetWord(S() + 0x36, cell_x);
    SetWord(S() + 0x3A, B(at::kGateCells + 1 + 2u * static_cast<U>(v)));
    S()[8] = v == 0 ? 1 : 0;
    s = S();
    const long e = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    const U height = Word(At(at::kGateHeights + 2u * static_cast<U>(v)));
    SetWord(S() + 0x3E, (0u - height) - static_cast<U>(Half(e)));
    SetWord(S() + 0x32, height);
    SetWord(S() + 0x2E, Word(At(at::kGateTextures + 2u * static_cast<U>(v))));
    SetWord(S() + 0x30, 0);
}

}  // namespace

// original 0x500930 (EffectKind18_States[15], hidden in 0x5007B0): jmp
// [EffectKind18Sub0F_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub0F_Run(void) {
    Dispatch("EffectKind18Sub0F_Run", AddressOf(EffectKind18Sub0F_States), EffectKind18Sub0F_States_count);
}

// original 0x500950 (state 0): the set-up (Gate0FPrepare); row 9's flag +0xB
// clear: the gate's cells 0x52 and +2 5; set: the cells 0xA1, +2 up, and with
// the leader at the gate +0x30 0xFF00 and +2 3. The gate drawn flat (0).
extern "C" void __cdecl EffectKind18Sub0F_Start(void) {
    Gate0FPrepare("EffectKind18Sub0F_Start", false);
    if (!SH_CALL(Flags_Test)(Row9(), S()[0xB])) {
        GateCells(0x52);
        S()[2] = 5;
        SH_CALL(EffectKind18Sub0F_DrawGate)(0);
        return;
    }
    GateCells(0xA1);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    unsigned char* const s = S();
    if (GateNear(s, 0x8000, 0x10000)) {
        SetWord(s + 0x30, 0xFF00);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(0);
}

// original 0x500BD0 (state 1; EffectKind18Sub0F_Raise calls it): Cond_ByteFD 0
// and Cond_ByteFE the record's +0xA, or the leader at the gate: sound 0x200
// (Field_Request 0 only) and +2 2, each on its own. The gate drawn on the ground.
extern "C" void __cdecl EffectKind18Sub0F_Wait(void) {
    if (Cond_ByteFD == 0 && Cond_ByteFE == S()[0xA]) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        S()[2] = 2;
    }
    if (GateNear(S(), 0x8000, 0x10000)) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(1);
}

// original 0x500CF0 (state 2 of both tables): +0x30 down 0x20; at -0x100 or below
// (s16): +2 up. The gate drawn flat.
extern "C" void __cdecl EffectKind18Sub0F_Lower(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) - 0x20u);
    if (SW(S() + 0x30) <= -0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub0F_DrawGate)(0);
}

// original 0x500D20 (state 3): unless Cond_ByteFD is 0 and Cond_ByteFE the
// record's +0xA, the leader away from the gate (two cells): +2 4. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub0F_Open(void) {
    unsigned char* const s = S();
    if (Cond_ByteFD == 0 && Cond_ByteFE == s[0xA]) return;
    if (!GateNear(s, 0x20000, 0x20000)) s[2] = 4;
}

// original 0x500E00 (state 4 of both tables): +0x30 up 0x20; still below 0 (s16):
// drawn flat. Else EffectKind18Sub0F_Wait (which draws); +2 left at 2 by it:
// done; else sound 0x201 (Field_Request 0 only), +2 1 and drawn flat.
extern "C" void __cdecl EffectKind18Sub0F_Raise(void) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + 0x20u);
    if (SW(S() + 0x30) < 0) {
        SH_CALL(EffectKind18Sub0F_DrawGate)(0);
        return;
    }
    SH_CALL(EffectKind18Sub0F_Wait)();
    if (S()[2] == 2) return;
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x201);
    S()[2] = 1;
    SH_CALL(EffectKind18Sub0F_DrawGate)(0);
}

// original 0x500E50 (state 5): row 9's flag +0xB set: the gate's cells 0xA1 and
// +2 1. The gate drawn on the ground.
extern "C" void __cdecl EffectKind18Sub0F_Closed(void) {
    if (SH_CALL(Flags_Test)(Row9(), S()[0xB])) {
        GateCells(0xA1);
        S()[2] = 1;
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(1);
}

// original 0x500EF0 (ground): under Draw_PassFlags bit 2, a draw mode (tpage
// 0x95) committed to slot 6; a POLY_FT4 standing over the gate's cell - along x
// with +8 set (x +0x36 * 0x80 - +0x30 - 0x3F40 to 0x100 less, z +0x3A * 0x80 -
// 0x3FC0), along z without (x +0x36 * 0x80 - 0x3FC0, z +0x3A * 0x80 + +0x30 -
// 0x3F40 to 0x100 less) - its feet at -(half the elevation at each end) -
// +0x3E with ground, else +0x32, its heads 0x140 above; Gte_RotTransPers4,
// Gte_PrimDepths4_10, Prim_SetTexture(+8 * 41 << 16 | (s16) +0x2E | 0x26500000,
// prim, 1), slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub0F_DrawGate(int ground) {
    if ((Draw_PassFlags & 4) == 0) return;
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const prim = Prim();
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    const unsigned char* const s = S();
    U vx, vz;
    if (s[8] != 0) {
        vz = (Word(s + 0x3A) << 7) - 0x3FC0u;
        SetV(3, 1, vz);
        SetV(2, 1, vz);
        SetV(1, 1, vz);
        SetV(0, 1, vz);
        vx = (Word(s + 0x36) << 7) - Word(s + 0x30) - 0x3F40u;
        SetV(2, 0, vx);
        SetV(0, 0, vx);
        const U x1 = (Word(s + 0x36) << 7) - Word(s + 0x30) - 0x4040u;
        SetV(3, 0, x1);
        SetV(1, 0, x1);
    } else {
        vx = (Word(s + 0x36) << 7) - 0x3FC0u;
        SetV(3, 0, vx);
        SetV(2, 0, vx);
        SetV(1, 0, vx);
        SetV(0, 0, vx);
        vz = (Word(s + 0x3A) << 7) + Word(s + 0x30) - 0x3F40u;
        SetV(2, 1, vz);
        SetV(0, 1, vz);
        const U z1 = (Word(s + 0x3A) << 7) + Word(s + 0x30) - 0x4040u;
        SetV(3, 1, z1);
        SetV(1, 1, z1);
    }
    if (ground != 0) {
        const U z0 = static_cast<U>(static_cast<std::int16_t>(vz) + 0x4000) << 9;
        const U x0 = static_cast<U>(static_cast<std::int16_t>(vx) + 0x4000) << 9;
        const long e1 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x0), static_cast<long>(z0));
        SetV(0, 2, (0u - static_cast<U>(Half(e1))) - Word(S() + 0x3E) - 0x140u);
        const long e2 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x0), static_cast<long>(z0));
        const U z1 = static_cast<U>(VS(1, 1) + 0x4000) << 9;
        const U x1 = static_cast<U>(VS(1, 0) + 0x4000) << 9;
        SetV(2, 2, (0u - static_cast<U>(Half(e2))) - Word(S() + 0x3E));
        const long e3 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
        SetV(1, 2, (0u - static_cast<U>(Half(e3))) - Word(S() + 0x3E) - 0x140u);
        const long e4 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
        SetV(3, 2, (0u - static_cast<U>(Half(e4))) - Word(S() + 0x3E));
    } else {
        const U head = Word(s + 0x32) - 0x140u;
        SetV(1, 2, head);
        SetV(0, 2, head);
        SetV(3, 2, Word(s + 0x32));
        SetV(2, 2, Word(s + 0x32));
    }
    Project4(prim, 0x10);
    SH_CALL(Gte_PrimDepths4_10)(prim);
    const unsigned char* const r = S();
    const U texture = ((static_cast<U>(r[8]) * 41u) << 16 | static_cast<U>(SW(r + 0x2E))) | 0x26500000u;
    SH_CALL(Prim_SetTexture)(texture, prim, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

// original 0x5011A0 (EffectKind18_States[79], hidden in 0x500EF0): jmp
// [EffectKind18Sub4F_States + +2 * 4], unbounded. Its entries 2 and 4 are
// sub-kind 0x0F's _Lower and _Raise.
extern "C" void __cdecl EffectKind18Sub4F_Run(void) {
    Dispatch("EffectKind18Sub4F_Run", AddressOf(EffectKind18Sub4F_States), EffectKind18Sub4F_States_count);
}

// original 0x5011C0 (state 0): the set-up (+0xA = 8 - the variant); the gate's
// cells (x, z) and (x, z + 1) 0x52; +2 up; with the leader at the gate (the +8
// clear test whatever +8 holds) the cells 0xA1, +0x30 0xFF00 and +2 3. Drawn
// flat.
extern "C" void __cdecl EffectKind18Sub4F_Start(void) {
    Gate0FPrepare("EffectKind18Sub4F_Start", true);
    Gate4FCells(0x52);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    const unsigned char* const s = S();
    const I x = SW(s + 0x36), z = SW(s + 0x3A);
    if (GateNearXZ(x, z, false, 0x8000, 0x10000)) {
        *Cell(x, z) = 0xA1;
        const unsigned char* const r = S();
        *Cell(SW(r + 0x36), SW(r + 0x3A) + 1) = 0xA1;
        SetWord(S() + 0x30, 0xFF00);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(0);
}

// original 0x5013A0 (state 1): Cond_ByteFE the record's +0xA: sound 0x200
// (Field_Request 0 only), the cells 0xA1 and +2 2. Drawn on the ground.
extern "C" void __cdecl EffectKind18Sub4F_Wait(void) {
    if (Cond_ByteFE == S()[0xA]) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        Gate4FCells(0xA1);
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(1);
}

// original 0x501430 (state 3): unless Cond_ByteFE is the record's +0xA, the leader
// away from the gate (two cells, the +8 clear test): the cells 0x52 and +2 4.
// Drawn flat.
extern "C" void __cdecl EffectKind18Sub4F_Open(void) {
    const unsigned char* const s = S();
    if (Cond_ByteFE != s[0xA]) {
        const I x = SW(s + 0x36), z = SW(s + 0x3A);
        if (!GateNearXZ(x, z, false, 0x20000, 0x20000)) {
            *Cell(x, z) = 0x52;
            const unsigned char* const r = S();
            *Cell(SW(r + 0x36), SW(r + 0x3A) + 1) = 0x52;
            S()[2] = 4;
        }
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(0);
}

void Effect5B_Inject() {
    if (bof3::WantsShadow("effect_5b")) effect_5b::SelfTest();
    BOF3_INJECT(EffectKind18Sub0D_Run);
    BOF3_INJECT(EffectKind18Sub0D_Start);
    BOF3_INJECT(EffectKind18Sub0D_Wait);
    BOF3_INJECT(EffectKind18Sub0D_Open);
    BOF3_INJECT(EffectKind18Sub0D_Idle);
    BOF3_INJECT(EffectKind18Sub0D_Shut);
    BOF3_INJECT(EffectKind18Sub0D_Flash);
    BOF3_INJECT(EffectKind18Sub0D_MoveFrame);
    BOF3_INJECT(EffectKind18Sub0D_DrawGlow);
    BOF3_INJECT(EffectKind18Sub0B_Run);
    BOF3_INJECT(EffectKind18Sub0B_Start);
    BOF3_INJECT(EffectKind18Sub0B_Rise);
    BOF3_INJECT(EffectKind18Sub0B_Fade);
    BOF3_INJECT(EffectKind18Sub0B_DrawTiles);
    BOF3_INJECT(EffectKind18Sub0C_Run);
    BOF3_INJECT(EffectKind18Sub0C_Start);
    BOF3_INJECT(EffectKind18Sub0C_Wait);
    BOF3_INJECT(EffectKind18Sub0C_Open);
    BOF3_INJECT(EffectKind18Sub0C_Idle);
    BOF3_INJECT(EffectKind18Sub0C_Shut);
    BOF3_INJECT(EffectKind18Sub0C_Flash);
    BOF3_INJECT(EffectKind18Sub0C_MoveFrame);
    BOF3_INJECT(EffectKind18Sub0E_Run);
    BOF3_INJECT(EffectKind18Sub0E_Start);
    BOF3_INJECT(EffectKind18Sub0E_Wait);
    BOF3_INJECT(EffectKind18Sub0E_Lower);
    BOF3_INJECT(EffectKind18Sub0E_Open);
    BOF3_INJECT(EffectKind18Sub0E_Raise);
    BOF3_INJECT(EffectKind18Sub0E_Closed);
    BOF3_INJECT(EffectKind18Sub0E_DrawGate);
    BOF3_INJECT(EffectKind18Sub13_Run);
    BOF3_INJECT(EffectKind18Sub13_Start);
    BOF3_INJECT(EffectKind18Sub13_Shown);
    BOF3_INJECT(EffectKind18Sub13_Fall);
    BOF3_INJECT(EffectKind18Sub13_Hidden);
    BOF3_INJECT(EffectKind18Sub13_Rise);
    BOF3_INJECT(EffectKind18Sub13_DrawPanel);
    BOF3_INJECT(EffectKind18Sub13_DrawLine);
    BOF3_INJECT(EffectKind18Sub0F_Run);
    BOF3_INJECT(EffectKind18Sub0F_Start);
    BOF3_INJECT(EffectKind18Sub0F_Wait);
    BOF3_INJECT(EffectKind18Sub0F_Lower);
    BOF3_INJECT(EffectKind18Sub0F_Open);
    BOF3_INJECT(EffectKind18Sub0F_Raise);
    BOF3_INJECT(EffectKind18Sub0F_Closed);
    BOF3_INJECT(EffectKind18Sub0F_DrawGate);
    BOF3_INJECT(EffectKind18Sub4F_Run);
    BOF3_INJECT(EffectKind18Sub4F_Start);
    BOF3_INJECT(EffectKind18Sub4F_Wait);
    BOF3_INJECT(EffectKind18Sub4F_Open);
}
