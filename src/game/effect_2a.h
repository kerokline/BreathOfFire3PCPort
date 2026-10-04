// Effect kinds 0x28..0x2E (round thirteen, wave two, group E2A, the band
// 0x470300..0x473192): seven kinds of the Effect_Objects pool, each a
// dispatcher by +1 through its state table and the states it names, their draw
// helpers, and three families of particles kept in the pool at
// EffectKind30_Shards (specks of 0x14, sparks of 0x28, drops of 0x18 bytes).
// docs/effect_2a.md.
//
// "Beam", "box", "ring", "spark", "drop", "curtain" name the shape the code
// builds - the points it projects, the primitives it commits - not a
// play-tested fact about what the game shows.
#pragma once

extern "C" {

// Kind 0x28 (area 42's init spawns it): a beam between two points on the
// ground, restarted while story flag 0xB is set, pushing a party member off a
// row of eight cells. Effect_KindHandlers[0x28].
void __cdecl EffectKind28_Run(void);
void __cdecl EffectKind28_Start(void);
void __cdecl EffectKind28_WaitFlag(void);
void __cdecl EffectKind28_Beam(void);
void __cdecl EffectKind28_PushParty(void);
// original 0x470470: the beam from `a` to `b` (each three dwords: x, z, height)
// - a line, a half-fan at each end and two quads joining the fans.
void __cdecl EffectKind28_DrawBeam(const long* a, const long* b);
// original 0x470640: eight semi-transparent Gouraud triangles round (x, y) of
// radius `radius` from `angle` (each read as its low word).
void __cdecl EffectKind28_DrawEnd(unsigned x, unsigned y, unsigned radius, unsigned angle);
// original 0x4707A0: two quads from (x0, y0) - (x1, y1) to the circles round
// them (each argument read as its low word).
void __cdecl EffectKind28_DrawSides(unsigned x0, unsigned y0, unsigned r0, unsigned a0, unsigned x1, unsigned y1,
                                    unsigned r1, unsigned a1);

// Kind 0x29: two boxes of five faces and five growing segments.
// Effect_KindHandlers[0x29].
void __cdecl EffectKind29_Run(void);
void __cdecl EffectKind29_Start(void);
void __cdecl EffectKind29_Extend(void);
void __cdecl EffectKind29_Hold(void);
void __cdecl EffectKind29_Fade(void);
void __cdecl EffectKind29_Linger(void);
// original 0x470BF0: the two boxes of `boxes` (a record + 0xC) - eight corners,
// five faces.
void __cdecl EffectKind29_DrawBoxes(const unsigned char* boxes);
// original 0x470D80: a quad of four corners (indexes, their low bytes) shaded
// by two byte levels.
void __cdecl EffectKind29_DrawFace(unsigned i0, unsigned i1, unsigned i2, unsigned i3, unsigned shade0, unsigned shade1);
void __cdecl EffectKind29_SetSegments(void);
// original 0x470F70: the five segments drawn to t / 16 of their length, shaded
// (both read as their low bytes).
void __cdecl EffectKind29_DrawSegments(unsigned t, unsigned shade);

// Kind 0x2A: kind 0x28's beam at three rows, by story flag 0xD.
// Effect_KindHandlers[0x2A].
void __cdecl EffectKind2A_Run(void);
void __cdecl EffectKind2A_Start(void);
void __cdecl EffectKind2A_Beam(void);
void __cdecl EffectKind2A_Beams(void);
void __cdecl EffectKind2A_PushParty(void);
void __cdecl EffectKind2A_DrawBeam(const long* a, const long* b);
void __cdecl EffectKind2A_DrawEnd(unsigned x, unsigned y, unsigned radius, unsigned angle);
void __cdecl EffectKind2A_DrawSides(unsigned x0, unsigned y0, unsigned r0, unsigned a0, unsigned x1, unsigned y1,
                                    unsigned r1, unsigned a1);

// Kind 0x2B (chapter 9's run 11 spawns it): a ring on the ground that rises,
// throws specks, settles and shrinks. Effect_KindHandlers[0x2B]; the dispatcher
// calls its state, then draws the ring.
void __cdecl EffectKind2B_Run(void);
void __cdecl EffectKind2B_Start(void);
void __cdecl EffectKind2B_Rise(void);
void __cdecl EffectKind2B_Specks(void);
void __cdecl EffectKind2B_Settle(void);
void __cdecl EffectKind2B_Shrink(void);
void __cdecl EffectKind2B_Draw(void);
// The specks (the pool's 0x80 of 0x14 bytes; kinds 0x1C and 0x1D use them too).
void __cdecl EffectSpecks_Spawn(void);
unsigned char __cdecl EffectSpecks_Move(void);
void __cdecl EffectSpecks_Draw(const unsigned char* speck);

// Kind 0x2C: sparks thrown from Sprite_ObjectsExtra[0] toward eight ground
// points. Effect_KindHandlers[0x2C].
void __cdecl EffectKind2C_Run(void);
void __cdecl EffectKind2C_Start(void);
void __cdecl EffectKind2C_Emit(void);
// The sparks (the pool's 0x80 of 0x28 bytes).
void __cdecl EffectSparks_Move(void);
void __cdecl EffectSparks_Draw(const unsigned char* spark);
void __cdecl EffectSparks_Clear(void);
unsigned char* __cdecl EffectSparks_FindFree(void);

// Kind 0x2E: sparks from Sprite_Objects[1]. Effect_KindHandlers[0x2E].
void __cdecl EffectKind2E_Run(void);
void __cdecl EffectKind2E_Start(void);
void __cdecl EffectKind2E_Trickle(void);
void __cdecl EffectKind2E_Burst(void);

// Kind 0x2D (area 67's handler 2 spawns it): rays, a disc, rings and a curtain
// at the screen point between Sprite_Objects[0] and [1], and drops.
// Effect_KindHandlers[0x2D].
void __cdecl EffectKind2D_Run(void);
void __cdecl EffectKind2D_Start(void);
void __cdecl EffectKind2D_Spin(void);
void __cdecl EffectKind2D_Pause(void);
void __cdecl EffectKind2D_Open(void);
void __cdecl EffectKind2D_Pour(void);
void __cdecl EffectKind2D_Close(void);
void __cdecl EffectKind2D_Lift(void);
void __cdecl EffectKind2D_End(void);
// Each draw takes the record + 0xC: a screen point (three floats), then the
// words +0xC (a radius), +0xE (an angle), +0x10 (a level).
void __cdecl EffectKind2D_DrawRays(const unsigned char* q);
void __cdecl EffectKind2D_DrawDisc(const unsigned char* q);
void __cdecl EffectKind2D_DrawRings(const unsigned char* q);
void __cdecl EffectKind2D_DrawCurtain(const unsigned char* q);
// The drops (the pool's 0x80 of 0x18 bytes).
void __cdecl EffectDrops_Clear(void);
void __cdecl EffectDrops_Move(void);
void __cdecl EffectDrops_Draw(const unsigned char* drop);
unsigned char* __cdecl EffectDrops_FindFree(void);
void __cdecl EffectDrops_Launch(unsigned char* drop);

}  // extern "C"

void Effect2A_Inject();

namespace effect_2a {
// BOF3X_SHADOW=effect_2a: the start-up fuzz, effect_2a_fuzz.cpp. Clones every
// original before Effect2A_Inject patches it.
void SelfTest();
}  // namespace effect_2a
