// Group S23 of the spell round: four overlays of Magic_Rows - MAGIC100
// (row 13), MAGIC101 (row 26), MAGIC102 (row 69) and MAGIC103 (row 57), read
// one id down as Cyclone, Typhoon, Quake and Simoon - and the child effects
// compiled with them. 51 functions, taken with the shared harness
// (magic_harness.h). docs/magic_s23.md.
#pragma once

void MagicS23_Inject();

namespace magic_s23 {
// BOF3X_SHADOW=magic_s23: the start-up fuzz, magic_s23_fuzz.cpp. Clones
// every original before MagicS23_Inject patches it.
void SelfTest();
}  // namespace magic_s23
