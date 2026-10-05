// Group R2D (round fourteen, wave two): the masters' screen and three field
// menu screens, originals 0x5869A0..0x58B1CD - the 51 functions of the cut
// (analysis/round14_cut.tsv). docs/rest_2d.md.
//
// Two bands, each one thing:
//
//  - 0x5869A0..0x587732, the screen a master opens when spoken to: the
//    machine on the bytes 0x9398CF (state, R2C's dispatcher 0x586670) and
//    0x9398D1 (step), with the member cursor 0x9398CE and the yes / no answer
//    0x9398D2; the master is the field mode tail's argument byte 0x9039F5.
//    State 3 (R2C's step table 0x664548) records a member as the master's
//    apprentice, state 4 (MasterQuit_Steps, read by ours) clears it, state 5
//    (MasterGrant_Steps) grants what the levels gained under the master earn,
//    state 6 leaves. The pick and the yes / no prompt draw the party's
//    panels (R2C's 0x585DC0 / 0x585BE0), a box (R2C's 0x586160) and a line.
//  - 0x589E00..0x58B1CD, the field menu (FieldMenu_States, menu block
//    0x929F00): the top bar's countdown into a screen, the camp check, the
//    Status screen (FieldMenu_States[6]) and the Items screen's first states
//    (FieldMenu_States[2]), and the field abilities' effects
//    (FieldAbility_Use and its ten handlers, called by R2E's Ability screen).
//
// All cdecl. The state handlers are void with no arguments; the dispatchers
// jump through their .data table by a byte no reader bounds (ours aborts past
// the table's own count).
#pragma once

extern "C" {

// --- the masters' screen -------------------------------------------------------------
void __cdecl MasterScreen_PickMember(unsigned message, unsigned apprentices);   // 0x5869A0
void __cdecl MasterScreen_DrawCursorFrame(unsigned x, unsigned y, unsigned w, unsigned h, unsigned blink,
                                          unsigned colours);                    // 0x586B90
void __cdecl MasterScreen_AskYesNo(unsigned message);                           // 0x586D20
void __cdecl MasterScreen_NameToText(unsigned member);                          // 0x587680
void __cdecl MasterScreen_State6Leave(void);                                    // 0x5876F0
void __cdecl MasterJoin_Step3Ask(void);                                         // 0x586D00
void __cdecl MasterJoin_Step5Apply(void);                                       // 0x586EB0
void __cdecl MasterJoin_Step6Told(void);                                        // 0x587010
void __cdecl MasterJoin_Step7AllCheck(void);                                    // 0x587050
void __cdecl MasterJoin_Step8Close(void);                                       // 0x5870E0
void __cdecl MasterQuit_ByStep(void);                                           // 0x587120
void __cdecl MasterQuit_Step2Pick(void);                                        // 0x587130
void __cdecl MasterQuit_Step3Ask(void);                                         // 0x587150
void __cdecl MasterQuit_Step5Apply(void);                                       // 0x587170
void __cdecl MasterQuit_Step6Told(void);                                        // 0x587260
void __cdecl MasterQuit_Step7NoneLeftCheck(void);                               // 0x5872A0
void __cdecl MasterQuit_Step8Close(void);                                       // 0x587330
void __cdecl MasterGrant_ByStep(void);                                          // 0x587370
void __cdecl MasterGrant_Step0Open(void);                                       // 0x587380
void __cdecl MasterGrant_Step1Member(void);                                     // 0x5873C0
void __cdecl MasterGrant_Step2Next(void);                                       // 0x5876C0

// --- the field menu --------------------------------------------------------------------
void __cdecl FieldMenu_TopBarCountdown(void);                                   // 0x589E00
unsigned char __cdecl FieldMenu_CampAllowedCell(void);                          // 0x589FB0

// FieldAbility_Use(caster, target, ability, battle) and FieldAbility_Effects'
// ten handlers (caster, target, battle): 1 done, 4 no effect, 3 not here.
unsigned char __cdecl FieldAbility_Use(unsigned caster, unsigned target, unsigned ability, unsigned battle);   // 0x58A3C0
unsigned char __cdecl FieldAbility_NotHere(unsigned caster, unsigned target, unsigned battle);        // 0x58A0E0
unsigned __cdecl FieldAbility_HealOne20(unsigned caster, unsigned target, unsigned battle);           // 0x58A0F0
unsigned __cdecl FieldAbility_HealOne40(unsigned caster, unsigned target, unsigned battle);           // 0x58A140
unsigned __cdecl FieldAbility_HealOneFull(unsigned caster, unsigned target, unsigned battle);         // 0x58A190
unsigned __cdecl FieldAbility_HealAll40(unsigned caster, unsigned target, unsigned battle);           // 0x58A1B0
unsigned __cdecl FieldAbility_HealAll120(unsigned caster, unsigned target, unsigned battle);          // 0x58A260
unsigned __cdecl FieldAbility_Clear80(unsigned caster, unsigned target, unsigned battle);             // 0x58A310
unsigned char __cdecl FieldAbility_NoEffect(unsigned caster, unsigned target, unsigned battle);       // 0x58A340
unsigned __cdecl FieldAbility_ClearA0(unsigned caster, unsigned target, unsigned battle);             // 0x58A350
unsigned __cdecl FieldAbility_HealFullClearA0(unsigned caster, unsigned target, unsigned battle);     // 0x58A380

// The Status screen: FieldMenu_States[6].
void __cdecl FieldMenuStatus_ByState(void);          // 0x58A4C0
void __cdecl FieldMenuStatus_Open(void);             // 0x58A4D0
void __cdecl FieldMenuStatus_SlideIn(void);          // 0x58A510
void __cdecl FieldMenuStatus_Choose(void);           // 0x58A570
void __cdecl FieldMenuStatus_Detail(void);           // 0x58A730
void __cdecl FieldMenuStatus_Close(void);            // 0x58A7D0
void __cdecl FieldMenuStatus_PlaceWindows(void);     // 0x58A850
void __cdecl FieldMenuStatus_DetailWindows(void);    // 0x58A920
void __cdecl FieldMenuStatus_ListWindows(void);      // 0x58AA30
void __cdecl FieldMenuStatus_ClearWindows(void);     // 0x58AAB0

// The Items screen: FieldMenu_States[2].
void __cdecl FieldMenuItems_ByState(void);           // 0x58AAE0
void __cdecl FieldMenuItems_Open(void);              // 0x58AAF0
void __cdecl FieldMenuItems_SlideIn(void);           // 0x58AB30
void __cdecl FieldMenuItems_Category(void);          // 0x58AB60
void __cdecl FieldMenuItems_List(void);              // 0x58AD30
void __cdecl FieldMenuItems_Close(void);             // 0x58B130
void __cdecl FieldMenuItems_State5ByStep(void);      // 0x58B1C0

}  // extern "C"

void Rest2D_Inject();

namespace rest_2d {
// BOF3X_SHADOW=rest_2d: the start-up fuzz, rest_2d_fuzz.cpp. Clones the 51
// originals before Rest2D_Inject patches them.
void SelfTest();
}  // namespace rest_2d
