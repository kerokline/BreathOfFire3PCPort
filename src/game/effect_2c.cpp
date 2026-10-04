// Effect kinds 0x3E, 0x3F, 0x40, 0x42, 0x43, 0x44 and 0x6B - round thirteen,
// wave two, group E2C: the 51 functions of the cut (analysis/round13_cut.tsv)
// in 0x474F40..0x4771A5 and the two its span holds that no list has (0x476560,
// a callee of kind 0x43's; 0x476680, kind 0x44's dispatcher), each read with
// capstone to its last instruction (docs/effect_2c.md section 1).
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and the
// states it names:
//
//   kind 0x3E  four lightning bolts between fixed cells, a sound while
//              Field_Request is 5 (Area145_Tail20 spawns it);
//   kind 0x3F  a beam between two world points for one frame, then
//              Effect_StateRelease (Area145_SpawnTrail);
//   kind 0x40  a disc growing round its point with 32 shards falling inside it
//              until the counter byte 0x903848 is 0x19 (Area28_SpawnEffect40);
//   kind 0x42  a glow cylinder at its point (Area146_DrawGlowCylinder);
//   kind 0x43  256 sparks round a fixed cell: they rise, circle and fly out,
//              phase by phase (+6), until none is left (chapter 10's run 13);
//   kind 0x44  a ring of light on sprite 1 fading in, widening, following it
//              with a cone and trailing sparks, fading out (Area78_SpawnEffect44);
//   kind 0x6B  sprite 2 drawn off screen through a borrowed record, read back
//              from the VRAM shadow and crumbled into falling pixels (chapter
//              10's run 13).
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Every x87 operation is done in x87 (long
// double, loads through `flds`) as the original does it, so rounding and NaNs
// come out alike. Where the original jumps through a state table past its end,
// divides by 0, indexes kind 0x6B's column heights past their 80 or loops
// forever, ours aborts with a message (docs/effect_2c.md section 6).
#include "game/effect_2c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2c::at;
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
void SetUL(U a, U v) { SetUL(At(a), v); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t SW(U a) { return SW(At(a)); }
unsigned char& B(U a) { return At(a)[0]; }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul r32, r32 then sar: the product's low 32 bits, shifted.
U MulSar(std::int32_t a, std::int32_t b, unsigned n) { return Sar(static_cast<U>(a) * static_cast<U>(b), n); }
// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n: a signed divide toward zero.
std::int32_t DivPow2(U v, unsigned n) { return static_cast<std::int32_t>(v) / (1 << n); }
// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

// --- x87 as the original has it: `fld dword` through inline assembly, so the
// optimizer cannot narrow an operation of two floats to SSE (whose NaN rule
// differs); every operation in long double on the x87 at the game's control
// word, `fstp dword` on the way out.
LD F(const void* p) {
    LD r;
    __asm__("flds %1" : "=t"(r) : "m"(*static_cast<const float*>(p)));
    return r;
}
LD FAt(U a) { return F(At(a)); }
LD I(std::int32_t v) { return static_cast<LD>(v); }
void StF(unsigned char* p, LD v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
// The CRT's _ftol 0x5B9550: truncation to 64 bits, the low dword; NaN and
// values out of range give the integer indefinite 0x8000000000000000, whose
// low dword is 0.
U Ftol(LD v) {
    if (!(v > -9223372036854775808.0L && v < 9223372036854775808.0L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + k]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, const void* table, unsigned entries, unsigned state) {
    if (state >= entries)
        bof3::Fatal("%s: state byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2c.md section 6)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// The draw-mode packet every draw here opens with: Gpu_GetTPage(0, abr, x, y),
// Gpu_SetDrawMode(packet, 0, dtd, the page's low word, 0 - the fifth word the
// leftover of GetTPage's five pushes), committed to `slot` (0xC).
void DrawMode(U abr, int dtd, U slot) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// Effect_FindFree's record (answer & 0xFF, as the original widens it): a Fatal
// on an answer past the twenty (the callee answers 0xFF or 0..19; the original
// would write past the pool).
unsigned char* NewRecord(const char* who, unsigned index) {
    if (index >= at::kEffects)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past the pool "
                    "(docs/effect_2c.md section 6)",
                    who, index);
    return Effect_Objects + index * at::kEffectStride;
}

// Kind 0x6B's column heights: 80 s16 at 0x676114 (the 0xA0 bytes its scatter
// clears); a Fatal past them, where the original reads and writes what follows.
U HeightAt(const char* who, std::int32_t column) {
    if (column < 0 || column >= static_cast<std::int32_t>(at::kHeightCount))
        bof3::Fatal("%s: column %d, past the 80 heights at 0x676114 - the original reads what lies beside them "
                    "(docs/effect_2c.md section 6)",
                    who, (int)column);
    return at::kHeights + 2u * static_cast<U>(column);
}

// +9 down one (Sprite_Current read for each access, as the originals); at 0,
// +9 = reload and +1 up.
void CountDown(unsigned reload) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    s[9] = static_cast<unsigned char>(reload);
    s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }
const long* Point(const unsigned char* p) { return reinterpret_cast<const long*>(p); }
float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

}  // namespace

// ===========================================================================
// Kind 0x3E: Effect_KindHandlers[0x3E] (0x655448), EffectKind3E_States (two)
// and its sub-state table EffectKind3E_SubStates (two)
// ===========================================================================

// original 0x474F40: jmp [EffectKind3E_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind3E_Run(void) {
    Dispatch("EffectKind3E_Run", EffectKind3E_States, EffectKind3E_States_count, S()[1]);
}

// original 0x474F60 (state 0): the point (+0x34, +0x38) = (0x318000, 0x638000),
// the heights +0x20 and +0x14 = 0x500000; Sound_PlayById(0x208); +1 up, +2 = 0.
extern "C" void __cdecl EffectKind3E_Start(void) {
    SetUL(S() + 0x34, 0x318000);
    SetUL(S() + 0x38, 0x638000);
    SetUL(S() + 0x20, 0x500000);
    SetUL(S() + 0x14, 0x500000);
    SH_CALL(Sound_PlayById)(0x208);
    NextState();
    S()[2] = 0;
}

// original 0x474FC0 (state 1): four bolts, each from the cell centre (c0, c1)
// to (c2, c3) of EffectKind3E_BoltCells' next four bytes (x = c << 16 | 0x8000
// into +0xC, +0x10, +0x18, +0x1C; the heights are +0x14 and +0x20), drawn by
// EffectKind3E_DrawBolt(record + 0xC, record + 0x18); then EffectKind3E_SubStates
// by +2, a call, unbounded.
extern "C" void __cdecl EffectKind3E_Bolts(void) {
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char* const c = At(at::kBoltCells + 4 * i);
        SetUL(S() + 0xC, static_cast<U>(c[0]) << 16 | 0x8000u);
        SetUL(S() + 0x10, static_cast<U>(c[1]) << 16 | 0x8000u);
        SetUL(S() + 0x18, static_cast<U>(c[2]) << 16 | 0x8000u);
        SetUL(S() + 0x1C, static_cast<U>(c[3]) << 16 | 0x8000u);
        unsigned char* const s = S();
        SH_CALL(EffectKind3E_DrawBolt)(Point(s + 0xC), Point(s + 0x18));
    }
    Dispatch("EffectKind3E_Bolts", EffectKind3E_SubStates, EffectKind3E_SubStates_count, S()[2]);
}

// original 0x475050 (sub-state 0): Field_Request not 5 - +2 = 1.
extern "C" void __cdecl EffectKind3E_SubWait(void) {
    if (Field_Request == 5) return;
    S()[2] = 1;
}

// original 0x475070 (sub-state 1): Field_Request 5 - Sound_PlayEffect(0x209).
extern "C" void __cdecl EffectKind3E_SubSound(void) {
    if (Field_Request != 5) return;
    SH_CALL(Sound_PlayEffect)(0x209);
}

// original 0x4750D0 (0x165 bytes): EffectGte_LoadMapCamera; a draw mode (abr 1,
// committed to slot 1); the world point L = from's x, z and height projected;
// then 16 times: a grey semi-transparent LINE_F2 from the last projection to
// the next (L's x and z stepped by (to - from) / 16, its height from's plus
// ((Rand & 0xFFF) - 0x800) << 12 on all but the last), committed 0x20, and
// EffectKind3E_BoltGlow of its two ends.
extern "C" void __cdecl EffectKind3E_DrawBolt(const long* from, const long* to) {
    SH_CALL(EffectGte_LoadMapCamera)();
    DrawMode(1, 1, 1);
    const auto* const a = reinterpret_cast<const unsigned char*>(from);
    const auto* const b = reinterpret_cast<const unsigned char*>(to);
    alignas(4) unsigned char l[12];
    SetUL(l, UL(a));
    SetUL(l + 4, UL(a + 4));
    const U height = UL(a + 8);
    SetUL(l + 8, height);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(l), Out(o));
    for (unsigned step = 1; step <= 0x10; ++step) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineF2)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        Copy12(prim + 8, o);
        SetUL(l, UL(l) + static_cast<U>(DivPow2(UL(b) - UL(a), 4)));
        SetUL(l + 4, UL(l + 4) + static_cast<U>(DivPow2(UL(b + 4) - UL(a + 4), 4)));
        SetUL(l + 8, height);
        if (step < 0x10) SetUL(l + 8, height + (((static_cast<U>(SH_CALL(Rand)()) & 0xFFFu) - 0x800u) << 12));
        SH_CALL(EffectGte_ProjectPoint)(Point(l), Out(o));
        Copy12(prim + 0x14, o);
        prim[4] = prim[5] = prim[6] = 0x80;
        SH_CALL(Gfx_CommitPrim)(1, 0x20);
        SH_CALL(EffectKind3E_BoltGlow)(prim + 8, prim + 0x14);
    }
}

