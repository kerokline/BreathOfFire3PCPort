// Effect kinds 0x3E, 0x3F, 0x40, 0x42, 0x43, 0x44 and 0x6B (round thirteen,
// wave two, group E2C, the band 0x474F40..0x4771A5): seven kinds of the
// Effect_Objects pool, each a dispatcher by +1 through its state table and the
// states it names, and their draw helpers. docs/effect_2c.md.
//
//   kind 0x3E  four lightning bolts between fixed cells (Area145_Tail20 spawns it)
//   kind 0x3F  a beam between two points, one frame (Area145_SpawnTrail)
//   kind 0x40  a growing disc and 32 shards falling inside it (Area28_SpawnEffect40)
//   kind 0x42  a glow cylinder at its point (state 0 is E2G's 0x47EEC0)
//   kind 0x43  256 sparks rising round its point, then circling, then flying out
//              (chapter 10's run 13)
//   kind 0x44  a ring of light on sprite 1 with a cone and trailing sparks
//              (Area78_SpawnEffect44)
//   kind 0x6B  sprite 2 drawn off screen, read back, and crumbled into falling
//              pixels (chapter 10's run 13)
#pragma once

extern "C" {

// Kind 0x3E: Effect_KindHandlers[0x3E].
void __cdecl EffectKind3E_Run(void);
void __cdecl EffectKind3E_Start(void);
void __cdecl EffectKind3E_Bolts(void);
void __cdecl EffectKind3E_SubWait(void);
void __cdecl EffectKind3E_SubSound(void);
// original 0x4750D0: a bolt of 16 grey lines from the world point `from`
// (x, z, height) toward `to` (x and z; the height stays `from`'s), each joint
// but the last lifted by a random amount, each segment glowing
// (EffectKind3E_BoltGlow).
void __cdecl EffectKind3E_DrawBolt(const long* from, const long* to);
// original 0x475240: two Gouraud quads either side of the screen segment p0 ->
// p1 (x, y, depth as floats), half-bright at the segment and black at the rim.
void __cdecl EffectKind3E_BoltGlow(const unsigned char* p0, const unsigned char* p1);

// Kind 0x3F: Effect_KindHandlers[0x3F].
void __cdecl EffectKind3F_Run(void);
void __cdecl EffectKind3F_Draw(void);
// original 0x475390: a beam from the world point `from` to `to`: two
// half-discs (EffectKind3F_DrawCap) sized at each end's depth and the band
// between them (EffectKind3F_DrawBand).
void __cdecl EffectKind3F_DrawBeam(const long* from, const long* to);
// original 0x4754A0: eight Gouraud triangles of a half-disc round the screen
// point `centre` (x, y, depth floats); the radius and angle read as words.
void __cdecl EffectKind3F_DrawCap(const unsigned char* centre, unsigned radius, unsigned angle);
// original 0x4755B0: two Gouraud quads joining two screen points, each widened
// by its own radius at its own angle; radii and angles read as words.
void __cdecl EffectKind3F_DrawBand(const unsigned char* p0, unsigned r0, unsigned a0, const unsigned char* p1,
                                   unsigned r1, unsigned a1);

// Kind 0x40: Effect_KindHandlers[0x40].
void __cdecl EffectKind40_Run(void);
void __cdecl EffectKind40_Start(void);
void __cdecl EffectKind40_Grow(void);
void __cdecl EffectKind40_ShardsClear(void);
void __cdecl EffectKind40_ShardsStep(void);
void __cdecl EffectKind40_ShardDraw(void);
// original 0x475A20: a shard record started: in use, a random offset, height 0x280.
void __cdecl EffectKind40_ShardSpawn(unsigned char* shard);
// original 0x475A60: the first free shard record of the 32 (the cursor left on
// it), 0 when none.
unsigned char* __cdecl EffectKind40_ShardFind(void);
// original 0x475A90: a disc of 32 triangles round the record's point, the
// radius read as a word; its screen centre and rim kept at 0x92C280.
void __cdecl EffectKind40_DrawDisc(unsigned radius);

// Kind 0x42: Effect_KindHandlers[0x42] (state 0 is 0x47EEC0, group E2G's).
void __cdecl EffectKind42_Run(void);
void __cdecl EffectKind42_Glow(void);

// Kind 0x43: Effect_KindHandlers[0x43].
void __cdecl EffectKind43_Run(void);
void __cdecl EffectKind43_Start(void);
void __cdecl EffectKind43_Gather(void);
void __cdecl EffectKind43_Wait(void);
void __cdecl EffectKind43_Fade(void);
void __cdecl EffectKind43_Setup(void);
void __cdecl EffectKind43_AddSpark(void);
// original 0x4762D0: every live spark stepped and drawn; al 1 when any was.
unsigned char __cdecl EffectKind43_SparksRun(void);
void __cdecl EffectKind43_SparkDraw(const unsigned char* spark);

// Kind 0x6B: Effect_KindHandlers[0x6B].
void __cdecl EffectKind6B_Run(void);
void __cdecl EffectKind6B_Capture(void);
void __cdecl EffectKind6B_Store(void);
void __cdecl EffectKind6B_Scatter(void);
void __cdecl EffectKind6B_Arm(void);
void __cdecl EffectKind6B_Fall(void);

// Kind 0x44: Effect_KindHandlers[0x44].
void __cdecl EffectKind44_Run(void);
void __cdecl EffectKind44_Start(void);
void __cdecl EffectKind44_FadeIn(void);
void __cdecl EffectKind44_Sparks(void);
void __cdecl EffectKind44_Widen(void);
void __cdecl EffectKind44_Follow(void);
void __cdecl EffectKind44_FadeOut(void);
// The ring record (0x92BF80, through the cell 0x6761C0): the top point +0,
// the centre +0x10, the radius word +0x20, two shades +0x22 / +0x23, the
// projections +0x24 (top), +0x30 (centre) and sixteen ring points from +0x3C.
void __cdecl EffectKind44_RingProject(unsigned char* ring);
void __cdecl EffectKind44_RingDraw(const unsigned char* ring);
void __cdecl EffectKind44_RingCone(const unsigned char* ring);
void __cdecl EffectKind44_SparksClear(void);
void __cdecl EffectKind44_SparksStep(void);
void __cdecl EffectKind44_SparkTrail(unsigned char* spark);
void __cdecl EffectKind44_SparkEmit(void);
unsigned char* __cdecl EffectKind44_SparkFind(void);

}  // extern "C"

void Effect2C_Inject();

namespace effect_2c {
// BOF3X_SHADOW=effect_2c: the start-up fuzz, effect_2c_fuzz.cpp. Clones every
// original before Effect2C_Inject patches it.
void SelfTest();
}  // namespace effect_2c
