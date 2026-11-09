// Moved here by tools/move_funcs.py from magic_s17.cpp (docs/source-layout-round.md).
#include "game/magic/revive.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s17.h"
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

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

constexpr ::bof3::image::Va kScratchA{SharedScratch_addr}; // SharedScratch (the PSX scratchpad): radius, angle words

constexpr ::bof3::image::Va kScratchB{0x903854};

constexpr ::bof3::image::Va kShade{0x903856};          // u16: a shade the draws copy to their primitives

constexpr ::bof3::image::Va kRgb{0x903858};            // three u16 at +0 / +2 / +4

constexpr ::bof3::image::Va kActingKind{0x904B35};     // u8: 4 an ability

constexpr ::bof3::image::Va kAbility{0x904B80};        // u16: the ability id

// MAGIC077's mote pool: 64 of 0x84 bytes, +0 bit 0 in use.
// kMotePool: now ReviveMote_Pool (symbols.toml).
constexpr std::uint32_t kMoteStride = 0x84;

constexpr unsigned kMotes = 0x40;

// Callees other groups own, ours now, by name: the effect library's
// MagicFx_LinkByDepth (through SortFn: the depths as long *), _StepToward,
// _NearSprite3D; S35's BenedictionMote_DrawGlow; S37's
// MagicFx_FreeCurrentRecord.
using Void = void (__cdecl*)();

void Bump(unsigned char& b, int by = 1) { b = static_cast<unsigned char>(b + by); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

unsigned char* Task(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * at::kTaskStride); }

unsigned char* Mote(unsigned i) { return Mem(::bof3::Img(ReviveMote_Pool_addr) + (i & 0xFF) * kMoteStride); }

unsigned char* Tint(unsigned i) { return Mem(::bof3::Img(MoveScript_TintRecords_addr) + (i & 0xFF) * 12u); }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

std::uint32_t Key(const void* p) { return Low32(p); }

// A .data table's entry, read in place, the index unchecked (as the
// originals' `jmp / call [eax*4 + table]`).
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// `imul` then `sar 0xC`: a 32-bit product that wraps, shifted arithmetically.
int M(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

int Wrap(std::uint32_t v) { return static_cast<int>(v); }

// `fild dword` then `fst(p) dword`: an integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, tpage, 0) and its commit.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

void LinkAtActor(int dy, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z), dy,
                                size);
}

// A mote's position round the owner: +0x34 / +0x38 = the owner's plus
// sin / cos of the angle (word 0x903854) times the radius +0xC, each 32-bit.
void Orbit() {
    const int s = Sin(S16(Mem(::bof3::Img(kScratchB.v))));
    unsigned char* sc = Sprite_Current;
    rv::Store(rv::As<records::BattleTask>(sc)->x, Wrap(static_cast<std::uint32_t>(s) * static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->step_x) +
                            static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x)));
    const int c = Cos(S16(Mem(::bof3::Img(kScratchB.v))));
    sc = Sprite_Current;
    rv::Store(rv::As<records::BattleTask>(sc)->z, Wrap(static_cast<std::uint32_t>(c) * static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->step_x) +
                            static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z)));
}

void MoteAngle() { SetWord(Mem(::bof3::Img(kScratchB.v)), static_cast<unsigned>((Sprite_Current[0xB] & 0xF) << 8)); }

}  // namespace

#define MS17_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4BCFB0: the kind-2 task. Its phase +1 through a six-entry table
// on its stack - Revive_Start, Revive_TintSource, BattleFx_Brighten (round
// eight's), Revive_WaitMotes, Revive_Fade, BattleFx_Finish (round eight's);
// 6..255 would call through the original's stack, ours aborts. Then every
// live mote of the pool 0x68C0B8 run: Sprite_Current the mote and 0x93B940
// its +0x80 for the call, both put back after (as they were after the
// phase).
MS17_EXPORT void __cdecl Revive_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Revive_Start,     bof3::addr::Revive_TintSource,
                                                 bof3::addr::BattleFx_Brighten, bof3::addr::Revive_WaitMotes,
                                                 bof3::addr::Revive_Fade,      bof3::addr::BattleFx_Finish};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 6) bof3::Fatal("Revive_Task: phase %u, past the six-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const saved = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = Mote(i);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = static_cast<std::int32_t>(tm->owner);
        Sprite_Current = m;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        MH_CALL(ReviveMote_Dispatch)();
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = saved;
    }
}

