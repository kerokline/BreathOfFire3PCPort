// Steal's overlay (the PSX's MAGIC216.EMI, Magic_Rows row 87): its task
// 0x4F50B0, its start 0x4F50E0 and its roll 0x4F5140 - the spell round's
// proof group, taken with the shared harness (magic_harness.h).
// docs/magic_steal.md.
#pragma once

void MagicSteal_Inject();

namespace magic_steal {
// BOF3X_SHADOW=magic_steal: the start-up fuzz, magic_steal_fuzz.cpp. Clones
// every original before MagicSteal_Inject patches it.
void SelfTest();
}  // namespace magic_steal
