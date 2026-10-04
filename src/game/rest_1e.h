// Round fourteen group R1E, originals 0x5226D0..0x523EC2: the field actions of
// party sets 13, 14, 15 and the first two forms of set 16 - the dispatchers
// Field_ActionBySet / Field_FormActions reach (by the sprite's form word
// +0x2C), the per-form state dispatchers (by +2, and for two forms by +3
// under +2), and the states they jump to. docs/rest_1e.md.
//
// Every function is cdecl. The states and dispatchers take nothing and work on
// Sprite_Current (the sprite whose state handler is running: its state bytes
// +2 / +3, +6, +0xA, +0xB, its direction +8, its 16.16 position +0x34 / +0x38,
// its height word +0x3E, its form word +0x2C). The five cell helpers take a
// map cell (x, z: the low words of the dwords; the callers pass dwords whose
// upper halves are their own uninitialised stack) and answer 0 / 1 in al
// (every caller tests al only).
//
// The three sets repeat one pattern: the same code at three addresses, the
// constants equal (capstone, docs/rest_1e.md section 1). The names say which
// set's table reaches each copy.
#pragma once

extern "C" {

// --- shared by many sets -------------------------------------------------------------

// original 0x5226D0: Field_State +0x137 = 0 - form 0 of the action tables of
// sets 5, 13, 14 and 15 (nothing to do: the leader's action ends).
void __cdecl PartyAction_NoAction(void);

// original 0x5239F0: a state 0 (seven tables): an even direction +8 turned one
// eighth back; +0x2B = 1, the side probes in directions 3 and 5 (each clears
// it on a steep slope whose ground is above the sprite);
// Sprite_EnsureAnimation((+8 >> 1) + 0x42); +2 one on.
void __cdecl PartyAction_ProbeBegin(void);

// original 0x522DE0: state 4 of the strike tables (nine cells): once effect
// object +6 is free (+0) or in its state 1 (+1 == 1) and Field_Kind2Hold is 0,
// +6 = 0 and +3 two back.
void __cdecl PartyAction_StrikeWait(void);

// --- set 13 (Field_ActionBySet / Field_FormActions entry 13) -------------------------

void __cdecl PartyFormAction13_ByForm(void);   // 0x522B40: PartyFormAction13_Forms by u16 +0x2C
void __cdecl PartyAction13_ByForm(void);       // 0x522B60: PartyAction13_Forms by u16 +0x2C
void __cdecl PartyFormAction13_Form1(void);    // 0x5226E0: PartyFormAction13_Form1States by +2
void __cdecl PartyAction13_Form1(void);        // 0x522700: PartyAction13_Form1States by +2
void __cdecl PartyFormAction13_Form2(void);    // 0x522720: PartyFormAction13_Form2States by +2
void __cdecl PartyAction13_Form2(void);        // 0x522740: PartyAction13_Form2States by +2

// original 0x522760 (and its copies 0x5230D0, 0x523550): an even +8 turned one
// eighth back, then two on, then back again while PartyAction_TargetAhead finds
// nothing; then the ground one step ahead (MapView_GroundAt) less the height
// word, and MapView_SlopeAt there (its answer unused): the scratch flag set and
// that difference above 0x40 - Sprite_EnsureAnimation((+8 - 1) / 2 + 0x46) and
// +2 one on; otherwise +0x2B = 1, the side probes, Sound_PlayEffect(u16 +0x2C
// + 0x100), Sprite_EnsureAnimation((+8 - 1) / 2 + 0x42), +0xA = 5. Then +0xB =
// 0 and +2 one on.
void __cdecl PartyAction13_Form2Begin(void);

// original 0x522940 (and 0x5232B0, 0x523730): +0xA counted down; at 0, the
// object two steps ahead gets bit 0 of its +0x80, the cell pickup on the
// point's cell, then the cells one on in x / z across a fraction, +2 one on.
// Sprite_ScriptTickOnce every time. PartyAction5_Form0Resolve's code.
void __cdecl PartyAction13_Form2Resolve(void);

// original 0x522A20 (and 0x523390, 0x523810): Field_CellPickup's code with the
// zenny bonus twenty times, not ten: AreaMap_ByteAt 0xF2 - an effect object
// spawned on the cell and, three Rand draws in sixteen, 2 or 5 zenny (times 20
// with Field_InputFlags & 6 and Rand & 3 zero); 0xF8 - item 0x56 of category 0
// taken. Both clear the cell and answer 1; anything else 0.
unsigned char __cdecl PartyAction13_CellPickup(unsigned x, unsigned z);

// --- set 14 ------------------------------------------------------------------------------

void __cdecl PartyFormAction14_ByForm(void);   // 0x5234B0
void __cdecl PartyAction14_ByForm(void);       // 0x5234D0
void __cdecl PartyFormAction14_Form0(void);    // 0x522B80: by +2
void __cdecl PartyFormAction14_Form1(void);    // 0x522BA0: by +2
void __cdecl PartyFormAction14_Form2(void);    // 0x523090: by +2
void __cdecl PartyAction14_Form1(void);        // 0x522BC0: PartyAction14_Form1Modes by +2
void __cdecl PartyAction14_Form1Mode0(void);   // 0x522BE0: PartyAction14_Mode0States by +3
void __cdecl PartyAction14_Form1Mode1(void);   // 0x523030: PartyAction14_Mode1States by +3
void __cdecl PartyAction14_Form2(void);        // 0x5230B0: by +2
void __cdecl PartyAction14_Form2Begin(void);   // 0x5230D0: PartyAction13_Form2Begin's code
void __cdecl PartyAction14_Form2Resolve(void); // 0x5232B0: PartyAction13_Form2Resolve's code
unsigned char __cdecl PartyAction14_CellPickup(unsigned x, unsigned z);   // 0x523390

// original 0x522C00 (and 0x523B40): an even +8 turned while
// PartyAction_BlockedAhead finds the way open (back one, on two, back two),
// PartyAction_SideProbes, Sprite_EnsureAnimation((+8 - 1) / 2 + 0x42), +0xB = 0,
// +0xA = 0xB, +3 = 1.
void __cdecl PartyAction14_StrikeBegin(void);

// original 0x523050: PartyAction_SideProbes, Sprite_EnsureAnimation((+8 - 1) / 2
// + 0x42), +0xA = 0xB, +3 = 1.
void __cdecl PartyAction14_Mode1Begin(void);

// original 0x522C90 (and 0x523BD0): +0xA counted down; at 0:
// Sound_PlayEffect(u16 +0x2C + 0x100); an effect object ahead
// (Field_EffectAhead) gets the sprite's direction (+8) and +0xA = 1, its
// index into +6, sound 0x10B, +3 two on; else the object two steps ahead
// flagged (+0x80 bit 0, sound 0x10B), the strike on the point's cell, then on
// the cells one on across a fraction, +3 one on. Sprite_ScriptTickOnce every
// time.
void __cdecl PartyAction14_Strike(void);

// original 0x522E20 (and 0x523D20): AreaMap_ByteAt at the cell - 0xF0, 0xF1,
// 0xF4: an effect object of kind 0x34 (state 0, and state 4 two Rand draws in
// eight) and sound 0x10B; 0xF6, 0xF7: state 0, sound 0x10B, then by Rand & 0xF
// - below 7 state 3 and item 0x29 of category 0 taken (+0xB = 2), 7..11
// nothing more, 12..15 state 2, Sprite_FlashClut(0), Char_LoseHp(1, Field_State
// +0x89) and message 0xD9 (+0xB = 1) - and Field_Request = 2. Those answer 1,
// anything else 0.
unsigned char __cdecl PartyAction14_StrikeCell(unsigned x, unsigned z);

// --- set 15 ------------------------------------------------------------------------------

void __cdecl PartyFormAction15_ByForm(void);   // 0x523970
void __cdecl PartyAction15_ByForm(void);       // 0x523990
void __cdecl PartyFormAction15_Form0(void);    // 0x5234F0
void __cdecl PartyFormAction15_Form1(void);    // 0x523510
void __cdecl PartyFormAction15_Form2(void);    // 0x523930
void __cdecl PartyAction15_Form1(void);        // 0x523530
void __cdecl PartyAction15_Form2(void);        // 0x523950
void __cdecl PartyAction15_Form1Begin(void);   // 0x523550
void __cdecl PartyAction15_Form1Resolve(void); // 0x523730
unsigned char __cdecl PartyAction15_CellPickup(unsigned x, unsigned z);   // 0x523810

// --- set 16's forms 0 and 1 (the table entries of 0x5243D0 / 0x5243F0, R1F's) ------------

void __cdecl PartyFormAction16_Form0(void);    // 0x5239B0
void __cdecl PartyAction16_Form0(void);        // 0x5239D0
void __cdecl PartyFormAction16_Form1(void);    // 0x523AE0
void __cdecl PartyAction16_Form1(void);        // 0x523B00: PartyAction16_Form1Modes by +2
void __cdecl PartyAction16_Form1Mode0(void);   // 0x523B20: PartyAction16_Mode0States by +3
void __cdecl PartyAction16_Form1Mode1(void);   // 0x523EB0: PartyAction16_Mode1States by +3
void __cdecl PartyAction16_StrikeBegin(void);  // 0x523B40
void __cdecl PartyAction16_Strike(void);       // 0x523BD0
unsigned char __cdecl PartyAction16_StrikeCell(unsigned x, unsigned z);   // 0x523D20

}  // extern "C"

void Rest1E_Inject();

namespace rest_1e {
// BOF3X_SHADOW=rest_1e: the start-up fuzz, rest_1e_fuzz.cpp. Clones the 47
// originals before Rest1E_Inject patches them.
void SelfTest();
}  // namespace rest_1e
