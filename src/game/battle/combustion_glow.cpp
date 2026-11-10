// Moved here by tools/move_funcs.py from magic_s37.cpp (docs/source-layout-round.md).
#include "game/battle/combustion_glow.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

constexpr auto& LongMagicS37 = move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(LongMagicS37(at)) + v)); }

void CopyLong(unsigned char* to, const unsigned char* from) { SetLong(to, LongMagicS37(from)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void Dispatch(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// The callees with the arguments the originals push: both projections get a
// flag pointer past the depth, which ours does not read.
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S37_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// Prim_VertexScratch's first three SVECTORs projected to +8, +0x18, +0x28.
void Rtp3(unsigned char* prim) {
    long p, flag;
    S37_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}

// All four projected to +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S37_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38, &p,
                                     &flag);
}

// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The draws' shared shape: a fan of 64 semi-transparent gouraud triangles
// round the origin (Prim_VertexScratch: the origin, the last rim point, the
// next at the angle; the radius the word 0x903850, all nine colour bytes the
// byte 0x90385A, both read again after every call), tpage 0x55, layer 5.
// The caller has set the two words.
void DrawGlowFan() {
    int v = Sin(0);
    SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x14, 0);
    SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = VW(0x10), y = VW(0x12);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        v = Sin(a);
        SetVW(0x10, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x12, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
}

// The other: a ring of 64 semi-transparent gouraud quads between the radius
// word 0x903852 (vertices 0 and 1, colour 1) and the radius word 0x903850
// (vertices 2 and 3, the byte 0x90385A), tpage 0x55, layer 5.
void DrawRingBand() {
    int v = Sin(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Cos(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    v = Sin(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Cos(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
    SetVW(0x14, 0);
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = VW(8), y = VW(0xA);
            SetVW(0, x);
            SetVW(2, y);
        }
        v = Sin(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = Cos(a);
        {
            const int inner = Mul12(v, SS(2));
            const std::uint16_t x = VW(0x18);
            SetVW(0xA, static_cast<unsigned>(inner));
            const std::uint16_t y = VW(0x1A);
            SetVW(0x10, x);
            SetVW(0x12, y);
        }
        v = Sin(a);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Cos(a);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(0))));
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = SB(0xA);
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
}

}  // namespace

#define S37_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4F7750: a call through CombustionGlow_Steps (four: _Start, _Grow,
// group S17's MagicFx_WaitA, group S12's MagicFx_CountDownRelease) by +2;
// then while +0 and +2 are not 0 the actor matrix, the glow, the ring, the
// matrix popped (a tail jmp).
S37_EXPORT void __cdecl CombustionGlow_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {addr::CombustionGlow_Start, addr::CombustionGlow_Grow, addr::MagicFx_WaitA,
                                                addr::MagicFx_CountDownRelease};
    Dispatch(kSteps, 4, Sc()[2], "CombustionGlow_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::CombustionGlow_DrawGlow);
    Call0(addr::CombustionGlow_DrawRing);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4F7790: +9 down; at 0 the glow at its owner's point, the radius
// +0x14 0, +9 0, +0xA 0x26, +2 on.
S37_EXPORT void __cdecl CombustionGlow_Start(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    CopyLong(s + 0x34, Owner() + 0x34);
    CopyLong(s + 0x38, Owner() + 0x38);
    CopyLong(s + 0x3C, Owner() + 0x3C);
    rv::Store(rv::As<records::BattleTask>(s)->step_y, 0);
    s[9] = 0;
    s[0xA] = 0x26;
    Inc(s[2]);
}

// original 0x4F7800: +9 (the shade) up, the radius +0x14 up by 6; at +9 0x10
// +2 on.
S37_EXPORT void __cdecl CombustionGlow_Grow(void) {
    unsigned char* const s = Sc();
    Inc(s[9]);
    AddLong(s + 0x14, 6);
    if (s[9] == 0x10) Inc(s[2]);
}

// original 0x4F7830: the glow fan, radius the word +0x14, colour +9 x 4.
S37_EXPORT void __cdecl CombustionGlow_DrawGlow(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
        SetSW(0xA, static_cast<unsigned>(s[9]) << 2);
    }
    DrawGlowFan();
}

// original 0x4F79D0: the ring, from radius (the word +0x14) x 2 (dark) in to
// the word +0x14 (colour +9 x 4).
S37_EXPORT void __cdecl CombustionGlow_DrawRing(void) {
    DrawMode(0x55);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(0, Word(s + 0x14));
        SetSW(2, static_cast<unsigned>(Word(s + 0x14)) << 1);
        SetSW(0xA, static_cast<unsigned>(s[9]) << 2);
    }
    DrawRingBand();
}

void CombustionGlow_Inject() {
    BOF3_INJECT(CombustionGlow_Run);
    BOF3_INJECT(CombustionGlow_Start);
    BOF3_INJECT(CombustionGlow_Grow);
    BOF3_INJECT(CombustionGlow_DrawGlow);
    BOF3_INJECT(CombustionGlow_DrawRing);
}
