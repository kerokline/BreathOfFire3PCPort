// Three spell overlays of Magic_Rows, round nine group S03 (the PSX's
// MAGIC006, MAGIC009 and MAGIC012.EMI, rows 109, 51 and 71; read one id down,
// the sibling's labels Mind Sword, Chlorine and Blitz): 44 functions,
// 0x49C3D0..0x49E9E2, taken with the shared harness (magic_harness.h).
// docs/magic_s03.md.
#pragma once

void MagicS03_Inject();

namespace magic_s03 {

// BOF3X_SHADOW=magic_s03: the start-up fuzz, magic_s03_fuzz.cpp. Clones every
// original before MagicS03_Inject patches it.
void SelfTest();

}  // namespace magic_s03
