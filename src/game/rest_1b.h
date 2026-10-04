// Party sets 2..6's field actions, originals 0x51D710..0x51F202 (round
// fourteen's wave-one group R1B, docs/rest_1b.md): the dispatchers that pick a
// party set's form (u16 Sprite_Current +0x2C), a form's state (+2) and a
// state's step (+3) through .data tables, and the states of sets 3, 4 and 5
// that act on what lies ahead of the leader - turn toward a target or a
// blocked cell, mark the object there, take what the map cell holds, push
// the kind-0x30 effect object lined up ahead, strike a blocking cell.
//
// Two paths reach a set's forms (docs/rest_1b.md section 1): leader state 10
// (Field_ActionState) calls Field_ActionBySet[set], here PartyActionN_ByForm;
// leader state 6 (Field_FormActionState) calls Field_FormActions[set], here
// PartyFormActionN_ByForm. Every function is cdecl and void, run on
// Sprite_Current, except the four cell handlers, which take a cell (x, z: the
// low words are read) and answer al 0 / 1. Every table index is unchecked, as
// the originals'; ours aborts where an entry is not code, and where an index
// would take an object record past its table (section 6).
#pragma once

extern "C" {

// --- the dispatchers: jmp [table + index * 4], the index unchecked ------------
// by u16 Sprite_Current +0x2C (the form), entries of Field_FormActions /
// Field_ActionBySet:
void __cdecl PartyFormAction2_ByForm(void);   // 0x51D710, Field_FormActions[2] -> PartyFormAction2_Forms
void __cdecl PartyAction2_ByForm(void);       // 0x51D730, Field_ActionBySet[2] -> PartyAction2_Forms
void __cdecl PartyFormAction3_ByForm(void);   // 0x51DE90, Field_FormActions[3] -> PartyFormAction3_Forms
void __cdecl PartyAction3_ByForm(void);       // 0x51DEB0, Field_ActionBySet[3] -> PartyAction3_Forms
void __cdecl PartyFormAction4_ByForm(void);   // 0x51E8B0, Field_FormActions[4] -> PartyFormAction4_Forms
void __cdecl PartyAction4_ByForm(void);       // 0x51E8D0, Field_ActionBySet[4] -> PartyAction4_Forms
void __cdecl PartyFormAction5_ByForm(void);   // 0x51F190, Field_FormActions[5] -> PartyFormAction5_Forms
// by Sprite_Current +2 (the form's state):
void __cdecl PartyFormAction3_Form0(void);    // 0x51D750 -> PartyFormAction3_Form0States
void __cdecl PartyAction3_Form0(void);        // 0x51D770 -> PartyAction3_Form0States
void __cdecl PartyFormAction3_Form1(void);    // 0x51DBC0 -> PartyFormAction3_Form1States
void __cdecl PartyAction3_Form1(void);        // 0x51DBE0 -> PartyAction3_Form1States
void __cdecl PartyFormAction3_Form2(void);    // 0x51DDE0 -> PartyFormAction3_Form2States
void __cdecl PartyAction3_Form2(void);        // 0x51DE00 -> PartyAction3_Form2States
void __cdecl PartyFormAction4_Form0(void);    // 0x51DED0 -> PartyFormAction4_Form0States
void __cdecl PartyAction4_Form0(void);        // 0x51DEF0 -> PartyAction4_Form0States
void __cdecl PartyFormAction4_Form1(void);    // 0x51E2D0 -> PartyFormAction4_Form1States
void __cdecl PartyAction4_Form1(void);        // 0x51E2F0 -> PartyAction4_Form1States
void __cdecl PartyFormAction4_Form2(void);    // 0x51E480 -> PartyFormAction4_Form2States
void __cdecl PartyAction4_Form2(void);        // 0x51E4A0 -> PartyAction4_Form2States
void __cdecl PartyFormAction5_Form0(void);    // 0x51E8F0 -> PartyFormAction5_Form0States
void __cdecl PartyFormAction5_Form1(void);    // 0x51ECF0 -> PartyFormAction5_Form1States
void __cdecl PartyAction5_Form1(void);        // 0x51ED10 -> PartyAction5_Form1States (PartyAction5_Forms[1])
void __cdecl PartyFormAction5_Form2(void);    // 0x51F170 -> PartyFormAction5_Form2States
void __cdecl PartyFormAction6_Form0(void);    // 0x51F1D0 -> PartyFormAction6_Form0States (set 6's, R1C's 0x51FA70 reaches it)
void __cdecl PartyAction6_Form0(void);        // 0x51F1F0 -> PartyAction6_Form0States (set 6's, R1C's 0x51FA90 reaches it)
// by Sprite_Current +3 (a state's step):
void __cdecl PartyAction4_Form2State0(void);  // 0x51E4C0 -> PartyAction4_Form2State0Steps
void __cdecl PartyAction4_Form2State1(void);  // 0x51E850 -> PartyAction4_Form2State1Steps
void __cdecl PartyAction5_Form1State0(void);  // 0x51ED30 -> PartyAction5_Form1State0Steps
void __cdecl PartyAction5_Form1State1(void);  // 0x51F0C0 -> PartyAction5_Form1State1Steps

// --- the states -------------------------------------------------------------------
// 0x51D790 / 0x51DF10: PartyAction5_Form0Begin's code (field_hidden.cpp), set
// 3's and set 4's copies: an even direction turned toward something to act on
// (PartyAction_TargetAhead), then a steep slope ahead (+2 two on) or the side
// probes, a sound, a pose and +0xA = 5 (+2 one on).
void __cdecl PartyAction3_Form0Begin(void);
void __cdecl PartyAction4_Form0Begin(void);
// 0x51D950 / 0x51E0D0: PartyAction5_Form0Resolve's code calling the set's own
// cell handler: +0xA counted down; at 0 the object two steps ahead marked and
// the cell (and the cells one on across a fraction) handed to the handler.
void __cdecl PartyAction3_Form0Resolve(void);
void __cdecl PartyAction4_Form0Resolve(void);
// 0x51DC00 / 0x51E310: the kind-0x30 effect object lined up ahead and no
// member on or beyond it: it is set going (+0xB = 1), the leader's jump
// started and Field_State +0x137 = 1, +2 one on; else the object two steps
// ahead marked, +0x137 = 0.
void __cdecl PartyAction3_Form1Begin(void);
void __cdecl PartyAction4_Form1Begin(void);
// 0x51DE20: shared by seven state tables - Sprite_ScriptTick; at script
// position 0xA an effect object of kind 0x3A taken into +0xB, a sound, +7 = 0,
// +2 one on.
void __cdecl PartyAction_SpawnKind3A(void);
// 0x51E4E0 / 0x51ED50: an even direction turned toward a blocked cell
// (PartyAction_BlockedAhead); then a pose, +0xA a count, +3 = 1 (set 4 makes
// the side probes, set 5 a sound and +6 = 0).
void __cdecl PartyAction4_Form2Aim(void);
void __cdecl PartyAction5_Form1Aim(void);
// 0x51E570 / 0x51EDF0: +0xA counted down; at 0 the effect object ahead set
// facing (+3 two on), or the object two steps ahead marked and the cell
// struck (the set's cell handler), +3 one on. Sprite_ScriptTickOnce last.
void __cdecl PartyAction4_Form2Hit(void);
void __cdecl PartyAction5_Form1Hit(void);
// 0x51E870 / 0x51F0E0: the next strike readied: (set 4) the side probes or (set
// 5) a sound, a pose, +0xA a count, +3 = 1.
void __cdecl PartyAction4_Form2Again(void);
void __cdecl PartyAction5_Form1Again(void);
// 0x51F130: once the effect object +0xB names is free or in state 1 and
// Field_Kind2Hold is 0, Field_State +0x137 = 0; Sprite_ScriptTick last.
void __cdecl PartyAction5_Form1Wait(void);

// --- the cell handlers: (x, z) the cell, al 1 when the cell held something --------
// 0x51DAA0 / 0x51E1B0: Field_CellPickup's code (codes 0xF2, 0xF8), sets 3 and 4.
unsigned char __cdecl PartyAction3_CellPickup(unsigned x, unsigned z);
unsigned char __cdecl PartyAction4_CellPickup(unsigned x, unsigned z);
// 0x51E6C0 / 0x51EF30: a blocking cell struck (codes 0xF0, 0xF1, 0xF4: effect
// objects; 0xF6, 0xF7: an effect object and by Rand item 0x29, nothing, or a
// point of damage to the member Field_State +0x89 names).
unsigned char __cdecl PartyAction4_CellHit(unsigned x, unsigned z);
unsigned char __cdecl PartyAction5_CellHit(unsigned x, unsigned z);

}  // extern "C"

void Rest1B_Inject();

namespace rest_1b {
// BOF3X_SHADOW=rest_1b: the start-up fuzz, rest_1b_fuzz.cpp. Clones the 47
// originals before Rest1B_Inject patches them.
void SelfTest();
}  // namespace rest_1b
