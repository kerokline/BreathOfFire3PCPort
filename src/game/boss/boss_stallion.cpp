// Moved here by tools/move_funcs.py from boss_se.cpp (docs/source-layout-round.md).
#include "game/boss/boss_stallion.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_se.h"
#include "game/boss_se_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_se::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using HookFn = void (__cdecl*)(unsigned);

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

// A kind's entry state: 0x939AD8's +0xFC (its animation bytes), +0xF4 (its
// hook) and +0xF8 (its sound words), 0x939AD8 read for each store - in the
// order the kind's code has them.
void StoreKind(U fc, U f4, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(f4);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
}

}  // namespace

// original 0x43B770: BossKind_Table[29]: jmp [BossStallion_States + 4 * +1] (12).
extern "C" void __cdecl BossStallion_Dispatch(void) {
    DispatchBossSe("BossStallion_Dispatch", AddressOf(BossStallion_States), 12, 1);
}

// original 0x43B790: state 0. 0x939AD8's +0xFC = BossStallion_Anims, +0xF4 =
// BossStallion_Hook, +0xF8 = BossStallion_Sounds; Sprite_Current +1 = 2; a
// tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossStallion_Enter(void) {
    StoreKind(AddressOf(BossStallion_Anims), ::bof3::Code(bof3::addr::BossStallion_Hook), AddressOf(BossStallion_Sounds));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43B7D0: the +0xF4 hook through BossStallion_Hooks (3, all BareRet).
extern "C" void __cdecl BossStallion_Hook(unsigned word) {
    DispatchHook("BossStallion_Hook", AddressOf(BossStallion_Hooks), word);
}

// original 0x43B7E0: BossKind_Table[55]: jmp [BossSample10_States + 4 * +1] (12).
extern "C" void __cdecl BossSample10_Dispatch(void) {
    DispatchBossSe("BossSample10_Dispatch", AddressOf(BossSample10_States), 12, 1);
}

// original 0x43B800: state 0. As BossStallion_Enter with Stallion's two byte
// tables and its own hook, and 0x939AD8's +0x114 |= 8 before +1 = 2 and the
// tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample10_Enter(void) {
    StoreKind(AddressOf(BossStallion_Anims), ::bof3::Code(bof3::addr::BossSample10_Hook), AddressOf(BossStallion_Sounds));
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8);
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43B860: the +0xF4 hook through BossSample10_Hooks (3, all BareRet).
extern "C" void __cdecl BossSample10_Hook(unsigned word) {
    DispatchHook("BossSample10_Hook", AddressOf(BossSample10_Hooks), word);
}

void BossStallion_Inject() {
    BOF3_INJECT(BossStallion_Dispatch);
    BOF3_INJECT(BossStallion_Enter);
    BOF3_INJECT(BossStallion_Hook);
    BOF3_INJECT(BossSample10_Dispatch);
    BOF3_INJECT(BossSample10_Enter);
    BOF3_INJECT(BossSample10_Hook);
}
