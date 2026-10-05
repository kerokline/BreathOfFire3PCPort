// Round fourteen group R3E (docs/rest_3e.md): the 50 functions of
// analysis/round14_cut.tsv's group R3E, 0x46A320..0x4801ED, each read with
// capstone to its last instruction (2026-10-04). All effect code: three kinds'
// dispatchers, the helpers five kinds of round twelve and thirteen call, the
// pools kinds 0x48 / 0x49 / 0x52 draw with, and the states of kinds 0x5D, 0x5E
// and 0x5F.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies, with one
// exception: EffectGlowTrail_Update is EffectKind52_TrailUpdate (E2F's)
// instruction for instruction but the call targets, so it calls that function
// directly (its callees go through the harness inside it). Sprite_Current is
// read again wherever the original reads [0x937F88] again after a call. No
// divergence: each is a faithful replacement. Where the original jumps through
// a state table past its end, or indexes the marks past their room, ours
// aborts with a message (docs/rest_3e.md section 7).
#include "game/rest_3e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/rest_3e_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3e::at;
using U = std::uint32_t;
using LD = long double;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
unsigned W(U a) { return Word(At(a)); }
unsigned char& B(U a) { return At(a)[0]; }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul r32, r32: the product's low 32 bits.
U Mul(U a, U b) { return a * b; }
// (cos or sin answered first) * the width word +0xE, zero-extended, sar 12:
// the width read after the call, as the original reads it.
U Turn(int answer, const unsigned char* point) { return Sar(Mul(static_cast<U>(answer), Word(point + 0xE)), 12); }
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

// --- x87 as the original has it (effect_3a.cpp's form): `fld dword` through
// inline assembly, every operation in long double at the game's control word,
// `fstp dword` out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}
LD I(std::int32_t v) { return static_cast<LD>(v); }
void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
// fild of a dword then fstp: an integer stored as a single.
void StI(unsigned char* p, std::int32_t v) { StF(p, I(v)); }
// fcomp; fnstsw; test ah, 0x40: C3 set - equal, or unordered.
bool EqualOrUnordered(const unsigned char* a, const unsigned char* b) {
    const LD x = F(a), y = F(b);
    return !(x < y) && !(x > y);
}

const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next table.
void Dispatch(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3e.md section 7)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// The draw-mode packet the draws open with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int x, int y, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

}  // namespace

// ===========================================================================
// Three kinds' dispatchers (Effect_KindHandlers[0x37], [0x17], [0x1B]; their
// state tables FC1's, docs/field_c1.md)
// ===========================================================================

// original 0x46A320 (hidden in 0x46A1E0's catalog extent; EffectKind32_Arc
// ends at 0x46A287): jmp [EffectKind37_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind37_Run(void) { Dispatch("EffectKind37_Run", EffectKind37_States, EffectKind37_States_count); }
// original 0x46ABB0: jmp [EffectKind17_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind17_Run(void) { Dispatch("EffectKind17_Run", EffectKind17_States, EffectKind17_States_count); }
// original 0x46B7A0 (EffectKind17_CellBlocked ends at 0x46B799): jmp
// [EffectKind1B_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind1B_Run(void) { Dispatch("EffectKind1B_Run", EffectKind1B_States, EffectKind1B_States_count); }

// ===========================================================================
// Helpers of kinds 0x41, 0x30, 0x1D, 0x21, 0x24
// ===========================================================================

// original 0x46D5F0 (FC2's kind 0x41 calls it): Crt_sprintf(0x904BA0,
// Area08_MessageFormat, the byte +6); a draw mode (page 0xF) committed to the
// slot +0x29 (0xC); then each character of the text from the first (the first
// taken whatever it is, the loop's test after it), a space skipped, else the
// character less '0' written back and an 8 x 8 SPRT at (x + 8 i, y) - x and y
// the low words, signed - its u (char - '0' + 0x16) * 8, v 0xD0, the colour
// 0x80, CLUT (clut byte * 16, 0x1E0), committed to the slot +0x29 (0x1C). The
// index is a byte: past 255 it wraps to the text's start (already rewritten),
// so the loop stops only at a NUL.
extern "C" void __cdecl EffectKind41_DrawNumber(int x, int y, int /*unused*/, unsigned clut) {
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kText)), reinterpret_cast<const char*>(At(at::kNumberFormat)),
                         static_cast<unsigned>(S()[6]));
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xF, 0);
    SH_CALL(Gfx_CommitPrim)(S()[0x29], 0xC);
    U px = static_cast<U>(x);
    unsigned char i = 0;
    unsigned char* c = At(at::kText);
    do {
        if (*c != 0x20) {
            unsigned char* const p = Gfx_PacketNext;
            *c = static_cast<unsigned char>(*c - 0x30);
            const U clut_word = SH_CALL(Gpu_GetClut)(static_cast<int>((clut & 0xFFu) << 4), 0x1E0);
            SetWord(p + 0x16, clut_word);
            p[4] = 0x80;
            p[5] = 0x80;
            p[6] = 0x80;
            StI(p + 8, static_cast<std::int16_t>(px));
            StI(p + 0xC, static_cast<std::int16_t>(y));
            p[0x15] = 0xD0;
            p[0x14] = static_cast<unsigned char>((*c + 0x16) << 3);
            SetWord(p + 0x18, 8);
            SetWord(p + 0x1A, 8);
            SH_CALL(Gpu_SetSprt)(p);
            SH_CALL(Gfx_CommitPrim)(S()[0x29], 0x1C);
        }
        px += 8;
        ++i;
        c = At(at::kText + i);
    } while (*c != 0);
}

// original 0x46D710 (Area135_SpawnCopy calls it; FC2's EffectKind30_Push does
// the same): the byte *(+0x54), signed, times 0x28 bytes copied from *(+0x50)
// to 0x8C5D80 one at a time, +0x50 stepped each byte (Sprite_Current read each
// time); +0x50 = 0x8C5D80; EffectKind30_ShardsInit, EffectKind30_SparksInit;
// +9 = 0x10.
extern "C" void __cdecl EffectShards_LoadModel(void) {
    unsigned char* s = S();
    std::int32_t n = static_cast<std::int8_t>(reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(UL(s + 0x54)))[0]) *
                     static_cast<std::int32_t>(at::kModelFace);
    unsigned char* to = At(at::kModelCopy);
    for (; n > 0; --n) {
        *to++ = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(UL(s + 0x50)))[0];
        SetUL(s + 0x50, UL(s + 0x50) + 1);
        s = S();
    }
    SetUL(s + 0x50, at::kModelCopy);
    SH_CALL(EffectKind30_ShardsInit)();
    SH_CALL(EffectKind30_SparksInit)();
    S()[9] = 0x10;
}

// original 0x46D770 (Area135_SpawnCountdown calls it): EffectKind30_ShardsStep,
// then a tail jump to EffectKind30_SparksDraw.
extern "C" void __cdecl EffectShards_Step(void) {
    SH_CALL(EffectKind30_ShardsStep)();
    SH_CALL(EffectKind30_SparksDraw)();
}

// original 0x46E190 (E1C's EffectKind1D_MoveShards calls it; PSX twin
// 0x801F752C): the speck's point +4 projected (EffectGte_ProjectPoint into a
// stack vector); a TILE_1 at the packet cursor, opaque, at it, white on an odd
// Rand, else black; committed to slot 1 (0x14). E2A's EffectSpecks_Draw links
// the same tile instead.
extern "C" void __cdecl EffectKind1D_DrawSpeck(const unsigned char* speck) {
    alignas(4) unsigned char screen[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(speck + 4), Out(screen));
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    Copy12(p + 8, screen);
    const auto c = static_cast<unsigned char>((SH_CALL(Rand)() & 1) ? 0xFF : 0);
    p[6] = c;
    p[5] = c;
    p[4] = c;
    SH_CALL(Gfx_CommitPrim)(1, 0x14);
}

// original 0x46F570 (E1D's kind 0x21 states call it; PSX twin 0x801F7FFC): the
// arm's four points round its centre. With the angle +0x4C and the length
// +0x52 (words, signed): c = cos * length << 4, s = sin * length << 4; a matrix
// EffectGte_SetDiagonalOne, turned by Gte_RotMatrixY(+0x4E) and
// Gte_RotMatrixZ(+0x50). The original then calls Gte_ApplyMatrixLV once with a
// stack vector it never wrote, and drops the answer (ours passes zeros: the
// callee writes only its out, which the loop overwrites). Point i of four
// (+0xC + 0x10 i): the vector (0, 0), (c, s), (c, 0), (c, -s) - z 0 - through
// the matrix; x = +0 + out x, y = +4 + out y, z = +8 + (out z << 8).
extern "C" void __cdecl EffectKind21_ArmPoints(unsigned char* arm) {
    const U cos = static_cast<U>(SH_CALL(Math_Cos)(SW(arm + 0x4C)));
    const U c = Mul(cos, static_cast<U>(SW(arm + 0x52))) << 4;
    const U sin = static_cast<U>(SH_CALL(Math_Sin)(SW(arm + 0x4C)));
    const U s = Mul(sin, static_cast<U>(SW(arm + 0x52))) << 4;
    alignas(4) short m[16] = {};
    SH_CALL(EffectGte_SetDiagonalOne)(m);
    SH_CALL(Gte_RotMatrixY)(SW(arm + 0x4E), m);
    SH_CALL(Gte_RotMatrixZ)(SW(arm + 0x50), m);
    long v[3] = {0, 0, 0};
    long out[3];
    SH_CALL(Gte_ApplyMatrixLV)(m, v, out);
    for (unsigned i = 0; i < 4; ++i) {
        switch (i) {
        case 0: v[0] = 0; v[1] = 0; break;
        case 1: v[0] = static_cast<long>(c); v[1] = static_cast<long>(s); break;
        case 2: v[0] = static_cast<long>(c); v[1] = 0; break;
        default: v[0] = static_cast<long>(c); v[1] = static_cast<long>(0u - s); break;
        }
        v[2] = 0;
        SH_CALL(Gte_ApplyMatrixLV)(m, v, out);
        unsigned char* const q = arm + 0xC + 0x10 * i;
        SetUL(q, UL(arm) + static_cast<U>(out[0]));
        SetUL(q + 4, UL(arm + 4) + static_cast<U>(out[1]));
        SetUL(q + 8, (static_cast<U>(out[2]) << 8) + UL(arm + 8));
    }
}

