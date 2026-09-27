// The engine-side rows of Magic_Rows (file 0xFFFF: rows 0, 108, 123, 126 and
// 128) and the one child effect they create (kind 1, parameter 0x5D): 22
// functions outside the BMAGIC band, compiled with the battle engine. Round
// nine group E, taken with the shared spell harness (magic_harness.h).
// docs/magic_engine.md.
#pragma once

void MagicEngine_Inject();

namespace magic_engine {
// BOF3X_SHADOW=magic_engine: the start-up fuzz, magic_engine_fuzz.cpp. Clones
// every original before MagicEngine_Inject patches it.
void SelfTest();
}  // namespace magic_engine
