// Round twelve group BE4 (docs/takeover-queue-field-battle.md section 3): the
// battle engine's 56 functions of 0x444660..0x44AAC9 that
// analysis/round12_cut.tsv lists for BE4 - each read to its last instruction
// with capstone (2026-09-29) and taken through the boss harness as an engine
// group (boss_harness.h, docs/boss_harness.md section 10). docs/battle_e4.md
// has every function one row each.
//
//   - two screen tiles (BattleWin_DimScreen, BattleWin_DrawTileTint);
//   - the turn and damage helpers: the enemy side's pace test, the downward
//     target search, the two hit-or-miss rolls, the party's mean +0xA6, the
//     HP tenth, the records reloaded and written back, the turn order's
//     front, the auto-battle's commands, the two vector turns, a member's
//     reaction roll, the enemy target pick, the status counter's roll, an
//     item given back to its slot, the end phase's three entries;
//   - the item command's states 5..9 (BattleItemCmd_States by 0x904AA3):
//     a whole side's target, the equipment window (its two options, the six
//     slots, the candidates, the equipment put on and taken off) and the
//     equipped item's own target pick, with their two dispatchers;
//   - the escape (0x44A000's table by 0x904AA3, unowned): the roll and its
//     chance, the two paths' steps and their two dispatchers;
//   - three text helpers (a name into Text_Records[0], a banner line).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original divides by a count that can be 0 (Battle_PartyDefenceMean,
// Escape_Roll), indexes its own six stack pointers past the sixth
// (BattleEquip_RemoveSlot) or jumps through a table past its end (the four
// dispatchers), ours aborts with a message (round9 doc section 6). Every call
// goes through the harness (BH_CALL / BH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/battle_e4.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e4_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_e4::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
unsigned char* Ptr(U cell) { return move_script::At(static_cast<U>(move_script::Long(move_script::At(cell)))); }
// The party's records by a byte (0..2 in play), the enemies' objects by an
// actor byte (3..10: object actor - 3), as every original indexes them - the
// byte alone, the product wrapping as 32-bit arithmetic does.
unsigned char* PartyBattleE4(unsigned member) { return move_script::At(::bof3::Img(ObjTrio_addr) + (member & 0xFF) * battle_e4::at::kPartyStride); }
unsigned char* CharRecord(unsigned index) { return move_script::At(::bof3::Img(CharacterRecords_addr) + (index & 0xFF) * battle_e4::at::kCharStride); }
std::int32_t S32BattleE4(U v) { return static_cast<std::int32_t>(v); }

}  // namespace

// ===========================================================================
// The screen tiles
// ===========================================================================

// ===========================================================================
// Turn and damage helpers
// ===========================================================================

// ===========================================================================
// The item command's states 5..9 and the equipment window
// ===========================================================================

// ===========================================================================
// The escape (0x44A000's 0x64E4FC by 0x904AA3: Escape_Roll, the failed path,
// the escaped path)
// ===========================================================================

// ===========================================================================
// Text
// ===========================================================================

// ============================================================================

void BattleE4_Inject() {
    if (bof3::WantsShadow("battle_e4")) battle_e4::SelfTest();
}
