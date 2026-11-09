// Moved here by tools/move_funcs.py from battle_items.cpp, magic_fx_reached.cpp (docs/source-layout-round.md).
#include "game/magic/fx_disc_fan.h"

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
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sound.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace battle_items;

using namespace magic_fx_reached;

namespace battle_items {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

namespace {

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

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

// `fild dword` then `fstp dword` of an int: the original's integer vertex as
// a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

}  // namespace

namespace magic_fx_reached {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

namespace {

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace magic_fx_reached

namespace {

unsigned char* Ptr(std::uint32_t cell) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(magic_fx_reached::Long(magic_fx_reached::At(cell)))));
}

std::int32_t PtrValue(const unsigned char* p) {
    return static_cast<std::int32_t>(Low32(p));
}

// A task slot by its index, unchecked as the originals index it.
unsigned char* Task(unsigned index) { return magic_fx_reached::At(::bof3::Img(BattleTask_Slots_addr) + index * magic_fx_reached::at::kTaskStride); }

unsigned char Byte(std::uint32_t address) { return magic_fx_reached::At(address)[0]; }

// A .data dispatch table's entry, read as the original reads it.
Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(magic_fx_reached::Long(magic_fx_reached::At(table + 4 * index)))));
}

// Sprite_Current's position copied from `from`: the actor byte +8 and the
// three dwords +0x34 / +0x38 / +0x3C, Sprite_Current read again for each.
void TakePosition(const unsigned char* from) {
    Sprite_Current[8] = from[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(from)->height);
}

void Bump(unsigned char& b, int by) { b = static_cast<unsigned char>(b + by); }

}  // namespace

#define MFX_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4C4FC0: the effect task. Its phase +1 through a three-entry
// table the original builds on its stack (FxDiscFan_Start, _Grow, _Fade),
// then while the task lives (+0 not zero) the source's screen point, 16 up,
// the disc, and the fan under the actor's matrix. The index is not checked:
// 3..255 would call through the original's stack; ours aborts.
MFX_EXPORT void __cdecl FxDiscFan_Task(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 3) bof3::Fatal("FxDiscFan_Task: phase %u, past the three-entry table", phase);
    magic_fx_reached::g.disc_fan[phase]();
    if (Sprite_Current[0] == 0) return;
    magic_fx_reached::g.update_screen_xy();
    unsigned char* const sc = Sprite_Current;
    rv::Store<unsigned>(rv::As<records::BattleTask>(sc)->screen_y, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_y) - 0x10u);
    magic_fx_reached::g.draw_disc();
    magic_fx_reached::g.push_matrix();
    magic_fx_reached::g.draw_fan();
    magic_fx_reached::g.pop_matrix();
}

// original 0x4C5020: the start. The position of the source sprite 0x904B4C;
// six ring tasks (kind 1, parameter 0x16), each owned by this one with its
// delay +9 = 6 i + 1, counted in +0xB; the CLUT of strip row 26 made
// semi-transparent (cells 1..15 from 0x4000 below with bit 15, cell 0
// cleared) and marked dirty; the target's flags 0x10; sound 0x100; +9 and
// +0xA to 1 and the phase on.
//
// As the original has it: BattleTask_Create's index is not tested (0xFF
// writes past the slots); Sprite_Current is read again after each create;
// the source pointer is read once.
MFX_EXPORT void __cdecl FxDiscFan_Start(void) {
    TakePosition(Ptr(::bof3::Img(magic_fx_reached::at::kSource)));
    Sprite_Current[0xB] = 0;
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned index = magic_fx_reached::g.task_create(1, 0x16);
        unsigned char* const sc = Sprite_Current;
        unsigned char* const t = Task(index);
        magic_fx_reached::SetLong(t + 0x80, PtrValue(sc));
        t[9] = static_cast<unsigned char>(i * 6 + 1);
        Bump(sc[0xB], 1);
    }
    for (std::uint32_t a = ::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + 2; a < (::bof3::Img(Gfx_ClutStrip_addr) + 0x3400) + 0x20; a += 2)
        magic_fx_reached::SetWord(magic_fx_reached::At(a), magic_fx_reached::Word(magic_fx_reached::At(a - 0x4000)) | 0x8000u);
    const unsigned char target = Byte(::bof3::Img(magic_fx_reached::at::kTarget));
    Gfx_ClutStrip[0x1a00] = 0;
    Gfx_ClutStripDirty = 1;
    magic_fx_reached::g.set_target_flags(target, 0x10);
    magic_fx_reached::g.play_by_id(0x100);
    Sprite_Current[9] = 1;
    Sprite_Current[0xA] = 1;
    Bump(Sprite_Current[1], 1);
}

