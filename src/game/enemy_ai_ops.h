// The enemy AI script ops (originals 0x4360F0..0x436B4B, 0x436D90..0x436EF1,
// 0x437420): the handlers an enemy object's state 0 runs through the op
// tables 0x64B1A0.. by its step bytes +1 / +2 / +3 - the entrance, the idle
// and target-highlight loop, the hit it takes and what follows, and the death
// animation that ends in Battle_EnemyDefeated. docs/enemy_ai_ops.md.
#pragma once

void EnemyAiOps_Inject();