// original 0x46F690 (E1D's kind 0x21 states call it): a draw mode
// (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, slot 1); EffectGte_LoadMapCamera; the
// triangles of points 0, 1, 2 and 0, 3, 2.
extern "C" void __cdecl EffectKind21_DrawArm(unsigned char* arm) {
    DrawMode(1, 0x3C0, 0, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    SH_CALL(EffectKind21_DrawArmTriangle)(arm, 0, 1, 2);
    SH_CALL(EffectKind21_DrawArmTriangle)(arm, 0, 3, 2);
}

// original 0x46F6F0: a POLY_G3 at the packet cursor, semi-transparent (1);
// its vertices the arm's points a, b, c (bytes; +0xC + 0x10 each) projected;
// the shades 0x40, 0, 0x80; committed to slot 1 (0x34).
extern "C" void __cdecl EffectKind21_DrawArmTriangle(unsigned char* arm, unsigned a, unsigned b, unsigned c) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(EffectGte_ProjectPoint)(Point(arm + 0xC + ((a & 0xFFu) << 4)), Out(p + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(arm + 0xC + ((b & 0xFFu) << 4)), Out(p + 0x18));
    SH_CALL(EffectGte_ProjectPoint)(Point(arm + 0xC + ((c & 0xFFu) << 4)), Out(p + 0x28));
    p[4] = p[5] = p[6] = 0x40;
    p[0x14] = p[0x15] = p[0x16] = 0;
    p[0x24] = p[0x25] = p[0x26] = 0x80;
    SH_CALL(Gfx_CommitPrim)(1, 0x34);
}

// original 0x46FAE0 (E1D's kind 0x24 calls it; PSX twin 0x801F88FC): a draw
// mode (as the arm's); the direction from the two angles (+4, +2: words,
// signed) - dx = cos(+4) sin(+2) sar 8, dz = sin(+4) sin(+2) sar 8, the height
// step cos(+2) << 4; EffectGte_LoadMapCamera; four LINE_G2s, semi-transparent,
// the direction turned a quarter each time ((dx, dz) := (-dz, dx)): from the
// leader's point (ObjTrio +0x34 / +0x38 / +0x3C) out by a = (+6 * +0) sar 8
// (its low word, signed) to b = (+8 * +0) sar 8 - x + a dx sar 8, z + a dz sar
// 8, y + a times the height step - each end projected; its colour the scale
// word (+6 or +8) times the bytes +0xA, +0xB, +0xC, sar 8; committed to slot 1
// (0x24).
extern "C" void __cdecl EffectKind24_DrawRay(const unsigned char* ray) {
    DrawMode(1, 0x3C0, 0, 1, 1);
    const U cos4 = static_cast<U>(SH_CALL(Math_Cos)(SW(ray + 4)));
    const U sin2 = static_cast<U>(SH_CALL(Math_Sin)(SW(ray + 2)));
    U dx = Sar(Mul(cos4, sin2), 8);
    const U sin4 = static_cast<U>(SH_CALL(Math_Sin)(SW(ray + 4)));
    const U sin2b = static_cast<U>(SH_CALL(Math_Sin)(SW(ray + 2)));
    U dz = Sar(Mul(sin4, sin2b), 8);
    const U lift = static_cast<U>(SH_CALL(Math_Cos)(SW(ray + 2))) << 4;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned n = 4; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineG2)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        alignas(4) unsigned char pt[12];
        U a = static_cast<U>(static_cast<std::int16_t>(Sar(Mul(static_cast<U>(SW(ray + 6)), static_cast<U>(SW(ray))), 8)));
        SetUL(pt, Sar(Mul(a, dx), 8) + UL(at::kLeaderX));
        SetUL(pt + 4, Sar(Mul(a, dz), 8) + UL(at::kLeaderZ));
        SetUL(pt + 8, Mul(a, lift) + UL(at::kLeaderY));
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(p + 8));
        p[4] = static_cast<unsigned char>((Word(ray + 6) * ray[0xA]) >> 8);
        p[5] = static_cast<unsigned char>((Word(ray + 6) * ray[0xB]) >> 8);
        p[6] = static_cast<unsigned char>((Word(ray + 6) * ray[0xC]) >> 8);
        a = static_cast<U>(static_cast<std::int16_t>(Sar(Mul(static_cast<U>(SW(ray + 8)), static_cast<U>(SW(ray))), 8)));
        SetUL(pt, Sar(Mul(a, dx), 8) + UL(at::kLeaderX));
        SetUL(pt + 4, Sar(Mul(a, dz), 8) + UL(at::kLeaderZ));
        SetUL(pt + 8, Mul(a, lift) + UL(at::kLeaderY));
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(p + 0x18));
        p[0x14] = static_cast<unsigned char>((Word(ray + 8) * ray[0xA]) >> 8);
        p[0x15] = static_cast<unsigned char>((Word(ray + 8) * ray[0xB]) >> 8);
        p[0x16] = static_cast<unsigned char>((Word(ray + 8) * ray[0xC]) >> 8);
        SH_CALL(Gfx_CommitPrim)(1, 0x24);
        const U old = dx;
        dx = 0u - dz;
        dz = old;
    }
}

// ===========================================================================
// The glow sparks: 8 records of 0x1C at EffectKind30_Shards (+0 in use, +1 the
// state, +2 the count, +3 the shade, +4 the reload, +8 the rise speed, +0xC
// the point). Kinds 0x48 and 0x49 (E2D, E2E) and kind 0x52 (E2F) use them.
// ===========================================================================

unsigned char* Spark(unsigned i) { return EffectKind30_Shards + i * at::kSparkStride; }

// original 0x4790C0: +0 of each of the 8 sparks = 0; the two sound flags
// 0x6761C9 and 0x6761C8 = 0.
extern "C" void __cdecl EffectGlowSparks_Clear(void) {
    for (unsigned i = 0; i < at::kSparks; ++i) Spark(i)[0] = 0;
    B(at::kGlowSounded) = 0;
    B(at::kRiseSounded) = 0;
}

// original 0x4790F0: a spark set going near the record: +0 = 1, +1 = 0, +2 =
// 8, +3 = 0, +8 = 0, +4 = 0x40; x = ((Rand & 0xFF) - 0x80) << 11 + the
// record's x, z likewise (Sprite_Current read after each Rand), the height the
// record's +0x3C.
extern "C" void __cdecl EffectGlowSparks_StartRise(unsigned char* spark) {
    spark[0] = 1;
    spark[1] = 0;
    spark[2] = 8;
    spark[3] = 0;
    SetUL(spark + 8, 0);
    spark[4] = 0x40;
    U r = static_cast<U>(SH_CALL(Rand)());
    SetUL(spark + 0xC, (((r & 0xFFu) - 0x80u) << 11) + UL(S() + 0x34));
    r = static_cast<U>(SH_CALL(Rand)());
    SetUL(spark + 0x10, (((r & 0xFFu) - 0x80u) << 11) + UL(S() + 0x38));
    SetUL(spark + 0x14, UL(S() + 0x3C));
}

// original 0x479160: a spark set on a sphere round the record: +0 = 1, +1 =
// 0, +2 = 8, +3 = 0, +8 = 0, +4 = 1; a = Rand & 0x7FF, b = Rand & 0xFFF, the
// radius r = the low word, signed, of ((Rand % 2, signed) << 12 | Rand &
// 0xFFF); x = (sin a cos b sar 12) r sar 8 + the record's x, z = (sin a sin b
// sar 12) r sar 8 + its z, the height cos a * r + its +0x3C + 0x2000000
// (Sprite_Current read for each).
extern "C" void __cdecl EffectGlowSparks_StartBurst(unsigned char* spark) {
    spark[0] = 1;
    spark[1] = 0;
    spark[2] = 8;
    spark[3] = 0;
    SetUL(spark + 8, 0);
    spark[4] = 1;
    const U a = static_cast<U>(SH_CALL(Rand)()) & 0x7FFu;
    const U b = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const std::int32_t half = static_cast<std::int32_t>(SH_CALL(Rand)()) % 2;
    const U low = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U r = static_cast<U>(static_cast<std::int16_t>((static_cast<U>(half) << 12) | low));
    const U sa = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
    const U cb = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(b)));
    U v = Sar(Mul(sa, cb), 12);
    SetUL(spark + 0xC, Sar(Mul(v, r), 8) + UL(S() + 0x34));
    const U sa2 = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
    const U sb = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(b)));
    v = Sar(Mul(sa2, sb), 12);
    SetUL(spark + 0x10, Sar(Mul(v, r), 8) + UL(S() + 0x38));
    const U ca = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
    SetUL(spark + 0x14, Mul(ca, r) + UL(S() + 0x3C) + 0x2000000u);
}

