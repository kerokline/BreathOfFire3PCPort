// Internal to battle_fx_tasks.cpp and battle_fx_tasks_fuzz.cpp: every call the
// battle effect tasks make, through pointers, so that the start-up fuzz can
// stand recording functions in for them - for the originals' copies and for
// ours alike. docs/battle_fx_tasks.md.
//
// Six dispatches build their table on their own stack (`mov [esp + k],
// imm32`, then `call [esp + eax * 4]`): the tables are the arrays below, as
// the originals' immediates, and the fuzz re-aims the copies' immediates.
// A seventh, BattleMagicRow_Run 0x4378B0, tail-jumps through the .data table
// Magic_Rows (0x64C2B8, 151 rows of {u16 file, code pointer}); it is read at
// the call, and the fuzz swaps its 151 code pointers for recorders.
//
// Addresses of other groups' functions (the round's cross-group rule,
// docs/takeover-queue-round8.md) are raw here and never bound by name.
#pragma once

#include <cstdint>

namespace battle_fx_tasks {

namespace at {

// The 48 battle-task slots, 0x84 bytes each (battle_flow_callees.h): +0
// flags, +1 the state, +5 the parameter (which handler of the kind), +6 the
// kind, +7 a mode, +9 a timer, +0xA a digit count, +0xB, +0x80 the owner.
constexpr std::uint32_t kTasks = 0x93A000;
constexpr std::uint32_t kTaskSize = 0x84;
constexpr unsigned kTaskCount = 0x30;
constexpr std::uint32_t kTaskCurrent = 0x93B8C4;  // unsigned char*: the slot being run
constexpr std::uint32_t kTaskOwner = 0x93B940;    // the running slot's +0x80, copied there by BattleTask_RunAll
// The party's working records (ObjTrio, 0x14C each) and the eight enemy
// objects (0x128 each): an actor 0..2 is a member, 3.. an enemy.
constexpr std::uint32_t kMembers = 0x802D40;
constexpr std::uint32_t kMemberSize = 0x14C;
constexpr std::uint32_t kEnemies = 0x93B960;
constexpr std::uint32_t kEnemySize = 0x128;
// The battle's globals.
constexpr std::uint32_t kPhase = 0x904AA0;        // u8 here: the actor watch waits out phase 5, the follower ends outside 1
constexpr std::uint32_t kRoundCount = 0x904AA2;   // u8, + 1 by the second round hook
constexpr std::uint32_t kRoundFlags = 0x904AA8;   // dword: 0x400 the actor watch's gate, 0x800 the magic rows'
constexpr std::uint32_t kBattleEnd = 0x904AE8;    // u8: bit 0 gates the two round hooks
constexpr std::uint32_t kTurnGate = 0x904B34;     // u8, compared with the owner's actor
// The sprite animation set pointer (docs/sprite-pose.md): the pose task and
// the actor watch point it at 0x8C5D80 around their state call and back at
// 0x8B3580 (mode_flow's pool) after it.
constexpr std::uint32_t kAnimSet = 0x9039D8;
constexpr std::uint32_t kAnimSetBattle = 0x8C5D80;
constexpr std::uint32_t kAnimSetField = 0x8B3580;
constexpr std::uint32_t kLastArea = 0x802290;     // u16: the previous area (mode_flow_callees.h)
constexpr std::uint32_t kScriptVar7 = 0x8034E4;   // MoveScript_Var7, and the byte after it
// Constant tables in .data.
constexpr std::uint32_t kPartyOffsets = 0x64DF70;  // s8 pairs (dx, dy) by (sprite +0x2C) * 4 + sprite +8
constexpr std::uint32_t kPoseClut = 0x64B058;      // u8 by mode +7; the CLUT row is it less 0x50
constexpr std::uint32_t kPoseAnim = 0x64B064;      // u8 by mode * 2 + (+8 >> 1)
constexpr std::uint32_t kMagicRows = 0x64C2B8;     // Magic_Rows: 151 x {u16 file, u16 pad, code pointer}
constexpr unsigned kMagicRowCount = 151;

}  // namespace at

// Callees with no name in symbols.gen.h, called by address.
constexpr std::uint32_t kAfterAreaScript = 0x446E20;  // not in round 8's queue; tail-jumped to by 0x437720 (unread)

using Handler = void (__cdecl*)();

struct Callees {
    // BattleFx_Dispatch 0x4352A0's stack table, by the slot's +5: 0x437CC0
    // (a bare ret), 0x432B70, 0x432F90, 0x433190, 0x4332B0, 0x433380,
    // 0x433460, 0x4337F0, 0x43C740, 0x4348E0, 0x434B90, 0x433970, 0x433B80,
    // 0x434D70, 0x434F40, 0x452680, 0x452AD0, 0x434310, 0x452B60
    Handler fx[19];
    // BattleMagicFx_Dispatch 0x435350's, by the slot's +5: 110 magic effects
    Handler magic_fx[110];
    // BattleFx_DamagePopup 0x432B70's, by +1: 0x432C40, 0x432DB0, 0x432DE0,
    // 0x432E50, 0x432EA0 (all this file's)
    Handler popup[5];
    // BattleFx_PoseTask 0x433190's: 0x4331D0, 0x433290 (this file's)
    Handler pose[2];
    // BattleFx_ActorWatch 0x433460's: 0x4334C0 (this file's), 0x433550,
    // 0x433640, 0x433650, 0x433790 (unread)
    Handler watch[5];
    // BattleFx_Follow 0x4337F0's: 0x433810 (this file's)
    Handler follow[1];
    // other modules' and Capcom's
    void (__cdecl* rolling_digits)(unsigned);
    void (__cdecl* draw_number)(int, int, unsigned, unsigned);
    void (__cdecl* draw_label)(int, int, unsigned, unsigned);
    void (__cdecl* free_current)();
    unsigned char (__cdecl* actor_is_out)(unsigned);
    void (__cdecl* set_animation)(unsigned char);
    void (__cdecl* queue_overlay)();
    unsigned char (__cdecl* script_tick)();
    void (__cdecl* update_screen)();
    void (__cdecl* script_flags_set40)();
    void (__cdecl* after_area_script)();
    void (__cdecl* transition_start)(unsigned char);
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (battle_fx_tasks_fuzz.cpp): clones every function of this
// file with each call out re-aimed at a recording stand-in, the six stack
// tables' immediates re-aimed and the popup's jump table relocated in the
// copies, Magic_Rows' code pointers swapped for recorders, runs ours against
// the clones from the same random state, and ends the process through
// bof3::Fatal on any difference.
void SelfTest();

}  // namespace battle_fx_tasks
