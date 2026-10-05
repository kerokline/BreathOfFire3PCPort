// Group R3E (round fourteen, wave three): what round thirteen left in the effect
// engine's first band, 0x46A320..0x4801ED - 50 functions, all effect code
// whatever the cut's classes say (26 rows it paired with SCENA code). docs/rest_3e.md.
//
// Three kinds' dispatchers; helpers kinds 0x1D, 0x21, 0x24, 0x30 and 0x41 call;
// the glow sparks, trail, spiral, ring and dust kinds 0x48 / 0x49 / 0x52 draw
// with; and the states of kinds 0x5D, 0x5E and 0x5F with their draws. Each
// state runs on Sprite_Current (an Effect_Objects record); the helpers are
// cdecl and handed a record or a pool entry.
#pragma once

extern "C" {

// --- three kinds' dispatchers: jmp [table + +1 * 4], unbounded ---------------------------
void __cdecl EffectKind37_Run(void);                 // 0x46A320, EffectKind37_States
void __cdecl EffectKind17_Run(void);                 // 0x46ABB0, EffectKind17_States
void __cdecl EffectKind1B_Run(void);                 // 0x46B7A0, EffectKind1B_States

// --- helpers of kinds 0x41, 0x30, 0x1D, 0x21, 0x24 ----------------------------------------
// 0x46D5F0: Sprite_Current +6 printed and drawn as 8 x 8 sprites from (x, y),
// one a character, a space skipped; `clut` the palette row.
void __cdecl EffectKind41_DrawNumber(int x, int y, int unused, unsigned clut);
// 0x46D710: the model *(+0x50) copied to 0x8C5D80 (the byte *(+0x54) faces of
// 0x28), +0x50 aimed at the copy, the shards and the sparks set up, +9 = 0x10.
void __cdecl EffectShards_LoadModel(void);
// 0x46D770: the shards stepped, the sparks drawn (a tail jump).
void __cdecl EffectShards_Step(void);
// 0x46E190: a speck (+4 its point) as a TILE_1, white or black, committed.
void __cdecl EffectKind1D_DrawSpeck(const unsigned char* speck);
// 0x46F570: an arm's four points round its centre (+0..+8), turned.
void __cdecl EffectKind21_ArmPoints(unsigned char* arm);
// 0x46F690: an arm drawn as two Gouraud triangles.
void __cdecl EffectKind21_DrawArm(unsigned char* arm);
// 0x46F6F0: one triangle of the arm's points a, b, c.
void __cdecl EffectKind21_DrawArmTriangle(unsigned char* arm, unsigned a, unsigned b, unsigned c);
// 0x46FAE0: a ray's four Gouraud lines from the leader's point.
void __cdecl EffectKind24_DrawRay(const unsigned char* ray);

// --- the glow sparks (8 of 0x1C at EffectKind30_Shards), kinds 0x48 / 0x49 / 0x52 ----------
void __cdecl EffectGlowSparks_Clear(void);                        // 0x4790C0
void __cdecl EffectGlowSparks_StartRise(unsigned char* spark);    // 0x4790F0
void __cdecl EffectGlowSparks_StartBurst(unsigned char* spark);   // 0x479160
unsigned char __cdecl EffectGlowSparks_Run(void);                 // 0x479260: al 1 when one was live
void __cdecl EffectGlowSparks_Draw(unsigned char* spark);         // 0x4792E0
void __cdecl EffectGlowSparks_Glow(unsigned char* spark);         // 0x479420, EffectGlowSparks_States[0]
void __cdecl EffectGlowSparks_Rise(unsigned char* spark);         // 0x479470, EffectGlowSparks_States[2]
// --- the glow trail (0x92C060), the spiral (0x92C4A4), the ring ---------------------------
void __cdecl EffectGlowTrail_Update(unsigned char* trail);        // 0x4794D0
void __cdecl EffectGlowTrail_Draw(unsigned char* trail);          // 0x4796B0
void __cdecl EffectSpiral_Init(unsigned char* spiral);            // 0x4799C0
void __cdecl EffectSpiral_StepDraw(unsigned char* spiral);        // 0x479B70
void __cdecl EffectRing_Draw(const unsigned char* ring);          // 0x479EE0
// --- the dust (64 of 0x20 at 0x92D1DC) ----------------------------------------------------
void __cdecl EffectDust_Clear(void);                              // 0x47A110
unsigned char* __cdecl EffectDust_FindFree(void);                 // 0x47A130
void __cdecl EffectDust_Start(unsigned char* dust);               // 0x47A150
unsigned char __cdecl EffectDust_Run(void);                       // 0x47A200: al 1 when one was live
unsigned char __cdecl EffectDust_DrawColumn(const unsigned char* dust);   // 0x47A2B0: al 1 when clamped
void __cdecl EffectDust_DrawFan(const unsigned char* dust);       // 0x47A3D0

// --- kind 0x5D's states (EffectKind5D_States 0..3) and kind 0x5E's (0..2) -------------------
void __cdecl EffectKind5D_Start(void);              // 0x47F2D0
void __cdecl EffectKind5D_Open(void);               // 0x47F340
void __cdecl EffectKind5D_Show(void);               // 0x47F3E0
void __cdecl EffectKind5D_Close(void);              // 0x47F550
void __cdecl EffectKind5E_Start(void);              // 0x47F5F0
void __cdecl EffectKind5E_Pick(void);               // 0x47F610
void __cdecl EffectKind5E_Wait(void);               // 0x47F720
void __cdecl EffectKind5D_DrawBoard(int x, int y, int w, int h);   // 0x47F7A0
void __cdecl EffectKind5D_DrawMark(int x, int y);                  // 0x47F910
void __cdecl EffectKind5E_DrawMenu(void);           // 0x47F9E0
void __cdecl EffectKind5E_DrawList(void);           // 0x47FAF0
void __cdecl EffectKind5E_DrawPanel(void);          // 0x47FBE0
// --- kind 0x5F's states 1..9 (EffectKind5F_States) ----------------------------------------
void __cdecl EffectKind5F_Launch(void);             // 0x47FDC0
void __cdecl EffectKind5F_Hop(void);                // 0x47FE30
void __cdecl EffectKind5F_Fly(void);                // 0x47FEE0
void __cdecl EffectKind5F_Bounce(void);             // 0x47FF60
void __cdecl EffectKind5F_FlyAway(void);            // 0x480010
void __cdecl EffectKind5F_PlaceHigh(void);          // 0x480080
void __cdecl EffectKind5F_Slide(void);              // 0x4800E0
void __cdecl EffectKind5F_PlaceLow(void);           // 0x480140
void __cdecl EffectKind5F_SlideOut(void);           // 0x480190

}

void Rest3E_Inject();

namespace rest_3e {
// BOF3X_SHADOW=rest_3e: the start-up fuzz, rest_3e_fuzz.cpp. Clones the 50
// originals before Rest3E_Inject patches them.
void SelfTest();
}  // namespace rest_3e
