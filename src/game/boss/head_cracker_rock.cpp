// Moved here by tools/move_funcs.py from magic_engine.cpp (docs/source-layout-round.md).
#include "game/boss/head_cracker_rock.h"

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

using magic_harness::EnemyOf;

using magic_harness::Mem;

using magic_harness::PartyOf;

using magic_harness::Pointer;

using move_script::Long;

using move_script::Word;

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

// A stack-table dispatcher: the phase byte through the table the original
// builds on its stack, unchecked there; past the table ours aborts.
void DispatchMagicEngine(const char* who, const std::uint32_t* phases, unsigned n, unsigned phase) {
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(phases[phase])();
}

}  // namespace

#define MENGINE_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x43FE90: phase +1 through a one-entry stack table -
// HeadCrackerRock_Run.
MENGINE_EXPORT void __cdecl HeadCrackerRock_Task(void) {
    static constexpr std::uint32_t kPhases[1] = {bof3::addr::HeadCrackerRock_Run};
    DispatchMagicEngine("HeadCrackerRock_Task", kPhases, 1, Sprite_Current[1]);
}

// original 0x43FEB0: phase +2 through a three-entry stack table - Start,
// Fall, Land; then, when Sprite_Current (read again) has bit 0 at +0,
// Sprite_ScriptTick and Sprite_UpdateScreen.
MENGINE_EXPORT void __cdecl HeadCrackerRock_Run(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::HeadCrackerRock_Start, bof3::addr::HeadCrackerRock_Fall,
                                                 bof3::addr::HeadCrackerRock_Land};
    DispatchMagicEngine("HeadCrackerRock_Run", kPhases, 3, Sprite_Current[2]);
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
    const unsigned char t = Mem(::bof3::Img(at::kTarget))[0];
    unsigned char* const sc = Sprite_Current;
    records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
    if (t <= 2) {
        tsc->x = Long(PartyOf(t) + 0x34);
        tsc->z = Long(PartyOf(Mem(::bof3::Img(at::kTarget))[0]) + 0x38);
    } else {
        tsc->x = Long(EnemyOf(t) + 0x34);
        tsc->z = Long(EnemyOf(Mem(::bof3::Img(at::kTarget))[0]) + 0x38);
    }
    rv::Store<unsigned>(tsc->height_whole, tsc->height_whole + 0x800);
    rv::Store(tsc->step_y, 0);
    rv::Store(tsc->work_20, -16);
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
    records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
    rv::Store<unsigned>(tsc->height_whole, tsc->height_whole + Word(sc + 0x14));
    rv::Store(tsc->step_y, tsc->step_y + tsc->work_20);
    const long ground = MH_CALL(AreaMap_Elevation)(tsc->x, tsc->z);
    const int limit = static_cast<short>(static_cast<std::uint32_t>(ground) & 0xFFFF) + 0x180;
    unsigned char* const now = Sprite_Current;
    if (static_cast<short>(rv::As<records::BattleTask>(now)->height_whole) < limit) Bump(now[2]);
}

// original 0x440060: flag 0x40 on the target; the owner's (0x93B940, read
// after the call: the Head Cracker task) children count +0xA down one; the
// slot freed (a tail jmp).
MENGINE_EXPORT void __cdecl HeadCrackerRock_Land(void) {
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    unsigned char* const owner = Pointer(::bof3::Img(BattleE2_Owner_addr));
    owner[0xA] = static_cast<unsigned char>(owner[0xA] - 1);
    MH_CALL(BattleTask_FreeCurrent)();
}

void HeadCrackerRock_Inject() {
    BOF3_INJECT(HeadCrackerRock_Task);
    BOF3_INJECT(HeadCrackerRock_Run);
    BOF3_INJECT(HeadCrackerRock_Start);
    BOF3_INJECT(HeadCrackerRock_Fall);
    BOF3_INJECT(HeadCrackerRock_Land);
}
