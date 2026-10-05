// Round fourteen group R3F (0x480210..0x49259C): what round thirteen left
// inside the effect engine's bands between kind 0x60 and kind 0xAB - the states
// of kinds 0x60, 0x9E, 0xA1, 0xA2, 0xA3, 0xA7, 0xA8, 0xA9, 0xAA and 0xAB's
// state 1, and the draws their dispatchers and neighbours call (kind 0x5F's
// line disc, kind 0x69's lines, kind 0x9C's trail, kind 0xA7's glow, kind
// 0xA9's disc, kind 0xAA's fill, the bars). Every state runs with
// Sprite_Current an Effect_Objects record (Effect_RunObjects). docs/rest_3f.md.
// The functions are declared by symbols.gen.h (each has an `impl` in
// symbols.toml).
#pragma once

void Rest3F_Inject();

namespace rest_3f {
// BOF3X_SHADOW=rest_3f: the start-up fuzz, rest_3f_fuzz.cpp. Clones every
// original before Rest3F_Inject patches it.
void SelfTest();
}  // namespace rest_3f
