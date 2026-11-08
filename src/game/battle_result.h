// The battle result (originals 0x431940..0x432163 and 0x598570..0x598749):
// the PSX BATL_END.EMI's code compiled into the exe - the EXP split and its
// tick, the level-up search, the zenny, the drops' sort and award, the two
// dispatch steps under the result phase, and the EXP and zenny windows'
// states and draws under the window-kind handler 0x597F60.
// docs/battle_result.md.
#pragma once

void BattleResult_Inject();
extern unsigned char g_exp_layout_us;   // defined in battle_result.cpp, armed by BattleResult_Inject (named here for a moved reader: source-layout B0)
