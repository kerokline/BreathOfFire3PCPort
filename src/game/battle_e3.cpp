// Group BE3 of round twelve (wave one, stage B): 49 functions of the battle
// engine's resident code, 0x437030..0x442F97 - analysis/round12_cut.tsv's 47
// rows for BE3, the unlisted 0x437230 (tools/band_rows.py's flag) and
// 0x441510 (docs/boss_harness.md section 10.7) - each read to its last
// instruction with capstone (2026-09-29) and taken through the boss harness's
// engine frame (boss_harness.h, Group::engine). docs/battle_e3.md has them
// one row each.
//
//   the enemy ops round eleven left   EnemyOp_Steps entries 7, 8 and 9 (the
//                                     cast, its end, the leave) with their
//                                     sub-tables, the action's end 0x4376A0,
//                                     the roll 0x4376F0, the sound 0x437450
//   the party objects' states         BattleObj_StateTable's states 6 (the hit
//                                     taken, with its five sub-trees), 10, 11
//                                     and 26, state 8's sub-state 3, the pose
//                                     helper 0x441510
//   two party helpers                 the stat pass 0x442310 and the loss test
//                                     0x442420
//   a fixed-point helper              0x441090
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables, the task spawns abort on
// BattleTask_Create's 0xFF (all 48 slots taken), the party loops abort on a
// party count past ObjTrio's three and the special attack's copy on an actor
// past them, where the original jumps through, writes past or reads past
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6; nothing reaches it). Every call goes through the harness
// (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in for the
// callees; the callees of other groups of this wave are raw addresses in
// battle_e3_callees.h.
//
// Sprite_Current, Field_State and 0x939AD8 are each re-read wherever the
// original re-reads it (after every call at least). A value the original
// pushes with a register's leftover upper bytes goes out as the byte or word
// its callee reads (each callee read, docs/battle_e3.md section 3), except
// Battle_ApplyDamage's target, whose upper bytes are Field_State's own (the
// register held that pointer) and are passed as such.
#include "game/battle_e3.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e3_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e3::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
U Key(const void* p) { return Low32(p); }
unsigned char* PtrAt(U cell) { return move_script::At(static_cast<U>(move_script::Long(move_script::At(cell)))); }
unsigned char* S() { return Sprite_Current; }
U ULong(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
void SetULong(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// The ability record `id` (24 bytes at 0x65C4D8): a flag byte, or the word +4.
unsigned AbilityId() { return move_script::Word(move_script::At(::bof3::Img(battle_e3::at::kAbility.v))); }
unsigned char AbilityByte(U base) { return move_script::At(base + AbilityId() * battle_e3::at::kAbilityStride)[0]; }
unsigned AbilityWord4() { return move_script::Word(move_script::At(::bof3::Img(battle_e3::at::kAbilityWord4.v) + AbilityId() * battle_e3::at::kAbilityStride)); }

// Sprite_Current is the enemy in the enemy ops, the member in the party
// objects' states; Field_State the member (docs/battle_e3.md 1.1, 1.2).

U EntryBattleE3(U table, unsigned index) { return ULong(move_script::At(table + 4 * index)); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, as read
// (the fuzz swaps the cells for recorders); a Fatal past them (the original
// jumps through the dword after, another table's entry or data).
unsigned CheckedState(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e3.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return state;
}

// The two moves the objects make: +0x34 / +0x38 by the velocity +0xC / +0x10,
// forward or back.
void MoveBy(unsigned char* s, bool back) {
    const U x = ULong(s + 0x34), vx = ULong(s + 0xC);
    SetULong(s + 0x34, back ? x - vx : x + vx);
    const U y = ULong(s + 0x38), vy = ULong(s + 0x10);
    SetULong(s + 0x38, back ? y - vy : y + vy);
}

}  // namespace

// ===========================================================================
// The enemy ops: EnemyOp_Steps entries 7 (the cast), 8 (its end), 9 (the leave)
// ===========================================================================

// ===========================================================================
// The party objects: state 6, the hit taken
// ===========================================================================

// ===========================================================================
// State 6's fall (+2 = 4)
// ===========================================================================

// ===========================================================================
// State 6's HP change (+2 = 5)
// ===========================================================================

// ===========================================================================
// States 8 (sub-state 3), 10, 11 and 26
// ===========================================================================

void BattleE3_Inject() {
    if (bof3::WantsShadow("battle_e3")) battle_e3::SelfTest();
}
