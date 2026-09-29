// Round twelve group BE5 (docs/takeover-queue-field-battle.md section 3): the
// battle engine's band 0x44B240..0x451480 - the enemy AI's row helpers (a row
// applied, marked, tested; the queue deduplicated), three Effect_Handlers
// slots (the HP and AP drains, the quarter-chance attack), the
// transformation's stat copy into the party, and the Dragon command's run
// through its seven parts' step tables (the load, the menu, the two slot
// lists, the gene grid, the store). 52 functions (47 of the cut less the case
// 0x44B8D0, and the six the cut did not list), through the boss harness's
// engine frame (boss_harness.h). docs/battle_e5.md.
#pragma once

void BattleE5_Inject();

namespace battle_e5 {
// BOF3X_SHADOW=battle_e5: the start-up fuzz, battle_e5_fuzz.cpp - one
// boss_harness::Run over the group, Group::engine set. Clones every original
// before BattleE5_Inject patches it.
void SelfTest();
}  // namespace battle_e5