// original 0x479260 (E2F's EffectKind52_MoveSparks instruction for
// instruction but its table): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd
// 1, slot 1); EffectGte_LoadMapCamera; each spark in use called through
// EffectGlowSparks_States by its +1 (unbounded; each handed the spark) and
// drawn (EffectGlowSparks_Draw). Answers 1 when a spark was in use, else 0.
extern "C" unsigned char __cdecl EffectGlowSparks_Run(void) {
    DrawMode(1, 0x3C0, 0, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (unsigned i = 0; i < at::kSparks; ++i) {
        unsigned char* const k = Spark(i);
        if (k[0] == 0) continue;
        const unsigned state = k[1];
        if (state >= EffectGlowSparks_States_count)
            bof3::Fatal("EffectGlowSparks_Run: spark %u's state +1 is %u, past the %u entries of EffectGlowSparks_States - "
                        "the original calls through the dword after (docs/rest_3e.md section 7)",
                        i, state, EffectGlowSparks_States_count);
        reinterpret_cast<void (__cdecl*)(unsigned char*)>(
            static_cast<std::uintptr_t>(UL(reinterpret_cast<const unsigned char*>(EffectGlowSparks_States) + 4 * state)))(k);
        SH_CALL(EffectGlowSparks_Draw)(k);
        any = 1;
    }
    return any;
}

// original 0x4792E0 (E3A's EffectKind68_DrawMote but its size 0x20 and slot
// 1): the spark's point +0xC projected (o) and its size {0x20, a word the
// original never writes} projected (EffectGte_ProjectSize reads the first); r
// = that size's first word + (Frame_Counter & 1), its low word signed; a disc
// of 16 semi-transparent POLY_G3 round o, the centre in the shade +3, the rim
// black at (cos, sin) * r sar 12, 0x100 apart; each committed to slot 1
// (0x34).
extern "C" void __cdecl EffectGlowSparks_Draw(unsigned char* spark) {
    alignas(4) short sz[2] = {0x20, 0};
    alignas(4) short r[2];
    SH_CALL(EffectGte_ProjectSize)(Point(spark + 0xC), sz, r);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(spark + 0xC), Out(o));
    const U radius = static_cast<U>(static_cast<std::int16_t>(static_cast<U>(static_cast<unsigned short>(r[0])) + (Frame_Counter & 1u)));
    U angle = 0;
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(o));
        SetUL(p + 0xC, UL(o + 4));
        U d = Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))), radius), 12);
        StF(p + 0x18, I(static_cast<std::int32_t>(d)) + F(o));
        d = Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))), radius), 12);
        angle += 0x100;
        StF(p + 0x1C, I(static_cast<std::int32_t>(d)) + F(o + 4));
        d = Sar(Mul(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))), radius), 12);
        StF(p + 0x28, I(static_cast<std::int32_t>(d)) + F(o));
        d = Sar(Mul(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))), radius), 12);
        StF(p + 0x2C, I(static_cast<std::int32_t>(d)) + F(o + 4));
        SetUL(p + 0x30, UL(o + 8));
        SetUL(p + 0x20, UL(o + 8));
        SetUL(p + 0x10, UL(o + 8));
        const unsigned char shade = spark[3];
        p[6] = shade;
        p[5] = shade;
        p[4] = shade;
        p[0x26] = p[0x25] = p[0x24] = 0;
        p[0x16] = p[0x15] = p[0x14] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// original 0x479420 (EffectGlowSparks_States[0]): the shade +3 up 6; the count
// +2 down, at 0 the count its reload +4, +1 up, and sound 0x200 the first time
// (the flag 0x6761C9).
extern "C" void __cdecl EffectGlowSparks_Glow(unsigned char* spark) {
    spark[3] = static_cast<unsigned char>(spark[3] + 6);
    spark[2] = static_cast<unsigned char>(spark[2] - 1);
    if (spark[2] != 0) return;
    spark[2] = spark[4];
    spark[1] = static_cast<unsigned char>(spark[1] + 1);
    if (B(at::kGlowSounded) != 0) return;
    SH_CALL(Sound_PlayEffect)(at::kSoundGlow);
    B(at::kGlowSounded) = 1;
}

// original 0x479470 (EffectGlowSparks_States[2]): the first time (the flag
// 0x6761C8) sound 0x201 unless the record's +6 is set, the flag set either way;
// then the rise speed +8 up 0x40000, the height +0x14 up by it, the shade +3
// up 6, the count +2 down, at 0 the spark freed (+0 = 0).
extern "C" void __cdecl EffectGlowSparks_Rise(unsigned char* spark) {
    if (B(at::kRiseSounded) == 0) {
        if (S()[6] == 0) SH_CALL(Sound_PlayEffect)(at::kSoundRise);
        B(at::kRiseSounded) = 1;
    }
    const U speed = UL(spark + 8) + 0x40000u;
    SetUL(spark + 8, speed);
    SetUL(spark + 0x14, UL(spark + 0x14) + speed);
    spark[3] = static_cast<unsigned char>(spark[3] + 6);
    spark[2] = static_cast<unsigned char>(spark[2] - 1);
    if (spark[2] == 0) spark[0] = 0;
}

// ===========================================================================
// The glow trail (0x92C060), the spiral (0x92C4A4), the ring
// ===========================================================================

// original 0x4794D0 (0x1D1 bytes): E2F's EffectKind52_TrailUpdate (0x47D040)
// and E3D's EffectKind80_TrailStep (0x4873E0) instruction for instruction but
// the call targets, which are the same callees: called directly (its callees
// go through the harness inside it). docs/effect_2f.md has the body.
extern "C" void __cdecl EffectGlowTrail_Update(unsigned char* trail) { EffectKind52_TrailUpdate(trail); }

// original 0x4796B0 (0x2BE bytes): E2F's EffectKind52_TrailDraw without the
// depths - a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, slot 1); the cap
// at point 0 (EffectTrail_DrawCap at its angle - 0x400, shade 0x80); for each
// pair of points k, k + 1 (screen floats at +0x10) whose x or y differ (equal
// or unordered both = skipped), two Gouraud quads at the packet cursor,
// semi-transparent: the first from point k's screen x, y and x + (cos(a +
// 0x400) w sar 12), y + (sin ..) to point k + 1's likewise (a, w each point's
// angle +0x1C and width +0x1E, zero-extended), shaded the running shade at
// point k's and it - 4 at k + 1's (black at the offset ones), committed to slot
// 1 (0x44); the second a copy of the first (0x44 bytes from the cursor back)
// with the offset vertices at a - 0x400, committed; the running shade down 4.
// The depth words of the quads are not written. The cap at point 31 at its
// angle + 0x400 and the running shade.
extern "C" void __cdecl EffectGlowTrail_Draw(unsigned char* trail) {
    DrawMode(1, 0x3C0, 0, 1, 1);
    unsigned char shade = 0x80;
    SH_CALL(EffectTrail_DrawCap)(trail + 0x10, Word(trail + 0x1E), (Word(trail + 0x1C) - 0x400u) & 0xFFFFu, 0x80);
    for (unsigned k = 0; k < 0x1F; ++k) {
        const unsigned char* const p = trail + 0x10 + 0x20 * k;
        const unsigned char* const q = p + 0x20;
        if (EqualOrUnordered(p, q) && EqualOrUnordered(p + 4, q + 4)) continue;
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SetUL(prim + 8, UL(p));
        SetUL(prim + 0xC, UL(p + 4));
        U v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(p + 0xC) + 0x400u)), p);
        StF(prim + 0x18, I(static_cast<std::int32_t>(v)) + F(p));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(p + 0xC) + 0x400u)), p);
        StF(prim + 0x1C, I(static_cast<std::int32_t>(v)) + F(p + 4));
        SetUL(prim + 0x28, UL(q));
        SetUL(prim + 0x2C, UL(q + 4));
        v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(q + 0xC) + 0x400u)), q);
        StF(prim + 0x38, I(static_cast<std::int32_t>(v)) + F(q));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(q + 0xC) + 0x400u)), q);
        const auto next = static_cast<unsigned char>(shade - 4);
        prim[6] = prim[5] = prim[4] = shade;
        prim[0x16] = prim[0x15] = prim[0x14] = 0;
        StF(prim + 0x3C, I(static_cast<std::int32_t>(v)) + F(q + 4));
        prim[0x26] = prim[0x25] = prim[0x24] = next;
        prim[0x36] = prim[0x35] = prim[0x34] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        unsigned char* const copy = Gfx_PacketNext;
        std::memmove(copy, copy - 0x44, 0x44);
        v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(p + 0xC) - 0x400u)), p);
        StF(copy + 0x18, I(static_cast<std::int32_t>(v)) + F(p));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(p + 0xC) - 0x400u)), p);
        StF(copy + 0x1C, I(static_cast<std::int32_t>(v)) + F(p + 4));
        v = Turn(SH_CALL(Math_Cos)(static_cast<int>(Word(q + 0xC) - 0x400u)), q);
        StF(copy + 0x38, I(static_cast<std::int32_t>(v)) + F(q));
        v = Turn(SH_CALL(Math_Sin)(static_cast<int>(Word(q + 0xC) - 0x400u)), q);
        StF(copy + 0x3C, I(static_cast<std::int32_t>(v)) + F(q + 4));
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        shade = next;
    }
    SH_CALL(EffectTrail_DrawCap)(trail + 0x3F0, Word(trail + 0x3FE), (Word(trail + 0x3FC) + 0x400u) & 0xFFFFu, shade);
}

