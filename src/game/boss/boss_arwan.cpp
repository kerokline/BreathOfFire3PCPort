// Moved here by tools/move_funcs.py from boss_si.cpp (docs/source-layout-round.md).
#include "game/boss/boss_arwan.h"

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

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

unsigned char& B(U address) { return move_script::At(address)[0]; }

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

// original 0x43F480: BossKind_Table[58]: BossArwan_Steps (12) by +1 (its entry
// at 0, BossArwan_State4Dispatch at 4, BossArwan_State5Await at 5, the generic
// states elsewhere).
extern "C" unsigned long __cdecl BossArwan_Dispatch(unsigned word) {
    return DispatchBossSi("BossArwan_Dispatch", AddressOf(BossArwan_Steps), 12, 1, word);
}

// original 0x43F4A0: state 0: BossArwan_Anims, BossArwan_Hook, BossArwan_Cues;
// a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossArwan_Enter(void) {
    return EnterBossSi(AddressOf(BossArwan_Anims), ::bof3::Code(bof3::addr::BossArwan_Hook), AddressOf(BossArwan_Cues));
}

// original 0x43F500: state 4 (the generic table's turn start EnemyOp_TurnStart 0x4365D0): jmp
// [BossArwan_State4Steps + 4 * Sprite_Current +2] (2: BossArwan_State4Fx,
// BareRet).
extern "C" unsigned long __cdecl BossArwan_State4Dispatch(unsigned word) {
    return DispatchBossSi("BossArwan_State4Dispatch", AddressOf(BossArwan_State4Steps), 2, 2, word);
}

// original 0x43F520: state 4 step 0. Sprite_Current +9 = the +0x8A byte of the
// area's enemy data record the current enemy's +0xF0 indexes (0x8C5652 + 0x8C
// * it); slot n = BattleTask_Create(3, 7) - the kind-3 dispatcher's slot 7,
// BossArwanFx_Dispatch - unchecked; slot n's owner +0x80 = Sprite_Current
// (read after the call), and its +2 up by one (the step then waits on
// BareRet: the task moves the enemy to state 5).
extern "C" void __cdecl BossArwan_State4Fx(void) {
    const unsigned type = rv::As<records::BattleEnemy>(Enemy())->data_id;
    Sprite_Current[9] = At(::bof3::Img(at::kEnemyDataCount.v) + at::kEnemyDataStride * type)[0];
    const unsigned slot = BH_CALL(BattleTask_Create)(3, 7) & 0xFF;
    if (slot >= at::kTaskCount)
        bof3::Fatal("BossArwan_State4Fx: BattleTask_Create answered slot %u, past the %u - the original stores the owner "
                    "at 0x%X (docs/boss_si.md section 6)",
                    slot, at::kTaskCount, (unsigned)(BattleTask_Slots_addr + 0x80 + at::kTaskStride * slot));
    unsigned char* const s = Sprite_Current;
    SetLong(At(::bof3::Img(BattleTask_Slots_addr) + 0x80 + at::kTaskStride * slot), S(AddressOf(s)));
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x43F580: state 5: once 0x904AA8 has bit 2 (the task's last step,
// MagicFx_DoneAndFree, sets it): 0x4376F0, then a tail jump to 0x4376A0 (the
// turn closed). Otherwise nothing.
extern "C" void __cdecl BossArwan_State5Await(void) {
    if (!(B(::bof3::Img(at::kFlags.v)) & 4)) return;
    BH_AT(Handler, at::kTurnChance.v)();
    BH_AT(Handler, at::kTurnClose.v)();
}

// original 0x43F5A0: the +0xF4 hook: BossArwan_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossArwan_Hook(unsigned word) {
    return HookDispatchBossSi("BossArwan_Hook", AddressOf(BossArwan_Hooks), word);
}

// original 0x43F5B0: Boss_SetupTable[51]. End Boss51_End, exit BareRet, event
// BareRetZero.
extern "C" void __cdecl Boss51_Setup(void) { SetupOwnEnd(::bof3::Code(bof3::addr::Boss51_End)); }

// original 0x43F5D0: the end hook: won, the chapter's step 0xF and step 1;
// otherwise step 2.
extern "C" void __cdecl Boss51_End(void) { EndStepBossSi(0xF); }

void BossArwan_Inject() {
    BOF3_INJECT(BossArwan_Dispatch);
    BOF3_INJECT(BossArwan_Enter);
    BOF3_INJECT(BossArwan_State4Dispatch);
    BOF3_INJECT(BossArwan_State4Fx);
    BOF3_INJECT(BossArwan_State5Await);
    BOF3_INJECT(BossArwan_Hook);
    BOF3_INJECT(Boss51_Setup);
    BOF3_INJECT(Boss51_End);
}
