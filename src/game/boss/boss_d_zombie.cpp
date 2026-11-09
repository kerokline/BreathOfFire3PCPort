// Moved here by tools/move_funcs.py from boss_sg.cpp (docs/source-layout-round.md).
#include "game/boss/boss_d_zombie.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sg.h"
#include "game/boss_sg_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sg::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

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

}  // namespace

// original 0x43D140: BossKind_Table[38]: jmp [BossDZombie_States + 4 * +1] (12).
extern "C" U __cdecl BossDZombie_Dispatch(unsigned passed) {
    return DispatchBossSg("BossDZombie_Dispatch", AddressOf(BossDZombie_States), 12, 1, passed);
}

// original 0x43D160: state 0: BossDZombie_Anims, BossDZombie_Hook,
// BossDZombie_Sounds, then 0x939AD8's +0x114 |= 8; +1 = 2; tail
// Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossDZombie_Enter(void) {
    StoreKind(AddressOf(BossDZombie_Anims), ::bof3::Code(bof3::addr::BossDZombie_Hook), AddressOf(BossDZombie_Sounds));
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(enemy_rec->flags_114) | 8u);
    return Idle();
}

// original 0x43D1C0: state 6: jmp [BossDZombie_ActSubs + 4 * +2] (6).
extern "C" U __cdecl BossDZombie_ActDispatch(unsigned passed) {
    return DispatchBossSg("BossDZombie_ActDispatch", AddressOf(BossDZombie_ActSubs), 6, 2, passed);
}

// original 0x43D1E0: +2 entry 4, the death: Sprite_EnsureAnimation(0),
// Sprite_ScriptTickOnce (answer not read), Battle_EnemyDefeated, then Fallen.
extern "C" void __cdecl BossDZombie_Death(void) {
    BH_CALL(Sprite_EnsureAnimation)(0);
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    Fallen();
}

// original 0x43D240: the +0xF4 hook through BossDZombie_Hooks (3, all BareRet).
extern "C" U __cdecl BossDZombie_Hook(unsigned word) { return HookTableBossSg("BossDZombie_Hook", AddressOf(BossDZombie_Hooks), word); }

void BossDZombie_Inject() {
    BOF3_INJECT(BossDZombie_Dispatch);
    BOF3_INJECT(BossDZombie_Enter);
    BOF3_INJECT(BossDZombie_ActDispatch);
    BOF3_INJECT(BossDZombie_Death);
    BOF3_INJECT(BossDZombie_Hook);
}