// original 0x475240 (0x146 bytes): the segment's normal - Gte_VectorNormal of
// (_ftol(-(p1.y - p0.y)), _ftol(p1.x - p0.x), 0), in place - times 5 >> 12 is
// (nx, ny); a semi-transparent POLY_G4 of p0, p1 (shade 0x40) and p0 + n, p1 +
// n (black), the depths p0's and p1's, committed 0x44; the same quad copied to
// the next packet with p0 - n, p1 - n, committed 0x44.
extern "C" void __cdecl EffectKind3E_BoltGlow(const unsigned char* p0, const unsigned char* p1) {
    alignas(4) long v[3];
    v[0] = static_cast<long>(Ftol(-(F(p1 + 4) - F(p0 + 4))));
    v[1] = static_cast<long>(Ftol(F(p1) - F(p0)));
    v[2] = 0;
    SH_CALL(Gte_VectorNormal)(v, v);
    unsigned char* const prim = Gfx_PacketNext;
    const auto nx = static_cast<std::int32_t>(MulSar(static_cast<std::int32_t>(v[0]), 5, 12));
    const auto ny = static_cast<std::int32_t>(MulSar(static_cast<std::int32_t>(v[1]), 5, 12));
    SH_CALL(Gpu_SetPolyG4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SetUL(prim + 8, UL(p0));
    SetUL(prim + 0xC, UL(p0 + 4));
    StF(prim + 0x18, I(nx) + F(p0));
    StF(prim + 0x1C, I(ny) + F(p0 + 4));
    const LD z0 = F(p0 + 8);
    StF(prim + 0x20, z0);
    StF(prim + 0x10, z0);
    SetUL(prim + 0x28, UL(p1));
    SetUL(prim + 0x2C, UL(p1 + 4));
    StF(prim + 0x38, I(nx) + F(p1));
    StF(prim + 0x3C, I(ny) + F(p1 + 4));
    const LD z1 = F(p1 + 8);
    StF(prim + 0x40, z1);
    StF(prim + 0x30, z1);
    prim[4] = prim[5] = prim[6] = 0x40;
    prim[0x14] = prim[0x15] = prim[0x16] = 0;
    prim[0x24] = prim[0x25] = prim[0x26] = 0x40;
    prim[0x34] = prim[0x35] = prim[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
    unsigned char* const next = Gfx_PacketNext;
    for (unsigned i = 0; i < 0x44; i += 4) SetUL(next + i, UL(next - 0x44 + i));   // rep movsd, forward
    StF(next + 0x18, F(p0) - I(nx));
    StF(next + 0x1C, F(p0 + 4) - I(ny));
    StF(next + 0x38, F(p1) - I(nx));
    StF(next + 0x3C, F(p1 + 4) - I(ny));
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
}

// ===========================================================================
// Kind 0x3F: Effect_KindHandlers[0x3F] (0x65544C), EffectKind3F_States (two,
// the second Effect_StateRelease)
// ===========================================================================

// original 0x475090: jmp [EffectKind3F_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind3F_Run(void) {
    Dispatch("EffectKind3F_Run", EffectKind3F_States, EffectKind3F_States_count, S()[1]);
}

// original 0x4750B0 (state 0): EffectKind3F_DrawBeam(record + 0xC, record +
// 0x18); +1 up.
extern "C" void __cdecl EffectKind3F_Draw(void) {
    unsigned char* const s = S();
    SH_CALL(EffectKind3F_DrawBeam)(Point(s + 0xC), Point(s + 0x18));
    NextState();
}

// original 0x475390 (0x10D bytes): a draw mode (abr 1, slot 1);
// EffectGte_LoadMapCamera; both ends projected; the angle Math_Ratan2(dy, dx)
// of the screen segment; each end's radius the size (0x10, 0) projected at it
// (EffectGte_ProjectSize) plus Frame_Counter & 1; a cap at `from` at angle +
// 0x400 and one at `to` at angle + 0xC00 (EffectKind3F_DrawCap), and the band
// between (EffectKind3F_DrawBand).
extern "C" void __cdecl EffectKind3F_DrawBeam(const long* from, const long* to) {
    DrawMode(1, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char p0[12];
    alignas(4) unsigned char p1[12];
    SH_CALL(EffectGte_ProjectPoint)(from, Out(p0));
    SH_CALL(EffectGte_ProjectPoint)(to, Out(p1));
    const auto dx = static_cast<float>(F(p1) - F(p0));
    const auto dy = static_cast<float>(F(p1 + 4) - F(p0 + 4));
    const U angle = static_cast<U>(SH_CALL(Math_Ratan2)(dy, dx));
    alignas(4) short size[2] = {0x10, 0};
    alignas(4) short out[2];
    SH_CALL(EffectGte_ProjectSize)(from, size, out);
    unsigned char out_bytes[4];
    std::memcpy(out_bytes, out, 4);
    const U r0 = (Frame_Counter & 1u) + UL(out_bytes);
    const U a0 = angle + 0x400;
    SH_CALL(EffectKind3F_DrawCap)(p0, r0, a0);
    SH_CALL(EffectGte_ProjectSize)(to, size, out);
    std::memcpy(out_bytes, out, 4);
    const U r1 = UL(out_bytes) + (Frame_Counter & 1u);
    const U a1 = angle + 0xC00;
    SH_CALL(EffectKind3F_DrawCap)(p1, r1, a1);
    SH_CALL(EffectKind3F_DrawBand)(p0, r0, a0, p1, r1, a1);
}

// original 0x4754A0 (0x108 bytes): eight semi-transparent POLY_G3 of a fan
// round `centre` (its three floats; its depth at all three vertices), the rim
// at (cos, sin)(t) * radius >> 12 for t = angle, angle + 0x100, .. (each masked
// to 16 bits), each triangle from t to t + 0x100; the centre yellow (0x80,
// 0x80, 0), the rim black; committed 0x34 each. The radius is read as an s16.
extern "C" void __cdecl EffectKind3F_DrawCap(const unsigned char* centre, unsigned radius, unsigned angle) {
    const std::int32_t r = static_cast<std::int16_t>(radius);
    U slot = angle;
    U t = angle & 0xFFFFu;
    for (unsigned n = 8; n != 0; --n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SetUL(prim + 8, UL(centre));
        SetUL(prim + 0xC, UL(centre + 4));
        const LD z = F(centre + 8);
        StF(prim + 0x30, z);
        StF(prim + 0x20, z);
        StF(prim + 0x10, z);
        std::int32_t v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(t)), r, 12));
        StF(prim + 0x18, I(v) + F(centre));
        v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(t)), r, 12));
        slot += 0x100;
        t = slot & 0xFFFFu;
        StF(prim + 0x1C, I(v) + F(centre + 4));
        v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(t)), r, 12));
        StF(prim + 0x28, I(v) + F(centre));
        v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(t)), r, 12));
        prim[4] = 0x80;
        prim[5] = 0x80;
        prim[6] = 0;
        prim[0x14] = prim[0x15] = prim[0x16] = 0;
        StF(prim + 0x2C, I(v) + F(centre + 4));
        prim[0x24] = prim[0x25] = prim[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
    }
}

// original 0x4755B0 (0x1A4 bytes): at the packet cursor, a semi-transparent
// POLY_G4 of p0, p1 (yellow) and p0 + (cos, sin)(a0) * r0 >> 12, p1 + (cos,
// sin)(a1 + 0x800) * r1 >> 12 (black), the depths p0's and p1's, committed
// 0x44; the same quad copied 0x44 bytes on (the cursor not read again) with
// p0's rim at a0 + 0x800 and p1's at a1, committed 0x44. The radii are read as
// s16, the angles' low words (a0's + 0x800 passed whole).
extern "C" void __cdecl EffectKind3F_DrawBand(const unsigned char* p0, unsigned r0, unsigned a0, const unsigned char* p1,
                                              unsigned r1, unsigned a1) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const U t0 = a0 & 0xFFFFu;
    std::int32_t r = static_cast<std::int16_t>(r0);
    SetUL(prim + 8, UL(p0));
    SetUL(prim + 0xC, UL(p0 + 4));
    LD z = F(p0 + 8);
    StF(prim + 0x30, z);
    StF(prim + 0x10, z);
    SetUL(prim + 0x18, UL(p1));
    SetUL(prim + 0x1C, UL(p1 + 4));
    z = F(p1 + 8);
    StF(prim + 0x40, z);
    StF(prim + 0x20, z);
    std::int32_t v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(t0)), r, 12));
    StF(prim + 0x28, I(v) + F(prim + 8));
    v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(t0)), r, 12));
    const U t1 = a1 & 0xFFFFu;
    r = static_cast<std::int16_t>(r1);
    StF(prim + 0x2C, I(v) + F(prim + 0xC));
    U t = t1 + 0x800;
    v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(t)), r, 12));
    StF(prim + 0x38, I(v) + F(prim + 0x18));
    v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(t)), r, 12));
    prim[4] = 0x80;
    prim[5] = 0x80;
    prim[6] = 0;
    prim[0x14] = 0x80;
    prim[0x15] = 0x80;
    prim[0x16] = 0;
    prim[0x24] = prim[0x25] = prim[0x26] = 0;
    StF(prim + 0x3C, I(v) + F(prim + 0x1C));
    prim[0x34] = prim[0x35] = prim[0x36] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
    unsigned char* const q = prim + 0x44;
    t = t0 + 0x800;
    for (unsigned i = 0; i < 0x44; i += 4) SetUL(q + i, UL(prim + i));   // rep movsd, forward
    r = static_cast<std::int16_t>(r0);
    v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(t)), r, 12));
    StF(q + 0x28, I(v) + F(q + 8));
    v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(t)), r, 12));
    StF(q + 0x2C, I(v) + F(q + 0xC));
    r = static_cast<std::int16_t>(r1);
    v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(t1)), r, 12));
    StF(q + 0x38, I(v) + F(q + 0x18));
    v = static_cast<std::int32_t>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(t1)), r, 12));
    StF(q + 0x3C, I(v) + F(q + 0x1C));
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
}

