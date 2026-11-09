// Group BSE of the boss round: fights 22..26, 30 and 48 and kinds 28..32 and
// 55 - 52 functions of 0x43B5B0..0x43E7A0 (tools/boss_rows.py, 2026-09-28;
// analysis/boss_funcs.tsv's group column BSE), each read to its last
// instruction with capstone (2026-09-28) and taken through the boss harness
// (boss_harness.h). Round eleven, wave one, stage B; docs/boss_se.md has them
// one row each.
//
// The names are the disc's (tools/boss_rows.py --disc, the US disc's area
// records), the fights the tool's rows:
//
//   set-up 22   BOSS022, area 80 row 7 (kind 27, Garr - group BSD's)
//   kind 28     Bully 1..3 (area 67)       set-up 23  BOSS023, area 67 row 7
//   kind 29     Stallion (area 67)         set-up 24  BOSS024, area 67 row 6
//   kind 55     Sample10..12 (area 166)    set-up 48  BOSS024, area 166 row 7
//   kinds 30, 31  Beyd, kind 32 Zig (area 92)  set-ups 25 and 26  BOSS025, area 92 row 7
//   set-up 30   BOSS030, area 103 row 7 (kinds 35 and 36 - group BSG's)
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// kinds' dispatchers and hook tables abort past their tables where the
// original jumps through whatever follows (the owner's rule for an unchecked
// index, round9 doc section 6; nothing reaches it). Every call goes through the
// harness (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in for
// the callees; a hook or table a function stores is the same literal address.
#include "game/boss_se.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_se_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_se::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

using HookFn = void (__cdecl*)(unsigned);

unsigned char& B(U address) { return move_script::At(address)[0]; }
unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }
// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
void DispatchBossSe(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_se.md section 6)",
                    who, at, state, entries, (unsigned)table);
    // the entry as read: the fuzz swaps the table's cells for its recorders
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
}

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - the entry gets the caller's word as it was (the jmp keeps the
// stack), three entries (the words 0, 1, 2 BattleEnemy_RunAll and the action
// phases pass), a Fatal past them.
void DispatchHook(const char* who, U table, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= 3)
        bof3::Fatal("%s(0x%X): past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_se.md section 6)",
                    who, word, (unsigned)table);
    reinterpret_cast<HookFn>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * i)))))(word);
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSe(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// A kind's entry state: 0x939AD8's +0xFC (its animation bytes), +0xF4 (its
// hook) and +0xF8 (its sound words), 0x939AD8 read for each store - in the
// order the kind's code has them.
void StoreKind(U fc, U f4, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(f4);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
}

// The death entry (+2 entry 4) of kinds 30, 31 and 32: Sprite_EnsureAnimation
// (the kind's), the Sprite_Current of after the call +0x2A = 0,
// Sprite_ScriptTickOnce (its answer not read), Battle_EnemyDefeated, then the
// Sprite_Current of after the calls +0 &= 0xBF, +1 = 3, +2 = 0, +3 = 0.
void Death(unsigned animation) {
    BH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(animation));
    Sprite_Current[0x2A] = 0;
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    unsigned char* const s = Sprite_Current;
    s[0] &= 0xBF;
    s[1] = 3;
    s[2] = 0;
    s[3] = 0;
}

// Msg_OpenScript(id) with the byte 0x802D20 = 2 first, as every message here.
void Message(unsigned id) {
    BossSa_WindowPass = 2;
    BH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id));
}

}  // namespace

// ===========================================================================
// Set-up 22 (BOSS022, area 80 row 7; its kind 27 is group BSD's)
// ===========================================================================

// ===========================================================================
// Kind 28 (Bully 1..3, area 67) and set-up 23 (BOSS023, area 67 row 7)
// ===========================================================================

// ===========================================================================
// Set-up 30 (BOSS030, area 103 row 7; its kinds 35 and 36 are group BSG's)
// ===========================================================================

// ===========================================================================
// Kind 29 (Stallion, area 67), kind 55 (Sample10..12, area 166), set-ups 24
// (BOSS024, area 67 row 6) and 48 (BOSS024, area 166 row 7)
// ===========================================================================

// ===========================================================================
// Kinds 30 and 31 (Beyd) and 32 (Zig), area 92
// ===========================================================================

// ===========================================================================
// Set-ups 25 and 26 (BOSS025, area 92 row 7)
// ===========================================================================

void BossSe_Inject() {
    if (bof3::WantsShadow("boss_se")) boss_se::SelfTest();
}
