// The battle windows' states: record handler 3 of Field_RunTaskRecords and
// the kinds and states under it - the party row sliding in, the command cross
// growing, the command label, the action banner, the enemy and member gauges
// and their target banners (the eighth round's group CL,
// docs/battle_win_states.md).
#pragma once

void BattleWinStates_Inject();

extern unsigned char g_banner_centre;   // defined in battle_win_states.cpp (named here for a moved reader: source-layout B6)
