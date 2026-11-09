// The battle menu states: two commands' state machines, reached from the
// command step 0x42EED0, which tail-jumps through 0x64AE54 by the command byte
// 0x904AA2 (Battle_PhaseDispatch -> 0x42E990 -> 0x42EED0 -> here, every link
// a tail jump, so each function below returns to Battle_PhaseDispatch and
// takes nothing). Command 3 (0x447FD0 and 0x64E44C) picks an enemy and
// commits action kind 0 - the plain attack, by that (a hypothesis in the
// names); command 2 (0x448180 and 0x64E45C) is the item command: the prompt,
// the party's item list, the item's target kind, the target picks and the
// commit. docs/battle_menu_states.md.
//
// Every call goes through battle_menu_states::g (battle_menu_states_callees.h),
// so that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Everything here is a faithful replacement.
#include "game/battle_menu_states.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_menu_states_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace battle_menu_states {

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Battle_DefaultTarget,
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(kPrevTarget.v)),
    Battle_WrapIndex,
    BattleBanner_SetMessage,
    Input_AutoRepeat,
    Sound_PlayEffect,
    Msg_SystemPtr,
    BattleQueue_Push,
    ItemMenu_SetupForParty,
    ItemMenu_FreeWindows,
    Item_HelpMessage,
    Item_CanUse,
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kItemFlags.v)),
    Battle_ReturnTrue,
};
Callees g = kOriginals;

}  // namespace battle_menu_states

using namespace battle_menu_states;

// ===========================================================================
// Command 3: pick an enemy, commit action kind 0
// ===========================================================================

// ===========================================================================
// Command 2: the item command
// ===========================================================================

// ===========================================================================

void BattleMenuStates_Inject() {
    if (bof3::WantsShadow("battle_menu_states")) battle_menu_states::SelfTest();
}
