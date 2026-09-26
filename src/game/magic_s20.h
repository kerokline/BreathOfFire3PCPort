// Three BMAGIC overlays compiled into the exe (Magic_Rows rows 48, 29, 49:
// the PSX's MAGIC087, MAGIC088 and MAGIC092): 51 functions at
// 0x4C3490..0x4C4486, 0x4C4490..0x4C4FB1 and 0x4C5680..0x4C62F6, spell round
// group S20. docs/magic_s20.md.
#pragma once

void MagicS20_Inject();

namespace magic_s20 {
// BOF3X_SHADOW=magic_s20: the start-up fuzz, magic_s20_fuzz.cpp. Clones every
// original before MagicS20_Inject patches it.
void SelfTest();
}  // namespace magic_s20
