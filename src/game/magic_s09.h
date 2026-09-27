// Four spell overlays of Magic_Rows, round nine group S09 (the PSX's
// MAGIC045, MAGIC046 / MAGIC047 - one copy of code -, MAGIC048 and
// MAGIC050.EMI, rows 61, 62 / 63, 41 and 31; read one id down, the sibling's
// labels Bone Dart, Firebreath / Icebreath, Dream Breath and Pollen / Venom
// Breath): 47 functions, 0x4A9830..0x4ABC95, taken with the shared harness
// (magic_harness.h). docs/magic_s09.md.
#pragma once

void MagicS09_Inject();

namespace magic_s09 {

// BOF3X_SHADOW=magic_s09: the start-up fuzz, magic_s09_fuzz.cpp. Clones every
// original before MagicS09_Inject patches it.
void SelfTest();

}  // namespace magic_s09
