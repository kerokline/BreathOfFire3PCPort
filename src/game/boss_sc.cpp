// Round eleven group BSC (docs/takeover-queue-bosses.md section 3): the boss
// band's units B11, K12, B12, K13, K14, K17, B16, K15, K53, B14, B46, K16 and
// B15 - 53 functions of 0x439410..0x43A589 (tools/boss_rows.py, 2026-09-28;
// analysis/boss_funcs.tsv's group column BSC), each read to its last
// instruction with capstone (2026-09-28) and taken through the boss harness
// (boss_harness.h). Enemy names are tools/boss_rows.py --disc's (the US
// disc's area records), not memory of the game; docs/boss_sc.md has every
// function one row each.
//
//   - six set-ups (fights 11, 12, 14, 15, 16, 46): the three hooks stored, and
//     fight 16's wait count drawn from Rand;
//   - their end hooks (the move-script counter 0x903848 and the way out by
//     the win bit), their exit hooks (the field actors found by tag given
//     their poses back), fight 16's event hook (a line by the phase code);
//   - seven kinds' dispatchers by the state byte +1, three second-level
//     dispatchers (+2, +3), seven +0xF4 hook tables by the hook's word;
//   - the kinds' entrances (state 0: the +0xF4 hook and the +0xF8 / +0xFC
//     tables stored), Amalgam's death (four steps and two draws), the
//     Balio / Sunder / Nina hooks of fights 13 and 16.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables where the original jumps through
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6); the exit hooks abort where the original would write through a
// null actor (unreachable: the same tag's BossActor_ClearBit40 aborts first).
// Every call goes through the harness (BH_CALL / BH_AT), so the start-up fuzz
// can stand recorders in for the callees.
#include "game/boss_sc.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sc_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sc::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

// kCurrentEnemy: now BattleE3_CurrentEnemy (symbols.toml).
// kHookExit: now BattleHook_Exit (symbols.toml).
// kHookEvent: now BattleHook_Event (symbols.toml).
// kHookEnd: now BattleHook_End (symbols.toml).
// kPacketNext: now Gfx_PacketNext (symbols.toml).

unsigned char& B(U address) { return move_script::At(address)[0]; }
unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }
U L(const unsigned char* p) { return static_cast<U>(Long(p)); }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    BattleHook_End = static_cast<std::int32_t>(end);
    BattleHook_Exit = static_cast<std::int32_t>(exit);
    BattleHook_Event = static_cast<std::int32_t>(event);
}

// A kind's entrance: 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook)
// and +0xF8, in that order, 0x939AD8 read before each (no call between).
void StoreKindTables(U fc, U f4, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = static_cast<std::int32_t>(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = static_cast<std::int32_t>(f4);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = static_cast<std::int32_t>(f8);
}

using EntryBossSc = U (__cdecl*)(U);
EntryBossSc EntryAt(U table, unsigned index) { return reinterpret_cast<EntryBossSc>(static_cast<std::uintptr_t>(L(At(table + 4 * index)))); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through whatever follows). The jmp
// leaves the stack as the dispatcher found it, so the entry sees the word
// above the return address that the dispatcher saw: ours hands it on (an
// entry that reads it - Port_DroppedCall, a bare ret with a byte - sees what
// it sees under the original), and answers the entry's eax.
U DispatchBossSc(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sc.md section 7)",
                    who, at, state, entries, (unsigned)table);
    return EntryAt(table, state)(through);   // the entry as read: the fuzz swaps the table's cells for its recorders
}

// An enemy's +0xF4 hook table: jmp [table + 4 * (word & 0xFF)], 3 entries.
U HookDispatchBossSc(const char* who, U table, U word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: the hook's word is 0x%X, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sc.md section 7)",
                    who, (unsigned)word, (unsigned)table);
    return EntryAt(table, index)(word);
}