// The spiral record (0xD20 bytes): +0..+8 its centre; eight rings of 32 screen
// points (12 bytes each) from +0x10, 0x180 a ring; eight rings of 32 shade
// bytes from +0xC10; the tilt word +0xD10; a light vector of three longs at
// +0xD14.
namespace {
constexpr U kRing = 0x180;
// Ring 0's point i: angle i * 0x80; with the tilt t: a = sin t cos i sar 12,
// b = sin t sin i sar 12, c = cos t; the shade from the light (a * l0 + b * l1
// + c * l2, summed as the original: l1 b + c l2, then + a l0): at most 0 the
// `dark` value, else ((sqrt, at most 0xFFF) sar 5 & 0x78) + 7; the point (a <<
// 5 + x, b << 5 + z, c << 13 + y) projected.
void SpiralPoint(unsigned char* spiral, unsigned i, unsigned char dark, unsigned char* shade, unsigned char* screen) {
    const int angle = static_cast<int>((i * 0x80u) & 0xFFFFu);
    U a = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(Word(spiral + 0xD10))));
    a = Sar(Mul(a, static_cast<U>(SH_CALL(Math_Cos)(angle))), 12);
    U b = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(Word(spiral + 0xD10))));
    b = Sar(Mul(b, static_cast<U>(SH_CALL(Math_Sin)(angle))), 12);
    const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(Word(spiral + 0xD10))));
    const U dot = Mul(UL(spiral + 0xD18), b) + Mul(c, UL(spiral + 0xD1C)) + Mul(a, UL(spiral + 0xD14));
    if (static_cast<std::int32_t>(dot) <= 0) {
        *shade = dark;
    } else {
        std::int32_t root = static_cast<std::int32_t>(SH_AT(long (__cdecl*)(long), at::kSqrt)(static_cast<long>(dot)));
        if (root > 0xFFF) root = 0xFFF;
        *shade = static_cast<unsigned char>(((static_cast<U>(root) >> 5) & 0x78u) + 7u);
    }
    alignas(4) unsigned char pt[12];
    SetUL(pt, (a << 5) + UL(spiral));
    SetUL(pt + 4, (b << 5) + UL(spiral + 4));
    SetUL(pt + 8, (c << 13) + UL(spiral + 8));
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(screen));
}
}  // namespace

// original 0x4799C0 (E2E's kind 0x48 states 8 and 10 call it): set up.
// EffectGte_LoadMapCamera; the light +0xD14 = (0, 0x1000, 0x1000) normalised in
// place (Gte_VectorNormal); ring 0's 32 points and shades (SpiralPoint, the
// dark shade 0x70), each copied to rings 1..7 at once (the point, then the
// shade).
extern "C" void __cdecl EffectSpiral_Init(unsigned char* spiral) {
    SH_CALL(EffectGte_LoadMapCamera)();
    SetUL(spiral + 0xD18, 0x1000);
    SetUL(spiral + 0xD14, 0);
    SetUL(spiral + 0xD1C, 0x1000);
    SH_CALL(Gte_VectorNormal)(Point(spiral + 0xD14), reinterpret_cast<long*>(spiral + 0xD14));
    for (unsigned i = 0; i < 0x20; ++i) {
        unsigned char* const shade = spiral + 0xC10 + i;
        unsigned char* const screen = spiral + 0x10 + 12 * i;
        SpiralPoint(spiral, i, 0x70, shade, screen);
        for (unsigned r = 1; r < 8; ++r) {
            Copy12(screen + kRing * r, screen);
            shade[0x20 * r] = *shade;
        }
    }
}

// original 0x479B70 (E2E's kind 0x48 states 8, 9 and 10 call it; PSX twin
// 0x801F9330): the rings' points moved out one (ring 6 to 7 .. 0 to 1; the
// shades stay); EffectGte_LoadMapCamera; ring 0 again (the dark shade 7); a
// draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, slot 1); for ring k = 1..7
// and point i of 32 (n = i + 1 mod 32): a POLY_G4 at the packet cursor,
// semi-transparent - ring k - 1's points i and n, ring k's points i and n -
// shaded ring k - 1's shades i and n times (8 - k) / 7 and ring k's shade i
// (twice: the original reads i for the fourth vertex too) times (7 - k) / 7;
// committed to slot 1 (0x44).
extern "C" void __cdecl EffectSpiral_StepDraw(unsigned char* spiral) {
    for (unsigned r = 7; r != 0; --r)
        for (unsigned i = 0; i < 0x20; ++i) Copy12(spiral + 0x10 + kRing * r + 12 * i, spiral + 0x10 + kRing * (r - 1) + 12 * i);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < 0x20; ++i) SpiralPoint(spiral, i, 7, spiral + 0xC10 + i, spiral + 0x10 + 12 * i);
    DrawMode(1, 0x3C0, 0, 1, 1);
    for (unsigned k = 1; k < 8; ++k) {
        const std::int32_t outer = static_cast<std::int32_t>(8 - k), inner = static_cast<std::int32_t>(7 - k);
        for (unsigned i = 0; i < 0x20; ++i) {
            const unsigned n = (i + 1) & 0x1F;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            Copy12(p + 8, spiral + 0x10 + kRing * (k - 1) + 12 * i);
            Copy12(p + 0x18, spiral + 0x10 + kRing * (k - 1) + 12 * n);
            p[4] = p[5] = p[6] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * (k - 1) + i] * outer / 7);
            p[0x14] = p[0x15] = p[0x16] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * (k - 1) + n] * outer / 7);
            Copy12(p + 0x28, spiral + 0x10 + kRing * k + 12 * i);
            Copy12(p + 0x38, spiral + 0x10 + kRing * k + 12 * n);
            p[0x24] = p[0x25] = p[0x26] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * k + i] * inner / 7);
            p[0x34] = p[0x35] = p[0x36] = static_cast<unsigned char>(spiral[0xC10 + 0x20 * k + i] * inner / 7);
            SH_CALL(Gfx_CommitPrim)(1, 0x44);
        }
    }
}

// original 0x479EE0 (E2E's kind 0x48 state 7 calls it): a ring of 32
// semi-transparent POLY_G4 round the ring record's point (+0, +4, +8): the
// rim at angle t is (x + cos t << 5, z + sin t << 5) at the height and at the
// height + 0x7000000, each projected; quad i joins the rim at t = 0x80 i to t
// = 0x80 (i + 1). Before each quad a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 1) linked at the new rim point (MapView_LinkPrimAt(x, z, 0, 0xC)); the
// quad linked there too (0x44). The shade: the old pair's the last quad's new
// one (the first: min(+0x10, 0xFF)); the new pair's min((s * +0x10, 16 bits)
// >> 7, 0xFF), s a running byte from 0x80 down 4 for quads 4..19, else up 4.
namespace {
// The ring's rim at angle t: x + cos t << 5, z + sin t << 5, the height (each
// read after its call).
void RimPoint(const unsigned char* ring, int t, unsigned char* pt) {
    const U cos = static_cast<U>(SH_CALL(Math_Cos)(t));
    SetUL(pt, (cos << 5) + UL(ring));
    const U sin = static_cast<U>(SH_CALL(Math_Sin)(t));
    SetUL(pt + 4, (sin << 5) + UL(ring + 4));
    SetUL(pt + 8, UL(ring + 8));
}
}  // namespace