// ===========================================================================
// Kind 0x40: Effect_KindHandlers[0x40] (0x655450), EffectKind40_States (three,
// the third Effect_StateRelease)
// ===========================================================================

// original 0x475760: jmp [EffectKind40_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind40_Run(void) {
    Dispatch("EffectKind40_Run", EffectKind40_States, EffectKind40_States_count, S()[1]);
}

// original 0x475780 (state 0): EffectKind40_ShardsClear; the radius word +0x2E
// = 0; +1 up.
extern "C" void __cdecl EffectKind40_Start(void) {
    SH_CALL(EffectKind40_ShardsClear)();
    SetWord(S() + 0x2E, 0);
    NextState();
}

// original 0x4757A0 (state 1): the counter byte 0x903848 at 0x19 - +1 up.
// Else the disc of radius +0x2E (EffectKind40_DrawDisc; the original pushes
// the register whose low word it loaded, its high half the dispatcher's
// Sprite_Current) and EffectKind40_ShardsStep; +0x2E up 0x40, at most 0x400
// (signed).
extern "C" void __cdecl EffectKind40_Grow(void) {
    const unsigned char counter = B(at::kCounter);
    unsigned char* s = S();
    if (counter == 0x19) {
        s[1] = static_cast<unsigned char>(s[1] + 1);
        return;
    }
    SH_CALL(EffectKind40_DrawDisc)(Word(s + 0x2E));
    SH_CALL(EffectKind40_ShardsStep)();
    s = S();
    SetWord(s + 0x2E, Word(s + 0x2E) + 0x40u);
    s = S();
    if (SW(s + 0x2E) >= 0x400) SetWord(s + 0x2E, 0x400);
}

// original 0x4757F0 (0x2B bytes): the 32 shard records' in-use bytes cleared
// through the cursor 0x67610C (left past the last).
extern "C" void __cdecl EffectKind40_ShardsClear(void) {
    SetUL(at::kShardCursor, at::kShards);
    for (unsigned n = at::kShardCount; n != 0; --n) {
        At(UL(at::kShardCursor))[0] = 0;
        SetUL(at::kShardCursor, UL(at::kShardCursor) + at::kShardStride);
    }
}

// original 0x475820 (0xA8 bytes): EffectGte_LoadMapCamera; a draw mode (abr
// 1, slot 3); each live shard of the 32 (the cursor 0x67610C on it): its
// height word +8 down 0x20, below 0x80 (signed) no longer in use, drawn
// (EffectKind40_ShardDraw) either way; then twice a free record
// (EffectKind40_ShardFind, the cursor its answer) started
// (EffectKind40_ShardSpawn).
extern "C" void __cdecl EffectKind40_ShardsStep(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    DrawMode(1, 1, 3);
    U p = at::kShards;
    SetUL(at::kShardCursor, p);
    for (unsigned n = at::kShardCount; n != 0; --n) {
        if (At(p)[0] != 0) {
            SetWord(At(p) + 8, Word(At(p) + 8) - 0x20u);
            p = UL(at::kShardCursor);
            if (SW(At(p) + 8) < 0x80) At(p)[0] = 0;
            SH_CALL(EffectKind40_ShardDraw)();
            p = UL(at::kShardCursor);
        }
        p += at::kShardStride;
        SetUL(at::kShardCursor, p);
    }
    for (unsigned k = 0; k < 2; ++k) {
        unsigned char* const found = SH_CALL(EffectKind40_ShardFind)();
        SetUL(at::kShardCursor, AddressOf(found));
        if (found != nullptr) SH_CALL(EffectKind40_ShardSpawn)(found);
    }
}

// original 0x4758D0 (0x14B bytes), on the cursor's shard: its screen point
// (+0xC, +0x10) the disc's centre 0x92C280 plus (s16 +4 << 7) / s16 +8 and
// (s16 +6 << 7) / s16 +8, its depth +0x14 the centre's; drawn only inside the
// disc's rim (the 32 points 0x92C28C: every edge's cross product with the
// point 0.0 or more) - a TILE_1 (not semi-transparent) at it, shaded (0x80 -
// s16 +8) * 192 / 512 - 0x40, committed to slot 3 (0x14). A height of 0 is the
// original's divide fault: ours aborts.
extern "C" void __cdecl EffectKind40_ShardDraw(void) {
    unsigned char* p = At(UL(at::kShardCursor));
    std::int32_t d = SW(p + 8);
    if (d == 0)
        bof3::Fatal("EffectKind40_ShardDraw: the shard's height +8 is 0 - the original faults on the divide "
                    "(docs/effect_2c.md section 6)");
    std::int32_t q = static_cast<std::int32_t>(static_cast<U>(SW(p + 4)) << 7) / d;
    StF(p + 0xC, I(q) + FAt(at::kDiscCentre));
    p = At(UL(at::kShardCursor));
    d = SW(p + 8);
    if (d == 0)
        bof3::Fatal("EffectKind40_ShardDraw: the shard's height +8 is 0 - the original faults on the divide "
                    "(docs/effect_2c.md section 6)");
    q = static_cast<std::int32_t>(static_cast<U>(SW(p + 6)) << 7) / d;
    StF(p + 0x10, I(q) + FAt(at::kDiscCentre + 4));
    const LD depth = FAt(at::kDiscCentre + 8);
    StF(At(UL(at::kShardCursor)) + 0x14, depth);
    const unsigned char* const s = At(UL(at::kShardCursor));
    unsigned prev = 0;
    unsigned char step = 0;
    do {
        ++step;
        const unsigned next = step & 0x1Fu;
        const U vn = at::kDiscRim + 12u * next;
        const U vp = at::kDiscRim + 12u * prev;
        const LD dx = FAt(vn) - FAt(vp);
        const LD dy = FAt(vn + 4) - FAt(vp + 4);
        const LD px = F(s + 0xC) - FAt(vp);
        const LD py = F(s + 0x10) - FAt(vp + 4);
        const LD cross = py * dx - px * dy;
        if (!(cross >= FAt(at::kZero))) return;
        prev = step;
    } while (step < 0x20);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 0);
    Copy12(prim + 8, At(UL(at::kShardCursor)) + 0xC);
    const U shade = (0x80u - static_cast<U>(SW(At(UL(at::kShardCursor)) + 8))) * 3u << 6;
    const auto c = static_cast<unsigned char>(DivPow2(shade, 9) - 0x40);
    prim[4] = prim[5] = prim[6] = c;
    SH_CALL(Gfx_CommitPrim)(3, 0x14);
}

// original 0x475A20 (0x36 bytes): the shard in use; its offset words +4, +6
// (Rand & 0xFF) - 0x80 each (+4 first); its height +8 = 0x280.
extern "C" void __cdecl EffectKind40_ShardSpawn(unsigned char* shard) {
    shard[0] = 1;
    U r = static_cast<U>(SH_CALL(Rand)());
    SetWord(shard + 4, (r & 0xFFu) - 0x80u);
    r = static_cast<U>(SH_CALL(Rand)());
    SetWord(shard + 8, 0x280);
    SetWord(shard + 6, (r & 0xFFu) - 0x80u);
}

// original 0x475A60 (0x23 bytes): the first shard record of the 32 not in use,
// the cursor 0x67610C left on it (past the last, and 0 answered, when none).
extern "C" unsigned char* __cdecl EffectKind40_ShardFind(void) {
    U p = at::kShards;
    SetUL(at::kShardCursor, p);
    for (unsigned char n = 0;;) {
        if (At(p)[0] == 0) return At(p);
        p += at::kShardStride;
        ++n;
        SetUL(at::kShardCursor, p);
        if (n >= at::kShardCount) return nullptr;
    }
}