// BossActor_Find(tag) for a hook that writes through the answer: the original
// uses it untested and faults near address 0 when no field actor carries the
// tag. Unreachable after the same tag's BossActor_ClearBit40 (ours aborts
// there first); ours aborts here too rather than write through null.
unsigned char* Actor(const char* who, unsigned tag) {
    unsigned char* const a = BH_CALL(BossActor_Find)(tag);
    if (a == nullptr)
        bof3::Fatal("%s: no field actor carries tag %u - the original writes through a null pointer (docs/boss_sc.md "
                    "section 7)",
                    who, tag);
    return a;
}

// Fights 13 and 16's line: the enemy's action byte, the window pass byte 2,
// then Msg_OpenScript(line).
void Line(unsigned char action, unsigned short line) {
    B(::bof3::Img(at::kAction.v)) = action;
    BossSa_WindowPass = 2;
    BH_CALL(Msg_OpenScript)(line);
}

// The end hooks' common shape: on the win (0x904AE8 bit 1) the move-script
// counter byte 0x903848 = scene and the end phase's step 1, else step 2.
void EndByWin(unsigned char scene) {
    if (Battle_EndFlags & 2) {
        B(::bof3::Img(at::kMoveCounter.v)) = scene;
        BH_AT(Handler, boss_sc::at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, boss_sc::at::kEndOther.v)();
}

}  // namespace

// ============================================================================
// Fight 11 (BOSS008, area 27 row 4)
// ============================================================================

// ============================================================================
// Kind 12 (Amalgam, area 28)
// ============================================================================

// ============================================================================
// Fight 12 (BOSS012, Amalgam, area 28 row 7)
// ============================================================================

// ============================================================================
// Kinds 13 and 14 (Balio, Sunder, areas 11 and 41) - fights 13 and 16
// ============================================================================

namespace {
// Kinds 13 and 14's state 0: the tables, +1 = 2; in fight 13 (0x904AAA 0xD)
// 0x939AD8's words +0xD0, +0xB0 and +0xA4 (HP) = 0xFFFF; then
// Sprite_SetAnimationBank(bank), Sprite_SetAnimation(0), and +0x2A = 1 on
// Sprite_Current read after the calls.
void BalioSunderEnter(U fc, U f4, U f8, unsigned short bank) {
    StoreKindTables(fc, f4, f8);
    Sprite_Current[1] = 2;
    if (EventBattle_Id == 0xD) {
        rv::Store<unsigned>(rv::As<records::BattleEnemy>(Enemy())->base_max_hp, 0xFFFF);
        rv::As<records::BattleEnemy>(Enemy())->max_hp = 0xFFFF;
        rv::As<records::BattleEnemy>(Enemy())->hp = 0xFFFF;
    }
    BH_CALL(Sprite_SetAnimationBank)(bank);
    BH_CALL(Sprite_SetAnimation)(0);
    Sprite_Current[0x2A] = 1;
}
// Kinds 13 and 14's hit hooks' floor (Balio's and Sunder's, fights 13 and 16):
// 0x939AD8's HP +0xA4 at 0 made 1.
void HpFloor() {
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    if (enemy_rec->hp == 0) enemy_rec->hp = 1;
}
}  // namespace

// ============================================================================
// Kind 17 (Nina, area 41) - fights 13 and 16
// ============================================================================

// ============================================================================
// Fight 16 (BOSS013, area 11 or 41 row 7: kinds 13, 14, 17)
// ============================================================================

// ============================================================================
// Kinds 15 and 53 (Rocky, area 26; Sample 8, area 164)
// ============================================================================

// ============================================================================
// Fights 14 and 46 (BOSS014, area 26 row 7; BOSS046)
// ============================================================================

// ============================================================================
// Kind 16 (Pooch, area 26) and fight 15 (BOSS015, area 26 row 6)
// ============================================================================

// ============================================================================

void BossSc_Inject() {
    if (bof3::WantsShadow("boss_sc")) boss_sc::SelfTest();
}
