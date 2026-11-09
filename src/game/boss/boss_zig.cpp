// Moved here by tools/move_funcs.py from boss_se.cpp (docs/source-layout-round.md).
#include "game/boss/boss_zig.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_se.h"
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

using move_script::Word;

using HookFn = void (__cdecl*)(unsigned);

using SoundFn = void (__cdecl*)(unsigned);

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

}  // namespace

// original 0x43BC90: BossKind_Table[32]: jmp [BossZig_States + 4 * +1] (12).
extern "C" void __cdecl BossZig_Dispatch(void) { DispatchBossSe("BossZig_Dispatch", AddressOf(BossZig_States), 12, 1); }

// original 0x43BCB0: state 0. 0x939AD8's +0xFC = BossZig_Anims, +0xF4 =
// BossZig_Hook, +0xF8 = BossZig_Sounds; Sprite_Current +8 = 3 and +1 = 2; a
// tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossZig_Enter(void) {
    StoreKind(AddressOf(BossZig_Anims), ::bof3::Code(bof3::addr::BossZig_Hook), AddressOf(BossZig_Sounds));
    rv::Current<records::BattleEnemy>()->layout = 3;
    rv::Current<records::BattleEnemy>()->state = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43BD00: state 2 (the generic table's 0x4363B0 in other kinds).
// With 0x904AAD bit 3 set and bit 5 clear: Sprite_EnsureAnimation(0xC); else
// BattleEnemy_SetAnimation(8 with 0x939AD8's +0x110 bit 1, else 0). Then
// BattleEnemy_ScriptTick (answer not read) and the Sprite_Current of after the
// calls +1 up by one.
extern "C" void __cdecl BossZig_Idle(void) {
    const unsigned char script = BossSe_Script;
    if ((script & 8) != 0 && (script & 0x20) == 0) {
        BH_CALL(Sprite_EnsureAnimation)(0xC);
    } else {
        BH_CALL(BattleEnemy_SetAnimation)((Enemy()[0x110] & 2) != 0 ? 8u : 0u);
    }
    BH_CALL(BattleEnemy_ScriptTick)();
    rv::Current<records::BattleEnemy>()->state = static_cast<unsigned char>(rv::Current<records::BattleEnemy>()->state + 1);
}

// original 0x43BD40: state 5: jmp [BossZig_Step5Subs + 4 * +2] (2).
extern "C" void __cdecl BossZig_Step5Dispatch(void) {
    DispatchBossSe("BossZig_Step5Dispatch", AddressOf(BossZig_Step5Subs), 2, 2);
}

// original 0x43BD60: step 5, +2 = 0: BattleEnemy_ScriptTick (answer not read);
// the Sprite_Current of after the call +9 down by one; at 0, the enemy sound
// 0x437450(the word at 0x939AD8's +0xF8) and the Sprite_Current of after that
// +2 up by one.
extern "C" void __cdecl BossZig_Step5Count(void) {
    BH_CALL(BattleEnemy_ScriptTick)();
    unsigned char* const s = Sprite_Current;
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (Sprite_Current[9] != 0) return;
    const unsigned char* const sounds = At(static_cast<U>(static_cast<std::int32_t>(rv::As<records::BattleEnemy>(Enemy())->cue_table)));
    BH_AT(SoundFn, at::kEnemySound.v)(Word(sounds));
    rv::Current<records::BattleEnemy>()->step = static_cast<unsigned char>(rv::Current<records::BattleEnemy>()->step + 1);
}

// original 0x43BDA0: step 5, +2 = 1: when BattleEnemy_ScriptTick answers al not
// 0 - Battle_SetTargetFlag40(the target 0x904B44); Rand, and when its low two
// bits are 0: 0x904AAD read, round-flag bit 7 set, 0x904AAD = the value read |
// 0x10, BattleTask_Create(0, 2); then round-flag bit 2 set and a tail jump to
// 0x4376A0 (the action's end).
extern "C" void __cdecl BossZig_Step5Fire(void) {
    if ((BH_CALL(BattleEnemy_ScriptTick)() & 0xFF) == 0) return;
    BH_CALL(Battle_SetTargetFlag40)(B(::bof3::Img(at::kTarget.v)));
    if ((static_cast<unsigned>(BH_CALL(Rand)()) & 3) == 0) {
        const unsigned char script = BossSe_Script;
        B(::bof3::Img(at::kFlags.v)) |= 0x80;
        BossSe_Script = static_cast<unsigned char>(script | 0x10);
        BH_CALL(BattleTask_Create)(0, 2);
    }
    B(::bof3::Img(at::kFlags.v)) |= 4;
    BH_AT(Handler, at::kEnemyActEnd.v)();
}

// original 0x43BDF0: state 6: jmp [BossZig_ActSubs + 4 * +2] (6).
extern "C" void __cdecl BossZig_ActDispatch(void) { DispatchBossSe("BossZig_ActDispatch", AddressOf(BossZig_ActSubs), 6, 2); }

// original 0x43BE10: +2 entry 4, the death (animation 0xD).
extern "C" void __cdecl BossZig_Death(void) { Death(0xD); }

// original 0x43BE60: the +0xF4 hook through BossZig_Hooks (3: BossZig_HookPick,
// BossZig_HookHit, BareRet).
extern "C" void __cdecl BossZig_Hook(unsigned word) { DispatchHook("BossZig_Hook", AddressOf(BossZig_Hooks), word); }

// original 0x43BE70: hook entry 0 (the action pick): without 0x904AAD bit 3 the
// target 0x904B44 = 3, with it the acting kind 0x904B35 = 0.
extern "C" void __cdecl BossZig_HookPick(unsigned) {
    if ((BossSe_Script & 8) == 0) B(::bof3::Img(at::kTarget.v)) = 3;
    else B(::bof3::Img(at::kActKind.v)) = 0;
}

// original 0x43BE90: hook entry 1 (the hit): 0x904AAD |= 0x20.
extern "C" void __cdecl BossZig_HookHit(unsigned) { BossSe_Script |= 0x20; }

void BossZig_Inject() {
    BOF3_INJECT(BossZig_Dispatch);
    BOF3_INJECT(BossZig_Enter);
    BOF3_INJECT(BossZig_Idle);
    BOF3_INJECT(BossZig_Step5Dispatch);
    BOF3_INJECT(BossZig_Step5Count);
    BOF3_INJECT(BossZig_Step5Fire);
    BOF3_INJECT(BossZig_ActDispatch);
    BOF3_INJECT(BossZig_Death);
    BOF3_INJECT(BossZig_Hook);
    BOF3_INJECT(BossZig_HookPick);
    BOF3_INJECT(BossZig_HookHit);
}
