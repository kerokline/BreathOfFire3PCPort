// Internal to battle_flow.cpp and battle_flow_fuzz.cpp: every call the battle
// task and the turn flow make, through pointers, so that the start-up fuzz can
// stand recording functions in for them - for the originals' copies and for
// ours alike. docs/battle_flow.md.
//
// Four dispatches are not calls to a named function:
//   - Battle_PhaseDispatch 0x42E400 builds a six-entry table of the battle's
//     phase handlers on its own stack (`mov [esp + k], imm32`) and calls
//     `[esp + eax * 4]`; the table is `phases` below, and the fuzz re-aims
//     the copy's immediates. Before it, when the byte 0x904AAA is set, it
//     calls the pointer at 0x904B6C with 3.
//   - BattleTask_RunAll 0x435110 does the same with the four task kinds.
//   - BattleEnemy_RunAll 0x435830 calls through the state table 0x64B084 in
//     .data (0x64B088, one entry on, while 0x904AAA is set), and through an
//     enemy object's +0xF4; the fuzz swaps the table's entries and points
//     +0xF4 at a recorder.
// Addresses of other groups' functions (the round's cross-group rule,
// docs/takeover-queue-round7.md) are raw here and never bound by name.
#pragma once

#include <cstdint>

namespace battle_flow {

namespace at {

// The 48 battle-task slots, 0x84 bytes each (PSX 0x801EC2A0, 0x78 each):
// byte +0 flags (bit 0 taken; any non-zero byte is run), +5 the parameter,
// +6 the kind (0..3), +0xA a count (s8), +0xB, +0x32 / +0x36 / +0x3A words,
// +0x80 the owner (a dword).
constexpr std::uint32_t kTasks = 0x93A000;
constexpr std::uint32_t kTaskSize = 0x84;
constexpr unsigned kTaskCount = 0x30;
constexpr std::uint32_t kTaskCurrent = 0x93B8C4;  // unsigned char*: the slot being run (PSX 0x801EC250)
constexpr std::uint32_t kTaskOwner = 0x93B940;    // the running slot's +0x80
// The eight enemy objects, 0x128 bytes each (the working record, PSX
// 0x801EB620, is +0x80 of each; kinship-probe-battle-engine.md).
constexpr std::uint32_t kEnemies = 0x93B960;
constexpr std::uint32_t kEnemySize = 0x128;
constexpr unsigned kEnemyCount = 8;
constexpr std::uint32_t kEnemyCurrent = 0x939AD8;  // unsigned char*: the enemy being run (PSX 0x801EB458)
constexpr std::uint32_t kEnemyStates = 0x64B084;   // code pointers by +0x100; 0x64B088 while paused
// The battle's globals (PSX 0x801462E0..).
constexpr std::uint32_t kPhase = 0x904AA0;         // dword; its low byte picks Battle_PhaseDispatch's entry
constexpr std::uint32_t kRoundFlags = 0x904AA8;    // u16 (PSX 0x801462E4)
constexpr std::uint32_t kPaused = 0x904AAA;        // u8
constexpr std::uint32_t kEnemiesLeft = 0x904AB3;   // u8 (PSX 0x801462EF)
constexpr std::uint32_t kDropCount = 0x904AE7;     // u8, at most 15 (PSX 0x80146323)
constexpr std::uint32_t kBattleEnd = 0x904AE8;     // u8, |= 2 when the last enemy falls (PSX 0x80146324)
constexpr std::uint32_t kExpTotal = 0x904AEC;      // dword (PSX 0x80146328)
constexpr std::uint32_t kZennyTotal = 0x904AF0;    // dword (PSX 0x8014632C)
constexpr std::uint32_t kDropItems = 0x904AF4;     // u16 x 16 (PSX 0x80146330)
constexpr std::uint32_t kDropCounts = 0x904B14;    // u8 x 16 (PSX 0x80146350)
constexpr std::uint32_t kTurnGate = 0x904B34;      // u8, BattleEnemy_Chance70's "3 or more" gate
constexpr std::uint32_t kFormation = 0x904B35;     // u8, BattleEnemy_Chance70's "4" gate
constexpr std::uint32_t kPauseHook = 0x904B6C;     // void (*)(int), called with 3 while paused
constexpr std::uint32_t kMagicId = 0x904B80;       // u16: the item or ability the magic loaders were given
constexpr std::uint32_t kActorAt = 0x904B8A;       // u8 (PSX 0x801463C6)
constexpr std::uint32_t kAnimGate = 0x904B8E;      // u8 (PSX 0x801463CA)
constexpr std::uint32_t kNumberText = 0x904BA0;    // char[]: Battle_DrawNumber's sprintf buffer
constexpr std::uint32_t kLoadFlags = 0x904AA9;     // u8, |= 4 once a magic file is asked for
// Constant tables in .data.
constexpr std::uint32_t kItemRows = 0x64B274;      // 4 pointers to u8 tables: (id >> 8, id & 0xFF) -> row (PSX 0x800B39F0)
constexpr std::uint32_t kAbilityRows = 0x64C1D0;   // u8 by ability (PSX 0x800B3450)
constexpr std::uint32_t kMagicFiles = 0x64C2B8;    // 8-byte rows, the u16 DAT file first (PSX 0x800B3538)
constexpr std::uint32_t kAbilityFlags = 0x65C4DD;  // a byte in each 24-byte ability record; bit 0x10 wants a second task
constexpr std::uint32_t kNumberFormat = 0x64E324;  // "%3d"
// The party's working records (ObjTrio, 0x14C each) and the enemy objects,
// as Battle_ActorIsOut reads them: actor 0..2 a member, 3.. an enemy.
constexpr std::uint32_t kMembers = 0x802D40;
constexpr std::uint32_t kMemberSize = 0x14C;

}  // namespace at

// Callees with no name in symbols.gen.h: other groups' functions this round,
// called by address (the round's cross-group rule).
constexpr std::uint32_t kRemoveFromTurnOrder = 0x446650;  // group BE's; PSX Battle_RemoveFromTurnOrder 0x801DD114
constexpr std::uint32_t kSetFlagBit = 0x494ED0;           // group BG's; ORs bit (n & 0x1F) into dword 0x904068 + (n >> 5) * 4
constexpr std::uint32_t kClearTurnBit = 0x446FD0;         // group BF's; word 0x904B82 &= ~(1 << n)

using Handler = void (__cdecl*)();

struct Callees {
    // Battle_PhaseDispatch's stack-built table, as the original's immediates:
    // 0x42E470, 0x42E990, 0x42F070, 0x42F220, 0x4302B0, 0x4311E0 (unread)
    Handler phases[6];
    // BattleTask_RunAll's, by kind: 0x4352A0, 0x435350, 0x4378B0, 0x4357D0 (unread)
    Handler tasks[4];
    // ours, in this file
    unsigned char (__cdecl* task_create)(unsigned, unsigned);
    void (__cdecl* roll_drops)();
    void (__cdecl* draw_number)(int, int, unsigned, unsigned);
    // other modules' and Capcom's
    void (__cdecl* update_screen)();
    unsigned char (__cdecl* ensure_animation)(unsigned char);
    unsigned char (__cdecl* script_tick)();
    unsigned char (__cdecl* script_tick_once)();
    int (__cdecl* crt_rand)();
    void (__cdecl* remove_from_turn_order)(unsigned);
    void (__cdecl* set_flag_bit)(unsigned);
    void (__cdecl* release_tint)(unsigned char*);
    void (__cdecl* clear_turn_bit)(unsigned);
    void (__cdecl* load_dat)(int);
    int (__cdecl* crt_sprintf)(char*, const char*, ...);
    unsigned (__cdecl* get_tpage)(unsigned, unsigned, int, int);
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);
    void (__cdecl* commit)(unsigned, unsigned);
    unsigned (__cdecl* get_clut)(int, int);
    void (__cdecl* set_sprt)(unsigned char*);
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (battle_flow_fuzz.cpp): clones every function of this
// file with each call out re-aimed at a recording stand-in, the two stack
// tables' immediates re-aimed in the copies and the one jump table relocated,
// runs ours against the clones from the same random state, and ends the
// process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace battle_flow
