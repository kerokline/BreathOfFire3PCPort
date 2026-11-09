// Moved here by tools/move_funcs.py from battle_items.cpp, magic_fx_reached.cpp, magic_s10.cpp (docs/source-layout-round.md).
#include "game/magic/ebonfire_ring.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_items.h"
#include "game/battle_items_callees.h"
#include "game/cheats.h"
#include "game/magic_fx_reached.h"
#include "game/magic_fx_reached_callees.h"
#include "game/magic_harness.h"
#include "game/magic_s10.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/sound.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

using namespace battle_items;

using namespace magic_fx_reached;

namespace battle_items {

using move_script::At;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_items

namespace {

// The scratch words: SharedScratch + k, and the vertex scratch + k.
unsigned char* Scr(unsigned k) { return battle_items::At(::bof3::Img(battle_items::at::kScratch.v) + k); }

short S16BattleItems(const unsigned char* at) { return static_cast<short>(battle_items::Word(at)); }

unsigned char* Vtx(unsigned k) { return battle_items::At(::bof3::Img(Prim_VertexScratch_addr) + k); }

const short* VtxP(unsigned k) { return reinterpret_cast<const short*>(Vtx(k)); }

// `imul` then `sar 0xC`: a 32-bit product that wraps, shifted arithmetically.
int Mul12(int a, int b) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12;
}

}  // namespace

namespace magic_fx_reached {

using move_script::At;

using move_script::Long;

namespace {

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace magic_fx_reached

namespace {

unsigned char* Ptr(std::uint32_t cell) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(magic_fx_reached::Long(magic_fx_reached::At(cell)))));
}

unsigned char Byte(std::uint32_t address) { return magic_fx_reached::At(address)[0]; }

void Bump(unsigned char& b, int by) { b = static_cast<unsigned char>(b + by); }

}  // namespace

#define MFX_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return move_script::Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return move_script::Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short VS(unsigned k) { return static_cast<short>(VW(k)); }

void SetVW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// `shl n` on a dword.
std::uint32_t Shl(int v, unsigned n) { return static_cast<std::uint32_t>(v) << n; }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions, and other groups' (bof3::addr), called by address,
// as the originals call them: in the game the jmp Inject put there (or
// Capcom's code), in the fuzz that address's recorder.
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

#define S10_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// All four projected to +8, +8 + step, +8 + 2 step, +8 + 3 step.
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S10_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

}  // namespace

#define S10_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4AD130 (shared with the twin effect 0x4ACFE0): +9 and +0xA up
// by one; at +9 = 0x10 the phase on.
MFX_EXPORT void __cdecl FxDiscFan_Grow(void) {
    Bump(Sprite_Current[9], 1);
    Bump(Sprite_Current[0xA], 1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0x10) Bump(sc[1], 1);
}

// original 0x4AD160 (shared with 0x4ACFE0): the fade, as the rings go. With
// fewer than four rings left +9 steps down to 0, with fewer than three +0xA;
// with none left and +0xA at 0, the target's flag 0x40, 0x904AA8 bit 2 (the
// effect is done) and the task freed (a tail jmp in the original).
MFX_EXPORT void __cdecl FxDiscFan_Fade(void) {
    unsigned char* sc = Sprite_Current;
    if (sc[0xB] < 4 && sc[9] != 0) {
        Bump(sc[9], -1);
        sc = Sprite_Current;
    }
    if (sc[0xB] < 3 && sc[0xA] != 0) {
        Bump(sc[0xA], -1);
        sc = Sprite_Current;
    }
    if (sc[0xB] != 0 || sc[0xA] != 0) return;
    magic_fx_reached::g.set_target_flag40(Byte(::bof3::Img(magic_fx_reached::at::kTarget)));
    magic_fx_reached::At(::bof3::Img(magic_fx_reached::at::kFlags))[0] = static_cast<unsigned char>(magic_fx_reached::At(::bof3::Img(magic_fx_reached::at::kFlags))[0] | 4);
    magic_fx_reached::g.free_current();
}

