// Effect kinds 0x21..0x27 (round thirteen, wave one, group E1D, the band
// 0x46F2B0..0x4702F6): seven kinds of the Effect_Objects pool, each a
// dispatcher by +1 through its state table and the states it names, and two
// draw helpers. Kind 0x22 spawns five kind-0x21 arms and waits for them; kind
// 0x23 spawns kind-0x24 rays; kind 0x25 is a disc at the leader's screen
// point; kind 0x27 (area 81's handler 0 spawns it) emits a kind-0x26 dome band
// every 16 frames. docs/effect_1d.md.
#pragma once

extern "C" {

// Kind 0x21: an arm of four vertices round a centre (0x46F570 builds them,
// 0x46F690 draws them); Effect_KindHandlers[0x21].
void __cdecl EffectKind21_Run(void);
void __cdecl EffectKind21_Start(void);
void __cdecl EffectKind21_Grow(void);
void __cdecl EffectKind21_Lift(void);
void __cdecl EffectKind21_Hold(void);
void __cdecl EffectKind21_Swirl(void);
void __cdecl EffectKind21_Fold(void);

// Kind 0x22: five kind-0x21 arms spawned and waited on; Effect_KindHandlers[0x22].
void __cdecl EffectKind22_Run(void);
void __cdecl EffectKind22_Sound(void);
void __cdecl EffectKind22_SpawnArms(void);
void __cdecl EffectKind22_WaitArms(void);

// Kind 0x23: kind-0x24 rays spawned on a count; Effect_KindHandlers[0x23].
void __cdecl EffectKind23_Run(void);
void __cdecl EffectKind23_Start(void);
void __cdecl EffectKind23_SpawnRays(void);

// Kind 0x24: a ray of a random direction and colour, shrinking; Effect_KindHandlers[0x24].
void __cdecl EffectKind24_Run(void);
void __cdecl EffectKind24_Start(void);
void __cdecl EffectKind24_Shrink(void);

// Kind 0x25: a disc at the leader's screen point; Effect_KindHandlers[0x25].
void __cdecl EffectKind25_Run(void);
void __cdecl EffectKind25_Start(void);
void __cdecl EffectKind25_Grow(void);
void __cdecl EffectKind25_Glow(void);
void __cdecl EffectKind25_Shrink(void);
// original 0x46FCF0: a fan of 32 Gouraud triangles round (x, y), the rim at
// `radius`; the centre shaded `centre`, the rim `rim`. Each argument is read
// as its low word (x, y, radius, signed) or byte (centre, rim): the callers
// push whole registers.
void __cdecl EffectKind25_DrawDisc(unsigned x, unsigned y, unsigned radius, unsigned centre, unsigned rim);

// Kind 0x26: a band of a dome widening at a world point; Effect_KindHandlers[0x26].
void __cdecl EffectKind26_Run(void);
void __cdecl EffectKind26_Start(void);
void __cdecl EffectKind26_Widen(void);
// original 0x46FFB0: sixteen Gouraud quads of the band between the polar
// angles t - 0x100 and t (at most 0x400) of a dome at the world point (x, z,
// height). `t` is read as its low word.
void __cdecl EffectKind26_DrawBand(long x, long z, long height, unsigned t);

// Kind 0x27: kind-0x26 bands emitted every 16 frames for 0x1000; Effect_KindHandlers[0x27].
void __cdecl EffectKind27_Run(void);
void __cdecl EffectKind27_Start(void);
void __cdecl EffectKind27_Emit(void);

}  // extern "C"

void Effect1D_Inject();

namespace effect_1d {
// BOF3X_SHADOW=effect_1d: the start-up fuzz, effect_1d_fuzz.cpp. Clones every
// original before Effect1D_Inject patches it.
void SelfTest();
}  // namespace effect_1d
