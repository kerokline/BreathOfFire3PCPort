// Round thirteen group E2B (0x4731A0..0x474F34): seven kinds of the
// Effect_Objects pool - kind 0x2F, three points circling down with a trail of
// 32 lines each; kind 0x33, a disc of 64 flat triangles widening round the
// record's point; kind 0x35, kind 0x1E's shape on the second extra sprite (its
// 55-face model split into tumbling pieces, sixteen shards); kind 0x38, a
// screen-wide tile brightened, a message, the tile dimmed; kind 0x39, a fan of
// 32 shaded triangles at a screen point that grows and shrinks; kind 0x3B, a
// glow and three spirals of quads round the record's point; kind 0x3D, a glow
// at the leader and rings rising from it. docs/effect_2b.md.
#pragma once

void Effect2B_Inject();

namespace effect_2b {
// BOF3X_SHADOW=effect_2b: the start-up fuzz, effect_2b_fuzz.cpp. Clones every
// original before Effect2B_Inject patches it.
void SelfTest();
}  // namespace effect_2b
