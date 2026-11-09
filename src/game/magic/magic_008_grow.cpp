// Moved here by tools/move_funcs.py from magic_s06.cpp (docs/source-layout-round.md).
#include "game/magic/magic_008_grow.h"

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

using move_script::Word;

// The cells the overlays read beyond the harness's names.
constexpr ::bof3::image::Va kAbility{0x904B80};        // u16: the ability being cast

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

std::uint16_t Ability() { return Word(Mem(::bof3::Img(kAbility.v))); }

std::uint32_t Key(const void* p) { return Low32(p); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v)); }

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

}  // namespace

#define S06_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4A2190: the kind-2 task. A three-entry stack table by +1:
// Magic008_Start, Magic008_Apply, MagicFx_EndWhenChildrenDone.
S06_EXPORT void __cdecl Magic008_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Magic008_Start, bof3::addr::Magic008_Apply,
                                                 bof3::addr::MagicFx_EndWhenChildrenDone};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Magic008_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4A21C0: the owner's direction and position; +4 the buff stat
// (Magic008_BuffStat), +0xB 0, +1 on; the actor's animation 0xC; a double
// (kind 1, 0x1F) with +1 2 - Magic008Mirror_Run - counted in +0xB; the owner's
// +0 bit 0x40 set; CLUT strip words 0x1A00..0x1A1F back from their source.
S06_EXPORT void __cdecl Magic008_Start(void) {
    Sc()[8] = Owner()[8];
    CopyOwnerPosition();
    const unsigned char stat = MH_AT(unsigned char (__cdecl*)(), bof3::addr::Magic008_BuffStat)();
    Sc()[4] = stat;
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    MakeDouble(0x1F, 2);
    Inc(Sc()[0xB]);
    Owner()[0] |= 0x40;
    for (unsigned k = 0x1A00; k < 0x1A20; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
}

// original 0x4A2320: while +0xB (the double lives), nothing. Then, unless +4 is
// 0xFF: the position from the source sprite, MagicFx_ApplyBuff(stat
// MagicFx_BuffStats[+4 & 3], the target) and its popup (kind 1, 0x48: owner
// this task, +4 the stat index if the buff took, else 8; +9 0x15, +0xA 8),
// counted in +0xB. Then the actor's animation 4, the owner's bit 0x40
// cleared, +1 on.
S06_EXPORT void __cdecl Magic008_Apply(void) {
    if (Sc()[0xB] != 0) return;
    if (Sc()[4] != 0xFF) {
        CopyPosition(Source());
        const unsigned char took = MH_CALL(MagicFx_ApplyBuff)(MagicFx_BuffStats[Sc()[4] & 3], TargetByte());
        const unsigned slot = NewTask(0x48);
        unsigned char* const popup = TaskSlot(slot);
        records::BattleTask* const tpopup = rv::As<records::BattleTask>(popup);
        unsigned char* const sc = Sc();
        tpopup->owner = static_cast<std::int32_t>(Key(sc));
        popup[4] = took != 0 ? sc[4] : 8;
        popup[9] = 0x15;
        popup[0xA] = 8;
        Inc(sc[0xB]);
    }
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    Inc(Sc()[1]);
}

// original 0x4A2440: 7 when the ability (0x904B80) is 7, else 0xFF (none). In
// eax whole: the original computes it from al over whatever eax held, and the
// mask leaves only 7 or 0xFF.
S06_EXPORT unsigned char __cdecl Magic008_BuffStat(void) {
    return Ability() == 7 ? 7 : 0xFF;
}

// original 0x4A2460: the doubles' task, jmp through Magic008Double_Bodies
// (six entries, 0x65A6A0) by +1, unchecked; ours aborts past it.
S06_EXPORT void __cdecl Magic008Double_Task(void) {
    static constexpr std::uint32_t kBodies[6] = {bof3::addr::Magic008Grow_Run,      bof3::addr::Magic008Glow_Run,
                                                 bof3::addr::Magic008Mirror_Run,    bof3::addr::Magic008Dash_Run,
                                                 bof3::addr::Magic008TwoBlows_Run,  bof3::addr::Magic008ThreeBlows_Run};
    const unsigned body = Sc()[1];
    if (body >= 6) PastTable("Magic008Double_Task", body, 6);
    magic_harness::Phase(kBodies[body])();
}

// original 0x4A2480: a six-entry stack table by +2 (_Start, _Grow, _Strike,
// _Wait, _Shrink, BattleFx_FreeTask); then while +0 is set the screen update.
S06_EXPORT void __cdecl Magic008Grow_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::Magic008Grow_Start,  bof3::addr::Magic008Grow_Grow,
                                                bof3::addr::Magic008Grow_Strike, bof3::addr::Magic008Grow_Wait,
                                                bof3::addr::Magic008Grow_Shrink, bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 6) PastTable("Magic008Grow_Run", phase, 6);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A24E0: the scale +0x40 / +0x44 0x10000, +0x48 2, +9 0x1E, +2 on.
