// Round thirteen group E4A (0x488020..0x48902E, and 0x433640): effect kinds
// 0x82 (states 11..23), 0x83, 0x84, 0x85, 0x86 and 0x87 - their dispatchers and
// states - and the one-line state Sprite_StateRestart kind 0x83 shares with
// the battle's actor watch. Every state runs with Sprite_Current an
// Effect_Objects record (Effect_RunObjects). docs/effect_4a.md. The functions
// are declared by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect4A_Inject();

namespace effect_4a {
// BOF3X_SHADOW=effect_4a: the start-up fuzz, effect_4a_fuzz.cpp. Clones every
// original before Effect4A_Inject patches it.
void SelfTest();
}  // namespace effect_4a
