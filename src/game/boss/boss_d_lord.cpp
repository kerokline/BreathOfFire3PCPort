// Moved here by tools/move_funcs.py from boss_sj.cpp (docs/source-layout-round.md).
#include "game/boss/boss_d_lord.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sj.h"
#include "game/boss_sj_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sj::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using HookFn = void (__cdecl*)(unsigned);

using ForwardBossSj = U (__cdecl*)(U);

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Enemy() { return reinterpret_cast<unsigned char*>(BattleE3_CurrentEnemy.Native()); }

std::int32_t S(U v) { return static_cast<std::int32_t>(v); }

// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return Low32(p); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after). The jmp
// leaves the caller's stack word in place and answers what the entry
// answers, so ours hands the word on and answers the entry's eax
// (docs/boss_sc.md section 1.2).
U DispatchBossSj(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sj.md section 6)",
                    who, at, state, entries, (unsigned)table);
    // the entry as read: the fuzz swaps the table's cells for its recorders
    return reinterpret_cast<ForwardBossSj>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))(through);
}

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries, the entry gets the caller's word whole, a Fatal past
// them.
void DispatchHook(const char* who, U table, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= 3)
        bof3::Fatal("%s(0x%X): past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sj.md section 6)",
                    who, word, (unsigned)table);
    reinterpret_cast<HookFn>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * i)))))(word);
}

// A kind's entry state (kinds 59 and 61 alike): 0x939AD8's +0xFC (its
// animation bytes), +0xF4 (its hook) and +0xF8 (its sound words), then its
// +0x114 |= 8 - 0x939AD8 read for each; Sprite_Current +1 = 2; a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char EnterBossSj(U fc, U f4, U f8) {
    rv::As<records::BattleEnemy>(Enemy())->anim_table = S(fc);
    rv::As<records::BattleEnemy>(Enemy())->event_hook = S(f4);
    rv::As<records::BattleEnemy>(Enemy())->cue_table = S(f8);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_114 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_114)) | 8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

}  // namespace

// original 0x43F7A0: BossKind_Table[59]. jmp [BossDLord_States + 4 *
// Sprite_Current +1] (12: its entry, the generic enemy states, its act table
// at 6).
extern "C" unsigned long __cdecl BossDLord_Dispatch(unsigned long through) {
    return DispatchBossSj("BossDLord_Dispatch", AddressOf(BossDLord_States), 12, 1, through);
}

// original 0x43F7C0: state 0 (BossDLord_Anims, _Hook, _Sounds; +0x114 |= 8;
// +1 = 2; tail Sprite_ScriptTick).
extern "C" unsigned char __cdecl BossDLord_Enter(void) {
    return EnterBossSj(AddressOf(BossDLord_Anims), ::bof3::Code(bof3::addr::BossDLord_Hook), AddressOf(BossDLord_Sounds));
}

// original 0x43F820: state 6: jmp [BossDLord_ActSubs + 4 * +2] (6: the
// generic act table with BossDLord_Death at 4).
extern "C" unsigned long __cdecl BossDLord_ActDispatch(unsigned long through) {
    return DispatchBossSj("BossDLord_ActDispatch", AddressOf(BossDLord_ActSubs), 6, 2, through);
}

// original 0x43F840: BossDLord_ActSubs 4, the death: Sprite_EnsureAnimation(3),
// Sprite_ScriptTickOnce, Battle_EnemyDefeated; then (read after the calls)
// 0x939AD8's +0x110 |= 0x1000, Sprite_Current +0 &= 0xBF, +1 = 3, +2 = 0,
// +3 = 0. The original leaves al 0; nothing reads it.
extern "C" void __cdecl BossDLord_Death(void) {
    BH_CALL(Sprite_EnsureAnimation)(3);
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy());
    enemy_rec->flags_110 = S(static_cast<U>(static_cast<std::int32_t>(enemy_rec->flags_110)) | 0x1000);
    auto* const cur_enemy = rv::Current<records::BattleEnemy>();
    cur_enemy->flags &= 0xBF;
    cur_enemy->state = 3;
    cur_enemy->step = 0;
    cur_enemy->state3 = 0;
}

// original 0x43F8A0: the +0xF4 hook: by the word's low byte through
// BossDLord_Hooks (BareRet, BareRet, BossDLord_HookFx).
extern "C" void __cdecl BossDLord_Hook(unsigned word) { DispatchHook("BossDLord_Hook", AddressOf(BossDLord_Hooks), word); }

// original 0x43F8B0: BossDLord_Hooks 2 (BattleEnemy_RunAll's word). With
// Sprite_Current +1 = 7, +2 = 1 and +9 = 0: a task of kind 3, parameter 4
// (BattleBossFx_Dispatch's slot 4: BossDLordFx_Dispatch); the acting enemy's
// object (0x93B960 + (the low byte of 0x904B34 - 3) * 0x128, read after the
// call) copied into the slot, 0x20 dwords forward; then the slot's +1 = 0,
// +2 = 0, +6 = 3, +5 = 4, +9 = 0, +0x29 = 3. A slot of 0xFF (none free)
// indexes past the 48: ours aborts.
extern "C" void __cdecl BossDLord_HookFx(unsigned) {
    const unsigned char* const s = Sprite_Current;
    if (s[1] != 7 || s[2] != 1 || s[9] != 0) return;
    const unsigned slot = BH_CALL(BattleTask_Create)(3, 4);
    if (slot >= at::kTaskCount)
        bof3::Fatal("BossDLord_HookFx: BattleTask_Create answered slot %u (none free) - the original writes the enemy's "
                    "0x80 bytes past the 48 slots (docs/boss_sj.md section 6)",
                    slot);
    const U actor = B(::bof3::Img(at::kActor.v));
    const unsigned char* const from = At(::bof3::Img(Battle_Enemies_addr) + (actor - 3u) * at::kEnemyStride);
    unsigned char* const to = At(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride);
    for (unsigned i = 0; i < 0x80; i += 4) SetLong(to + i, Long(from + i));
    to[1] = 0;
    to[2] = 0;
    to[6] = 3;
    to[5] = 4;
    to[9] = 0;
    to[0x29] = 3;
}

void BossDLord_Inject() {
    BOF3_INJECT(BossDLord_Dispatch);
    BOF3_INJECT(BossDLord_Enter);
    BOF3_INJECT(BossDLord_ActDispatch);
    BOF3_INJECT(BossDLord_Death);
    BOF3_INJECT(BossDLord_Hook);
    BOF3_INJECT(BossDLord_HookFx);
}
