// Two spell overlays of Magic_Rows, round nine group S11 (the PSX's MAGIC058
// and MAGIC059.EMI, rows 82 and 83; read one id down, the sibling's labels
// Sanctuary and Tornado): 35 functions, 0x4AF040..0x4B0D42, taken with the
// shared harness (magic_harness.h). docs/magic_s11.md.
#pragma once

void MagicS11_Inject();

namespace magic_s11 {

// BOF3X_SHADOW=magic_s11: the start-up fuzz, magic_s11_fuzz.cpp. Clones every
// original before MagicS11_Inject patches it.
void SelfTest();

}  // namespace magic_s11
