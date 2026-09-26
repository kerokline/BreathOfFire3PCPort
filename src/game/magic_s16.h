// Four sparkle overlays (the PSX's MAGIC071..074.EMI; Magic_Rows rows 45, 113,
// 47 and 114), compiled into the exe at 0x4B9930..0x4BCBEE: each a copy of the
// Healing Herb's sparkle code (MAGIC070, round seven and eight's) on its own
// pool and tables, the last two with a child task per living actor of the
// target side. Round nine group S16, taken with the shared spell harness
// (magic_harness.h). docs/magic_s16.md.
#pragma once

void MagicS16_Inject();

namespace magic_s16 {
// BOF3X_SHADOW=magic_s16: the start-up fuzz, magic_s16_fuzz.cpp. Clones every
// original before MagicS16_Inject patches it.
void SelfTest();
}  // namespace magic_s16
