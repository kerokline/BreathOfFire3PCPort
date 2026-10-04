// Group R2B of round fourteen (wave two): 38 functions in 0x56E040..0x57F33C -
// the cut's 39 rows for R2B (analysis/round14_cut.tsv) less three jump-table
// cases, and two starts no list had (0x57E720, 0x57EDF0). docs/rest_2b.md.
//
// What they are, by the code:
//   - Effect_Spawn / Effect_SpawnAt, the two effect-record spawners the
//     movement-script ops and some sixty area handlers call;
//   - two menu primitives the community band calls by address: a grey
//     horizontal line (Menu_DrawGreyHLine) and the window colour's outline
//     with notched corners (Menu_DrawOutlineNotched);
//   - Field_ObjectTriggers' entry 1 (Field_TriggerCounterF0);
//   - game mode 8's step 8 screen (Shisu_*: the PSX twins are in the SHISU
//     overlay): its mode dispatcher on the menu block's 0x929F00, the open,
//     pick, show and close modes with their state and step tables, the
//     four counts and the score, and the two models it turns, drops and
//     draws (records 0x9398E0 and 0x939960).
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest2B_Inject();

namespace rest_2b {
// BOF3X_SHADOW=rest_2b: the start-up fuzz, rest_2b_fuzz.cpp. Clones the 38
// originals before Rest2B_Inject patches them.
void SelfTest();
}  // namespace rest_2b
