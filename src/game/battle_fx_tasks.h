// The battle effect tasks (originals 0x432B70..0x433969, 0x4352A0..0x4357C5,
// 0x437720..0x43777A, 0x4378B0..0x4378CC): the battle-task kinds 0, 1 and 2
// that BattleTask_RunAll dispatches, and under kind 0 the damage-number popup,
// the pose task, the actor watch and the owner-following sprite; plus the two
// round hooks an enemy state stores at 0x904B64 / 0x904B68.
// docs/battle_fx_tasks.md.
#pragma once

void BattleFxTasks_Inject();