// original 0x475A90 (0x201 bytes): a draw mode (abr 2, slot 3);
// EffectGte_LoadMapCamera; the record's point projected - its screen point the
// disc's centre, kept at 0x92C280; the rim point at angle 0xFF80 ((cos, sin) *
// radius >> 4 from the record's point, Sprite_Current read after each call)
// projected; then 32 white-to-black POLY_G3 fans (centre white, rim black,
// semi-transparent) round it, the angle up 0x80 each, every new rim point kept
// in 0x92C28C.. as well, committed to slot 3 (0x34). The radius is read as an s16.
extern "C" void __cdecl EffectKind40_DrawDisc(unsigned radius) {
    DrawMode(2, 1, 3);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* s = S();
    alignas(4) unsigned char q[12];
    SetUL(q, UL(s + 0x34));
    SetUL(q + 4, UL(s + 0x38));
    SetUL(q + 8, UL(s + 0x3C));
    alignas(4) unsigned char c[12];
    alignas(4) unsigned char p[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(c));
    const std::int32_t r = static_cast<std::int16_t>(radius);
    U angle = 0xFF80;
    SetUL(at::kDiscCentre, UL(c));
    SetUL(at::kDiscCentre + 4, UL(c + 4));
    SetUL(at::kDiscCentre + 8, UL(c + 8));
    U v = MulSar(SH_CALL(Math_Cos)(static_cast<int>(angle)), r, 4);
    SetUL(q, v + UL(S() + 0x34));
    v = MulSar(SH_CALL(Math_Sin)(static_cast<int>(angle)), r, 4);
    s = S();
    SetUL(q + 4, v + UL(s + 0x38));
    SetUL(q + 8, UL(s + 0x3C));
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(p));
    U rim = at::kDiscRim;
    for (unsigned n = 0x20; n != 0; --n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        Copy12(prim + 8, c);
        angle += 0x80;
        Copy12(prim + 0x18, p);
        const U t = angle & 0xFFFFu;
        v = MulSar(SH_CALL(Math_Cos)(static_cast<int>(t)), r, 4);
        SetUL(q, v + UL(S() + 0x34));
        v = MulSar(SH_CALL(Math_Sin)(static_cast<int>(t)), r, 4);
        s = S();
        SetUL(q + 4, v + UL(s + 0x38));
        SetUL(q + 8, UL(s + 0x3C));
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(p));
        Copy12(prim + 0x28, p);
        Copy12(At(rim), p);
        prim[4] = prim[5] = prim[6] = 0xFF;
        prim[0x14] = prim[0x15] = prim[0x16] = 0;
        prim[0x24] = prim[0x25] = prim[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(3, 0x34);
        rim += 0xC;
    }
}

// ===========================================================================
// Kind 0x42: Effect_KindHandlers[0x42] (0x655458), EffectKind42_States (two:
// 0x47EEC0, group E2G's, then ours)
// ===========================================================================

// original 0x475CA0: jmp [EffectKind42_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind42_Run(void) {
    Dispatch("EffectKind42_Run", EffectKind42_States, EffectKind42_States_count, S()[1]);
}

// original 0x475CC0 (state 1): Area146_DrawGlowCylinder at a copy of the
// record's point; +1 stays (the record runs until something releases it).
extern "C" void __cdecl EffectKind42_Glow(void) {
    const unsigned char* const s = S();
    alignas(4) long point[4];
    point[0] = Long(s + 0x34);
    point[1] = Long(s + 0x38);
    point[2] = Long(s + 0x3C);
    point[3] = 0;   // the original's fourth dword is never written or read
    SH_CALL(Area146_DrawGlowCylinder)(point);
}

// ===========================================================================
// Kind 0x43: Effect_KindHandlers[0x43] (0x65545C), EffectKind43_States (four)
// ===========================================================================

// original 0x475CF0: jmp [EffectKind43_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind43_Run(void) {
    Dispatch("EffectKind43_Run", EffectKind43_States, EffectKind43_States_count, S()[1]);
}

// original 0x475D10 (state 0): EffectKind43_Setup; the word +0x32 = 0x78, the
// phase +6 = 0; +1 up; Sound_PlayEffect(0x201).
extern "C" void __cdecl EffectKind43_Start(void) {
    SH_CALL(EffectKind43_Setup)();
    SetWord(S() + 0x32, 0x78);
    S()[6] = 0;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x201);
}

// original 0x475D40 (state 1): four sparks added (EffectKind43_AddSpark), all
// run (EffectKind43_SparksRun); +0x32 down, at 0 the phase +6 = 1, +0x32 =
// 0x3C and +1 up.
extern "C" void __cdecl EffectKind43_Gather(void) {
    for (unsigned k = 0; k < 4; ++k) SH_CALL(EffectKind43_AddSpark)();
    SH_CALL(EffectKind43_SparksRun)();
    SetWord(S() + 0x32, Word(S() + 0x32) - 1u);
    unsigned char* s = S();
    if (Word(s + 0x32) != 0) return;
    s[6] = 1;
    SetWord(S() + 0x32, 0x3C);
    NextState();
}

// original 0x475D90 (state 2): the sparks run; at the counter byte 0x903848
// 0x11: Sound_PlayEffect(0x203), (0x202), the phase +6 = 2, +1 up.
extern "C" void __cdecl EffectKind43_Wait(void) {
    SH_CALL(EffectKind43_SparksRun)();
    if (B(at::kCounter) != 0x11) return;
    SH_CALL(Sound_PlayEffect)(0x203);
    SH_CALL(Sound_PlayEffect)(0x202);
    S()[6] = 2;
    NextState();
}

