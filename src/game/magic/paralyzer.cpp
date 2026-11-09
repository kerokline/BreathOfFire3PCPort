// Moved here by tools/move_funcs.py from magic_engine.cpp (docs/source-layout-round.md).
#include "game/magic/paralyzer.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_engine.h"
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

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::Word;

// The current enemy (docs/battle_flow.md): whichever enemy object
// BattleEnemy_RunAll ran last, not the caster. Its +0xF8 is a pointer to a
// table of u16 sound cues that only the event-battle set-ups write
// (0x437A55, 0x437BB5, ... : 0x64C7A0 and on).
// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml).
constexpr unsigned kEnemyCues = 0xF8;

// Sound_PlayEffect(id) unless id is 0xFFFF - unnamed, no group's
// (battle_items_callees.h, enemy_ai_ops_callees.h).
constexpr ::bof3::image::CodeVa kPlayCue{bof3::addr::Sound_PlayEffectUnlessNone};  // BE3's since round twelve (battle_e3.cpp): the same value, so the fuzz keys stand

using PlayCueFn = void (__cdecl*)(unsigned);

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

// original 0x43F3B0 (Magic_Rows row 123): phase +1 through a three-entry
// stack table - Paralyzer_Start, MagicFx_WaitOwnerAnim, MagicFx_FlagTargetEnd.
MENGINE_EXPORT void __cdecl Paralyzer_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Paralyzer_Start, bof3::addr::MagicFx_WaitOwnerAnim,
                                                 bof3::addr::MagicFx_FlagTargetEnd};
    DispatchMagicEngine("Paralyzer_Task", kPhases, 3, Sprite_Current[1]);
}

// original 0x43F3E0: with Sprite_Current the slot's owner (0x93B8C4 +0x80, the
// caster), the cue (the first u16 of the current enemy's +0xF8 table) + 4
// through 0x437450 and Sprite_SetAnimation(2); Sprite_Current back, and its
// phase on.
//
// DIVERGENCE DIV-0183: the latent defect of this row (D102, docs/magic_engine.md
// section 6 E1). The original reads the +0xF8 pointer unchecked. Only the
// event-battle set-ups write it, and every other reader tests the event-battle
// byte 0x904AAA first; here a pointer an ordinary battle never set is read
// through, and a null one faults at the read - the crash the spell sweep met
// 2026-10-26 (docs/spell-sweep.md section 5) from the combat route's Gooey.
// Ours: a null current-enemy pointer, or a null cue pointer in it, plays no
// cue - the one thing the word feeds is 0x437450, Sound_PlayEffect unless
// 0xFFFF, and nothing later in the task reads it - and the rest of the step
// runs as written: animation 2 on the caster, the phase on, so the skill
// resolves through MagicFx_WaitOwnerAnim and MagicFx_FlagTargetEnd. A written
// pointer takes the original's path unchanged: the cue goes out as the low
// word, and 0x437450 reads no more. The fuzz seeds a written pointer every
// round (Capcom's clone would fault on a null one), so it compares the
// original's path alone; the null path is checked live by the sweep.
// BOF3X_ORIGINAL=Paralyzer_Start leaves Capcom's function, its read with it.
MENGINE_EXPORT void __cdecl Paralyzer_Start(void) {
    unsigned char* const slot = Sprite_Current;
    unsigned char* const owner = SlotOwner();
    const unsigned char* const enemy = Pointer(::bof3::Img(BattleE3_CurrentEnemy_addr));
    Sprite_Current = owner;
    const unsigned char* const cues =
        enemy != nullptr ? Pointer(Low32(enemy) + kEnemyCues) : nullptr;
    if (cues != nullptr) {   // DIV-0183: no table, no cue
        const unsigned cue = static_cast<unsigned>(Word(cues) + 4) & 0xFFFF;
        MH_AT(PlayCueFn, kPlayCue.v)(cue);
    } else {
        bof3::Log("DIV-0183    Paralyzer_Start: no cue table on the current enemy (%p), the cue skipped", enemy);
    }
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
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    SetDone();
    MH_CALL(BattleTask_FreeCurrent)();
}

void Paralyzer_Inject() {
    BOF3_INJECT(Paralyzer_Task);
    BOF3_INJECT(Paralyzer_Start);
    BOF3_INJECT(MagicFx_WaitOwnerAnim);
    BOF3_INJECT(MagicFx_FlagTargetEnd);
}