// original 0x4AD1C0: a ring task: a call through FxRing_PhasesTwin (four:
// FxRing_Wait, _Rise, _Fade, EbonfireRing_End) by +1; then while it lives
// and is past its wait the ring under the actor's matrix.
S10_EXPORT void __cdecl EbonfireRing_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::FxRing_Wait, addr::FxRing_Rise, addr::FxRing_Fade,
                                                 addr::EbonfireRing_End};
    Dispatch(kPhases, 4, Sc()[1], "EbonfireRing_Task");
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[1] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(addr::EbonfireRing_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4AD200: the ring's wait. +9 down; at 0 the owner's position
// (+0x34..+0x3C, not its actor byte), +9 = 0x40, +0xA and +0xB 0, phase on.
MFX_EXPORT void __cdecl FxRing_Wait(void) {
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
    if (sc[9] != 0) return;
    tsc->x = rv::As<records::SpriteHead>(Ptr(::bof3::Img(magic_fx_reached::at::kOwner)))->x;
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Ptr(::bof3::Img(magic_fx_reached::at::kOwner)))->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Ptr(::bof3::Img(magic_fx_reached::at::kOwner)))->height);
    Sprite_Current[9] = 0x40;
    Sprite_Current[0xA] = 0;
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1], 1);
}

// original 0x4AD270: the ring rises. +0xB up one, +0xA up two, +9 down two;
// at +9 = 0x10 the phase on.
MFX_EXPORT void __cdecl FxRing_Rise(void) {
    Bump(Sprite_Current[0xB], 1);
    Bump(Sprite_Current[0xA], 2);
    Bump(Sprite_Current[9], -2);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0x10) Bump(sc[1], 1);
}

// original 0x4AD2B0: the ring fades. +0xB up one, +0xA down three, +9 down
// two; at +9 = 0 the owner's ring count +0xB down and the task freed (a tail
// jmp in the original).
MFX_EXPORT void __cdecl FxRing_Fade(void) {
    Bump(Sprite_Current[0xB], 1);
    Bump(Sprite_Current[0xA], -3);
    Bump(Sprite_Current[9], -2);
    if (Sprite_Current[9] != 0) return;
    Bump(Ptr(::bof3::Img(magic_fx_reached::at::kOwner))[0xB], -1);
    magic_fx_reached::g.free_current();
}

// original 0x4AD300: the rings' fourth phase (after FxRing_Fade, which frees
// the task): +9 down, +0xB up by 2, +0xA up; at +0xA 0x10 the owner's +0xB
// 0xFF and the task freed.
S10_EXPORT void __cdecl EbonfireRing_End(void) {
    Dec(Sc()[9]);
    AddB(Sc()[0xB], 2);
    Inc(Sc()[0xA]);
    if (Sc()[0xA] != 0x10) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4AD350: MagicFx_DrawRing with tpage 0x55 and abr 2 - a band of
// 32 textured gouraud quads round the actor, inner radius +9 x 4 lifted by
// sin(((+0xB + i) & 0xF) << 8) of +0xA x 4 + 0x40, outer +9 x 3 on the
// ground; page (0x340, 0x100), clut (0, 0x1FA), columns i x 4, rows
// (+0xB + 8) x 4 / (+0xB + 12) x 4, the rim shade 0x68 - +0xB x 3; each quad
// sorted at the actor's position moved by its inner x << 9 on both axes.
S10_EXPORT void __cdecl EbonfireRing_Draw(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0, static_cast<unsigned>(s[9]) << 2);
        SetSW(4, s[9] * 3u);
        SetSW(6, s[0xA] * 4u + 0x40);
        SetSW(8, static_cast<unsigned>(s[0xB] & 0xF) << 8);
    }
    int v = Cos(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Sin(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
    v = Sin(SS(8));
    {
        const std::uint16_t lift = SW(6);
        SetVW(0xC, static_cast<unsigned>(Mul12(v, static_cast<short>(lift))) - lift);
    }
    v = Cos(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
    v = Sin(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
    unsigned i = 1;
    for (int a = 0x80; a < 0x1080; a += 0x80, ++i) {
        {
            const unsigned char* const s = Sc();
            SetVW(0, VW(8));
            SetSW(8, static_cast<unsigned>((s[0xB] + static_cast<unsigned char>(i)) & 0xF) << 8);
            SetVW(2, VW(0xA));
            SetVW(4, VW(0xC));
        }
        v = Cos(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Sin(a);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(0))));
        v = Sin(SS(8));
        {
            const std::uint16_t lift = SW(6);
            SetVW(0xC, static_cast<unsigned>(Mul12(v, static_cast<short>(lift))) - lift);
        }
        SetVW(0x10, VW(0x18));
        SetVW(0x12, VW(0x1A));
        SetVW(0x14, 0);
        v = Cos(a);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = Sin(a);
        const unsigned char* s = Sc();
        unsigned char* p = Gfx_PacketNext;
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
        const std::uint32_t lean = Shl(VS(8), 9);
        SetVW(0x1C, 0);
        const auto x = static_cast<unsigned long>(static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->x) + lean);
        const auto z = static_cast<unsigned long>(static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->z) + lean);
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x55, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0xC);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 2, 0x340, 0x100));
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        s = Sc();
        const auto u0 = static_cast<unsigned char>(i << 2);
        const auto u1 = static_cast<unsigned char>((i + 1) << 2);
        p[0x14] = u0;
        p[0x15] = static_cast<unsigned char>((s[0xB] + 8) << 2);
        p[0x28] = u1;
        p[0x29] = static_cast<unsigned char>((s[0xB] + 8) << 2);
        p[0x3C] = u0;
        p[0x50] = u1;
        p[0x3D] = static_cast<unsigned char>((s[0xB] + 0xC) << 2);
        p[0x51] = static_cast<unsigned char>((s[0xB] + 0xC) << 2);
        SetSW(0xA, 0x68u - s[0xB] * 3u);
        for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = 1;
        for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = SB(0xA);
        Rtp4(p, 0x14);
        MH_CALL(Gte_PrimDepths4_14)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x54);
    }
}

