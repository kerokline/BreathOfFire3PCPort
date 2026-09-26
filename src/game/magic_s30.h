// Two spell overlays of Magic_Rows, round nine group S30 (the PSX's
// MAGIC130 and MAGIC131.EMI, rows 141 and 144; read one id down, the
// sibling's labels Venom and KaiserBreath): 60 functions, 0x4E4420..0x4E6944,
// taken with the shared harness (magic_harness.h). docs/magic_s30.md.
#pragma once

void MagicS30_Inject();

namespace magic_s30 {

// BOF3X_SHADOW=magic_s30: the start-up fuzz, magic_s30_fuzz.cpp. Clones every
// original before MagicS30_Inject patches it.
void SelfTest();

}  // namespace magic_s30
