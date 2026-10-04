// Two spell overlays compiled into the exe, round nine group S06
// (docs/magic_s06.md): the PSX's MAGIC008.EMI and MAGIC020.EMI, Magic_Rows
// rows 42 and 8. Read one id down (docs/cut-content.md section 2) the sibling
// labels row 42's nine abilities Gambit, Mind Flay, Blind, Devour, (no label),
// Syphon, Feign Swing, Backhand and Risky Blow, and row 8's Disembowel - names
// that are hypotheses. The names below say what the code does.
//
//   - MAGIC008 0x4A2190..0x4A3140: the kind-2 task copies the acting actor's
//     record (its first 0x80 bytes, a sprite) into a child - a double of the
//     actor - plays the actor's animation 0xC, then applies one buff to the
//     target (MagicFx_ApplyBuff, the stat by ability 7 or none) with its
//     popup, and ends when the double has. The doubles are one kind-1 task
//     (parameter 0x1F) with six bodies by +1 (0x65A6A0): a double that grows
//     and shrinks, one that glows at the field's kind-2 point, the mirror this
//     overlay makes (it strikes and flashes the screen), one that dashes
//     beside its owner, and two that strike two or three times, each waiting
//     for the target's reaction. MAGIC017, 018 and 019 make the other five.
//   - MAGIC020 0x4A3150..0x4A3B18: five doubles of the actor - four shadows
//     at staggered delays and one lead - run to the source sprite; when the
//     lead arrives the screen darkens and eight slashes (sprite bank 0x1D,
//     animations by 0x65A6DC) play one after another at the source; the last
//     flags the target; the lead then returns and fades.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table, and an index past one of the two .data tables read by +0xB,
// abort where the original would read whatever follows
// (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s06.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kAbility = 0x904B80;        // u16: the ability being cast
constexpr std::uint32_t kFrameSet = 0x9039D8;       // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetSlash = 0x8C5D80;
constexpr std::uint32_t kShade = 0x903852;          // u16: DamageScratch +2, Magic020_DrawFade's shade
constexpr std::uint32_t kDashOffsets = 0x65A6B8;    // Magic008Dash_Offsets: 3 x (dword dx, dword dz)
constexpr unsigned kDashOffsetCount = 3;
constexpr std::uint32_t kSlashAnimations = 0x65A6DC;   // Magic020Slash_Animations: 12 bytes to the next table
constexpr unsigned kSlashAnimationCount = 12;

// Capcom's, unnamed, in no group: 0x446770 turns the dx / dz pair +0xC / +0x10
// of the task it is given by its direction byte +8 (docs/magic_s22.md);
// 0x435A70 plays animation `animation` on enemy `actor` (BattleEnemy_SetAnimation
// with 0x939AD8 pointed at that enemy's record and put back).
constexpr std::uint32_t kTurnOffset = bof3::addr::Battle_TurnVectorC;
constexpr std::uint32_t kEnemyAnimation = 0x435A70;
// Magic008Dash_Run's stack table names S04's KickImage_Tick (ours now).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned char ActorByte() { return Mem(at::kActor)[0]; }
unsigned char* Source() { return Pointer(at::kSource); }
std::uint16_t Ability() { return Word(Mem(kAbility)); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v)); }

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The acting actor's record as the originals index it: a member below 3, else
// the enemy by index - 3, unchecked.
unsigned char* ActorRecord(unsigned char actor) {
    if (actor < 3) return Mem(at::kParty + actor * at::kPartyStride);
    return Mem(at::kEnemies + (static_cast<std::uint32_t>(actor) - 3) * at::kEnemyStride);
}
// A battle actor's state byte +1, as the reaction steps read it.
unsigned char ActorState(unsigned char actor) { return ActorRecord(actor)[1]; }

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
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(to + k, Long(from + k));
    SetLong(to + 0x80, static_cast<std::int32_t>(Key(Sc())));
    to[1] = body;
    to[2] = 0;
    to[6] = 1;
    to[5] = static_cast<unsigned char>(parameter);
    return to;
}

