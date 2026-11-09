// Round fourteen group R3B (docs/takeover-queue-round14.md, wave three;
// analysis/round14_cut.tsv's group column): the battle engine's band
// 0x4468B0..0x44CFF4, each function read to its last instruction with
// capstone (2026-10-04) and taken through the boss harness's engine frame
// (boss_harness.h, docs/boss_harness.md section 10). docs/rest_3b.md has
// every function one row each.
//
//   - the result screen's EXP: BattleResult_AddExp (PSX 0x801DD564, the
//     sibling's name, verified), the member test it runs per slot, and
//     CharId_ToRosterIndex (PSX 0x801DD774, the sibling's, verified);
//   - three percent clamps, (value * percent) / 100 within 0..999, 0..9999
//     and 0..100 (the stat rebuilds' and the transformation's);
//   - the command menus' steps no earlier group took: BattleTarget_Picks' 2
//     and 4, BattleItem_Steps' 2 and 5 (a dispatcher, with its table
//     0x64E43C named here) and that table's three, BattleAttackCmd_States' 3,
//     BattleItemCmd_States' 2, 5, 6 (two dispatchers) and the target cancel,
//     and Battle_MenuSteps' 7 (the escape's dispatcher);
//   - 45 Effect_Handlers slots (0..49 less slots 8, 22, 23, 31 and 38, which
//     are battle_odds' and battle_e5's): void (void), as Effect_ApplyResult
//     calls them, filling the result record *0x904B60 (+4 the HP delta,
//     positive is damage; +6 the AP delta; +8 flags).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// four dispatchers abort past their tables where the original jumps through
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6). The handlers index the party and the enemies by an actor or a
// target byte unchecked, as the original does (docs/rest_3b.md section 5):
// ours reads and writes exactly where the original would. Every call goes
// through the harness (BH_CALL / BH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/rest_3b.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3b_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3b::at;
using U = std::uint32_t;
using S32Rest3b = std::int32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
U L(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }
unsigned char* PtrRest3b(U cell) { return move_script::At(L(cell)); }

// The records as the originals index them: a party record by the byte
// itself (0x802D40 + 0x14C n, any n), an enemy object by the byte less 3
// (0x93B960 + 0x128 (n - 3), the difference in 32 bits - a byte below 3
// lands before the enemies). Neither is checked, as in the original.
unsigned char* PartyRest3b(unsigned n) { return move_script::At(::bof3::Img(ObjTrio_addr) + (n & 0xFF) * rest_3b::at::kPartyStride); }
unsigned char* CharRecordRest3b(unsigned roster) { return move_script::At(::bof3::Img(CharacterRecords_addr) + (roster & 0xFF) * rest_3b::at::kCharStride); }
unsigned char* AbilityRow(unsigned id) { return move_script::At(::bof3::Img(rest_3b::at::kAbilityRows.v) + (id & 0xFFFF) * rest_3b::at::kAbilityStride); }

}  // namespace

// ===========================================================================
// The result screen's EXP
// ===========================================================================

// ===========================================================================
// The percent clamps
// ===========================================================================

// ===========================================================================
// The command menus' steps
// ===========================================================================

// ===========================================================================
// Effect_Handlers slots 0..49 (less 8, 22, 23, 31, 38)
// ===========================================================================
//
// Each is called by Effect_ApplyResult through Effect_Handlers 0x64E73C with no
// argument, its eax not read. "The actor" is the byte 0x904B34, "the target"
// the byte 0x904B54, "the result" the record *0x904B60 (read again after each
// call); a member's record 0x802D40 + 0x14C n, an enemy's object 0x93B960 +
// 0x128 (n - 3).

// ============================================================================

void Rest3B_Inject() {
    if (bof3::WantsShadow("rest_3b")) rest_3b::SelfTest();
}
