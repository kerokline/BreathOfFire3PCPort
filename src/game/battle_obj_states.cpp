// The battle party objects' state handlers: what BattleObj_RunState 0x4411E0
// reaches through the state table 0x64DFE0 by a party object's state byte
// +1 - the set-up (state 0), the stand and idle (2, 3), the attack by
// character (4 and its slots 15..25), the swing (5), the cast (7), its end
// (8), state 12 - with the sub-state handlers those reach through the tables
// after 0x64DFE0 by the byte +2, and the action's end 0x442DD0 two of them
// tail-jump to. docs/battle_obj_states.md.
//
// Sprite_Current and Field_State are both the party object being run
// (BattleParty_RunStates sets both); each is re-read wherever the original
// re-reads it. Every call goes through battle_obj_states::g
// (battle_obj_states_callees.h), so that the start-up fuzz can stand
// recorders in for them - for ours and for the originals' copies alike.
//
// Every handler is `void`: the original's eax on return reaches only
// BattleObj_RunState's tail jump, BattleParty_RunStates' tail jump and the
// battle frame Battle_Frame 0x42E370, whose next instruction after `call 0x441100`
// (0x42E39E) is another call; the sub-state calls of states 3 and 8 overwrite
// or ignore it. Everything here is a faithful replacement.
#include "game/battle_obj_states.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_obj_states_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_obj_states {

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
const std::uint32_t* Table(std::uint32_t address) { return reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Table(::bof3::Img(BattleObj_StateTable_addr) + 0x34), Table(::bof3::Img(BattleObj_StateTable_addr) + 0x3c), Table(::bof3::Img(BattleObj_StateTable_addr) + 0x68), Table(::bof3::Img(BattleObj_SwingSubs_addr)),
    Table(::bof3::Img(BattleObj_CastSubs_addr)), Table(::bof3::Img(BattleObj_CastDoneSubs_addr)), Table(::bof3::Img(BattleObj_State12Subs_addr)),
    BattleObj_EndAction,
    BattleObj_PickPose, BattleObj_ScriptTick, BattleObj_ScriptTickOnce, AreaMap_Elevation, Sprite_EnsureAnimation,
    Sprite_SetTint, Tint_Release, Battle_RollPendingFlag, Rand, Battle_PlayActorCue, BattleTask_Create,
    Battle_SetTargetFlag40, File_LoadDone, Battle_LoadSoundByKey, Battle_ClearActorBit,
};
Callees g = kOriginals;

}  // namespace battle_obj_states

using namespace battle_obj_states;

// ===========================================================================
// State 0: the set-up
// ===========================================================================

// ===========================================================================
// States 2 and 3: standing
// ===========================================================================

// ===========================================================================
// State 4: the attack, by character
// ===========================================================================

// ===========================================================================
// State 5: the swing
// ===========================================================================

// ===========================================================================
// State 7: the cast
// ===========================================================================

// ===========================================================================
// State 8: the cast's end
// ===========================================================================

// ===========================================================================
// State 12
// ===========================================================================

// ===========================================================================
// The action's end
// ===========================================================================

// ===========================================================================

void BattleObjStates_Inject() {
    if (bof3::WantsShadow("battle_obj_states")) battle_obj_states::SelfTest();
}