S06_EXPORT void __cdecl Magic008Grow_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->scale_x, 0x10000);
    rv::Store(rv::Current<records::BattleTask>()->scale_y, 0x10000);
    Sc()[0x48] = 2;
    Sc()[9] = 0x1E;
    Inc(Sc()[2]);
}

// original 0x4A2520: the scale up by 0x400 (x) and 0x800 (y); +9 down; at 0 +9
// the actor's effect size (BattleActor_FxSize) and +2 on.
S06_EXPORT void __cdecl Magic008Grow_Grow(void) {
    AddLong(Sc() + 0x40, 0x400);
    AddLong(Sc() + 0x44, 0x800);
    if (!CountDown()) return;
    const unsigned char size = MH_CALL(BattleActor_FxSize)();
    Sc()[9] = size;
    Inc(Sc()[2]);
}

// original 0x4A2580: the script ticked once; +9 down; at 0 the target flagged
// 0x40, the actor's sound (2, 4), +2 on.
S06_EXPORT void __cdecl Magic008Grow_Strike(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    if (!CountDown()) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    MH_CALL(BattleActor_PlaySound)(2, 4);
    Inc(Sc()[2]);
}

// original 0x4A25C0: the script ticked once until it reports its end; then
// sound 0x101, +9 0xF, +2 on.
S06_EXPORT void __cdecl Magic008Grow_Wait(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 0xF;
    Inc(Sc()[2]);
}

// original 0x4A25F0: the scale down by 0x800 (x) and 0x1000 (y); +9 down; at
// 0 the owner's +0xB 0xFF (the double is done) and +2 on.
S06_EXPORT void __cdecl Magic008Grow_Shrink(void) {
    AddLong(Sc() + 0x40, 0xFFFFF800u);
    AddLong(Sc() + 0x44, 0xFFFFF000u);
    if (!CountDown()) return;
    Owner()[0xB] = 0xFF;
    Inc(Sc()[2]);
}