extern "C" void __cdecl EffectRing_Draw(const unsigned char* ring) {
    alignas(4) unsigned char pt[12];
    alignas(4) unsigned char bottom[12], top[12];
    int angle = 0;
    RimPoint(ring, angle, pt);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(bottom));
    SetUL(pt + 8, UL(ring + 8) + 0x7000000u);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(top));
    unsigned char s = 0x80;
    unsigned shade = Word(ring + 0x10);
    if (shade > 0xFF) shade = 0xFF;
    for (unsigned i = 0; i < 0x20; ++i) {
        angle += 0x80;
        const int t = angle & 0xFFFF;
        RimPoint(ring, t, pt);
        const U tp = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tp & 0xFFFFu, 0);
        SH_CALL(MapView_LinkPrimAt)(UL(pt), UL(pt + 4), 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, bottom);
        Copy12(p + 0x18, top);
        p[4] = p[5] = p[6] = static_cast<unsigned char>(shade);
        p[0x14] = p[0x15] = p[0x16] = static_cast<unsigned char>(shade);
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(bottom));
        SetUL(pt + 8, UL(ring + 8) + 0x7000000u);
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(top));
        Copy12(p + 0x28, bottom);
        Copy12(p + 0x38, top);
        if (i >= 4 && i < 0x14)
            s = static_cast<unsigned char>(s - 4);
        else
            s = static_cast<unsigned char>(s + 4);
        shade = ((static_cast<unsigned>(s) * Word(ring + 0x10)) & 0xFFFFu) >> 7;
        if (shade > 0xFF) shade = 0xFF;
        p[0x24] = p[0x25] = p[0x26] = static_cast<unsigned char>(shade);
        p[0x34] = p[0x35] = p[0x36] = static_cast<unsigned char>(shade);
        SH_CALL(MapView_LinkPrimAt)(UL(pt), UL(pt + 4), 0, 0x44);
    }
}

// ===========================================================================
// The dust: 64 records of 0x20 at 0x92D1DC (+0 in use, +2 the count, +4 x, +8
// z, +0xC the height, +0x14 the spread, +0x18 the rise, +0x1C the ground).
// Kind 0x49's variant 3 (E2D) uses them.
// ===========================================================================

unsigned char* Dust(unsigned i) { return At(at::kDust + i * at::kDustStride); }

// original 0x47A110: +0 of each of the 64 = 0.
extern "C" void __cdecl EffectDust_Clear(void) {
    for (unsigned i = 0; i < at::kDustCount; ++i) Dust(i)[0] = 0;
}

// original 0x47A130: the first of the 64 whose +0 is 0, or null.
extern "C" unsigned char* __cdecl EffectDust_FindFree(void) {
    for (unsigned i = 0; i < at::kDustCount; ++i)
        if (Dust(i)[0] == 0) return Dust(i);
    return nullptr;
}

// original 0x47A150: a dust record set round Sprite_Current's point: +0 = 1,
// +1 = 0, +2 = 0x10, +0x18 = +0x14 = 0; the radius ((Rand % 2, signed) << 8 |
// Rand & 0xFF) << 8, the angle Rand & 0xFC0; x = cos * radius sar 12 + the
// record's x, z = sin * radius sar 12 + its z (Sprite_Current read for each);
// the height and the ground AreaMap_Elevation(x, z) << 16 (its low word,
// signed).
extern "C" void __cdecl EffectDust_Start(unsigned char* dust) {
    dust[0] = 1;
    dust[1] = 0;
    dust[2] = 0x10;
    SetUL(dust + 0x18, 0);
    SetUL(dust + 0x14, 0);
    const std::int32_t half = static_cast<std::int32_t>(SH_CALL(Rand)()) % 2;
    const U low = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
    const U radius = ((static_cast<U>(half) << 8) | low) << 8;
    const int angle = static_cast<int>(static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    const U cos = static_cast<U>(SH_CALL(Math_Cos)(angle));
    SetUL(dust + 4, Sar(Mul(cos, radius), 12) + UL(S() + 0x34));
    const U sin = static_cast<U>(SH_CALL(Math_Sin)(angle));
    const U z = Sar(Mul(sin, radius), 12) + UL(S() + 0x38);
    SetUL(dust + 8, z);
    const U ground = static_cast<U>(static_cast<std::int16_t>(SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(dust + 4)), static_cast<long>(z)))) << 16;
    SetUL(dust + 0x1C, ground);
    SetUL(dust + 0xC, ground);
}

// original 0x47A200 (E2D's kind 0x49 variant 3 calls it; PSX twin 0x801F9EFC):
// a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1, slot 1);
// EffectGte_LoadMapCamera; each dust record in use: its column drawn
// (EffectDust_DrawColumn), its fan when the column answered 1; then the rise
// +0x18 up 0x100000, the height +0xC up by it, the spread +0x14 up 0x2000, the
// count +2 down, at 0 the record freed. Answers 1 when one was in use.
extern "C" unsigned char __cdecl EffectDust_Run(void) {
    DrawMode(1, 0x3C0, 0, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    for (unsigned i = 0; i < at::kDustCount; ++i) {
        unsigned char* const d = Dust(i);
        if (d[0] == 0) continue;
        if (SH_CALL(EffectDust_DrawColumn)(d) != 0) SH_CALL(EffectDust_DrawFan)(d);
        const U rise = UL(d + 0x18) + 0x100000u;
        SetUL(d + 0x14, UL(d + 0x14) + 0x2000u);
        SetUL(d + 0x18, rise);
        SetUL(d + 0xC, UL(d + 0xC) + rise);
        d[2] = static_cast<unsigned char>(d[2] - 1);
        if (d[2] == 0) d[0] = 0;
        any = 1;
    }
    return any;
}

// original 0x47A2B0 (PSX twin 0x801F9A80): a POLY_F4 at the packet cursor,
// semi-transparent, colour 0x40: the top point (x, z, height + spread << 8)
// and the bottom (x, z, height - spread << 8, raised to the ground +0x1C when
// below it - the answer 1 then, else 0), each projected; each end's two
// vertices its screen x -/+ the float 0x5C41B8, its y and depth; committed to
// slot 1 (0x38).
extern "C" unsigned char __cdecl EffectDust_DrawColumn(const unsigned char* dust) {
    unsigned char clamped = 0;
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    alignas(4) unsigned char pt[12], sc[12];
    SetUL(pt, UL(dust + 4));
    SetUL(pt + 4, UL(dust + 8));
    SetUL(pt + 8, (UL(dust + 0x14) << 8) + UL(dust + 0xC));
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(sc));
    const unsigned char* const k = At(at::kHalfWidth);
    StF(p + 8, F(sc) - F(k));
    SetUL(p + 0xC, UL(sc + 4));
    StF(p + 0x14, F(sc) + F(k));
    SetUL(p + 0x18, UL(sc + 4));
    SetUL(p + 0x1C, UL(sc + 8));
    SetUL(p + 0x10, UL(sc + 8));
    SetUL(pt, UL(dust + 4));
    SetUL(pt + 4, UL(dust + 8));
    U low = UL(dust + 0xC) - (UL(dust + 0x14) << 8);
    const U ground = UL(dust + 0x1C);
    if (static_cast<std::int32_t>(low) < static_cast<std::int32_t>(ground)) {
        low = ground;
        clamped = 1;
    }
    SetUL(pt + 8, low);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(sc));
    StF(p + 0x20, F(sc) - F(k));
    SetUL(p + 0x24, UL(sc + 4));
    StF(p + 0x2C, F(sc) + F(k));
    SetUL(p + 0x30, UL(sc + 4));
    SetUL(p + 0x34, UL(sc + 8));
    SetUL(p + 0x28, UL(sc + 8));
    p[4] = p[5] = p[6] = 0x40;
    SH_CALL(Gfx_CommitPrim)(1, 0x38);
    return clamped;
}

// original 0x47A3D0 (PSX twin 0x801F9BDC): a fan of 16 semi-transparent
// POLY_G3 on the ground round the dust's point: the centre (x, z, the ground
// AreaMap_Elevation(x, z) << 16) projected, shade 0x40; the rim at angle t
// (x + cos t * 4, z + sin t * 4) at the height AreaMap_Elevation(x, z) << 16 -
// read again at the dust's own point for each, not at the rim - projected,
// black; triangle j joins the rim at t = 0x100 j and 0x100 (j + 1); each
// committed to slot 1 (0x34).
extern "C" void __cdecl EffectDust_DrawFan(const unsigned char* dust) {
    alignas(4) unsigned char pt[12], centre[12], rim[12];
    SetUL(pt, UL(dust + 4));
    SetUL(pt + 4, UL(dust + 8));
    SetUL(pt + 8, static_cast<U>(static_cast<std::int16_t>(
                      SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(dust + 4)), static_cast<long>(UL(dust + 8)))))
                      << 16);
    SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(centre));
    U angle = 0;
    // the rim point at angle t: x, z, then the height
    auto rimAt = [&](int t) {
        const U cos = static_cast<U>(SH_CALL(Math_Cos)(t));
        SetUL(pt, UL(dust + 4) + (cos << 2));
        const U sin = static_cast<U>(SH_CALL(Math_Sin)(t));
        SetUL(pt + 4, UL(dust + 8) + (sin << 2));
        SetUL(pt + 8, static_cast<U>(static_cast<std::int16_t>(
                          SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(dust + 4)), static_cast<long>(UL(dust + 8)))))
                          << 16);
        SH_CALL(EffectGte_ProjectPoint)(Point(pt), Out(rim));
    };
    rimAt(0);
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, centre);
        angle += 0x100;
        Copy12(p + 0x18, rim);
        rimAt(static_cast<int>(angle & 0xFFFFu));
        Copy12(p + 0x28, rim);
        p[4] = p[5] = p[6] = 0x40;
        p[0x14] = p[0x15] = p[0x16] = 0;
        p[0x24] = p[0x25] = p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// ===========================================================================
