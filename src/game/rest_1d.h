// Group R1D (round fourteen, wave one): party sets 9 to 12's field actions,
// originals 0x520E10..0x5226C1 - 46 functions of the cut (analysis/
// round14_cut.tsv). docs/rest_1d.md.
//
// Each party set has two entry tables the field core indexes by the set
// (Field_ActionBySet 0x6609D0 and Field_FormActions 0x660A44); each entry
// jumps by Sprite_Current's u16 form word +0x2C to a form, a form by its state
// byte +2 (or +3) to a state handler. 27 of the 46 are those dispatchers
// (void, no arguments; ours aborts on an index past its table's own count,
// which no state handler writes); 17 are state handlers (void, on
// Sprite_Current); two answer al: the cell probes a set's state calls on the
// cells ahead. All cdecl. "Sprite_Current" is the sprite whose state runs: its
// direction +8, its state bytes +2 / +3, its counters +9 / +0xA, +0xB, its
// 16.16 position +0x34 / +0x38, its height word +0x3E.
#pragma once

extern "C" {

// --- the dispatchers: jmp [table + 4 * index] -----------------------------------
void __cdecl PartyAction9_ByForm(void);          // 0x521340: Field_ActionBySet[9], by +0x2C (PartyAction9_Forms)
void __cdecl PartyFormAction9_ByForm(void);      // 0x521320: Field_FormActions[9], by +0x2C (PartyFormAction9_Forms)
void __cdecl PartyAction9_Form1State1(void);     // 0x520E10: by +3 (PartyAction9_Form1State1Steps)
void __cdecl PartyFormAction9_Form2(void);       // 0x520E70: by +2 (PartyFormAction9_Form2States)
void __cdecl PartyAction9_Form2(void);           // 0x520F20: by +2 (PartyAction9_Form2States)
void __cdecl PartyAction10_ByForm(void);         // 0x521A70: Field_ActionBySet[10]
void __cdecl PartyFormAction10_ByForm(void);     // 0x521A50: Field_FormActions[10]
void __cdecl PartyFormAction10_Form0(void);      // 0x521360
void __cdecl PartyAction10_Form0(void);          // 0x521380
void __cdecl PartyFormAction10_Form1(void);      // 0x5215C0
void __cdecl PartyAction10_Form1(void);          // 0x5215E0
void __cdecl PartyFormAction10_Form2(void);      // 0x5219E0
void __cdecl PartyAction10_Form2(void);          // 0x521A00
void __cdecl PartyAction11_ByForm(void);         // 0x5220C0: Field_ActionBySet[11]
void __cdecl PartyFormAction11_ByForm(void);     // 0x5220A0: Field_FormActions[11]
void __cdecl PartyFormAction11_Form0(void);      // 0x521A90
void __cdecl PartyAction11_Form0(void);          // 0x521AB0
void __cdecl PartyFormAction11_Form1(void);      // 0x521C80
void __cdecl PartyAction11_Form1(void);          // 0x521CA0
void __cdecl PartyAction12_ByForm(void);         // 0x522690: Field_ActionBySet[12]
void __cdecl PartyFormAction12_ByForm(void);     // 0x522670: Field_FormActions[12]
void __cdecl PartyFormAction12_Form0(void);      // 0x5220E0
void __cdecl PartyAction12_Form0(void);          // 0x522100
void __cdecl PartyAction12_Form0State0(void);    // 0x522120: by +3, five steps
void __cdecl PartyAction12_Form0State1(void);    // 0x5224B0: by +3
void __cdecl PartyFormAction12_Form1(void);      // 0x522650
void __cdecl PartyFormAction13_Form0(void);      // 0x5226B0: PartyFormAction13's forms (0x522B40's table) entry 0

// --- the state handlers ----------------------------------------------------------

// 0x520E30: PartyAction_SideProbes, the pose (+8 - 1) / 2 + 0x42, +0xA = 0xB,
// +3 = 1.
void __cdecl PartyAction9_Form1Start(void);
// 0x520E90 (entry 0 of eleven state tables): unless Cond_ByteFA is 0xF (then
// Field_State +0x137 = 0 only), the side direction nearer +8 - 5 when |+8 - 3|
// is greater than |+8 - 5|, else 3 - into +3, Sprite_TurnSense's answer for it
// into +0xB, +9 = 2, +2 one on.
void __cdecl PartyFormAction_TurnToSide(void);
// 0x520F40, 0x521600, 0x521CC0 (one code, three copies): an even direction
// turned toward something PartyAction_TargetAhead finds; the ground one step
// ahead more than 0x40 above the sprite on a slope - the pose + 0x46 and +2
// two on; else the side probes inline, a sound, the pose + 0x42, +0xA = 5, +2
// one on. +0xB = 0.
void __cdecl PartyAction9_Form2Begin(void);
void __cdecl PartyAction10_Form1Begin(void);
void __cdecl PartyAction11_Form1Begin(void);
// 0x521120, 0x5217E0, 0x521EA0: PartyAction5_Form0Resolve's code calling the
// set's own cell pickup: +0xA counted down; at 0 the object two steps ahead
// gets +0x80 bit 0 and the cells there are picked up; +2 one on; the script
// ticked once.
void __cdecl PartyAction9_Form2Resolve(void);
void __cdecl PartyAction10_Form1Resolve(void);
void __cdecl PartyAction11_Form1Resolve(void);
// 0x521200, 0x5218C0, 0x521F80: Field_CellPickup's code with the zenny
// bonus's multiplier 20, not 10. al 1 when the cell (x, z) held 0xF2 or
// 0xF8 (now cleared), else 0.
unsigned char __cdecl PartyAction9_CellPickup(unsigned x, unsigned z);
unsigned char __cdecl PartyAction10_CellPickup(unsigned x, unsigned z);
unsigned char __cdecl PartyAction11_CellPickup(unsigned x, unsigned z);
// 0x5213A0, 0x521AD0: the kind-0x30 effect object lined up ahead and no member
// on or beyond it: its +0xB = 1, the jump started (Field_JumpStart), Field_State
// +0x137 = 1, +2 one on; else the object two steps ahead marked (margin 1)
// unless Field_State +0x138 bit 0; Field_State +0x137 = 0.
void __cdecl PartyAction10_Form0Begin(void);
void __cdecl PartyAction11_Form0Begin(void);
// 0x521A20 (seven tables): Field_State +0x137 = 0 once effect object +0xB is
// free; Sprite_ScriptTick.
void __cdecl PartyAction_WaitEffectDone(void);
// 0x521C40 (eight tables): +9 counted down with Field_LeaderStepTick; at 0
// Field_State +0x137 = 0 and Field_ScriptFlags bit 0x1000 cleared;
// Sprite_ScriptTick.
void __cdecl PartyAction_StepCountdown(void);
// 0x522140: an even direction turned toward what PartyAction_BlockedAhead
// finds; a sound, the pose + 0x42; +0xB = +6 = 0, +0xA = 8, +3 = 1.
void __cdecl PartyAction12_Form0Begin(void);
// 0x5221E0: +0xA counted down; at 0 the kind-0x17 effect object ahead
// (Field_EffectAhead) gets the direction and +0xA = 1 and +3 moves two; else
// the object two steps ahead is marked and the cells there are hit
// (PartyAction12_CellHit), +3 one on. Sprite_ScriptTickOnce.
void __cdecl PartyAction12_Form0Resolve(void);
// 0x522320: al 1 when the cell (x, z) held 0xF0, 0xF1, 0xF4, 0xF6 or 0xF7 (an
// effect object of kind 0x34 placed on it, and by Rand more: a second object,
// an item, or a hurt member), else 0.
unsigned char __cdecl PartyAction12_CellHit(unsigned x, unsigned z);
// 0x5224D0: Sprite_ScriptTickOnce; at the script's end the pose +8 and +3 = 2;
// else +0xA counted down, at 0 effect object +0xB given the direction and +7
// (1 when 0) and a sound.
void __cdecl PartyAction12_Form0EffectSet(void);

}  // extern "C"

void Rest1D_Inject();

namespace rest_1d {
// BOF3X_SHADOW=rest_1d: the start-up fuzz, rest_1d_fuzz.cpp. Clones the 46
// originals before Rest1D_Inject patches them.
void SelfTest();
}  // namespace rest_1d
