// Internal to battle_obj_states.cpp and battle_obj_states_fuzz.cpp: every
// call the battle party objects' state handlers make, through pointers, so
// that the start-up fuzz can stand recording functions in for them - for the
// originals' copies and for ours alike. docs/battle_obj_states.md.
//
// Seven dispatches are not calls to a named function but reads of a .data
// table the original indexes without a bound (as BattleObj_RunState's own
// 0x64DFE0): state 3 calls through 0x64E014 by the sub-state byte +2, state 4
// tail-jumps through 0x64E01C by the character +0x89 of Field_State (or
// through the dword 0x64E048 while +0x134 has bit 0), states 5, 7 and 12
// tail-jump through 0x64E074, 0x64E0E4 and 0x64E134 by +2, and state 8 calls
// through 0x64E118 by +2. Each is a table pointer here; the fuzz points them,
// and the copies' disp32 operands, at tables of recorders of its own.
//
// Every callee is ours already or Capcom's and named in symbols.gen.h; no
// other group's address of the eighth round is called.
#pragma once

#include <cstdint>

namespace battle_obj_states {

namespace at {

// The party objects (ObjTrio, 0x14C each) are what Sprite_Current and
// Field_State point at while BattleParty_RunStates runs them: +0 flags,
// +1 the state (0x64DFE0), +2 the sub-state, +3 / +4, +5 the actor (0..2;
// also the battle-task slot 0x441570 reads), +7 a tint record index, +8 the
// pose base, +9 / +0xA / +0xB counters, +0x34 / +0x38 x and y (16.16),
// +0x3E the ground height, +0x5D..+0x5F a colour, +0x89 the character,
// +0x90 / +0x91 status flags, +0x9A a u16 (MP, by 0x442BD0's subtraction -
// hypothesis), +0xBA a percentage, +0x125 the action, +0x130 / +0x134 flags.
constexpr std::uint32_t kMembers = 0x802D40;
constexpr std::uint32_t kMemberSize = 0x14C;
// The 48 battle-task slots (battle_flow.md): 0x441570 moves the byte +9 of
// slot Sprite_Current +5 and reads its +0x34 / +0x38.
constexpr std::uint32_t kTasks = 0x93A000;
constexpr std::uint32_t kTaskSize = 0x84;
// The battle's globals (PSX 0x801462E0..).
constexpr std::uint32_t kPhase = 0x904AA0;        // u8: 1 and 5 matter to 0x441570
constexpr std::uint32_t kPhaseArg = 0x904AA1;     // u8: above 1 in phase 1, the targeted colour
constexpr std::uint32_t kRoundFlags = 0x904AA8;   // dword / u16 / u8: bits 2, 6, 7, 11, 15 read or set here
constexpr std::uint32_t kPoseBase = 0x904AAC;     // u8: 0x441200's pose base
constexpr std::uint32_t kTargetMember = 0x904AAE; // u8: the member whose character 0x441570 compares
constexpr std::uint32_t kTintOn = 0x904AAF;       // u8: non-zero while the target tint is shown
constexpr std::uint32_t kTintLevel = 0x904AC8;    // u8: the tint's three channel bytes
constexpr std::uint32_t kSwingTarget = 0x904B44;  // u8: Battle_SetTargetFlag40's target after a swing
constexpr std::uint32_t kSkillId = 0x904B80;      // u16: the skill (24-byte records at 0x65C4DC)
constexpr std::uint32_t kSkillCost = 0x904B88;    // u8: subtracted from Field_State +0x9A after a skill
constexpr std::uint32_t kCastKind = 0x904B89;     // u8: 4, 7 and 8 are special to states 7
constexpr std::uint32_t kSoundSet = 0x904B8D;     // u8: Battle_LoadSoundByKey's set
constexpr std::uint32_t kTargetPtr = 0x939FA0;    // unsigned char*: its byte is the target (0x80 = all)
constexpr std::uint32_t kHomeX = 0x903780;        // dword: x in phase 5 with 0x904AA8 bit 15
constexpr std::uint32_t kHomeY = 0x903784;        // dword: y
constexpr std::uint32_t kTints = 0x7E0700;        // MoveScript_TintRecords, 12 bytes each: +2..+4 written
constexpr std::uint32_t kSkillWord = 0x65C4DC;    // u16 of each 24-byte skill record (bit 11: 0x65C4DD bit 3)
// Byte tables after the state table.
constexpr std::uint32_t kAttackCount = 0x64E04C;  // u8 by character: +9 / +0xA of 0x441890
constexpr std::uint32_t kAttackCountBy = 0x64E058;// u8 by 0x904B89: the same while +0x134 has bit 1
constexpr std::uint32_t kCastCount = 0x64E0F0;    // u8 by character: +9 of 0x442A00
constexpr std::uint32_t kCastCountBy = 0x64E0FC;  // u8 by 0x904B89: the same while +0x134 has bit 1

// The dispatch tables (code pointers, indices unchecked).
constexpr std::uint32_t kStates = 0x64DFE0;       // BattleObj_StateTable, 27 dwords by +1 (BattleObj_RunState's)
constexpr std::uint32_t kIdleSubs = 0x64E014;     // by +2, called by state 3 (slots 13 and 14 of 0x64DFE0)
constexpr std::uint32_t kAttackByChar = 0x64E01C; // by Field_State +0x89, state 4's tail jump (slots 15..)
constexpr std::uint32_t kAttackSpecial = 0x64E048;// the dword state 4 jumps through while +0x134 bit 0 (slot 26)
constexpr std::uint32_t kSwingSubs = 0x64E074;    // by +2, state 5
constexpr std::uint32_t kCastSubs = 0x64E0E4;     // by +2, state 7
constexpr std::uint32_t kCastDoneSubs = 0x64E118; // by +2, called by state 8
constexpr std::uint32_t kState12Subs = 0x64E134;  // by +2, state 12

}  // namespace at

using Handler = void (__cdecl*)();

struct Callees {
    // the tables, as the original indexes them
    const std::uint32_t* idle_subs;
    const std::uint32_t* attack_by_char;
    const std::uint32_t* attack_special;
    const std::uint32_t* swing_subs;
    const std::uint32_t* cast_subs;
    const std::uint32_t* cast_done_subs;
    const std::uint32_t* state12_subs;
    // ours, in this file
    void (__cdecl* end_action)();                                          // BattleObj_EndAction 0x442DD0
    // other modules' and Capcom's
    unsigned (__cdecl* pick_pose)();                                       // BattleObj_PickPose
    unsigned char (__cdecl* tick)();                                       // BattleObj_ScriptTick
    unsigned char (__cdecl* tick_once)();                                  // BattleObj_ScriptTickOnce
    long (__cdecl* elevation)(long, long);                                 // AreaMap_Elevation
    unsigned char (__cdecl* ensure_animation)(unsigned char);              // Sprite_EnsureAnimation
    unsigned char (__cdecl* set_tint)(unsigned char*, unsigned char, unsigned char, unsigned char, unsigned char);  // Sprite_SetTint
    void (__cdecl* tint_release)(unsigned char);                           // Tint_Release
    unsigned char (__cdecl* roll_pending)();                               // Battle_RollPendingFlag
    int (__cdecl* rand_)();                                                // Rand
    unsigned long (__cdecl* play_cue)(unsigned);                           // Battle_PlayActorCue
    unsigned char (__cdecl* task_create)(unsigned, unsigned);              // BattleTask_Create
    void (__cdecl* set_target_flag)(unsigned);                             // Battle_SetTargetFlag40
    int (__cdecl* load_done)();                                            // File_LoadDone
    unsigned char (__cdecl* load_sound)(unsigned, unsigned);               // Battle_LoadSoundByKey
    unsigned long (__cdecl* clear_actor_bit)(unsigned);                    // Battle_ClearActorBit
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (battle_obj_states_fuzz.cpp): clones every function of
// this file with each call out re-aimed at a recording stand-in and each
// table operand aimed at a table of recorders, runs ours against the clones
// from the same random state, and ends the process through bof3::Fatal on any
// difference.
void SelfTest();

}  // namespace battle_obj_states
