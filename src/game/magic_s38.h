// Three spell overlays of Magic_Rows, round nine group S38 (the PSX's
// MAGIC223, MAGIC225 and MAGIC226/227.EMI, rows 137, 140, 135 and 147; read
// one id down, the sibling's labels Tempest / Hurricane, an unlabelled id and
// MeteorStrike): 54 functions, 0x4F8640..0x4FAFE6, taken with the shared
// harness (magic_harness.h). docs/magic_s38.md.
#pragma once

void MagicS38_Inject();

namespace magic_s38 {

// BOF3X_SHADOW=magic_s38: the start-up fuzz, magic_s38_fuzz.cpp. Clones every
// original before MagicS38_Inject patches it.
void SelfTest();

}  // namespace magic_s38