// original 0x4C5110: a ring task. Its phase +1 through FxRing_Phases
// 0x65B5B8 (read in place, the index unchecked - four entries, the fourth
// 0x4AD300 never reached: phase 2 frees the task), then while it lives and
// is past its wait (+0 and +1 not zero) the ring under the actor's matrix.
MFX_EXPORT void __cdecl FxRing_Task(void) {
    Entry(::bof3::Img(FxRing_Phases_addr), Sprite_Current[1])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[1] == 0) return;
    magic_fx_reached::g.push_matrix();
    magic_fx_reached::g.draw_ring();
    magic_fx_reached::g.pop_matrix();
}

// original 0x4C5150 (PSX Sacrifice 0x801EF240's loop): a band of 32 textured
// gouraud quads round the actor - inner radius +9 * 4 lifted by
// sin(((+0xB + i) & 0xF) << 8) of +0xA * 4 + 0x40, outer radius +9 * 3 on the
// ground - the texture a strip of page (0x340, 0x100), clut (0, 0x1FA), its
// columns i * 4 and rows (+0xB + 8) * 4 / (+0xB + 12) * 4, the rim shade
// 0x68 - +0xB * 3. Each quad sorted at the actor's position moved by its
// inner x << 9 on both axes, as on the PSX. Tpage 0x35 where the PSX picks
// 0x55 or 0x125.
extern "C" void __cdecl MagicFx_DrawRing(void) {
    const unsigned char* sc = Sprite_Current;
    battle_items::SetWord(Scr(0), sc[9] * 4u);
    battle_items::SetWord(Scr(4), sc[9] * 3u);
    battle_items::SetWord(Scr(6), sc[0xA] * 4u + 0x40);
    battle_items::SetWord(Scr(8), static_cast<unsigned>((sc[0xB] & 0xF) << 8));
    int v = battle_items::g.cos(0);
    battle_items::SetWord(Vtx(8), static_cast<unsigned>(Mul12(v, S16BattleItems(Scr(0)))));
    v = battle_items::g.sin(0);
    battle_items::SetWord(Vtx(0xA), static_cast<unsigned>(Mul12(v, S16BattleItems(Scr(0)))));
    v = battle_items::g.sin(S16BattleItems(Scr(8)));
    {
        const std::uint16_t lift = battle_items::Word(Scr(6));
        battle_items::SetWord(Vtx(0xC), static_cast<unsigned>(Mul12(v, static_cast<short>(lift))) - lift);
    }
    v = battle_items::g.cos(0);
    battle_items::SetWord(Vtx(0x18), static_cast<unsigned>(Mul12(v, S16BattleItems(Scr(4)))));
    v = battle_items::g.sin(0);
    battle_items::SetWord(Vtx(0x1C), 0);
    battle_items::SetWord(Vtx(0x1A), static_cast<unsigned>(Mul12(v, S16BattleItems(Scr(4)))));
    unsigned i = 1;
    for (int a = 0x80; a < 0x1080; a += 0x80, ++i) {
        sc = Sprite_Current;
        battle_items::SetWord(Vtx(0), battle_items::Word(Vtx(8)));
        battle_items::SetWord(Scr(8), static_cast<unsigned>(((sc[0xB] + static_cast<unsigned char>(i)) & 0xF) << 8));
        battle_items::SetWord(Vtx(2), battle_items::Word(Vtx(0xA)));
        battle_items::SetWord(Vtx(4), battle_items::Word(Vtx(0xC)));
        v = battle_items::g.cos(a);
        battle_items::SetWord(Vtx(8), static_cast<unsigned>(Mul12(v, S16BattleItems(Scr(0)))));
        v = battle_items::g.sin(a);
        battle_items::SetWord(Vtx(0xA), static_cast<unsigned>(Mul12(v, S16BattleItems(Scr(0)))));
        v = battle_items::g.sin(S16BattleItems(Scr(8)));
        {
            const std::uint16_t lift = battle_items::Word(Scr(6));
            battle_items::SetWord(Vtx(0xC), static_cast<unsigned>(Mul12(v, static_cast<short>(lift))) - lift);
        }
        battle_items::SetWord(Vtx(0x10), battle_items::Word(Vtx(0x18)));
        battle_items::SetWord(Vtx(0x12), battle_items::Word(Vtx(0x1A)));
        battle_items::SetWord(Vtx(0x14), 0);
        v = battle_items::g.cos(a);
        battle_items::SetWord(Vtx(0x18), static_cast<unsigned>(Mul12(v, S16BattleItems(Scr(4)))));
        v = battle_items::g.sin(a);
        sc = Sprite_Current;
        unsigned char* p = Gfx_PacketNext;
        battle_items::SetWord(Vtx(0x1A), static_cast<unsigned>(Mul12(v, S16BattleItems(Scr(4)))));
        const std::uint32_t lean = static_cast<std::uint32_t>(static_cast<int>(S16BattleItems(Vtx(8)))) << 9;
        battle_items::SetWord(Vtx(0x1C), 0);
        const unsigned x = static_cast<std::uint32_t>(battle_items::Long(sc + 0x34)) + lean;
        const unsigned z = static_cast<std::uint32_t>(battle_items::Long(sc + 0x38)) + lean;
        battle_items::g.set_draw_mode(p, 0, 1, 0x35, 0);
        battle_items::g.commit_sorted(x, z, 2, 0xC);
        p = Gfx_PacketNext;
        battle_items::g.set_poly_gt4(p);
        battle_items::g.set_semi(p, 1);
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, battle_items::g.get_tpage(0, 1, 0x340, 0x100));
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, battle_items::g.get_clut(0, 0x1FA));
        const auto u0 = static_cast<unsigned char>(i << 2);
        const auto u1 = static_cast<unsigned char>((i + 1) << 2);
        p[0x14] = u0;
        p[0x15] = static_cast<unsigned char>((Sprite_Current[0xB] + 8) << 2);
        p[0x28] = u1;
        p[0x29] = static_cast<unsigned char>((Sprite_Current[0xB] + 8) << 2);
        p[0x3C] = u0;
        p[0x50] = u1;
        p[0x3D] = static_cast<unsigned char>((Sprite_Current[0xB] + 0xC) << 2);
        p[0x51] = static_cast<unsigned char>((Sprite_Current[0xB] + 0xC) << 2);
        battle_items::SetWord(Scr(0xA), 0x68u - Sprite_Current[0xB] * 3u);
        for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = 1;
        for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = Scr(0xA)[0];
        long depth, flag;
        battle_items::g.rtp4(VtxP(0), VtxP(8), VtxP(0x10), VtxP(0x18), reinterpret_cast<float*>(p + 8),
               reinterpret_cast<float*>(p + 0x1C), reinterpret_cast<float*>(p + 0x30), reinterpret_cast<float*>(p + 0x44),
               &depth, &flag);
        battle_items::g.depths4(p);
        battle_items::g.commit_sorted(x, z, 2, 0x54);
    }
}