// original 0x4BD050: the pool emptied (+0..+2 of all 64); +4 the strength
// (Revive_IsStrong); the source's actor byte and position to the task; +0xB
// 0, +9 0x3C, +0xA 0; on. The halo (kind 1, parameter 0x62: this task its
// owner, +1 0, +9 1), the task's count +0xB up; sound 0x100. CLUT row 26
// whole and row 2's first 16 cells from their sources with the
// semi-transparency bit, each row's cell 0 without; the strip dirty.
MS17_EXPORT void __cdecl Revive_Start(void) {
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = Mote(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    const unsigned char strong = MH_CALL(Revive_IsStrong)();
    Sprite_Current[4] = strong;
    unsigned char* const src = Source();
    Sprite_Current[8] = src[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0x3C;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[1]);
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x62) & 0xFF;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const t = Task(slot);
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    tt->owner = static_cast<std::int32_t>(Key(sc));
    t[1] = 0;
    t[9] = 1;
    Bump(sc[0xB]);
    MH_CALL(Sound_PlayById)(0x100);
    for (unsigned i = 0; i < 0x100; ++i) SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + 2 * i), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3400 + 2 * i)) | 0x8000u);
    for (unsigned i = 0; i < 0x10; ++i) SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x400 + 2 * i), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x400 + 2 * i)) | 0x8000u);
    SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3400)));
    SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x400), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x400)));
    Gfx_ClutStripDirty = 1;
}

// original 0x4BD180: +9 down; at 0 the source's tint released and a new one
// (0, 0, 0, 1) to +0xA, the source read again for each; +9 8; on.
MS17_EXPORT void __cdecl Revive_TintSource(void) {
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    const unsigned char tint = MH_CALL(Sprite_SetTint)(Source(), 0, 0, 0, 1);
    Sprite_Current[0xA] = tint;
    Sprite_Current[9] = 8;
    Bump(Sprite_Current[1]);
}

// original 0x4BD1E0: held until the count +0xB is 0 (the halo and its motes
// gone); then +9 8, on.
MS17_EXPORT void __cdecl Revive_WaitMotes(void) {
    unsigned char* const sc = Sprite_Current;
    if (sc[0xB] != 0) return;
    sc[9] = 8;
    Bump(sc[1]);
}

// original 0x4BD200: the tint's r, g, b each down 1; +9 down; at 0 the
// source's tint released, on.
MS17_EXPORT void __cdecl Revive_Fade(void) {
    unsigned char* const sc = Sprite_Current;
    Bump(Tint(sc[0xA])[2], -1);
    Bump(Tint(sc[0xA])[3], -1);
    Bump(Tint(sc[0xA])[4], -1);
    Bump(sc[9], -1);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Source());
    Bump(Sprite_Current[1]);
}

// original 0x4BD280: 1 when the acting kind 0x904B35 is 4 (an ability) and
// the ability 0x904B80 (u16) is neither 0x4C nor 0xB4; else 0. In al.
MS17_EXPORT unsigned char __cdecl Revive_IsStrong(void) {
    if (Mem(::bof3::Img(kActingKind.v))[0] != 4) return 0;
    const unsigned id = Word(Mem(::bof3::Img(kAbility.v)));
    return id == 0x4C || id == 0xB4 ? 0 : 1;
}

// original 0x4BD2B0: the halo (kind 1, parameter 0x62): a jmp through
// ReviveHalo_Types 0x65B1C0 by +1, read in place (one entry).
MS17_EXPORT void __cdecl ReviveHalo_Dispatch(void) { Entry(::bof3::Img(ReviveHalo_Types_addr), Sprite_Current[1])(); }

// original 0x4BD2D0: a call through ReviveHalo_Phases 0x65B1C4 by +2, read in
// place (four entries: ReviveHalo_Wait, ReviveHalo_Spawn, MagicFx_WaitA,
// MAGIC060's 0x4B1740); then while it lives and has started (+0, +2 not 0)
// its screen point and its draw (a tail jmp).
MS17_EXPORT void __cdecl ReviveHalo_Task(void) {
    Entry(::bof3::Img(ReviveHalo_Phases_addr), Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(ReviveHalo_Draw)();
}

// original 0x4BD300: +9 down; at 0 the owner's strength +4 and position to
// the halo, 0xC00000 higher; +9 0, +0xA 0x3C; +2 on.
MS17_EXPORT void __cdecl ReviveHalo_Wait(void) {
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 0) return;
    sc[4] = Owner()[4];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, Wrap(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) + 0xC00000u));
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0x3C;
    Bump(Sprite_Current[2]);
}

