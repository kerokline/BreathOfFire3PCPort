// Internal to menu_windows.cpp and menu_windows_fuzz.cpp: every call the
// menu and shop windows make, through pointers, so that the start-up fuzz can
// stand recording functions in for them - for the originals' copies and for
// ours alike. Many callees are ours in this module (the box, the pieces, the
// small fonts); through the pointers each function is still tested alone.
// docs/menu-windows.md.
//
// The callees of other groups of the sixth round are called by raw address
// (docs/takeover-queue-round6.md, "The rule for calls across groups"): the
// stats and inventory helpers of group W (0x5903F0, 0x5905D0, 0x5917A0,
// 0x5918E0, 0x5919B0, 0x590960, 0x591680, 0x591720, 0x591810, 0x591A80),
// the shop prices of group X (0x5830D0, 0x583100) and group M's PSX setter
// 0x5A7670.
#pragma once

#include <cstdint>

namespace menu_windows {

namespace at {

constexpr std::uint32_t kColour = 0x903A5A;       // s8: the window colour (Config), a CLUT row
constexpr std::uint32_t kCharRecords = 0x903A70;  // CharacterRecords, stride 0xA4
constexpr std::uint32_t kCharStride = 0xA4;
constexpr std::uint32_t kPrintBuf = 0x904BA0;     // the sprintf buffer every number goes through
constexpr std::uint32_t kGold = 0x904058;         // u32: the party's money
constexpr std::uint32_t kPartyLists = 0x904062;   // the party's 3-byte lists (item_use_callees.h)
constexpr std::uint32_t kMemberRecord = 0x66972C; // u8 per party entry: its record (MoveScript_EffectState)
constexpr std::uint32_t kMenuMember = 0x929F06;   // s8: the member the field menu is on
constexpr std::uint32_t kYesNo = 0x929F0B;        // u8: Menu_YesNo's selection, 1 Yes
constexpr std::uint32_t kLeaderX = 0x802D76;      // u16: the leader's map x (ObjTrio +0x36)
constexpr std::uint32_t kLeaderZ = 0x802D7A;      // u16: the leader's map z (+0x3A)
constexpr std::uint32_t kClutShadow = 0x80B7A8;   // 64 bytes a CLUT row: entry 0 is the window's line colour
// Named in symbols.toml, whose names are macros here (by address).
constexpr std::uint32_t kFrameCounter = 0x937F94;   // Frame_Counter
constexpr std::uint32_t kInputPressed = 0x7E1BEC;   // Input_Pressed
constexpr std::uint32_t kConfirmButtons = 0x90358E; // Field_ConfirmButtons
constexpr std::uint32_t kCancelButtons = 0x903590;  // Field_CancelButtons
constexpr std::uint32_t kInputFlags = 0x905BA2;     // Field_InputFlags
constexpr std::uint32_t kCondFA = 0x8034E0;         // Cond_ByteFA
constexpr std::uint32_t kButtonSets = 0x66383C;     // Menu_ButtonSets
constexpr std::uint32_t kVerbPointers = 0x6637E4;   // Menu_VerbPointers

// MsgBoxState (symbols.toml block MsgBoxState, window_task_callees.h).
constexpr std::uint32_t kMessage = 0x7DEE48;      // u16: the message index
constexpr std::uint32_t kBoxText = 0x7DEE4C;      // the box string base
constexpr std::uint32_t kListText = 0x7DEE50;     // the stepper pointer, the list's text
constexpr std::uint32_t kListSet = 0x7DEE65;      // u8: the list's geometry set
constexpr std::uint32_t kListCount = 0x7DEE66;    // s8: the last item's index
constexpr std::uint32_t kCurrent = 0x905B84;      // the window record Field_RunTaskRecords is running
constexpr std::uint32_t kSetXY = 0x66AE2C;        // 6-byte records by set: s16 x, s16 y, s16 y stride

// Tables in .data, read as the originals read them.
constexpr std::uint32_t kInventory = 0x656B00;    // 5 pointers: each category's item ids
constexpr std::uint32_t kCounts = 0x656B14;       // 5 pointers: each category's counts
constexpr std::uint32_t kItemFlags = 0x656B38;    // consumables' flags byte, stride 22
constexpr std::uint32_t kPieceRects = 0x663B94;   // Menu_DrawPiece's rectangles (u, v, w, h)
constexpr std::uint32_t kPieceRectsAlt = 0x663C8C;// the same with flag bit 0
constexpr std::uint32_t kIconRects = 0x66367C;    // Menu_DrawIcon's (u, v, clut x << 4, clut y - 0x1E0)
constexpr std::uint32_t kExpTable = 0x658F48;     // u16 at stride 8, 99 a member (0x318 bytes)
constexpr std::uint32_t kBackdropRects = 0x663874;// the backdrop's tile rectangles
constexpr std::uint32_t kBackdropPattern = 0x663920;
constexpr std::uint32_t kBackdropStart = 0x66396C;// 4 bytes: each kind's start in the pattern
constexpr std::uint32_t kCategoryLabels = 0x663970;
constexpr std::uint32_t kRowIcons = 0x663D60;     // an item kind's 8x8 icon (Menu_DrawItemRow)
constexpr std::uint32_t kBuyIcons = 0x66B29C;     // the same table, three more times
constexpr std::uint32_t kBuyDetailIcons = 0x66B2AC;
constexpr std::uint32_t kSellIcons = 0x66B2C8;

// The strings the draws name (all in .rdata, read 2026-09-23).
constexpr std::uint32_t kFmt7d = 0x64ADDC;        // "%7d"
constexpr std::uint32_t kFmt3d = 0x64E324;        // "%3d"
constexpr std::uint32_t kFmt3d3d = 0x6639C8;      // "%3d/%3d"
constexpr std::uint32_t kFmtCount = 0x653EC0;     // "*%2d"
constexpr std::uint32_t kFmt6dZ = 0x66B4A0;       // "%6dZ"
constexpr std::uint32_t kFmt7dZ = 0x66B4A8;       // "%7dZ"
constexpr std::uint32_t kEquipLabel = 0x663668;   // "EQUIP"
constexpr std::uint32_t kMoneyLabel = 0x66A31C;
constexpr std::uint32_t kStatLabels[3] = {0x66A0F8, 0x66A100, 0x66A110};

}  // namespace at

// Other groups' functions, by address (the round's rule).
constexpr std::uint32_t kStatIcon = 0x5903F0;      // (kind, x, y, w, h, shade), W
constexpr std::uint32_t kDrawHand = 0x5905D0;      // Menu_DrawHand (x, y, unused), W
constexpr std::uint32_t kEquipCompare = 0x590960;  // (member, slot, item, u8 out[4], u16 out[4]), W
constexpr std::uint32_t kItemName = 0x591680;      // (category, id) -> name, W
constexpr std::uint32_t kItemKind = 0x591720;      // (category, id) -> 0..15, W
constexpr std::uint32_t kEquipMask = 0x5917A0;     // (category, id) -> the members who can equip it, W
constexpr std::uint32_t kItemFlagsOf = 0x591810;   // (category, id) -> flags, W
constexpr std::uint32_t kHasKeyItem = 0x5918E0;    // (id) -> 1 when held, W
constexpr std::uint32_t kCountOwned = 0x5919B0;    // (category, id, where) -> u16, W
constexpr std::uint32_t kCountCategory = 0x591A80; // (category) -> u8, W
constexpr std::uint32_t kPriceScale = 0x5830D0;    // (price, percent) -> price * percent / 100, at least 1, X
constexpr std::uint32_t kSellPrice = 0x583100;     // (category, id, flag) -> the price a shop pays, X
constexpr std::uint32_t kSetLineF3 = 0x5A7670;     // the PSX setter (prim), M

struct Callees {
    // other modules' (ours) and Capcom's
    const unsigned char* (__cdecl* text_draw_at)(int, int, int, int, const unsigned char*);   // Text_DrawAt
    // Text_DrawAt, or MenuVerbs_DrawLabel under DIV-0018 (Menu_DrawButtonRow's label only)
    const unsigned char* (__cdecl* verb_label)(int, int, int, int, const unsigned char*);
    const unsigned char* (__cdecl* text_immediate)(int, int, const unsigned char*);           // Text_DrawImmediate
    const unsigned char* (__cdecl* msg_system_ptr)(unsigned);                                 // Msg_SystemPtr
    // Msg_SystemPtr, or YesNo_Line under DIV-0027 (Menu_YesNo's line only)
    const unsigned char* (__cdecl* yes_no_line)(unsigned);
    void (__cdecl* msgbox_reset)();                                                           // MsgBox_Reset
    void (__cdecl* sound)(unsigned);                                                          // Sound_PlayEffect
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);             // Gpu_SetDrawMode
    void (__cdecl* commit)(unsigned, unsigned);                                               // Gfx_CommitPrim
    void (__cdecl* set_sprt)(unsigned char*);                                                 // Gpu_SetSprt
    void (__cdecl* set_semi)(unsigned char*, unsigned);                                       // Gpu_SetSemiTrans
    unsigned (__cdecl* get_tpage)(unsigned, unsigned, int, int);                              // Gpu_GetTPage
    unsigned (__cdecl* get_clut)(int, int);                                                   // Gpu_GetClut
    void (__cdecl* set_poly_ft4)(unsigned char*);                                             // Gpu_SetPolyFT4
    void (__cdecl* set_line_f2)(unsigned char*);                                              // Gpu_SetLineF2
    void (__cdecl* set_line_f3)(unsigned char*);                                              // 0x5A7670, M's
    void (__cdecl* set_line_f4)(unsigned char*);                                              // Gpu_SetLineF4
    void (__cdecl* set_tile)(unsigned char*);                                                 // Gpu_SetTile
    unsigned char (__cdecl* area_byte)(int, int);                                             // AreaMap_ByteAt
    int (__cdecl* sprintf_)(char*, const char*, ...);                                         // Crt_sprintf
    // group W's and X's
    void (__cdecl* stat_icon)(unsigned, int, int, unsigned, unsigned, unsigned);
    void (__cdecl* draw_hand)(int, int, int);
    void (__cdecl* equip_compare)(unsigned, unsigned, unsigned, unsigned char*, unsigned short*);
    const unsigned char* (__cdecl* item_name)(unsigned, unsigned);
    unsigned (__cdecl* item_kind)(unsigned, unsigned);
    unsigned (__cdecl* equip_mask)(unsigned, unsigned);
    unsigned (__cdecl* item_flags)(unsigned, unsigned);
    unsigned (__cdecl* has_key_item)(unsigned);
    unsigned (__cdecl* count_owned)(unsigned, unsigned, unsigned);
    unsigned (__cdecl* count_category)(unsigned);
    unsigned (__cdecl* price_scale)(unsigned, unsigned);
    unsigned (__cdecl* sell_price)(unsigned, unsigned, unsigned);
    // ours, in this module
    void (__cdecl* font12)(int, int, int, const unsigned char*);                              // Text_DrawFont12
    void (__cdecl* font8)(int, int, int, const unsigned char*);                               // Text_DrawFont8
    void (__cdecl* panel)(int, int, int, int);                                                // Menu_DrawPanel
    void (__cdecl* box)(int, int, int, int, int, int);                                        // Menu_DrawBox
    void (__cdecl* icon8)(int, int, int, int);                                                // Menu_DrawIcon8
    unsigned char (__cdecl* char_count)(const unsigned char*);                                // Text_CharCount
    void (__cdecl* pieces)(int, int, const unsigned char*, int);                              // Menu_DrawPieces
    void (__cdecl* piece)(int, int, unsigned, unsigned);                                      // Menu_DrawPiece
    const unsigned char* (__cdecl* piece_rect)(unsigned, unsigned);                           // Menu_PieceRect
    void (__cdecl* line)(int, int, int, int, int, int, int, int);                             // Menu_DrawLine
    void (__cdecl* outline)(int, int, int, int, int);                                         // Menu_DrawOutline
    void (__cdecl* item_icon)(int, int, int, int);                                            // Menu_DrawItemIcon
    void (__cdecl* icon)(int, int, int, int);                                                 // Menu_DrawIcon
    int (__cdecl* exp_for_level)(unsigned, unsigned);                                         // Char_ExpForLevel
    unsigned (__cdecl* item_price)(unsigned, unsigned);                                       // Item_Price
    unsigned char (__cdecl* can_use)(unsigned, unsigned, unsigned, unsigned);                 // Item_CanUse
    void (__cdecl* item_row)(int, int, int, unsigned, unsigned, unsigned, int);               // Menu_DrawItemRow
    void (__cdecl* scroll_bar)(const unsigned char*, unsigned, int, int, unsigned, unsigned, unsigned);  // Menu_DrawScrollBar
    unsigned char (__cdecl* list_scroll)(unsigned char*, unsigned char*, unsigned char*, unsigned char*);  // Menu_ListScroll
    void (__cdecl* border)(int, int, int, int);                                               // Menu_DrawBorder
    void (__cdecl* set_sprt8)(unsigned char*);                                                // Gpu_SetSprt8
};

extern const Callees kOriginals;
extern Callees g;

// DIV-0027's hand stops (yes_no_layout.cpp): set at inject time when that
// module's bytes went in, so that ours places the hand as the patched
// original does.
extern bool g_yes_no_div;

// BOF3X_SHADOW=menu_windows: the start-up fuzz, menu_windows_fuzz.cpp. Clones
// every original before MenuWindows_Inject patches it.
void SelfTest();

}  // namespace menu_windows
