// Group R4E of round fourteen (wave four): 48 functions at 0x45E870..0x460CAD -
// the cut's 48 rows for R4E (analysis/round14_cut.tsv), every one a function.
// docs/rest_4e.md.
//
// What they are, by the code (the community band, the faerie village's
// resident code; areas 175..185):
//   - the panels of the band's two name lists - an entry of the 60-entry
//     table 0x9046D0 / 0x9048F0 and a member of the seven character records -
//     their frames, bars, portraits and selection marks (CommuEntry_*,
//     CommuMember_*, CommuCursor_DrawArrow, Commu_DrawPiece6,
//     Commu_DrawTiledFrame) and a random name of two message halves
//     (CommuName_MakeRandom), and the two name commits (CommuName_*);
//   - a track list on 0x939A3E / 0x939A40 (CommuMusic_*: a dispatcher, its
//     three modes' tables, the list drawn, a track played and the one before
//     put back);
//   - an item handed to the entry 0x9039F5 (CommuItem_*: the inventory
//     window, the choice, the item's name and its removal);
//   - three ranked lists paged through (CommuRank_*).
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest4E_Inject();

namespace rest_4e {
// BOF3X_SHADOW=rest_4e: the start-up fuzz, rest_4e_fuzz.cpp. Clones the 48
// originals before Rest4E_Inject patches them.
void SelfTest();
}  // namespace rest_4e