// Kinds 0x5D and 0x5E (EffectKind5D_States 0..3, EffectKind5E_States 0..2;
// their dispatchers E2G's). Three windows (+7 bit j hides window j) open,
// show the item +6 and its count and a board of marks, and close; kind 0x5E
// picks one of the eight items 0x4E..0x55 (+2) and adds it to the counts at
// 0x903A10.
// ===========================================================================

namespace {
unsigned char* Window(unsigned j) { return At(at::kWindows + 8 * j); }

// The three windows' outlines at the size the step +9 gives (n = +9 * 16, at
// most each window's w and h); counted: how many of the six sides were at
// their full size.
unsigned Outlines() {
    unsigned full = 0;
    for (unsigned j = 0; j < 3; ++j) {
        const unsigned char* const w = Window(j);
        const unsigned n = (static_cast<unsigned>(S()[9]) << 4) & 0xFFFFu;
        unsigned width = n, height = n;
        if (Word(w + 4) <= n) {
            width = Word(w + 4);
            ++full;
        }
        if (Word(w + 6) <= n) {
            height = Word(w + 6);
            ++full;
        }
        if (((S()[7] >> j) & 1) == 0) SH_CALL(Window_DrawOutline)(Word(w), Word(w + 2), static_cast<int>(width), static_cast<int>(height));
    }
    return full;
}
}  // namespace

// original 0x47F2D0 (EffectKind5D_States[0]; PSX twin 0x801F6FAC):
// Sprite_SetAnimationBank(0x1EA); +0x3E = 0x64 (a word), +0x48, +0x24, +0x2A,
// +9 = 0, +0xA = 0x1E, +0x29 = 1; Sprite_SetAnimation(+6 - 0x4E); +1 = 1.
extern "C" void __cdecl EffectKind5D_Start(void) {
    SH_CALL(Sprite_SetAnimationBank)(0x1EA);
    SetWord(S() + 0x3E, 0x64);
    S()[0x48] = 0;
    S()[0x24] = 0;
    S()[0x2A] = 0;
    S()[9] = 0;
    S()[0xA] = 0x1E;
    S()[0x29] = 1;
    SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(S()[6] - 0x4E));
    S()[1] = 1;
}

// original 0x47F340 (EffectKind5D_States[1]; PSX twin 0x801F7068): the three
// windows' outlines growing; +9 up; when all six sides were at full size, +1 =
// 2.
extern "C" void __cdecl EffectKind5D_Open(void) {
    const unsigned full = Outlines();
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (full == 6) S()[1] = 2;
}

// original 0x47F3E0 (EffectKind5D_States[2]; PSX twin 0x801F719C): the shown
// windows' frames (Window_DrawFrame); window 0 shown: the sprite placed at
// its corner (+0x2E = 0x654890's byte + x, +0x30 = 0x654891's + y),
// Sprite_ScriptTick, Sprite_QueueOverlay; window 1 shown: the item +6's name
// at (x + 4, y + 2) of window 1 and its count (Inventory_Count(0, +6, 0)) at
// (x + 0x48, y + 2); the board (EffectKind5D_DrawBoard at the board frame's
// corner + 1, 0x80 x 0x68). Then +0xA down when not 0; at 0, any held button
// sets +1 = 3.
extern "C" void __cdecl EffectKind5D_Show(void) {
    for (unsigned j = 0; j < 3; ++j) {
        const unsigned char* const w = Window(j);
        if (((S()[7] >> j) & 1) == 0) SH_CALL(Window_DrawFrame)(Word(w), Word(w + 2), w[4], w[6]);
    }
    if ((S()[7] & 1) == 0) {
        SetWord(S() + 0x2E, B(at::kSpriteDx) + UL(at::kWindows));
        SetWord(S() + 0x30, B(at::kSpriteDy) + W(at::kWindows + 2));
        SH_CALL(Sprite_ScriptTick)();
        SH_CALL(Sprite_QueueOverlay)();
    }
    if ((S()[7] & 2) == 0) {
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, S()[6]);
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kText)), reinterpret_cast<const char*>(At(at::kStringFormat)), name);
        SH_CALL(Text_DrawAt)(static_cast<int>(UL(at::kWindows + 8) + 4), static_cast<int>((W(at::kWindows + 0xA) + 2) & 0xFFFFu), 0, 8, At(at::kText));
        const unsigned count = SH_CALL(Inventory_Count)(0, S()[6], 0);
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kText)), reinterpret_cast<const char*>(At(at::kCountFormat)), count & 0xFFFFu);
        SH_CALL(Text_DrawAt)(static_cast<int>(UL(at::kWindows + 8) + 0x48), static_cast<int>((W(at::kWindows + 0xA) + 2) & 0xFFFFu), 0, 2, At(at::kText));
    }
    SH_CALL(EffectKind5D_DrawBoard)(static_cast<int>(UL(at::kBoard) + 1), static_cast<int>((W(at::kBoard + 2) + 1) & 0xFFFFu), 0x80, 0x68);
    unsigned char* const s = S();
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        return;
    }
    if (Input_Held != 0) s[1] = 3;
}

// original 0x47F550 (EffectKind5D_States[3]; PSX twin 0x801F73EC): the three
// windows' outlines shrinking; +9 down; at 0, +1 = 4 (Effect_StateRelease).
extern "C" void __cdecl EffectKind5D_Close(void) {
    Outlines();
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    if (S()[9] == 0) S()[1] = 4;
}

// original 0x47F5F0 (EffectKind5E_States[0]; PSX twin 0x801F755C): +6 = 0x4E,
// kind 0x5D's start (EffectKind5D_Start), +2 = 0.
extern "C" void __cdecl EffectKind5E_Start(void) {
    S()[6] = 0x4E;
    SH_CALL(EffectKind5D_Start)();
    S()[2] = 0;
}

// original 0x47F610 (EffectKind5E_States[1]; PSX twin 0x801F7590): the menu
// drawn (EffectKind5E_DrawMenu); Input_Pressed's 0x1000 / 0x4000: the pick +2
// down / up, & 7, sound 0x206, +6 = 0x4E + the pick, Sprite_SetAnimation(the
// pick). Then by the low byte (read again after a move): 0x10 sound 0x209,
// +0xA = 0x14, +1 = 2; else 0x40 sound 0x20C, +1 = 3; else 0x20: the item
// +6 held (Inventory_Count(0, +6, 0) not 0) - the count 0x903A10[+2] up, held
// at 100, Inventory_Remove(0, +6, 1), sound 0x20A - or sound 0x20D.
extern "C" void __cdecl EffectKind5E_Pick(void) {
    SH_CALL(EffectKind5E_DrawMenu)();
    U pressed = UL(At(AddressOf(&Input_Pressed)));
    if (pressed & 0x5000u) {
        unsigned char* s = S();
        s[2] = static_cast<unsigned char>((pressed & 0x1000u) ? s[2] - 1 : s[2] + 1);
        S()[2] &= 7;
        SH_CALL(Sound_PlayEffect)(0x206);
        s = S();
        s[6] = static_cast<unsigned char>((s[2] & 7) + 0x4E);
        SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(S()[6] - 0x4E));
        pressed = UL(At(AddressOf(&Input_Pressed)));
    }
    if (pressed & 0x10u) {
        SH_CALL(Sound_PlayEffect)(0x209);
        S()[0xA] = 0x14;
        S()[1] = 2;
        return;
    }
    if (pressed & 0x40u) {
        SH_CALL(Sound_PlayEffect)(0x20C);
        S()[1] = 3;
        return;
    }
    if ((pressed & 0x20u) == 0) return;
    if ((SH_CALL(Inventory_Count)(0, S()[6], 0) & 0xFFFFu) == 0) {
        SH_CALL(Sound_PlayEffect)(0x20D);
        return;
    }
    const unsigned char* const s = S();
    unsigned char& count = B(at::kCounts + s[2]);
    if (count < 100) count = static_cast<unsigned char>(count + 1);
    SH_CALL(Inventory_Remove)(0, s[6], 1);
    SH_CALL(Sound_PlayEffect)(0x20A);
}

// original 0x47F720 (EffectKind5E_States[2]; PSX twin 0x801F7758): the board's
// frame (Window_DrawFrame) and the board (EffectKind5D_DrawBoard at its corner
// + 1, 0x80 x 0x68); +0xA down when not 0; at 0, any pressed button: sound
// 0x20C, +1 = 1.
extern "C" void __cdecl EffectKind5E_Wait(void) {
    SH_CALL(Window_DrawFrame)(W(at::kBoard), W(at::kBoard + 2), B(at::kBoard + 4), B(at::kBoard + 6));
    SH_CALL(EffectKind5D_DrawBoard)(static_cast<int>(UL(at::kBoard) + 1), static_cast<int>((W(at::kBoard + 2) + 1) & 0xFFFFu), 0x80, 0x68);
    unsigned char* const s = S();
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        return;
    }
    if (Input_Pressed == 0) return;
    SH_CALL(Sound_PlayEffect)(0x20C);
    S()[1] = 1;
}

