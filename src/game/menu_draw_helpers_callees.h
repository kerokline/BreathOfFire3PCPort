// Internal to menu_draw_helpers.cpp and menu_draw_helpers_fuzz.cpp: the
// addresses the group's functions read that have no name of their own, and
// every call they make - through pointers, so that the start-up fuzz can stand
// recording functions in for them, for the originals' copies and for ours
// alike. Every callee is already ours (other modules'); none belongs to another
// group of the round.
//
// The dispatches are not calls to a named function: two `jmp [table + kind *
// 4]` stubs (record byte +2) and eleven `call [table + step * 4]` (record byte
// +3), all through .data tables named in symbols.toml ([[data]]). Ours reads
// the same .data dword at the moment the original does, unbounded as the
// original is; the fuzz swaps the tables' entries for recorders.
// docs/menu_draw_helpers.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace menu_draw_helpers {

using U = std::uint32_t;

namespace at {

constexpr U kRecords = 0x803160;     // 22 window records of 0x24 bytes (Window_Alloc 0x59E2D0)
constexpr U kRecordsEnd = 0x803478;
constexpr U kCurrent = 0x905B84;     // dword: the record Field_RunTaskRecords is running
constexpr U kColour = 0x903A5A;      // s8: the window colour (Config), a CLUT row
constexpr U kZenny = 0x904058;       // dword: the party's zenny
constexpr U kPartyList = 0x904062;   // u8 x 3: the party's members, in order
// u8 per member id: its character record (the byte item_use.cpp and
// battle_draw read; symbols.toml calls it MoveScript_EffectState, a
// hypothesis this group does not own - docs/menu_draw_helpers.md section 6).
constexpr U kMemberRecord = 0x66972C;
// The imm32 of MenuWin_SlideOutLeft's `mov ecx, 0xFFFFFF6A` (-150) at
// 0x59B445: the bound Widescreen_Inject (DIV-0041) widens in place. Ours reads
// it back, so the divergence survives the takeover.
constexpr U kSlideLeftMov = 0x59B445;
constexpr U kSlideLeftBound = 0x59B446;

}  // namespace at

// The dispatch tables (symbols.toml [[data]], by the name in each comment).
namespace table {
constexpr U kHandler7Kinds = 0x66B1F8;        // Window_Handler7KindTable, 19, by +2
constexpr U kHandler8Kinds = 0x66B534;        // Window_Handler8KindTable, 6, by +2
constexpr U kTitleSteps = 0x66B244;                 // ShopWin_TitleSteps, 3, by +3
constexpr U kButtonsSteps = 0x66B250;             // ShopWin_ButtonsSteps, 3
constexpr U kMoneySteps = 0x66B25C;                 // ShopWin_MoneySteps, 3
constexpr U kCursorWidths = 0x66B268;          // ShopWin_CursorBoxWidths, u16 x 2, by +0xA
constexpr U kMemberSteps = 0x66B26C;          // ShopWin_MemberStatsSteps, 5
constexpr U kEquipSteps = 0x66B280;                 // ShopWin_EquipSteps, 3
constexpr U kBuyListSteps = 0x66B28C;             // ShopWin_BuyListSteps, 4
constexpr U kItemListSteps = 0x66B2BC;           // ShopWin_ItemListSteps, 3
constexpr U kBattleItemSteps = 0x66B54C;   // BattleMenuWin_ItemListSteps, 5
constexpr U kBattleSkillSteps = 0x66B560; // BattleMenuWin_SkillListSteps, 3
}  // namespace table

struct Callees {
    void (__cdecl* title_box)(int, int, int, int, int);                      // Menu_DrawTitleBox 0x574AB0
    const unsigned char* (__cdecl* msg_system)(unsigned);                     // Msg_SystemPtr 0x497740
    const unsigned char* (__cdecl* text_draw_at)(int, int, int, int, const unsigned char*);  // Text_DrawAt 0x516B30
    void (__cdecl* button_row)(int, int, int, int, int);                     // Menu_DrawButtonRow 0x574890
    void (__cdecl* money_box)(int, int, int, unsigned);                      // Menu_DrawMoneyBox 0x574610
    void (__cdecl* cursor_box)(int, int, int, int, int, int);                // Menu_DrawCursorBox 0x573CE0
    void (__cdecl* member_stats)(int, int, unsigned, unsigned, unsigned);    // Shop_DrawMemberStats 0x575430
    void (__cdecl* equip_panel)(int, int, unsigned);                         // Menu_DrawEquipPanel 0x573A80
    void (__cdecl* buy_list)(unsigned char*);                                // Shop_DrawBuyList 0x59B580
    void (__cdecl* buy_detail)(unsigned char*);                              // Shop_DrawBuyDetail 0x59B820
    void (__cdecl* item_list)(unsigned char*);                               // Menu_DrawItemList 0x5759C0
    void (__cdecl* sell_detail)(unsigned char*);                             // Shop_DrawSellDetail 0x59BBC0
    void (__cdecl* hand)(int, int, int);                                     // Menu_DrawHand 0x5905D0
    void (__cdecl* battle_items)(unsigned char*);                            // BattleMenu_DrawItemList 0x59CD00
    void (__cdecl* battle_skills)(unsigned char*);                           // BattleMenu_DrawSkillList 0x59D200
    U slide_left_bound;   // where MenuWin_SlideOutLeft reads its bound: at::kSlideLeftBound
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=menu_draw_helpers: the start-up fuzz, menu_draw_helpers_fuzz.cpp.
// Clones every original before MenuDrawHelpers_Inject patches it.
void SelfTest();

}  // namespace menu_draw_helpers