// original 0x4C54F0 (PSX Sacrifice 0x801EF7B0): eight gouraud triangles round
// the actor's screen point (+0x2E / +0x30), radius +9 * 2 + 0x20, the centre
// red +9 * 10, each sorted at the actor's position. Tpage 0x35 where the
// PSX picks 0x35 or 0xA5.
extern "C" void __cdecl MagicFx_DrawDisc(void) {
    battle_items::SetWord(Scr(0), Sprite_Current[9] * 2u + 0x20);
    int a = 0;
    do {
        battle_items::g.set_draw_mode(Gfx_PacketNext, 0, 1, 0x35, 0);
        const unsigned char* sc = Sprite_Current;
        battle_items::g.commit_sorted(static_cast<unsigned>(battle_items::Long(sc + 0x34)), static_cast<unsigned>(battle_items::Long(sc + 0x38)), 2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        battle_items::g.set_poly_g3(p);
        battle_items::g.set_semi(p, 1);
        PutFloat(p + 8, S16BattleItems(Sprite_Current + 0x2E));
        PutFloat(p + 0xC, S16BattleItems(Sprite_Current + 0x30));
        int v = battle_items::g.sin(a);
        PutFloat(p + 0x18, Mul12(v, S16BattleItems(Scr(0))) + S16BattleItems(Sprite_Current + 0x2E));
        v = battle_items::g.cos(a);
        PutFloat(p + 0x1C, Mul12(v, S16BattleItems(Scr(0))) + S16BattleItems(Sprite_Current + 0x30));
        a += 0x200;
        v = battle_items::g.sin(a);
        PutFloat(p + 0x28, Mul12(v, S16BattleItems(Scr(0))) + S16BattleItems(Sprite_Current + 0x2E));
        v = battle_items::g.cos(a);
        PutFloat(p + 0x2C, Mul12(v, S16BattleItems(Scr(0))) + S16BattleItems(Sprite_Current + 0x30));
        p[4] = static_cast<unsigned char>(Sprite_Current[9] * 10u);
        for (unsigned k : {5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        sc = Sprite_Current;
        battle_items::g.commit_sorted(static_cast<unsigned>(battle_items::Long(sc + 0x34)), static_cast<unsigned>(battle_items::Long(sc + 0x38)), 2, 0x34);
    } while (a < 0x1000);
}

void FxDiscFan_Inject() {
    BOF3_INJECT(FxDiscFan_Task);
    BOF3_INJECT(FxDiscFan_Start);
    BOF3_INJECT(FxRing_Task);
    BOF3_INJECT(MagicFx_DrawRing);
    BOF3_INJECT(MagicFx_DrawDisc);
}
