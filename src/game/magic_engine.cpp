// The engine-side rows of Magic_Rows (docs/takeover-queue-round9-spells.md
// section 3): the five rows whose file is 0xFFFF have their handler in the
// battle engine, not in a BMAGIC overlay. Read one id down (the names are
// hypotheses, docs/cut-content.md section 2):
//
//   - rows 0 and 126, 0x4378D0 (row 126: id 0x7F, TCRF's Assault): a hold of
//     60 frames, then flag 0x40 on the target and the effect done;
//   - row 108, 0x4525F0 (id 0xD9, Restore Form): the acting member's state
//     byte set to 6 (the reaction state) with sub-states 4 and 4, then a wait
//     until no actor is pending;
//   - row 123, 0x43F3B0 (id 0x8B, TCRF's Paralyzer): the current enemy's cue,
//     the caster's animation 2 to its end, flag 0x40 on the target;
//   - row 128, 0x43FC80 (id 0x80, TCRF's Head Cracker): the caster's
//     animation 2, a sound, then three rocks dropped on the target (the kind-1
//     child 0x43FE90, parameter 0x5D), each followed by a wait for the target
//     to leave state 6.
//
// Faithful, latent defects included (docs/magic_engine.md section 6): row 123
// reads a word through the current enemy's +0xF8 unchecked (only the
// event-battle set-ups write that pointer), and row 128's waits have no limit.
// Neither is fixed: a fix is a divergence for the owner to choose.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// stack table aborts where the original would call through its own stack
// (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_engine.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::EnemyOf;
using magic_harness::Mem;
using magic_harness::PartyOf;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The current enemy (docs/battle_flow.md): whichever enemy object
// BattleEnemy_RunAll ran last, not the caster. Its +0xF8 is a pointer to a
// table of u16 sound cues that only the event-battle set-ups write
// (0x437A55, 0x437BB5, ... : 0x64C7A0 and on).
constexpr std::uint32_t kCurrentEnemy = 0x939AD8;
constexpr unsigned kEnemyCues = 0xF8;
// Sound_PlayEffect(id) unless id is 0xFFFF - unnamed, no group's
// (battle_items_callees.h, enemy_ai_ops_callees.h).
constexpr std::uint32_t kPlayCue = bof3::addr::Sound_PlayEffectUnlessNone;  // BE3's since round twelve (battle_e3.cpp): the same value, so the fuzz keys stand
using PlayCueFn = void (__cdecl*)(unsigned);
// A bit per actor with a reaction to show (docs/battle_actions.md).
constexpr std::uint32_t kPending = 0x904B82;
// The task slots' owner cells: slot i's +0x80.
constexpr std::uint32_t kSlotOwners = at::kTasks + 0x80;

// The state byte +1 of an actor: 6 is the reaction state
// BattleAction_EffectWait sets on every actor flagged 0x40.
constexpr unsigned kState = 1;
constexpr unsigned char kReacting = 6;

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

// The effect-done bit: `or byte [0x904AA8], 4`.
void SetDone() { Mem(at::kFlags)[0] = static_cast<unsigned char>(Mem(at::kFlags)[0] | 4); }

// The task slot's owner (0x93B8C4's +0x80), read at the point of the call.
unsigned char* SlotOwner() { return Pointer(static_cast<std::uint32_t>(Long(Mem(at::kCurrentSlot))) + 0x80); }

// A stack-table dispatcher: the phase byte through the table the original
// builds on its stack, unchecked there; past the table ours aborts.
void Dispatch(const char* who, const std::uint32_t* phases, unsigned n, unsigned phase) {
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(phases[phase])();
}

}  // namespace

#define MENGINE_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Rows 0 and 126: the hold
// ===========================================================================

// original 0x4378D0 (Magic_Rows rows 0 and 126): phase +1 through a
// two-entry stack table - Task_StartHold60, MagicHold_Countdown.
MENGINE_EXPORT void __cdecl MagicHold_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Task_StartHold60, bof3::addr::MagicHold_Countdown};
    Dispatch("MagicHold_Task", kPhases, 2, Sprite_Current[1]);
}

// original 0x47FDA0: +9 = 60, the phase on. Shared: also entry 0 of the .data
// table EffectKind5F_States (0x654904) EffectKind5F_Run (0x47FD80) jumps through.
MENGINE_EXPORT void __cdecl Task_StartHold60(void) {
    Sprite_Current[9] = 0x3C;
    Bump(Sprite_Current[1]);
}

