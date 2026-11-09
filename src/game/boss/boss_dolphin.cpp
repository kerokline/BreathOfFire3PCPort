// Moved here by tools/move_funcs.py from boss_sg.cpp (docs/source-layout-round.md).
#include "game/boss/boss_dolphin.h"

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

using boss_harness::Handler;

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

}  // namespace

// original 0x43CDE0: BossKind_Table[34]: jmp [BossDolphin_States + 4 * +1] (12).
extern "C" U __cdecl BossDolphin_Dispatch(unsigned passed) {
    return DispatchBossSg("BossDolphin_Dispatch", AddressOf(BossDolphin_States), 12, 1, passed);
}

// original 0x43CE00: state 0. 0x939AD8's +0xFC = BossDolphin_Anims, +0xF4 =
// BossDolphin_Hook, +0xF8 = BossDolphin_Sounds; Sprite_Current +1 = 2; a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossDolphin_Enter(void) {
    StoreKind(AddressOf(BossDolphin_Anims), ::bof3::Code(bof3::addr::BossDolphin_Hook), AddressOf(BossDolphin_Sounds));
    return Idle();
}

// original 0x43CE40: the +0xF4 hook through BossDolphin_Hooks (3, all BareRet).
extern "C" U __cdecl BossDolphin_Hook(unsigned word) { return HookTableBossSg("BossDolphin_Hook", AddressOf(BossDolphin_Hooks), word); }

// original 0x43CE50: Boss_SetupTable[29]. The hooks: end Boss29_End, exit
// BossHook_ExitClearActor0, event BareRetZero.
extern "C" void __cdecl Boss29_Setup(void) {
    StoreHooksBossSg(::bof3::Code(bof3::addr::Boss29_End), ::bof3::Code(bof3::addr::BossHook_ExitClearActor0), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43CE70: the end hook. Won (0x904AE8 bit 1): the chapter's step
// 0x8034E5 = 0x1C, 0x446DE0 (the end phase, step 1) called, then Music_Track =
// 0x62. Otherwise a tail jump to 0x446E00 (step 2).
extern "C" void __cdecl Boss29_End(void) {
    if ((Battle_EndFlags & 2) == 0) {
        BH_AT(Handler, at::kEndOther.v)();
        return;
    }
    MoveScript_PartyRecords[0x65] = 0x1C;
    BH_AT(Handler, at::kEndWin.v)();
    Music_Track = 0x62;
}

void BossDolphin_Inject() {
    BOF3_INJECT(BossDolphin_Dispatch);
    BOF3_INJECT(BossDolphin_Enter);
    BOF3_INJECT(BossDolphin_Hook);
    BOF3_INJECT(Boss29_Setup);
    BOF3_INJECT(Boss29_End);
}
