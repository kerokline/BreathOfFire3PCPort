// Group BSH of the boss round: fights 34, 35, 36, 41, 43 and 47 and kinds 41,
// 42, 43, 44, 48, 50 and 54 plus the kind-3 dispatcher's slot 3 - 46 functions
// of 0x43DEF0..0x43ECC0 (tools/boss_rows.py, 2026-09-28; analysis/boss_funcs.tsv's
// group column BSH), each read to its last instruction with capstone
// (2026-09-28) and taken through the boss harness (boss_harness.h). Round
// eleven, wave two; docs/boss_sh.md has them one row each.
//
// The names are the disc's (tools/boss_rows.py --disc, the US disc's area
// records), the fights the tool's rows:
//
//   kind 48   Sample 3 (area 160)       set-ups 34 and 41   BOSS034 (kinds 40 Mikba - group BSG's - and 48)
//   kind 41   Gaist, kind 42 Torch (area 120), kind 54 Sample 9 (area 165)
//                                       set-ups 35 and 47   BOSS035 (= BOSS047 by section md5)
//   kind 43   Angler (area 75), kind 50 Sample 5 (area 162)
//                                       set-ups 36 and 43   BOSS036
//   kind 44   Elder (area 144; its set-up 38 / 44 is group BSI's)
//   slot 3    the kind-3 dispatcher's (BattleBossFx_Dispatch) slot 3: the
//             effect task the Angler's hook starts (BattleTask_Create(3, 3))
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers and hook tables abort past their tables where the original
// jumps through whatever follows; the Angler's hook aborts where the original
// copies into the slot BattleTask_Create's "none free" 0xFF names (past the
// 48 slots and the image); set-up 34's exit hook aborts where the original
// writes through BossActor_Find's null (the owner's rule for an unchecked
// index or pointer, round9 doc section 6; nothing reaches any of them). Every
// call goes through the harness (BH_CALL / BH_AT / Phase), so the start-up
// fuzz can stand recorders in for the callees; a hook or table a function
// stores is the same literal address.
#include "game/boss_sh.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sh_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sh::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll pushes
// nothing; an entry that reads it - Port_DroppedCall's byte, a hook's word -
// reads that word), its eax the dispatcher's answer.
using EntryBossSh = U (__cdecl*)(U);
EntryBossSh EntryAtBossSh(U table, unsigned index) {
    return reinterpret_cast<EntryBossSh>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))));
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
U DispatchBossSh(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sh.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return EntryAtBossSh(table, state)(through);   // the entry as read: the fuzz swaps the table's cells for its recorders
}

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries (the words 0, 1, 2 the action pick, the hit and
// BattleEnemy_RunAll pass), the entry gets the word whole, a Fatal past them.
U HookDispatchBossSh(const char* who, U table, U word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: the hook's word is 0x%X, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sh.md section 6)",
                    who, (unsigned)word, (unsigned)table);
    return EntryAtBossSh(table, index)(word);
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSh(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// A kind's entrance, the one shape all seven kinds share (0x51 bytes each):
// 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook) and +0xF8 (its
// sound words), 0x939AD8 read for each store; then 0x939AD8 read once more and
// its dword +0x114 |= 8; Sprite_Current +1 = 2; a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char EnterBossSh(U fc, U hook, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(hook);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// The end hooks of set-ups 35 and 36: won (0x904AE8 bit 1) - the chapter step
// `step`, then a tail jump to 0x446DE0 (the end phase, step 1); else to
// 0x446E00 (step 2).
void EndWithStep(unsigned char step) {
    if (Battle_EndFlags & 2) {
        MoveScript_PartyRecords[0x65] = step;
        BH_AT(Handler, boss_sh::at::kEndWin.v)();
    } else {
        BH_AT(Handler, boss_sh::at::kEndOther.v)();
    }
}

}  // namespace

// ===========================================================================
// Kind 48 (Sample 3, area 160) and set-ups 34 and 41 (BOSS034)
// ===========================================================================

// ===========================================================================
// Kinds 41 (Gaist), 42 (Torch), 54 (Sample 9) and set-ups 35 and 47
// ===========================================================================

// ===========================================================================
// Kinds 43 (Angler) and 50 (Sample 5), set-ups 36 and 43, and the Angler's
// effect task (the kind-3 dispatcher's slot 3)
// ===========================================================================

// ===========================================================================
// Kind 44 (Elder, area 144)
// ===========================================================================

void BossSh_Inject() {
    if (bof3::WantsShadow("boss_sh")) boss_sh::SelfTest();
}