// original 0x437900: +9 down while it is not 0; at 0 flag 0x40 on the target
// (Battle_SetTargetFlag40), the effect-done bit, and the slot freed (a tail
// jmp).
MENGINE_EXPORT void __cdecl MagicHold_Countdown(void) {
    unsigned char* const sc = Sprite_Current;
    const unsigned char left = sc[9];
    if (left != 0) {
        sc[9] = static_cast<unsigned char>(left - 1);
        return;
    }
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    SetDone();
    MH_CALL(BattleTask_FreeCurrent)();
}

// ===========================================================================
// Row 108: Restore Form (read one id down)
// ===========================================================================

// original 0x4525F0 (Magic_Rows row 108): phase +1 through a two-entry stack
// table - RestoreForm_Start, RestoreForm_Wait.
MENGINE_EXPORT void __cdecl RestoreForm_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::RestoreForm_Start, bof3::addr::RestoreForm_Wait};
    Dispatch("RestoreForm_Task", kPhases, 2, Sprite_Current[1]);
}

// original 0x452620: the acting actor's (0x904B34) party record gets state 6,
// sub-states 4 and 4 and +4 = 0 - the member's own state-6 handler 0x441A10,
// entry 4 of its table (0x442080); the phase on.
//
// As the original has it: the actor indexes the party records (0x802D40 +
// 0x14C actor), unchecked - an enemy actor writes past the three members.
MENGINE_EXPORT void __cdecl RestoreForm_Start(void) {
    unsigned char* const member = PartyOf(Mem(at::kActor)[0]);
    member[1] = kReacting;
    member[2] = 4;
    member[3] = 4;
    member[4] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x452660: nothing while any actor is pending (the word 0x904B82);
// then the effect-done bit and the slot freed (a tail jmp).
MENGINE_EXPORT void __cdecl RestoreForm_Wait(void) {
    if (Word(Mem(kPending)) != 0) return;
    SetDone();
    MH_CALL(BattleTask_FreeCurrent)();
}

// ===========================================================================
// Row 123: Paralyzer (TCRF's name for id 0x8B)
// ===========================================================================

// original 0x43F3B0 (Magic_Rows row 123): phase +1 through a three-entry
// stack table - Paralyzer_Start, MagicFx_WaitOwnerAnim, MagicFx_FlagTargetEnd.
MENGINE_EXPORT void __cdecl Paralyzer_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Paralyzer_Start, bof3::addr::MagicFx_WaitOwnerAnim,
                                                 bof3::addr::MagicFx_FlagTargetEnd};
    Dispatch("Paralyzer_Task", kPhases, 3, Sprite_Current[1]);
}

// original 0x43F3E0: with Sprite_Current the slot's owner (0x93B8C4 +0x80, the
// caster), the cue (the first u16 of the current enemy's +0xF8 table) + 4
// through 0x437450 and Sprite_SetAnimation(2); Sprite_Current back, and its
// phase on.
//
// As the original has it - the latent defect of this row: the +0xF8 pointer
// is read unchecked. Only the event-battle set-ups write it, and every other
// reader tests the event-battle byte 0x904AAA first; here a pointer an
// ordinary battle never set is read through, and a null one faults at the
// read, in ours exactly where it does in Capcom's (docs/magic_engine.md
// section 6). The cue goes out as the low word, and 0x437450 reads no more.
MENGINE_EXPORT void __cdecl Paralyzer_Start(void) {
    unsigned char* const slot = Sprite_Current;
    unsigned char* const owner = SlotOwner();
    const unsigned char* const enemy = Pointer(kCurrentEnemy);
    Sprite_Current = owner;
    const unsigned char* const cues = Pointer(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(enemy)) + kEnemyCues);
    const unsigned cue = static_cast<unsigned>(Word(cues) + 4) & 0xFFFF;
    MH_AT(PlayCueFn, kPlayCue)(cue);
    MH_CALL(Sprite_SetAnimation)(2);
    Sprite_Current = slot;
    Bump(slot[1]);
}

