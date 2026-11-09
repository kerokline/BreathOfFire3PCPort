// Group BSF of the boss round: fights 27 and 28, kinds 33 (Gazer) and 62
// (Myria), fight 26's count task and the Gazer's effect task - 54 functions of
// 0x43C480..0x4406D7 (tools/boss_rows.py, 2026-09-28; analysis/boss_funcs.tsv's
// group column BSF), each read to its last instruction with capstone
// (2026-09-28) and taken through the boss harness (boss_harness.h). Round
// eleven, wave two; docs/boss_sf.md has them one row each. The ten functions
// of the group already ours (the Paralyzer and Head Cracker rows) are not
// here and nothing here calls them.
//
// The names are the disc's (tools/boss_rows.py --disc, the US disc's area
// records), the fights the tool's rows:
//
//   set-up 27   BOSS027, row 6 - one of the five set-ups the tool leaves open
//               (no kinds of its own; plan section 7): Boss27_*
//   FB8         BattleFx_Dispatch's slot 8: fight 26's count (created by
//               Boss26_Event, group BSE's): Boss26Fx_*
//   kind 33     Gazer (area 77)             set-up 28  BOSS028, area 77 row 7
//   F2          BattleBossFx_Dispatch's slot 2: the Gazer's effect task (its
//               state 4 creates it): BossGazerFx_*
//   kind 62     Myria (area 198; its set-up 55 is group BSJ's): BossMyria_*
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers and hook tables abort past their tables and the three task
// spawns abort on BattleTask_Create's 0xFF (all 48 slots taken), where the
// original jumps through or writes past whatever follows (the owner's rule for
// an unchecked index, round9 doc section 6; nothing reaches it). Every call
// goes through the harness (BH_CALL / BH_AT), so the start-up fuzz can stand
// recorders in for the callees; a hook or table a function stores is the same
// literal address.
#include "game/boss_sf.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sf_callees.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sf::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }
U Key(const void* p) { return Low32(p); }
void SetCell(unsigned char* p, U value) { SetLong(p, static_cast<std::int32_t>(value)); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll and
// BattleTask_RunAll push nothing; an entry that reads it - Port_DroppedCall's
// byte, a hook's code - reads that word), its eax passed back.
using ForwardBossSf = U (__cdecl*)(unsigned);
ForwardBossSf EntryBossSf(U table, unsigned index) { return reinterpret_cast<ForwardBossSf>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index))))); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, as
// read (the fuzz swaps the cells for recorders); a Fatal past them (the
// original jumps through the dword after).
U DispatchBossSf(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sf.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return EntryBossSf(table, state)(passed);
}

// call [table + 4 * Sprite_Current[at]]: the same, as a call (the caller goes
// on after it); the entries read no word.
void Step(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original calls through the dword after "
                    "(docs/boss_sf.md section 6)",
                    who, at, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
}

// An enemy's +0xF4 hook: jmp [table + 4 * (word & 0xFF)], three entries, a
// Fatal past them; the entry gets the hook's word.
U HookDispatchBossSf(const char* who, U table, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= 3)
        bof3::Fatal("%s: hook word %u, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sf.md section 6)",
                    who, i, (unsigned)table);
    return EntryBossSf(table, i)(word);
}

// BattleTask_Create's slot: a Fatal on 0xFF (all 48 taken), where the original
// writes at 0x93A000 + 0xFF * 0x84, past the pool (docs/boss_sf.md section 6).
unsigned char* TaskSlot(const char* who, unsigned n) {
    if (n >= at::kTaskCount)
        bof3::Fatal("%s: BattleTask_Create answered slot %u (all 48 taken) - the original writes at 0x93A000 + %u * 0x84, "
                    "past the pool (docs/boss_sf.md section 6)",
                    who, n, n);
    return At(::bof3::Img(BattleTask_Slots_addr) + n * at::kTaskStride);
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSf(U end, U exit, U event) {
    BattleHook_End = end;
    BattleHook_Exit = exit;
    BattleHook_Event = event;
}

// The area's enemy data record the current enemy's +0xF0 names, byte `field`
// (+0x8A the state 4 count, +0x8B state 7's).
unsigned char EnemyDataByte(unsigned field) {
    const unsigned index = rv::As<records::BattleEnemy>(Enemy())->data_id;
    return At(::bof3::Img(at::kEnemyData.v) + index * at::kEnemyDataStride)[field];
}

// An enemy state's sound: 0x437450 with the first word of the current
// enemy's +0xF8 table.
void EnemySound() { BH_AT(void (__cdecl*)(unsigned), at::kEnemySound.v)(Word(At(static_cast<U>(static_cast<std::int32_t>(rv::As<records::BattleEnemy>(Enemy())->cue_table))))); }

}  // namespace

// ===========================================================================
// Set-up 27 (BOSS027, row 6: the tool resolves no kinds for it)
// ===========================================================================

// ===========================================================================
// FB8: BattleFx_Dispatch's slot 8, fight 26's count (Boss26_Event creates it)
// ===========================================================================

// ===========================================================================
// Kind 33 (Gazer, area 77)
// ===========================================================================

// ===========================================================================
// Set-up 28 (BOSS028, area 77 row 7: the Gazer)
// ===========================================================================

// ===========================================================================
// F2: BattleBossFx_Dispatch's slot 2, the Gazer's effect task
// ===========================================================================

// ===========================================================================
// Kind 62 (Myria, area 198; its set-up 55 is group BSJ's)
// ===========================================================================

void BossSf_Inject() {
    if (bof3::WantsShadow("boss_sf")) boss_sf::SelfTest();
}
