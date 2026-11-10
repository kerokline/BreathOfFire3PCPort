// Moved here by tools/move_funcs.py from boss_se.cpp (docs/source-layout-round.md).
#include "game/boss/boss_beyd.h"

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

using move_script::SetWord;

using move_script::Word;

using HookFn = void (__cdecl*)(unsigned);

unsigned char& B(U address) { return move_script::At(address)[0]; }

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

// Kinds 30 and 31's entry: 0x939AD8's HP +0xA4 and the stat words +0xD0 and
// +0xB0 from the word 0x903F0C, +0xD4 and +0xB4 from 0x903F10, +0xD6 and +0xB6
// from 0x903F12 - each word and 0x939AD8 read for each store.
void CarryStats() {
    SetWord(Enemy() + 0xA4, CharacterRecords[7].stats.max_hp);
    SetWord(Enemy() + 0xD0, CharacterRecords[7].stats.max_hp);
    SetWord(Enemy() + 0xB0, CharacterRecords[7].stats.max_hp);
    SetWord(Enemy() + 0xD4, CharacterRecords[7].stats.atk);
    SetWord(Enemy() + 0xB4, CharacterRecords[7].stats.atk);
    SetWord(Enemy() + 0xD6, CharacterRecords[7].stats.def);
    SetWord(Enemy() + 0xB6, CharacterRecords[7].stats.def);
}

// The death entry (+2 entry 4) of kinds 30, 31 and 32: Sprite_EnsureAnimation
// (the kind's), the Sprite_Current of after the call +0x2A = 0,
// Sprite_ScriptTickOnce (its answer not read), Battle_EnemyDefeated, then the
// Sprite_Current of after the calls +0 &= 0xBF, +1 = 3, +2 = 0, +3 = 0.
void Death(unsigned animation) {
    BH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(animation));
    Sprite_Current[0x2A] = 0;
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    unsigned char* const s = Sprite_Current;
    s[0] &= 0xBF;
    s[1] = 3;
    s[2] = 0;
    s[3] = 0;
}

// Msg_OpenScript(id) with the byte 0x802D20 = 2 first, as every message here.
void Message(unsigned id) {
    BossSa_WindowPass = 2;
    BH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id));
}

}  // namespace

// original 0x43B8F0: BossKind_Table[30]: jmp [BossBeyd_States + 4 * +1] (12).
extern "C" void __cdecl BossBeyd_Dispatch(void) { DispatchBossSe("BossBeyd_Dispatch", AddressOf(BossBeyd_States), 12, 1); }

// original 0x43B910: state 0. 0x939AD8's +0xFC = BossBeyd_Anims, +0xF4 =
// BossBeyd_Hook, +0xF8 = BossBeyd_Sounds; Sprite_Current +1 = 2;
// Sprite_ScriptTick called (its answer not kept: no caller reads one). In fight
// 0x1A (26) only: Battle_CopyEnemyData(0, EnemyData_FindByTag(0x59)), then
// 0x939AD8's +0x8F = 1, its kind +0x100 = 0x1E (30) and CarryStats.
extern "C" void __cdecl BossBeyd_Enter(void) {
    StoreKind(AddressOf(BossBeyd_Anims), ::bof3::Code(bof3::addr::BossBeyd_Hook), AddressOf(BossBeyd_Sounds));
    rv::Current<records::BattleEnemy>()->state = 2;
    BH_CALL(Sprite_ScriptTick)();
    if (EventBattle_Id != 0x1A) return;
    const unsigned char id = BH_CALL(EnemyData_FindByTag)(0x59);
    BH_CALL(Battle_CopyEnemyData)(0, id);
    rv::As<records::BattleEnemy>(Enemy())->kind_flag = 1;
    rv::As<records::BattleEnemy>(Enemy())->run_state = 0x1E;
    CarryStats();
}

// original 0x43BA10: state 6: jmp [BossBeyd_ActSubs + 4 * +2] (6: the generic
// EnemyOp_ActSubs with the kind's death at 4).
extern "C" void __cdecl BossBeyd_ActDispatch(void) {
    DispatchBossSe("BossBeyd_ActDispatch", AddressOf(BossBeyd_ActSubs), 6, 2);
}

// original 0x43BA30: +2 entry 4, the death (animation 0xF); in fight 0x19
// (25) then Msg_OpenScript(0x20).
extern "C" void __cdecl BossBeyd_Death(void) {
    Death(0xF);
    if (EventBattle_Id == 0x19) Message(0x20);
}