namespace {
// The FT4 the board and the marks are: page (0, 0, 0x2C0, 0x100), CLUT (clut
// x, 0x1E3), colour 0x80, the corners (x0, y0) .. (x1, y1) as singles.
unsigned char* Quad(int clut_x) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SetWord(p + 0x26, SH_CALL(Gpu_GetTPage)(0, 0, 0x2C0, 0x100));
    SetWord(p + 0x16, SH_CALL(Gpu_GetClut)(clut_x, 0x1E3));
    return p;
}
void Corners(unsigned char* p, std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1) {
    StI(p + 8, x0);
    StI(p + 0xC, y0);
    StI(p + 0x18, x1);
    StI(p + 0x1C, y0);
    StI(p + 0x28, x0);
    StI(p + 0x2C, y1);
    StI(p + 0x38, x1);
    StI(p + 0x3C, y1);
}
}  // namespace

// original 0x47F7A0 (PSX twin 0x801F781C): the board - an FT4 at (x, y) of w x
// h (the low words; CLUT x 0x40), u 0 / 0x80, v 0x98 / 0xFF, committed to slot
// 1 (0x48); then for each of the 8 items its count (0x903A10[i], at most 3)
// marks from its row of the table 0x6548BC (3 bytes a row, 0xA none), mark m
// drawn (EffectKind5D_DrawMark) at (x + its dx, y + its dy) from the table
// 0x6548D4.
extern "C" void __cdecl EffectKind5D_DrawBoard(int x, int y, int w, int h) {
    unsigned char* const p = Quad(0x40);
    const std::int32_t x0 = static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFFu);
    const std::int32_t y0 = static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFFu);
    Corners(p, x0, y0, x0 + static_cast<std::int32_t>(static_cast<U>(w) & 0xFFFFu), y0 + static_cast<std::int32_t>(static_cast<U>(h) & 0xFFFFu));
    p[4] = p[5] = p[6] = 0x80;
    p[0x14] = 0;
    p[0x15] = 0x98;
    p[0x24] = 0x80;
    p[0x25] = 0x98;
    p[0x34] = 0;
    p[0x35] = 0xFF;
    p[0x44] = 0x80;
    p[0x45] = 0xFF;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
    for (unsigned i = 0; i < at::kItems; ++i) {
        unsigned n = B(at::kCounts + i);
        if (n > 3) n = 3;
        for (unsigned k = 0; k < n; ++k) {
            const unsigned m = B(at::kRows + 3 * i + k);
            if (m == 0xA) continue;
            if (m >= at::kMarkCount)
                bof3::Fatal("EffectKind5D_DrawBoard: mark %u of row %u is past the %u the table 0x6548D4 has room for - the "
                            "original reads what follows (docs/rest_3e.md section 7)",
                            m, i, at::kMarkCount);
            SH_CALL(EffectKind5D_DrawMark)(static_cast<int>((W(at::kMarks + 4 * m) + static_cast<U>(x)) & 0xFFFFu),
                                           static_cast<int>((W(at::kMarks + 4 * m + 2) + static_cast<U>(y)) & 0xFFFFu));
        }
    }
}

// original 0x47F910: a mark - a 16 x 16 FT4 at (x, y) (the low words; CLUT x
// 0x30), u 0xB0 / 0xC0, v 0x50 / 0x60, colour 0x80, committed to slot 1
// (0x48).
extern "C" void __cdecl EffectKind5D_DrawMark(int x, int y) {
    unsigned char* const p = Quad(0x30);
    p[4] = p[5] = p[6] = 0x80;
    const std::int32_t x0 = static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFFu);
    const std::int32_t y0 = static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFFu);
    Corners(p, x0, y0, x0 + 0x10, y0 + 0x10);
    p[0x14] = 0xB0;
    p[0x34] = 0xB0;
    p[0x24] = 0xC0;
    p[0x44] = 0xC0;
    p[0x15] = 0x50;
    p[0x25] = 0x50;
    p[0x35] = 0x60;
    p[0x45] = 0x60;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x47F9E0: kind 0x5E's menu - its box (Menu_DrawBox, flags 2, the
// window style 0x903A5A) and title (the text 0x66A098 at the box's corner + (4,
// 2), colour 0xF); the list (EffectKind5E_DrawList); the hand at the pick
// (Menu_DrawHand(the low byte of 0x654894, its high byte + 15 * (+2 & 7)));
// the panel (EffectKind5E_DrawPanel); the sprite placed at the panel's corner
// (+0x2E, +0x30, as kind 0x5D's), the panel's frame (Window_DrawFrame);
// Sprite_ScriptTick, then a tail jump to Sprite_QueueOverlay.
extern "C" void __cdecl EffectKind5E_DrawMenu(void) {
    SH_CALL(Menu_DrawBox)(W(at::kMenuBox), W(at::kMenuBox + 2), W(at::kMenuBox + 4), W(at::kMenuBox + 6), 2, B(at::kStyle));
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kText)), reinterpret_cast<const char*>(At(at::kStringFormat)), At(at::kTitle));
    SH_CALL(Text_DrawAt)(static_cast<int>(UL(at::kMenuBox) + 4), static_cast<int>((W(at::kMenuBox + 2) + 2) & 0xFFFFu), 0, 0xF, At(at::kText));
    SH_CALL(EffectKind5E_DrawList)();
    const unsigned pick = S()[2] & 7u;
    const unsigned hand = W(at::kHand);
    SH_CALL(Menu_DrawHand)(static_cast<int>(hand & 0xFFu), static_cast<int>(15 * pick + (hand >> 8)), 0);
    SH_CALL(EffectKind5E_DrawPanel)();
    SetWord(S() + 0x2E, B(at::kSpriteDx) + UL(at::kPanel));
    SetWord(S() + 0x30, B(at::kSpriteDy) + W(at::kPanel + 2));
    SH_CALL(Window_DrawFrame)(W(at::kPanel), W(at::kPanel + 2), B(at::kPanel + 4), B(at::kPanel + 6));
    SH_CALL(Sprite_ScriptTick)();
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x47FAF0: the list - its box (Menu_DrawBox, flags 0, the style);
// for each item 0x4E..0x55 a line 0xF lower from (x + 8, y + 0x14): its name
// (Item_NamePtr(0, item)) in colour 0 when held, else 7 (Inventory_Count(0,
// item, 0)'s low byte), its length Text_CharCount; the count (that byte)
// through the format 0x653EC0 at (x + 8 + 0x5A, the line + 3) in the same
// colour (Text_DrawFont8).
extern "C" void __cdecl EffectKind5E_DrawList(void) {
    SH_CALL(Menu_DrawBox)(W(at::kListBox), W(at::kListBox + 2), W(at::kListBox + 4), W(at::kListBox + 6), 0, B(at::kStyle));
    const U x = UL(at::kListBox) + 8;
    U line = W(at::kListBox + 2) + 0x14;
    for (unsigned item = at::kFirstItem; item < at::kFirstItem + at::kItems; ++item, line += 0xF) {
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, item);
        const auto count = static_cast<unsigned char>(SH_CALL(Inventory_Count)(0, item, 0));
        const unsigned colour = count != 0 ? 0u : 7u;
        const unsigned length = SH_CALL(Text_CharCount)(name);
        SH_CALL(Text_DrawAt)(static_cast<int>(x), static_cast<int>(line), static_cast<int>(colour), static_cast<int>(length), name);
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kText)), reinterpret_cast<const char*>(At(at::kListFormat)), static_cast<unsigned>(count));
        SH_CALL(Text_DrawFont8)(static_cast<int>(x + 0x5A), static_cast<int>(line + 3), static_cast<int>(colour), At(at::kText));
    }
}

