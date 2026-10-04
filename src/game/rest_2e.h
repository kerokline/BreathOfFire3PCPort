// Group R2E of round fourteen (wave two): 49 functions at 0x58B1D0..0x58ED3F -
// the cut's 48 rows for R2E (analysis/round14_cut.tsv) and the start in their
// span no list had (0x58CFC0, band_rows' "code no list has"). docs/rest_2e.md.
//
// Three of the field menu's screens (docs/menu-screens.md section 1):
//   - the Items screen's arrange steps (FieldItems_ArrangeSteps, the state R2D's
//     0x58B1C0 runs), its discard prompt, its use-on-a-member state, a 32-entry
//     list view, two window helpers, and the seven sorts behind FieldItems_Sort;
//   - the Equipment screen (FieldMenu_States[4]): its dispatcher, its states but
//     the two FS took (Equip_ChooseSlot, Equip_ChooseItem), the preview helpers;
//   - the Ability screen (FieldMenu_States[3]): its dispatcher, states, and the
//     two step machines under it (arrange, view).
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest2E_Inject();

namespace rest_2e {
// BOF3X_SHADOW=rest_2e: the start-up fuzz, rest_2e_fuzz.cpp. Clones the 49
// originals before Rest2E_Inject patches them.
void SelfTest();
}  // namespace rest_2e
