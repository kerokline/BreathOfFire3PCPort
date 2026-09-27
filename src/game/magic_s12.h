// Two spell overlays of Magic_Rows, round nine group S12 (the PSX's MAGIC060
// and MAGIC062.EMI, rows 39 and 95; read one id down, the sibling's labels
// Identify and Celerity): 44 functions, 0x4B0D50..0x4B2F34, taken with the
// shared harness (magic_harness.h). docs/magic_s12.md.
#pragma once

void MagicS12_Inject();

namespace magic_s12 {

// BOF3X_SHADOW=magic_s12: the start-up fuzz, magic_s12_fuzz.cpp. Clones every
// original before MagicS12_Inject patches it.
void SelfTest();

}  // namespace magic_s12