// Sprite_Current's position (+0x34..+0x3C) from `from`, read afresh.
void CopyPosition(const unsigned char* from) {
    SetLong(Sc() + 0x34, Long(from + 0x34));
    SetLong(Sc() + 0x38, Long(from + 0x38));
    SetLong(Sc() + 0x3C, Long(from + 0x3C));
}
// The same from the owner, the owner read again for each word.
void CopyOwnerPosition() {
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
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
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// A draw-mode packet (tpage `tpage`, dithered) committed to layer `layer`.
void DrawMode(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

constexpr std::uint32_t kF0 = 0;             // 0.0f
constexpr std::uint32_t kF120 = 0x42F00000;  // 120.0f
constexpr std::uint32_t kF240 = 0x43700000;  // 240.0f
constexpr std::uint32_t kF319 = 0x439F8000;  // 319.0f
constexpr std::uint32_t kF320 = 0x43A00000;  // 320.0f
constexpr std::uint32_t kF359 = 0x43B38000;  // 359.0f
void PutU32(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(v)); }
void PutRgb(unsigned char* at, unsigned char v) { at[0] = at[1] = at[2] = v; }

}  // namespace

#define S06_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC008 (row 42)

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
        unsigned char* const sc = Sc();
        SetLong(popup + 0x80, static_cast<std::int32_t>(Key(sc)));
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

// --- body 0: a double that grows, strikes and shrinks -----------------------

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
    SetLong(Sc() + 0x40, 0x10000);
    SetLong(Sc() + 0x44, 0x10000);
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

// --- body 1: a double that glows at the field's kind-2 point ----------------

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
    SetLong(Sc() + 0x34, Field_Kind2X);
    SetLong(Sc() + 0x38, Field_Kind2Z);
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    SetLong(Sc() + 0x40, 0x18000);
    SetLong(Sc() + 0x44, 0x28000);
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

// --- body 2: the mirror Magic008_Start makes ---------------------------------

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

// original 0x4A29C0: a flash over the screen, two semi-transparent gouraud
// quads (x 0..319; y 0..120 white at the top to shade +9 x 16, then 120..359
// back to white), between draw modes tpage 0x35 and 0x15 on layer 1. The shade
// is read once, before the first quad.
S06_EXPORT void __cdecl Magic008_DrawFlash(void) {
    DrawMode(0x35, 1);
    unsigned char* p = Gfx_PacketNext;
    const unsigned char shade = static_cast<unsigned char>(Sc()[9] << 4);
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutU32(p + 8, kF0);
    PutU32(p + 0xC, kF0);
    PutU32(p + 0x18, kF319);
    PutU32(p + 0x1C, kF0);
    PutU32(p + 0x28, kF0);
    PutU32(p + 0x2C, kF120);
    PutU32(p + 0x38, kF319);
    PutU32(p + 0x3C, kF120);
    PutRgb(p + 4, 0x80);
    PutRgb(p + 0x14, 0x80);
    PutRgb(p + 0x24, shade);
    PutRgb(p + 0x34, shade);
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutU32(p + 8, kF0);
    PutU32(p + 0xC, kF120);
    PutU32(p + 0x18, kF319);
    PutU32(p + 0x1C, kF120);
    PutU32(p + 0x28, kF0);
    PutU32(p + 0x2C, kF359);
    PutU32(p + 0x38, kF319);
    PutU32(p + 0x3C, kF359);
    PutRgb(p + 4, shade);
    PutRgb(p + 0x14, shade);
    PutRgb(p + 0x24, 0x80);
    PutRgb(p + 0x34, 0x80);
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    DrawMode(0x15, 1);
}

// --- body 3: a double that dashes beside its owner ---------------------------

