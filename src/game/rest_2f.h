// Group R2F of round fourteen (wave two): 49 functions at 0x58ED40..0x596F98 -
// the cut's 48 rows for R2F (analysis/round14_cut.tsv) and the start in their
// spans no list had (0x596530, window-record handler 1). docs/rest_2f.md.
//
// What they are, by the code:
//   - the field menu's Tactics screen (FieldMenu_States[5], Tactics_*): its
//     step machine, the formation grid and the party / reserve swap;
//   - the Config screen's three steps and state 8's dispatcher
//     (FieldMenu_States[7] and [8]);
//   - the Ability screen's helpers R2E's states call: the window set-ups and
//     the ability list's compaction and sorts;
//   - window-record handlers 1 and 2 of Field_RunTaskRecords (their kind tables
//     and kinds, the slide steps, the master's panel and the item list) and
//     window kind 1's list set-up and draws;
//   - three helpers other modules called by address: Stat_AddClampedTo,
//     AbilityList_CountSet, ItemTrade_TakeNeeds.
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest2F_Inject();

namespace rest_2f {
// BOF3X_SHADOW=rest_2f: the start-up fuzz, rest_2f_fuzz.cpp. Clones the 49
// originals before Rest2F_Inject patches them.
void SelfTest();
}  // namespace rest_2f
