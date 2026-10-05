// Group R3G of round fourteen (wave three): 32 functions in 0x4925C0..0x5171FB -
// the cut's 32 rows for R3G (analysis/round14_cut.tsv), none dropped, none
// added. docs/rest_3g.md.
//
// What they are, by the code (the band is what round thirteen's effect groups
// left, and the catalog's top-level rows between them):
//   - effect kinds 0xAC (a fade in and out through R3F's full-screen gradient),
//     0xAD's first two states (a ring rising at the third member), 0xAE's
//     first state and 0xBA's line state;
//   - a screen-space triangle's winding (three callers: a cone, a beam, the
//     masters' model);
//   - the boss actors' placement into the enemy records at a boss encounter,
//     and the enemies' state bytes cleared;
//   - game modes 8, 9, 10 and 11: their step dispatchers and the steps not
//     already ours (entering and leaving mode 8, the two menu modes' loads,
//     their shared way back, mode 11's frame and look steps);
//   - Quake's vertex lift for a BMAGIC cell;
//   - area 109's switch (Area_CellHooks' entry for area 0x6D) and its flag
//     pattern;
//   - kind 0x18 sub-kind 0x41's two draws;
//   - area 0xBD's view: AreaMap_FrameAreaBD, its build and its cell texture;
//   - kind 0xF's character count.
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest3G_Inject();

namespace rest_3g {
// BOF3X_SHADOW=rest_3g: the start-up fuzz, rest_3g_fuzz.cpp. Clones the 32
// originals before Rest3G_Inject patches them.
void SelfTest();
}  // namespace rest_3g
