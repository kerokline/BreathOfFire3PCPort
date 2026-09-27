// Four spell overlays of Magic_Rows, round nine group S08 (the PSX's
// MAGIC041, MAGIC042, MAGIC043 and MAGIC044.EMI, rows 80, 81, 110 and 96;
// read one id down, the sibling's labels Berserk, Counter / Mind's Eye,
// WardOfLight / Resist and Evil Eye): 59 functions, 0x4A6440..0x4A9822,
// taken with the shared harness (magic_harness.h). docs/magic_s08.md.
#pragma once

void MagicS08_Inject();

namespace magic_s08 {

// BOF3X_SHADOW=magic_s08: the start-up fuzz, magic_s08_fuzz.cpp. Clones every
// original before MagicS08_Inject patches it.
void SelfTest();

}  // namespace magic_s08
