// Internal to battle_menu_states.cpp and battle_menu_states_fuzz.cpp: the
// addresses the group's functions touch that have no name in symbols.toml, and
// every call they make - through pointers, so that the start-up fuzz can stand
// recording functions in for them, for the originals' copies and for ours
// alike. docs/battle_menu_states.md.
//
// Four of the fifteen are one-line dispatches through .data tables, which ours
// reads where the original does (so the fuzz swaps the tables' entries, and an
// index past a table reads the next one's, as the original's does):
//   0x447FD0  byte 0x904AA3 through 0x64E44C (BattleAttackCmd_States, 4)
//   0x448180  byte 0x904AA3 through 0x64E45C (BattleItemCmd_States, 10)
//   0x448190  byte 0x904AA4 through 0x64E484 (BattleItemCmd_OpenSteps, 2)
//   0x4486C0  byte 0x904AA4 through 0x64E48C (BattleItemCmd_TargetSteps, 5)
// Addresses of functions nobody has named or that another group owns are raw
// here and never bound (the round's cross-group rule,
// docs/takeover-queue-round8.md).
#pragma once

#include <cstdint>

namespace battle_menu_states {

namespace at {

// The battle's step bytes (PSX 0x801462DD..): the command step the phase
// table 0x64AE28 dispatches on, the command 0x42EED0 dispatches on through
// 0x64AE54 (2 the item command, 3 the one whose states are here first), and
// the command's state and sub-state.
constexpr std::uint32_t kStep = 0x904AA1;
constexpr std::uint32_t kCommand = 0x904AA2;
constexpr std::uint32_t kState = 0x904AA3;
constexpr std::uint32_t kSubState = 0x904AA4;
// u8: 1 while a target is being picked (set with the latch cleared, 0 on the
// commit; its readers were not read - a hypothesis: the target cursor's)
constexpr std::uint32_t kPicking = 0x904AAF;
constexpr std::uint32_t kPartyCount = 0x904AB0;  // read as a dword, its low byte used
constexpr std::uint32_t kEnemyCount = 0x904AB2;  // u8
constexpr std::uint32_t kCommandsChosen = 0x904AC3;  // u8, one up for each command committed
// The party item list's last page, top row and cursor, kept for its next
// opening (ItemMenu_SetupForParty reads them back).
constexpr std::uint32_t kSavedList = 0x904605;
// Pointers: the command being built (+0 the target, +1 the action kind, +2 a
// word: category << 8 | item, +0xA the list row, dword +0xC flags), and the
// acting member's record (ObjTrio + 0x14C n: +1 its state, +5 its actor index,
// +0x125 a byte).
constexpr std::uint32_t kCommandRecord = 0x939FA0;
constexpr std::uint32_t kActing = 0x939EC4;
// The party's records.
constexpr std::uint32_t kMembers = 0x802D40;
constexpr std::uint32_t kMemberSize = 0x14C;
// Input_AutoRepeat's latch word: zeroed so that a held direction starts over.
constexpr std::uint32_t kRepeatLatch = 0x7E01B8;
// The window task's records, 0x24 bytes each from 0x803160.
//   Record 16 (0x8033A0), the party's item list: +3 done, dword +4 x (its
//   low word), word +6 y, +8 the use mode, +0xA the category (0..3),
//   +0xB the top row, +0xC the cursor row, words +0x10 and +0x12 (the
//   category-change and scroll requests: 0x31 / 0x32 and 0x10 / 0xF0).
//   Record 19 (0x80340C): +0 shown, words +4 / +6 x and y (the hand).
//   Record 21 (0x803454): opened by the list's up-at-the-top.
//   Record 4 (0x8031F0): +0 cleared then.
constexpr std::uint32_t kList = 0x8033A0;
constexpr std::uint32_t kHand = 0x80340C;
constexpr std::uint32_t kAbove = 0x803454;
constexpr std::uint32_t kRecord4 = 0x8031F0;
// The inventory lists by category: ids and counts, five pointers each
// (0x904154 + 0x80 c and 0x904354 + 0x80 c for 0..3; docs/char-stats.md).
constexpr std::uint32_t kInventoryIds = 0x656B00;
constexpr std::uint32_t kInventoryCounts = 0x656B14;
// The battle message queue: 16 entries of 8 (+0 a, +1 b, dword +4 the text)
// and its write index (BattleQueue_Push).
constexpr std::uint32_t kQueue = 0x93C2C0;
constexpr std::uint32_t kQueueWrite = 0x93C2A1;
// The four dispatch tables (.data).
constexpr std::uint32_t kAttackStates = 0x64E44C;
constexpr std::uint32_t kItemStates = 0x64E45C;
constexpr std::uint32_t kItemOpenSteps = 0x64E484;
constexpr std::uint32_t kItemTargetSteps = 0x64E48C;

}  // namespace at

// Callees with no name in symbols.gen.h, called by address.
constexpr std::uint32_t kPrevTarget = 0x4457F0;  // Battle_DefaultTarget's downward twin: the first actor not out from the byte down; 0xFF if none
constexpr std::uint32_t kItemFlags = 0x591810;   // (category, item) -> the item's flag byte (0x656B38 / 0x657461 / 0x657D79 / 0x658461 tables)

struct Callees {
    unsigned char (__cdecl* default_target)(unsigned);
    unsigned char (__cdecl* prev_target)(unsigned);
    long (__cdecl* wrap_index)(long, long, long);
    unsigned long (__cdecl* banner_message)(unsigned, unsigned);
    unsigned (__cdecl* auto_repeat)(unsigned);
    void (__cdecl* sound)(unsigned short);
    const unsigned char* (__cdecl* system_ptr)(unsigned);
    unsigned long (__cdecl* queue_push)(unsigned, unsigned, unsigned long);
    unsigned long (__cdecl* setup_for_party)();
    unsigned long (__cdecl* free_windows)();
    unsigned (__cdecl* item_price)(unsigned, unsigned);
    unsigned char (__cdecl* item_can_use)(unsigned, unsigned, unsigned, unsigned);
    unsigned char (__cdecl* item_flags)(unsigned, unsigned);
    unsigned char (__cdecl* return_true)();
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (battle_menu_states_fuzz.cpp): clones every function of
// this file with each call out re-aimed at a recording stand-in, swaps the four
// tables' entries for recorders, runs ours against the clones from the same
// random state, and ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace battle_menu_states
