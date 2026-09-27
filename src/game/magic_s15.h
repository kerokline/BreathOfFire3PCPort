// Three spell overlays of Magic_Rows, round nine group S15 (the PSX's
// MAGIC067, MAGIC068 and MAGIC069.EMI, rows 18, 40 and 75; read one id down,
// the sibling's labels Chill, Foretell and Influence): 52 functions,
// 0x4B66D0..0x4B8D66, taken with the shared harness (magic_harness.h).
// docs/magic_s15.md.
#pragma once

void MagicS15_Inject();

namespace magic_s15 {

// BOF3X_SHADOW=magic_s15: the start-up fuzz, magic_s15_fuzz.cpp. Clones every
// original before MagicS15_Inject patches it.
void SelfTest();

}  // namespace magic_s15