// original 0x4BD380: +9 up; at 0x10 the motes - as many as
// ReviveHalo_MoteCounts 0x65B1D4 [+4] (u8, read in place, the index
// unchecked; rows 0 and 1 are the weak and the strong), each from the pool (ReviveMote_Alloc; none left, none):
// the owner its +0x80, +1 0, +0xB its number, +9 16..31 by Rand, the owner's
// count +0xB up - and +2 on.
MS17_EXPORT void __cdecl ReviveHalo_Spawn(void) {
    Bump(Sprite_Current[9]);
    if (Sprite_Current[9] != 0x10) return;
    unsigned char number = 0;
    if (Mem(::bof3::Img(ReviveHalo_MoteCounts_addr) + Sprite_Current[4])[0] != 0) {
        do {
            const unsigned char index = MH_CALL(ReviveMote_Alloc)();
            if (index != 0xFF) {
                unsigned char* const m = Mote(index);
                records::BattleTask* const tm = rv::As<records::BattleTask>(m);
                tm->owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
                m[1] = 0;
                m[0xB] = number;
                const int r = MH_CALL(Rand)();
                m[9] = static_cast<unsigned char>((r & 0xF) + 0x10);
                Bump(Owner()[0xB]);
            }
            ++number;
        } while (number < Mem(::bof3::Img(ReviveHalo_MoteCounts_addr) + Sprite_Current[4])[0]);
    }
    Bump(Sprite_Current[2]);
}

// original 0x4BD420: the halo: a draw mode (tpage 0xB5) sorted at the halo,
// then a semi-transparent textured quad over its screen point (+0x2E /
// +0x30), from y - 0x60 (strong) or y - 0x40 to y, half-width 0x38 or 0x18;
// tpage (1, 1, 0x340, 0x100), clut (0, 0x1FA), texture cells by the
// strength; the shade +9 * 8 (word 0x903856); sorted at the halo, size 0x48.
MS17_EXPORT void __cdecl ReviveHalo_Draw(void) {
    DrawMode(0xB5);
    LinkAtActor(1, 0xC);
    const unsigned char* sc = Sprite_Current;
    int height, half;
    if (sc[4] != 0) {
        height = 0x60;
        half = 0x38;
    } else {
        height = 0x40;
        half = 0x18;
    }
    SetWord(Mem(::bof3::Img(kShade.v)), static_cast<unsigned>(sc[9]) << 3);
    unsigned char* const p = Gfx_PacketNext;
    const int x = S16(sc + 0x2E);
    const int y = S16(sc + 0x30);
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, x - half);
    PutFloat(p + 0xC, y - height);
    PutFloat(p + 0x18, x + half);
    PutFloat(p + 0x1C, y - height);
    PutFloat(p + 0x38, x + half);
    PutFloat(p + 0x28, x - half);
    PutFloat(p + 0x2C, y);
    PutFloat(p + 0x3C, y);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    if (Sprite_Current[4] != 0) {
        p[0x14] = 8;
        p[0x15] = 8;
        p[0x25] = 8;
        p[0x34] = 8;
        p[0x24] = 0x78;
        p[0x35] = 0x68;
        p[0x44] = 0x78;
        p[0x45] = 0x68;
    } else {
        p[0x14] = 0x78;
        p[0x15] = 8;
        p[0x24] = 0xA8;
        p[0x25] = 8;
        p[0x34] = 0x78;
        p[0x35] = 0x48;
        p[0x44] = 0xA8;
        p[0x45] = 0x48;
    }
    p[4] = Mem(::bof3::Img(kShade.v))[0];
    p[5] = Mem(::bof3::Img(kShade.v))[0];
    p[6] = Mem(::bof3::Img(kShade.v))[0];
    LinkAtActor(1, 0x48);
}