// original 0x475DD0 (state 3): the sparks run; none live - Effect_Release (a
// tail jmp).
extern "C" void __cdecl EffectKind43_Fade(void) {
    if (SH_CALL(EffectKind43_SparksRun)() != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x476230 (0x51 bytes): the record's point (0x250000, 0x748000) on
// the ground (AreaMap_Elevation's s16 << 16 into +0x3C); the 256 spark records
// of 0x1C at 0x92BF80 out of use.
extern "C" void __cdecl EffectKind43_Setup(void) {
    SetUL(S() + 0x34, 0x250000);
    SetUL(S() + 0x38, 0x748000);
    const unsigned char* const s = S();
    const long ground = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetUL(S() + 0x3C, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16);
    for (unsigned i = 0; i < at::kSparkCount43; ++i) At(at::kShards + i * at::kSparkStride43)[0] = 0;
}

// original 0x476290 (0x32 bytes): the first spark record not in use (none:
// nothing): in use, its side +3 the byte 0x6761B4 (then flipped), its phase +2
// = 0.
extern "C" void __cdecl EffectKind43_AddSpark(void) {
    U p = at::kShards;
    for (unsigned n = 0; At(p)[0] != 0;) {
        p += at::kSparkStride43;
        ++n;
        if ((n & 0xFFFFu) >= at::kSparkCount43) return;
    }
    const unsigned char side = B(at::kSparkSide);
    At(p)[0] = 1;
    At(p)[3] = side;
    At(p)[2] = 0;
    B(at::kSparkSide) = static_cast<unsigned char>(side ^ 1);
}

// original 0x4762D0 (0x290 bytes with its five-case jump table at 0x47654C):
// EffectGte_LoadMapCamera; each live spark record (+1 a count, +2 its phase,
// +3 its side, +4 a dword, +8 an angle word, +0xC x, +0x10 z, +0x14 height) by
// its phase:
//   0  at the record's point (z 0x28000 behind, or 0x28000 ahead on side 1),
//      0x180 of height above it; +4 = ((Rand & 0x1FF) - 0x180) << 16 / 32; the
//      count 0x20; phase 1;
//   1  x - 0x2C00, z + 0xC00 (side 1: - 0xC00), height + +4; at the count's
//      end the angle (Rand & 0x1F) + 0xFC00 (side 1: + 0x400), the count 0x20
//      and the phase + the record's +6 + 1;
//   2  on a circle round the record's point ((cos - 0x5800) << 4, sin << 4) at
//      the angle, which turns 0xFF80 (side 1: 0x80) mod 0x1000; at the count's
//      end out of use;
//   3  the same without the count; the record's +6 at 2: +4 = (Rand & 0x7F) +
//      0x80, the count 0x10, phase 4;
//   4  flying out: x + cos * +4 >> 4, z + sin * +4 >> 4; at the count's end
//      out of use;
//   above 4 nothing;
// then drawn (EffectKind43_SparkDraw). al 1 when any record was live.
extern "C" unsigned char __cdecl EffectKind43_SparksRun(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char any = 0;
    unsigned char* r = At(at::kShards);
    for (unsigned n = at::kSparkCount43; n != 0; --n, r += at::kSparkStride43) {
        if (r[0] == 0) continue;
        switch (r[2]) {
        case 0: {
            const unsigned char* const s = S();
            const bool side = r[3] != 0;
            r[1] = 0x20;
            SetUL(r + 0xC, UL(s + 0x34));
            SetUL(r + 0x10, (side ? 0x50000u : 0u) + 0xFFFD8000u + UL(s + 0x38));
            SetUL(r + 0x14, UL(s + 0x3C) + 0x1800000u);
            const U v = ((static_cast<U>(SH_CALL(Rand)()) & 0x1FFu) - 0x180u) << 16;
            SetUL(r + 4, static_cast<U>(DivPow2(v, 5)));
            r[2] = static_cast<unsigned char>(r[2] + 1);
            break;
        }
        case 1: {
            SetUL(r + 0xC, UL(r + 0xC) + 0xFFFFD400u);
            const unsigned char count = r[1];
            SetUL(r + 0x10, UL(r + 0x10) + (r[3] != 0 ? 0xFFFFF400u : 0xC00u));
            SetUL(r + 0x14, UL(r + 0x14) + UL(r + 4));
            r[1] = static_cast<unsigned char>(count - 1);
            if (r[1] != 0) break;
            U v = static_cast<U>(SH_CALL(Rand)()) & 0x1Fu;
            v += r[3] != 0 ? 0x400u : 0xFC00u;
            const unsigned char* const s = S();
            SetWord(r + 8, v);
            r[1] = 0x20;
            r[2] = static_cast<unsigned char>(r[2] + s[6] + 1);
            break;
        }
        case 2:
        case 3: {
            const int c = SH_CALL(Math_Cos)(static_cast<int>(Word(r + 8)));
            SetUL(r + 0xC, ((static_cast<U>(c) - 0x5800u) << 4) + UL(S() + 0x34));
            const int sn = SH_CALL(Math_Sin)(static_cast<int>(Word(r + 8)));
            const unsigned char* const s = S();
            SetUL(r + 0x10, (static_cast<U>(sn) << 4) + UL(s + 0x38));
            const unsigned char count = r[1];
            SetWord(r + 8, ((r[3] != 0 ? 0x80u : 0xFF80u) + Word(r + 8)) & 0xFFFu);
            if (r[2] == 2) {
                r[1] = static_cast<unsigned char>(count - 1);
                if (r[1] == 0) r[0] = 0;
                break;
            }
            if (s[6] != 2) break;
            const U v = static_cast<U>(SH_CALL(Rand)()) & 0x7Fu;
            r[1] = 0x10;
            SetUL(r + 4, v + 0x80u);
            r[2] = static_cast<unsigned char>(r[2] + 1);
            break;
        }
        case 4: {
            const int c = SH_CALL(Math_Cos)(static_cast<int>(Word(r + 8)));
            SetUL(r + 0xC, UL(r + 0xC) + MulSar(c, static_cast<std::int32_t>(UL(r + 4)), 4));
            const int sn = SH_CALL(Math_Sin)(static_cast<int>(Word(r + 8)));
            SetUL(r + 0x10, UL(r + 0x10) + MulSar(sn, static_cast<std::int32_t>(UL(r + 4)), 4));
            r[1] = static_cast<unsigned char>(r[1] - 1);
            if (r[1] == 0) r[0] = 0;
            break;
        }
        default: break;
        }
        SH_CALL(EffectKind43_SparkDraw)(r);
        any = 1;
    }
    return any;
}

// original 0x476560 (0x114 bytes; in the span of 0x4762D0, in no list of the
// cut): a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 0) linked at the
// spark's point (MapView_LinkPrimAt(x, z, 0, 0xC)); its point projected; a
// semi-transparent TILE_1 there, shaded (0x80, 0x80, (Rand & 1) << 7), linked
// (0x14); a semi-transparent TILE of 3 x 3 one pixel up and left, shaded
// (0x20, 0x20, (Rand & 1) << 5), linked (0x1C).
extern "C" void __cdecl EffectKind43_SparkDraw(const unsigned char* spark) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tp & 0xFFFFu, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(spark + 0xC), UL(spark + 0x10), 0, 0xC);
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(spark + 0xC), Out(o));
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    Copy12(prim + 8, o);
    prim[5] = prim[4] = 0x80;
    prim[6] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) << 7);
    SH_CALL(MapView_LinkPrimAt)(UL(spark + 0xC), UL(spark + 0x10), 0, 0x14);
    prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    StF(prim + 8, F(o) - FAt(at::kOne));
    StF(prim + 0xC, F(o + 4) - FAt(at::kOne));
    SetUL(prim + 0x10, UL(o + 8));
    SetUL(prim + 0x14, 0x40400000u);   // 3.0
    SetUL(prim + 0x18, 0x40400000u);
    prim[5] = prim[4] = 0x20;
    prim[6] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 1u) << 5);
    SH_CALL(MapView_LinkPrimAt)(UL(spark + 0xC), UL(spark + 0x10), 0, 0x1C);
}

// ===========================================================================
// Kind 0x6B: Effect_KindHandlers[0x6B] (0x6554FC), EffectKind6B_States (five)
// ===========================================================================

// original 0x475DE0: jmp [EffectKind6B_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind6B_Run(void) {
    Dispatch("EffectKind6B_Run", EffectKind6B_States, EffectKind6B_States_count, S()[1]);
}

// original 0x475E00 (state 0): a free record (Effect_FindFree; none: +1 up and
// nothing else); VRAM (0x340, 0x100, 0x80, 0x100) cleared (Gfx_ClearRect);
// Sprite_Objects record 2's first 0x80 bytes copied into the record, in use,
// its point again, its screen position (+0x2E, +0x30) EffectKind6B_Frame's x
// and y, +0x24 | 0x88; with Sprite_Current the record, Sprite_UpdateScreen and
// Effect_Release (the borrowed record freed at once); Sprite_Current back, +9
// = 2 frames, +1 up.
extern "C" void __cdecl EffectKind6B_Capture(void) {
    const unsigned char found = SH_CALL(Effect_FindFree)();
    if (found == 0xFF) {
        NextState();
        return;
    }
    unsigned char* const e = NewRecord("EffectKind6B_Capture", found);
    SH_CALL(Gfx_ClearRect)(0x340, 0x100, 0x80, 0x100);
    for (unsigned i = 0; i < at::kEffectStride; i += 4) SetUL(e + i, UL(at::kSprite2 + i));
    e[0] = 1;
    SetUL(e + 0x34, UL(at::kSprite2Point));
    SetUL(e + 0x38, UL(at::kSprite2Point + 4));
    SetUL(e + 0x3C, UL(at::kSprite2Point + 8));
    SetWord(e + 0x2E, Word(At(at::kFrameX)));
    SetWord(e + 0x30, Word(At(at::kFrameY)));
    e[0x24] = static_cast<unsigned char>(e[0x24] | 0x88);
    unsigned char* const self = S();
    Sprite_Current = e;
    SH_CALL(Sprite_UpdateScreen)();
    SH_CALL(Effect_Release)();
    Sprite_Current = self;
    self[9] = 2;
    NextState();
}

// original 0x475EC0 (state 1): +9 down; at 0 the rectangle (0x340, 0x100, w,
// h) of EffectKind6B_Frame read back into 0x92BF80 (0x59E930, Gfx_VramShadow's
// rows) and +1 up.
extern "C" void __cdecl EffectKind6B_Store(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    alignas(4) short rect[4] = {0x340, 0x100, static_cast<short>(Word(At(at::kFrameW))), static_cast<short>(Word(At(at::kFrameH)))};
    SH_AT(void (__cdecl*)(const short*, void*), at::kStoreImage)(rect, At(at::kShards));
    NextState();
}

// original 0x475F20 (state 2): the 80 column heights cleared;
// EffectGte_LoadMapCamera; Sprite_Objects record 2's point projected (o); the
// count 0x676110 = 0; for each row below h and column below w (s16, the row
// and column bytes) a pixel of the read-back rectangle that is not 0 becomes a
// particle at 0x92EC80 + 0x14 n: its colour +2, its screen point ((o.x - x +
// column) * 16, (o.y - y + row) * 16) at +4 / +8 (floats), its depth +0xC;
// below its column's height (s16) that height becomes its y through _ftol;
// its count +1 (Rand & 7) - row + h's low byte, its speed +0x10 Rand & 0x1F,
// its column +0x12; +1 up. A w or h above 0xFF never ends in the original (the
// row and column are bytes): ours aborts, as past the 80 heights.
extern "C" void __cdecl EffectKind6B_Scatter(void) {
    for (unsigned i = 0; i < 2 * at::kHeightCount; i += 4) SetUL(at::kHeights + i, 0);
    SH_CALL(EffectGte_LoadMapCamera)();
    alignas(4) unsigned char q[12];
    SetUL(q, UL(at::kSprite2Point));
    SetUL(q + 4, UL(at::kSprite2Point + 4));
    SetUL(q + 8, UL(at::kSprite2Point + 8));
    alignas(4) unsigned char o[12];
    SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(o));
    U pixel = at::kShards;
    U particle = at::kPixels;
    SetWord(At(at::kPixelCount), 0);
    unsigned char row = 0;
    if (SW(at::kFrameH) <= 0) {
        NextState();
        return;
    }
    if (SW(at::kFrameH) > 0xFF || SW(at::kFrameW) > 0xFF)
        bof3::Fatal("EffectKind6B_Scatter: the rectangle is %d x %d - the original's row and column bytes never reach "
                    "past 0xFF, it never ends (docs/effect_2c.md section 6)",
                    (int)SW(at::kFrameW), (int)SW(at::kFrameH));
    U width = UL(at::kFrameW);
    do {
        unsigned char column = 0;
        if (static_cast<std::int16_t>(width) > 0) {
            do {
                const U colour = Word(At(pixel));
                if (colour != 0) {
                    unsigned char* const p = At(particle);
                    const std::int32_t x0 = SW(at::kFrameX);
                    SetWord(p + 2, colour);
                    const LD x = (F(o) - I(x0) + I(column)) * FAt(at::kSixteen);
                    const std::int32_t y0 = SW(at::kFrameY);
                    const U height = HeightAt("EffectKind6B_Scatter", column);
                    const std::int32_t lowest = SW(height);
                    SetUL(p + 0xC, UL(o + 8));
                    StF(p + 4, x);
                    const LD y = (F(o + 4) - I(y0) + I(row)) * FAt(at::kSixteen);
                    StF(p + 8, y);
                    if (y > I(lowest)) SetWord(At(height), Ftol(F(p + 8)));
                    U r = static_cast<U>(SH_CALL(Rand)());
                    p[1] = static_cast<unsigned char>((r & 7u) - row + B(at::kFrameH));
                    r = static_cast<U>(SH_CALL(Rand)());
                    SetWord(p + 0x10, r & 0x1Fu);
                    SetWord(p + 0x12, column);
                    width = UL(at::kFrameW);
                    particle += at::kPixelStride;
                    SetWord(At(at::kPixelCount), Word(At(at::kPixelCount)) + 1u);
                }
                pixel += 2;
                ++column;
            } while (static_cast<std::int16_t>(column) < static_cast<std::int16_t>(width));
        }
        ++row;
    } while (static_cast<std::int16_t>(row) < SW(at::kFrameH));
    NextState();
}

