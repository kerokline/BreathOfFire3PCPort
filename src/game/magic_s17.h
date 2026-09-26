// Round nine group S17: three overlays of Magic_Rows, taken with the shared
// spell harness (magic_harness.h) - MAGIC075 (row 46, Purify read one id
// down), MAGIC077 (row 30, Raise Dead / Resurrect) and MAGIC078 (row 15,
// Leech Power): 48 functions at 0x4BCBF0..0x4BEB45. docs/magic_s17.md.
#pragma once

void MagicS17_Inject();

namespace magic_s17 {
// BOF3X_SHADOW=magic_s17: the start-up fuzz, magic_s17_fuzz.cpp. Clones every
// original before MagicS17_Inject patches it.
void SelfTest();
}  // namespace magic_s17
