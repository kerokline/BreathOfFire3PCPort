// Group R2G of round fourteen (wave two): 48 functions at 0x597FA0..0x59AA77 -
// the cut's 48 rows for R2G (analysis/round14_cut.tsv). docs/rest_2g.md.
//
// Window code of three record handlers of Field_RunTaskRecords:
//   - handler 4's kind 0 (the battle result's level-up window's one state)
//     and the EXP a party slot still needs, which its EXP window prints;
//   - handler 5 (the gene windows): its kind dispatch, kind 0's states - the
//     grid's open, its slide in and out;
//   - handler 6 (MenuList_Run's kinds): kinds 5..13 and 15..19 - panels,
//     boxes and lists other screens put in window records - and 26 of the
//     slide states its kinds' tables hold.
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest2G_Inject();

namespace rest_2g {
// BOF3X_SHADOW=rest_2g: the start-up fuzz, rest_2g_fuzz.cpp. Clones the 48
// originals before Rest2G_Inject patches them.
void SelfTest();
}  // namespace rest_2g
