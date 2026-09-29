// Round thirteen group E1F, the cut's fifteen rows at 0x52A6C0..0x52D07C
// (analysis/round13_cut.tsv): a field menu whose state lives in effect record
// 6, the sprite pass game mode 8 runs, the effect records' reset, the kind
// points FE1's panel prints, game mode 8's three steps, and the two primitive
// helpers a hundred and thirty effect and panel sites call. docs/effect_1f.md.
//
// Every one is ours in effect_1f.cpp, read with capstone to its last
// instruction; the declarations come from symbols.toml (bof3/symbols.gen.h).
// The cdecl helpers other code calls by name:
#pragma once

extern "C" {

// original 0x52CD50 (PSX twin 0x801E08A4): Sprite_UpdateScreen with
// Sprite_Current +0x3C held at 0, then +0x40 and +0x44 = 0x4650000 / d with the
// low byte cleared (d = +0x60 above 0, +0x60 - 2 * s16 +0x3E below 0; both 0
// at 0). A d of 0 is the original's divide fault: ours aborts.
unsigned long __cdecl Sprite_UpdateScreenScaled(void);

// original 0x52CE60 (PSX twin 0x801E0B14): a kind's points for a count - the
// record's word +0x12 at or above its threshold byte +0xF (36-byte records
// from 0x66A6A0), else (count * 10 / threshold) * points / 10. eax: the
// count's high word and the points, or the scaled value (callers read ax).
unsigned long __cdecl FieldPanel_KindPoints(unsigned kind, unsigned count);

// original 0x52CED0 (PSX twin 0x801E0BC4): FieldPanel_KindPoints summed over
// the 32 count bytes 0x9040EC that are not 0.
unsigned short __cdecl FieldPanel_KindTotal(void);

// original 0x52CF60 (PSX twin 0x801E0CD8): a draw mode of 0xC at the packet
// cursor from UiSprite_Modes[index], committed to `slot`.
void __cdecl UiSprite_SetMode(unsigned index, unsigned slot);

// original 0x52CFE0 (PSX twin 0x801E0E24): a sprite primitive of 0x1C at the
// packet cursor from UiSprite_Sheet[sprite] at (s16 x, s16 y), committed to
// `slot`; answers the primitive (callers write through it).
unsigned char* __cdecl UiSprite_Draw(unsigned sprite, unsigned slot, int x, int y);

}  // extern "C"

void Effect1F_Inject();

namespace effect_1f {
// BOF3X_SHADOW=effect_1f: the start-up fuzz, effect_1f_fuzz.cpp. Clones the
// fifteen originals before Effect1F_Inject patches them.
void SelfTest();
}  // namespace effect_1f
