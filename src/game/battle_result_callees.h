// Internal to battle_result.cpp and battle_result_fuzz.cpp: every call the
// battle result makes, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. docs/battle_result.md.
//
// Three dispatches are not calls to a named function:
//   - BattleResult_LevelUpStep 0x431B60 and BattleResult_RewardStep 0x431D50
//     are `jmp [eax * 4 + table]` stubs on the byte 0x904AA4, through the
//     .data tables 0x64AFC0 (two entries) and 0x64AFC8 (four). Ours calls
//     through the same .data dword, re-read each time; the fuzz swaps the
//     entries for recorders.
//   - BattleResultWin_ExpState 0x598570 and BattleResultWin_ZennyState
//     0x5986C0 build a two-entry table on their own stack (`mov [esp + k],
//     imm32`) and call `[esp + eax * 4]` by the window record's byte +3; the
//     tables are `exp_window` and `zenny_window` below, and the fuzz re-aims
//     the copies' immediates.
// Addresses of functions no group owns, or other groups own (the round's
// cross-group rule, docs/takeover-queue-round8.md), are raw here and never
// bound by name.
#pragma once

#include <cstdint>

namespace battle_result {

namespace at {

// The battle's globals (PSX 0x801462E0..): the result's sub-phases.
constexpr std::uint32_t kPhase2 = 0x904AA3;       // u8: the result phase's step (0x64AFA0, 0x431910 dispatches it)
constexpr std::uint32_t kStep = 0x904AA4;         // u8: the step within it (0x64AFAC / 0x64AFC0 / 0x64AFC8)
constexpr std::uint32_t kMember = 0x904AA5;       // u8: the party slot the level-up search is at
constexpr std::uint32_t kRoster = 0x904AA7;       // u8: that slot's roster index (0x4469D0's answer)
constexpr std::uint32_t kMode1 = 0x904AA1;        // u8: the battle's top-level mode bytes, 4 / 0 / 0 / 0 after the award
constexpr std::uint32_t kMode2 = 0x904AA2;
constexpr std::uint32_t kPartyCount = 0x904AB0;   // u8: party slots in the fight (PSX 0x801462EC)
constexpr std::uint32_t kDropCount = 0x904AE7;    // u8 (PSX 0x80146323)
constexpr std::uint32_t kBattleEnd = 0x904AE8;    // u8: |= 0x20 when a level-up was found
constexpr std::uint32_t kExpTotal = 0x904AEC;     // dword (PSX 0x80146328)
constexpr std::uint32_t kZennyTotal = 0x904AF0;   // dword (PSX 0x8014632C)
constexpr std::uint32_t kDropItems = 0x904AF4;    // u16 x 16: category << 8 | item (PSX 0x80146330)
constexpr std::uint32_t kDropCounts = 0x904B14;   // u8 x 16 (PSX 0x80146350)
constexpr std::uint32_t kTickStep = 0x904B70;     // u16: EXP / zenny counted per frame (PSX 0x801463AC)
constexpr std::uint32_t kBankIndex = 0x904EFC;    // u16: Snd_LoadBankFile(this + 3) after the setup
constexpr std::uint32_t kPartySet = 0x90412C;     // u8: PartySet_Select's current set (bit 7 its mode-0 mark)
constexpr std::uint32_t kPartyZenny = 0x904058;   // dword: the party's zenny (Zenny_Add 0x591BE0 adds into it)
constexpr std::uint32_t kMsgCurrent = 0x93B8E4;   // const unsigned char*: the battle message Msg_SystemPtr gave
// The text records (Text_Records, 32 bytes each): record 0 the number the
// messages substitute, record 2 the windows' scratch.
constexpr std::uint32_t kText0 = 0x904CE0;
constexpr std::uint32_t kText2 = 0x904D20;
// Input (docs/input-script.md): the held and the newly pressed buttons.
constexpr std::uint32_t kHeld = 0x7E1BE8;         // u16 Input_Held
constexpr std::uint32_t kPressed = 0x7E1BEC;      // u16 Input_Pressed
// The party's working records (ObjTrio + 0x80, 0x14C each): +0 the name,
// +9 the character id, +0xA the level.
constexpr std::uint32_t kMembers = 0x802DC0;
constexpr std::uint32_t kMemberSize = 0x14C;
// The window records (0x803160 + 36 n, Window_Alloc 0x59E2D0): +0 taken,
// +1 kind, +2 the kind handler's state, +3 its step; +4 / +6 x / y words,
// +9 the height. Slot 1 is the EXP / zenny window, slot 0x15 the drops'.
constexpr std::uint32_t kWindows = 0x803160;
constexpr std::uint32_t kWindowSize = 36;
constexpr std::uint32_t kWindowCurrent = 0x905B84;   // the record the window task is running
// Constant data in .data / .rdata.
constexpr std::uint32_t kFormatD = 0x5E10C0;      // "%d"
constexpr std::uint32_t kFormat2d = 0x64D3EC;     // "%2d"
constexpr std::uint32_t kFormat6d = 0x66AF34;     // "%6d"
constexpr std::uint32_t kFormat7d = 0x64ADDC;     // "%7d"
constexpr std::uint32_t kZennyUnit = 0x66A31C;    // the text after the zenny amount
// The dispatch tables (symbols.toml [[data]]).
constexpr std::uint32_t kExpSteps = 0x64AFAC;     // 5 entries, by 0x904AA4, under 0x431920 (group CC's stub)
constexpr std::uint32_t kLevelUpSteps = 0x64AFC0; // 2 entries, by 0x904AA4, BattleResult_LevelUpStep's
constexpr std::uint32_t kRewardSteps = 0x64AFC8;  // 4 entries, by 0x904AA4, BattleResult_RewardStep's

}  // namespace at

// Callees with no name in symbols.gen.h, called by address.
constexpr std::uint32_t kCountMembers = 0x4319B0;   // u8: party slots not out (Battle_ActorIsOut) whose dword ObjTrio +0x134 lacks 0x400
constexpr std::uint32_t kZennyBonus = 0x431FE0;     // u8: 1 when a slot not out holds 7 in byte +0x16 or +0x17 of its 0x802DC0 record
constexpr std::uint32_t kAddExp = 0x4468B0;         // PSX BattleResult_AddExp 0x801DD564 (the sibling's)
constexpr std::uint32_t kRosterIndex = 0x4469D0;    // PSX CharId_ToRosterIndex 0x801DD774 (the sibling's): the byte at 0x66972C + id, 7 is 0
constexpr std::uint32_t kLevelUpPending = 0x432170; // u16: non-zero when the roster index has a level to gain (PSX 0x801EF92C)
constexpr std::uint32_t kLevelUp = 0x498DE0;        // the PSX Char_LevelUp's place in BattleResult_Setup (the sibling's)
constexpr std::uint32_t kAddZenny = 0x591BE0;       // Zenny_Add: 0x904058 += n (0x904138 too when the flag is 0), capped 9,999,999
constexpr std::uint32_t kDrawFrame = 0x5982D0;      // a window frame, (x, y, w, h) by its use here (hypothesis; reads each as a word)
constexpr std::uint32_t kExpToNext = 0x598810;      // by its reads, the EXP a party slot still needs for its next level, 0 at none (hypothesis)

using Handler = void (__cdecl*)();

struct Callees {
    // BattleResultWin_ExpState's stack table, as the original's immediates:
    // 0x5986F0 (BattleResultWin_NextStep), 0x5985A0 (BattleResultWin_DrawExp)
    Handler exp_window[2];
    // BattleResultWin_ZennyState's: 0x5986F0, 0x598700 (BattleResultWin_DrawZenny)
    Handler zenny_window[2];
    // Capcom's and other modules'
    unsigned char (__cdecl* count_members)();
    unsigned char (__cdecl* zenny_bonus)();
    void (__cdecl* add_exp)(unsigned);
    unsigned char (__cdecl* roster_index)(unsigned);
    unsigned (__cdecl* level_up_pending)(unsigned, unsigned);
    void (__cdecl* level_up)(unsigned);
    unsigned char (__cdecl* add_zenny)(unsigned, unsigned);
    void (__cdecl* draw_frame)(int, int, int, int);
    unsigned (__cdecl* exp_to_next)(unsigned);
    int (__cdecl* crt_sprintf)(char*, const char*, ...);
    const unsigned char* (__cdecl* msg_system)(unsigned);
    void (__cdecl* party_set_select)(unsigned, unsigned);
    unsigned (__cdecl* window_alloc)(unsigned, unsigned);
    int (__cdecl* file_load_done)();
    void (__cdecl* snd_load_bank)(unsigned);
    // the original pushes a fourth argument, 0 (the PSX's four), which ours reads not
    unsigned char (__cdecl* inventory_add)(unsigned, unsigned, unsigned, unsigned);
    const unsigned char* (__cdecl* text_draw_at)(int, int, int, int, const unsigned char*);
    void (__cdecl* text_draw_font12)(int, int, int, const unsigned char*);
    void (__cdecl* draw_medium_box)(int, int);
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (battle_result_fuzz.cpp): clones every function of this
// file with each call out re-aimed at a recording stand-in, the two stack
// tables' immediates re-aimed in the copies and the two .data tables' entries
// swapped, runs ours against the clones from the same random state, and ends
// the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace battle_result
