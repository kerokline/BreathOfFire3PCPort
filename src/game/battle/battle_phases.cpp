// The battle's frame and the phase table's first half: the battle mode's
// frame (0x42E370); phase 0, the start - its dispatch (0x42E470),
// Battle_Init (0x42E4A0), the intro's dispatch and its four steps
// (0x42E730..0x42E98D); phase 1, the command input - its dispatch
// (0x42E990), the round start (0x42E9A0), the next member (0x42E9E0), the
// command cross (0x42EAD0), the confirm dispatch (0x42EED0) and Defend
// (0x42EEF0); phase 2, the commit - its dispatch (0x42F070), the commit
// (0x42F080), the enemy messages (0x42F130) and the wait (0x42F1D0).
// docs/battle_phases.md.
//
// Every call goes through battle_phases::g (battle_phases_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Everything here is a faithful replacement.
#include "game/battle_phases.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_phases_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace battle_phases {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace battle_phases

using namespace battle_phases;

namespace {

unsigned char& BBattlePhases(std::uint32_t address) { return *battle_phases::At(address); }
unsigned char* PtrAtBattlePhases(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_phases::Long(battle_phases::At(address)))));
}
unsigned char* MemberBattlePhases(unsigned index) { return battle_phases::At(::bof3::Img(ObjTrio_addr) + index * battle_phases::at::kMemberSize); }
unsigned char* EnemyObject(unsigned index) { return battle_phases::At(::bof3::Img(Battle_Enemies_addr) + index * battle_phases::at::kEnemySize); }
unsigned char* Window(unsigned index) { return battle_phases::At(::bof3::Img(WindowRecords_addr) + index * battle_phases::at::kWindowSize); }

}  // namespace

// ===========================================================================
// The battle's frame
// ===========================================================================

// ===========================================================================
// Phase 0: the battle's start
// ===========================================================================

// original 0x42E4A0 (PSX Battle_Init 0x801D1228): once, at the battle's
// start.
//  1. The banners cleared. 0x904AB1 counts the actors below the byte
//     0x904AB0 (re-read for each) that Battle_ActorIsOut says are in.
//  2. The menu actor is member 0; the battle's globals reset (the turn
//     counter 0x904B90 to 1, the phase sub-bytes, the round flags, the
//     totals, the drop count, 0x904B82 ...).
//  3. The encounter kind, the battle tasks and the actor contexts set up.
//  4. For each member in use: its stat block +0xA0..+0xBF snapshotted to
//     +0xC0; +0x90 bit 13 (the byte +0x91's bit 5) set when the HP +0x98 is
//     below max HP +0xA0 / 4, else cleared; its command fields +0x125,
//     +0x12D, +0x130..+0x13F, +0x142..+0x145 zeroed. Then
//     Formation_ApplyStatMods, and each member in use has +0xC0 copied back
//     over +0xA0 (bit 0 of +0 re-read).
//  5. 16 words 0x80D560 -> 0x811560; CLUT strip rows 0x1A and 0x1B.
//  6. Actors 0..10: each member in use, and each enemy that is not out
//     unless 0x904AAA is 0x25, becomes Sprite_Current for Sprite_SetClutStp.
//  7. Unless 0x904AAA is set, the DAT file 0x904EFC + 0x15D is loaded (the
//     battle's music, PSX: the battle-start sound); 0x904B24..0x904B33
//     zeroed; while 0x904AAA (re-read) is set, the event hook with 6. Then
//     the step on.
//
// As the original has it: the actor passed to Battle_ActorIsOut is a byte
// in a dword whose upper bytes are the caller's ecx; Battle_ActorIsOut reads
// the byte.
//
// As the original has it, and the JP disc: step 2 leaves the target-pick
// flag BattleE4_Picking (0x904AAF) as the last battle left it. The US disc's
// Battle_Init (BATTLE.EMI#3 0x801D1228) adds its clear (US 0x801462EF, the
// JP's 0x801462EB) among the same resets.
//
// DIVERGENCE DIV-0163: 1 through PatchBytes("BattleInitPickClear") in
// BattlePhases_Inject, after the self-test. Step 2 then clears the flag
// with its neighbours, as the US disc does (D349), so a battle cannot start
// with a target highlighted that nobody is picking.
namespace battle_phases { std::uint8_t g_init_pick_clear = 0; }

void BattlePhases_ArmPickClear() {
    const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("BattleInitPickClear", Low32(&battle_phases::g_init_pick_clear), &was, &is, 1);
    bof3::Log("DIV-0163    a battle's start clears the target-pick flag, as the US disc (on unless the line above says OFF)");
}

// ===========================================================================
// Phase 1: the command input
// ===========================================================================

// ===========================================================================
// Phase 2: the commit
// ===========================================================================

// The module's inject, in the live file as every module's is (the 64-bit
// DLL leaves the fuzz files out, docs/x64-build-round.md 2); the self-test
// is battle_phases_fuzz.cpp's, hosted only.
void BattlePhases_Inject() {
    if (bof3::WantsShadow("battle_phases")) battle_phases::SelfTest();
    BattlePhases_ArmPickClear();  // DIV-0163: after the self-test, which compared Capcom's resets
}
