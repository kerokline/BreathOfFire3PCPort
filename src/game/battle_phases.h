// The battle's frame and the phase table's first half (originals
// 0x42E370..0x42F213): the battle mode's frame, phase 0 - the battle's
// start (Battle_Init, the intro windows) - phase 1 - the command input (the
// round start, the next member, the command cross, the confirm dispatch,
// Defend) - and phase 2 - the commit (the turn order, the enemy messages,
// the wait). docs/battle_phases.md.
#pragma once

void BattlePhases_Inject();
