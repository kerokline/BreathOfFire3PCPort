// Moved here by tools/move_funcs.py from boss_se.cpp (docs/source-layout-round.md).
#include "game/boss/boss_bully.h"

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

}  // namespace

// original 0x43B650: BossKind_Table[28]. jmp [BossBully_States + 4 *
// Sprite_Current +1] (12: its entry, then the generic enemy states).
extern "C" void __cdecl BossBully_Dispatch(void) { DispatchBossSe("BossBully_Dispatch", AddressOf(BossBully_States), 12, 1); }

// original 0x43B670: state 0. 0x939AD8's +0xFC = BossBully_AnimsB when
// Sprite_Current +5 is 3, else BossBully_AnimsA; +0xF8 by +5 read again - 3
// BossBully_SoundsA, 4 BossBully_SoundsB, else BossBully_SoundsC; +0xF4 =
// BossBully_Hook; Sprite_Current +1 = 2; a tail jump to Sprite_ScriptTick,
// whose al is the answer.
extern "C" unsigned char __cdecl BossBully_Enter(void) {
    rv::Store<std::int32_t>(rv::As<records::BattleEnemy>(Enemy())->anim_table, S(rv::Current<records::BattleEnemy>()->actor == 3 ? AddressOf(BossBully_AnimsB) : AddressOf(BossBully_AnimsA)));
    const unsigned char slot = rv::Current<records::BattleEnemy>()->actor;
    const U sounds = slot == 3   ? AddressOf(BossBully_SoundsA)
                     : slot == 4 ? AddressOf(BossBully_SoundsB)
                                 : AddressOf(BossBully_SoundsC);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(sounds);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(::bof3::Code(bof3::addr::BossBully_Hook));
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43B700: the +0xF4 hook: by the word's low byte through
// BossBully_Hooks (3, all BareRet).
extern "C" void __cdecl BossBully_Hook(unsigned word) { DispatchHook("BossBully_Hook", AddressOf(BossBully_Hooks), word); }

void BossBully_Inject() {
    BOF3_INJECT(BossBully_Dispatch);
    BOF3_INJECT(BossBully_Enter);
    BOF3_INJECT(BossBully_Hook);
}
