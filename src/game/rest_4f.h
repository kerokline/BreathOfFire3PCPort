// Round fourteen group R4F (0x460CB0..0x464B9E): the Config screen's machine and
// draws (field menu state 7), WorldMap_ExitRecords, and the states of effect
// kinds 2, 7, 8, 9 and 0xB with their draws. docs/rest_4f.md. The functions are
// declared by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Rest4F_Inject();

namespace rest_4f {
// BOF3X_SHADOW=rest_4f: the start-up fuzz, rest_4f_fuzz.cpp. Clones every
// original before Rest4F_Inject patches it.
void SelfTest();
}  // namespace rest_4f
