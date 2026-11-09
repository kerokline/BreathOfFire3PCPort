// Moved here by tools/move_funcs.py from magic_s06.cpp (docs/source-layout-round.md).
#include "game/magic/magic_020_lead.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s06.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
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

// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetSlash{0x8C5D80};

constexpr ::bof3::image::Va kShade{0x903852};          // u16: SharedScratch +2, Magic020_DrawFade's shade

// kSlashAnimations: now Magic020Slash_Animations (symbols.toml). Magic020Slash_Animations: 12 bytes to the next table
constexpr unsigned kSlashAnimationCount = 12;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

std::uint32_t Key(const void* p) { return Low32(p); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The acting actor's record as the originals index it: a member below 3, else
// the enemy by index - 3, unchecked.
unsigned char* ActorRecord(unsigned char actor) {
    if (actor < 3) return Mem(::bof3::Img(ObjTrio_addr) + actor * at::kPartyStride);
    return Mem(::bof3::Img(Battle_Enemies_addr) + (static_cast<std::uint32_t>(actor) - 3) * at::kEnemyStride);
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// A double: a kind-1 task (`parameter`) whose first 0x80 bytes are the acting
// actor's record (the actor byte read after the task is made), copied dword by
// dword forward as `rep movsd` does; its owner this task, +1 `body`, +2 0,
// +6 1 (kind 1), +5 `parameter`. Answers the new slot.
unsigned char* MakeDouble(unsigned parameter, unsigned char body) {
    const unsigned slot = NewTask(parameter);
    const unsigned char* const from = ActorRecord(ActorByte());
    unsigned char* const to = TaskSlot(slot);
    records::BattleTask* const tto = rv::As<records::BattleTask>(to);
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(to + k, Long(from + k));
    tto->owner = static_cast<std::int32_t>(Key(Sc()));
    to[1] = body;
    to[2] = 0;
    to[6] = 1;
    to[5] = static_cast<unsigned char>(parameter);
    return to;
}

// Sprite_Current's position (+0x34..+0x3C) from `from`, read afresh.
void CopyPosition(const unsigned char* from) {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(from)->height);
}

// The same from the owner, the owner read again for each word.
void CopyOwnerPosition() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

// The sprite's colour +0x5D..+0x5F, each moved by `by` (a byte).
void AddColour(unsigned by) {
    AddB(Sc()[0x5D], by);
    AddB(Sc()[0x5E], by);
    AddB(Sc()[0x5F], by);
}

// +9 down; true when it reaches 0 (read again).
bool CountDown() {
    Dec(Sc()[9]);
    return Sc()[9] == 0;
}

using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// A draw-mode packet (tpage `tpage`, dithered) committed to layer `layer`.
void DrawMode(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

constexpr std::uint32_t kF0 = 0;             // 0.0f

constexpr std::uint32_t kF240 = 0x43700000;  // 240.0f

void PutU32(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(v)); }

}  // namespace

#define S06_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4A3150: the kind-2 task. A six-entry stack table by +1:
// Magic020_Start, _Darken, _Hold, _End, MagicFx_DoneAndFree,
// MagicFx_FlagTargetEnd.
S06_EXPORT void __cdecl Magic020_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Magic020_Start, bof3::addr::Magic020_Darken,
                                                 bof3::addr::Magic020_Hold,  bof3::addr::Magic020_End,
                                                 bof3::addr::MagicFx_DoneAndFree, bof3::addr::MagicFx_FlagTargetEnd};
    const unsigned phase = Sc()[1];
    if (phase >= 6) PastTable("Magic020_Task", phase, 6);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4A31A0: the owner's direction and position; +0xB and +9 0, +1
// on; the actor's animation 0xC. Four doubles (kind 1, 0x2E, +1 1: shadows)
// with +9 4, 3, 2, 1 and +0xB 3, 2, 1, 0; a fifth with +1 0 (the lead). The
// owner's sprite CLUT to the effect row (its answer to +0x27), +0x28 and +0x24
// the owner's, SpriteClut_SetStp on this task; a member's sound (2, 0); the
// owner's +0 bit 0x40.
S06_EXPORT void __cdecl Magic020_Start(void) {
    Sc()[8] = Owner()[8];
    CopyOwnerPosition();
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char* const shadow = MakeDouble(0x2E, 1);
        const unsigned char order = static_cast<unsigned char>(i ^ 3);
        shadow[9] = static_cast<unsigned char>(order + 1);
        shadow[0xB] = order;
    }
    MakeDouble(0x2E, 0);
    const unsigned row = MH_CALL(SpriteClut_CopyToFxRow)(Owner());
    Sc()[0x27] = static_cast<unsigned char>(row);
    Sc()[0x28] = Owner()[0x28];
    Sc()[0x24] = Owner()[0x24];
    MH_CALL(SpriteClut_SetStp)(Sc());
    if (ActorByte() < 3) MH_CALL(BattleActor_PlaySound)(2, 0);
    Owner()[0] |= 0x40;
}

