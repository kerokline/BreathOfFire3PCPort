// Three spell overlays of Magic_Rows, round nine group S37 (the PSX's
// MAGIC219, MAGIC220/221 and MAGIC222.EMI, rows 138, 136 / 146 and 133; read
// one id down, the sibling's labels Magma Breath, Geo Breath / Gaea's Breath
// and Combustion): 60 functions, 0x4F59D0..0x4F8638, taken with the shared
// harness (magic_harness.h). docs/magic_s37.md.
#pragma once

void MagicS37_Inject();

namespace magic_s37 {

// BOF3X_SHADOW=magic_s37: the start-up fuzz, magic_s37_fuzz.cpp. Clones every
// original before MagicS37_Inject patches it.
void SelfTest();

}  // namespace magic_s37
