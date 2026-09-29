// Round twelve group BE2 (docs/takeover-queue-field-battle.md section 3): the
// battle engine's 0x433650..0x437030 - the effect tasks of BattleFx_Dispatch's
// slots 9, 10, 13, 14 and 17 and the actor watch's state 3, the party restored
// from its backup records, the enemy's action pick and its target helpers
// (the action's begin, 0x435AB0), and the enemy ops of EnemyOp_Steps 4 and 5
// and EnemyOp_ActSubs 3 and 5. 48 functions through the boss harness as an
// engine group (boss_harness.h, docs/boss_harness.md section 10).
// docs/battle_e2.md.
#pragma once

void BattleE2_Inject();

namespace battle_e2 {
// BOF3X_SHADOW=battle_e2: the start-up fuzz, battle_e2_fuzz.cpp - one
// boss_harness::Run per family. Clones every original before
// BattleE2_Inject patches it.
void SelfTest();
}  // namespace battle_e2
