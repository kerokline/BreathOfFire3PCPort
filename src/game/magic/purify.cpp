// Moved here by tools/move_funcs.py from magic_s17.cpp (docs/source-layout-round.md).
#include "game/magic/purify.h"

#include <cstdint>
#include <cstring>

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

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

constexpr ::bof3::image::Va kScratchA{SharedScratch_addr}; // SharedScratch (the PSX scratchpad): radius, angle words

constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

void Bump(unsigned char& b, int by = 1) { b = static_cast<unsigned char>(b + by); }

unsigned char* Task(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * at::kTaskStride); }

unsigned char* Tint(unsigned i) { return Mem(::bof3::Img(MoveScript_TintRecords_addr) + (i & 0xFF) * 12u); }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

std::uint32_t Key(const void* p) { return Low32(p); }

// A .data table's entry, read in place, the index unchecked (as the
// originals' `jmp / call [eax*4 + table]`).
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

int Wrap(std::uint32_t v) { return static_cast<int>(v); }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// The target's done flag, 0x904AA8 bit 2, and the task freed: every end of
// an effect here.
void TargetDone() {
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

}  // namespace

#define MS17_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4BCBF0: the kind-2 task. Its phase +1 through a three-entry
// table on its stack - Purify_Start, Purify_Glow, Purify_Fade. 3..255 would
// call through the original's stack; ours aborts.
MS17_EXPORT void __cdecl Purify_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Purify_Start, bof3::addr::Purify_Glow, bof3::addr::Purify_Fade};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 3) bof3::Fatal("Purify_Task: phase %u, past the three-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4BCC20: the source sprite's (0x904B4C, read once) position to
// the task; +0xB 0; on. Seven motes (kind 1, parameter 0x25), each with this
// task its owner, +1 0, +0xB its number 0..6 and +9 its delay 1, 6, .. 31.
// Then the source's tint released and a new one (0, 0, 0, 1) to +0xA; CLUT
// row 26's first 16 cells from their source 0x4000 below; the strip dirty;
// sound 0x100.
MS17_EXPORT void __cdecl Purify_Start(void) {
    unsigned char* const src = Source();
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1]);
    unsigned char number = 0;
    for (unsigned char delay = 1; delay < 0x24; delay = static_cast<unsigned char>(delay + 5)) {
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x25) & 0xFF;
        unsigned char* const t = Task(slot);
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        tt->owner = static_cast<std::int32_t>(Key(Sprite_Current));
        t[1] = 0;
        t[0xB] = number;
        t[9] = delay;
        ++number;
    }
    MH_CALL(Sprite_ReleaseTint)(src);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(src, 0, 0, 0, 1);
    Sprite_Current[0xA] = tint;
    for (unsigned i = 0; i < 0x10; ++i) SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + 2 * i), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3400 + 2 * i)));
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4BCD10: the tint record +0xA's r, g, b each +0xB * 2 (the motes
// landed so far); at seven, on.
MS17_EXPORT void __cdecl Purify_Glow(void) {
    unsigned char* const sc = Sprite_Current;
    Tint(sc[0xA])[2] = static_cast<unsigned char>(sc[0xB] << 1);
    Tint(sc[0xA])[3] = static_cast<unsigned char>(sc[0xB] << 1);
    Tint(sc[0xA])[4] = static_cast<unsigned char>(sc[0xB] << 1);
    if (sc[0xB] == 7) Bump(sc[1]);
}

// original 0x4BCD60: the tint's r, g, b each down 2; once r is 0 the tint
// released, the target's done flag, the task freed (a tail jmp).
MS17_EXPORT void __cdecl Purify_Fade(void) {
    unsigned char* const sc = Sprite_Current;
    Bump(Tint(sc[0xA])[2], -2);
    Bump(Tint(sc[0xA])[3], -2);
    Bump(Tint(sc[0xA])[4], -2);
    const unsigned char index = sc[0xA];
    if (Tint(index)[2] != 0) return;
    MH_CALL(Tint_Release)(index);
    TargetDone();
}