// original 0x43F430 (Paralyzer_Task entry 1; also a stack-table entry of
// MAGIC081's, 0x4BF8EE): with Sprite_Current the slot's owner,
// Sprite_ScriptTickOnce; when it answers not 0 (al) the phase of the task
// (Sprite_Current as it was) on; Sprite_Current back.
MENGINE_EXPORT void __cdecl MagicFx_WaitOwnerAnim(void) {
    unsigned char* const owner = SlotOwner();
    unsigned char* const slot = Sprite_Current;
    Sprite_Current = owner;
    if (MH_CALL(Sprite_ScriptTickOnce)() != 0) Bump(slot[1]);
    Sprite_Current = slot;
}

// original 0x43F460 (Paralyzer_Task entry 2; also a stack-table entry of
// twelve overlays' tasks): flag 0x40 on the target, the effect-done bit, the
// slot freed (a tail jmp).
MENGINE_EXPORT void __cdecl MagicFx_FlagTargetEnd(void) {
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    SetDone();
    MH_CALL(BattleTask_FreeCurrent)();
}

// ===========================================================================
// Row 128: Head Cracker (TCRF's name for id 0x80)
// ===========================================================================

// original 0x43FC80 (Magic_Rows row 128): phase +1 through a ten-entry stack
// table - Start, Windup, WaitCaster, then Drop / WaitTarget three times, then
// MagicFx_DoneAndFree. (The original loads Drop's and WaitTarget's addresses
// into ecx / eax and stores each three times.)
MENGINE_EXPORT void __cdecl HeadCracker_Task(void) {
    static constexpr std::uint32_t kPhases[10] = {
        bof3::addr::HeadCracker_Start,      bof3::addr::HeadCracker_Windup,     bof3::addr::HeadCracker_WaitCaster,
        bof3::addr::HeadCracker_Drop,       bof3::addr::HeadCracker_WaitTarget, bof3::addr::HeadCracker_Drop,
        bof3::addr::HeadCracker_WaitTarget, bof3::addr::HeadCracker_Drop,       bof3::addr::HeadCracker_WaitTarget,
        bof3::addr::MagicFx_DoneAndFree};
    Dispatch("HeadCracker_Task", kPhases, 10, Sprite_Current[1]);
}

// original 0x43FCE0: with Sprite_Current the slot's owner (the caster),
// Sprite_SetAnimation(2); Sprite_Current back; the dword +0xC = 8; the phase
// on (Sprite_Current read again).
MENGINE_EXPORT void __cdecl HeadCracker_Start(void) {
    unsigned char* const owner = SlotOwner();
    unsigned char* const slot = Sprite_Current;
    Sprite_Current = owner;
    MH_CALL(Sprite_SetAnimation)(2);
    Sprite_Current = slot;
    SetLong(slot + 0xC, 8);
    Bump(Sprite_Current[1]);
}

// original 0x43FD20: the dword +0xC down; at 0 Sound_PlayEffect(0x601) and the
// phase on (Sprite_Current read again after the call). Every call: with
// Sprite_Current the slot's owner, Sprite_ScriptTickOnce, its answer unused;
// Sprite_Current back.
MENGINE_EXPORT void __cdecl HeadCracker_Windup(void) {
    SetLong(Sprite_Current + 0xC, Long(Sprite_Current + 0xC) - 1);
    if (Long(Sprite_Current + 0xC) == 0) {
        MH_CALL(Sound_PlayEffect)(0x601);
        Bump(Sprite_Current[1]);
    }
    unsigned char* const owner = SlotOwner();
    unsigned char* const slot = Sprite_Current;
    Sprite_Current = owner;
    MH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = slot;
}

// original 0x43FD80: with Sprite_Current the slot's owner,
// Sprite_ScriptTickOnce; when it answers not 0 (al), the task's (Sprite_Current
// as it was) children count +0xA = 0 and its phase on; Sprite_Current back.
//
// The wait has no limit: it ends when the caster's animation 2 reports its end
// (docs/magic_engine.md section 6).
MENGINE_EXPORT void __cdecl HeadCracker_WaitCaster(void) {
    unsigned char* const owner = SlotOwner();
    unsigned char* const slot = Sprite_Current;
    Sprite_Current = owner;
    if (MH_CALL(Sprite_ScriptTickOnce)() != 0) {
        slot[0xA] = 0;
        Bump(slot[1]);
    }
    Sprite_Current = slot;
}

