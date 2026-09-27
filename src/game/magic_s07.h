// Four spell overlays of Magic_Rows, round nine group S07 (the PSX's
// MAGIC021, MAGIC038, MAGIC039 and MAGIC040.EMI, rows 33, 121, 78 and 79;
// read one id down, the sibling's labels Bonebreak, War Shout, Focus /
// Meditation and Enlighten): 59 functions, 0x4A3B20..0x4A6434, taken with
// the shared harness (magic_harness.h). docs/magic_s07.md.
#pragma once

void MagicS07_Inject();

namespace magic_s07 {

// BOF3X_SHADOW=magic_s07: the start-up fuzz, magic_s07_fuzz.cpp. Clones every
// original before MagicS07_Inject patches it.
void SelfTest();

}  // namespace magic_s07
