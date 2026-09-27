// Two spell overlays of Magic_Rows, round nine group S32 (the PSX's MAGIC144
// and MAGIC150.EMI, rows 67 and 72; read one id down, the sibling's labels Wall
// of Fire and Eye Beam): 36 functions, 0x4E9140..0x4E9A66 and
// 0x4EA450..0x4EAE61, taken with the shared harness (magic_harness.h).
// docs/magic_s32.md.
#pragma once

void MagicS32_Inject();

namespace magic_s32 {

// BOF3X_SHADOW=magic_s32: the start-up fuzz, magic_s32_fuzz.cpp. Clones every
// original before MagicS32_Inject patches it.
void SelfTest();

}  // namespace magic_s32
