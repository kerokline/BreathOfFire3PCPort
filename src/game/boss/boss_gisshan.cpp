// Moved here by tools/move_funcs.py from boss_se.cpp, boss_sg.cpp (docs/source-layout-round.md).
#include "game/boss/boss_gisshan.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_gisshan_callees.h"
#include "game/boss_harness.h"
#include "game/boss_se.h"
#include "game/boss_se_callees.h"
#include "game/boss_sg.h"
#include "game/boss_sg_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_gisshan::at;

}  // namespace

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

unsigned char& B(U address) { return move_script::At(address)[0]; }

// Sprite_Current (rv::Current<T>()): the boss enemy in its kind's handlers,
// the slot in an effect task.
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooksBossSe(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

using move_script::Long;

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

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

}  // namespace

// original 0x43CEA0: BossKind_Table[35]: jmp [BossGisshan_States + 4 * +1] (12).
extern "C" U __cdecl BossGisshan_Dispatch(unsigned passed) {
    return DispatchBossSg("BossGisshan_Dispatch", AddressOf(BossGisshan_States), 12, 1, passed);
}

// original 0x43CEC0: state 0: BossGisshan_Anims, BossGisshan_Hook,
// BossGisshan_Sounds; +1 = 2; tail Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossGisshan_Enter(void) {
    StoreKind(AddressOf(BossGisshan_Anims), ::bof3::Code(bof3::addr::BossGisshan_Hook), AddressOf(BossGisshan_Sounds));
    return Idle();
}

// original 0x43CF00: the +0xF4 hook through BossGisshan_Hooks (3, all BareRet).
extern "C" U __cdecl BossGisshan_Hook(unsigned word) { return HookTableBossSg("BossGisshan_Hook", AddressOf(BossGisshan_Hooks), word); }

// original 0x43CF10: BossKind_Table[36]: jmp [BossScylla_States + 4 * +1] (12).
extern "C" U __cdecl BossScylla_Dispatch(unsigned passed) {
    return DispatchBossSg("BossScylla_Dispatch", AddressOf(BossScylla_States), 12, 1, passed);
}

// original 0x43CF30: state 0: BossScylla_Anims, BossScylla_Hook,
// BossScylla_Sounds (the two byte tables lie in kind 35's header); +1 = 2;
// tail Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossScylla_Enter(void) {
    StoreKind(AddressOf(BossScylla_Anims), ::bof3::Code(bof3::addr::BossScylla_Hook), AddressOf(BossScylla_Sounds));
    return Idle();
}

// original 0x43CF70: the +0xF4 hook through BossScylla_Hooks (3, all BareRet).
extern "C" U __cdecl BossScylla_Hook(unsigned word) { return HookTableBossSg("BossScylla_Hook", AddressOf(BossScylla_Hooks), word); }

// original 0x43CF80: Boss_SetupTable[30]. The hooks: end Boss30_End, exit
// BossHook_ExitClearActors012, event BareRetZero.
extern "C" void __cdecl Boss30_Setup(void) {
    StoreHooksBossSe(::bof3::Code(bof3::addr::Boss30_End), ::bof3::Code(bof3::addr::BossHook_ExitClearActors012), ::bof3::Code(bof3::addr::BareRetZero));
}

// original 0x43CFA0: the end hook. Won (0x904AE8 bit 1): variable 3 read, the
// chapter's step 0x8034E5 = 0xC, variable 3 = the value read + 1, 0x446DE0
// (the end phase, step 1) called, then Music_Track = 0x5A. Otherwise a tail
// jump to 0x446E00 (step 2).
extern "C" void __cdecl Boss30_End(void) {
    if ((Battle_EndFlags & 2) == 0) {
        BH_AT(Handler, boss_se::at::kEndOther.v)();
        return;
    }
    const unsigned char var = B(::bof3::Img(at::kScriptVar3.v));
    MoveScript_PartyRecords[0x65] = 0xC;
    B(::bof3::Img(at::kScriptVar3.v)) = static_cast<unsigned char>(var + 1);
    BH_AT(Handler, boss_se::at::kEndWin.v)();
    Music_Track = 0x5A;
}

void BossGisshan_Inject() {
    BOF3_INJECT(BossGisshan_Dispatch);
    BOF3_INJECT(BossGisshan_Enter);
    BOF3_INJECT(BossGisshan_Hook);
    BOF3_INJECT(BossScylla_Dispatch);
    BOF3_INJECT(BossScylla_Enter);
    BOF3_INJECT(BossScylla_Hook);
    BOF3_INJECT(Boss30_Setup);
    BOF3_INJECT(Boss30_End);
}