// original 0x43FDC0 (Head Cracker entries 3, 5 and 7): a rock -
// BattleTask_Create(1, 0x5D) (HeadCrackerRock_Task); the new slot's owner +0x80
// = Sprite_Current (read after the call), whose children count +0xA goes up
// one; the phase on.
//
// As the original has it: the slot index is not checked. 0xFF (no slot free)
// writes the owner at 0x9423FC, past the image's end (0x93F000), and the count
// waits for a rock that never lands (docs/magic_engine.md section 6).
MENGINE_EXPORT void __cdecl HeadCracker_Drop(void) {
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x5D);
    unsigned char* const self = Sprite_Current;
    magic_harness::SetPointer(kSlotOwners + (slot & 0xFF) * at::kTaskStride, self);
    Bump(self[0xA]);
    Bump(Sprite_Current[1]);
}

// original 0x43FE00 (Head Cracker entries 4, 6 and 8): nothing while rocks are
// falling (+0xA not 0). Then a target out of the fight (Battle_ActorIsOut)
// sends the task to entry 9 (the end); otherwise nothing while the target's
// state byte +1 is 6 (a member's 0x802D41 + 0x14C t, an enemy's 0x93B961 +
// 0x128 (t - 3)), and then the phase on.
//
// As the original has it: the target is read again after Battle_ActorIsOut,
// and indexes the records unchecked. The wait has no limit - it ends when the
// target's own reaction moves its +1 off 6 (docs/magic_engine.md section 6).
MENGINE_EXPORT void __cdecl HeadCracker_WaitTarget(void) {
    if (Sprite_Current[0xA] != 0) return;
    if (MH_CALL(Battle_ActorIsOut)(Mem(at::kTarget)[0]) != 0) {
        Sprite_Current[1] = 9;
        return;
    }
    const unsigned char target = Mem(at::kTarget)[0];
    const unsigned char* const actor = target < 3 ? PartyOf(target) : EnemyOf(target);
    if (actor[kState] == kReacting) return;
    Bump(Sprite_Current[1]);
}

// original 0x43FE80 (Head Cracker entry 9; also a stack-table entry of about
// twenty overlays' tasks, e.g. Simoon's and Ragnarok's): the effect-done bit,
// the slot freed (a tail jmp).
MENGINE_EXPORT void __cdecl MagicFx_DoneAndFree(void) {
    SetDone();
    MH_CALL(BattleTask_FreeCurrent)();
}

// ===========================================================================
// The rock: BattleMagicFx_Dispatch's kind 1, parameter 0x5D
// ===========================================================================

// original 0x43FE90: phase +1 through a one-entry stack table -
// HeadCrackerRock_Run.
MENGINE_EXPORT void __cdecl HeadCrackerRock_Task(void) {
    static constexpr std::uint32_t kPhases[1] = {bof3::addr::HeadCrackerRock_Run};
    Dispatch("HeadCrackerRock_Task", kPhases, 1, Sprite_Current[1]);
}

// original 0x43FEB0: phase +2 through a three-entry stack table - Start,
// Fall, Land; then, when Sprite_Current (read again) has bit 0 at +0,
// Sprite_ScriptTick and Sprite_UpdateScreen.
MENGINE_EXPORT void __cdecl HeadCrackerRock_Run(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::HeadCrackerRock_Start, bof3::addr::HeadCrackerRock_Fall,
                                                 bof3::addr::HeadCrackerRock_Land};
    Dispatch("HeadCrackerRock_Run", kPhases, 3, Sprite_Current[2]);
    if ((Sprite_Current[0] & 1) == 0) return;
    MH_CALL(Sprite_ScriptTick)();
    MH_CALL(Sprite_UpdateScreen)();
}

