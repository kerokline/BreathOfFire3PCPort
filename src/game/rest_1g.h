// Group R1G of round fourteen (wave one): 45 functions at 0x5289A0..0x52CD46 -
// the cut's 43 rows for R1G (analysis/round14_cut.tsv) and the two starts in
// their spans no list had (0x528BE0, 0x52BF90). docs/rest_1g.md.
//
// Two machines, both the fishing spot's (docs/fishing-text.md places it):
//   - the rest of the leader's state 9 (LeaderPanel_*, E1E's stages 2..9 are
//     ours since round thirteen): stage 1's dispatcher and five of its steps,
//     stage 9's last step, stages 10 and 11 with their steps, the two pages of
//     the fishing menu E1F's ChoiceMenu_Run jumps to, and the helpers the
//     stages call - run with Sprite_Current the leader's ObjTrio record;
//   - game mode 8's fish (Fish_*): the spawn game mode 8's entry calls, the
//     frame's run over Sprite_Objects, its seven states and their helpers - run
//     with Sprite_Current (and Field_ActiveMember) one Sprite_Objects record.
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest1G_Inject();

namespace rest_1g {
// BOF3X_SHADOW=rest_1g: the start-up fuzz, rest_1g_fuzz.cpp. Clones the 45
// originals before Rest1G_Inject patches them.
void SelfTest();
}  // namespace rest_1g