// original 0x4BCDF0: the mote (kind 1, parameter 0x25): a jmp through
// PurifyMote_Types 0x65B1B4 by +1, read in place (one entry).
MS17_EXPORT void __cdecl PurifyMote_Dispatch(void) { Entry(::bof3::Img(PurifyMote_Types_addr), Sprite_Current[1])(); }

// original 0x4BCE10: with the frame-offset table 0x9039D8 the effects'
// (0x8E3580), a call through PurifyMote_Phases 0x65B1B8 by +2, read in place
// (two entries); then while it lives and is placed (+0, +2 not 0) its
// sprite's screen update; the battle's table (0x8B3580) back.
MS17_EXPORT void __cdecl PurifyMote_Task(void) {
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(kFrameSetEffect.v));
    Entry(::bof3::Img(PurifyMote_Phases_addr), Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] != 0 && sc[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(kFrameSetBattle.v));
}

// original 0x4BCE50: +9 down; at 0 the mote placed round its owner - angle
// (+0xB & 7) << 9 (to 0x903850), +0x34 / +0x38 the owner's plus sin / cos
// times 10 (as 5 << 13 >> 12, 32-bit), height the owner's +0x3E + 0x180 -
// its sprite fields set (+0x24 4, +0x25 0x1D, +0x27 0xA0, +0x29 4, +0x2B 1,
// the rest 0), Sprite_SetAnimation(+0xB / 3), and +2 on.
MS17_EXPORT void __cdecl PurifyMote_Place(void) {
    Bump(Sprite_Current[9], -1);
    unsigned char* sc = Sprite_Current;
    if (sc[9] != 0) return;
    const int angle = (sc[0xB] & 7) << 9;
    SetLong(Mem(::bof3::Img(kScratchA.v)), angle);
    const int s = Sin(angle);
    const int dx = static_cast<int>((static_cast<std::uint32_t>(s) * 5u) << 13) >> 12;
    rv::Store(rv::Current<records::BattleTask>()->x, Wrap(static_cast<std::uint32_t>(dx) + static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x)));
    const int c = Cos(Long(Mem(::bof3::Img(kScratchA.v))));
    const int dz = static_cast<int>((static_cast<std::uint32_t>(c) * 5u) << 13) >> 12;
    rv::Store(rv::Current<records::BattleTask>()->z, Wrap(static_cast<std::uint32_t>(dz) + static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z)));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::As<records::SpriteHead>(Owner())->height_whole + 0x180u);
    sc = Sprite_Current;
    sc[0x25] = 0x1D;
    sc[0x26] = 0;
    sc[0x27] = 0xA0;
    sc[0x28] = 0;
    sc[0x5D] = 0;
    sc[0x5E] = 0;
    sc[0x5F] = 0;
    sc[0x5C] = 0;
    sc[0x2A] = 0;
    sc[0x29] = 4;
    sc[0x24] = 4;
    rv::Store<unsigned>(rv::As<records::BattleTask>(sc)->column, 0);
    sc[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(sc[0xB] / 3));
    Bump(Sprite_Current[2]);
}

// original 0x4BCF90: the mote's script once; at its end the owner's count
// +0xB up and the mote freed (a tail jmp).
MS17_EXPORT void __cdecl PurifyMote_End(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    Bump(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

void Purify_Inject() {
    BOF3_INJECT(Purify_Task);
    BOF3_INJECT(Purify_Start);
    BOF3_INJECT(Purify_Glow);
    BOF3_INJECT(Purify_Fade);
    BOF3_INJECT(PurifyMote_Dispatch);
    BOF3_INJECT(PurifyMote_Task);
    BOF3_INJECT(PurifyMote_Place);
    BOF3_INJECT(PurifyMote_End);
}
