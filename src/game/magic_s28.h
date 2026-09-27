// Three spell overlays of Magic_Rows, round nine group S28 (the PSX's
// MAGIC122..MAGIC124.EMI, rows 74, 76 and 124; read one id down, the sibling's
// labels Firebreath, Icebreath and ThundrBreath): 42 functions,
// 0x4DD8B0..0x4E0906, taken with the shared harness (magic_harness.h), and the
// program's empty function Port_DroppedCall that the linker left inside
// MAGIC124. docs/magic_s28.md.
#pragma once

void MagicS28_Inject();

namespace magic_s28 {

// BOF3X_SHADOW=magic_s28: the start-up fuzz, magic_s28_fuzz.cpp. Clones every
// original before MagicS28_Inject patches it.
void SelfTest();

}  // namespace magic_s28
