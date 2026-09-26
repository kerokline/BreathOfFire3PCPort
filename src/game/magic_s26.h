// Three spell overlays of Magic_Rows, round nine group S26 (the PSX's
// MAGIC114, MAGIC115 and MAGIC117.EMI, rows 11, 115, 73 and 106; read one id
// down, the sibling's labels Fire Whip, Remedy, Rest / Snooze and Douse):
// 48 functions, 0x4D7960..0x4DA214, taken with the shared harness
// (magic_harness.h). docs/magic_s26.md.
#pragma once

void MagicS26_Inject();

namespace magic_s26 {

// BOF3X_SHADOW=magic_s26: the start-up fuzz, magic_s26_fuzz.cpp. Clones every
// original before MagicS26_Inject patches it.
void SelfTest();

}  // namespace magic_s26