// original 0x4A33E0: once +0xB is 0xFF (the lead has arrived): the fade drawn
// (Magic020_DrawFade), +9 up by 4; at 0x10 sound 0x100, +0xB 0, +1 on, and
// eight slashes (kind 1, 0x2E, +1 2) with +0xB 0..7 and +9 1, 9, .., 57,
// counted in +0xB.
S06_EXPORT void __cdecl Magic020_Darken(void) {
    if (Sc()[0xB] != 0xFF) return;
    Call0(bof3::addr::Magic020_DrawFade);
    AddB(Sc()[9], 4);
    if (Sc()[9] != 0x10) return;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    unsigned char order = 0;
    for (unsigned delay = 1; delay < 0x41; delay += 8) {
        const unsigned slot = NewTask(0x2E);
        unsigned char* const sc = Sc();
        unsigned char* const slash = TaskSlot(slot);
        records::BattleTask* const tslash = rv::As<records::BattleTask>(slash);
        tslash->owner = static_cast<std::int32_t>(Key(sc));
        slash[1] = 2;
        slash[2] = 0;
        slash[0xB] = order;
        slash[9] = static_cast<unsigned char>(delay);
        Inc(sc[0xB]);
        ++order;
    }
}

// original 0x4A34B0: the fade drawn; once +0xB is 0 (the slashes are done), +9
// down, at 0 +1 on.
S06_EXPORT void __cdecl Magic020_Hold(void) {
    Call0(bof3::addr::Magic020_DrawFade);
    if (Sc()[0xB] != 0) return;
    if (!CountDown()) return;
    Inc(Sc()[1]);
}

// original 0x4A34E0: once +0xB is 0xFF (the lead has faded): the effect CLUT
// row restored, the owner's bit 0x40 cleared, the actor's animation 4, +1 on.
S06_EXPORT void __cdecl Magic020_End(void) {
    if (Sc()[0xB] != 0xFF) return;
    MH_CALL(SpriteClut_RestoreFxRow)();
    Owner()[0] &= 0xBF;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Inc(Sc()[1]);
}

// original 0x4A3510: the children's task, jmp through Magic020Child_Bodies
// (three entries, 0x65A6D0) by +1, unchecked; ours aborts past it.
S06_EXPORT void __cdecl Magic020Child_Task(void) {
    static constexpr std::uint32_t kBodies[3] = {bof3::addr::Magic020Lead_Run, bof3::addr::Magic020Shadow_Run,
                                                 bof3::addr::Magic020Slash_Run};
    const unsigned body = Sc()[1];
    if (body >= 3) PastTable("Magic020Child_Task", body, 3);
    magic_harness::Phase(kBodies[body])();
}

