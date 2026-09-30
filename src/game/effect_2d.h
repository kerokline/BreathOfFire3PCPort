// Effect kinds 0x45..0x49 (round thirteen, wave two, group E2D, the band
// 0x4771B0..0x4789C1): five kinds of the Effect_Objects pool, each a dispatcher
// by +1 through its state table and the states it names, and their helpers.
// Kind 0x45 draws a 72 x 72 panel with a grid and a sine trace and reads the
// confirm button against the trace (chapter 6's run 13 spawns it); kind 0x46
// fills the screen with a colour stepped up and down five times, then counts
// the chapters' counter up; kind 0x47 puts two tinted sprites beside the
// leader or sprite object 2 (area 85's handlers 3 and 4 spawn it); kind 0x48
// throws rings of four sparks (area 52's handlers 6 and 7); kind 0x49 is ten
// variants picked by +1 at the spawn (chapter 7, Scena07_TakeEffect49), five
// of which are this group's. docs/effect_2d.md.
#pragma once

extern "C" {

// Kind 0x45: Effect_KindHandlers[0x45], EffectKind45_States (four).
void __cdecl EffectKind45_Run(void);
void __cdecl EffectKind45_Start(void);
void __cdecl EffectKind45_Tune(void);
void __cdecl EffectKind45_Aim(void);
void __cdecl EffectKind45_End(void);
// original 0x477500: the panel at (x, y) - two filled squares, its border, its
// grid and five markers. x and y are read as whole words; the callees they
// reach read the low 16 bits.
void __cdecl EffectKind45_DrawPanel(unsigned x, unsigned y);
// original 0x4776E0: a flat line (x0, y0) - (x1, y1), each read as an s16, in
// the colour of the three bytes 0x6761C4..0x6761C6.
void __cdecl EffectKind45_DrawLine(unsigned x0, unsigned y0, unsigned x1, unsigned y1);
// original 0x477760: a draw mode with the blend `abr` (its low byte).
void __cdecl EffectKind45_DrawMode(unsigned abr);
// original 0x4777A0: the 36-point history at EffectKind30_Shards moved up one
// and a sample of the sine wave put first. Every argument is read as its low
// word.
void __cdecl EffectKind45_PushSample(unsigned x, unsigned y, unsigned phase, unsigned frequency, unsigned amplitude, unsigned offset);
// original 0x477820: the trace inside the panel at (x, y).
void __cdecl EffectKind45_DrawTrace(unsigned x, unsigned y);
// original 0x4778A0: `count` (its low byte) points from `points`, joined inside
// the rectangle `clip` (four s16: x, y, w, h), fading.
void __cdecl EffectKind45_DrawPoints(unsigned count, const short* points, const short* clip);
// original 0x477940: the pixels from `from` to `to` (each two s16 in a dword)
// stepped along the longer axis; `shade` is read as its low byte.
void __cdecl EffectKind45_DrawSegment(unsigned from, unsigned to, unsigned shade);
// original 0x477AC0: one pixel at the two s16 `point`, grey `shade` (its low byte).
void __cdecl EffectKind45_Plot(const short* point, unsigned shade);
// original 0x477B20: a filled rectangle (x, y, w, h), each read as an s16, in
// the colour of 0x6761C4..0x6761C6.
void __cdecl EffectKind45_FillRect(unsigned x, unsigned y, unsigned w, unsigned h);

// Kind 0x46: Effect_KindHandlers[0x46], EffectKind46_States (five).
void __cdecl EffectKind46_Run(void);
void __cdecl EffectKind46_Start(void);
void __cdecl EffectKind46_FadeIn(void);
void __cdecl EffectKind46_FadeOut(void);
void __cdecl EffectKind46_Count(void);
void __cdecl EffectKind46_Hold(void);
void __cdecl EffectKind46_DrawFlash(void);
void __cdecl EffectKind46_RedrawSprites(void);

// Kind 0x47: Effect_KindHandlers[0x47], EffectKind47_States (three).
void __cdecl EffectKind47_Run(void);
void __cdecl EffectKind47_Start(void);
void __cdecl EffectKind47_Blink(void);
// original 0x478160: the two sprites +3 and +4 untinted, the record released.
// Also EffectKind14_States[2] and EffectKind3C_States[2].
void __cdecl EffectTwinSprites_Release(void);

// Kind 0x48: Effect_KindHandlers[0x48], EffectKind48_States (three).
void __cdecl EffectKind48_Run(void);
void __cdecl EffectKind48_Start(void);
void __cdecl EffectKind48_Burst(void);
void __cdecl EffectKind48_Fade(void);
void __cdecl EffectKind48_SpawnRing(void);
// original 0x478320: the first of the 16 sparks at EffectKind30_Shards whose
// +0x15 is 0, or null.
unsigned char* __cdecl EffectKind48_FindFreeSpark(void);
void __cdecl EffectKind48_ClearSparks(void);
// original 0x478360: every live spark drawn and stepped; answers 1 in al when
// any was live, else 0.
unsigned char __cdecl EffectKind48_StepSparks(void);
// original 0x4783C0: one spark's POLY_FT4.
void __cdecl EffectKind48_DrawSpark(const unsigned char* spark);

// Kind 0x49: Effect_KindHandlers[0x49], EffectKind49_Variants (ten, +1 the
// variant its spawner chose); variants 0..4 dispatch by +2.
void __cdecl EffectKind49_Run(void);
void __cdecl EffectKind49_V0Run(void);
void __cdecl EffectKind49_V0Start(void);
void __cdecl EffectKind49_V0Emit(void);
void __cdecl EffectKind49_V0End(void);
void __cdecl EffectKind49_V1Run(void);
void __cdecl EffectKind49_V1Start(void);
void __cdecl EffectKind49_V1Fade(void);
void __cdecl EffectKind49_V2Run(void);
void __cdecl EffectKind49_V2Launch(void);
void __cdecl EffectKind49_V2Fly(void);
void __cdecl EffectKind49_V2Wait(void);
void __cdecl EffectKind49_V2Rise(void);
void __cdecl EffectKind49_V2Sink(void);
void __cdecl EffectKind49_V3Run(void);
void __cdecl EffectKind49_V3Start(void);
void __cdecl EffectKind49_V3Burst(void);
void __cdecl EffectKind49_V3End(void);
void __cdecl EffectKind49_V4Run(void);

}  // extern "C"

void Effect2D_Inject();

namespace effect_2d {
// BOF3X_SHADOW=effect_2d: the start-up fuzz, effect_2d_fuzz.cpp. Clones every
// original before Effect2D_Inject patches it.
void SelfTest();
}  // namespace effect_2d