// original 0x4A2640: a five-entry stack table by +2 (_Start, _Brighten,
// _Strike, _Wait, _Fade); then while +0 is set the screen update.
S06_EXPORT void __cdecl Magic008Glow_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::Magic008Glow_Start, bof3::addr::Magic008Glow_Brighten,
                                                bof3::addr::Magic008Glow_Strike, bof3::addr::Magic008Glow_Wait,
                                                bof3::addr::Magic008Glow_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("Magic008Glow_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A2690: +0 bit 0x20, +0x5C 1, +0x2B 1, the colour +0x5D..+0x5F
// 0x80; at the field's kind-2 point (x, z) at the owner's height; the scale
// 0x18000 / 0x28000, +0x48 2, +0x27 the owner's; sound 0x100; +9 0x20, +2 on.
S06_EXPORT void __cdecl Magic008Glow_Start(void) {
    Sc()[0] |= 0x20;
    Sc()[0x5C] = 1;
    Sc()[0x2B] = 1;
    Sc()[0x5D] = 0x80;
    Sc()[0x5E] = 0x80;
    Sc()[0x5F] = 0x80;
    rv::Store(rv::Current<records::BattleTask>()->x, Field_Kind2X);
    rv::Store(rv::Current<records::BattleTask>()->z, Field_Kind2Z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    rv::Store(rv::Current<records::BattleTask>()->scale_x, 0x18000);
    rv::Store(rv::Current<records::BattleTask>()->scale_y, 0x28000);
    Sc()[0x48] = 2;
    Sc()[0x27] = Owner()[0x27];
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4A2750: the colour up by 3; +9 down; at 0 +9 the effect size and
// +2 on.
S06_EXPORT void __cdecl Magic008Glow_Brighten(void) {
    AddColour(3);
    if (!CountDown()) return;
    const unsigned char size = MH_CALL(BattleActor_FxSize)();
    Sc()[9] = size;
    Inc(Sc()[2]);
}

// original 0x4A27B0: the script ticked once; +9 down; at 0 the target flagged
// 0x40, the actor's sound (2, 4), +9 0x20, +2 on.
S06_EXPORT void __cdecl Magic008Glow_Strike(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    if (!CountDown()) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    MH_CALL(BattleActor_PlaySound)(2, 4);
    Sc()[9] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4A2800: the script ticked once until its end; then sound 0x101,
// +9 0x20, +2 on.
S06_EXPORT void __cdecl Magic008Glow_Wait(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 0x20;
    Inc(Sc()[2]);
}

// original 0x4A2830: the colour down by 3; +9 down; at 0 the owner's +0xB
// 0xFF and the task freed (a tail jmp).
S06_EXPORT void __cdecl Magic008Glow_Fade(void) {
    AddColour(0xFD);
    if (!CountDown()) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4A2890: a four-entry stack table by +2 (_Size, _Strike, _Hit,
// _Flash); no screen update after it (the steps tail-call their own).
S06_EXPORT void __cdecl Magic008Mirror_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::Magic008Mirror_Size, bof3::addr::Magic008Mirror_Strike,
                                                bof3::addr::Magic008Mirror_Hit, bof3::addr::Magic008Mirror_Flash};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("Magic008Mirror_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4A28D0: +9 the effect size, +2 on, the screen update (tail).
S06_EXPORT void __cdecl Magic008Mirror_Size(void) {
    const unsigned char size = MH_CALL(BattleActor_FxSize)();
    Sc()[9] = size;
    Inc(Sc()[2]);
    MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A28F0: +9 down. At 0: the script ticked once, the actor's sound
// (2, 4), +2 on. Else the script ticked once, and when it reports its end the
// sound, +9 0 and +2 on. The screen update (tail) either way.
S06_EXPORT void __cdecl Magic008Mirror_Strike(void) {
    if (CountDown()) {
        MH_CALL(Sprite_ScriptTickOnce)();
        MH_CALL(BattleActor_PlaySound)(2, 4);
        Inc(Sc()[2]);
    } else if (MH_CALL(Sprite_ScriptTickOnce)() != 0) {
        MH_CALL(BattleActor_PlaySound)(2, 4);
        Sc()[9] = 0;
        Inc(Sc()[2]);
    }
    MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A2950: the script ticked once until its end; then the target
// flagged 0x40, the owner's +0xB down (the double counted off), +2 on. The
// screen update (tail) either way.
S06_EXPORT void __cdecl Magic008Mirror_Hit(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() != 0) {
        MH_CALL(Battle_SetTargetFlag40)(TargetByte());
        Dec(Owner()[0xB]);
        Inc(Sc()[2]);
    }
    MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A2990: while +9 is below 0xC: the screen flash
// (Magic008_DrawFlash) unless the ability is 0xA4, and +9 up by 4. Then the
// task freed (a tail jmp).
S06_EXPORT void __cdecl Magic008Mirror_Flash(void) {
    if (Sc()[9] >= 0xC) {
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    if (Ability() != 0xA4) Call0(bof3::addr::Magic008_DrawFlash);
    AddB(Sc()[9], 4);
}

void Magic008Grow_Inject() {
    BOF3_INJECT(Magic008_Task);
    BOF3_INJECT(Magic008_Start);
    BOF3_INJECT(Magic008_Apply);
    BOF3_INJECT(Magic008_BuffStat);
    BOF3_INJECT(Magic008Double_Task);
    BOF3_INJECT(Magic008Grow_Run);
    BOF3_INJECT(Magic008Grow_Start);
    BOF3_INJECT(Magic008Grow_Grow);
    BOF3_INJECT(Magic008Grow_Strike);
    BOF3_INJECT(Magic008Grow_Wait);
    BOF3_INJECT(Magic008Grow_Shrink);
    BOF3_INJECT(Magic008Glow_Run);
    BOF3_INJECT(Magic008Glow_Start);
    BOF3_INJECT(Magic008Glow_Brighten);
    BOF3_INJECT(Magic008Glow_Strike);
    BOF3_INJECT(Magic008Glow_Wait);
    BOF3_INJECT(Magic008Glow_Fade);
    BOF3_INJECT(Magic008Mirror_Run);
    BOF3_INJECT(Magic008Mirror_Size);
    BOF3_INJECT(Magic008Mirror_Strike);
    BOF3_INJECT(Magic008Mirror_Hit);
    BOF3_INJECT(Magic008Mirror_Flash);
}