// original 0x4A2AF0: a six-entry stack table by +2 (_Start, _Follow, _Strike,
// MAGIC018/019's 0x4A01C0, _Fade, BattleFx_FreeTask); then while +0 and +2
// are set the screen update.
S06_EXPORT void __cdecl Magic008Dash_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::Magic008Dash_Start,  bof3::addr::Magic008Dash_Follow,
                                                bof3::addr::Magic008Dash_Strike, bof3::addr::KickImage_Tick,
                                                bof3::addr::Magic008Dash_Fade,   bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 6) PastTable("Magic008Dash_Run", phase, 6);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const sc = Sc();
    if (sc[0] != 0 && sc[2] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A2B50: +0 bit 0x20, +0x5C 1, +0x2B 1, the colour 0xB0; +0x27,
// the direction and the position the owner's; the offset +0xC / +0x10 from
// Magic008Dash_Offsets by +0xB (three pairs, unchecked; ours aborts past
// them), turned by the direction (0x446770); +9 the effect size; +2 on.
S06_EXPORT void __cdecl Magic008Dash_Start(void) {
    Sc()[0] |= 0x20;
    Sc()[0x5C] = 1;
    Sc()[0x2B] = 1;
    Sc()[0x5D] = 0xB0;
    Sc()[0x5E] = 0xB0;
    Sc()[0x5F] = 0xB0;
    Sc()[0x27] = Owner()[0x27];
    Sc()[8] = Owner()[8];
    CopyOwnerPosition();
    unsigned char* const sc = Sc();
    const unsigned index = sc[0xB];
    if (index >= kDashOffsetCount) bof3::Fatal("Magic008Dash_Start: +0xB %u, past the %u offsets", index, kDashOffsetCount);
    SetLong(sc + 0xC, Long(Mem(kDashOffsets + 8 * index)));
    SetLong(sc + 0x10, Long(Mem(kDashOffsets + 8 * index + 4)));
    MH_AT(void (__cdecl*)(unsigned char*), kTurnOffset)(Sc());
    const unsigned char size = MH_CALL(BattleActor_FxSize)();
    Sc()[9] = size;
    Inc(Sc()[2]);
}

namespace {
// The dashing double's position: the owner's plus its offset (+0xC, +0x10).
void BesideOwner() {
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x34)) +
                                                    static_cast<std::uint32_t>(Long(Sc() + 0xC))));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Owner() + 0x38)) +
                                                    static_cast<std::uint32_t>(Long(Sc() + 0x10))));
}
// On the owner (x, z only).
void OnOwner() {
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
}
}  // namespace

// original 0x4A2C40: on odd frames on the owner, the script ticked once, +9
// down, at 0 +9 = +0xB + 4 and +2 on; on even frames beside the owner.
S06_EXPORT void __cdecl Magic008Dash_Follow(void) {
    if ((Frame_Counter & 1) == 0) {
        BesideOwner();
        return;
    }
    OnOwner();
    MH_CALL(Sprite_ScriptTickOnce)();
    if (!CountDown()) return;
    unsigned char* const sc = Sc();
    sc[9] = static_cast<unsigned char>(sc[0xB] + 4);
    Inc(Sc()[2]);
}

// original 0x4A2CD0: on odd frames on the owner, +9 down, at 0: when +0xB is 0
// (the first double) the actor's sound (2, 4) and the target flagged 0x40;
// +2 on. On even frames beside the owner.
S06_EXPORT void __cdecl Magic008Dash_Strike(void) {
    if ((Frame_Counter & 1) == 0) {
        BesideOwner();
        return;
    }
    OnOwner();
    if (!CountDown()) return;
    unsigned char* sc = Sc();
    if (sc[0xB] == 0) {
        MH_CALL(BattleActor_PlaySound)(2, 4);
        MH_CALL(Battle_SetTargetFlag40)(TargetByte());
        sc = Sc();
    }
    Inc(sc[2]);
}

