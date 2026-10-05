// Group R4B (round fourteen, wave four): the community band's tail kinds and
// its board - originals 0x456D50..0x459EDA, the cut's 60 functions.
// docs/rest_4b.md.
//
// Field_ModeTailKinds 14, 21..26 and 60 are dispatchers on the s8 tail state
// 0x9039F4 (one run of code words at 0x6529E4, another at 0x652A70, read from
// several cells) and the states they reach; the board (CommuBoard_States, by
// 0x939A3E) runs the grid of slots, the 60 community records at 0x9046D0 and
// the lists a slot's kind is picked from. The dispatchers abort on an index
// outside their table's own run (section 3); the draw helpers take cdecl
// words. All cdecl.
#pragma once

extern "C" {

// --- the tail kinds and their states -------------------------------------------
void __cdecl CommuTail14_Dispatch(void);        // 0x456D50: Field_ModeTailKinds[14]
void __cdecl CommuTail14_OpenF8(void);          // 0x456D60
void __cdecl CommuTail14_LoadDat(void);         // 0x456D90
void __cdecl CommuTail_WaitLoad(void);          // 0x456DB0
void __cdecl CommuTail_LoadSoundBank(void);     // 0x456DF0
void __cdecl CommuTail14_End(void);             // 0x456E20
void __cdecl CommuTail_RestoreFacing(void);     // 0x456E40
void __cdecl CommuTail21_Dispatch(void);        // 0x456E70: Field_ModeTailKinds[21]
void __cdecl CommuTail_TimedGift(void);         // 0x456E80
void __cdecl CommuTail_EndAfterMessage(void);   // 0x4570C0
void __cdecl CommuTail_Open97(void);            // 0x4570E0
void __cdecl CommuTail22_Dispatch(void);        // 0x457110: Field_ModeTailKinds[22]
void __cdecl CommuTail_RandomGift(void);        // 0x457120
void __cdecl CommuTail23_Dispatch(void);        // 0x4572F0: Field_ModeTailKinds[23]
void __cdecl CommuTail23_LoadDat(void);         // 0x457300
void __cdecl CommuTail_GameDispatch(void);      // 0x457340
void __cdecl CommuTail_EndAfterLoad(void);      // 0x457350
void __cdecl CommuTail24_Dispatch(void);        // 0x457370: Field_ModeTailKinds[24]
void __cdecl CommuTail24_LoadDat(void);         // 0x457380
void __cdecl CommuTail25_Dispatch(void);        // 0x4573B0: Field_ModeTailKinds[25]
void __cdecl CommuTail_NibbleGift(void);        // 0x4573C0
void __cdecl CommuTail25_LoadDat(void);         // 0x4574E0
void __cdecl CommuTail26_Dispatch(void);        // 0x457510: Field_ModeTailKinds[26]
void __cdecl CommuTail26_LoadDat(void);         // 0x457520
void __cdecl CommuTail26_End(void);             // 0x457570
void __cdecl CommuTail60_Stream(void);          // 0x457590: Field_ModeTailKinds[60]

// --- the board ------------------------------------------------------------------
void __cdecl CommuBoard_Dispatch(void);         // 0x457640: CommuTail14_States[3]
void __cdecl CommuBoard_Init(void);             // 0x457650
void __cdecl CommuBoard_Grid(void);             // 0x457680
void __cdecl CommuBoard_ModeDispatch(void);     // 0x457780
void __cdecl CommuBoard_StepDispatch(void);     // 0x457790
void __cdecl CommuBoard_PickRecord(void);       // 0x4577A0
void __cdecl CommuBoard_MoveRecord(void);       // 0x457990
void __cdecl CommuBoard_StepDispatchB(void);    // 0x457AD0
void __cdecl CommuBoard_PickRecordB(void);      // 0x457AE0
void __cdecl CommuBoard_MoveRecordB(void);      // 0x457CE0
void __cdecl CommuBoard_PickList(void);         // 0x457DD0
void __cdecl CommuBoard_PickListB(void);        // 0x457FE0
void __cdecl CommuBoard_PickListC(void);        // 0x458240
void __cdecl CommuBoard_Confirm(void);          // 0x458410

// --- the board's draws and helpers --------------------------------------------------
void __cdecl CommuBoard_DrawRecordCard(int x, int y, unsigned record, unsigned highlight);   // 0x458830
void __cdecl CommuBoard_DrawCardFrame(int x, int y);                                         // 0x458AE0
void __cdecl CommuBoard_DrawBar(int x, int y, int w, unsigned row, unsigned flash);          // 0x458B90
void __cdecl CommuBoard_DrawPanel(void);                                                     // 0x458D70
void __cdecl CommuBoard_DrawPanelFrame(int x, int y);                                        // 0x4591A0
void __cdecl CommuBoard_DrawSlotLines(unsigned slot, unsigned steady);                       // 0x459250
unsigned char __cdecl Commu_CountInSlot(unsigned slot);                                      // 0x459430
unsigned __cdecl Commu_NthInSlot(unsigned slot, unsigned nth);                               // 0x459460
void __cdecl CommuBoard_MoveGridCursor(unsigned char* cell);                                 // 0x4594A0
void __cdecl CommuBoard_DrawDigits(int x, int y, unsigned number);                           // 0x459720
void __cdecl CommuBoard_DrawSprite(int x, int y, unsigned id);                               // 0x4597E0
void __cdecl CommuBoard_DrawListBox(int x, int y, unsigned steady);                          // 0x4598A0
void __cdecl CommuBoard_DrawListFrame(int x, int y, unsigned h);                             // 0x459960
int __cdecl CommuBoard_ListY(void);                                                          // 0x459A30
unsigned short __cdecl CommuBoard_DrawListBoxB(int x, int y, unsigned list, unsigned steady);  // 0x459A80
void __cdecl CommuBoard_SlotRecordXY(short* x, short* y, unsigned slot, unsigned k);         // 0x459B70
void __cdecl CommuBoard_CancelStep(void);                                                    // 0x459C40
unsigned char __cdecl CommuBoard_PlaceRecord(unsigned back);                                 // 0x459C80
void __cdecl CommuBoard_SlotHelp(void);                                                      // 0x459D80
void __cdecl CommuBoard_PickHelp(unsigned char* cell);                                       // 0x459E50

}  // extern "C"

void Rest4B_Inject();

namespace rest_4b {
void SelfTest();   // rest_4b_fuzz.cpp: BOF3X_SHADOW=rest_4b
}  // namespace rest_4b