// original 0x43BA90: the +0xF4 hook through BossBeyd_Hooks (3: BossBeyd_HookPick,
// BareRet, BossBeyd_HookTick).
extern "C" void __cdecl BossBeyd_Hook(unsigned word) { DispatchHook("BossBeyd_Hook", AddressOf(BossBeyd_Hooks), word); }

// original 0x43BAA0: hook entry 0 (the action pick's call). In fight 0x19
// only: 0x904AE2 at 1 or above clears bit 1 of 0x939AD8's +0x110; then the
// acting kind 0x904B35 = 1. The word is not read.
extern "C" void __cdecl BossBeyd_HookPick(unsigned) {
    if (EventBattle_Id != 0x19) return;
    if (BossSe_RoundSlot >= 1) {
        auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
        enemy_rec->flags_110 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_110)) & 0xFFFFFFFDu);
    }
    B(::bof3::Img(at::kActKind.v)) = 1;
}

// original 0x43BAD0: hook entry 2 (BattleEnemy_RunAll's, each frame). With
// round-flag bit 1 and Transition_Busy 0: BossActor_ClearBit40(0), then
// the round flags' word &= 0xFFFD and Draw_PassFlags = 0.
extern "C" void __cdecl BossBeyd_HookTick(unsigned) {
    if ((B(::bof3::Img(at::kFlags.v)) & 2) == 0) return;
    if (Transition_Busy != 0) return;
    BH_CALL(BossActor_ClearBit40)(0);
    SetWord(At(::bof3::Img(at::kFlags.v)), Word(At(::bof3::Img(at::kFlags.v))) & 0xFFFDu);
    Draw_PassFlags = 0;
}

// original 0x43BB00: BossKind_Table[31]: jmp [BossBeyd2_States + 4 * +1] (12).
extern "C" void __cdecl BossBeyd2_Dispatch(void) { DispatchBossSe("BossBeyd2_Dispatch", AddressOf(BossBeyd2_States), 12, 1); }

// original 0x43BB20: state 0. Kind 30's two byte tables and its own hook;
// Sprite_Current +1 = 2; Sprite_ScriptTick called (answer not kept); the
// Sprite_Current of after the call +8 = 1; 0x939AD8's +0x8F = 1 and
// CarryStats - in every fight.
extern "C" void __cdecl BossBeyd2_Enter(void) {
    StoreKind(AddressOf(BossBeyd_Anims), ::bof3::Code(bof3::addr::BossBeyd2_Hook), AddressOf(BossBeyd_Sounds));
    rv::Current<records::BattleEnemy>()->state = 2;
    BH_CALL(Sprite_ScriptTick)();
    rv::Current<records::BattleEnemy>()->layout = 1;
    rv::As<records::BattleEnemy>(Enemy())->kind_flag = 1;
    CarryStats();
}

// original 0x43BC00: state 6: jmp [BossBeyd2_ActSubs + 4 * +2] (6).
extern "C" void __cdecl BossBeyd2_ActDispatch(void) {
    DispatchBossSe("BossBeyd2_ActDispatch", AddressOf(BossBeyd2_ActSubs), 6, 2);
}

// original 0x43BC20: +2 entry 4, the death (animation 0xF).
extern "C" void __cdecl BossBeyd2_Death(void) { Death(0xF); }

// original 0x43BC70: the +0xF4 hook through BossBeyd2_Hooks (3:
// BossBeyd2_HookTarget4, BareRet, BareRet).
extern "C" void __cdecl BossBeyd2_Hook(unsigned word) { DispatchHook("BossBeyd2_Hook", AddressOf(BossBeyd2_Hooks), word); }

// original 0x43BC80: hook entry 0: the battle's target 0x904B44 = 4.
extern "C" void __cdecl BossBeyd2_HookTarget4(unsigned) { B(::bof3::Img(at::kTarget.v)) = 4; }

void BossBeyd_Inject() {
    BOF3_INJECT(BossBeyd_Dispatch);
    BOF3_INJECT(BossBeyd_Enter);
    BOF3_INJECT(BossBeyd_ActDispatch);
    BOF3_INJECT(BossBeyd_Death);
    BOF3_INJECT(BossBeyd_Hook);
    BOF3_INJECT(BossBeyd_HookPick);
    BOF3_INJECT(BossBeyd_HookTick);
    BOF3_INJECT(BossBeyd2_Dispatch);
    BOF3_INJECT(BossBeyd2_Enter);
    BOF3_INJECT(BossBeyd2_ActDispatch);
    BOF3_INJECT(BossBeyd2_Death);
    BOF3_INJECT(BossBeyd2_Hook);
    BOF3_INJECT(BossBeyd2_HookTarget4);
}