// original 0x4BD5C0: a mote: a jmp through ReviveMote_Types 0x65B1D8 by +1,
// read in place (one entry).
MS17_EXPORT void __cdecl ReviveMote_Dispatch(void) { Entry(::bof3::Img(ReviveMote_Types_addr), Sprite_Current[1])(); }

// original 0x4BD5E0: its phase +2 through a three-entry table on its stack -
// ReviveMote_Launch, MAGIC169's 0x4F1A40, ReviveMote_Rise; 3..255 would call
// through the original's stack, ours aborts. Then between two draw modes
// (tpage 0x35, then 0x15, each sorted at the mote) while it lives and has
// launched (+0, +2 not 0): its screen point, its fan, and MAGIC169's
// 0x4F1BD0.
MS17_EXPORT void __cdecl ReviveMote_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::ReviveMote_Launch, bof3::addr::BenedictionMote_Spiral,
                                                 bof3::addr::ReviveMote_Rise};
    const unsigned phase = Sprite_Current[2];
    if (phase >= 3) bof3::Fatal("ReviveMote_Task: phase %u, past the three-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    DrawMode(0x35);
    LinkAtActor(2, 0xC);
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] != 0 && sc[2] != 0) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        MH_CALL(ReviveMote_Draw)();
        MH_AT(Void, bof3::addr::BenedictionMote_DrawGlow)();
    }
    DrawMode(0x15);
    LinkAtActor(2, 0xC);
}

// original 0x4BD690: +9 down; at 0 (the first mote with sound 0x101) the
// mote launched from the owner: radius +0xC 8, angle (+0xB & 0xF) << 8 (word
// 0x903854) and its orbit; height the owner's + 0x800000; rise +0x14
// ((Rand & 3) + 2) << 20 and its step +0x20 a sixteenth of it; colour
// +0x5D..+0x5F each (Rand & 7) + 6; +9 0, +0xA 0x10; +2 on.
MS17_EXPORT void __cdecl ReviveMote_Launch(void) {
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    if (Sprite_Current[0xB] == 0) MH_CALL(Sound_PlayById)(0x101);
    rv::Current<records::BattleTask>()->step_x = 8;
    MoteAngle();
    Orbit();
    rv::Store(rv::Current<records::BattleTask>()->height, Wrap(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height) + 0x800000u));
    const int r = MH_CALL(Rand)();
    rv::Store(rv::Current<records::BattleTask>()->step_y, ((r & 3) + 2) << 20);
    unsigned char* sc = Sprite_Current;
    rv::Store(rv::As<records::BattleTask>(sc)->work_20, rv::As<records::BattleTask>(sc)->step_y / 16);
    for (unsigned k = 0x5D; k <= 0x5F; ++k) {
        const int v = MH_CALL(Rand)();
        Sprite_Current[k] = static_cast<unsigned char>((v & 7) + 6);
    }
    sc = Sprite_Current;
    sc[9] = 0;
    sc[0xA] = 0x10;
    Bump(sc[2]);
}

// original 0x4BD7D0: the radius +0xC up one and the orbit again; while the
// step +0x20 is below the rise +0x14 (signed), the rise less the step, and
// the height up by it; on odd frames +9 down, and at 0 the owner's count
// +0xB down and MAGIC219's 0x4F6290 (a tail jmp).
MS17_EXPORT void __cdecl ReviveMote_Rise(void) {
    rv::Store(rv::Current<records::BattleTask>()->step_x, Wrap(static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x) + 1u));
    MoteAngle();
    Orbit();
    unsigned char* sc = Sprite_Current;
    const std::int32_t step = rv::As<records::BattleTask>(sc)->work_20, rise = rv::As<records::BattleTask>(sc)->step_y;
    if (step < rise) {
        rv::Store(rv::As<records::BattleTask>(sc)->step_y, Wrap(static_cast<std::uint32_t>(rise) - static_cast<std::uint32_t>(step)));
        sc = Sprite_Current;
        rv::Store(rv::As<records::BattleTask>(sc)->height, Wrap(static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->height) + static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->step_y)));
        sc = Sprite_Current;
    }
    if ((Frame_Counter & 1) == 0) return;
    Bump(sc[9], -1);
    if (Sprite_Current[9] != 0) return;
    Bump(Owner()[0xB], -1);
    MH_AT(Void, bof3::addr::MagicFx_FreeCurrentRecord)();
}

