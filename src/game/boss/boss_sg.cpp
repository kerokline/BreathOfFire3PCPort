// Group BSG of the boss round: fights 29, 31, 32 and 33, kinds 34..38 and 40,
// and the kind-3 effect task F6 - 53 functions of 0x43CDE0..0x43E535
// (tools/boss_rows.py, 2026-09-28), each read to its last instruction with
// capstone (2026-09-28) and taken through the boss harness (boss_harness.h).
// Round eleven, wave two; docs/boss_sg.md has them one row each.
//
// The names are the disc's (tools/boss_rows.py --disc, the US disc's area
// records), the fights the tool's rows:
//
//   kind 34     Dolphin (area 175)          set-up 29  BOSS029, area 175 row 7
//   kinds 35, 36  Gisshan, Charyb / Scylla (area 103; set-up 30 is BSE's)
//   kind 37     Garr, the second fight (area 85)   set-up 31  BOSS031, area 85 row 7
//   kind 38     D>Zombie (area 108)         set-up 32  BOSS032, area 108 row 7
//   set-up 33   BOSS033, area 35 row 7 (kind 39, Weretigr, is BSA's)
//   F6          BattleBossFx_Dispatch's slot 6: Weretigr's turn, the task
//               BossWeretigr_State4Fx (BSA's) creates (BattleTask_Create(3, 6))
//   kind 40     Mikba (area 43; set-ups 34 and 41 are BSH's)
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers and hook tables abort past their tables where the original
// jumps through whatever follows (the owner's rule for an unchecked index,
// round9 doc section 6; nothing reaches it). Every call goes through the
// harness (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in for
// the callees; a hook or table a function stores is the same literal address.
#include "game/boss_sg.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sg_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sg::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }
// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
U L(const unsigned char* p) { return static_cast<U>(Long(p)); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll and
// BattleTask_RunAll push nothing; an entry that reads it - Port_DroppedCall's
// byte, a hook's word - reads that word), its eax the dispatcher's.
using ForwardBossSg = U (__cdecl*)(unsigned);
ForwardBossSg EntryBossSg(U table, unsigned index) {
    return reinterpret_cast<ForwardBossSg>(static_cast<std::uintptr_t>(L(At(table + 4 * index))));   // as read: the fuzz swaps the cells
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
U DispatchBossSg(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sg.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return EntryBossSg(table, state)(passed);
}

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries (the words 0, 1, 2 the engine passes), a Fatal past
// them; the entry gets the caller's word whole.
U HookTableBossSg(const char* who, U table, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= 3)
        bof3::Fatal("%s(0x%X): past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sg.md section 6)",
                    who, word, (unsigned)table);
    return EntryBossSg(table, i)(word);
}

// The three set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSg(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// A kind's entry: 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook)
// and +0xF8 (its sound words), 0x939AD8 read for each store.
void StoreKind(U fc, U f4, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(f4);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
}

// The entry's end: Sprite_Current +1 = 2 (the idle step) and a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char Idle() {
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// Kinds 37 and 38's death's tail, after Battle_EnemyDefeated: 0x939AD8's
// +0x110 |= 0x1000, then the Sprite_Current of after the calls +0 &= 0xBF, +1
// = 3, +2 = 0, +3 = 0.
void Fallen() {
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_110 = S(static_cast<U>(enemy_rec->flags_110) | 0x1000u);
    unsigned char* const s = Sprite_Current;
    s[0] &= 0xBF;
    s[1] = 3;
    s[2] = 0;
    s[3] = 0;
}

// The exit hooks of set-ups 31 and 32: BossActor_ClearBit40(0), the actor
// BossActor_Find(0) made Field_ActiveMember and Sprite_Current,
// Sprite_SetAnimation(animation).
void ActorZero(unsigned animation) {
    BH_CALL(BossActor_ClearBit40)(0);
    unsigned char* const actor = BH_CALL(BossActor_Find)(0);
    Field_ActiveMember = actor;
    Sprite_Current = actor;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
}

}  // namespace

// ===========================================================================
// Kind 34 (Dolphin, area 175) and set-up 29 (BOSS029, area 175 row 7)
// ===========================================================================

// ===========================================================================
// Kinds 35 (Gisshan) and 36 (Charyb / Scylla), area 103 - set-up 30 is BSE's
// ===========================================================================

// ===========================================================================
// Kind 37 (Garr, area 85) and set-up 31 (BOSS031, area 85 row 7)
// ===========================================================================

// ===========================================================================
// Kind 38 (D>Zombie, area 108) and set-up 32 (BOSS032, area 108 row 7)
// ===========================================================================

// ===========================================================================
// Set-up 33 (BOSS033, area 35 row 7; kind 39, Weretigr, is BSA's)
// ===========================================================================

// ===========================================================================
// F6: BattleBossFx_Dispatch's slot 6 - Weretigr's turn (a copy of the acting
// enemy's object made a task by BossWeretigr_State4Fx, group BSA's) and its
// trail (copies of the task made by its strike). Sprite_Current is the task's
// slot throughout.
// ===========================================================================

// The trail's shade: +0x5D, +0x5F, +0x5E = the low byte of +0xB * 0xF0 (the
// byte read again for each).
void Shade() {
    Sprite_Current[0x5D] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
    Sprite_Current[0x5F] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
    Sprite_Current[0x5E] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
}

// ===========================================================================
// Kind 40 (Mikba, area 43) - set-ups 34 and 41 are BSH's
// ===========================================================================

void BossSg_Inject() {
    if (bof3::WantsShadow("boss_sg")) boss_sg::SelfTest();
}
