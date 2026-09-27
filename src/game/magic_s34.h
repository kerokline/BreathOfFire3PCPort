// Five spell overlays of Magic_Rows, round nine group S34 (the PSX's MAGIC158,
// MAGIC159, MAGIC161, MAGIC162 and MAGIC166.EMI, rows 89, 120, 111, 116 and
// 90; read one id down, the sibling's labels Charm, (no label), Timed Blow,
// Transfer and Monopolize): 47 functions, 0x4ED670..0x4EF616, taken with the
// shared harness (magic_harness.h). docs/magic_s34.md.
#pragma once

void MagicS34_Inject();

namespace magic_s34 {

// BOF3X_SHADOW=magic_s34: the start-up fuzz, magic_s34_fuzz.cpp. Clones every
// original before MagicS34_Inject patches it.
void SelfTest();

}  // namespace magic_s34
