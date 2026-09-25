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

namespace battle_actor_copies {

using U = std::uint32_t;

namespace at {

// The battle's phase bytes (PSX 0x801462E0..): 0x904AA0 is Battle_PhaseDispatch's
// entry; phase 1 (0x42E990) dispatches on 0x904AA1 through the table 0x64AE28,
// whose entries 11 and 12 are this group's two stubs; each of those dispatches
// on 0x904AA3, and the step handlers below them on 0x904AA4.
constexpr U kPhase1 = 0x904AA1;      // u8: 0x64AE28's index
constexpr U kPhase2 = 0x904AA2;      // u8: set to 7 by the item window on item 0x97, else 0
constexpr U kStep = 0x904AA3;        // u8: BattleTarget_Steps' / BattleItem_Steps' index
constexpr U kSub = 0x904AA4;         // u8: the pick states' index (read as a dword & 0xFF by two stubs)
constexpr U kTargetCursor = 0x904AAF;  // u8: 1 while a target is being picked, 0 on confirm / cancel (a guess)
constexpr U kPartyCount = 0x904AB0;  // u8 (read as a dword & 0xFF): members in the party row
constexpr U kEnemyCount = 0x904AB2;  // u8: enemies in the battle (actors 3 .. count + 2)
constexpr U kEntryCount = 0x904AC3;  // u8: counted up by each committed command (PSX 0x801462FF)
// Input.
constexpr U kPressed = 0x7E1BEC;     // Input_Pressed, u16
constexpr U kConfirm = 0x90358E;     // Field_ConfirmButtons, u16
constexpr U kCancel = 0x903590;      // Field_CancelButtons, u16
constexpr U kRepeatLatch = 0x7E01B8; // u16, zeroed when a pick starts (Input_AutoRepeat's)
// The command being built and the acting actor.
constexpr U kCommand = 0x939FA0;     // pointer: +0 the target byte, +1 a kind byte, +2 the item id (u16), +0x10 flags
constexpr U kActing = 0x939EC4;      // pointer: the acting actor's context, +1 a state byte, +5 the actor
// Window record 4's byte +3 (0x803160 + 4 * 0x24 + 3), set to 2 when item
// 0x97 is chosen; battle_damage names the same byte kInstantKill.
constexpr U kWindow4State = 0x8031F3;
// The item window: record 16 (0x8033A0) and its neighbours.
constexpr U kItemState = 0x8033A3;   // u8: record 16's byte +3, set to 1 when the window closes
constexpr U kItemBaseX = 0x8033A4;   // u16: the window's x; +7 is the cursor's
constexpr U kItemBaseY = 0x8033A6;   // u16: the window's y
constexpr U kItemPageCue = 0x8033A9; // u8: 0x32 on a page left, 0x31 on a page right
constexpr U kItemOwner = 0x8033AA;   // u8: the actor whose page / scroll / cursor are kept
constexpr U kItemPage = 0x8033AB;    // u8: the inventory page 0..3
constexpr U kItemCursor = 0x8033AC;  // u8: the cursor's item index 0..9
constexpr U kItemScroll = 0x8033B0;  // s16: the first row shown
constexpr U kItemScrollMotion = 0x8033B2;  // u16: 0xF0 / 0x10 while the list scrolls
constexpr U kItemCursorOn = 0x80340C;      // u8
constexpr U kItemCursorX = 0x803410;       // u16
constexpr U kItemCursorY = 0x803412;       // u16
constexpr U kItemActor = 0x929F06;   // u8: the actor the item window is for
constexpr U kActorPages = 0x9045FC;  // 3 bytes per actor: page, scroll, cursor
// The item records: 24 bytes each, byte +0 the target flags (0x10 a side,
// 0x20 the party / the enemies, 0x40 a choice, 0x80 all), u16 +6 the
// description's system message id.
constexpr U kItemRecords = 0x65C4D8;
constexpr U kItemRecordSize = 0x18;
// The message queue (battle_misc's): its write index and 16 entries of 8
// bytes, +1 a byte, +4 the text pointer.
constexpr U kQueueWrite = 0x93C2A1;
constexpr U kQueueByte = 0x93C2C1;
constexpr U kQueueText = 0x93C2C4;

}  // namespace at

// Callees with no name in symbols.gen.h: unnamed Capcom functions, called by
// address (the round's cross-group rule).
constexpr U kPrevTarget = 0x4457F0;  // unnamed, in no round-8 group: Battle_DefaultTarget's downward twin

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
