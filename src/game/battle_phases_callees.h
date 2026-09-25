// Internal to battle_phases.cpp and battle_phases_fuzz.cpp: every call the
// battle's frame and its first three phases make, through pointers, so that
// the start-up fuzz can stand recording functions in for them - for the
// originals' copies and for ours alike. docs/battle_phases.md.
//
// Five dispatches are not calls to a named function:
//   - BattleStart_Dispatch 0x42E470 and BattleIntro_Dispatch 0x42E730 build
//     a two- and a four-entry table on their own stacks (`mov [esp + k],
//     imm32`) and call `[esp + eax * 4]`; the tables are `start_steps` and
//     `intro_steps` below, and the fuzz re-aims the copies' immediates.
//   - BattleInput_Dispatch 0x42E990, BattleMenu_ConfirmDispatch 0x42EED0 and
//     BattleCommit_Dispatch 0x42F070 tail-jump through the .data tables
//     Battle_InputSteps 0x64AE28, Battle_MenuSteps 0x64AE54 and
//     Battle_CommitSteps 0x64AE74; ours reads the same tables at the call,
//     and the fuzz swaps their entries for recorders.
//   - Battle_Init 0x42E4A0 calls the event hook 0x904B6C with 6.
// Addresses of functions no group owns yet (the round's cross-group rule,
// docs/takeover-queue-round8.md) are raw here and never bound by name.
#pragma once

#include <cstdint>

