// Moved here by tools/move_funcs.py from boss_sh.cpp, boss_si.cpp (docs/source-layout-round.md).
#include "game/boss/boss_elder.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss/boss_elder_callees.h"
#include "game/boss_harness.h"
#include "game/boss_sh_callees.h"
#include "game/boss_si_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_elder::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll pushes
// nothing; an entry that reads it - Port_DroppedCall's byte, a hook's word -
// reads that word), its eax the dispatcher's answer.
using EntryBossSh = U (__cdecl*)(U);

EntryBossSh EntryAtBossSh(U table, unsigned index) {
    return reinterpret_cast<EntryBossSh>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))));
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
U DispatchBossSh(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sh.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return EntryAtBossSh(table, state)(through);   // the entry as read: the fuzz swaps the table's cells for its recorders
}

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries (the words 0, 1, 2 the action pick, the hit and
// BattleEnemy_RunAll pass), the entry gets the word whole, a Fatal past them.
U HookDispatchBossSh(const char* who, U table, U word) {
    const unsigned index = word & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: the hook's word is 0x%X, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sh.md section 6)",
                    who, (unsigned)word, (unsigned)table);
    return EntryAtBossSh(table, index)(word);
}

// A kind's entrance, the one shape all seven kinds share (0x51 bytes each):
// 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook) and +0xF8 (its
// sound words), 0x939AD8 read for each store; then 0x939AD8 read once more and
// its dword +0x114 |= 8; Sprite_Current +1 = 2; a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char EnterBossSh(U fc, U hook, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(hook);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

using boss_harness::Handler;

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void SetupBossSi(U end, U exit, U event) {
    BattleHook_End = S(end);
    BattleHook_Exit = S(exit);
    BattleHook_Event = S(event);
}

// The set-ups with an end of their own: exit BareRet, event BareRetZero.
void SetupOwnEnd(U end) { SetupBossSi(end, ::bof3::Code(bof3::addr::BareRet), ::bof3::Code(bof3::addr::BareRetZero)); }

// An end hook of five copies (fights 37, 49, 51, 53): with 0x904AE8 bit 1 (the
// win) the chapter's step 0x8034E5 = step and a tail jump to 0x446DE0 (the end
// phase, step 1); otherwise a tail jump to 0x446E00 (step 2).
void EndStepBossSi(unsigned step) {
    if (Battle_EndFlags & 2) {
        MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(step);
        BH_AT(Handler, boss_si::at::kEndWin.v)();
        return;
    }
    BH_AT(Handler, boss_si::at::kEndOther.v)();
}

}  // namespace

// original 0x43EC30: BossKind_Table[44]: jmp [BossElder_States + 4 * +1] (12).
extern "C" unsigned long __cdecl BossElder_Dispatch(unsigned long through) {
    return DispatchBossSh("BossElder_Dispatch", AddressOf(BossElder_States), 12, 1, through);
}

// original 0x43EC50: state 0 - BossElder_Anims / BossElder_Sounds and the hook
// BossElder_Hook.
extern "C" unsigned char __cdecl BossElder_Enter(void) {
    return EnterBossSh(AddressOf(BossElder_Anims), ::bof3::Code(bof3::addr::BossElder_Hook), AddressOf(BossElder_Sounds));
}

// original 0x43ECB0: the +0xF4 hook through BossElder_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossElder_Hook(unsigned long word) {
    return HookDispatchBossSh("BossElder_Hook", AddressOf(BossElder_Hooks), word);
}

// original 0x43ECC0: Boss_SetupTable[37]. End Boss37_End, exit BareRet, event
// BareRetZero.
extern "C" void __cdecl Boss37_Setup(void) { SetupOwnEnd(::bof3::Code(bof3::addr::Boss37_End)); }

// original 0x43ECE0: the end hook: won, the chapter's step 0x1C and step 1;
// otherwise step 2.
extern "C" void __cdecl Boss37_End(void) { EndStepBossSi(0x1C); }

void BossElder_Inject() {
    BOF3_INJECT(BossElder_Dispatch);
    BOF3_INJECT(BossElder_Enter);
    BOF3_INJECT(BossElder_Hook);
    BOF3_INJECT(Boss37_Setup);
    BOF3_INJECT(Boss37_End);
}
