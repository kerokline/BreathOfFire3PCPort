// Internal to battle_draw.cpp and battle_draw_fuzz.cpp: the addresses group
// BJ's functions touch that have no name in symbols.toml, and every call they
// make - through pointers, so that the start-up fuzz can stand recording
// functions in for them, for the originals' copies and for ours alike.
// docs/battle_draw.md.
//
// The callees of other groups of the seventh round are called by raw address
// (docs/takeover-queue-round7.md, "The rule for calls across groups"): group
// BD's skill helpers 0x57DA70, 0x57DC90, 0x5918A0, 0x591DB0 and 0x591E50,
// under BattleMenu_DrawSkillList. Never bound or renamed here.
#pragma once

#include <cstdint>

namespace battle_draw {

using U = std::uint32_t;

// --- the Direct3D handlers (docs/d3d-draw.md section 3) ------------------------
constexpr U kScaleX = 0x7C9F4C;        // D3d_ScaleX, float
constexpr U kScaleY = 0x7C9F48;        // D3d_ScaleY, float
constexpr U kVertices = 0x7CA958;      // D3d_Vertices, 4 x D3DTLVERTEX
constexpr U kDrawTpage = 0x7DED14;     // Gfx_DrawTpage, the low word read
constexpr U kRhwNumerator = 0x5C4610;  // float 0.1: rhw = 0.1 / z
constexpr U kDevice = 0x7CC350;        // D3d_Device, an IDirect3DDevice3 *
constexpr U kRetOnly = 0x437CC0;       // a bare ret

// --- the battle menu's lists ---------------------------------------------------
constexpr U kColour = 0x903A5A;        // s8: the window colour (Config), a CLUT row
constexpr U kMenuMember = 0x929F06;    // s8: the member the menu is on
constexpr U kBattleParty = 0x904065;   // the second of the two 3-byte party lists (0x904062 the first)
constexpr U kMemberRecord = 0x66972C;  // u8 per party entry: its character record (MoveScript_EffectState)
constexpr U kInventory = 0x656B00;     // 5 pointers: each category's item ids
constexpr U kCounts = 0x656B14;        // 5 pointers: each category's counts (the fifth, key items, 0)
constexpr U kItemLabels = 0x66B58C;    // 5 pointers: the item list's title by category
constexpr U kSkillLabels = 0x66B5A0;   // 4 pointers: the skill list's title by kind (+0xB)
constexpr U kSkillLabelAlt = 0x66B5B0; // a pointer: the title while word +0x14 is set (0x66A220)
constexpr U kSkillRecords = 0x65C4C8;  // 0x18-byte records by skill id, the name first
constexpr U kPrintBuf = 0x904BA0;      // the sprintf buffer every number goes through
constexpr U kFmt3d3d = 0x6639C8;       // "%3d/%3d"
// Menu_DrawPieces lists (3-byte records to an id of 0xFF), chosen by two flag
// bits of the window record; the field's item list uses its own four
// (0x663484..0x6634A8).
constexpr U kTitleBit1Off = 0x66B4B0, kTitleBit1On = 0x66B4C8;   // by bit 1 of the flags byte
constexpr U kTitleBit0Off = 0x66B4BC, kTitleBit0On = 0x66B4D4;   // by bit 0

// --- the area tint ---------------------------------------------------------------
constexpr U kStatusBits = 0x8034E1;    // Field_StatusBits: bit 0 holds the tint off

// Group BD's (round 7), by address: never bound here.
constexpr U kSkillUsable = 0x57DA70;   // (mode, member, id) -> al: whether the row is usable
constexpr U kSkillRow = 0x57DC90;      // (x, y, colour, icon, record, cost, dim): one row
constexpr U kSkillIcon = 0x5918A0;     // (id) -> al: the row's icon kind (0x663D70[al])
constexpr U kSkillCost = 0x591DB0;     // (member, id, 1) -> al: the number drawn at x + 0x6D
constexpr U kSkillList = 0x591E50;     // (member, kind, battle) -> a 10-byte list of skill ids

struct Callees {
    // the Direct3D helpers (ours, d3d_draw.cpp) and the bare ret
    void (__cdecl* prim_color)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned long*,
                               unsigned long*);                            // D3d_PrimColor 0x59FBA0
    void (__cdecl* ret_only)(unsigned);                                    // 0x437CC0
    void (__cdecl* set_blend)(unsigned, unsigned);                         // D3d_SetBlend 0x59FCA0
    void (__cdecl* set_shade)(unsigned);                                   // D3d_SetShadeMode 0x59FD80
    // the menus' (ours, menu_windows.cpp, msgbox.cpp, char_stats.cpp) and the CRT's
    void (__cdecl* box)(int, int, int, int, int, int);                     // Menu_DrawBox 0x57CF60
    unsigned char (__cdecl* list_scroll)(unsigned char*, unsigned char*, unsigned char*,
                                         unsigned char*);                  // Menu_ListScroll 0x57DF00
    unsigned char (__cdecl* can_use)(unsigned, unsigned, unsigned, unsigned);  // Item_CanUse 0x57D9A0
    void (__cdecl* item_row)(int, int, int, unsigned, unsigned, unsigned, int);  // Menu_DrawItemRow 0x57DBF0
    unsigned char (__cdecl* char_count)(const unsigned char*);             // Text_CharCount 0x57D800
    const unsigned char* (__cdecl* text_draw_at)(int, int, int, int, const unsigned char*);  // Text_DrawAt 0x516B30
    unsigned char (__cdecl* count_used)(unsigned);                         // Inventory_CountUsed 0x591A80
    int (__cdecl* sprintf_)(char*, const char*, ...);                      // Crt_sprintf 0x5B9380
    void (__cdecl* font8)(int, int, int, const unsigned char*);            // Text_DrawFont8 0x517090
    void (__cdecl* pieces)(int, int, const unsigned char*, int);           // Menu_DrawPieces 0x57D910
    void (__cdecl* piece)(int, int, unsigned, unsigned);                   // Menu_DrawPiece 0x57D860
    void (__cdecl* scroll_bar)(const unsigned char*, unsigned, int, int, unsigned, unsigned,
                               unsigned);                                  // Menu_DrawScrollBar 0x57DD10
    // group BD's
    unsigned (__cdecl* skill_usable)(unsigned, unsigned, unsigned);        // 0x57DA70
    void (__cdecl* skill_row)(int, int, int, unsigned, const unsigned char*, unsigned, int);  // 0x57DC90
    unsigned (__cdecl* skill_icon)(unsigned);                              // 0x5918A0
    unsigned (__cdecl* skill_cost)(unsigned, unsigned, unsigned);          // 0x591DB0
    const unsigned char* (__cdecl* skill_list)(unsigned, unsigned, unsigned);  // 0x591E50
    // the area palette's (ours, field_misc.cpp and map_scroll.cpp)
    long (__cdecl* clut_adjust)(int, int, int, int, int);                  // Gfx_ClutAdjust 0x5718F0
    void (__cdecl* clut_cycle)();                                          // AreaMap_ClutCycleStart 0x5717B0
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_draw: the start-up fuzz, battle_draw_fuzz.cpp. Clones
// every original before BattleDraw_Inject patches it.
void SelfTest();

}  // namespace battle_draw
