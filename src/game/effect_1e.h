// Round thirteen group E1E: the leader's state 9 - LeaderPanel_Stages' stages
// 2..9 and five steps of stage 1, every step of theirs (0x528CD0..0x52A6B0).
// docs/effect_1e.md.
#pragma once

void Effect1E_Inject();
extern unsigned char g_fish_panel_duck;   // defined in effect_1e.cpp, armed by Effect1E_Inject (named here for a moved reader: source-layout B0)
extern unsigned char g_shop_ask_row;   // defined in effect_1e.cpp, armed by Effect1E_Inject (named here for a moved reader: source-layout B0)

namespace effect_1e {
// BOF3X_SHADOW=effect_1e: the start-up fuzz, effect_1e_fuzz.cpp. Clones every
// original before Effect1E_Inject patches it.
void SelfTest();
}  // namespace effect_1e
