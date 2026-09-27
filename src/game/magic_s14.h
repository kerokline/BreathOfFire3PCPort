// Three spell overlays of Magic_Rows, round nine group S14 (the PSX's
// MAGIC064, MAGIC065 and MAGIC066.EMI, rows 77, 70 and 17; read one id down,
// the sibling's labels Weretiger, Pilfer and Tsunami): 57 functions,
// 0x4B3F00..0x4B66C0, taken with the shared harness (magic_harness.h). Pilfer's
// other eight functions were ours since round eight (magic_fx_reached.cpp);
// this group takes its last, the item-name copy 0x4B58F0. docs/magic_s14.md.
#pragma once

void MagicS14_Inject();

namespace magic_s14 {

// BOF3X_SHADOW=magic_s14: the start-up fuzz, magic_s14_fuzz.cpp. Clones every
// original before MagicS14_Inject patches it.
void SelfTest();

}  // namespace magic_s14
