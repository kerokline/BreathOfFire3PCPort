// The party sets' field actions of sets 6, 7, 8 and part of 9, originals
// 0x51F210..0x520E08 (round fourteen, wave one, group R1C): the dispatchers that
// Field_FormActions / Field_ActionBySet reach for those sets (by the form, the
// word +0x2C, then the state byte +2 or the step byte +3), the state handlers
// they reach - the probe and resolve states a set acts with, three copies of
// Field_CellPickup, two cell-hit helpers - and four state handlers every set's
// tables share (the turn to a side and its steps, a script tick that faces, a
// wait on an effect object). docs/rest_1c.md.
//
// Every function is declared by symbols.gen.h (symbols.toml's `impl` entries):
// 46 are void state handlers or dispatchers run on Sprite_Current, five are
// cdecl helpers (x, z) answering in al (every caller tests al only).
#pragma once

void Rest1C_Inject();

namespace rest_1c {
// BOF3X_SHADOW=rest_1c: the start-up fuzz, rest_1c_fuzz.cpp. Clones the 51
// originals before Rest1C_Inject patches them.
void SelfTest();
}  // namespace rest_1c
