// Internal to battle_windows.cpp and battle_windows_fuzz.cpp: every call the
// battle windows make, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. docs/battle_windows.md.
//
// The callees of other groups of the seventh round are called by raw address
// (docs/takeover-queue-round7.md, "The rule for calls across groups"): group
// BD's draw helpers (0x444340, 0x4447B0, 0x444900, 0x4449E0, 0x444A90,
// 0x444C40, 0x444CE0, 0x444D50, 0x444E00, 0x444EB0), and two that no group
// owns yet: the small 8 px UI font 0x516E70 and 0x589110 (the next round's).
#pragma once

#include <cstdint>

namespace battle_windows {

namespace at {

constexpr std::uint32_t kColour = 0x903A5A;        // s8: the window colour (Config), a CLUT row
constexpr std::uint32_t kClutShadow = 0x80B7A8;    // 64 bytes a row: word 0 is the window's line colour
constexpr std::uint32_t kPartyObjects = 0x802D40;  // ObjTrio: the battle's party objects, stride 0x14C
constexpr std::uint32_t kObjectStride = 0x14C;
constexpr std::uint32_t kPartyBars = 0x80333F;     // 36 bytes a member: +0 +1 +2 +3 the bar bytes
constexpr std::uint32_t kEnemies = 0x93B9E0;       // EnemyWorkingRecords, stride 0x128: +0 the name, +0xF 1 = has bar
constexpr std::uint32_t kEnemyStride = 0x128;
constexpr std::uint32_t kPartyCount = 0x904AB0;    // u8: members in the party row
constexpr std::uint32_t kCrossSel = 0x904AB4;      // u8 (read as a dword): the command cross's selection
constexpr std::uint32_t kCrossGrow = 0x904ABC;     // u8 x 5: each arm's growth; +5, +6 the two extras
constexpr std::uint32_t kCrossExtra5 = 0x904AC1;
constexpr std::uint32_t kCrossExtra6 = 0x904AC2;
constexpr std::uint32_t kActor = 0x939EC4;         // the acting member's record pointer (+0x134 bit 1)
constexpr std::uint32_t kCrossArms = 0x64E2AC;     // (x, y) bytes per arm, 7 of them
constexpr std::uint32_t kCommandBoxes = 0x64E2C8;  // Battle_CommandBoxes: s16 x, y per command, stride 8
constexpr std::uint32_t kCommandLabels = 0x669D60; // Battle_CommandLabelPointers: 7 pointers to 0x669D28 + 8 i
constexpr std::uint32_t kSlash = 0x64E31C;         // "/"
constexpr std::uint32_t kPrintBuf = 0x904BA0;      // the sprintf buffer
constexpr std::uint32_t kCurrent = 0x905B84;       // the window record Field_RunTaskRecords is running
constexpr std::uint32_t kMsgHead = 0x93C2A0;       // u8: the battle message ring's read index (0..15)
constexpr std::uint32_t kMsgTail = 0x93C2A1;       // u8: its write index
constexpr std::uint32_t kMsgRing = 0x93C2C4;       // 8 bytes an entry: +0 the text pointer
constexpr std::uint32_t kStateTable = 0x64DFE0;    // 27 handlers of a party object's state byte +1
constexpr std::uint32_t kPoseFlags = 0x904AE5;     // u8, bit 0x20
constexpr std::uint32_t kBattleKind = 0x904AA0;    // u8, 5 = a kind with its own poses
constexpr std::uint32_t kKindFlags = 0x904AE8;     // u8, bit 1
constexpr std::uint32_t kPoseMode = 0x904AA8;      // u8, bit 0x10
constexpr std::uint32_t kTickGate = 0x904B8E;      // u8: 0x441180 / 0x4411B0 tick unless set
constexpr std::uint32_t kSkillFlags = 0x65C4DD;    // 24 bytes a skill: bit 3 of this byte
constexpr std::uint32_t kPoseSet = 0x8C5D80;       // the pose set 0x589110 is handed (0x1800 bytes)
// The three handlers of window kind 0x597A30 dispatches on (+0 of its eighth
// argument). Unnamed pointer-reached neighbours, not in this group.
constexpr std::uint32_t kKindHandlers[3] = {0x597A80, 0x597BD0, 0x597C10};

}  // namespace at

// Other groups' functions, by address (the round's rule).
constexpr std::uint32_t kDrawValue = 0x444340;     // (x, y, colour, u16 value): "%4d", or ":" (its glyph 10) for 0xFFFF, BD
constexpr std::uint32_t kDrawEdge = 0x4447B0;      // (x, y, piece, semi): a flat POLY_F4 of the piece table 0x64E148, BD
constexpr std::uint32_t kDrawTile = 0x444900;      // (x, y, size, semi): a TILE in the window colour, BD
constexpr std::uint32_t kDrawTileRgb = 0x4449E0;   // (x, y, size, colour15, semi): a TILE, BD
constexpr std::uint32_t kDrawBar = 0x444A90;       // (x, y, width, fill, flag): a gauge, BD
constexpr std::uint32_t kDrawDigit = 0x444C40;     // (x, y, n): a 16 x 8 SPRT, u = n << 4, BD
constexpr std::uint32_t kLinePlain = 0x444CE0;     // (x0, y0, x1, y1, r, g, b): a LINE_F2, BD
constexpr std::uint32_t kLineSemi0 = 0x444D50;     // the same under abr 0, BD
constexpr std::uint32_t kLineSemi1 = 0x444E00;     // the same under abr 1, BD
constexpr std::uint32_t kEnemyNameShown = 0x444EB0;// (enemy) -> al: 0 when a later battle slot holds an enemy with its +0xC byte, BD
constexpr std::uint32_t kTinyFont = 0x516E70;      // (x, y, colour, count, text) -> text end: the 8 px UI font
constexpr std::uint32_t kPoseFrom = 0x589110;      // (pose, set, size): a pose from a set (the next round's)

struct Callees {
    // ours already, and Capcom's by name
    unsigned (__cdecl* get_tpage)(unsigned, unsigned, int, int);                        // Gpu_GetTPage
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);       // Gpu_SetDrawMode
    void (__cdecl* commit)(unsigned, unsigned);                                         // Gfx_CommitPrim
    void (__cdecl* set_poly_ft4)(unsigned char*);                                       // Gpu_SetPolyFT4
    const unsigned char* (__cdecl* text_draw_at)(int, int, int, int, const unsigned char*);  // Text_DrawAt
    void (__cdecl* font8)(int, int, int, const unsigned char*);                         // Text_DrawFont8
    int (__cdecl* sprintf_)(char*, const char*, ...);                                   // Crt_sprintf
    void (__cdecl* icon)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);   // Menu_DrawIcon
    void (__cdecl* update_screen)();                                                    // Sprite_UpdateScreen
    unsigned (__cdecl* script_tick)();                                                  // Sprite_ScriptTick (al)
    unsigned (__cdecl* script_tick_once)();                                             // Sprite_ScriptTickOnce (al)
    unsigned (__cdecl* ensure_pose)(unsigned);                                          // Sprite_EnsureAnimation
    // other groups', by address
    unsigned (__cdecl* pose_from)(unsigned, std::uint32_t, unsigned);                   // 0x589110
    void (__cdecl* draw_value)(int, int, unsigned, unsigned);                           // 0x444340
    void (__cdecl* draw_edge)(int, int, unsigned, unsigned);                            // 0x4447B0
    void (__cdecl* draw_tile)(int, int, unsigned, unsigned);                            // 0x444900
    void (__cdecl* draw_tile_rgb)(int, int, unsigned, unsigned, unsigned);              // 0x4449E0
    void (__cdecl* draw_bar)(int, int, unsigned, unsigned, unsigned);                   // 0x444A90
    void (__cdecl* draw_digit)(int, int, unsigned);                                     // 0x444C40
    void (__cdecl* line_plain)(int, int, int, int, unsigned, unsigned, unsigned);       // 0x444CE0
    void (__cdecl* line_semi0)(int, int, int, int, unsigned, unsigned, unsigned);       // 0x444D50
    void (__cdecl* line_semi1)(int, int, int, int, unsigned, unsigned, unsigned);       // 0x444E00
    unsigned (__cdecl* enemy_name_shown)(unsigned);                                     // 0x444EB0 (al)
    const unsigned char* (__cdecl* tiny_font)(int, int, unsigned, unsigned, const unsigned char*);  // 0x516E70
    // the state handlers (0x64DFE0) and window kinds (0x597A80 ...), as tables
    const std::uint32_t* state_table;
    std::uint32_t kind_handlers[3];
    // ours, in this module
    unsigned (__cdecl* run_state)();                                                    // BattleObj_RunState
    void (__cdecl* command_icon)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);  // BattleWin_DrawCommandIcon
    void (__cdecl* message_box)(int, int);                                              // BattleWin_DrawMessageBox
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_windows: the start-up fuzz, battle_windows_fuzz.cpp.
// Clones every original before BattleWindows_Inject patches it.
void SelfTest();

}  // namespace battle_windows
