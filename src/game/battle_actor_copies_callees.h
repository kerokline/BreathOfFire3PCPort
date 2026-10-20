// Internal to battle_actor_copies.cpp and battle_actor_copies_fuzz.cpp: the
// addresses group CH's sixteen functions touch that have no name in
// symbols.toml, and every call they make - through pointers, so that the
// start-up fuzz can stand recording functions in for them, for the originals'
// copies and for ours alike. docs/battle_actor_copies.md.
//
// Five of the sixteen are dispatch stubs: `mov al, [step]; jmp [eax*4 +
// table]` through a .data table (symbols.toml names the five tables). Ours read
// the table afresh from .data, as the originals do, so the fuzz swaps its
// entries for recorders; those entries are not calls through Callees.
//
// Addresses of other groups' functions (the round's cross-group rule,
// docs/takeover-queue-round8.md) and unnamed ones are raw here and never bound
// by name.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace battle_actor_copies {

using U = std::uint32_t;

namespace at {

// The battle's phase bytes (PSX 0x801462E0..): 0x904AA0 is Battle_PhaseDispatch's
// entry; phase 1 (0x42E990) dispatches on 0x904AA1 through the table 0x64AE28,
// whose entries 11 and 12 are this group's two stubs; each of those dispatches
// on 0x904AA3, and the step handlers below them on 0x904AA4.
// kPhase1: now BattleActions_Step (symbols.toml). u8: 0x64AE28's index
// kPhase2: now BattleE4_Step2 (symbols.toml). u8: set to 7 by the item window on item 0x97, else 0
constexpr U kStep = 0x904AA3;        // u8: BattleTarget_Steps' / BattleItem_Steps' index
// kSub: now BattleE4_Step4 (symbols.toml). u8: the pick states' index (read as a dword & 0xFF by two stubs)
// kTargetCursor: now BattleE4_Picking (symbols.toml). u8: 1 while a target is being picked, 0 on confirm / cancel (a guess)
constexpr U kPartyCount = 0x904AB0;  // u8 (read as a dword & 0xFF): members in the party row
// kEnemyCount: now BattleActorCopies_EnemyCount (symbols.toml). u8: enemies in the battle (actors 3 .. count + 2)
// kEntryCount: now BattleActorCopies_EntryCount (symbols.toml). u8: counted up by each committed command (PSX 0x801462FF)
// Input.
// kPressed: now Input_Pressed (symbols.toml). Input_Pressed, u16
constexpr U kConfirm = 0x90358E;     // Field_ConfirmButtons, u16
// kCancel: now Field_CancelButtons (symbols.toml). Field_CancelButtons, u16
constexpr U kRepeatLatch = 0x7E01B8; // u16, zeroed when a pick starts (Input_AutoRepeat's)
// The command being built and the acting actor.
constexpr U kCommand = 0x939FA0;     // pointer: +0 the target byte, +1 a kind byte, +2 the item id (u16), +0x10 flags
constexpr U kActing = 0x939EC4;      // pointer: the acting actor's context, +1 a state byte, +5 the actor
// Window record 4's byte +3 (0x803160 + 4 * 0x24 + 3), set to 2 when item
// 0x97 is chosen; battle_damage names the same byte kInstantKill.
constexpr U kWindow4State = 0x8031F3;
// The item window: record 16 (0x8033A0) and its neighbours.
// kItemState: now Window16_State (symbols.toml). u8: record 16's byte +3, set to 1 when the window closes
constexpr U kItemBaseX = 0x8033A4;   // u16: the window's x; +7 is the cursor's
// kItemBaseY: now Window16_Y (symbols.toml). u16: the window's y
// kItemPageCue: now Window16_PageCue (symbols.toml). u8: 0x32 on a page left, 0x31 on a page right
// kItemOwner: now Window16_Owner (symbols.toml). u8: the actor whose page / scroll / cursor are kept
// kItemPage: now Window16_Page (symbols.toml). u8: the inventory page 0..3
constexpr U kItemCursor = 0x8033AC;  // u8: the cursor's item index 0..9
constexpr U kItemScroll = 0x8033B0;  // s16: the first row shown
// kItemScrollMotion: now Window16_Scroll (symbols.toml). u16: 0xF0 / 0x10 while the list scrolls
// kItemCursorOn: now Window19 (symbols.toml). u8
constexpr U kItemCursorX = 0x803410;       // u16
// kItemCursorY: now Window19_Y (symbols.toml). u16
// kItemActor: now BattleActorCopies_ItemActor (symbols.toml). u8: the actor the item window is for
constexpr U kActorPages = 0x9045FC;  // 3 bytes per actor: page, scroll, cursor
// The item records: 24 bytes each, byte +0 the target flags (0x10 a side,
// 0x20 the party / the enemies, 0x40 a choice, 0x80 all), u16 +6 the
// description's system message id.
constexpr U kItemRecords = 0x65C4D8;
constexpr U kItemRecordSize = 0x18;
// The message queue (battle_misc's): its write index and 16 entries of 8
// bytes, +1 a byte, +4 the text pointer.
// kQueueWrite: now BattleActorCopies_QueueWrite (symbols.toml).
constexpr U kQueueByte = 0x93C2C1;
constexpr U kQueueText = 0x93C2C4;

}  // namespace at

// Callees with no name in symbols.gen.h: unnamed Capcom functions, called by
// address (the round's cross-group rule).
constexpr U kPrevTarget = bof3::addr::Battle_PrevTarget;  // unnamed, in no round-8 group: Battle_DefaultTarget's downward twin

// Every callee returns eax whole (U), so that a use of more than the original
// reads shows in the fuzz; ours keep the byte where the original keeps al.
struct Callees {
    U (__cdecl* set_message)(U, U);          // BattleBanner_SetMessage 0x44A8E0 (ours)
    U (__cdecl* default_target)(U);          // Battle_DefaultTarget 0x445730 (ours): al
    U (__cdecl* prev_target)(U);             // 0x4457F0 (unnamed): al
    U (__cdecl* wrap_index)(U, U, U);        // Battle_WrapIndex 0x4469F0 (ours): the whole dword
    U (__cdecl* auto_repeat)(U);             // Input_AutoRepeat 0x461EB0 (ours)
    U (__cdecl* play_effect)(U);             // Sound_PlayEffect 0x587740 (ours), the u16 id
    U (__cdecl* setup_for_actor)();          // ItemMenu_SetupForActor 0x447E60 (ours)
    U (__cdecl* ability_list)(U, U, U);      // Char_AbilityList 0x591E50 (ours): a pointer
    U (__cdecl* msg_system)(U);              // Msg_SystemPtr 0x497740 (ours): a pointer
    U (__cdecl* can_use)();                  // ItemMenu_CanUseSelected 0x447840 (ours): al
    U (__cdecl* return_true)();              // Battle_ReturnTrue 0x449E00 (ours): al
    U (__cdecl* free_windows)();             // ItemMenu_FreeWindows 0x449FE0 (ours)
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_actor_copies: the start-up fuzz (battle_actor_copies_fuzz.cpp).
// Clones every original before BattleActorCopies_Inject patches it.
void SelfTest();

}  // namespace battle_actor_copies