// original 0x47FBE0: the panel - its box (Menu_DrawBox, flags 0, the style),
// then an FT4 over it (CLUT x 0x20; the corners the box's, the low words) with
// u 0xC0 / (w byte - 0x41), v 0x50 / (h byte + 0x50), colour 0x80, committed
// to slot 1 (0x48).
extern "C" void __cdecl EffectKind5E_DrawPanel(void) {
    SH_CALL(Menu_DrawBox)(W(at::kPanelBox), W(at::kPanelBox + 2), W(at::kPanelBox + 4), W(at::kPanelBox + 6), 0, B(at::kStyle));
    unsigned char* const p = Quad(0x20);
    p[4] = p[5] = p[6] = 0x80;
    const std::int32_t x0 = static_cast<std::int32_t>(W(at::kPanelBox));
    const std::int32_t y0 = static_cast<std::int32_t>(W(at::kPanelBox + 2));
    Corners(p, x0, y0, x0 + static_cast<std::int32_t>(W(at::kPanelBox + 4)), y0 + static_cast<std::int32_t>(W(at::kPanelBox + 6)));
    p[0x14] = 0xC0;
    p[0x15] = 0x50;
    p[0x25] = 0x50;
    p[0x34] = 0xC0;
    p[0x24] = static_cast<unsigned char>(B(at::kPanelBox + 4) - 0x41);
    p[0x35] = static_cast<unsigned char>(B(at::kPanelBox + 6) + 0x50);
    p[0x44] = static_cast<unsigned char>(B(at::kPanelBox + 4) - 0x41);
    p[0x45] = static_cast<unsigned char>(B(at::kPanelBox + 6) + 0x50);
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// ===========================================================================
// Kind 0x5F's states 1..9 (EffectKind5F_States; state 0 is Task_StartHold60).
// The record's point +0x34 / +0x38 / +0x3C, its velocity +0xC / +0x10 / +0x14.
// ===========================================================================

namespace {
// The moving states' draw: R3F's 0x480300(the point, 0x20 unread, the wobble 8,
// the dy `last`).
void Ring(U last) {
    SH_AT(void (__cdecl*)(const long*, unsigned, unsigned, unsigned), at::kR3FRing)(Point(S() + 0x34), 0x20, 8, last);
}
void Add(U offset, U by) { SetUL(S() + offset, UL(S() + offset) + by); }
void Placed(U x, U z, U y) {
    SetUL(S() + 0x34, x);
    SetUL(S() + 0x38, z);
    SetUL(S() + 0x3C, y);
}
void TwoSounds(unsigned a, unsigned b) {
    SH_CALL(Sound_PlayEffect)(a);
    SH_CALL(Sound_PlayEffect)(b);
}
}  // namespace

// original 0x47FDC0 (state 1): +9 down; at 0 the point (0x510000, 0x420000,
// 0x2C00000), the rise +0x14 = 0x800000, +1 up, sounds 0x202 and 0x203.
extern "C" void __cdecl EffectKind5F_Launch(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    if (S()[9] != 0) return;
    Placed(0x510000, 0x420000, 0x2C00000);
    SetUL(S() + 0x14, 0x800000);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    TwoSounds(0x202, 0x203);
}

// original 0x47FE30 (state 2; PSX twin 0x801F995C): x down 0x10000, the height
// up by the rise, the rise down 0x200000; drawn (Ring, last 0); at x 0x490000
// the velocity (0x38E3, 0x10000, 0x38E38E + 0x800000), +1 up, sounds 0x202,
// 0x203.
extern "C" void __cdecl EffectKind5F_Hop(void) {
    Add(0x34, 0xFFFF0000u);
    Add(0x3C, UL(S() + 0x14));
    Add(0x14, 0xFFE00000u);
    Ring(0);
    if (UL(S() + 0x34) != 0x490000) return;
    SetUL(S() + 0xC, 0x38E3);
    SetUL(S() + 0x10, 0x10000);
    SetUL(S() + 0x14, 0x38E38E);
    Add(0x14, 0x800000);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    TwoSounds(0x202, 0x203);
}

// original 0x47FEE0 (state 3): the point moved by the velocity, the rise down
// 0x200000; drawn (Ring, last 0); at z 0x4B0000 +9 = 0x3C and +1 up.
extern "C" void __cdecl EffectKind5F_Fly(void) {
    Add(0x34, UL(S() + 0xC));
    Add(0x38, UL(S() + 0x10));
    Add(0x3C, UL(S() + 0x14));
    Add(0x14, 0xFFE00000u);
    Ring(0);
    if (UL(S() + 0x38) != 0x4B0000) return;
    S()[9] = 0x3C;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x47FF60 (state 4): +9 down; at 0 x = z = 0x4B0000, the height
// AreaMap_Elevation(x, z) << 16 (its low word, signed), the velocity
// (0xFFFF4925, 0xFFFF0000, 0xFFCC30C4 + 0x800000), +1 up, sounds 0x202,
// 0x203.
extern "C" void __cdecl EffectKind5F_Bounce(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    if (S()[9] != 0) return;
    SetUL(S() + 0x34, 0x4B0000);
    SetUL(S() + 0x38, 0x4B0000);
    const U ground = static_cast<U>(static_cast<std::int16_t>(
                         SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(S() + 0x34)), static_cast<long>(UL(S() + 0x38)))))
                     << 16;
    SetUL(S() + 0x3C, ground);
    SetUL(S() + 0xC, 0xFFFF4925u);
    SetUL(S() + 0x10, 0xFFFF0000u);
    SetUL(S() + 0x14, 0xFFCC30C4u);
    Add(0x14, 0x800000);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    TwoSounds(0x202, 0x203);
}

// original 0x480010 (state 5): the point moved by the velocity, the rise down
// 0x100000; drawn (Ring, last 0); at z 0x360000 a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind5F_FlyAway(void) {
    Add(0x34, UL(S() + 0xC));
    Add(0x38, UL(S() + 0x10));
    Add(0x3C, UL(S() + 0x14));
    Add(0x14, 0xFFF00000u);
    Ring(0);
    if (UL(S() + 0x38) == 0x360000) SH_CALL(Effect_Release)();
}

// original 0x480080 (state 6): the point (0x270000, 0x80000, 0x8000000), the
// rise 0, +1 up, sounds 0x204 and 0x205.
extern "C" void __cdecl EffectKind5F_PlaceHigh(void) {
    Placed(0x270000, 0x80000, 0x8000000);
    SetUL(S() + 0x14, 0);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    TwoSounds(0x204, 0x205);
}

// original 0x4800E0 (state 7): z up 0x10000, the height up by the rise, the
// rise down 0x200000; drawn (Ring, last 0); at x 0xF0000 a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind5F_Slide(void) {
    Add(0x38, 0x10000);
    Add(0x3C, UL(S() + 0x14));
    Add(0x14, 0xFFE00000u);
    Ring(0);
    if (UL(S() + 0x34) == 0xF0000) SH_CALL(Effect_Release)();
}

// original 0x480140 (state 8): the point (0x190000, 0xF0000, 0x6000000), the
// rise 0, +1 up, sound 0x20A.
extern "C" void __cdecl EffectKind5F_PlaceLow(void) {
    Placed(0x190000, 0xF0000, 0x6000000);
    SetUL(S() + 0x14, 0);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    SH_CALL(Sound_PlayEffect)(0x20A);
}

// original 0x480190 (state 9): z up 0x10000, the height up by the rise, the
// rise down 0x200000; drawn (Ring, last 4); at z 0x190000 or more (signed) a
// tail jump to Effect_Release.
extern "C" void __cdecl EffectKind5F_SlideOut(void) {
    Add(0x38, 0x10000);
    Add(0x3C, UL(S() + 0x14));
    Add(0x14, 0xFFE00000u);
    Ring(4);
    if (static_cast<std::int32_t>(UL(S() + 0x38)) >= 0x190000) SH_CALL(Effect_Release)();
}

void Rest3E_Inject() {
    if (bof3::WantsShadow("rest_3e")) rest_3e::SelfTest();
    BOF3_INJECT(EffectKind37_Run);
    BOF3_INJECT(EffectKind17_Run);
    BOF3_INJECT(EffectKind1B_Run);
    BOF3_INJECT(EffectKind41_DrawNumber);
    BOF3_INJECT(EffectShards_LoadModel);
    BOF3_INJECT(EffectShards_Step);
    BOF3_INJECT(EffectKind1D_DrawSpeck);
    BOF3_INJECT(EffectKind21_ArmPoints);
    BOF3_INJECT(EffectKind21_DrawArm);
    BOF3_INJECT(EffectKind21_DrawArmTriangle);
    BOF3_INJECT(EffectKind24_DrawRay);
    BOF3_INJECT(EffectGlowSparks_Clear);
    BOF3_INJECT(EffectGlowSparks_StartRise);
    BOF3_INJECT(EffectGlowSparks_StartBurst);
    BOF3_INJECT(EffectGlowSparks_Run);
    BOF3_INJECT(EffectGlowSparks_Draw);
    BOF3_INJECT(EffectGlowSparks_Glow);
    BOF3_INJECT(EffectGlowSparks_Rise);
    BOF3_INJECT(EffectGlowTrail_Update);
    BOF3_INJECT(EffectGlowTrail_Draw);
    BOF3_INJECT(EffectSpiral_Init);
    BOF3_INJECT(EffectSpiral_StepDraw);
    BOF3_INJECT(EffectRing_Draw);
    BOF3_INJECT(EffectDust_Clear);
    BOF3_INJECT(EffectDust_FindFree);
    BOF3_INJECT(EffectDust_Start);
    BOF3_INJECT(EffectDust_Run);
    BOF3_INJECT(EffectDust_DrawColumn);
    BOF3_INJECT(EffectDust_DrawFan);
    BOF3_INJECT(EffectKind5D_Start);
    BOF3_INJECT(EffectKind5D_Open);
    BOF3_INJECT(EffectKind5D_Show);
    BOF3_INJECT(EffectKind5D_Close);
    BOF3_INJECT(EffectKind5E_Start);
    BOF3_INJECT(EffectKind5E_Pick);
    BOF3_INJECT(EffectKind5E_Wait);
    BOF3_INJECT(EffectKind5D_DrawBoard);
    BOF3_INJECT(EffectKind5D_DrawMark);
    BOF3_INJECT(EffectKind5E_DrawMenu);
    BOF3_INJECT(EffectKind5E_DrawList);
    BOF3_INJECT(EffectKind5E_DrawPanel);
    BOF3_INJECT(EffectKind5F_Launch);
    BOF3_INJECT(EffectKind5F_Hop);
    BOF3_INJECT(EffectKind5F_Fly);
    BOF3_INJECT(EffectKind5F_Bounce);
    BOF3_INJECT(EffectKind5F_FlyAway);
    BOF3_INJECT(EffectKind5F_PlaceHigh);
    BOF3_INJECT(EffectKind5F_Slide);
    BOF3_INJECT(EffectKind5F_PlaceLow);
    BOF3_INJECT(EffectKind5F_SlideOut);
}
