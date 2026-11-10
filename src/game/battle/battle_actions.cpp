// The action phases: the battle's phase 3 (0x42F220, the fourth entry of
// Battle_PhaseDispatch's table) and the seven step tables under it. An
// action goes: the next actor out of the turn order (Battle_BeginAction), its
// command kind (0x904B35) dispatched - kinds 0 and 2, kind 1, the ability
// (kind 4) and the item (kind 5) each with their checks, and kind 3 through
// BattleAction_Kind3Dispatch 0x42F5E0 (round twelve BE1's) - then the effect steps, the steps after it
// (the enemy messages among them), and the end, where a set 0x40 in the round
// flags swaps the actor and the target and runs one more action. What the
// kinds are in game terms is not read here. docs/battle_actions.md.
//
// Every call goes through battle_actions::g (battle_actions_callees.h), so
// that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. The step tables are read in .data, afresh at
// each dispatch, as the originals read them. Everything here is a faithful
// replacement, but for DIV-0179's opt-in banner colour (UnlearnedBlue).
#include "game/battle_actions.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_actions_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/psp_code.h"
#include "game/sound.h"
#include "game/sound_st.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_actions {

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace battle_actions

using namespace battle_actions;

// ===========================================================================
// Phase 3 and the step dispatches
// ===========================================================================

// ===========================================================================
// Step 0: the next actor
// ===========================================================================

// ===========================================================================
// Step 1: the command kinds
// ===========================================================================

// ===========================================================================
// Kind 4: the ability
// ===========================================================================

// ===========================================================================
// Kind 5: the item
// ===========================================================================

// ===========================================================================
// Step 2: the effects
// ===========================================================================

// ===========================================================================
// Step 3: after the effects
// ===========================================================================

// ===========================================================================
// Step 4: the action's end
// ===========================================================================

// ===========================================================================

void BattleActions_Inject() {
    if (bof3::WantsShadow("battle_actions")) battle_actions::SelfTest();
}
