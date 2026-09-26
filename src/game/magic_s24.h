// Spell group S24 of round nine: three overlays of Magic_Rows, taken with the
// shared harness (magic_harness.h). docs/magic_s24.md.
//
//   MAGIC104 (row 14,  read one id down: Sirocco)  0x4D04B0..0x4D16D3, 19 functions
//   MAGIC105 (row 64,  read one id down: Kyrie)    0x4D16E0..0x4D2442, 16 functions
//   MAGIC106 (row 104, read one id down: Death)    0x4D2450..0x4D2D7E, 12 functions
//
// The names are hypotheses (cut-content section 2): nothing here depends on
// them.
#pragma once

void MagicS24_Inject();

namespace magic_s24 {
// BOF3X_SHADOW=magic_s24: the start-up fuzz, magic_s24_fuzz.cpp. Clones
// every original before MagicS24_Inject patches it.
void SelfTest();
}  // namespace magic_s24
