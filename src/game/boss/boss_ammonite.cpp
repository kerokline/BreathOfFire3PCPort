// Moved here by tools/move_funcs.py from boss_si.cpp (docs/source-layout-round.md).
#include "game/boss/boss_ammonite.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_si_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_si::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry as the original's jmp reaches it: the word its caller left on
// the stack, eax back.
using EntryBossSi = unsigned long (__cdecl*)(unsigned);

unsigned long CallEntryBossSi(U table, unsigned index, unsigned word) {
    // the entry as read: the fuzz swaps the table's cells for its recorders
    return reinterpret_cast<EntryBossSi>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))))(word);
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
unsigned long DispatchBossSi(const char* who, U table, unsigned entries, unsigned at, unsigned word) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_si.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return CallEntryBossSi(table, state, word);
}

// mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 * eax]: an enemy's +0xF4
// hook by its word's low byte (3 entries in every table of the group), the
// word itself left for the entry.
unsigned long HookDispatchBossSi(const char* who, U table, unsigned word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: hook word %u is past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_si.md section 6)",
                    who, index, (unsigned)table);
    return CallEntryBossSi(table, index, word);
}

// A kind's state 0, the part every kind of the group shares: 0x939AD8's (read
// again for each store) +0xFC = its animation bytes, +0xF4 = its hook, +0xF8 =
// its cue words, then its +0x114 |= 8 (the dword), and Sprite_Current +1 = 2.
void EnterStores(U fc, U hook, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(hook);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8u);
    Sprite_Current[1] = 2;
}

// ... and a tail jump to Sprite_ScriptTick, whose al is the answer.
unsigned char EnterBossSi(U fc, U hook, U f8) {
    EnterStores(fc, hook, f8);
    return BH_CALL(Sprite_ScriptTick)();
}

}  // namespace

// original 0x43ED00: BossKind_Table[45]. jmp [BossAmmonite_Steps + 4 *
// Sprite_Current +1] (12: its entry, then the generic enemy states).
extern "C" unsigned long __cdecl BossAmmonite_Dispatch(unsigned word) {
    return DispatchBossSi("BossAmmonite_Dispatch", AddressOf(BossAmmonite_Steps), 12, 1, word);
}

// original 0x43ED20: state 0. The stores (BossAmmonite_Anims,
// BossAmmonite_Hook, BossAmmonite_Cues, +0x114 |= 8, +1 = 2); then
// Sprite_ScriptTick is CALLED (its answer dropped); with bit 2 of the chapter's
// flag bits ([0x929ED0], read after the call) set: Battle_CopyEnemyData(the
// current enemy's +5 - 3 as a byte - 0x939AD8 read after EnemyData_FindByTag,
// EnemyData_FindByTag(0x96)). Nothing answered (eax is a callee's leftover;
// BattleEnemy_RunAll reads none).
extern "C" void __cdecl BossAmmonite_Enter(void) {
    EnterStores(AddressOf(BossAmmonite_Anims), ::bof3::Code(bof3::addr::BossAmmonite_Hook), AddressOf(BossAmmonite_Cues));
    BH_CALL(Sprite_ScriptTick)();
    const unsigned char* const bits = At(static_cast<U>(Chapter_FlagRow));
    if (BH_CALL(Flags_Test)(bits, 2) == 0) return;
    const unsigned id = BH_CALL(EnemyData_FindByTag)(0x96);
    const unsigned slot = static_cast<unsigned char>(rv::As<records::BattleEnemy>(Enemy())->actor - 3);
    BH_CALL(Battle_CopyEnemyData)(slot, id);
}

// original 0x43EDB0: the +0xF4 hook: by the word's low byte through
// BossAmmonite_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossAmmonite_Hook(unsigned word) {
    return HookDispatchBossSi("BossAmmonite_Hook", AddressOf(BossAmmonite_Hooks), word);
}

// original 0x43EDC0: BossKind_Table[51]: BossSample6_Steps (12) by +1.
extern "C" unsigned long __cdecl BossSample6_Dispatch(unsigned word) {
    return DispatchBossSi("BossSample6_Dispatch", AddressOf(BossSample6_Steps), 12, 1, word);
}

// original 0x43EDE0: state 0: kind 45's two byte tables, its own hook; a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample6_Enter(void) {
    return EnterBossSi(AddressOf(BossAmmonite_Anims), ::bof3::Code(bof3::addr::BossSample6_Hook), AddressOf(BossAmmonite_Cues));
}

// original 0x43EE40: the +0xF4 hook: BossSample6_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossSample6_Hook(unsigned word) {
    return HookDispatchBossSi("BossSample6_Hook", AddressOf(BossSample6_Hooks), word);
}

void BossAmmonite_Inject() {
    BOF3_INJECT(BossAmmonite_Dispatch);
    BOF3_INJECT(BossAmmonite_Enter);
    BOF3_INJECT(BossAmmonite_Hook);
    BOF3_INJECT(BossSample6_Dispatch);
    BOF3_INJECT(BossSample6_Enter);
    BOF3_INJECT(BossSample6_Hook);
}
