// Two spell overlays of Magic_Rows, round nine group S02: the PSX's
// MAGIC003.EMI (row 3; Super Combo read one id down) and MAGIC004.EMI, whose
// code ten files run (rows 88, 92, 93, 98..100, 129..132: the Strike and Claw
// abilities read one id down; MAGIC005, 029, 049, 133..136, 156, 157 were
// folded into it by the linker): 48 functions, 0x49A7B0..0x49C3C5, taken with
// the shared harness (magic_harness.h). docs/magic_s02.md.
#pragma once

void MagicS02_Inject();

namespace magic_s02 {

// BOF3X_SHADOW=magic_s02: the start-up fuzz, magic_s02_fuzz.cpp. Clones every
// original before MagicS02_Inject patches it.
void SelfTest();

}  // namespace magic_s02
