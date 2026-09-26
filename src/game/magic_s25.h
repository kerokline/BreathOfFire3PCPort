// Four spell overlays of Magic_Rows (the PSX's MAGIC107..110.EMI, rows 24, 25,
// 12 and 38; read one id down Sleep, Confuse, Depress and Ragnarok - labels,
// not measurements), compiled into the exe at 0x4D2D80..0x4D610B: 56
// functions, the spell round's group S25. docs/magic_s25.md.
#pragma once

void MagicS25_Inject();

namespace magic_s25 {
// BOF3X_SHADOW=magic_s25: the start-up fuzz, magic_s25_fuzz.cpp. Clones every
// original before MagicS25_Inject patches it.
void SelfTest();
}  // namespace magic_s25
