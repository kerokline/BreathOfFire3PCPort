// Four spell overlays of Magic_Rows, round nine group S31 (the PSX's
// MAGIC132, MAGIC137, MAGIC138 and MAGIC143.EMI, rows 139, 101, 118 and 66;
// read one id down, the sibling's labels Doom Breath, Corona, Main Cannon and
// Thunder Clap): 51 functions, 0x4E6950..0x4E9138, taken with the shared
// harness (magic_harness.h). docs/magic_s31.md.
#pragma once

void MagicS31_Inject();

namespace magic_s31 {

// BOF3X_SHADOW=magic_s31: the start-up fuzz, magic_s31_fuzz.cpp. Clones every
// original before MagicS31_Inject patches it.
void SelfTest();

}  // namespace magic_s31