// original 0x4760E0 (state 3): every particle made (the count 0x676110, a word)
// flagged 3 - live, waiting on its count; Sound_PlayEffect(0x200); +1 up.
extern "C" void __cdecl EffectKind6B_Arm(void) {
    U n = Word(At(at::kPixelCount));
    for (U p = at::kPixels; n != 0; --n, p += at::kPixelStride) At(p)[0] = 3;
    SH_CALL(Sound_PlayEffect)(0x200);
    NextState();
}

// original 0x476120 (state 4): Sprite_Objects record 2's +0 | 0x40; each live
// particle (bit 0 of +0) by its flags' high nibble - 0: its count +1 down, at 0
// the count 0x10 and the flags + 0x10; 0x10: its speed +0x10 up 2, its y +8 up
// by it (a float), past its column's height out of use - then drawn either way
// as a TILE_1 at (x, y) / 16, its depth, the colour the 15-bit pixel's
// channels (<< 3, >> 2, >> 7, each & 0xF8), committed to slot 2 (0x14); none
// live (or none made) - Effect_Release.
extern "C" void __cdecl EffectKind6B_Fall(void) {
    B(at::kSprite2) = static_cast<unsigned char>(B(at::kSprite2) | 0x40);
    unsigned char none = 1;
    if (Word(At(at::kPixelCount)) != 0) {
        U i = 0;
        U particle = at::kPixels;
        do {
            unsigned char* const p = At(particle);
            const unsigned char flags = p[0];
            if (flags & 1) {
                const unsigned phase = flags & 0xF0u;
                if (phase == 0) {
                    p[1] = static_cast<unsigned char>(p[1] - 1);
                    if (p[1] == 0) {
                        const unsigned char f = p[0];
                        p[1] = 0x10;
                        p[0] = static_cast<unsigned char>(f + 0x10);
                    }
                } else if (phase == 0x10) {
                    SetWord(p + 0x10, Word(p + 0x10) + 2u);
                    const std::int32_t speed = SW(p + 0x10);
                    const U height = HeightAt("EffectKind6B_Fall", SW(p + 0x12));
                    const std::int32_t lowest = SW(height);
                    const LD y = I(speed) + F(p + 8);
                    StF(p + 8, y);
                    if (y > I(lowest)) p[0] = 0;
                }
                unsigned char* const prim = Gfx_PacketNext;
                SH_CALL(Gpu_SetTile1)(prim);
                StF(prim + 8, F(p + 4) * FAt(at::kSixteenth));
                StF(prim + 0xC, F(p + 8) * FAt(at::kSixteenth));
                SetUL(prim + 0x10, UL(p + 0xC));
                prim[4] = static_cast<unsigned char>(p[2] << 3);
                prim[5] = static_cast<unsigned char>((Word(p + 2) >> 2) & 0xF8u);
                prim[6] = static_cast<unsigned char>((Word(p + 2) >> 7) & 0xF8u);
                SH_CALL(Gfx_CommitPrim)(2, 0x14);
                none = 0;
            }
            particle += at::kPixelStride;
            ++i;
        } while ((i & 0xFFFFu) < Word(At(at::kPixelCount)));
    }
    if (none != 0) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x44: Effect_KindHandlers[0x44] (0x655460), EffectKind44_States (six)
// ===========================================================================

// original 0x476680 (in the span of 0x4762D0, in no list of the cut; catalog
// part 2): jmp [EffectKind44_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind44_Run(void) {
    Dispatch("EffectKind44_Run", EffectKind44_States, EffectKind44_States_count, S()[1]);
}

// original 0x4766A0 (state 0): the record's point Sprite_Objects record 1's;
// the ring cell 0x6761C0 = 0x92BF80, whose top point is the record's point
// 0x800 up (+8 + 0x8000000) and centre the record's point, radius 0, shades
// 0x20 and 0; the sparks cleared (EffectKind44_SparksClear); +9 = 0x10, +1 up.
extern "C" void __cdecl EffectKind44_Start(void) {
    SetUL(S() + 0x34, UL(at::kSprite1Point));
    SetUL(S() + 0x38, UL(at::kSprite1Point + 4));
    SetUL(S() + 0x3C, UL(at::kSprite1Point + 8));
    const unsigned char* const s = S();
    SetUL(at::kRingCell, at::kShards);
    SetUL(at::kShards, UL(s + 0x34));
    SetUL(at::kShards + 4, UL(s + 0x38));
    SetUL(at::kShards + 8, UL(s + 0x3C) + 0x8000000u);
    SetUL(at::kShards + 0x10, UL(s + 0x34));
    SetUL(at::kShards + 0x14, UL(s + 0x38));
    SetUL(at::kShards + 0x18, UL(s + 0x3C));
    B(at::kShards + 0x22) = 0x20;
    B(at::kShards + 0x23) = 0;
    SetWord(At(at::kShards + 0x20), 0);
    SH_CALL(EffectKind44_SparksClear)();
    S()[9] = 0x10;
    NextState();
}

namespace {
unsigned char* Ring() { return At(UL(at::kRingCell)); }
}  // namespace

// original 0x476750 (state 1): the ring's shade +0x23 up 3; projected and
// drawn; +9 down, at 0 the sparks cleared, +9 = 0x78 and +1 up.
extern "C" void __cdecl EffectKind44_FadeIn(void) {
    Ring()[0x23] = static_cast<unsigned char>(Ring()[0x23] + 3);
    SH_CALL(EffectKind44_RingProject)(Ring());
    SH_CALL(EffectKind44_RingDraw)(Ring());
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] != 0) return;
    SH_CALL(EffectKind44_SparksClear)();
    S()[9] = 0x78;
    NextState();
}

// original 0x4767B0 (state 2): the ring projected and drawn, the sparks
// stepped and one emitted (EffectKind44_SparkEmit); +9 down, at 0 +9 = 0x1E,
// +1 up.
extern "C" void __cdecl EffectKind44_Sparks(void) {
    SH_CALL(EffectKind44_RingProject)(Ring());
    SH_CALL(EffectKind44_RingDraw)(Ring());
    SH_CALL(EffectKind44_SparksStep)();
    SH_CALL(EffectKind44_SparkEmit)();
    CountDown(0x1E);
}

// original 0x476800 (state 3): the ring's radius word +0x20 up 0x30, at most
// 0x180 (signed); projected, drawn, its cone drawn (EffectKind44_RingCone), the
// sparks stepped; +9 down, at 0 +9 = 0x78, +1 up.
extern "C" void __cdecl EffectKind44_Widen(void) {
    SetWord(Ring() + 0x20, Word(Ring() + 0x20) + 0x30u);
    unsigned char* ring = Ring();
    if (SW(ring + 0x20) > 0x180) {
        SetWord(ring + 0x20, 0x180);
        ring = Ring();
    }
    SH_CALL(EffectKind44_RingProject)(ring);
    SH_CALL(EffectKind44_RingDraw)(Ring());
    SH_CALL(EffectKind44_RingCone)(Ring());
    SH_CALL(EffectKind44_SparksStep)();
    CountDown(0x78);
}

// original 0x476870 (state 4): the ring's centre Sprite_Objects record 1's
// point; projected, drawn, its cone, the sparks; +9 down, at 0 +9 = 0x10, +1 up.
extern "C" void __cdecl EffectKind44_Follow(void) {
    SetUL(Ring() + 0x10, UL(at::kSprite1Point));
    SetUL(Ring() + 0x14, UL(at::kSprite1Point + 4));
    SetUL(Ring() + 0x18, UL(at::kSprite1Point + 8));
    SH_CALL(EffectKind44_RingProject)(Ring());
    SH_CALL(EffectKind44_RingDraw)(Ring());
    SH_CALL(EffectKind44_RingCone)(Ring());
    SH_CALL(EffectKind44_SparksStep)();
    CountDown(0x10);
}

