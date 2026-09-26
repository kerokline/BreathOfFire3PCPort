// One spell overlay of Magic_Rows, round nine group S01 (the PSX's
// MAGIC001.EMI, rows 1 and 105; read one id down, the sibling's labels Nue
// Stomp and Jump): 26 functions, 0x498FE0..0x499D74, taken with the shared
// harness (magic_harness.h). docs/magic_s01.md.
#pragma once

void MagicS01_Inject();

namespace magic_s01 {

// BOF3X_SHADOW=magic_s01: the start-up fuzz, magic_s01_fuzz.cpp. Clones every
// original before MagicS01_Inject patches it.
void SelfTest();

}  // namespace magic_s01
