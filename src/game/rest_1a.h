// Group R1A of round fourteen (wave one): the party-member states 4, 6 and 8,
// and party sets 0, 1 and 2's field actions, originals 0x51BA80..0x51D70C
// (docs/rest_1a.md). The field core reaches them through .data tables: a
// member's state through Member_States (Field_MemberFrame, by +1), a set's
// action through Field_ActionBySet (Field_ActionState, by the party set) and
// its "form action" through Field_FormActions (Member_FormActionState below,
// FE1's Field_FormActionState), then a dispatcher by the form (u16 +0x2C), a
// dispatcher by the state (+2) and, for set 2's form 2, one by the step (+3).
//
// Every function here is a void(void) state handler run on Sprite_Current
// (a party member's ObjTrio record, or the leader's), but four cdecl helpers:
// the three per-set copies of the cell pickup and set 2's cell strike, which
// take a map cell (x, z) and answer in al (their callers test al). Sets 0, 1
// and 2 repeat the same code: the three Form0Begin, Form0Resolve and
// CellPickup copies are byte-identical but for their relative calls (and the
// same as set 5's, ours in field_hidden.cpp), so each set's is its own
// function over one body.
#pragma once

extern "C" {

// --- party-member states (Member_States 0x65F960) -----------------------------------
void __cdecl Member_ResumeUnlessHeld800(void);   // 0x51BA80, state 4
void __cdecl Member_FormActionState(void);       // 0x51BAA0, state 6
void __cdecl Member_JumpState(void);             // 0x51BCF0, state 8: jmp Member_JumpSteps[+2]
void __cdecl Member_JumpAir(void);               // 0x51BD10, jump step 2

// --- the dispatchers: jmp [table + index * 4], the index unchecked ------------------
void __cdecl PartyFormAction0_ByForm(void);   // 0x51C740, Field_FormActions[0], by u16 +0x2C
void __cdecl PartyAction0_ByForm(void);       // 0x51C760, Field_ActionBySet[0], by u16 +0x2C
void __cdecl PartyFormAction0_Form0(void);    // 0x51BE90, by +2
void __cdecl PartyFormAction0_Form1(void);    // 0x51C430
void __cdecl PartyFormAction0_Form2(void);    // 0x51C470
void __cdecl PartyAction0_Form0(void);        // 0x51BFB0
void __cdecl PartyAction0_Form1(void);        // 0x51C450
void __cdecl PartyAction0_Form2(void);        // 0x51C510
void __cdecl PartyFormAction1_ByForm(void);   // 0x51CC00, Field_FormActions[1]
void __cdecl PartyAction1_ByForm(void);       // 0x51CC20, Field_ActionBySet[1]
void __cdecl PartyFormAction1_Form0(void);    // 0x51C780
void __cdecl PartyFormAction1_Form1(void);    // 0x51CB80
void __cdecl PartyFormAction1_Form2(void);    // 0x51CBC0
void __cdecl PartyAction1_Form0(void);        // 0x51C7A0
void __cdecl PartyAction1_Form1(void);        // 0x51CBA0
void __cdecl PartyAction1_Form2(void);        // 0x51CBE0
void __cdecl PartyFormAction2_Form0(void);    // 0x51CC40 (R1B's 0x51D710 reaches it)
void __cdecl PartyFormAction2_Form1(void);    // 0x51D0D0
void __cdecl PartyFormAction2_Form2(void);    // 0x51D200
void __cdecl PartyAction2_Form0(void);        // 0x51CCF0 (R1B's 0x51D730 reaches it)
void __cdecl PartyAction2_Form1(void);        // 0x51D1E0
void __cdecl PartyAction2_Form2(void);        // 0x51D220
void __cdecl PartyAction2_Form2State0(void);  // 0x51D240, by +3
void __cdecl PartyAction2_Form2State1(void);  // 0x51D670, by +3

// --- the form actions' states, shared by the sets' tables ---------------------------
void __cdecl PartyFormAction_Form0Begin(void);   // 0x51BEB0
void __cdecl PartyFormAction_Form0Turn(void);    // 0x51CC60
void __cdecl PartyFormAction_Form1Begin(void);   // 0x51D0F0
void __cdecl PartyFormAction_Form1Turn(void);    // 0x51D160
void __cdecl PartyFormAction_Form2Turn(void);    // 0x51C490

// --- the actions' states ------------------------------------------------------------
void __cdecl PartyAction0_Form0Begin(void);      // 0x51BFD0
void __cdecl PartyAction1_Form0Begin(void);      // 0x51C7C0
void __cdecl PartyAction2_Form0Begin(void);      // 0x51CD10
void __cdecl PartyAction0_Form0Resolve(void);    // 0x51C190
void __cdecl PartyAction1_Form0Resolve(void);    // 0x51C980
void __cdecl PartyAction2_Form0Resolve(void);    // 0x51CED0
void __cdecl PartyAction0_Form2Begin(void);      // 0x51C530
void __cdecl PartyAction2_Form2Aim(void);        // 0x51D260
void __cdecl PartyAction2_Form2Strike(void);     // 0x51D2F0
void __cdecl PartyAction2_Form2Reaim(void);      // 0x51D690
void __cdecl PartyAction_FinishPalette(void);    // 0x51D440
void __cdecl PartyAction_WaitEffect(void);       // 0x51D6D0

// --- the cell helpers: (x, z) a map cell (the callee reads each as 16 bits), al 0 / 1
unsigned char __cdecl PartyAction0_CellPickup(unsigned x, unsigned z);   // 0x51C270
unsigned char __cdecl PartyAction1_CellPickup(unsigned x, unsigned z);   // 0x51CA60
unsigned char __cdecl PartyAction2_CellPickup(unsigned x, unsigned z);   // 0x51CFB0
unsigned char __cdecl PartyAction2_CellStrike(unsigned x, unsigned z);   // 0x51D4E0

}  // extern "C"

void Rest1A_Inject();

namespace rest_1a {
// BOF3X_SHADOW=rest_1a: the start-up fuzz, rest_1a_fuzz.cpp. Clones the 49
// originals before Rest1A_Inject patches them.
void SelfTest();
}  // namespace rest_1a