// original 0x4A3530: an eight-entry stack table by +2 (_Start, _Approach,
// _Tick, _WaitSlashes, _Flash, _Return, _Brighten, BattleFx_FreeTask); then
// while +0 and +2 are set the screen update.
S06_EXPORT void __cdecl Magic020Lead_Run(void) {
    static constexpr std::uint32_t kSteps[8] = {bof3::addr::Magic020Lead_Start,       bof3::addr::Magic020Lead_Approach,
                                                bof3::addr::Magic020Lead_Tick,        bof3::addr::Magic020Lead_WaitSlashes,
                                                bof3::addr::Magic020Lead_Flash,       bof3::addr::Magic020Lead_Return,
                                                bof3::addr::Magic020Lead_Brighten,    bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 8) PastTable("Magic020Lead_Run", phase, 8);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const sc = Sc();
    if (sc[0] != 0 && sc[2] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A35A0: +0x27 the owner's, +0x2B 1, the owner's position, +9
// 0x10, +2 on.
S06_EXPORT void __cdecl Magic020Lead_Start(void) {
    Sc()[0x27] = Owner()[0x27];
    Sc()[0x2B] = 1;
    CopyOwnerPosition();
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A3600: a step of 0x60 toward the source sprite; once within
// 0x20000 of it, +9 the effect size, the owner's +0xB 0xFF (arrived), +2 on.
S06_EXPORT void __cdecl Magic020Lead_Approach(void) {
    MH_CALL(MagicFx_StepToward)(Source(), 0x60);
    if (MH_CALL(MagicFx_NearSprite)(Source(), 0x20000) == 0) return;
    const unsigned char size = MH_CALL(BattleActor_FxSize)();
    Sc()[9] = size;
    Owner()[0xB] = 0xFF;
    Inc(Sc()[2]);
}

// original 0x4A3650: the script ticked once; +9 down; at 0 +2 on.
S06_EXPORT void __cdecl Magic020Lead_Tick(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    if (!CountDown()) return;
    Inc(Sc()[2]);
}

// original 0x4A3680: once the owner's +0xB is 0 (the slashes are done) the
// script ticked once until its end; then +9 0x10, +2 on.
S06_EXPORT void __cdecl Magic020Lead_WaitSlashes(void) {
    if (Owner()[0xB] != 0) return;
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4A36B0: +9 down; at 0 +0 bit 0x20, +0x5C 1, the colour 0xC0, +2
// on.
S06_EXPORT void __cdecl Magic020Lead_Flash(void) {
    if (!CountDown()) return;
    Sc()[0] |= 0x20;
    Sc()[0x5C] = 1;
    Sc()[0x5D] = 0xC0;
    Sc()[0x5E] = 0xC0;
    Sc()[0x5F] = 0xC0;
    Inc(Sc()[2]);
}

// original 0x4A3700: the colour down by 8; at 0x80 back at the owner's
// position and +2 on.
S06_EXPORT void __cdecl Magic020Lead_Return(void) {
    AddColour(0xF8);
    if (Sc()[0x5D] != 0x80) return;
    CopyOwnerPosition();
    Inc(Sc()[2]);
}

// original 0x4A3770: the colour up by 0x10; when it wraps to 0 the owner's
// +0xB 0xFF (the lead is done) and +2 on.
S06_EXPORT void __cdecl Magic020Lead_Brighten(void) {
    AddColour(0x10);
    if (Sc()[0x5D] != 0) return;
    Owner()[0xB] = 0xFF;
    Inc(Sc()[2]);
}

// original 0x4A37C0: a three-entry stack table by +2 (_Start, _Approach,
// BattleFx_FreeTask); then while +0 and +2 are set the screen update.
S06_EXPORT void __cdecl Magic020Shadow_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::Magic020Shadow_Start, bof3::addr::Magic020Shadow_Approach,
                                                bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("Magic020Shadow_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const sc = Sc();
    if (sc[0] != 0 && sc[2] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A3810: +9 (its delay) down; at 0: +0 bit 0x20, +0x5C 1, the
// colour (0xFF - +0xB) x 0x14 (a byte: the later shadows darker), +0x2B 0,
// +0x27 and the position the owner's, +2 on.
S06_EXPORT void __cdecl Magic020Shadow_Start(void) {
    if (!CountDown()) return;
    Sc()[0] |= 0x20;
    Sc()[0x5C] = 1;
    for (unsigned c = 0x5D; c < 0x60; ++c) {
        unsigned char* const sc = Sc();
        sc[c] = static_cast<unsigned char>((0xFFu - sc[0xB]) * 0x14u);
    }
    Sc()[0x2B] = 0;
    Sc()[0x27] = Owner()[0x27];
    CopyOwnerPosition();
    Inc(Sc()[2]);
}

// original 0x4A38D0: a step of 0x60 toward the source sprite; within 0x20000
// of it, +2 on.
S06_EXPORT void __cdecl Magic020Shadow_Approach(void) {
    MH_CALL(MagicFx_StepToward)(Source(), 0x60);
    if (MH_CALL(MagicFx_NearSprite)(Source(), 0x20000) == 0) return;
    Inc(Sc()[2]);
}

// original 0x4A3900: the frame-offset table 0x9039D8 the effects' (0x8C5D80);
// a two-entry stack table by +2 (_Start, _Play); while +0 and +2 are set the
// sprite queued (Sprite_QueueOverlay); the table the battle's (0x8B3580) again.
S06_EXPORT void __cdecl Magic020Slash_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::Magic020Slash_Start, bof3::addr::Magic020Slash_Play};
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetSlash.v)));
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("Magic020Slash_Run", phase, 2);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const sc = Sc();
    if (sc[0] != 0 && sc[2] != 0) MH_CALL(Sprite_QueueOverlay)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4A3950: +9 (its delay) down; at 0: the direction and position the
// source sprite's; the screen point (BattleActor_UpdateScreenXY); +0x29 0,
// bank +0x25 0x1D, +0x26 0, +0x24 0x84, the word +0x2C 0, +0x2B 0, +0x28 0,
// +0x27 Magic020Slash_Animations[+0xB] - 0x50 (twelve bytes, unchecked; ours
// aborts past them); +0x2A 1 facing 1 or 2, else 0; the animation +0xB; +2 on.
S06_EXPORT void __cdecl Magic020Slash_Start(void) {
    if (!CountDown()) return;
    const unsigned char* const src = Source();
    Sc()[8] = src[8];
    CopyPosition(src);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[0x29] = 0;
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x24] = 0x84;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 0;
    Sc()[0x28] = 0;
    {
        unsigned char* const sc = Sc();
        const unsigned index = sc[0xB];
        if (index >= kSlashAnimationCount)
            bof3::Fatal("Magic020Slash_Start: +0xB %u, past the %u animations", index, kSlashAnimationCount);
        sc[0x27] = static_cast<unsigned char>(Mem(::bof3::Img(Magic020Slash_Animations_addr))[index] - 0x50);
    }
    {
        unsigned char* const sc = Sc();
        sc[0x2A] = (sc[8] == 1 || sc[8] == 2) ? 1 : 0;
    }
    MH_CALL(Sprite_SetAnimation)(Sc()[0xB]);
    Inc(Sc()[2]);
}

// original 0x4A3A30: the script ticked until its end; then the last slash (+0xB
// 7) flags the target 0x40; the owner's +0xB down; the task freed (a tail jmp).
S06_EXPORT void __cdecl Magic020Slash_Play(void) {
    if (MH_CALL(Sprite_ScriptTick)() == 0) return;
    if (Sc()[0xB] == 7) MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A3A70: the darkening, one semi-transparent tile over the screen
// (0, 0, 320 x 240) of grey +9 x 15 (the word kept at 0x903852 and read back),
// between draw modes tpage 0x35 and 0x15 on layer 2.
S06_EXPORT void __cdecl Magic020_DrawFade(void) {
    DrawMode(0x35, 2);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetTile)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutU32(p + 0xC, kF0);
    PutU32(p + 8, std::bit_cast<std::uint32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    PutU32(p + 0x14, std::bit_cast<std::uint32_t>(Widescreen_FillWidth()));   // kF320 narrow
    PutU32(p + 0x18, kF240);
    const unsigned grey = (static_cast<unsigned>(Sc()[9]) * 15u) & 0xFFFF;
    SetWord(Mem(::bof3::Img(kShade.v)), grey);
    p[4] = static_cast<unsigned char>(grey);
    p[5] = Mem(::bof3::Img(kShade.v))[0];
    p[6] = Mem(::bof3::Img(kShade.v))[0];
    MH_CALL(Gfx_CommitPrim)(2, 0x1C);
    DrawMode(0x15, 2);
}

void Magic020Lead_Inject() {
    BOF3_INJECT(Magic020_Task);
    BOF3_INJECT(Magic020_Start);
    BOF3_INJECT(Magic020_Darken);
    BOF3_INJECT(Magic020_Hold);
    BOF3_INJECT(Magic020_End);
    BOF3_INJECT(Magic020Child_Task);
    BOF3_INJECT(Magic020Lead_Run);
    BOF3_INJECT(Magic020Lead_Start);
    BOF3_INJECT(Magic020Lead_Approach);
    BOF3_INJECT(Magic020Lead_Tick);
    BOF3_INJECT(Magic020Lead_WaitSlashes);
    BOF3_INJECT(Magic020Lead_Flash);
    BOF3_INJECT(Magic020Lead_Return);
    BOF3_INJECT(Magic020Lead_Brighten);
    BOF3_INJECT(Magic020Shadow_Run);
    BOF3_INJECT(Magic020Shadow_Start);
    BOF3_INJECT(Magic020Shadow_Approach);
    BOF3_INJECT(Magic020Slash_Run);
    BOF3_INJECT(Magic020Slash_Start);
    BOF3_INJECT(Magic020Slash_Play);
    BOF3_INJECT(Magic020_DrawFade);
}
