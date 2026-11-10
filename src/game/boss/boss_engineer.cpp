// Moved here by tools/move_funcs.py from boss_sb.cpp (docs/source-layout-round.md).
#include "game/boss/boss_engineer.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
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

using move_script::SetLong;

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

// original 0x438290: BossKind_Table[3]: by Sprite_Current +1 through
// BossEngineer_Steps (12).
extern "C" unsigned long __cdecl BossEngineer_Dispatch(unsigned word) {
    return DispatchBossSb("BossEngineer_Dispatch", AddressOf(BossEngineer_Steps), 12, 1, word);
}

// original 0x4382B0: state 0: +0xFC = BossEngineer_Anims, +0xF4 =
// BossEngineer_Hook, +0xF8 = BossEngineer_Cues; +1 = 2; Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossEngineer_Enter(void) {
    return Enter(AddressOf(BossEngineer_Anims), ::bof3::Code(bof3::addr::BossEngineer_Hook), AddressOf(BossEngineer_Cues));
}

// original 0x4382F0: BossEngineer_Steps 6: by +2 through BossEngineer_ActSubs
// (6: EnemyOp_ActSubs with BossOp_Death at 4).
extern "C" unsigned long __cdecl BossEngineer_ActDispatch(unsigned word) {
    return DispatchBossSb("BossEngineer_ActDispatch", AddressOf(BossEngineer_ActSubs), 6, 2, word);
}

// original 0x438310: BossEngineer_Steps 11: by +2 through
// BossEngineer_HitSteps (3: HitStart, HitStep, BossOp_ScriptTick).
extern "C" unsigned long __cdecl BossEngineer_HitDispatch(unsigned word) {
    return DispatchBossSb("BossEngineer_HitDispatch", AddressOf(BossEngineer_HitSteps), 3, 2, word);
}

// original 0x438330: BossEngineer_HitSteps 0: BattleEnemy_SetAnimation(4);
// then (Sprite_Current read after the call) +9 = 0x3C and +2 up by one.
extern "C" void __cdecl BossEngineer_HitStart(void) {
    BH_CALL(BattleEnemy_SetAnimation)(4);
    Sprite_Current[9] = 0x3C;
    rv::Current<records::BattleEnemy>()->step += 1;
}

// original 0x438350: BossEngineer_HitSteps 1: +9 down by one; at 0:
// Sprite_SetAnimationBank(0x5F); in fight 5 (0x904AAA, read after the call)
// the flip +0x2A = 0 and z +0x38 += 0x18000, otherwise +0x2A = 1 and x +0x34
// += 0x18000; the ground word +0x3E = AreaMap_Elevation(x, z); the enemy's
// +0xFC = BossEngineer_AnimsHit; Sprite_SetAnimation(1); then 0x904AE8 |= 4,
// 0x904AAD |= 1 << (+5 & 0x1F) as a byte (0 from 8 up), +2 up by one. Every
// path ends in Sprite_ScriptTick. Sprite_Current read after each call.
extern "C" unsigned char __cdecl BossEngineer_HitStep(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
    if (Sprite_Current[9] == 0) {
        BH_CALL(Sprite_SetAnimationBank)(0x5F);
        const bool fight5 = EventBattle_Id == 5;
        rv::Current<records::BattleEnemy>()->flip = fight5 ? 0 : 1;
        unsigned char* const axis = Sprite_Current + (fight5 ? 0x38 : 0x34);
        SetLong(axis, static_cast<std::int32_t>(static_cast<U>(Long(axis)) + 0x18000u));
        auto* const cur_enemy = rv::Current<records::BattleEnemy>();
        const long ground = BH_CALL(AreaMap_Elevation)(cur_enemy->x, cur_enemy->z);
        rv::Store<unsigned>(rv::Current<records::BattleEnemy>()->height_whole, static_cast<unsigned>(ground) & 0xFFFF);
        rv::As<records::BattleEnemy>(Enemy())->anim_table = static_cast<std::int32_t>(AddressOf(BossEngineer_AnimsHit));
        BH_CALL(Sprite_SetAnimation)(1);
        Battle_EndFlags |= 4;
        auto* const cur_enemy_t = rv::Current<records::BattleEnemy>();
        const unsigned count = cur_enemy_t->actor & 0x1F;
        BossSe_Script |= static_cast<unsigned char>(count < 8 ? 1u << count : 0u);
        cur_enemy_t->step += 1;
    }
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x438420: kind 3's +0xF4 hook: by the word through
// BossEngineer_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossEngineer_Hook(unsigned word) {
    return HookDispatchBossSb("BossEngineer_Hook", AddressOf(BossEngineer_Hooks), word);
}

void BossEngineer_Inject() {
    BOF3_INJECT(BossEngineer_Dispatch);
    BOF3_INJECT(BossEngineer_Enter);
    BOF3_INJECT(BossEngineer_ActDispatch);
    BOF3_INJECT(BossEngineer_HitDispatch);
    BOF3_INJECT(BossEngineer_HitStart);
    BOF3_INJECT(BossEngineer_HitStep);
    BOF3_INJECT(BossEngineer_Hook);
}
