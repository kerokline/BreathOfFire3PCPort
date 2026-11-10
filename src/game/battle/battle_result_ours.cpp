// The battle result: the PSX BATL_END.EMI's code compiled into the exe
// (docs/battle_result.md). Three .data tables of steps by the byte 0x904AA4,
// under the result phase's own table 0x64AFA0 (by 0x904AA3, dispatched by
// group CC's 0x431910):
//   - 0x64AFAC (entry 0 of 0x64AFA0, through CC's 0x431920): the EXP - split
//     it, open its window, wait for a button, tick it into the party, wait
//     again (0x431940, 0x431A20, 0x431A90, 0x431AB0, 0x431B30);
//   - 0x64AFC0 (entry 1, through BattleResult_LevelUpStep 0x431B60): the
//     level-up search (0x431B80) and its announcement (0x431C10,
//     BattleResult_LevelUpNotice, round twelve BE1's - no route reaches it);
//   - 0x64AFC8 (entry 2, through BattleResult_RewardStep 0x431D50): the
//     level-ups applied, the zenny and the drops' windows (0x431D70), wait
//     (0x432050), tick the zenny in (0x432070), the drops into the
//     inventory (0x4320F0).
// And under the window-kind handler 0x597F60 (group CM's), the result
// window's two states and their draws (0x598570..0x598749).
//
// Every call goes through battle_result::g (battle_result_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Everything here is a faithful replacement. The
// EXP and zenny multipliers (DIV-0045) are applied before any of this, when
// Battle_EnemyDefeated (battle_flow.cpp) adds an enemy's yield to the
// totals 0x904AEC / 0x904AF0 that these functions read.
#include "game/battle_result.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_result_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_result {

using move_script::At;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace battle_result

using namespace battle_result;

namespace {

unsigned char& Byte(std::uint32_t address) { return battle_result::At(address)[0]; }
unsigned char* MemberBattleResult(unsigned slot) { return battle_result::At(::bof3::Img(battle_result::at::kMembers.v) + (slot & 0xFF) * battle_result::at::kMemberSize); }
// MemberBattleResult() is +0x80 into the member record (0x802DC0); the record, and its
// typed view, starts 0x80 before.
char* TextBattleResult(std::uint32_t address) { return reinterpret_cast<char*>(battle_result::At(address)); }
const char* Format(std::uint32_t address) { return reinterpret_cast<const char*>(battle_result::At(address)); }

}  // namespace

// ===========================================================================
// The EXP (0x64AFAC)
// ===========================================================================

// ===========================================================================
// The level-ups (0x64AFC0)
// ===========================================================================

// ===========================================================================
// The zenny and the drops (0x64AFC8)
// ===========================================================================

// ===========================================================================
// The result window's states (under the window-kind handler 0x597F60)
// ===========================================================================

// ===========================================================================

void BattleResult_Inject() {
    if (bof3::WantsShadow("battle_result")) battle_result::SelfTest();
}
