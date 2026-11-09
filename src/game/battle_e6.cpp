// Round twelve group BE6 (docs/takeover-queue-field-battle.md section 3,
// analysis/round12_cut.tsv's BE6 rows): 39 battle-engine functions of
// 0x451480..0x4552F6 and 0x4CEB40..0x4CF4A4, each read to its last instruction
// with capstone (2026-09-29) and taken through the boss harness's engine frame
// (boss_harness.h, docs/boss_harness.md section 10). docs/battle_e6.md has
// every function one row each.
//
//   - the transformation (DragonForm_*): the chosen genes (0x904B84, count
//     0x904B87) looked up as a recipe of eleven, or - gene 0x10 in a party of
//     three - a pair of the other members' +0x89 bytes, or mixed from the
//     genes' rows; a recipe's or the mix's stats, bytes and abilities written
//     to the actor's record; the history of six gene sets; the stats rebuilt;
//   - the genes' cost (DragonGenes_SumCost) and the history's last step;
//   - three BattleFx_Dispatch tasks (slots 15, 16, 18) and their states;
//   - the stat rebuild from the buffs (Battle_RecalcStats, a member's and an
//     enemy's), a member's roll for an action and its two tests, the AP pop-up;
//   - the Field_Slots release by owner and start (round eleven's two debts);
//   - BMAGIC's four MapCell_Handlers slots (40..43).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original indexes past a table, divides by a byte that can be 0, reads a
// stack byte it never wrote or loops past its record, ours aborts with a
// message (the owner's rule, round9 doc section 6; known-defects D106 for the
// unwritten stack bytes); each is described in docs/battle_e6.md section 7.
// Every call goes through the harness (BH_CALL / BH_AT), so the start-up fuzz
// can stand recorders in for the callees.
#include "game/battle_e6.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e6_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e6::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

U LBattleE6(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
void SetL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t S8(unsigned char v) { return static_cast<std::int8_t>(v); }
std::int32_t S16BattleE6(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
U Addr(const void* p) { return Low32(p); }

// Sprite_Current in the effect tasks is the task slot (docs/battle_e6.md 2.3).

struct Matrix {
    std::int16_t m[9];
    std::int16_t pad;
    std::int32_t t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a PSX MATRIX");

}  // namespace

// ============================================================================
// The transformation
// ============================================================================

// ============================================================================
// Three battle effect tasks (BattleFx_Dispatch slots 15, 16, 18;
// Sprite_Current the task slot)
// ============================================================================

// ============================================================================
// Stats, rolls, pop-ups
// ============================================================================

// ============================================================================
// Field_Slots (round eleven's debts)
// ============================================================================

// ============================================================================
// BMAGIC's four map cells (MapCell_Handlers 40..43: DrawLayer_Open's
// (record, byte 1, byte 0)); each draws only when Area_TestCondition(the
// record's word) answers
// ============================================================================

// ============================================================================

void BattleE6_Inject() {
    if (bof3::WantsShadow("battle_e6")) battle_e6::SelfTest();
}
