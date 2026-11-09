// Round twelve group BE1 (docs/takeover-queue-field-battle.md section 3;
// analysis/round12_cut.tsv's BE1 rows and the two starts the cut lacks):
// 41 functions of the battle engine's first run 0x42D7A0..0x432B6A, each
// read to its last instruction with capstone (2026-09-29) and taken through
// the boss harness's engine frame (boss_harness.h, docs/boss_harness.md
// section 10). docs/battle_e1.md has every function one row each.
//
//   - BATE (game mode 9's overlay, the byte block 0x929F00): state 3's tally
//     - six counters counted up to what 0x939A04 / 0x939A08 / 0x939A0C
//     hold, character record 7's stat words raised by them - and state 2's
//     equipment screen over the same record's two equipment bytes, with
//     their dispatchers by 0x929F01 / 0x929F02;
//   - the command menu's held-button steps (Battle_InputSteps[3] and its
//     three), auto battle (Battle_MenuSteps[5], the PSX Cmd_AutoBattle_Begin
//     by the sibling's notes, verified by reading);
//   - action kind 3's dispatcher and two steps, the random ability pick of
//     actions 0x24 / 0x25 / 0x8C, the ability notice (AfterSteps[1]);
//   - the end's party restore (0x64AF7C[2]), the result's EXP-share count,
//     zenny-bonus test, level-up notice, and a character's level gain from
//     its EXP (the result windows' and BE7's);
//   - the loss screen (0x904AE8 bit 0's way out): its dispatcher, five steps
//     and four draws.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables where the original jumps through
// whatever follows (round9 doc section 6); every other unchecked index is
// reproduced and described in the doc (section 7). Every call goes through
// the harness (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in
// for the callees.
#include "game/battle_e1.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e1_callees.h"
#include "game/boss_harness.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_e1::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& BBattleE1(U address) { return move_script::At(address)[0]; }
U LBattleE1(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }
unsigned char* Packet() { return Gfx_PacketNext; }
unsigned char* Member(U index) { return move_script::At(::bof3::Img(ObjTrio_addr) + index * battle_e1::at::kMemberSize); }
unsigned Pressed() { return Input_Pressed; }
void Rgb(unsigned char* p, unsigned at_, unsigned char v) { p[at_] = p[at_ + 1] = p[at_ + 2] = v; }

// jmp [table + 4 * index]: the entry as read (the fuzz swaps the table's
// cells for its recorders), a Fatal past `entries` (the original jumps
// through the dword after: docs/battle_e1.md section 7). The jmp leaves the
// caller's stack word and the entry's eax in place: ours hands the word on
// and answers the entry's eax (round 11 doc section 5.1).
using Entry = unsigned long (__cdecl*)(unsigned long);
unsigned long Dispatch(const char* who, U table, unsigned index, unsigned entries, unsigned long through) {
    if (index >= entries)
        bof3::Fatal("%s: the step byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e1.md section 7)",
                    who, index, entries, (unsigned)table);
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(LBattleE1(table + 4 * index)))(through);
}


}  // namespace

// ============================================================================
// BATE: state 3, the tally (0x42D760 by 0x929F01, 0x42D770 by 0x929F02)
// ============================================================================

// ============================================================================
// BATE: state 2, the equipment screen (0x42DC00 by 0x929F01)
// ============================================================================

// ============================================================================
// The command menu: the held-button steps and auto battle
// ============================================================================

// ============================================================================
// Action kind 3, the random pick, the ability notice
// ============================================================================

// ============================================================================
// The end and the result
// ============================================================================

// ============================================================================
// The loss screen (0x904AE8 bit 0's way out: BattleEnd_Steps[2], its table
// 0x64AF70 by 0x904AA2, entry 2)
// ============================================================================

// The loss screen's member reset: Sprite_Current the member,
// Sprite_EnsureAnimation(+8 + 0x1C), +0x29 = 1, +0x24 |= 0x80 (read after
// the call), +0x2E / +0x30 the words of BattleLoss_Steps' neighbour rows
// (0x64AFD8 + 24 * the count, re-read, + 8 * the member) - data read in
// place, past the five code pointers -, Sprite_ReleaseTint(member).
// ============================================================================

void BattleE1_Inject() {
    if (bof3::WantsShadow("battle_e1")) battle_e1::SelfTest();
}
