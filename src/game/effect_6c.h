// Round thirteen group E6C (0x510C90..0x51426B): kind 0x18's sub-kinds 0x44
// (its frame and sky draw, under E6B's dispatcher), 0x45, 0x51, 0x53, 0x55,
// 0x59, 0x5A, 0x5B and 0x66 - their sub-state dispatchers by +2, the states and
// the draws they share - and the map's corner height 0x511C10. Every function
// but that one runs with Sprite_Current an Effect_Objects record
// (Effect_RunObjects, EffectKind18_Run) or is a cdecl draw helper of one.
// docs/effect_6c.md. The functions are declared by symbols.gen.h (each has an
// `impl` in symbols.toml).
#pragma once

void Effect6C_Inject();

namespace effect_6c {
// BOF3X_SHADOW=effect_6c: the start-up fuzz, effect_6c_fuzz.cpp. Clones every
// original before Effect6C_Inject patches it.
void SelfTest();
}  // namespace effect_6c
