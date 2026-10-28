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

#include "bof3/symbols.gen.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace battle_phases {

namespace at {

// The battle's globals (PSX 0x801462DC..): the phase and its two sub-steps.
constexpr ::bof3::image::Va kPhase{0x904AA0};         // u8 (read as a dword by Battle_PhaseDispatch)
constexpr ::bof3::image::Va kStep{0x904AA1};          // u8: the phase's step
constexpr ::bof3::image::Va kSubStep{0x904AA2};       // u8: the step's own step; the chosen command in the menu
// kSubStep2: now BattleE4_Step3 (symbols.toml). u8: zeroed by Cmd_ConfirmDefend
// kTapTimer: now BossSb_Countdown (symbols.toml). u8: frames left for a second tap (8)
// kTapCommand: now BattleE5_GeneRow (symbols.toml). u8: the command + 1 the first tap chose
// kAA7: now BattleE1_Roster (symbols.toml). u8, zeroed by Battle_Init
constexpr ::bof3::image::Va kRoundFlags{0x904AA8};    // u16: bit 1 the phases held, 3 committed, 4 auto battle
// kEventBattle: now EventBattle_Id (symbols.toml). u8 (battle_flow.md: "event battle", a guess)
constexpr ::bof3::image::Va kFacing{0x904AAC};        // u8: its (b ^ 2) >> 1 picks a word of 0x64E2BC
// kAAD: now BossSe_Script (symbols.toml). u8, zeroed by Battle_Init
// kMenuMember: now BattleE4_AutoMember (symbols.toml). u8: the member whose menu is open
constexpr ::bof3::image::Va kActorCount{0x904AB0};    // u8: actors Battle_Init counts over
// kActorsIn: now Battle_PartyStanding (symbols.toml). u8: of those, the ones not out
constexpr ::bof3::image::Va kCommand{0x904AB4};       // u8: the command the cross points at (0 none)
constexpr ::bof3::image::Va kEntryOrder{0x904AB6};    // u8 x 3: members in menu order, 0xFF none
constexpr ::bof3::image::Va kCrossGrow{0x904ABC};     // u8 x 7: the command cross's arms' growth
// kMenuIndex: now BattleActorCopies_EntryCount (symbols.toml). s8: the entry of kEntryOrder the menu is at
// kInitiative: now BattlePhases_Initiative (symbols.toml). u8: 1 and 2 show messages 0x16 / 0x17 at the start; 2+ no cross
// kAE6: now BattlePhases_AE6 (symbols.toml).
// kDropCount: now Battle_DropCount (symbols.toml).
// kBattleEnd: now Battle_EndFlags (symbols.toml).
// kAE9: now BattleMisc_BannerKinds (symbols.toml). u8: bit 1 holds the phases until phase 5
// kExpTotal: now Battle_ExpTotal (symbols.toml).
// kZennyTotal: now BattleFlow_ZennyTotal (symbols.toml).
// kB24: now BattlePhases_B24 (symbols.toml). 4 dwords zeroed by Battle_Init
constexpr ::bof3::image::Va kEventHook{0x904B6C};     // void (*)(int): called with 6 by Battle_Init
// kWaitFrames: now BattleResult_TickStep (symbols.toml). u16: 0xE, counted down by BattleCommit_WaitLoad
// kB7A: now BattlePhases_B7A (symbols.toml).
// kTurnBits: now BattleActions_Pending (symbols.toml). u16
// kB89: now BattlePhases_B89 (symbols.toml).
// kActorAt: now BattleFlow_ActorAt (symbols.toml).
// kB8B: now BattlePhases_B8B (symbols.toml).
// kAnimGate: now BattleFlow_AnimGate (symbols.toml).
constexpr ::bof3::image::Va kTurnCounter{0x904B90};   // dword, 1 at the start (PSX 0x801463CC)
// kB97: now BattleDamage_AiByteB97 (symbols.toml).
constexpr ::bof3::image::Va kB98{0x904B98};           // u16
constexpr ::bof3::image::Va kTextRecord0{0x904CE0};   // Text_Records[0], 32 bytes
// kBattleMusic: now Game_AreaNumber (symbols.toml). u16: + 0x15D is the DAT file Battle_Init loads
// kEFE: now MoveScript_FAWord (symbols.toml). u16
// The party's working records (ObjTrio, 0x14C each) and the enemy objects.
// kMembers: now ObjTrio (symbols.toml).
constexpr std::uint32_t kMemberSize = 0x14C;
// kEnemies: now Battle_Enemies (symbols.toml).
constexpr std::uint32_t kEnemySize = 0x128;
// kMenuActor: now BattleE4_MenuActor (symbols.toml). unsigned char*: the member the menu is for (PSX 0x801EBE78)
constexpr ::bof3::image::Va kMenuRecord{0x939FA0};    // unsigned char*: that member's +0x124, its command record
// kMessageBusy: now BattleE1_MessageUp (symbols.toml). u8
constexpr ::bof3::image::Va kMessages{0x939FBC};      // 4-byte records by 1..count: u8 enemy, -, u16 message
// kMessageCount: now BattleE5_EnemyMessageCount (symbols.toml). u8
constexpr ::bof3::image::Va kC2A0{0x93C2A0};          // u8, zeroed with 0x93C2A1 by BattleIntro_OpenWindows
constexpr ::bof3::image::Va kB8E0{0x93B8E0};          // u8, zeroed by Battle_CommitRound
// The window records (WindowRecords, 0x24 each).
// kWindows: now WindowRecords (symbols.toml).
constexpr std::uint32_t kWindowSize = 0x24;
// 16 words copied from 0x80D560 to 0x811560 by Battle_Init.
// kClutFrom: now Gfx_ClutStripSource[0xff0] (symbols.toml).
// kClutTo: now Gfx_ClutStrip[0xff0] (symbols.toml).
// The pad words (symbols.toml's Input_Held, Input_Pressed,
// Field_ConfirmButtons, Field_CancelButtons; their names are lvalue macros).
// kInputHeld: now Input_Held (symbols.toml).
// kInputPressed: now Input_Pressed (symbols.toml).
// kConfirmButtons: now Field_ConfirmButtons (symbols.toml).
// kCancelButtons: now Field_CancelButtons (symbols.toml).
// Constant tables in .data.
// kPadMasks: now Battle_CommandPadMasks (symbols.toml). Battle_CommandPadMasks: u16 x 6, the six command directions
// kInputSteps: now Battle_InputSteps (symbols.toml). Battle_InputSteps: 5 code pointers by kStep in phase 1
// kMenuSteps: now Battle_MenuSteps (symbols.toml). Battle_MenuSteps: 8 code pointers by kSubStep
// kCommitSteps: now Battle_CommitSteps (symbols.toml). Battle_CommitSteps: 3 code pointers by kStep in phase 2
constexpr ::bof3::image::Va kFacingOffsets{0x64E2BC}; // s8 pairs by (kFacing ^ 2) >> 1
constexpr ::bof3::image::Va kStatusOffsets{0x64DF70}; // s8 pairs by member +8 + 4 * member +0x89

constexpr unsigned kInputStepCount = 5;
constexpr unsigned kMenuStepCount = 8;
constexpr unsigned kCommitStepCount = 3;

}  // namespace at

// Callees with no name in symbols.gen.h, called by address: no group of the
// round owns them (docs/battle_phases.md section 3).
constexpr std::uint32_t kAutoFillCommands = bof3::addr::AutoBattle_FillCommands;  // PSX AutoBattle_FillCommands 0x801DD264 (sibling name)
constexpr std::uint32_t kReturnItem = bof3::addr::Battle_ReturnItem;        // (slot, item) -> al; PSX 0x801DDE44; battle_setup's kReturnItem

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
