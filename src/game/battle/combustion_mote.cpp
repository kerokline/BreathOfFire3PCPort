// Moved here by tools/move_funcs.py from magic_s37.cpp (docs/source-layout-round.md).
#include "game/battle/combustion_mote.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s37.h"
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

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(LongMagicS37(at)) + v)); }

void CopyLong(unsigned char* to, const unsigned char* from) { SetLong(to, LongMagicS37(from)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `imul` alone: the 32-bit product, wrapped.
std::uint32_t Mul(int a, int b) { return static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

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

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);

#define S37_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// All four projected to +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S37_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38, &p,
                                     &flag);
}

// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// Gfx_PacketNext's primitive of `size` linked at Sprite_Current's point.
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), 2,
                                size);
}

// The textured quad of a record or mote: page (0x340, 0x100) abr 1, clut
// (0, 0x1E2), the texture's 0x20 x 0x20 at (0, 0x80); corners at the angles
// 0x200, 0x600, 0xE00, 0xA00, radius 0x100; the colour the byte 0x90385A
// (the word +0xA x `shade`, +0xA read after the packet is set up); tpage
// 0x35, linked at the sprite's point before and after.
void DrawTexturedQuad(unsigned shade) {
    DrawMode(0x35);
    LinkAtSprite(0xC);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    {
        const unsigned b = Sc()[0xA];
        SetSW(0, 0x100);
        SetSW(0xA, b * shade);
    }
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned c = 0; c < 4; ++c) {
        int v = Cos(kAngles[c]);
        SetVW(c * 8, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Sin(kAngles[c]);
        SetVW(c * 8 + 2, static_cast<unsigned>(Mul12(v, SS(0))));
        SetVW(c * 8 + 4, 0);
    }
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E2));
    p[0x15] = 0x80;
    p[0x25] = 0x80;
    p[0x14] = 0;
    p[0x24] = 0x20;
    p[0x34] = 0;
    p[0x35] = 0xA0;
    p[0x44] = 0x20;
    p[0x45] = 0xA0;
    p[4] = SB(0xA);
    p[5] = SB(0xA);
    p[6] = SB(0xA);
    Rtp4(p);
    MH_CALL(Gte_PrimDepths4_10)(p);
    LinkAtSprite(0x48);
}

}  // namespace

#define S37_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The mote on its orbit: at the angle +0xB << 9 (the word 0x903854) round its
// owner, radius +0xC (no shift), Sprite_Current read again after each call.
void MoteOrbit() {
    const auto angle = static_cast<std::uint16_t>(static_cast<unsigned>(Sc()[0xB]) << 9);
    SetSW(4, angle);
    int v = Sin(static_cast<short>(angle));
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->x = static_cast<std::int32_t>(Mul(v, ts->step_x) + U(rv::As<records::SpriteHead>(Owner())->x));
    }
    v = Cos(SS(4));
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    ts->z = static_cast<std::int32_t>(Mul(v, ts->step_x) + U(rv::As<records::SpriteHead>(Owner())->z));
}

}  // namespace

// original 0x4F7C00: a call through CombustionMote_Steps (three) by +2; then
// while +0 and +2 are not 0 the actor matrix, the quad, the matrix popped (a
// tail jmp). MAGIC053's LavaburstRecord_Steps holds this unit's
// CombustionMote_Start too.
S37_EXPORT void __cdecl CombustionMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {addr::CombustionMote_Start, addr::CombustionMote_Grow,
                                                addr::CombustionMote_Shrink};
    Dispatch(kSteps, 3, Sc()[2], "CombustionMote_Run");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::CombustionMote_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4F7C40: the radius +0xC (Rand & 7) + 8, the mote on its orbit at
// its owner's height; +9 8, +0xA 0x10, +2 on.
S37_EXPORT void __cdecl CombustionMote_Start(void) {
    const std::uint32_t r = RandCall();
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>((r & 7) + 8));
    MoteOrbit();
    CopyLong(Sc() + 0x3C, Owner() + 0x3C);
    unsigned char* const s = Sc();
    s[9] = 8;
    s[0xA] = 0x10;
    Inc(s[2]);
}

// original 0x4F7CE0: the radius up by 1, the orbit; +9 down, at 0 +2 on.
S37_EXPORT void __cdecl CombustionMote_Grow(void) {
    AddLong(Sc() + 0xC, 1);
    MoteOrbit();
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4F7D70: the radius up by 1, the orbit; +0xA down, at 0 the
// owner's count +0xB down and the task freed (a tail jmp).
S37_EXPORT void __cdecl CombustionMote_Shrink(void) {
    AddLong(Sc() + 0xC, 1);
    MoteOrbit();
    unsigned char* const s = Sc();
    Dec(s[0xA]);
    if (s[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F7E10: the textured quad, its colour +0xA x 8.
S37_EXPORT void __cdecl CombustionMote_Draw(void) { DrawTexturedQuad(8); }

void CombustionMote_Inject() {
    BOF3_INJECT(CombustionMote_Run);
    BOF3_INJECT(CombustionMote_Start);
    BOF3_INJECT(CombustionMote_Grow);
    BOF3_INJECT(CombustionMote_Shrink);
    BOF3_INJECT(CombustionMote_Draw);
}
