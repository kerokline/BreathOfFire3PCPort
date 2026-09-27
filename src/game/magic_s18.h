// Two overlays of Magic_Rows, round nine group S18: MAGIC079.EMI (row 52, the
// ability ids 0x4F / 0xB7, read one id down Drain) at 0x4BEB50..0x4BF8C3 and
// MAGIC082.EMI (row 32, the ids 0x22 0x23 0x52 0x54 0x55 0xB8 0xBA 0xBB, read
// one id down Steroids, Magic Belt, Protect, Speed, Might) at
// 0x4C01F0..0x4C1461 - 42 functions, taken with the spell harness
// (magic_harness.h). docs/magic_s18.md.
#pragma once

void MagicS18_Inject();

namespace magic_s18 {
// BOF3X_SHADOW=magic_s18: the start-up fuzz, magic_s18_fuzz.cpp. Clones every
// original before MagicS18_Inject patches it.
void SelfTest();
}  // namespace magic_s18
