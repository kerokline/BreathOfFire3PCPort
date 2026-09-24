// Internal to battle_window_draw.cpp and battle_window_draw_fuzz.cpp: every
// call the battle windows' draw helpers make, through pointers, so that the
// start-up fuzz can stand recording functions in for them - for the originals'
// copies and for ours alike. docs/battle_window_draw.md.
//
// Every callee is already ours in another module (Window_Alloc, the PSX
// setters, Gfx_CommitPrim, the menu's icon, character count and small font,
// Text_DrawAt) or Capcom's and named (Crt_sprintf), and one is ours in this
// module (Skill_ApCost, which Skill_CanUse calls). None is another group's of
// the seventh round, so nothing here is called by raw address.
#pragma once

#include <cstdint>

namespace battle_window_draw {

namespace at {

// Data the helpers read or write, as the originals address it.
constexpr std::uint32_t kWindows = 0x803160;        // WindowRecords, 22 of 0x24 bytes
constexpr std::uint32_t kWindowStride = 0x24;
constexpr std::uint32_t kPartyCount = 0x904AB0;     // dword; its low byte: the battle's party size (x of window 1)
constexpr std::uint32_t kStatusX = 0x64E307;        // u8 by party size: window 1's x (0x70, 0x41, 0x12)
constexpr std::uint32_t kPrintBuf = 0x904BA0;       // the sprintf buffer
constexpr std::uint32_t kFmtNumber = 0x64ADD8;      // "%4d"
constexpr std::uint32_t kFmtNone = 0x64E320;        // ":" (the digit strip's cell 10), for a value of 0xFFFF
constexpr std::uint32_t kFmtCost = 0x64D3EC;        // "%2d"
constexpr std::uint32_t kColour = 0x903A5A;         // s8: the window colour (Config), a CLUT row pair
constexpr std::uint32_t kClutShadow = 0x80B788;     // 32 bytes a CLUT row: entry 0 of row (colour * 2 + shade)
constexpr std::uint32_t kQuadShapes = 0x64E148;     // 16 bytes a shape: four (u16 dx, u16 dy) corners
constexpr std::uint32_t kTileSizes = 0x64E268;      // 4 bytes a size: u16 w, u16 h
constexpr std::uint32_t kEnemies = 0x93B9E0;        // EnemyWorkingRecords, stride 0x128
constexpr std::uint32_t kEnemyStride = 0x128;
constexpr std::uint32_t kEnemyKind = 0xC;           // the byte BattleWin_FirstOfKind compares
constexpr std::uint32_t kAbilities = 0x65C4C8;      // the ability table, 0x18 bytes a record: name[16] then the PSX's 8 bytes
constexpr std::uint32_t kAbilityStride = 0x18;
constexpr std::uint32_t kSkillIcons = 0x663D70;     // u8 by ability type: the row's 8 x 8 icon
constexpr std::uint32_t kCharRecords = 0x903A70;    // CharacterRecords, stride 0xA4
constexpr std::uint32_t kCharStride = 0xA4;
constexpr std::uint32_t kWorking = 0x802DC0;        // the party's working records inside ObjTrio, stride 0x14C
constexpr std::uint32_t kWorkingStride = 0x14C;
constexpr std::uint32_t kPartyLists = 0x904062;     // u8 per party slot: its character
constexpr std::uint32_t kMemberRecord = 0x66972C;   // u8 per character: its record (MoveScript_EffectState)
constexpr std::uint32_t kFlags15 = 0x904660;        // four bytes Skill_CanUse tests for ability 0x15
constexpr std::uint32_t kFlags3E = 0x90465C;        // four bytes it tests for ability 0x3E
constexpr std::uint32_t kBattle97 = 0x904AAA;       // a byte it compares with 0x25 for ability 0x97 (PSX 0x801462E6)

}  // namespace at

struct Callees {
    unsigned (__cdecl* window_alloc)(unsigned, unsigned);                              // Window_Alloc
    int (__cdecl* sprintf_)(char*, const char*, ...);                                  // Crt_sprintf
    unsigned (__cdecl* get_tpage)(unsigned, unsigned, int, int);                       // Gpu_GetTPage
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);      // Gpu_SetDrawMode
    void (__cdecl* commit)(unsigned, unsigned);                                        // Gfx_CommitPrim
    unsigned (__cdecl* get_clut)(int, int);                                            // Gpu_GetClut
    void (__cdecl* set_sprt)(unsigned char*);                                          // Gpu_SetSprt
    unsigned char* (__cdecl* set_poly_f4)(unsigned char*);                             // Gpu_SetPolyF4
    void (__cdecl* set_semi)(unsigned char*, unsigned);                                // Gpu_SetSemiTrans
    void (__cdecl* set_tile)(unsigned char*);                                          // Gpu_SetTile
    void (__cdecl* set_poly_ft4)(unsigned char*);                                      // Gpu_SetPolyFT4
    void (__cdecl* set_line_f2)(unsigned char*);                                       // Gpu_SetLineF2
    void (__cdecl* icon8)(int, int, int, int);                                         // Menu_DrawIcon8
    unsigned char (__cdecl* char_count)(const unsigned char*);                         // Text_CharCount
    const unsigned char* (__cdecl* text_draw_at)(int, int, int, int, const unsigned char*);  // Text_DrawAt
    void (__cdecl* font8)(int, int, int, const unsigned char*);                        // Text_DrawFont8
    // ours, in this module
    unsigned char (__cdecl* ap_cost)(unsigned, unsigned, unsigned);                    // Skill_ApCost
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_window_draw: the start-up fuzz,
// battle_window_draw_fuzz.cpp. Clones every original before
// BattleWindowDraw_Inject patches it.
void SelfTest();

}  // namespace battle_window_draw
