// Round twelve group BE6 (docs/takeover-queue-field-battle.md section 3): the
// battle engine's transformation (the chosen genes to a form: a recipe or a
// mix, the stats, the abilities, the history), the gene cost, three battle
// effect tasks (BattleFx_Dispatch slots 15, 16 and 18), the stat rebuild from
// the buffs, the member roll and its two tests, the AP pop-up, the Field_Slots
// release and start, and BMAGIC's four map-cell handlers - 39 functions of
// 0x451480..0x4552F6 and 0x4CEB40..0x4CF4A4, through the boss harness's
// engine frame (boss_harness.h). docs/battle_e6.md.
#pragma once

void BattleE6_Inject();

namespace battle_e6 {
// BOF3X_SHADOW=battle_e6: the start-up fuzz, battle_e6_fuzz.cpp - one
// boss_harness::Run per unit (six). Clones every original before
// BattleE6_Inject patches it.
void SelfTest();
}  // namespace battle_e6