// original 0x4AD6F0 (PSX Sacrifice / Fireblast 0x801EF9F8's shape): a fan of
// eight gouraud triangles of radius 0x120 round the origin of the current
// matrix (MagicFx_PushActorMatrix's), the centre +0xA * 12 grey, the rim
// dark. The PSX picks tpage 0x55 or 0x125 by the video mode; the port 0x55.
extern "C" void __cdecl MagicFx_DrawFan(void) {
    battle_items::g.set_draw_mode(Gfx_PacketNext, 0, 1, 0x55, 0);
    battle_items::g.commit(5, 0xC);
    const unsigned char* const sc = Sprite_Current;
    battle_items::SetWord(Scr(0), 0x120);
    battle_items::SetWord(Scr(0xA), sc[0xA] * 12u);
    int s = battle_items::g.sin(0);
    battle_items::SetWord(Vtx(0x10), static_cast<unsigned>(Mul12(s, S16BattleItems(Scr(0)))));
    int co = battle_items::g.cos(0);
    battle_items::SetWord(Vtx(0x14), 0);
    battle_items::SetWord(Vtx(0x12), static_cast<unsigned>(Mul12(co, S16BattleItems(Scr(0)))));
    battle_items::SetWord(Vtx(0xC), 0);
    battle_items::SetWord(Vtx(4), 0);
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        battle_items::g.set_poly_g3(p);
        battle_items::g.set_semi(p, 1);
        const std::uint16_t x = battle_items::Word(Vtx(0x10)), y = battle_items::Word(Vtx(0x12));
        battle_items::SetWord(Vtx(0), 0);
        battle_items::SetWord(Vtx(2), 0);
        battle_items::SetWord(Vtx(8), x);
        battle_items::SetWord(Vtx(0xA), y);
        s = battle_items::g.sin(a);
        battle_items::SetWord(Vtx(0x10), static_cast<unsigned>(Mul12(s, S16BattleItems(Scr(0)))));
        co = battle_items::g.cos(a);
        battle_items::SetWord(Vtx(0x12), static_cast<unsigned>(Mul12(co, S16BattleItems(Scr(0)))));
        long depth, flag;
        battle_items::g.rtp3(VtxP(0), VtxP(8), VtxP(0x10), reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
               reinterpret_cast<float*>(p + 0x28), &depth, &flag);
        battle_items::g.depths3(p);
        p[4] = Scr(0xA)[0];
        p[5] = Scr(0xA)[0];
        p[6] = Scr(0xA)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        battle_items::g.commit(5, 0x34);
    }
    battle_items::g.set_draw_mode(Gfx_PacketNext, 0, 1, 0x15, 0);
    battle_items::g.commit(5, 0xC);
}

void EbonfireRing_Inject() {
    BOF3_INJECT(FxDiscFan_Grow);
    BOF3_INJECT(FxDiscFan_Fade);
    BOF3_INJECT(EbonfireRing_Task);
    BOF3_INJECT(FxRing_Wait);
    BOF3_INJECT(FxRing_Rise);
    BOF3_INJECT(FxRing_Fade);
    BOF3_INJECT(EbonfireRing_End);
    BOF3_INJECT(EbonfireRing_Draw);
    BOF3_INJECT(MagicFx_DrawFan);
}
