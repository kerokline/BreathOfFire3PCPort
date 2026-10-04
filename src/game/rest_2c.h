// Group R2C (round fourteen, wave two): the inn's, save point's and rest's
// last states, the save block's builder, the shop's browse and sell modes,
// the master's talk and its panels, and the figure record's moves - originals
// 0x57F340..0x58699F, 61 functions (the cut's 60 and ShopMode_States[9]
// 0x583350, which no list had). docs/rest_2c.md.
//
// Most are states of a .data table: void, no arguments, reading the menu
// block (0x929F00 mode, +1 state, +2 step, +4 timer), the master's bytes
// (0x9398CF mode, 0x9398D1 step) or the figure record 0x9398E0. The
// dispatchers jump through their table by that byte; ours aborts on an index
// past the table's own count (section 3), which no state writes. The draw
// helpers take cdecl words. All cdecl.
#pragma once

extern "C" {

// --- the figure record 0x9398E0 (MasterFigure_States, by +1) -------------------
void __cdecl MasterFigure_DrawFaded(void);      // 0x57F340: drawn with its colour faded by +0x107
void __cdecl MasterFigure_TurnHome(void);       // 0x57F420: the angle +0x6C 0x40 toward 0
void __cdecl MasterFigure_Settle(void);         // 0x57F450: the height +0x3C toward the ground's
void __cdecl MasterFigure_Hold(void);           // 0x57F4E0: drawn, nothing moved
void __cdecl MasterFigure_TurnOn(void);         // 0x57F4F0: the angle + 0x20

// --- the inn, the save point, the rest -------------------------------------------
void __cdecl InnPrompt_NotEnough(void);         // 0x57FA40: InnPrompt_States[4]
void __cdecl FieldSave_Write(void);             // 0x580010: FieldSave_States[4]
void __cdecl FieldSave_Written(void);           // 0x5800D0: FieldSave_States[5]
void __cdecl FieldSave_PromptAnswer(void);      // 0x580230: FieldSave_States[8]
void __cdecl Inn_TitleOut(void);                // 0x580280: Inn_Steps[4]
void __cdecl Rest_Dispatch(void);               // 0x580300: ShopMode_States[10]
void __cdecl Rest_LoadJingle(void);             // 0x580560: Rest_States[2]
void __cdecl Rest_WaitJingle(void);             // 0x580590: Rest_States[3]
void __cdecl Rest_Restore(void);                // 0x5805D0: Rest_States[4]
void __cdecl Rest_End(void);                    // 0x580600: Rest_States[5]
void __cdecl Rest_EndAfterMessage(void);        // 0x580610: Rest_States[6]
void __cdecl Save_BuildBlock(void);             // 0x5806F0: the block, its staging copy, the slot's summary
// Save_QuickWrite (0x5809C0) is declared by symbols.gen.h.

// --- the shop's modes --------------------------------------------------------------
void __cdecl PartyForm_Dispatch(void);          // 0x580A40: ShopMode_States[6]
void __cdecl ShopSell_Dispatch(void);           // 0x582EB0: ShopMode_States[8]
void __cdecl ShopSell_Open(void);               // 0x582EC0: ShopSell_States[0]
void __cdecl ShopSell_Leave(void);              // 0x582FB0: ShopSell_States[1]
void __cdecl ShopSell_SellStep(void);           // 0x582FF0: ShopSell_States[2]
void __cdecl ShopSell_LeaveWait(void);          // 0x583000: ShopSell_States[3]
void __cdecl ShopBrowse_Dispatch(void);         // 0x583350: ShopMode_States[9]
void __cdecl ShopBrowse_OpenStep(void);         // 0x583360: ShopBrowse_States[0]
void __cdecl ShopBrowse_Open(void);             // 0x583370
void __cdecl ShopBrowse_OpenWait(void);         // 0x5833A0
void __cdecl ShopBrowse_ChooseStep(void);       // 0x5833D0: ShopBrowse_States[1]
void __cdecl ShopBrowse_Choose(void);           // 0x5833E0
void __cdecl ShopBrowse_Detail(void);           // 0x5835F0
void __cdecl ShopBrowse_DetailClose(void);      // 0x583680
void __cdecl ShopBrowse_CloseStep(void);        // 0x5836A0: ShopBrowse_States[2]
void __cdecl ShopBrowse_CloseNext(void);        // 0x5836B0
void __cdecl ShopBrowse_End(void);              // 0x5836C0
void __cdecl ShopResist_Dispatch(void);         // 0x5837E0: ShopMode_States[5]
void __cdecl SharedList_Dispatch(void);         // 0x584180: ShopMode_States[7]

// --- the master's talk (MasterTalk_States, by 0x9398CF) ------------------------------
void __cdecl MasterTalk_Reset(void);            // 0x585A00: FieldTail_LoadBank's step 1
void __cdecl MasterTalk_PanelsOpen(void);       // 0x585A20
void __cdecl MasterTalk_PanelsIn(void);         // 0x585A50
void __cdecl MasterTalk_PanelsOut(void);        // 0x585B20
void __cdecl MasterTalk_Dispatch(void);         // 0x586670: FieldTail_LoadBank's step 2 (a tail jump)
void __cdecl MasterTalk_Begin(void);            // 0x586680: state 0
void __cdecl MasterTalk_IntroStep(void);        // 0x5866D0: state 1
void __cdecl MasterTalk_IntroSay(void);         // 0x5866E0
void __cdecl MasterTalk_IntroWait(void);        // 0x586720
void __cdecl MasterTalk_AskStep(void);          // 0x586750: state 2
void __cdecl MasterTalk_Say5(void);             // 0x586760
void __cdecl MasterTalk_Say6(void);             // 0x586790
void __cdecl MasterTalk_CheckAllPupils(void);   // 0x5867D0
void __cdecl MasterTalk_Say8(void);             // 0x586860
void __cdecl MasterTalk_CheckAnyPupil(void);    // 0x5868A0
void __cdecl MasterTalk_SayFarewell(void);      // 0x586930
void __cdecl MasterTalk_PickStep(void);         // 0x586970: state 3
void __cdecl MasterTalk_PickAsk(void);          // 0x586980

// --- the panels ---------------------------------------------------------------------
void __cdecl MasterPanel_DrawStats(int x, int y, unsigned record);                  // 0x585BE0
void __cdecl MasterPanel_DrawMember(int x, int y, unsigned record, unsigned slot);  // 0x585DC0
void __cdecl MasterPanel_DrawExpBar(int x, int y, unsigned record, unsigned level, unsigned exp);   // 0x586030
int __cdecl MasterPanel_ExpForLevel(unsigned record, unsigned level);               // 0x586110
void __cdecl Menu_DrawPanelBox(int x, int y, int w, int h, unsigned colour);        // 0x586160
void __cdecl MasterPanel_DrawFace(int x, int y, unsigned id, unsigned shade);       // 0x586570

}  // extern "C"

namespace rest_2c {
void SelfTest();   // rest_2c_fuzz.cpp
}
void Rest2C_Inject();
