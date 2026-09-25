// Internal to menu_lists.cpp and menu_lists_fuzz.cpp: every call the field
// menu and the list draws make, through pointers, so that the start-up fuzz
// can stand recording functions in for them - for the originals' copies and
// for ours alike. docs/menu_lists.md.
//
// Eight of the twenty dispatch through a code-pointer table in .data, read
// afresh and unchecked, as the originals do (`jmp` / `call [index * 4 +
// table]`): FieldMenu_Run through 0x6672B4 by the menu state, FieldMenu_TopBar
// through 0x6672D8 by the step, MenuList_Run through 0x66AF94 by the window
// record's kind, and the five kinds through their state tables by the
// record's state. Those tables are not in Callees: the fuzz swaps their
// entries in .data for recorders, which both sides then reach.
//
// Addresses of other groups' functions (the round's cross-group rule,
// docs/takeover-queue-round8.md) are raw here and never bound by name.
#pragma once

#include <cstdint>

namespace menu_lists {

namespace at {

// The field menu's state block (docs/menu-screens.md section 1).
constexpr std::uint32_t kMenu = 0x929F00;          // u8 state (the dword's low byte picks FieldMenu_Run's entry)
constexpr std::uint32_t kStep = 0x929F01;          // u8 step inside a state
constexpr std::uint32_t kCountdown = 0x929F04;     // u8 the open / close countdown
constexpr std::uint32_t kCursor = 0x929F05;        // u8 the top bar's cursor, 0..6
constexpr std::uint32_t kOpenedAt = 0x929F10;      // u8: 1 opens the menu straight on state 5, step 5
constexpr std::uint32_t kPartyChanged = 0x929F11;  // u8: set when a member of the party left it
constexpr std::uint32_t kMemberCount = 0x929EC0;   // Field_MemberCount (u8)
// The party (area_entry_callees.h, battle_draw_callees.h): three bytes of
// characters, then three of the reserve; 0xFF is none.
constexpr std::uint32_t kParty = 0x904062;
constexpr std::uint32_t kReserve = 0x904065;
constexpr std::uint32_t kReserveData = 0x9045FC;   // 3 bytes a reserve slot, kept with its member
constexpr std::uint32_t kMoney = 0x904058;         // dword, Menu_DrawMoneyBox's value
constexpr std::uint32_t kCampFlag = 0x904152;      // u8 (hypothesis: "camping allowed anywhere")
constexpr std::uint32_t kMemberRecord = 0x66972C;  // u8 by character: its record (MoveScript_EffectState)
constexpr std::uint32_t kCharRecords = 0x903A70;   // 0xA4 bytes each
constexpr std::uint32_t kCharRecordSize = 0xA4;
constexpr std::uint32_t kColour = 0x903A5A;        // Config's window colour byte
constexpr std::uint32_t kBackground = 0x903A5B;    // Config's "Background" byte
// The party as the menu opened on it (FieldMenu_Open), for
// FieldMenu_ReconcileParty to compare the party it closes on against.
constexpr std::uint32_t kSavedParty = 0x6BDF98;
constexpr std::uint32_t kSavedReserve = 0x6BDF9B;
// The window records (WindowRecords, docs/window-task.md section 3).
constexpr std::uint32_t kRecords = 0x803160;
constexpr std::uint32_t kRecordSize = 0x24;
constexpr std::uint32_t kRecordsEnd = 0x803478;
constexpr std::uint32_t kCurrent = 0x905B84;       // dword: the record the window layer is running
constexpr std::uint32_t kMemberPanels = 0x8031F0;  // record 4: the first member panel
constexpr std::uint32_t kMoneyWindow = 0x80325C;   // record 7
constexpr std::uint32_t kTimeWindow = 0x803280;    // record 8
constexpr std::uint32_t kIconWindow = 0x8032A4;    // record 9, the top bar's icons
constexpr std::uint32_t kTitleWindow = 0x8032C8;   // record 10, the screen title
constexpr std::uint32_t kObjTrio = 0x802D40;       // the party's working records; Sprite_Current for the gateway test
// Field_InputFlags, Game_AreaNumber, Game_Step, Input_Pressed and the
// confirm / cancel words are bound in symbols.gen.h; read here by address
// through the same helpers as the rest.
constexpr std::uint32_t kInputFlags = 0x905BA2;
constexpr std::uint32_t kArea = 0x904EFC;
constexpr std::uint32_t kGameStep = 0x66C7EA;
constexpr std::uint32_t kPressed = 0x7E1BEC;
constexpr std::uint32_t kConfirm = 0x90358E;
constexpr std::uint32_t kCancel = 0x903590;
constexpr std::uint32_t kCampChosen = 0x905B60;    // u8 = 1 when Camp is taken
// The .data tables (symbols.toml [[data]]).
constexpr std::uint32_t kStates = 0x6672B4;        // FieldMenu_States, 9
constexpr std::uint32_t kTopBarSteps = 0x6672D8;   // FieldMenu_TopBarSteps, 3
constexpr std::uint32_t kTitleIds = 0x6672E4;      // FieldMenu_TitleIds, 7 bytes
constexpr std::uint32_t kIconIds = 0x6672AC;       // FieldMenu_IconIds, 7 bytes
constexpr std::uint32_t kKinds = 0x66AF94;         // MenuList_Kinds, 21
constexpr std::uint32_t kPanelStates = 0x66AFE8;   // MenuList_PanelStates, 11
constexpr std::uint32_t kMoneyStates = 0x66B014;   // MenuList_MoneyStates, 3
constexpr std::uint32_t kTimeStates = 0x66B020;    // MenuList_TimeStates, 3
constexpr std::uint32_t kIconStates = 0x66B02C;    // MenuList_IconStates, 3
constexpr std::uint32_t kTitleStates = 0x66B038;   // MenuList_TitleStates, 3
// The two slide bounds DIV-0041 (widescreen.cpp, kSlides) patches in the
// originals' code: read from there, so ours holds whatever went in.
constexpr std::uint32_t kLeftOffBound = 0x59A586;  // imm32 of `mov ecx, -200` in 0x59A580
constexpr std::uint32_t kRightOffBound = 0x59A5E6; // imm32 of `mov ecx, 0x140` in 0x59A5E0

}  // namespace at

// Callees with no name in symbols.gen.h: other groups' functions this round,
// or no group's, called by address.
constexpr std::uint32_t kExitGateway = 0x531820;   // u8(): an exit from the gateway tables (event_ops_callees.h); no group
constexpr std::uint32_t kCampCell = 0x589FB0;      // u8(): 1 unless ObjTrio's cell is 0xA0 / 0xA1 / 0xAF / 0x91 (AreaMap_ByteAt & 0xF0); no group
constexpr std::uint32_t kMemberBody = 0x573560;    // group DD's: a member's panel (x, y, record, flag, 0)
constexpr std::uint32_t kMemberFace = 0x5744B0;    // group DD's: an 8 x 8 cell SPRT (x, y, u / 8, v / 8, clut, shade)
constexpr std::uint32_t kTimeBox = 0x5746C0;       // group DD's: the play-time box (x, y)

struct Callees {
    // ours, in this file
    void (__cdecl* place_windows)();                        // FieldMenu_PlaceWindows 0x589E60
    void (__cdecl* reconcile)();                            // FieldMenu_ReconcileParty 0x589FE0
    void (__cdecl* draw_member)(unsigned char*);            // MenuList_DrawMemberPanel 0x599B90
    // other modules' and Capcom's
    void (__cdecl* backdrop)(unsigned);                     // Menu_DrawBackdrop
    void (__cdecl* recalc_stats)(unsigned char*);           // Char_RecalcStats
    int (__cdecl* party_count)(unsigned);                   // Party_Count
    unsigned char (__cdecl* exit_gateway)();                // 0x531820
    unsigned char (__cdecl* camp_cell)();                   // 0x589FB0
    unsigned (__cdecl* auto_repeat)(unsigned);              // Input_AutoRepeat
    void (__cdecl* sound)(unsigned short);                  // Sound_PlayEffect
    void (__cdecl* window_reset)();                         // Window_ResetAll
    void (__cdecl* member_body)(int, int, unsigned, unsigned, int);                        // 0x573560
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);        // Gpu_SetDrawMode
    void (__cdecl* commit)(unsigned, unsigned);                                           // Gfx_CommitPrim
    unsigned (__cdecl* get_clut)(int, int);                                               // Gpu_GetClut
    void (__cdecl* member_face)(int, int, unsigned, unsigned, unsigned, unsigned);        // 0x5744B0
    void (__cdecl* money_box)(int, int, int, unsigned);                                   // Menu_DrawMoneyBox
    void (__cdecl* time_box)(int, int);                                                   // 0x5746C0
    void (__cdecl* icon)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);     // Menu_DrawIcon
    void (__cdecl* title_box)(int, int, int, int, int);                                   // Menu_DrawTitleBox
    const unsigned char* (__cdecl* msg)(unsigned);                                        // Msg_SystemPtr
    unsigned char (__cdecl* char_count)(const unsigned char*);                            // Text_CharCount
    const unsigned char* (__cdecl* text)(int, int, int, int, const unsigned char*);       // Text_DrawAt
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (menu_lists_fuzz.cpp): clones all twenty with every call
// out re-aimed at a recording stand-in and every .data dispatch table's
// entries swapped for recorders, runs ours against the clones from the same
// random state, and ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace menu_lists
