// Two spell overlays of Magic_Rows, round nine group S29 (the PSX's
// MAGIC125 and MAGIC126.EMI, rows 125 and 127; read one id down, the
// sibling's labels DivineBreath and ShadowBreath): 49 functions,
// 0x4E0910..0x4E3254, taken with the shared harness (magic_harness.h).
// docs/magic_s29.md.
#pragma once

void MagicS29_Inject();

namespace magic_s29 {

// BOF3X_SHADOW=magic_s29: the start-up fuzz, magic_s29_fuzz.cpp. Clones every
// original before MagicS29_Inject patches it.
void SelfTest();

}  // namespace magic_s29
