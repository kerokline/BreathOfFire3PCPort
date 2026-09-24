// Damage, effects and affinities: group BE of the seventh round - the damage
// chain Battle_ApplyDamage 0x445A30 -> Battle_CalcDamage 0x445CF0 ->
// Battle_BaseDamage 0x4462B0 -> Battle_ScaleDamage 0x446430, the effect
// result Effect_ApplyResult 0x44B9F0 with the two amounts its handlers use
// (0x44ED10, 0x44F130) and the element affinity 0x44EE80; the turn order
// around them (0x444F40, 0x4450E0 and their helpers 0x445640 .. 0x445980,
// 0x446650) and the enemies' AI conditions 0x44AE90 / 0x44B2C0. The PSX's
// BATTLE.EMI overlays #3 and #15. docs/battle_damage.md.
#pragma once

void BattleDamage_Inject();
