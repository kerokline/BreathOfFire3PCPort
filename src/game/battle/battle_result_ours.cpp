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

// original 0x5985A0 (PSX 0x801F06D4, paired): the EXP window's frame -
// 0x5982D0(0x14, 0x28, 0x118, the record's height +9) - and one line per
// party slot, y = 0x2C + 16 n (a byte): the name (Text_DrawAt at 0x19, 5
// characters); at level 99 (byte +0xA of the slot's record is 0x63)
// Msg_SystemPtr(0x41) at 0x55; otherwise the next level ("%2d" of level +
// 1, Text_DrawFont12 at 0x85), the EXP still to go ("%6d" of 0x598810(n),
// at 0xE3) and Msg_SystemPtr(0x15) at 0x55. Text record 2 is the scratch.
//
// As the original has it: the record 0x905B84 is read once, before the
// frame; the count 0x904AB0 after the frame and after each line; the level
// after the name. The original's frame height and y carry the upper half of
// a register the caller left (ecx, esi) - 0x5982D0, Text_DrawAt and
// Text_DrawFont12 read those arguments as words, and ours passes them
// zero-extended; 0x598810 reads its slot as a byte.
//
// DIVERGENCE DIV-0060: under a Latin language overlay the line takes the US
// release's layout - the EXP still to go right-aligned before the sentence,
// the sentence, the next level at the right edge: "%6d" at 0x36, the
// message at 0x82, "%2d" at 0x114 (read off the owner's photograph of the
// US screen, 2026-09-27; the level-99 message at 0x82 too). The Chinese
// layout puts the sentence at 0x55 and the level at 0x85, which the US
// sentence "EXP to next level:" (144 units) runs over - the owner's "a
// digit hidden in 'next'". Same calls, same order, other x. 0 until
// BattleResult_Inject patches it to 1 under the name BattleResultExpLayout.
unsigned char g_exp_layout_us = 0;

// ===========================================================================

void BattleResult_Inject() {
    if (bof3::WantsShadow("battle_result")) battle_result::SelfTest();
    // DIVERGENCE DIV-0060: a Latin language overlay only, as MenuLists_Inject
    // tests it for DIV-0058.
    {
        if (Lang_Latin()) {
            static const std::uint8_t was = 0, is = 1;
            bof3::PatchBytes("BattleResultExpLayout",
                             Low32(&g_exp_layout_us),
                             &was, &is, 1);
            bof3::Log("DIV-0060    EXP line in the US layout: %s", g_exp_layout_us ? "on" : "off");
        }
    }
}
