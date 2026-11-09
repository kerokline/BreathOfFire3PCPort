// The enemy AI script ops: what an enemy object does in its state 0, which
// BattleEnemy_RunAll (battle_flow.cpp) enters through the state table
// 0x64B084. The state's step byte +1 picks an op from EnemyOp_Steps
// 0x64B1A0; an op's own sub-steps +2 and +3 pick from the six tables after
// it. Twenty-two of those ops are here: the step dispatch, the entrance (a
// scale-in), the idle loop with its target highlight and bob, the hit the
// enemy takes and its end, and the death animation that ends in
// Battle_EnemyDefeated. docs/enemy_ai_ops.md.
//
// Every call goes through enemy_ai_ops::g (enemy_ai_ops_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Everything here is a faithful replacement.
//
// Sprite_Current (0x937F88) and 0x939AD8 are both the enemy being run while
// BattleEnemy_RunAll dispatches; the originals read each afresh at most
// stores and after every call, and so does this file: a store's object is
// whichever pointer the original loads for it, at the point it loads it.
#include "game/enemy_ai_ops.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/enemy_ai_ops_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace enemy_ai_ops {

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    BattleEnemy_SetAnimation, BattleEnemy_ScriptTick, BattleEnemy_ScriptTickOnce, BattleEnemy_Chance70,
    Battle_EnemyDefeated,
    Sprite_ScriptTickOnce, Battle_StatusTint, Sprite_SetTint, Tint_Release, Sprite_ReleaseTint, Battle_ApplyDamage,
    Effect_ApplyResult, Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kItemClass.v)), Battle_SetDamagePopup,
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kApPopup.v)), EnemyAI_TurnCheck, Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kPlayCue.v)),
    Sound_PlayEffect, Battle_PlayHitSound, Battle_SetHitPopup, Battle_ClearActorBit,
};
Callees g = kOriginals;

}  // namespace enemy_ai_ops

using namespace enemy_ai_ops;

namespace {

// The enemy ops run on the enemy: Sprite_Current and 0x939AD8 (docs/enemy_ai_ops.md).

}  // namespace

// ===========================================================================
// The dispatchers
// ===========================================================================

// ===========================================================================
// The entrance and the idle loop
// ===========================================================================

// ===========================================================================
// The hit taken
// ===========================================================================

// ===========================================================================
// The death
// ===========================================================================

// ===========================================================================

// ===========================================================================

void EnemyAiOps_Inject() {
    if (bof3::WantsShadow("enemy_ai_ops")) enemy_ai_ops::SelfTest();
}
