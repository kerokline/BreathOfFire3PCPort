// Moved here by tools/move_funcs.py from boss_sb.cpp (docs/source-layout-round.md).
#include "game/boss/boss_worker.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sb.h"
#include "game/boss_sb_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sb::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

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

}  // namespace

// original 0x438B30: BossKind_Table[4]: by +1 through BossWorker_Steps (12).
extern "C" unsigned long __cdecl BossWorker_Dispatch(unsigned word) {
    return DispatchBossSb("BossWorker_Dispatch", AddressOf(BossWorker_Steps), 12, 1, word);
}

// original 0x438B50: state 0: +0xFC = BossWorker_Anims, +0xF4 =
// BossWorker_Hook, +0xF8 = BossWorker_Cues; +1 = 2; Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossWorker_Enter(void) {
    return Enter(AddressOf(BossWorker_Anims), ::bof3::Code(bof3::addr::BossWorker_Hook), AddressOf(BossWorker_Cues));
}

// original 0x438B90: kind 4's +0xF4 hook through BossWorker_Hooks (3, BareRet).
extern "C" unsigned long __cdecl BossWorker_Hook(unsigned word) {
    return HookDispatchBossSb("BossWorker_Hook", AddressOf(BossWorker_Hooks), word);
}

void BossWorker_Inject() {
    BOF3_INJECT(BossWorker_Dispatch);
    BOF3_INJECT(BossWorker_Enter);
    BOF3_INJECT(BossWorker_Hook);
}
