// The battle party objects' state handlers (originals 0x441200..0x4419D0 and
// 0x4429E0..0x442E31): the handlers BattleObj_RunState reaches through the
// state table 0x64DFE0 by a party object's state byte +1, and the sub-state
// handlers states 3, 4, 5, 7, 8 and 12 reach through the tables after it.
// docs/battle_obj_states.md.
#pragma once

void BattleObjStates_Inject();
