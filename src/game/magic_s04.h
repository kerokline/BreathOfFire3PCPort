// Two spell overlays of Magic_Rows, round nine group S04 (the PSX's MAGIC013
// and MAGIC015.EMI, with MAGIC016 folded into MAGIC015; rows 50, 4, 7, 55 and
// 58; read one id down, the sibling's labels Snap, Charge, Flying Kick and Air
// Raid): 56 functions, 0x49E9F0..0x4A11D8, taken with the shared harness
// (magic_harness.h). docs/magic_s04.md.
#pragma once

void MagicS04_Inject();

namespace magic_s04 {

// BOF3X_SHADOW=magic_s04: the start-up fuzz, magic_s04_fuzz.cpp. Clones every
// original before MagicS04_Inject patches it.
void SelfTest();

}  // namespace magic_s04
