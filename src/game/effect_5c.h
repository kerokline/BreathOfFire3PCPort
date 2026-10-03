// Round thirteen group E5C (0x501500..0x503DDD): effect kind 0x18's sub-kinds
// 0x10, 0x11, 0x12, 0x15, 0x16, 0x17, 0x50, 0x56, 0x57 and 0x58 -
// EffectKind18_States' entries 16, 17, 18, 21, 22, 23, 80, 86, 87, 88: their
// dispatchers by +2, sub-states and draws. Every function runs with
// Sprite_Current an Effect_Objects record (Effect_RunObjects) or is a cdecl
// helper of one. docs/effect_5c.md. The functions are declared by
// symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Effect5C_Inject();

namespace effect_5c {
// BOF3X_SHADOW=effect_5c: the start-up fuzz, effect_5c_fuzz.cpp. Clones every
// original before Effect5C_Inject patches it.
void SelfTest();
}  // namespace effect_5c