// original 0x43FF00: Sprite_SetAnimationBank(0x1D3); x +0x34 and z +0x38 from
// the target's object (a member's +0x34 / +0x38, an enemy's), the target read
// again for the second; the height word +0x3E up 0x800 (from whatever the slot
// held); the speed +0x14 = 0, its step +0x20 = -16; +0x5C..+0x5F, +0x24,
// +0x48 and +0x2A = 0, +0x29 = 2; Sprite_SetAnimation(3); the sub-phase +2 on
// (Sprite_Current read again).
MENGINE_EXPORT void __cdecl HeadCrackerRock_Start(void) {
    MH_CALL(Sprite_SetAnimationBank)(0x1D3);
    const unsigned char t = Mem(at::kTarget)[0];
    unsigned char* const sc = Sprite_Current;
    if (t <= 2) {
        SetLong(sc + 0x34, Long(PartyOf(t) + 0x34));
        SetLong(sc + 0x38, Long(PartyOf(Mem(at::kTarget)[0]) + 0x38));
    } else {
        SetLong(sc + 0x34, Long(EnemyOf(t) + 0x34));
        SetLong(sc + 0x38, Long(EnemyOf(Mem(at::kTarget)[0]) + 0x38));
    }
    SetWord(sc + 0x3E, Word(sc + 0x3E) + 0x800);
    SetLong(sc + 0x14, 0);
    SetLong(sc + 0x20, -16);
    sc[0x5D] = 0;
    sc[0x5E] = 0;
    sc[0x5F] = 0;
    sc[0x5C] = 0;
    sc[0x24] = 0;
    sc[0x48] = 0;
    sc[0x2A] = 0;
    sc[0x29] = 2;
    MH_CALL(Sprite_SetAnimation)(3);
    Bump(Sprite_Current[2]);
}

// original 0x440010: the height word +0x3E moves by the speed's low word, the
// speed +0x14 by its step +0x20; then the ground AreaMap_Elevation(x +0x34, z
// +0x38) - its low word, signed - plus 0x180; when the height (signed, of
// Sprite_Current read again) is below that, the sub-phase +2 on.
MENGINE_EXPORT void __cdecl HeadCrackerRock_Fall(void) {
    unsigned char* const sc = Sprite_Current;
    SetWord(sc + 0x3E, Word(sc + 0x3E) + Word(sc + 0x14));
    SetLong(sc + 0x14, Long(sc + 0x14) + Long(sc + 0x20));
    const long ground = MH_CALL(AreaMap_Elevation)(Long(sc + 0x34), Long(sc + 0x38));
    const int limit = static_cast<short>(static_cast<std::uint32_t>(ground) & 0xFFFF) + 0x180;
    unsigned char* const now = Sprite_Current;
    if (static_cast<short>(Word(now + 0x3E)) < limit) Bump(now[2]);
}

// original 0x440060: flag 0x40 on the target; the owner's (0x93B940, read
// after the call: the Head Cracker task) children count +0xA down one; the
// slot freed (a tail jmp).
MENGINE_EXPORT void __cdecl HeadCrackerRock_Land(void) {
    MH_CALL(Battle_SetTargetFlag40)(Mem(at::kTarget)[0]);
    unsigned char* const owner = Pointer(at::kOwner);
    owner[0xA] = static_cast<unsigned char>(owner[0xA] - 1);
    MH_CALL(BattleTask_FreeCurrent)();
}

void MagicEngine_Inject() {
    if (bof3::WantsShadow("magic_engine")) magic_engine::SelfTest();
    BOF3_INJECT(MagicHold_Task);
    BOF3_INJECT(Task_StartHold60);
    BOF3_INJECT(MagicHold_Countdown);
    BOF3_INJECT(RestoreForm_Task);
    BOF3_INJECT(RestoreForm_Start);
    BOF3_INJECT(RestoreForm_Wait);
    BOF3_INJECT(Paralyzer_Task);
    BOF3_INJECT(Paralyzer_Start);
    BOF3_INJECT(MagicFx_WaitOwnerAnim);
    BOF3_INJECT(MagicFx_FlagTargetEnd);
    BOF3_INJECT(HeadCracker_Task);
    BOF3_INJECT(HeadCracker_Start);
    BOF3_INJECT(HeadCracker_Windup);
    BOF3_INJECT(HeadCracker_WaitCaster);
    BOF3_INJECT(HeadCracker_Drop);
    BOF3_INJECT(HeadCracker_WaitTarget);
    BOF3_INJECT(MagicFx_DoneAndFree);
    BOF3_INJECT(HeadCrackerRock_Task);
    BOF3_INJECT(HeadCrackerRock_Run);
    BOF3_INJECT(HeadCrackerRock_Start);
    BOF3_INJECT(HeadCrackerRock_Fall);
    BOF3_INJECT(HeadCrackerRock_Land);
}