// original 0x4768F0 (state 5): the ring's shade +0x23 down 3; projected,
// drawn, its cone, the sparks; +9 down, at 0 Effect_Release (a tail jmp).
extern "C" void __cdecl EffectKind44_FadeOut(void) {
    Ring()[0x23] = static_cast<unsigned char>(Ring()[0x23] - 3);
    SH_CALL(EffectKind44_RingProject)(Ring());
    SH_CALL(EffectKind44_RingDraw)(Ring());
    SH_CALL(EffectKind44_RingCone)(Ring());
    SH_CALL(EffectKind44_SparksStep)();
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x476950 (0xA2 bytes): EffectGte_LoadMapCamera; the top point
// projected to +0x24, the centre to +0x30; sixteen points of the circle of
// radius +0x20 (s16) round the centre at the angles 0, 0x100, .. ((cos, sin) *
// radius >> 4) projected to +0x3C + 0xC i.
extern "C" void __cdecl EffectKind44_RingProject(unsigned char* ring) {
    SH_CALL(EffectGte_LoadMapCamera)();
    SH_CALL(EffectGte_ProjectPoint)(Point(ring), Out(ring + 0x24));
    SH_CALL(EffectGte_ProjectPoint)(Point(ring + 0x10), Out(ring + 0x30));
    U angle = 0;
    unsigned char* out = ring + 0x3C;
    for (unsigned n = 0x10; n != 0; --n) {
        const U t = angle & 0xFFFFu;
        alignas(4) unsigned char q[12];
        U v = MulSar(SH_CALL(Math_Cos)(static_cast<int>(t)), SW(ring + 0x20), 4);
        SetUL(q, v + UL(ring + 0x10));
        v = MulSar(SH_CALL(Math_Sin)(static_cast<int>(t)), SW(ring + 0x20), 4);
        SetUL(q + 4, v + UL(ring + 0x14));
        SetUL(q + 8, UL(ring + 0x18));
        SH_CALL(EffectGte_ProjectPoint)(Point(q), Out(out));
        out += 0xC;
        angle += 0x100;
    }
}

// original 0x476A00 (0x1F2 bytes): a draw mode (abr 2, slot 1); for each
// ring point i and the next j, a semi-transparent POLY_F4 of the edge i-j and
// its run to the screen's side (x 320.0 when the edge's middle is right of the
// centre's projection, else 0), shaded +0x23, committed 0x38 - the lowest and
// highest screen y met kept (_ftol, from 0 and 0xF0); then two TILEs of
// 320 across, above the highest (from 0) and below the lowest (to 0xF0), the
// same shade, committed 0x1C each (their +0x10 not written).
extern "C" void __cdecl EffectKind44_RingDraw(const unsigned char* ring) {
    DrawMode(2, 1, 1);
    U lowest = 0;
    U highest = 0xF0;
    const unsigned char* pi = ring + 0x3C;
    unsigned char i = 0;
    do {
        const unsigned j = (i + 1u) & 0xFu;
        if (!(I(static_cast<std::int16_t>(lowest)) >= F(pi + 4))) lowest = Ftol(F(pi + 4));
        if (I(static_cast<std::int16_t>(highest)) > F(pi + 4)) highest = Ftol(F(pi + 4));
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyF4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        const unsigned char* const pj = ring + 0x3C + 12u * j;
        SetUL(prim + 8, UL(pi));
        LD y = F(pi + 4);
        StF(prim + 0x18, y);
        StF(prim + 0xC, y);
        SetUL(prim + 0x20, UL(pj));
        y = F(pj + 4);
        StF(prim + 0x30, y);
        StF(prim + 0x24, y);
        const LD middle = (F(pi) + F(pj)) * FAt(at::kHalf);
        const U side = middle > F(ring + 0x30) ? 0x43A00000u : 0u;   // 320.0 or 0
        SetUL(prim + 0x2C, side);
        SetUL(prim + 0x14, side);
        LD z = F(pi + 8);
        StF(prim + 0x1C, z);
        StF(prim + 0x10, z);
        z = F(pj + 8);
        StF(prim + 0x34, z);
        StF(prim + 0x28, z);
        prim[6] = prim[5] = prim[4] = ring[0x23];
        SH_CALL(Gfx_CommitPrim)(1, 0x38);
        ++i;
        pi += 0xC;
    } while (i < 0x10);
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SetUL(prim + 8, 0);
    SetUL(prim + 0xC, 0);
    SetUL(prim + 0x14, 0x43A00000u);
    StF(prim + 0x18, I(static_cast<std::int16_t>(highest)));
    prim[6] = prim[5] = prim[4] = ring[0x23];
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
    prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const std::int32_t below = static_cast<std::int16_t>(lowest);
    SetUL(prim + 8, 0);
    SetUL(prim + 0x14, 0x43A00000u);
    StF(prim + 0xC, I(below));
    StF(prim + 0x18, I(static_cast<std::int32_t>(0xF0u - static_cast<U>(below))));
    prim[6] = prim[5] = prim[4] = ring[0x23];
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x476C00 (0xFE bytes): a draw mode (abr 1, slot 1); for each ring
// point i and the next j, the triangle top-i-j where it faces the eye (0x4941B0's
// ax above 0): a semi-transparent POLY_F3 (0x5A7570) shaded +0x22, committed 0x2C.
extern "C" void __cdecl EffectKind44_RingCone(const unsigned char* ring) {
    DrawMode(1, 1, 1);
    const unsigned char* pi = ring + 0x3C;
    unsigned char step = 0;
    do {
        ++step;
        const unsigned char* const pj = ring + 0x3C + 12u * (step & 0xFu);
        const U facing = SH_AT(U (__cdecl*)(const unsigned char*, const unsigned char*, const unsigned char*),
                               at::kWinding)(ring + 0x24, pi, pj);
        if (static_cast<std::int16_t>(facing) > 0) {
            unsigned char* const prim = Gfx_PacketNext;
            SH_AT(void (__cdecl*)(unsigned char*), at::kSetPolyF3)(prim);
            SH_CALL(Gpu_SetSemiTrans)(prim, 1);
            Copy12(prim + 8, ring + 0x24);
            Copy12(prim + 0x14, pi);
            Copy12(prim + 0x20, pj);
            prim[6] = prim[5] = prim[4] = ring[0x22];
            SH_CALL(Gfx_CommitPrim)(1, 0x2C);
        }
        pi += 0xC;
    } while (step < 0x10);
}

// original 0x476D00 (0x1C bytes): the spark cursor 0x6761B8 = 0x92C07C, then
// eight times the in-use byte it points at cleared - the cursor never moves,
// so only the first of the eight records is (docs/effect_2c.md section 7).
extern "C" void __cdecl EffectKind44_SparksClear(void) {
    SetUL(at::kSparkCursor, at::kSparks44);
    for (unsigned n = at::kSparkCount44; n != 0; --n) At(UL(at::kSparkCursor))[0] = 0;
}

// original 0x476D20 (0x90 bytes): each live spark of the eight (the cursor on
// it): its ground point +0x28 / +0x2C moved by +8 / +0xC, its height +0x30 the
// ground's (AreaMap_Elevation's s16 << 16); its trail drawn
// (EffectKind44_SparkTrail); its life +1 down, at 0 out of use.
extern "C" void __cdecl EffectKind44_SparksStep(void) {
    U p = at::kSparks44;
    SetUL(at::kSparkCursor, p);
    for (unsigned n = at::kSparkCount44; n != 0; --n) {
        if (At(p)[0] != 0) {
            SetUL(At(p) + 0x28, UL(At(p) + 0x28) + UL(At(p) + 8));
            unsigned char* q = At(UL(at::kSparkCursor));
            SetUL(q + 0x2C, UL(q + 0x2C) + UL(q + 0xC));
            q = At(UL(at::kSparkCursor));
            const long ground = SH_CALL(AreaMap_Elevation)(Long(q + 0x28), Long(q + 0x2C));
            SetUL(At(UL(at::kSparkCursor)) + 0x30, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16);
            SH_CALL(EffectKind44_SparkTrail)(At(UL(at::kSparkCursor)));
            q = At(UL(at::kSparkCursor));
            q[1] = static_cast<unsigned char>(q[1] - 1);
            q = At(UL(at::kSparkCursor));
            if (q[1] == 0) q[0] = 0;
            p = UL(at::kSparkCursor);
        }
        p += at::kSparkStride44;
        SetUL(at::kSparkCursor, p);
    }
}

// original 0x476DB0 (0x206 bytes): a draw mode (abr 1, slot 1);
// EffectGte_LoadMapCamera; the spark's two histories of four screen points
// (+0x40 the high end's, +0x70 the ground end's) shifted one on and the newest
// projected (+0x18, +0x28); a semi-transparent LINE_F2 between the newest two
// in the spark's colour +4, committed 0x20; three semi-transparent POLY_G4
// between the histories' successive pairs, the colour down a quarter of itself
// (bytes, wrapping) at each step, committed 0x44 each.
extern "C" void __cdecl EffectKind44_SparkTrail(unsigned char* spark) {
    DrawMode(1, 1, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned k = 3; k != 0; --k) {
        Copy12(spark + 0x40 + 12 * k, spark + 0x40 + 12 * (k - 1));
        Copy12(spark + 0x70 + 12 * k, spark + 0x70 + 12 * (k - 1));
    }
    SH_CALL(EffectGte_ProjectPoint)(Point(spark + 0x18), Out(spark + 0x40));
    SH_CALL(EffectGte_ProjectPoint)(Point(spark + 0x28), Out(spark + 0x70));
    const U colour = UL(spark + 4);
    auto red = static_cast<unsigned char>(colour);
    auto green = static_cast<unsigned char>(colour >> 8);
    auto blue = static_cast<unsigned char>(colour >> 16);
    const auto dr = static_cast<unsigned char>(red >> 2);
    const auto dg = static_cast<unsigned char>(green >> 2);
    const auto db = static_cast<unsigned char>(blue >> 2);
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    Copy12(prim + 8, spark + 0x40);
    Copy12(prim + 0x14, spark + 0x70);
    prim[4] = red;
    prim[5] = green;
    prim[6] = blue;
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
    const unsigned char* a = spark + 0x40;
    const unsigned char* b = spark + 0x70;
    for (unsigned n = 3; n != 0; --n) {
        prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        Copy12(prim + 8, a);
        Copy12(prim + 0x18, b);
        prim[4] = red;
        prim[0x14] = red;
        red = static_cast<unsigned char>(red - dr);
        prim[5] = green;
        prim[0x15] = green;
        green = static_cast<unsigned char>(green - dg);
        prim[6] = blue;
        prim[0x16] = blue;
        blue = static_cast<unsigned char>(blue - db);
        Copy12(prim + 0x28, a + 0xC);
        Copy12(prim + 0x38, b + 0xC);
        prim[0x24] = red;
        prim[0x25] = green;
        prim[0x26] = blue;
        prim[0x34] = red;
        prim[0x35] = green;
        prim[0x36] = blue;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        a += 0xC;
        b += 0xC;
    }
}

// original 0x476FC0 (0x1B5 bytes): a free spark (EffectKind44_SparkFind, the
// cursor its answer); on every fourth frame (Frame_Counter & 3 = 0) and a
// record: its high end the record's point 0x600 up (+0x20 + 0x6000000), its
// ground end at the angle e = Rand & 0xFFF ((cos, sin) << 8 from the record's
// point) on the ground, its speed (cos, sin)(e + 0x800 + (Rand & 0x1FF) -
// 0x100) << 4, +0x10 = 0, its colour EffectKind44_SparkColours[Rand & 7], in
// use, life 0x20; EffectGte_LoadMapCamera; both ends projected into the first
// history points and copied to the other three.
extern "C" void __cdecl EffectKind44_SparkEmit(void) {
    unsigned char* const found = SH_CALL(EffectKind44_SparkFind)();
    const unsigned frame = Frame_Counter & 0xFFu;
    SetUL(at::kSparkCursor, AddressOf(found));
    if ((frame & 3u) != 0 || found == nullptr) return;
    SetUL(found + 0x18, UL(S() + 0x34));
    SetUL(At(UL(at::kSparkCursor)) + 0x1C, UL(S() + 0x38));
    SetUL(At(UL(at::kSparkCursor)) + 0x20, UL(S() + 0x3C) + 0x6000000u);
    U e = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    U spread = static_cast<U>(SH_CALL(Rand)()) & 0x1FFu;
    e &= 0xFFFFu;
    spread -= 0x100u;
    U v = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(e))) << 8;
    SetUL(At(UL(at::kSparkCursor)) + 0x28, v + UL(S() + 0x34));
    v = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(e))) << 8;
    SetUL(At(UL(at::kSparkCursor)) + 0x2C, v + UL(S() + 0x38));
    const unsigned char* q = At(UL(at::kSparkCursor));
    const long ground = SH_CALL(AreaMap_Elevation)(Long(q + 0x28), Long(q + 0x2C));
    const U angle = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(spread))) + e + 0x800u;
    SetUL(At(UL(at::kSparkCursor)) + 0x30, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16);
    v = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))) << 4;
    SetUL(At(UL(at::kSparkCursor)) + 8, v);
    v = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))) << 4;
    SetUL(At(UL(at::kSparkCursor)) + 0xC, v);
    SetUL(At(UL(at::kSparkCursor)) + 0x10, 0);
    const U pick = static_cast<U>(SH_CALL(Rand)()) & 7u;
    SetUL(At(UL(at::kSparkCursor)) + 4, UL(at::kSparkColours + 4 * pick));
    At(UL(at::kSparkCursor))[0] = 1;
    At(UL(at::kSparkCursor))[1] = 0x20;
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* sp = At(UL(at::kSparkCursor));
    SH_CALL(EffectGte_ProjectPoint)(Point(sp + 0x18), Out(sp + 0x40));
    sp = At(UL(at::kSparkCursor));
    SH_CALL(EffectGte_ProjectPoint)(Point(sp + 0x28), Out(sp + 0x70));
    for (unsigned k = 1; k <= 3; ++k) {
        sp = At(UL(at::kSparkCursor));
        Copy12(sp + 0x40 + 12 * k, sp + 0x40);
        sp = At(UL(at::kSparkCursor));
        Copy12(sp + 0x70 + 12 * k, sp + 0x70);
    }
}

