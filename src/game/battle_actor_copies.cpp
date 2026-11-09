// Group CH of the eighth takeover round (docs/battle_actor_copies.md): the
// sixteen functions pe_funcs.py folded into Battle_SpawnActorCopies 0x446FF0
// and ItemMenu_CanUseSelected 0x447840 - two entries of the battle's phase-1
// table 0x64AE28 (11 and 12, reached from 0x42E990 by a tail jump) and the
// states below them: the target choice of a command, and the battle's item
// window with the item's target choice. Each was read to its last
// instruction with capstone against bof3/BOF3.exe on 2026-09-25. Faithful:
// no divergence. The start-up fuzz is battle_actor_copies_fuzz.cpp.
//
//   BattleTarget_Dispatch     0x447110..0x44711D (0xE)    phase-1 entry 11: BattleTarget_Steps by 0x904AA3
//   BattleTarget_ShowPrompt   0x447120..0x447138 (0x19)   step 0
//   BattleTarget_PickDispatch 0x447140..0x447150 (0x11)   step 1: BattleTarget_Picks by 0x904AA4
//   BattleTarget_Begin        0x447160..0x44718E (0x2F)   pick 0
//   BattleTarget_PickEnemy    0x447190..0x447281 (0xF2)   pick 1
//   BattleTarget_Confirm      0x447390..0x4473DC (0x4D)   pick 3
//   BattleItem_Dispatch       0x447430..0x44743D (0xE)    phase-1 entry 12: BattleItem_Steps by 0x904AA3
//   BattleItem_OpenDispatch   0x447440..0x447450 (0x11)   step 0: BattleItem_OpenSteps by 0x904AA4
//   BattleItem_OpenWindow     0x447460..0x4474AA (0x4B)   step 0, sub 1
//   BattleItem_Browse         0x4474B0..0x447836 (0x387)  step 1
//   BattleItem_Choose         0x4478B0..0x44793F (0x90)   step 3
//   BattleItem_TargetDispatch 0x447940..0x447950 (0x11)   step 4: BattleItem_TargetSteps by 0x904AA4
//   BattleItem_TargetBegin    0x447960..0x4479DE (0x7F)   pick 0
//   BattleItem_PickEnemy      0x4479E0..0x447B05 (0x126)  pick 1
//   BattleItem_PickParty      0x447B10..0x447CBB (0x1AC)  pick 2
//   BattleItem_Confirm        0x447CC0..0x447D28 (0x69)   pick 3, and step 5's pick 2
//
// Every function is `void (void)`: the stubs are entered by a tail jump, and
// the chain's first call is Battle_PhaseDispatch's (ours), which discards eax.
// Every call goes through battle_actor_copies::g, so that the fuzz can stand
// recorders in for them. The five stubs read their table afresh from .data.
#include "game/battle_actor_copies.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_actor_copies_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace battle_actor_copies {

namespace {
template <typename T> T Fn(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
using F0 = U (__cdecl*)();
using F1 = U (__cdecl*)(U);
using F2 = U (__cdecl*)(U, U);
using F3 = U (__cdecl*)(U, U, U);
}  // namespace

const Callees kOriginals = {
    As<F2>(BattleBanner_SetMessage),
    As<F1>(Battle_DefaultTarget),
    Fn<F1>(::bof3::Code(kPrevTarget.v)),
    As<F3>(Battle_WrapIndex),
    As<F1>(Input_AutoRepeat),
    As<F1>(Sound_PlayEffect),
    As<F0>(ItemMenu_SetupForActor),
    As<F3>(Char_AbilityList),
    As<F1>(Msg_SystemPtr),
    As<F0>(ItemMenu_CanUseSelected),
    As<F0>(Battle_ReturnTrue),
    As<F0>(ItemMenu_FreeWindows),
};
Callees g = kOriginals;

}  // namespace battle_actor_copies

using namespace battle_actor_copies;

namespace {

// `mov al, [step]; jmp [eax*4 + table]`: entry `index` of a .data table,
// read afresh. As the original has it, the index is not checked - one past
// the end reads the next table's first entry (the tables sit back to back),
// so ours reads exactly what the original's jmp would.
using Handler = void (__cdecl*)();
void Dispatch(const unsigned long* table, unsigned index) { reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))(); }

}  // namespace

// ===========================================================================
// The target choice: phase-1 entry 11
// ===========================================================================

// ===========================================================================
// The item window: phase-1 entry 12
// ===========================================================================

void BattleActorCopies_Inject() {
    if (bof3::WantsShadow("battle_actor_copies")) battle_actor_copies::SelfTest();
}
