// Moved here by tools/move_funcs.py from magic_engine.cpp (docs/source-layout-round.md).
#include "game/boss/head_cracker.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::EnemyOf;

using magic_harness::Mem;

using magic_harness::PartyOf;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

// A bit per actor with a reaction to show (docs/battle_actions.md).
// kPending: now BattleActions_Pending (symbols.toml).
// The task slots' owner cells: slot i's +0x80.
constexpr ::bof3::image::Va kSlotOwners{BattleTask_Slots_addr + 0x80};

// The state byte +1 of an actor: 6 is the reaction state
// BattleAction_EffectWait sets on every actor flagged 0x40.
constexpr unsigned kState = 1;

constexpr unsigned char kReacting = 6;

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

// The effect-done bit: `or byte [0x904AA8], 4`.
void SetDone() { Mem(::bof3::Img(magic_harness::at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(magic_harness::at::kFlags))[0] | 4); }

// The task slot's owner (0x93B8C4's +0x80), read at the point of the call.
unsigned char* SlotOwner() { return Pointer(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(BattleFlow_TaskCurrent_addr)))) + 0x80); }

// A stack-table dispatcher: the phase byte through the table the original
// builds on its stack, unchecked there; past the table ours aborts.
void DispatchMagicEngine(const char* who, const std::uint32_t* phases, unsigned n, unsigned phase) {
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(phases[phase])();
}

}  // namespace

#define MENGINE_EXPORT extern "C" __attribute__((disable_tail_calls))

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
    DispatchMagicEngine("HeadCracker_Task", kPhases, 10, Sprite_Current[1]);
}

// original 0x43FCE0: with Sprite_Current the slot's owner (the caster),
// Sprite_SetAnimation(2); Sprite_Current back; the dword +0xC = 8; the phase
// on (Sprite_Current read again).
MENGINE_EXPORT void __cdecl HeadCracker_Start(void) {
    unsigned char* const owner = SlotOwner();
    unsigned char* const slot = Sprite_Current;
    records::BattleTask* const tslot = rv::As<records::BattleTask>(slot);
    Sprite_Current = owner;
    MH_CALL(Sprite_SetAnimation)(2);
    Sprite_Current = slot;
    tslot->step_x = 8;
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
    magic_harness::SetPointer(::bof3::Img(kSlotOwners.v) + (slot & 0xFF) * at::kTaskStride, self);
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
    if (MH_CALL(Battle_ActorIsOut)(Mem(::bof3::Img(at::kTarget))[0]) != 0) {
        Sprite_Current[1] = 9;
        return;
    }
    const unsigned char target = Mem(::bof3::Img(at::kTarget))[0];
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

void HeadCracker_Inject() {
    BOF3_INJECT(HeadCracker_Task);
    BOF3_INJECT(HeadCracker_Start);
    BOF3_INJECT(HeadCracker_Windup);
    BOF3_INJECT(HeadCracker_WaitCaster);
    BOF3_INJECT(HeadCracker_Drop);
    BOF3_INJECT(HeadCracker_WaitTarget);
    BOF3_INJECT(MagicFx_DoneAndFree);
}
