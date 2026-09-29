// Effect kinds 0x48 (states 7..12) and 0x4A..0x4E (round thirteen, wave two,
// group E2E, the band 0x4789D0..0x47B7CF): the sub-states of kind 0x48's states
// 7..12 (each a dispatcher by +2 through its own table; kind 0x49's table,
// which overlaps kind 0x48's three entries on, reaches the same ones as its
// states 4..9), five kinds each a dispatcher by +1 and its states, and the
// helpers they call - a lit sphere of 0x1E2 points, a trail of 64 points, 32
// debris, a spark line, a rising column. docs/effect_2e.md.
#pragma once

extern "C" {

// Kind 0x48's state 7 (E2D's dispatcher 0x4789B0 by +2): a ring at 0x92D1C8.
void __cdecl EffectKind48_State7_Start(void);
void __cdecl EffectKind48_State7_Swell(void);
void __cdecl EffectKind48_State7_Widen(void);
void __cdecl EffectKind48_State7_Lift(void);
// State 8: a spiral record at 0x92C4A4, turning up.
void __cdecl EffectKind48_State8_Run(void);
void __cdecl EffectKind48_State8_Start(void);
void __cdecl EffectKind48_State8_Turn(void);
// State 9: sparks at the leader.
void __cdecl EffectKind48_State9_Run(void);
void __cdecl EffectKind48_State9_Start(void);
void __cdecl EffectKind48_State9_Emit(void);
void __cdecl EffectKind48_State9_Drain(void);
// State 10: the spiral at the leader, turning down.
void __cdecl EffectKind48_State10_Run(void);
void __cdecl EffectKind48_State10_Start(void);
void __cdecl EffectKind48_State10_Turn(void);
// State 11: the sphere at the leader.
void __cdecl EffectKind48_State11_Run(void);
void __cdecl EffectKind48_State11_Start(void);
void __cdecl EffectKind48_State11_Draw(void);
// State 12: a burst record at 0x92C060 that travels, holds, grows and shrinks.
void __cdecl EffectKind48_State12_Run(void);
void __cdecl EffectKind48_State12_Start(void);
void __cdecl EffectKind48_State12_Travel(void);
void __cdecl EffectKind48_State12_Hold(void);
void __cdecl EffectKind48_State12_Grow(void);
void __cdecl EffectKind48_State12_Shrink(void);

// original 0x479970: the mean of two angles of 12 bits, taken the short way
// round (0x800 added when they lie 0x800 or more apart). Answers in eax.
long __cdecl EffectAngle_Mean(unsigned a, unsigned b);
// original 0x47A560: the unit sphere's 0x1E2 vertices (15 rings of 32 and the
// two poles, 0x1000 = 1), its 0x200 quads and each vertex's shade under the
// light (0, 0x1000, 0x1000) normalised. al 0.
unsigned char __cdecl EffectSphere_Build(void);
// original 0x47A780: the sphere at Sprite_Current's point, radius 0x20 in
// cells, projected and drawn as 0x200 semi-transparent Gouraud quads. al 0.
unsigned char __cdecl EffectSphere_Draw(void);

// Kind 0x4A: a trail of 64 points after Sprite_Objects[1]; Effect_KindHandlers[0x4A].
void __cdecl EffectKind4A_Run(void);
void __cdecl EffectKind4A_Start(void);
void __cdecl EffectKind4A_Follow(void);
void __cdecl EffectKind4A_TrailInit(void);
void __cdecl EffectKind4A_TrailStep(void);
void __cdecl EffectKind4A_TrailDraw(void);
// original 0x47AF10: a fan of 32 Gouraud triangles round `point`'s screen
// position, its radius `size` (the low word) scaled to the point's depth; the
// centre shaded `centre`, the rim `rim` (bytes).
void __cdecl EffectKind4A_DrawGlow(const long* point, unsigned size, unsigned centre, unsigned rim);

// Kind 0x4B: 32 debris at a fixed cell; Effect_KindHandlers[0x4B].
void __cdecl EffectKind4B_Run(void);
void __cdecl EffectKind4B_Start(void);
void __cdecl EffectKind4B_Scatter(void);
// original 0x47B070: one debris record of 0x2C set at Sprite_Current's point.
void __cdecl EffectKind4B_DebrisInit(unsigned char* debris);

// Kind 0x4C: a spark line at a fixed cell for 300 frames; Effect_KindHandlers[0x4C].
void __cdecl EffectKind4C_Run(void);
void __cdecl EffectKind4C_Start(void);
void __cdecl EffectKind4C_Crackle(void);
void __cdecl EffectKind4C_DrawSpark(void);

// Kind 0x4D: a column rising at its point; Effect_KindHandlers[0x4D].
void __cdecl EffectKind4D_Run(void);
void __cdecl EffectKind4D_Start(void);
void __cdecl EffectKind4D_Rise(void);
void __cdecl EffectKind4D_Fade(void);
void __cdecl EffectKind4D_DrawColumn(void);

// Kind 0x4E: a disc at the leader's screen point; Effect_KindHandlers[0x4E].
void __cdecl EffectKind4E_Run(void);
void __cdecl EffectKind4E_Start(void);
void __cdecl EffectKind4E_Grow(void);
void __cdecl EffectKind4E_Glow(void);
void __cdecl EffectKind4E_Shrink(void);

}  // extern "C"

void Effect2E_Inject();

namespace effect_2e {
// BOF3X_SHADOW=effect_2e: the start-up fuzz, effect_2e_fuzz.cpp. Clones every
// original before Effect2E_Inject patches it.
void SelfTest();
}  // namespace effect_2e
