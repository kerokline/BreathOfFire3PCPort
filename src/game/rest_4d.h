// Group R4D (round fourteen, wave four): two of the community's games and the
// board helpers a third one draws with - originals 0x45C400..0x45E86E, the
// cut's 60 functions. docs/rest_4d.md.
//
// The games are entries 7 and 8 of R4B's table 0x652A70 (by 0x9039F4):
// CommuDraw (a draw of names at random, its title, two music changes) and
// CommuName (a slot's or a character record's five-byte name, drawn at random
// or entered). Each dispatches on 0x939A3E, then 0x939A40, then 0x939A3F
// through .data tables; ours aborts on an index past its table's count
// (section 3), which no state writes. Most are void with no arguments; the
// draw helpers take cdecl words. All cdecl.
#pragma once

extern "C" {

// --- the board R4C's states draw (0x675F98: five bytes a row) ------------------------
void __cdecl CommuBoard_DrawRows(int x, int y, unsigned shown);                   // 0x45C400
void __cdecl CommuBoard_DrawFrame(int x, int y);                                  // 0x45C700
void __cdecl CommuBoard_DrawRowCells(int x, int y, unsigned row, unsigned lift);  // 0x45C7D0
void __cdecl CommuBoard_DrawCells(int x, int y, unsigned shown);                  // 0x45C850

// --- CommuDraw: 0x652A70[7] (CommuDraw_States, by 0x939A3E) ----------------------------
void __cdecl CommuDraw_Dispatch(void);          // 0x45C8C0: 0x652A70[7]
void __cdecl CommuDraw_OpenStep(void);          // 0x45C8D0: state 0
void __cdecl CommuDraw_FadeOut(void);           // 0x45C8E0: state 0, step 0
void __cdecl CommuDraw_MusicIn(void);           // 0x45C900: state 0, step 1
void __cdecl CommuDraw_ShowStep(void);          // 0x45C950: state 1
void __cdecl CommuDraw_Pick(void);              // 0x45C960: state 1, step 0
void __cdecl CommuDraw_Reveal(void);            // 0x45CC50: step 1
void __cdecl CommuDraw_WaitKey(void);           // 0x45CDC0: step 2
void __cdecl CommuDraw_MusicBack(void);         // 0x45CDF0: step 3
void __cdecl CommuDraw_Close(void);             // 0x45CE40: step 4
unsigned char __cdecl CommuDraw_RandBelow(unsigned limit);   // 0x45CE80: Rand & 0x7F brought below the byte limit
void __cdecl CommuDraw_DrawTitle(void);         // 0x45CEA0

// --- CommuName: 0x652A70[8] (CommuName_States, by 0x939A3E) ------------------------------
void __cdecl CommuName_Dispatch(void);          // 0x45D040: 0x652A70[8]
void __cdecl CommuName_Begin(void);             // 0x45D050: state 0
void __cdecl CommuName_SlotStep(void);          // 0x45D080: state 1
void __cdecl CommuName_PanelReset(void);        // 0x45D090: step 0 of states 1 and 2
void __cdecl CommuName_SlotPanelIn(void);       // 0x45D0D0
void __cdecl CommuName_SlotChoose(void);        // 0x45D170
void __cdecl CommuName_SlotConfirm(void);       // 0x45D290
void __cdecl CommuName_SlotPanelOut(void);      // 0x45D3E0
void __cdecl CommuName_SlotHow(void);           // 0x45D480
void __cdecl CommuName_SlotRandomStep(void);    // 0x45D4D0: step 6
void __cdecl CommuName_SlotRandomPick(void);    // 0x45D4E0
void __cdecl CommuName_SlotRandomAsk(void);     // 0x45D550
void __cdecl CommuName_SlotRandomOut(void);     // 0x45D5B0
void __cdecl CommuName_SlotEntryStep(void);     // 0x45D600: step 7
void __cdecl CommuName_SlotEntryIn(void);       // 0x45D610
void __cdecl CommuName_SlotEntry(void);         // 0x45D730
void __cdecl CommuName_SlotEntryOut(void);      // 0x45D7C0
void __cdecl CommuName_SlotEntryAsk(void);      // 0x45D930
void __cdecl CommuName_SlotEntryDone(void);     // 0x45D990
void __cdecl CommuName_SlotClose(void);         // 0x45D9E0: step 8
void __cdecl CommuName_MemberStep(void);        // 0x45DAA0: state 2
void __cdecl CommuName_MemberPanelIn(void);     // 0x45DAB0
void __cdecl CommuName_MemberChoose(void);      // 0x45DB90
void __cdecl CommuName_MemberConfirm(void);     // 0x45DD10
void __cdecl CommuName_MemberPanelOut(void);    // 0x45DE60
void __cdecl CommuName_MemberHow(void);         // 0x45E000
void __cdecl CommuName_MemberRandomStep(void);  // 0x45E050: step 6
void __cdecl CommuName_MemberRandomPick(void);  // 0x45E060
void __cdecl CommuName_MemberRandomAsk(void);   // 0x45E0D0
void __cdecl CommuName_MemberRandomOut(void);   // 0x45E130
void __cdecl CommuName_MemberEntryStep(void);   // 0x45E180: step 7
void __cdecl CommuName_MemberEntryIn(void);     // 0x45E190
void __cdecl CommuName_MemberEntry(void);       // 0x45E2C0
void __cdecl CommuName_MemberEntryOut(void);    // 0x45E350
void __cdecl CommuName_MemberEntryAsk(void);    // 0x45E4C0
void __cdecl CommuName_MemberEntryDone(void);   // 0x45E520
void __cdecl CommuName_MemberClose(void);       // 0x45E570: step 8
void __cdecl CommuName_End(void);               // 0x45E670: state 3
void __cdecl Commu_LeaveWhenClosed(void);       // 0x45E6A0: the last state of four tables
unsigned char __cdecl CommuName_CountSlots(void);       // 0x45E6B0: the cells in use (al)
unsigned __cdecl CommuName_NthSlot(unsigned n);         // 0x45E6D0: the n-th in use (eax, 0xFF none)
void __cdecl CommuName_DrawSlotBar(int x, int y);       // 0x45E700
void __cdecl CommuName_DrawSlotFrame(int x, int y);     // 0x45E770
void __cdecl CommuName_DrawHeader(void);                // 0x45E820

}  // extern "C"

namespace rest_4d {
void SelfTest();           // rest_4d_fuzz.cpp
void EntryAbandonTest();   // rest_4d_fuzz.cpp: DIV-0075's row, run once the switch is set
}
void Rest4D_Inject();
