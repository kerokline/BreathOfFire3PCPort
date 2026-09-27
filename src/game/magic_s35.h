// Three spell overlays of Magic_Rows, round nine group S35 (the PSX's
// MAGIC167, MAGIC168 and MAGIC169.EMI, rows 91, 117 and 97; read one id down,
// the sibling's labels Last Resort, Cure and Benediction): 46 functions,
// 0x4EF620..0x4F1E36, taken with the shared harness (magic_harness.h).
// docs/magic_s35.md.
#pragma once

void MagicS35_Inject();

namespace magic_s35 {

// BOF3X_SHADOW=magic_s35: the start-up fuzz, magic_s35_fuzz.cpp. Clones every
// original before MagicS35_Inject patches it.
void SelfTest();

}  // namespace magic_s35
