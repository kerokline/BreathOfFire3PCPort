// Round thirteen group E3B (0x4823D0..0x48404A): effect kinds 0x63, 0x65, 0x67,
// 0x69 and 0x6C - their dispatchers, states and draws (kind 0x67's dispatcher
// and its spawning state; its other two states are EffectKind54_Start and
// BareRet). Every function runs with Sprite_Current an Effect_Objects record
// (Effect_RunObjects) or is a cdecl draw helper of one. docs/effect_3b.md. The
// functions are declared by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect3B_Inject();

namespace effect_3b {
// BOF3X_SHADOW=effect_3b: the start-up fuzz, effect_3b_fuzz.cpp. Clones every
// original before Effect3B_Inject patches it.
void SelfTest();
}  // namespace effect_3b