// original 0x4A2D70: the colour down by 3; at 0x80 the owner's +0xB down and
// +2 on.
S06_EXPORT void __cdecl Magic008Dash_Fade(void) {
    AddColour(0xFD);
    if (Sc()[0x5D] != 0x80) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

// --- bodies 4 and 5: two or three blows ---------------------------------------

// original 0x4A2DC0: a seven-entry stack table by +2 (BattleFx_SetSize,
// _Strike, _WaitTwo, _ReactTwo, _Strike, _WaitTwo, BattleFx_FreeTask); then
// while +0 is set the screen update.
S06_EXPORT void __cdecl Magic008TwoBlows_Run(void) {
    static constexpr std::uint32_t kSteps[7] = {
        bof3::addr::BattleFx_SetSize,     bof3::addr::Magic008Blow_Strike, bof3::addr::Magic008Blow_WaitTwo,
        bof3::addr::Magic008Blow_ReactTwo, bof3::addr::Magic008Blow_Strike, bof3::addr::Magic008Blow_WaitTwo,
        bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 7) PastTable("Magic008TwoBlows_Run", phase, 7);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A2F40: a ten-entry stack table by +2 (BattleFx_SetSize; _Strike,
// _WaitThree, _ReactThree twice; _Strike, _WaitThree; BattleFx_FreeTask); then
// while +0 is set the screen update.
S06_EXPORT void __cdecl Magic008ThreeBlows_Run(void) {
    static constexpr std::uint32_t kSteps[10] = {
        bof3::addr::BattleFx_SetSize,        bof3::addr::Magic008Blow_Strike,     bof3::addr::Magic008Blow_WaitThree,
        bof3::addr::Magic008Blow_ReactThree, bof3::addr::Magic008Blow_Strike,     bof3::addr::Magic008Blow_WaitThree,
        bof3::addr::Magic008Blow_ReactThree, bof3::addr::Magic008Blow_Strike,     bof3::addr::Magic008Blow_WaitThree,
        bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 10) PastTable("Magic008ThreeBlows_Run", phase, 10);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A2FB0: the script ticked once; +9 down; at 0 the actor's sound
// (2, 0), the target flagged 0x40, +2 on.
S06_EXPORT void __cdecl Magic008Blow_Strike(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    if (!CountDown()) return;
    MH_CALL(BattleActor_PlaySound)(2, 0);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Inc(Sc()[2]);
}

// original 0x4A2E20: the script ticked once until its end; then at step 5 (the
// last blow's) the owner's +0xB 0xFF; +2 on.
S06_EXPORT void __cdecl Magic008Blow_WaitTwo(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    unsigned char* sc = Sc();
    if (sc[2] == 5) {
        Owner()[0xB] = 0xFF;
        sc = Sc();
    }
    Inc(sc[2]);
}

// original 0x4A2FF0: the same at step 8.
S06_EXPORT void __cdecl Magic008Blow_WaitThree(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    unsigned char* sc = Sc();
    if (sc[2] == 8) {
        Owner()[0xB] = 0xFF;
        sc = Sc();
    }
    Inc(sc[2]);
}

namespace {
// The reaction both React steps share, once the target is not out: while the
// target's state +1 is 6 (reacting), nothing - answers false. Else the double
// starts its next swing - a member's animation +8 + 0xC on Sprite_Current, the
// upload that just queued cancelled (the queue count down, that entry's x, y
// and record zeroed); an enemy's animation 2 through 0x435A70, with +0x4B
// 0xFF - +9 the effect size, +2 on; answers true.
bool React() {
    const unsigned char target = TargetByte();
    if (ActorState(target) == 6) return false;
    if (ActorByte() < 3) {
        MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[8] + 0xC));
        const unsigned char n = static_cast<unsigned char>(Gfx_UploadQueueCount - 1);
        Gfx_UploadQueueCount = n;
        Gfx_UploadQueueX[n] = 0;
        Gfx_UploadQueueRecord[n] = nullptr;
        Gfx_UploadQueueY[n] = 0;
    } else {
        Sc()[0x4B] = 0xFF;
        MH_AT(void (__cdecl*)(unsigned, unsigned), kEnemyAnimation)(ActorByte(), 2);
    }
    const unsigned char size = MH_CALL(BattleActor_FxSize)();
    Sc()[9] = size;
    Inc(Sc()[2]);
    return true;
}
}  // namespace

// original 0x4A2E50: when the target is out, the owner's +0xB 0xFF and +2 6
// (the free); else React.
S06_EXPORT void __cdecl Magic008Blow_ReactTwo(void) {
    if (MH_CALL(Battle_ActorIsOut)(TargetByte()) != 0) {
        Owner()[0xB] = 0xFF;
        Sc()[2] = 6;
        return;
    }
    React();
}

// original 0x4A3020: the same with +2 9; then, unless +2 is 9, when the
// ability is 0xB and Rand & 7 is 0 (one in eight) the owner's +0xB 0xFF and
// +2 9 - the blows cut short. The target's state 6 skips React but not the
// cut.
S06_EXPORT void __cdecl Magic008Blow_ReactThree(void) {
    if (MH_CALL(Battle_ActorIsOut)(TargetByte()) != 0) {
        Owner()[0xB] = 0xFF;
        Sc()[2] = 9;
    } else {
        React();
    }
    if (Sc()[2] == 9) return;
    if (Ability() != 0xB) return;
    if ((MH_CALL(Rand)() & 7) != 0) return;
    Owner()[0xB] = 0xFF;
    Sc()[2] = 9;
}

// ===========================================================================
// MAGIC020 (row 8)

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
        SetLong(slash + 0x80, static_cast<std::int32_t>(Key(sc)));
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

// --- body 0: the lead -----------------------------------------------------------

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

// --- body 1: a shadow -----------------------------------------------------------

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

// --- body 2: a slash -------------------------------------------------------------

// original 0x4A3900: the frame-offset table 0x9039D8 the effects' (0x8C5D80);
// a two-entry stack table by +2 (_Start, _Play); while +0 and +2 are set the
// sprite queued (Sprite_QueueOverlay); the table the battle's (0x8B3580) again.
S06_EXPORT void __cdecl Magic020Slash_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::Magic020Slash_Start, bof3::addr::Magic020Slash_Play};
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetSlash));
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("Magic020Slash_Run", phase, 2);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const sc = Sc();
    if (sc[0] != 0 && sc[2] != 0) MH_CALL(Sprite_QueueOverlay)();
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
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
    SetWord(Sc() + 0x2C, 0);
    Sc()[0x2B] = 0;
    Sc()[0x28] = 0;
    {
        unsigned char* const sc = Sc();
        const unsigned index = sc[0xB];
        if (index >= kSlashAnimationCount)
            bof3::Fatal("Magic020Slash_Start: +0xB %u, past the %u animations", index, kSlashAnimationCount);
        sc[0x27] = static_cast<unsigned char>(Mem(kSlashAnimations)[index] - 0x50);
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
    SetWord(Mem(kShade), grey);
    p[4] = static_cast<unsigned char>(grey);
    p[5] = Mem(kShade)[0];
    p[6] = Mem(kShade)[0];
    MH_CALL(Gfx_CommitPrim)(2, 0x1C);
    DrawMode(0x15, 2);
}

void MagicS06_Inject() {
    if (bof3::WantsShadow("magic_s06")) magic_s06::SelfTest();
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
    BOF3_INJECT(Magic008_DrawFlash);
    BOF3_INJECT(Magic008Dash_Run);
    BOF3_INJECT(Magic008Dash_Start);
    BOF3_INJECT(Magic008Dash_Follow);
    BOF3_INJECT(Magic008Dash_Strike);
    BOF3_INJECT(Magic008Dash_Fade);
    BOF3_INJECT(Magic008TwoBlows_Run);
    BOF3_INJECT(Magic008Blow_WaitTwo);
    BOF3_INJECT(Magic008Blow_ReactTwo);
    BOF3_INJECT(Magic008ThreeBlows_Run);
    BOF3_INJECT(Magic008Blow_Strike);
    BOF3_INJECT(Magic008Blow_WaitThree);
    BOF3_INJECT(Magic008Blow_ReactThree);
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
