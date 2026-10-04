// Group R2A (round fourteen, wave two): the band 0x5372E0..0x537F1B left
// between the field core and chapter 0's block - 22 functions (the cut's 19
// and three starts no list had: 0x537760, 0x537B10, 0x537CE0). docs/rest_2a.md.
//
// Not scenario code, whatever the cut's classes say: a field frame's screen
// pass, two character-record helpers, Area_ObjectHandler and the four object
// handlers its fallback table Area_ObjectFallbacks (0x660CA0) names, with
// their states and the two effect spawns they share. An object handler runs
// on Sprite_Current (the object) and dispatches on its state byte +4 through
// a table it builds on its own stack; Field_ActiveMember +0xA0 bit 7 is the
// once-only mark its roll sets. All cdecl.
#pragma once

extern "C" {

// --- the field frame's pass and the record helpers --------------------------------
// 0x5372E0: every live Sprite_Objects record's draw slot +0x29 set, then its
// screen updated: Sprite_UpdateScreenA for type 0xA, Sprite_UpdateScreen for
// a sprite whose bank is in Mode11_ScreenBanks. Mode11_FieldFrame's sixth call.
void __cdecl Mode11_ListedSpriteScreens(void);
// 0x5373F0: a member's HP (+0x18) up by `amount`, held at +0x20; then the
// actor Field_State +0x148 names loses its 0x2000 when above a quarter.
void __cdecl Char_GainHp(unsigned amount, unsigned member);
// 0x537500: a member's AP (+0x1A) down by `amount`, never below 1; answers
// what it took (Char_LoseHp's AP twin, without its actor test).
unsigned __cdecl Char_LoseAp(unsigned amount, unsigned member);
// 0x537540 (named 2026-09-22, ours since round fourteen): a tail jump to the
// active member's area handler (Area_Descriptors[Game_AreaNumber] +0x3C by
// +0xA0 & 0x7F) or, at 0x7F, to Area_ObjectFallbacks[(short)n].
void __cdecl Area_ObjectHandler(unsigned short n);

// --- the object handlers of Area_ObjectFallbacks and their states ------------------
void __cdecl LinkedObjectA_Run(void);       // 0x5375A0: by +4 over 5 states
void __cdecl LinkedObjectA_Roll(void);      // 0x5375E0: state 0
void __cdecl LinkedObject_Show2(void);      // 0x537760: state 1 of A and C
void __cdecl LinkedObject_Show5(void);      // 0x5377B0: state 2 of A and C
void __cdecl LinkedObject_EndAfterEffect(void);    // 0x537800: state 3 of A and C, state 1 of B
void __cdecl LinkedObject_EndAfterMessage(void);   // 0x537850: state 4 of A and C
void __cdecl LinkedObjectB_Run(void);       // 0x5378A0: by +4 over 2 states
void __cdecl LinkedObjectB_Roll(void);      // 0x5378D0: state 0
void __cdecl LinkedObjectC_Run(void);       // 0x537950: by +4 over 5 states
void __cdecl LinkedObjectC_Roll(void);      // 0x537990: state 0
void __cdecl LinkedObjectD_Run(void);       // 0x537B10: by +4 over 6 states
void __cdecl LinkedObjectD_Roll(void);      // 0x537B50: state 0
void __cdecl LinkedObjectD_ShowAmount(void);   // 0x537CE0: states 1..3
void __cdecl LinkedObjectD_WaitMessage(void);  // 0x537D50: state 4
void __cdecl LinkedObjectD_ColourStep(void);   // 0x537D70: state 5

// --- the effects they spawn ---------------------------------------------------------
// 0x537DE0: an Effect_FindFree record of kind 0x19 at Sprite_Current, its
// sub-kind +6 the argument's byte; the record's index kept in +0xB.
void __cdecl LinkedObject_SpawnEffect19(unsigned sub);
// 0x537EA0: (s8)(Sprite_Current +0x30 - the leader's +0x30) / 28, four
// times that when negative; answers al.
unsigned char __cdecl LinkedObject_EffectRise(void);
// 0x537ED0: an Effect_FindFree record of kind 0x32, +6 the argument's byte.
void __cdecl LinkedObject_SpawnEffect32(unsigned sub);

}  // extern "C"

void Rest2A_Inject();

namespace rest_2a {
// BOF3X_SHADOW=rest_2a: the start-up fuzz, rest_2a_fuzz.cpp. Clones the 22
// originals before Rest2A_Inject patches them.
void SelfTest();
}  // namespace rest_2a
