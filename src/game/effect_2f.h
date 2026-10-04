// Effect kinds 0x4F, 0x50, 0x51, 0x52, 0x53 and 0x56, and kind 0x4E's disc
// (round thirteen, wave two, group E2F, the band 0x47B7D0..0x47DBD1): each kind
// a dispatcher by +1 through its state table and the states it names, and the
// helpers they call. docs/effect_2f.md.
#pragma once

extern "C" {

// Kind 0x4E's disc (E2E owns kind 0x4E's dispatcher and states; they call
// this): original 0x47B7D0, cdecl. A draw mode, then a fan of 32 Gouraud
// triangles round the record's screen point (+0x74, +0x78 floats, its depth
// +0x7C), the rim at `radius` (read as s16), the centre shaded `centre` and
// the rim `rim` (bytes).
void __cdecl EffectKind4E_DrawDisc(unsigned radius, unsigned centre, unsigned rim);

// Kind 0x4F: Effect_KindHandlers[0x4F], EffectKind4F_States (three: this start,
// FC1's EffectKind3C_Hold, E2D's 0x478160).
void __cdecl EffectKind4F_Run(void);
void __cdecl EffectKind4F_Start(void);

// Kind 0x50: specks thrown from the record's point; Effect_KindHandlers[0x50].
void __cdecl EffectKind50_Run(void);
void __cdecl EffectKind50_Start(void);
void __cdecl EffectKind50_Emit(void);
void __cdecl EffectKind50_Drain(void);
void __cdecl EffectKind50_ClearSpecks(void);
// 1 when no speck was in use, else 0 (al).
unsigned char __cdecl EffectKind50_MoveSpecks(void);
void __cdecl EffectKind50_SpeckQuad(unsigned char* speck);
void __cdecl EffectKind50_SpawnSpeck(void);
// The first free speck of the 64, or null.
unsigned char* __cdecl EffectKind50_FreeSpeck(void);

// Kind 0x51: a column swaying on a history of angles, then a ring;
// Effect_KindHandlers[0x51].
void __cdecl EffectKind51_Run(void);
void __cdecl EffectKind51_Start(void);
void __cdecl EffectKind51_Grow(void);
void __cdecl EffectKind51_Wait(void);
void __cdecl EffectKind51_Shrink(void);
void __cdecl EffectKind51_RingIn(void);
void __cdecl EffectKind51_RingOut(void);
void __cdecl EffectKind51_Start2(void);
void __cdecl EffectKind51_Grow2(void);
void __cdecl EffectKind51_Wait2(void);
void __cdecl EffectKind51_RingFade(void);
void __cdecl EffectKind51_Burst(void);
void __cdecl EffectKind51_RingClose(void);
void __cdecl EffectKind51_DrawMoves(void);
// `angle` read as its low word.
void __cdecl EffectKind51_PushAngle(unsigned angle);
void __cdecl EffectKind51_DrawFrame(void);
// `half` and `sway` read as s16.
void __cdecl EffectKind51_DrawColumn(unsigned half, unsigned sway);
// `radius` read as s16.
void __cdecl EffectKind51_DrawRing(unsigned radius);

// Kind 0x52: sparks, then a trail; Effect_KindHandlers[0x52].
void __cdecl EffectKind52_Run(void);
void __cdecl EffectKind52_Sparks(void);
void __cdecl EffectKind52_SparksClear(void);
void __cdecl EffectKind52_SparksEmit(void);
void __cdecl EffectKind52_SparksDrain(void);
void __cdecl EffectKind52_Trail(void);
void __cdecl EffectKind52_TrailStart(void);
void __cdecl EffectKind52_TrailRise(void);
void __cdecl EffectKind52_TrailTurn(void);
void __cdecl EffectKind52_TrailFall(void);
void __cdecl EffectKind52_TrailHold(void);
void __cdecl EffectKind52_TrailFade(void);
void __cdecl EffectKind52_ClearSparks(void);
// original 0x47CF20: the first free spark of the eight (0x1C apart from
// EffectKind30_Shards), or null. E2D's 0x4785B0 and E2E's 0x478CC0 call it too.
unsigned char* __cdecl EffectSpark_FindFree(void);
// 1 when a spark was in use, else 0 (al).
unsigned char __cdecl EffectKind52_MoveSparks(void);
void __cdecl EffectKind52_SparkGlow(unsigned char* spark);
// original 0x47CFF0: a spark's count down, at 0 reloaded 0x20 and its state
// up. Entry 1 of EffectKind52_SparkStates and of the table at 0x654660 that
// E2E's 0x479260 calls through.
void __cdecl EffectSpark_Wait(unsigned char* spark);
void __cdecl EffectKind52_SparkRise(unsigned char* spark);
void __cdecl EffectKind52_TrailUpdate(unsigned char* trail);
void __cdecl EffectKind52_TrailDraw(unsigned char* trail);
// original 0x47D4F0: a half fan of eight Gouraud triangles round a screen
// point (three floats: x, y, depth), `size` read as s16, `angle` as its low
// word, `shade` as its low byte. Kind 0x52's trail and two callers of other
// groups (E2E's 0x4796B0, E3D's 0x4875C0 EffectKind80_DrawTrail) draw a trail's end caps with it.
void __cdecl EffectTrail_DrawCap(const unsigned char* point, unsigned size, unsigned angle, unsigned shade);

// Kind 0x53: a beam from the top of the screen onto an extra sprite;
// Effect_KindHandlers[0x53].
void __cdecl EffectKind53_Run(void);
void __cdecl EffectKind53_Start(void);
void __cdecl EffectKind53_Beam(void);
// Each argument read as its low word.
void __cdecl EffectKind53_TexWindow(unsigned x, unsigned y, unsigned w, unsigned h);

// Kind 0x56: a column of crosses from an extra sprite's height to the ground;
// Effect_KindHandlers[0x56].
void __cdecl EffectKind56_Run(void);
void __cdecl EffectKind56_Start(void);
void __cdecl EffectKind56_Wait(void);
void __cdecl EffectKind56_Arm(void);
void __cdecl EffectKind56_Markers(void);
void __cdecl EffectKind56_Place(void);
// `semi` read as its low byte.
void __cdecl EffectKind56_DrawCross(long x, long z, long height, unsigned semi);

}  // extern "C"

void Effect2F_Inject();

namespace effect_2f {
// BOF3X_SHADOW=effect_2f: the start-up fuzz, effect_2f_fuzz.cpp. Clones every
// original before Effect2F_Inject patches it.
void SelfTest();
}  // namespace effect_2f