namespace battle_phases {

namespace at {

// The battle's globals (PSX 0x801462DC..): the phase and its two sub-steps.
constexpr std::uint32_t kPhase = 0x904AA0;         // u8 (read as a dword by Battle_PhaseDispatch)
constexpr std::uint32_t kStep = 0x904AA1;          // u8: the phase's step
constexpr std::uint32_t kSubStep = 0x904AA2;       // u8: the step's own step; the chosen command in the menu
constexpr std::uint32_t kSubStep2 = 0x904AA3;      // u8: zeroed by Cmd_ConfirmDefend
constexpr std::uint32_t kTapTimer = 0x904AA5;      // u8: frames left for a second tap (8)
constexpr std::uint32_t kTapCommand = 0x904AA6;    // u8: the command + 1 the first tap chose
constexpr std::uint32_t kAA7 = 0x904AA7;           // u8, zeroed by Battle_Init
constexpr std::uint32_t kRoundFlags = 0x904AA8;    // u16: bit 1 the phases held, 3 committed, 4 auto battle
constexpr std::uint32_t kEventBattle = 0x904AAA;   // u8 (battle_flow.md: "event battle", a guess)
constexpr std::uint32_t kFacing = 0x904AAC;        // u8: its (b ^ 2) >> 1 picks a word of 0x64E2BC
constexpr std::uint32_t kAAD = 0x904AAD;           // u8, zeroed by Battle_Init
constexpr std::uint32_t kMenuMember = 0x904AAE;    // u8: the member whose menu is open
constexpr std::uint32_t kActorCount = 0x904AB0;    // u8: actors Battle_Init counts over
constexpr std::uint32_t kActorsIn = 0x904AB1;      // u8: of those, the ones not out
constexpr std::uint32_t kCommand = 0x904AB4;       // u8: the command the cross points at (0 none)
constexpr std::uint32_t kEntryOrder = 0x904AB6;    // u8 x 3: members in menu order, 0xFF none
constexpr std::uint32_t kCrossGrow = 0x904ABC;     // u8 x 7: the command cross's arms' growth
constexpr std::uint32_t kMenuIndex = 0x904AC3;     // s8: the entry of kEntryOrder the menu is at
constexpr std::uint32_t kInitiative = 0x904AE4;    // u8: 1 and 2 show messages 0x16 / 0x17 at the start; 2+ no cross
constexpr std::uint32_t kAE6 = 0x904AE6;
constexpr std::uint32_t kDropCount = 0x904AE7;
constexpr std::uint32_t kBattleEnd = 0x904AE8;
constexpr std::uint32_t kAE9 = 0x904AE9;           // u8: bit 1 holds the phases until phase 5
constexpr std::uint32_t kExpTotal = 0x904AEC;
constexpr std::uint32_t kZennyTotal = 0x904AF0;
constexpr std::uint32_t kB24 = 0x904B24;           // 4 dwords zeroed by Battle_Init
constexpr std::uint32_t kEventHook = 0x904B6C;     // void (*)(int): called with 6 by Battle_Init
constexpr std::uint32_t kWaitFrames = 0x904B70;    // u16: 0xE, counted down by BattleCommit_WaitLoad
constexpr std::uint32_t kB7A = 0x904B7A;
constexpr std::uint32_t kTurnBits = 0x904B82;      // u16
constexpr std::uint32_t kB89 = 0x904B89;
constexpr std::uint32_t kActorAt = 0x904B8A;
constexpr std::uint32_t kB8B = 0x904B8B;
constexpr std::uint32_t kAnimGate = 0x904B8E;
constexpr std::uint32_t kTurnCounter = 0x904B90;   // dword, 1 at the start (PSX 0x801463CC)
constexpr std::uint32_t kB97 = 0x904B97;
constexpr std::uint32_t kB98 = 0x904B98;           // u16
constexpr std::uint32_t kTextRecord0 = 0x904CE0;   // Text_Records[0], 32 bytes
constexpr std::uint32_t kBattleMusic = 0x904EFC;   // u16: + 0x15D is the DAT file Battle_Init loads
constexpr std::uint32_t kEFE = 0x904EFE;           // u16
// The party's working records (ObjTrio, 0x14C each) and the enemy objects.
constexpr std::uint32_t kMembers = 0x802D40;
constexpr std::uint32_t kMemberSize = 0x14C;
constexpr std::uint32_t kEnemies = 0x93B960;
constexpr std::uint32_t kEnemySize = 0x128;
constexpr std::uint32_t kMenuActor = 0x939EC4;     // unsigned char*: the member the menu is for (PSX 0x801EBE78)
constexpr std::uint32_t kMenuRecord = 0x939FA0;    // unsigned char*: that member's +0x124, its command record
constexpr std::uint32_t kMessageBusy = 0x939F60;   // u8
constexpr std::uint32_t kMessages = 0x939FBC;      // 4-byte records by 1..count: u8 enemy, -, u16 message
constexpr std::uint32_t kMessageCount = 0x93C2A2;  // u8
constexpr std::uint32_t kC2A0 = 0x93C2A0;          // u8, zeroed with 0x93C2A1 by BattleIntro_OpenWindows
constexpr std::uint32_t kB8E0 = 0x93B8E0;          // u8, zeroed by Battle_CommitRound
// The window records (WindowRecords, 0x24 each).
constexpr std::uint32_t kWindows = 0x803160;
constexpr std::uint32_t kWindowSize = 0x24;
// 16 words copied from 0x80D560 to 0x811560 by Battle_Init.
constexpr std::uint32_t kClutFrom = 0x80D560;
constexpr std::uint32_t kClutTo = 0x811560;
// The pad words (symbols.toml's Input_Held, Input_Pressed,
// Field_ConfirmButtons, Field_CancelButtons; their names are lvalue macros).
constexpr std::uint32_t kInputHeld = 0x7E1BE8;
constexpr std::uint32_t kInputPressed = 0x7E1BEC;
constexpr std::uint32_t kConfirmButtons = 0x90358E;
constexpr std::uint32_t kCancelButtons = 0x903590;
// Constant tables in .data.
constexpr std::uint32_t kPadMasks = 0x64AE3C;      // Battle_CommandPadMasks: u16 x 6, the six command directions
constexpr std::uint32_t kInputSteps = 0x64AE28;    // Battle_InputSteps: 5 code pointers by kStep in phase 1
constexpr std::uint32_t kMenuSteps = 0x64AE54;     // Battle_MenuSteps: 8 code pointers by kSubStep
constexpr std::uint32_t kCommitSteps = 0x64AE74;   // Battle_CommitSteps: 3 code pointers by kStep in phase 2
constexpr std::uint32_t kFacingOffsets = 0x64E2BC; // s8 pairs by (kFacing ^ 2) >> 1
constexpr std::uint32_t kStatusOffsets = 0x64DF70; // s8 pairs by member +8 + 4 * member +0x89

constexpr unsigned kInputStepCount = 5;
constexpr unsigned kMenuStepCount = 8;
constexpr unsigned kCommitStepCount = 3;

}  // namespace at

// Callees with no name in symbols.gen.h, called by address: no group of the
// round owns them (docs/battle_phases.md section 3).
constexpr std::uint32_t kAutoFillCommands = 0x446720;  // PSX AutoBattle_FillCommands 0x801DD264 (sibling name)
constexpr std::uint32_t kReturnItem = 0x446D90;        // (slot, item) -> al; PSX 0x801DDE44; battle_setup's kReturnItem

using Handler = void (__cdecl*)();

struct Callees {
    // Battle_Frame's, in its order
    void (__cdecl* phase_dispatch)();
    unsigned (__cdecl* party_run_states)();
    void (__cdecl* enemy_run_all)();
    void (__cdecl* banner_dispatch)();
    void (__cdecl* area_map_frame)();
    void (__cdecl* task_run_all)();
    void (__cdecl* party_update_screens)();
    void (__cdecl* enemy_update_screens)();
    void (__cdecl* objects_screen)();
    void (__cdecl* update_object_screens)();
    void (__cdecl* party_extra_screens)();
    void (__cdecl* effect_run_objects)();
    void (__cdecl* field_run_slots)();
    void (__cdecl* tint_frame)();
    void (__cdecl* run_task_records)();
    void (__cdecl* draw_frame)();
    // the two stack-built tables, as the originals' immediates:
    // 0x42E4A0, 0x42E730 and 0x42E770, 0x42E8C0, 0x42E8D0, 0x42E930 (ours)
    Handler start_steps[2];
    Handler intro_steps[4];
    // Battle_Init's
    unsigned long (__cdecl* banner_clear_all)();
    unsigned char (__cdecl* actor_is_out)(unsigned);
    void (__cdecl* init_encounter_kind)();
    void (__cdecl* task_clear_all)();
    void (__cdecl* init_actor_contexts)();
    void (__cdecl* apply_stat_mods)();
    void (__cdecl* clut_strip_copy_row)(unsigned);
    void (__cdecl* set_clut_stp)();
    void (__cdecl* load_dat)(int);
    // the intro's
    unsigned (__cdecl* window_alloc)(unsigned, unsigned);
    const unsigned char* (__cdecl* msg_system_ptr)(unsigned);
    unsigned long (__cdecl* queue_push)(unsigned, unsigned, unsigned long);
    void (__cdecl* open_status)(unsigned);
    void (__cdecl* open_enemy_names)();
    void (__cdecl* open_sub1)(unsigned);
    unsigned char (__cdecl* queue_pending)();
    int (__cdecl* file_load_done)();
    void (__cdecl* open_sub2)(unsigned);
    void (__cdecl* open_sub3)(unsigned);
    // the command input's
    void (__cdecl* clear_commands)();
    void (__cdecl* build_entry_order)();
    void (__cdecl* auto_fill_commands)();
    unsigned long (__cdecl* spawn_actor_copies)();
    unsigned long (__cdecl* banner_show_name)(const unsigned char*);
    void (__cdecl* play_effect)(unsigned short);
    unsigned char (__cdecl* return_item)(unsigned, unsigned);
    void (__cdecl* draw_command_cross)(int, int);
    void (__cdecl* draw_command_label)(unsigned);
    void (__cdecl* draw_party_status)(int, int);
    void (__cdecl* pulse_step)();
    // the commit's
    void (__cdecl* build_turn_order)();
    void (__cdecl* choose_actions)();
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (battle_phases_fuzz.cpp): clones every function of this
// file with each call out re-aimed at a recording stand-in, the two stack
// tables' immediates re-aimed in the copies and the three .data tables'
// entries swapped for recorders, runs ours against the clones from the same
// random state, and ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace battle_phases
