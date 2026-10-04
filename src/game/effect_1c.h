// Round thirteen group E1C (0x46A850..0x46F2A6): eight kinds of the
// Effect_Objects pool - kind 0x36, four VRAM rectangles moved one a frame;
// kind 0x3C's dispatcher; kind 0x70, the per-frame hook of five areas; kinds
// 0x1C and 0x1D, a ring of shaded quads round the record's point with specks
// rising inside it; kind 0x1E, the extra sprite's model drawn as lines, then
// split into its faces that tumble apart with eight shards and sixteen
// debris triangles; kind 0x1F, 32 debris triangles; kind 0x20, a fan whose
// size and angle step through seven states. docs/effect_1c.md.
#pragma once

void Effect1C_Inject();

namespace effect_1c {
// BOF3X_SHADOW=effect_1c: the start-up fuzz, effect_1c_fuzz.cpp. Clones every
// original before Effect1C_Inject patches it.
void SelfTest();
}  // namespace effect_1c