// original 0x4BD8A0: a fan of eight semi-transparent gouraud triangles round
// the mote's screen point (+0x2E / +0x30), radius 8 (word 0x903850, read
// back each time), the centre shade +9 * 12 (word 0x903856), the rim +9
// times the colour +0x5D..+0x5F (signed, words 0x903858..), each sorted at
// the mote, size 0x34.
MS17_EXPORT void __cdecl ReviveMote_Draw(void) {
    const unsigned char* const sc = Sprite_Current;
    const int x = S16(sc + 0x2E);
    const int y = S16(sc + 0x30);
    SetWord(Mem(::bof3::Img(kScratchA.v)), 8);
    SetWord(Mem(::bof3::Img(kShade.v)), sc[9] * 12u);
    SetWord(Mem(::bof3::Img(kRgb.v)), static_cast<unsigned>(static_cast<signed char>(sc[0x5D]) * sc[9]));
    SetWord(Mem(::bof3::Img(kRgb.v) + 2), static_cast<unsigned>(static_cast<signed char>(sc[0x5E]) * sc[9]));
    SetWord(Mem(::bof3::Img(kRgb.v) + 4), static_cast<unsigned>(static_cast<signed char>(sc[0x5F]) * sc[9]));
    unsigned char centre[8];
    PutFloat(centre, x);
    PutFloat(centre + 4, y);
    int a = 0;
    for (int n = 8; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        std::memcpy(p + 8, centre, 8);
        int v = Sin(a);
        PutFloat(p + 0x18, M(v, S16(Mem(::bof3::Img(kScratchA.v)))) + x);
        v = Cos(a);
        PutFloat(p + 0x1C, M(v, S16(Mem(::bof3::Img(kScratchA.v)))) + y);
        a += 0x200;
        v = Sin(a);
        PutFloat(p + 0x28, M(v, S16(Mem(::bof3::Img(kScratchA.v)))) + x);
        v = Cos(a);
        PutFloat(p + 0x2C, M(v, S16(Mem(::bof3::Img(kScratchA.v)))) + y);
        p[4] = Mem(::bof3::Img(kShade.v))[0];
        p[5] = Mem(::bof3::Img(kShade.v))[0];
        p[6] = Mem(::bof3::Img(kShade.v))[0];
        p[0x14] = Mem(::bof3::Img(kRgb.v))[0];
        p[0x15] = Mem(::bof3::Img(kRgb.v) + 2)[0];
        p[0x16] = Mem(::bof3::Img(kRgb.v) + 4)[0];
        p[0x24] = Mem(::bof3::Img(kRgb.v))[0];
        p[0x25] = Mem(::bof3::Img(kRgb.v) + 2)[0];
        p[0x26] = Mem(::bof3::Img(kRgb.v) + 4)[0];
        LinkAtActor(2, 0x34);
    }
}

// original 0x4BDA60: the first free mote of the pool (+0 bit 0 clear) marked
// in use; its index in al, or 0xFF when all 64 are.
MS17_EXPORT unsigned char __cdecl ReviveMote_Alloc(void) {
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = Mote(i);
        if ((m[0] & 1) == 0) {
            m[0] = static_cast<unsigned char>(m[0] | 1);
            return static_cast<unsigned char>(i);
        }
    }
    return 0xFF;
}

void Revive_Inject() {
    BOF3_INJECT(Revive_Task);
    BOF3_INJECT(Revive_Start);
    BOF3_INJECT(Revive_TintSource);
    BOF3_INJECT(Revive_WaitMotes);
    BOF3_INJECT(Revive_Fade);
    BOF3_INJECT(Revive_IsStrong);
    BOF3_INJECT(ReviveHalo_Dispatch);
    BOF3_INJECT(ReviveHalo_Task);
    BOF3_INJECT(ReviveHalo_Wait);
    BOF3_INJECT(ReviveHalo_Spawn);
    BOF3_INJECT(ReviveHalo_Draw);
    BOF3_INJECT(ReviveMote_Dispatch);
    BOF3_INJECT(ReviveMote_Task);
    BOF3_INJECT(ReviveMote_Launch);
    BOF3_INJECT(ReviveMote_Rise);
    BOF3_INJECT(ReviveMote_Draw);
    BOF3_INJECT(ReviveMote_Alloc);
}
