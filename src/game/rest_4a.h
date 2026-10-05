// Group R4A of round fourteen (wave four): 48 functions in 0x452DD0..0x456D4F -
// the cut's 48 rows for R4A (analysis/round14_cut.tsv), none dropped, none
// added. docs/rest_4a.md.
//
// What they are, by the code (the cut's labels said field core and a table of
// R4B's; neither holds):
//   - six battle helpers: the auto-target check, three random picks (a live
//     member, a live enemy, a live member other than one), a member's action
//     test and the fixed auto-target;
//   - Field_RunSlot and its CLUT-row copy (a Field_Slots record's step);
//   - the community's simulation (areas 175..185): the area entry that runs
//     it, its queue of new areas, population, mood, the two building levels,
//     the ticks of building kinds 5, 9, 0xB and 0xD, a record's spawn and
//     removal, two sums, the rolled offer words, and the placement of the
//     village's objects with the pose of each building kind;
//   - twelve Field_ObjectTriggers entries (ids 1..11 and 61).
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void Rest4A_Inject();

namespace rest_4a {
// BOF3X_SHADOW=rest_4a: the start-up fuzz, rest_4a_fuzz.cpp. Clones the 48
// originals before Rest4A_Inject patches them.
void SelfTest();
}  // namespace rest_4a
