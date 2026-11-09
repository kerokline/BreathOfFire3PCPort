// Group BSB of the boss round: fights 4..10 and 13 and enemy kinds 3, 4, 5
// and 8..11 (chapters 0 and 2) - 52 functions of 0x438290..0x43A022, the ones
// tools/boss_rows.py's group column gives BSB (analysis/boss_funcs.tsv,
// 2026-09-28), each read to its last instruction with capstone (2026-09-28)
// and taken through the boss harness (boss_harness.h). docs/boss_sb.md has
// them one row each.
//
// Names come from the US disc's area records as tools/boss_rows.py --disc
// prints them, never from memory of the game: kind 3 is one script for three
// enemies the tool names Engineer, Foreman and Miner (area 24), named for the
// first; kind 4 Worker (areas 2 and 32), kind 5 Operator (area 2), kinds 8..11
// Torast, Kassen, Galtel, Doksen (area 27). A set-up is Boss<id>_*, its fight
// in its evidence string.
//
//   - the kinds' dispatchers: `jmp [table + 4 * Sprite_Current +1]` through
//     the kind's state table, and each kind's +0xF4 hook, `jmp [table + 4 *
//     (word & 0xFF)]` through its hook table; kind 3's two +2 dispatchers;
//   - the kinds' state 0 (the entrance): the enemy's +0xFC / +0xF4 / +0xF8
//     stored, +1 = 2, Sprite_ScriptTick;
//   - kind 3's hit sequence (its +1 entry 11, where the generic table has
//     EnemyOp_HitPose): pose 4, a wait of 0x3C frames, then a step of 1.5
//     away along x (z in fight 5), the enemy's +0xFC re-pointed, 0x904AE8
//     bit 2 and the enemy's bit in 0x904AAD set;
//   - kind 5's hook entry 1 (the hit): the target block's word +4 cleared;
//   - the set-ups (the three hook stores), their event hooks (a seven-way
//     jump table by the phase code), end hooks (the chapter's step and the
//     way out) and exit hooks (the field actors posed from the enemies).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables where the original jumps through
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6); nothing reaches it. A dispatcher hands its handler the word the
// original's jmp leaves on the stack, and answers what the handler answers.
// Every call goes through the harness (BH_CALL / BH_AT), so the start-up fuzz
// can stand recorders in for the callees.
#include "game/boss_sb.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sb_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sb::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }
// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry as the original's jmp reaches it: the word its caller left on
// the stack, eax back.
using Entry = unsigned long (__cdecl*)(unsigned);
unsigned long CallEntry(U table, unsigned index, unsigned word) {
    // the entry as read: the fuzz swaps the table's cells for its recorders
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))))(word);
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
unsigned long DispatchBossSb(const char* who, U table, unsigned entries, unsigned at, unsigned word) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sb.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return CallEntry(table, state, word);
}

// mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 * eax]: an enemy's +0xF4
// hook by its word's low byte (3 entries in every table of the group), the
// word itself left for the entry.
unsigned long HookDispatchBossSb(const char* who, U table, unsigned word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: hook word %u is past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sb.md section 6)",
                    who, index, (unsigned)table);
    return CallEntry(table, index, word);
}

// A kind's state 0: the enemy's (0x939AD8's) +0xFC animation table, +0xF4
// hook and +0xF8 table, then Sprite_Current +1 = 2 and Sprite_ScriptTick.
void StoreTables(U fc, U hook, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = static_cast<std::int32_t>(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = static_cast<std::int32_t>(hook);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = static_cast<std::int32_t>(f8);
}
unsigned char Enter(U fc, U hook, U f8) {
    StoreTables(fc, hook, f8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

void Setup(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
}

// The event hooks' shared cases (the same code in B04, B05, B06 and B07).
// Phase 0: with round-flag bit 0x40 and the target 0, 0x904AAD bit 0.
void EventRoundFlag() {
    if ((B(::bof3::Img(0x904AA8)) & 0x40) && B(::bof3::Img(at::kTarget.v)) == 0) BossSe_Script |= 1;
}
// The action phase's test: actor 0, command kind 4, command id 0x78.
bool LeaderCommand78() {
    if (B(::bof3::Img(at::kActor.v)) != 0 || B(::bof3::Img(at::kCommandKind.v)) != 4) return false;
    return Word(At(BattleSprites_Action) + 2) == 0x78;
}
// Phase 3: the leader's HP 0 clears its +0x134 bit 1.
void EventLeaderDown() {
    if (ObjTrio[0].hp == 0) rv::Bytes(ObjTrio)[0x134] &= 0xFD;
}
// Phase 5: with 0x904AAD bit 0, the leader's command is kind 4, id 0x78, at
// target 0x40, and the battle's command kind and target the same.
void EventLeaderCommand() {
    if (!(BossSe_Script & 1)) return;
    SetWord(At(::bof3::Img(ObjTrio_addr) + 0x126), 0x78);
    rv::Bytes(ObjTrio)[0x125] = 4;
    B(::bof3::Img(at::kCommandKind.v)) = 4;
    rv::Bytes(ObjTrio)[0x124] = 0x40;
    B(::bof3::Img(at::kTarget.v)) = 0x40;
}

// BossActor_Find(tag) into Sprite_Current. The original writes through the
// answer untested; with no field actor carrying the tag it writes near
// address 0 and faults. Ours aborts there with a message (the owner's rule,
// round9 doc section 6; docs/boss_sb.md section 6), as BH's spawn writers do.
void FindActor(const char* who, unsigned tag) {
    unsigned char* const actor = BH_CALL(BossActor_Find)(tag);
    if (actor == nullptr)
        bof3::Fatal("%s: no field actor carries tag %u - the original writes through the null answer (docs/boss_sb.md section 6)",
                    who, tag);
    Sprite_Current = actor;
}

}  // namespace

// ===========================================================================
// Kind 3 (Engineer, Foreman, Miner - one script; area 24, fights 4, 5, 6)
// ===========================================================================

// ===========================================================================
// Set-ups 4, 5, 6 (BOSS004: area 24 rows 7, 6, 5; kind 3)
// ===========================================================================

// ===========================================================================
// Kinds 4 and 5 (Worker, areas 2 and 32; Operator, area 2; fight 7)
// ===========================================================================

// ===========================================================================
// Set-up 7 (BOSS007: row 7, kinds 4 and 5) and set-up 13 (BOSS013: row 7)
// ===========================================================================

// ===========================================================================
// Kinds 8..11 (Torast, Kassen, Galtel, Doksen; area 27) - the dispatchers,
// entrances and hooks; their shared death is group BH's (BossTorast_*)
// ===========================================================================

// ===========================================================================
// Set-ups 8, 9, 10 (BOSS008: area 27 rows 7, 6, 5)
// ===========================================================================

void BossSb_Inject() {
    if (bof3::WantsShadow("boss_sb")) boss_sb::SelfTest();
}
