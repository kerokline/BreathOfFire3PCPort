// Round fourteen's group R1F (docs/rest_1f.md): the 49 functions the cut
// (analysis/round14_cut.tsv) lists for R1F, 0x523ED0..0x52899B, in three
// parts:
//
//   - party sets 16, 17 and 18's field actions: the entries of
//     Field_FormActions / Field_ActionBySet for the three sets, their forms'
//     and states' dispatchers (each `jmp [table + index * 4]` through a .data
//     table by the word +0x2C, the byte +2 or the byte +3 of Sprite_Current),
//     the state handlers, the three cell pickups and set 18's cell strike, and
//     three state handlers other sets' tables share;
//   - a raised sprite's cell ahead (Field_CellAhead's other half, which it
//     tail-jumps to when +0x70 is set) and its five helpers;
//   - the leader's state 9's stage 0 (Field_LeaderStates[9], its dispatcher
//     through LeaderPanel_Stages, and stage 0's three steps).
//
// All are cdecl. The state handlers take nothing and work on Sprite_Current
// (the sprite whose state runs); the helpers take what docs/rest_1f.md
// section 2 lists. Where a function answers, it answers in al and its
// callers read al only. A dispatcher's index past its own table, and an
// object or effect index past the records it indexes, abort (the originals
// read past them; docs/rest_1f.md section 7).
#pragma once

extern "C" {

// --- party set 16 ---------------------------------------------------------------------
void __cdecl PartyAction16_FormAction(void);     // 0x5243D0: Field_FormActions[16], by +0x2C
void __cdecl PartyAction16_ByForm(void);         // 0x5243F0: Field_ActionBySet[16], by +0x2C
void __cdecl PartyAction16_FormAction2(void);    // 0x523FB0: by +2
void __cdecl PartyAction16_Form2(void);          // 0x523FD0: by +2
void __cdecl PartyAction16_Form2Begin(void);     // 0x523FF0
void __cdecl PartyAction16_Form2Resolve(void);   // 0x5241D0
unsigned char __cdecl PartyAction16_CellPickup(unsigned x, unsigned z);   // 0x5242B0

// --- party set 17 ---------------------------------------------------------------------
void __cdecl PartyAction17_FormAction(void);     // 0x524930: Field_FormActions[17]
void __cdecl PartyAction17_ByForm(void);         // 0x524950: Field_ActionBySet[17]
void __cdecl PartyAction17_FormAction0(void);    // 0x524410
void __cdecl PartyAction17_FormAction1(void);    // 0x524450
void __cdecl PartyAction17_FormAction2(void);    // 0x5248F0
void __cdecl PartyAction17_Form0(void);          // 0x524430
void __cdecl PartyAction17_Form1(void);          // 0x524470
void __cdecl PartyAction17_Form2(void);          // 0x524910
void __cdecl PartyAction17_Form1Begin(void);     // 0x524490
void __cdecl PartyAction17_Form1Resolve(void);   // 0x524670
unsigned char __cdecl PartyAction17_CellPickup(unsigned x, unsigned z);   // 0x524750

// --- party set 18 ---------------------------------------------------------------------
void __cdecl PartyAction18_FormAction(void);     // 0x525330: Field_FormActions[18]
void __cdecl PartyAction18_ByForm(void);         // 0x525350: Field_ActionBySet[18]
void __cdecl PartyAction18_FormAction0(void);    // 0x524970
void __cdecl PartyAction18_FormAction1(void);    // 0x524E50
void __cdecl PartyAction18_FormAction2(void);    // 0x525270
void __cdecl PartyAction18_Form0(void);          // 0x524990: by +2
void __cdecl PartyAction18_Form0Sub0(void);      // 0x5249B0: by +3
void __cdecl PartyAction18_Form0Sub1(void);      // 0x524D40: by +3
void __cdecl PartyAction18_Form0Sub0Begin(void); // 0x5249D0
void __cdecl PartyAction18_Form0Sub0Strike(void);   // 0x524A60
unsigned char __cdecl PartyAction18_CellStrike(unsigned x, unsigned z);   // 0x524BB0
void __cdecl PartyAction18_ProbeStart(void);     // 0x524D60
void __cdecl PartyAction18_Form1(void);          // 0x524E70
void __cdecl PartyAction18_Form1Begin(void);     // 0x524E90
void __cdecl PartyAction18_Form1Resolve(void);   // 0x525070
unsigned char __cdecl PartyAction18_CellPickup(unsigned x, unsigned z);   // 0x525150
void __cdecl PartyAction18_Form2(void);          // 0x525290

// --- state handlers other sets' tables share ------------------------------------------
void __cdecl PartyAction_ProbeStart(void);       // 0x523ED0
void __cdecl PartyAction_EffectCountdown(void);  // 0x523F10
void __cdecl PartyAction_SpawnKind1B(void);      // 0x5252B0

// --- a raised sprite's cell ahead -------------------------------------------------------
unsigned char __cdecl Field_CellAheadRaised(void);                          // 0x527640
unsigned char __cdecl Field_CellClass5(unsigned a, unsigned b, unsigned c, unsigned d, unsigned e);   // 0x527DB0
unsigned char __cdecl Field_CornerTurn(void);                               // 0x527FF0
unsigned char __cdecl Field_SlopeBetween(unsigned x0, unsigned x1, unsigned z0, unsigned z1, unsigned direction);   // 0x5280A0
void __cdecl Field_RaisedEdgeTurns(unsigned x, unsigned z);                 // 0x528190
void __cdecl Field_ReadCellsRaised(unsigned x, unsigned z, unsigned x0, unsigned z0);   // 0x5287B0

// --- the leader's state 9, stage 0 ---------------------------------------------------------
void __cdecl LeaderPanel_Run(void);              // 0x528880: Field_LeaderStates[9], by +2
void __cdecl LeaderPanel_S0(void);               // 0x5288A0: LeaderPanel_Stages[0], by +3
void __cdecl LeaderPanel_S0Begin(void);          // 0x5288C0
void __cdecl LeaderPanel_S0Wait(void);           // 0x528940
void __cdecl LeaderPanel_S0End(void);            // 0x528970

}  // extern "C"

void Rest1F_Inject();

namespace rest_1f {
// BOF3X_SHADOW=rest_1f: the start-up fuzz, rest_1f_fuzz.cpp. Clones the 49
// originals before Rest1F_Inject patches them.
void SelfTest();
}  // namespace rest_1f