// original 0x477180 (0x25 bytes): the first spark record of the eight not in
// use, the cursor 0x6761B8 left on it (past the last, and 0 answered, when none).
extern "C" unsigned char* __cdecl EffectKind44_SparkFind(void) {
    U p = at::kSparks44;
    SetUL(at::kSparkCursor, p);
    for (unsigned char n = 0;;) {
        if (At(p)[0] == 0) return At(p);
        p += at::kSparkStride44;
        ++n;
        SetUL(at::kSparkCursor, p);
        if (n >= at::kSparkCount44) return nullptr;
    }
}

void Effect2C_Inject() {
    if (bof3::WantsShadow("effect_2c")) effect_2c::SelfTest();
    BOF3_INJECT(EffectKind3E_Run);
    BOF3_INJECT(EffectKind3E_Start);
    BOF3_INJECT(EffectKind3E_Bolts);
    BOF3_INJECT(EffectKind3E_SubWait);
    BOF3_INJECT(EffectKind3E_SubSound);
    BOF3_INJECT(EffectKind3E_DrawBolt);
    BOF3_INJECT(EffectKind3E_BoltGlow);
    BOF3_INJECT(EffectKind3F_Run);
    BOF3_INJECT(EffectKind3F_Draw);
    BOF3_INJECT(EffectKind3F_DrawBeam);
    BOF3_INJECT(EffectKind3F_DrawCap);
    BOF3_INJECT(EffectKind3F_DrawBand);
    BOF3_INJECT(EffectKind40_Run);
    BOF3_INJECT(EffectKind40_Start);
    BOF3_INJECT(EffectKind40_Grow);
    BOF3_INJECT(EffectKind40_ShardsClear);
    BOF3_INJECT(EffectKind40_ShardsStep);
    BOF3_INJECT(EffectKind40_ShardDraw);
    BOF3_INJECT(EffectKind40_ShardSpawn);
    BOF3_INJECT(EffectKind40_ShardFind);
    BOF3_INJECT(EffectKind40_DrawDisc);
    BOF3_INJECT(EffectKind42_Run);
    BOF3_INJECT(EffectKind42_Glow);
    BOF3_INJECT(EffectKind43_Run);
    BOF3_INJECT(EffectKind43_Start);
    BOF3_INJECT(EffectKind43_Gather);
    BOF3_INJECT(EffectKind43_Wait);
    BOF3_INJECT(EffectKind43_Fade);
    BOF3_INJECT(EffectKind43_Setup);
    BOF3_INJECT(EffectKind43_AddSpark);
    BOF3_INJECT(EffectKind43_SparksRun);
    BOF3_INJECT(EffectKind43_SparkDraw);
    BOF3_INJECT(EffectKind6B_Run);
    BOF3_INJECT(EffectKind6B_Capture);
    BOF3_INJECT(EffectKind6B_Store);
    BOF3_INJECT(EffectKind6B_Scatter);
    BOF3_INJECT(EffectKind6B_Arm);
    BOF3_INJECT(EffectKind6B_Fall);
    BOF3_INJECT(EffectKind44_Run);
    BOF3_INJECT(EffectKind44_Start);
    BOF3_INJECT(EffectKind44_FadeIn);
    BOF3_INJECT(EffectKind44_Sparks);
    BOF3_INJECT(EffectKind44_Widen);
    BOF3_INJECT(EffectKind44_Follow);
    BOF3_INJECT(EffectKind44_FadeOut);
    BOF3_INJECT(EffectKind44_RingProject);
    BOF3_INJECT(EffectKind44_RingDraw);
    BOF3_INJECT(EffectKind44_RingCone);
    BOF3_INJECT(EffectKind44_SparksClear);
    BOF3_INJECT(EffectKind44_SparksStep);
    BOF3_INJECT(EffectKind44_SparkTrail);
    BOF3_INJECT(EffectKind44_SparkEmit);
    BOF3_INJECT(EffectKind44_SparkFind);
}
