// Group BSE of the boss round: fights 22..26, 30 and 48 and kinds 28..32 and
// 55 - 52 functions of 0x43B5B0..0x43E7A0 (tools/boss_rows.py, 2026-09-28;
// analysis/boss_funcs.tsv's group column BSE), each read to its last
// instruction with capstone (2026-09-28) and taken through the boss harness
// (boss_harness.h). Round eleven, wave one, stage B; docs/boss_se.md has them
// one row each.
//
// The names are the disc's (tools/boss_rows.py --disc, the US disc's area
// records), the fights the tool's rows:
//
//   set-up 22   BOSS022, area 80 row 7 (kind 27, Garr - group BSD's)
//   kind 28     Bully 1..3 (area 67)       set-up 23  BOSS023, area 67 row 7
//   kind 29     Stallion (area 67)         set-up 24  BOSS024, area 67 row 6
//   kind 55     Sample10..12 (area 166)    set-up 48  BOSS024, area 166 row 7
//   kinds 30, 31  Beyd, kind 32 Zig (area 92)  set-ups 25 and 26  BOSS025, area 92 row 7
//   set-up 30   BOSS030, area 103 row 7 (kinds 35 and 36 - group BSG's)
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// kinds' dispatchers and hook tables abort past their tables where the
// original jumps through whatever follows (the owner's rule for an unchecked
// index, round9 doc section 6; nothing reaches it). Every call goes through the
// harness (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in for
// the callees; a hook or table a function stores is the same literal address.
#include "game/boss_se.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_se_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_se::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using HookFn = void (__cdecl*)(unsigned);
using SoundFn = void (__cdecl*)(unsigned);

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(at::kCurrentEnemy)))); }
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
void Dispatch(const char* who, U table, unsigned entries, unsigned at) {
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

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    SetLong(At(at::kHookEnd), S(end));
    SetLong(At(at::kHookExit), S(exit));
    SetLong(At(at::kHookEvent), S(event));
}

// A kind's entry state: 0x939AD8's +0xFC (its animation bytes), +0xF4 (its
// hook) and +0xF8 (its sound words), 0x939AD8 read for each store - in the
// order the kind's code has them.
void StoreKind(U fc, U f4, U f8) {
    SetLong(Enemy() + 0xFC, S(fc));
    SetLong(Enemy() + 0xF4, S(f4));
    SetLong(Enemy() + 0xF8, S(f8));
}

// Kinds 30 and 31's entry: 0x939AD8's HP +0xA4 and the stat words +0xD0 and
// +0xB0 from the word 0x903F0C, +0xD4 and +0xB4 from 0x903F10, +0xD6 and +0xB6
// from 0x903F12 - each word and 0x939AD8 read for each store.
void CarryStats() {
    SetWord(Enemy() + 0xA4, Word(At(at::kCarryHp)));
    SetWord(Enemy() + 0xD0, Word(At(at::kCarryHp)));
    SetWord(Enemy() + 0xB0, Word(At(at::kCarryHp)));
    SetWord(Enemy() + 0xD4, Word(At(at::kCarryB4)));
    SetWord(Enemy() + 0xB4, Word(At(at::kCarryB4)));
    SetWord(Enemy() + 0xD6, Word(At(at::kCarryB6)));
    SetWord(Enemy() + 0xB6, Word(At(at::kCarryB6)));
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
    B(at::kMsgMode) = 2;
    BH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id));
}

}  // namespace

// ===========================================================================
// Set-up 22 (BOSS022, area 80 row 7; its kind 27 is group BSD's)
// ===========================================================================

// original 0x43B5B0: Boss_SetupTable[22]. The hooks: end Boss22_End, exit
// Boss22_Exit, event BareRetZero.
extern "C" void __cdecl Boss22_Setup(void) {
    StoreHooks(bof3::addr::Boss22_End, bof3::addr::Boss22_Exit, bof3::addr::BareRetZero);
}

// original 0x43B5D0: the end hook. 0x904AE8 and 0x904AE5 read; movement-script
// variable 3 (0x903848) = 0xC, 0x904AE8 |= 8, 0x904AE5 &= 0xBF (the battle's
// music not kept), Music_Track = 0x6A, the byte 0x92BF18 = 3; then a tail jump
// to 0x446E20 (the end phase, step 3) - whatever the battle's end.
extern "C" void __cdecl Boss22_End(void) {
    const unsigned char end = B(at::kBattleEnd);
    const unsigned char music = B(at::kMusicFlags);
    B(at::kScriptVar3) = 0xC;
    B(at::kBattleEnd) = static_cast<unsigned char>(end | 8);
    B(at::kMusicFlags) = static_cast<unsigned char>(music & 0xBF);
    Music_Track = 0x6A;
    B(at::kLeaderPick) = 3;
    BH_AT(Handler, at::kEndThird)();
}

// original 0x43B610: the exit hook. BossActor_ClearBit40(0); Sprite_Current =
// BossActor_Find(0); Sprite_SetAnimationBank(0xAA); the Sprite_Current of after
// the call +0x2A = 0 and +0x48 = 0; Sprite_SetAnimation(6).
extern "C" void __cdecl Boss22_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(0);
    Sprite_Current = BH_CALL(BossActor_Find)(0);
    BH_CALL(Sprite_SetAnimationBank)(0xAA);
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x48] = 0;
    BH_CALL(Sprite_SetAnimation)(6);
}

// ===========================================================================
// Kind 28 (Bully 1..3, area 67) and set-up 23 (BOSS023, area 67 row 7)
// ===========================================================================

// original 0x43B650: BossKind_Table[28]. jmp [BossBully_States + 4 *
// Sprite_Current +1] (12: its entry, then the generic enemy states).
extern "C" void __cdecl BossBully_Dispatch(void) { Dispatch("BossBully_Dispatch", AddressOf(BossBully_States), 12, 1); }

// original 0x43B670: state 0. 0x939AD8's +0xFC = BossBully_AnimsB when
// Sprite_Current +5 is 3, else BossBully_AnimsA; +0xF8 by +5 read again - 3
// BossBully_SoundsA, 4 BossBully_SoundsB, else BossBully_SoundsC; +0xF4 =
// BossBully_Hook; Sprite_Current +1 = 2; a tail jump to Sprite_ScriptTick,
// whose al is the answer.
extern "C" unsigned char __cdecl BossBully_Enter(void) {
    SetLong(Enemy() + 0xFC, S(Sprite_Current[5] == 3 ? AddressOf(BossBully_AnimsB) : AddressOf(BossBully_AnimsA)));
    const unsigned char slot = Sprite_Current[5];
    const U sounds = slot == 3   ? AddressOf(BossBully_SoundsA)
                     : slot == 4 ? AddressOf(BossBully_SoundsB)
                                 : AddressOf(BossBully_SoundsC);
    SetLong(Enemy() + 0xF8, S(sounds));
    SetLong(Enemy() + 0xF4, S(bof3::addr::BossBully_Hook));
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43B700: the +0xF4 hook: by the word's low byte through
// BossBully_Hooks (3, all BareRet).
extern "C" void __cdecl BossBully_Hook(unsigned word) { DispatchHook("BossBully_Hook", AddressOf(BossBully_Hooks), word); }

// original 0x43B710: Boss_SetupTable[23]. The hooks: end 0x43B730 (set-up 21's
// too, group BSD's), exit BossHook_ExitClearActors012, event BareRetZero.
extern "C" void __cdecl Boss23_Setup(void) {
    StoreHooks(at::kEnd21, bof3::addr::BossHook_ExitClearActors012, bof3::addr::BareRetZero);
}

// original 0x43B750: the exit hook of set-ups 23 and 30: BossActor_Clear(0),
// (1), (2).
extern "C" void __cdecl BossHook_ExitClearActors012(void) {
    BH_CALL(BossActor_Clear)(0);
    BH_CALL(BossActor_Clear)(1);
    BH_CALL(BossActor_Clear)(2);
}

// ===========================================================================
// Set-up 30 (BOSS030, area 103 row 7; its kinds 35 and 36 are group BSG's)
// ===========================================================================

// original 0x43CF80: Boss_SetupTable[30]. The hooks: end Boss30_End, exit
// BossHook_ExitClearActors012, event BareRetZero.
extern "C" void __cdecl Boss30_Setup(void) {
    StoreHooks(bof3::addr::Boss30_End, bof3::addr::BossHook_ExitClearActors012, bof3::addr::BareRetZero);
}

// original 0x43CFA0: the end hook. Won (0x904AE8 bit 1): variable 3 read, the
// chapter's step 0x8034E5 = 0xC, variable 3 = the value read + 1, 0x446DE0
// (the end phase, step 1) called, then Music_Track = 0x5A. Otherwise a tail
// jump to 0x446E00 (step 2).
extern "C" void __cdecl Boss30_End(void) {
    if ((B(at::kBattleEnd) & 2) == 0) {
        BH_AT(Handler, at::kEndOther)();
        return;
    }
    const unsigned char var = B(at::kScriptVar3);
    B(at::kChapterStep) = 0xC;
    B(at::kScriptVar3) = static_cast<unsigned char>(var + 1);
    BH_AT(Handler, at::kEndWin)();
    Music_Track = 0x5A;
}

// ===========================================================================
// Kind 29 (Stallion, area 67), kind 55 (Sample10..12, area 166), set-ups 24
// (BOSS024, area 67 row 6) and 48 (BOSS024, area 166 row 7)
// ===========================================================================

// original 0x43B770: BossKind_Table[29]: jmp [BossStallion_States + 4 * +1] (12).
extern "C" void __cdecl BossStallion_Dispatch(void) {
    Dispatch("BossStallion_Dispatch", AddressOf(BossStallion_States), 12, 1);
}

// original 0x43B790: state 0. 0x939AD8's +0xFC = BossStallion_Anims, +0xF4 =
// BossStallion_Hook, +0xF8 = BossStallion_Sounds; Sprite_Current +1 = 2; a
// tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossStallion_Enter(void) {
    StoreKind(AddressOf(BossStallion_Anims), bof3::addr::BossStallion_Hook, AddressOf(BossStallion_Sounds));
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43B7D0: the +0xF4 hook through BossStallion_Hooks (3, all BareRet).
extern "C" void __cdecl BossStallion_Hook(unsigned word) {
    DispatchHook("BossStallion_Hook", AddressOf(BossStallion_Hooks), word);
}

// original 0x43B7E0: BossKind_Table[55]: jmp [BossSample10_States + 4 * +1] (12).
extern "C" void __cdecl BossSample10_Dispatch(void) {
    Dispatch("BossSample10_Dispatch", AddressOf(BossSample10_States), 12, 1);
}

// original 0x43B800: state 0. As BossStallion_Enter with Stallion's two byte
// tables and its own hook, and 0x939AD8's +0x114 |= 8 before +1 = 2 and the
// tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample10_Enter(void) {
    StoreKind(AddressOf(BossStallion_Anims), bof3::addr::BossSample10_Hook, AddressOf(BossStallion_Sounds));
    unsigned char* const e = Enemy();
    SetLong(e + 0x114, S(static_cast<U>(Long(e + 0x114)) | 8));
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43B860: the +0xF4 hook through BossSample10_Hooks (3, all BareRet).
extern "C" void __cdecl BossSample10_Hook(unsigned word) {
    DispatchHook("BossSample10_Hook", AddressOf(BossSample10_Hooks), word);
}

// original 0x43B870: Boss_SetupTable[24]. The hooks: end Boss24_End, exit
// Boss24_Exit, event BareRetZero.
extern "C" void __cdecl Boss24_Setup(void) {
    StoreHooks(bof3::addr::Boss24_End, bof3::addr::Boss24_Exit, bof3::addr::BareRetZero);
}

// original 0x43B890: the end hook. 0x904AE8 read; won (bit 1): variable 3 =
// 5, 0x904AE8 = the value read | 8, Music_Track = 0x44, a tail jump to
// 0x446DE0 (step 1). Otherwise a tail jump to 0x446E00 (step 2).
extern "C" void __cdecl Boss24_End(void) {
    const unsigned char end = B(at::kBattleEnd);
    if ((end & 2) == 0) {
        BH_AT(Handler, at::kEndOther)();
        return;
    }
    B(at::kScriptVar3) = 5;
    B(at::kBattleEnd) = static_cast<unsigned char>(end | 8);
    Music_Track = 0x44;
    BH_AT(Handler, at::kEndWin)();
}

// original 0x43B8C0: the exit hook: BossActor_Clear(3).
extern "C" void __cdecl Boss24_Exit(void) { BH_CALL(BossActor_Clear)(3); }

// original 0x43B8D0: Boss_SetupTable[48]. The hooks: end BossHook_EndPickWay,
// exit BareRet, event BareRetZero.
extern "C" void __cdecl Boss48_Setup(void) {
    StoreHooks(bof3::addr::BossHook_EndPickWay, bof3::addr::BareRet, bof3::addr::BareRetZero);
}

// ===========================================================================
// Kinds 30 and 31 (Beyd) and 32 (Zig), area 92
// ===========================================================================

// original 0x43B8F0: BossKind_Table[30]: jmp [BossBeyd_States + 4 * +1] (12).
extern "C" void __cdecl BossBeyd_Dispatch(void) { Dispatch("BossBeyd_Dispatch", AddressOf(BossBeyd_States), 12, 1); }

// original 0x43B910: state 0. 0x939AD8's +0xFC = BossBeyd_Anims, +0xF4 =
// BossBeyd_Hook, +0xF8 = BossBeyd_Sounds; Sprite_Current +1 = 2;
// Sprite_ScriptTick called (its answer not kept: no caller reads one). In fight
// 0x1A (26) only: Battle_CopyEnemyData(0, EnemyData_FindByTag(0x59)), then
// 0x939AD8's +0x8F = 1, its kind +0x100 = 0x1E (30) and CarryStats.
extern "C" void __cdecl BossBeyd_Enter(void) {
    StoreKind(AddressOf(BossBeyd_Anims), bof3::addr::BossBeyd_Hook, AddressOf(BossBeyd_Sounds));
    Sprite_Current[1] = 2;
    BH_CALL(Sprite_ScriptTick)();
    if (B(at::kFight) != 0x1A) return;
    const unsigned char id = BH_CALL(EnemyData_FindByTag)(0x59);
    BH_CALL(Battle_CopyEnemyData)(0, id);
    Enemy()[0x8F] = 1;
    Enemy()[0x100] = 0x1E;
    CarryStats();
}

// original 0x43BA10: state 6: jmp [BossBeyd_ActSubs + 4 * +2] (6: the generic
// EnemyOp_ActSubs with the kind's death at 4).
extern "C" void __cdecl BossBeyd_ActDispatch(void) {
    Dispatch("BossBeyd_ActDispatch", AddressOf(BossBeyd_ActSubs), 6, 2);
}

// original 0x43BA30: +2 entry 4, the death (animation 0xF); in fight 0x19
// (25) then Msg_OpenScript(0x20).
extern "C" void __cdecl BossBeyd_Death(void) {
    Death(0xF);
    if (B(at::kFight) == 0x19) Message(0x20);
}

// original 0x43BA90: the +0xF4 hook through BossBeyd_Hooks (3: BossBeyd_HookPick,
// BareRet, BossBeyd_HookTick).
extern "C" void __cdecl BossBeyd_Hook(unsigned word) { DispatchHook("BossBeyd_Hook", AddressOf(BossBeyd_Hooks), word); }

// original 0x43BAA0: hook entry 0 (the action pick's call). In fight 0x19
// only: 0x904AE2 at 1 or above clears bit 1 of 0x939AD8's +0x110; then the
// acting kind 0x904B35 = 1. The word is not read.
extern "C" void __cdecl BossBeyd_HookPick(unsigned) {
    if (B(at::kFight) != 0x19) return;
    if (B(at::kRoundSlot) >= 1) {
        unsigned char* const e = Enemy();
        SetLong(e + 0x110, S(static_cast<U>(Long(e + 0x110)) & 0xFFFFFFFDu));
    }
    B(at::kActKind) = 1;
}

// original 0x43BAD0: hook entry 2 (BattleEnemy_RunAll's, each frame). With
// round-flag bit 1 and MoveScript_WaitWordDA 0: BossActor_ClearBit40(0), then
// the round flags' word &= 0xFFFD and Draw_PassFlags = 0.
extern "C" void __cdecl BossBeyd_HookTick(unsigned) {
    if ((B(at::kFlags) & 2) == 0) return;
    if (MoveScript_WaitWordDA != 0) return;
    BH_CALL(BossActor_ClearBit40)(0);
    SetWord(At(at::kFlags), Word(At(at::kFlags)) & 0xFFFDu);
    Draw_PassFlags = 0;
}

// original 0x43BB00: BossKind_Table[31]: jmp [BossBeyd2_States + 4 * +1] (12).
extern "C" void __cdecl BossBeyd2_Dispatch(void) { Dispatch("BossBeyd2_Dispatch", AddressOf(BossBeyd2_States), 12, 1); }

// original 0x43BB20: state 0. Kind 30's two byte tables and its own hook;
// Sprite_Current +1 = 2; Sprite_ScriptTick called (answer not kept); the
// Sprite_Current of after the call +8 = 1; 0x939AD8's +0x8F = 1 and
// CarryStats - in every fight.
extern "C" void __cdecl BossBeyd2_Enter(void) {
    StoreKind(AddressOf(BossBeyd_Anims), bof3::addr::BossBeyd2_Hook, AddressOf(BossBeyd_Sounds));
    Sprite_Current[1] = 2;
    BH_CALL(Sprite_ScriptTick)();
    Sprite_Current[8] = 1;
    Enemy()[0x8F] = 1;
    CarryStats();
}

// original 0x43BC00: state 6: jmp [BossBeyd2_ActSubs + 4 * +2] (6).
extern "C" void __cdecl BossBeyd2_ActDispatch(void) {
    Dispatch("BossBeyd2_ActDispatch", AddressOf(BossBeyd2_ActSubs), 6, 2);
}

// original 0x43BC20: +2 entry 4, the death (animation 0xF).
extern "C" void __cdecl BossBeyd2_Death(void) { Death(0xF); }

// original 0x43BC70: the +0xF4 hook through BossBeyd2_Hooks (3:
// BossBeyd2_HookTarget4, BareRet, BareRet).
extern "C" void __cdecl BossBeyd2_Hook(unsigned word) { DispatchHook("BossBeyd2_Hook", AddressOf(BossBeyd2_Hooks), word); }

// original 0x43BC80: hook entry 0: the battle's target 0x904B44 = 4.
extern "C" void __cdecl BossBeyd2_HookTarget4(unsigned) { B(at::kTarget) = 4; }

// original 0x43BC90: BossKind_Table[32]: jmp [BossZig_States + 4 * +1] (12).
extern "C" void __cdecl BossZig_Dispatch(void) { Dispatch("BossZig_Dispatch", AddressOf(BossZig_States), 12, 1); }

// original 0x43BCB0: state 0. 0x939AD8's +0xFC = BossZig_Anims, +0xF4 =
// BossZig_Hook, +0xF8 = BossZig_Sounds; Sprite_Current +8 = 3 and +1 = 2; a
// tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossZig_Enter(void) {
    StoreKind(AddressOf(BossZig_Anims), bof3::addr::BossZig_Hook, AddressOf(BossZig_Sounds));
    Sprite_Current[8] = 3;
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43BD00: state 2 (the generic table's 0x4363B0 in other kinds).
// With 0x904AAD bit 3 set and bit 5 clear: Sprite_EnsureAnimation(0xC); else
// BattleEnemy_SetAnimation(8 with 0x939AD8's +0x110 bit 1, else 0). Then
// BattleEnemy_ScriptTick (answer not read) and the Sprite_Current of after the
// calls +1 up by one.
extern "C" void __cdecl BossZig_Idle(void) {
    const unsigned char script = B(at::kScript);
    if ((script & 8) != 0 && (script & 0x20) == 0) {
        BH_CALL(Sprite_EnsureAnimation)(0xC);
    } else {
        BH_CALL(BattleEnemy_SetAnimation)((Enemy()[0x110] & 2) != 0 ? 8u : 0u);
    }
    BH_CALL(BattleEnemy_ScriptTick)();
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
}

// original 0x43BD40: state 5: jmp [BossZig_Step5Subs + 4 * +2] (2).
extern "C" void __cdecl BossZig_Step5Dispatch(void) {
    Dispatch("BossZig_Step5Dispatch", AddressOf(BossZig_Step5Subs), 2, 2);
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
    const unsigned char* const sounds = At(static_cast<U>(Long(Enemy() + 0xF8)));
    BH_AT(SoundFn, at::kEnemySound)(Word(sounds));
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

// original 0x43BDA0: step 5, +2 = 1: when BattleEnemy_ScriptTick answers al not
// 0 - Battle_SetTargetFlag40(the target 0x904B44); Rand, and when its low two
// bits are 0: 0x904AAD read, round-flag bit 7 set, 0x904AAD = the value read |
// 0x10, BattleTask_Create(0, 2); then round-flag bit 2 set and a tail jump to
// 0x4376A0 (the action's end).
extern "C" void __cdecl BossZig_Step5Fire(void) {
    if ((BH_CALL(BattleEnemy_ScriptTick)() & 0xFF) == 0) return;
    BH_CALL(Battle_SetTargetFlag40)(B(at::kTarget));
    if ((static_cast<unsigned>(BH_CALL(Rand)()) & 3) == 0) {
        const unsigned char script = B(at::kScript);
        B(at::kFlags) |= 0x80;
        B(at::kScript) = static_cast<unsigned char>(script | 0x10);
        BH_CALL(BattleTask_Create)(0, 2);
    }
    B(at::kFlags) |= 4;
    BH_AT(Handler, at::kEnemyActEnd)();
}

// original 0x43BDF0: state 6: jmp [BossZig_ActSubs + 4 * +2] (6).
extern "C" void __cdecl BossZig_ActDispatch(void) { Dispatch("BossZig_ActDispatch", AddressOf(BossZig_ActSubs), 6, 2); }

// original 0x43BE10: +2 entry 4, the death (animation 0xD).
extern "C" void __cdecl BossZig_Death(void) { Death(0xD); }

// original 0x43BE60: the +0xF4 hook through BossZig_Hooks (3: BossZig_HookPick,
// BossZig_HookHit, BareRet).
extern "C" void __cdecl BossZig_Hook(unsigned word) { DispatchHook("BossZig_Hook", AddressOf(BossZig_Hooks), word); }

// original 0x43BE70: hook entry 0 (the action pick): without 0x904AAD bit 3 the
// target 0x904B44 = 3, with it the acting kind 0x904B35 = 0.
extern "C" void __cdecl BossZig_HookPick(unsigned) {
    if ((B(at::kScript) & 8) == 0) B(at::kTarget) = 3;
    else B(at::kActKind) = 0;
}

// original 0x43BE90: hook entry 1 (the hit): 0x904AAD |= 0x20.
extern "C" void __cdecl BossZig_HookHit(unsigned) { B(at::kScript) |= 0x20; }

// ===========================================================================
// Set-ups 25 and 26 (BOSS025, area 92 row 7)
// ===========================================================================

// original 0x43BEA0: Boss_SetupTable[25]. Party member 0's HP +0x98 and AP
// +0x9A saved - their LOW BYTES only - in 0x675F05 / 0x675F04, and the two words
// set from its +0xA0 / +0xA2; the hooks: end Boss25_End, exit Boss25_Exit,
// event Boss25_Event; 0x904AE4 = 3. In the original's order (every read of a
// cell before its store).
extern "C" void __cdecl Boss25_Setup(void) {
    const unsigned char hp = B(at::kMember0Hp);
    const unsigned max_hp = Word(At(at::kMember0MaxHp));
    const unsigned char ap = B(at::kMember0Ap);
    B(at::kSavedHp) = hp;
    const unsigned max_ap = Word(At(at::kMember0MaxAp));
    StoreHooks(bof3::addr::Boss25_End, bof3::addr::Boss25_Exit, bof3::addr::Boss25_Event);
    B(at::kInitiative) = 3;
    SetWord(At(at::kMember0Hp), max_hp);
    B(at::kSavedAp) = ap;
    SetWord(At(at::kMember0Ap), max_ap);
}

// original 0x43BF00: the event hook, by the code's low byte; al 0 on every
// path.
//   0 (an action phase's): the turn counter 0x904B90 = (0x904AE2 >> 1) + 1;
//     by 0x904AE2 - 1: message 0x1B; 2: 0x802D20 = 2, member 0's action +0x125
//     = 1, message 0x1C; 3: message 0x1D; 4 (unless round-flag bit 6): member
//     0's action = 4 and skill +0x126 = 0x46; 5: message 0x1E; 6 (unless bit
//     6): message 0x1F, then member 0's action = 1, +0xBA = 0x64, enemy 0's HP
//     = 1.
//   3 (Battle_PhaseDispatch's): 0x904AAD's bits in turn -
//     0x20 clear and phase 1: BattleTask_Create(0, 8), bit 0x20 set;
//     0x10 clear, phase 3, step 0: enemy 0's +0x110 |= 2 and +0x104 = 0, +0x105
//       = 2; member 0's target +0x124 = 3, action 1, +0xBC = 0x64, +0xBA = 0; the
//       turn order 0x904ACC = 0, 3, 0, 3, 0, 3, 0 (7); bit 0x10 set (on the
//       value held);
//     then the first of bits 1, 2, 4 clear: message 0x19 / enemy 0 made
//       Sprite_Current with BattleEnemy_SetAnimation(8) / message 0x1A, and that
//       bit set (0x904AAD read again after the call); each returns;
//     all set: 0x904AE2 4 with member 0's +1 at 6 and +2 at 1 - its words
//       +0x128 and +0x12A = 0 and the acting kind 0x904B35 = 0xFF; 0x904AE2 4 or
//       6 with member 0's +1 at 2 and +2 at 0 - round-flag bit 6 set.
//   any other: nothing.
extern "C" unsigned char __cdecl Boss25_Event(unsigned code) {
    const unsigned char c = static_cast<unsigned char>(code);
    if (c == 0) {
        const unsigned char round = B(at::kRoundSlot);
        SetLong(At(at::kTurn), S((static_cast<U>(round) >> 1) + 1));
        switch (round) {
        case 1: Message(0x1B); break;
        case 2:
            B(at::kMsgMode) = round;
            B(at::kMember0Action) = 1;
            BH_CALL(Msg_OpenScript)(0x1C);
            break;
        case 3: Message(0x1D); break;
        case 4:
            if (B(at::kFlags) & 0x40) break;
            B(at::kMember0Action) = 4;
            SetWord(At(at::kMember0Skill), 0x46);
            break;
        case 5: Message(0x1E); break;
        case 6:
            if (B(at::kFlags) & 0x40) break;
            Message(0x1F);
            B(at::kMember0Action) = 1;
            B(at::kMember0Ba) = 0x64;
            SetWord(At(at::kEnemy0Hp), 1);
            break;
        default: break;
        }
        return 0;
    }
    if (c != 3) return 0;
    unsigned char script = B(at::kScript);
    if ((script & 0x20) == 0 && B(at::kPhase) == 1) {
        BH_CALL(BattleTask_Create)(0, 8);
        script = static_cast<unsigned char>(B(at::kScript) | 0x20);
        B(at::kScript) = script;
    }
    if ((script & 0x10) == 0 && B(at::kPhase) == 3 && B(at::kStep) == 0) {
        const U flags = static_cast<U>(Long(At(at::kEnemy0Flags))) | 2;
        B(at::kMember0Target) = 3;
        B(at::kMember0Action) = 1;
        B(at::kMember0Bc) = 0x64;
        B(at::kMember0Ba) = 0;
        B(at::kEnemy0Byte104) = 0;
        SetLong(At(at::kEnemy0Flags), S(flags));
        B(at::kEnemy0Byte105) = 2;
        B(at::kOrderCount) = 7;
        static constexpr unsigned char kOrder[7] = {0, 3, 0, 3, 0, 3, 0};
        for (unsigned i = 0; i < 7; ++i) B(at::kOrder + i) = kOrder[i];
        script = static_cast<unsigned char>(script | 0x10);
        B(at::kScript) = script;
    }
    if ((script & 1) == 0) {
        Message(0x19);
        B(at::kScript) |= 1;
        return 0;
    }
    if ((script & 2) == 0) {
        Sprite_Current = At(at::kEnemy0);
        BH_CALL(BattleEnemy_SetAnimation)(8);
        B(at::kScript) |= 2;
        return 0;
    }
    if ((script & 4) == 0) {
        Message(0x1A);
        B(at::kScript) |= 4;
        return 0;
    }
    const unsigned char round = B(at::kRoundSlot);
    unsigned char member;
    if (round == 4) {
        member = B(at::kMember0State1);
        if (member == 6) {
            if (B(at::kMember0State2) != 1) return 0;
            SetWord(At(at::kMember0Word128), 0);
            SetWord(At(at::kMember0Word12A), 0);
            B(at::kActKind) = 0xFF;
            return 0;
        }
    } else if (round == 6) {
        member = B(at::kMember0State1);
    } else {
        return 0;
    }
    if (member != 2 || B(at::kMember0State2) != 0) return 0;
    B(at::kFlags) |= 0x40;
    return 0;
}

// original 0x43C180: the end hook. Member 0's HP and AP words = the two saved
// bytes (zero-extended); the chapter's step 0x8034E5 = 4; a tail jump to
// 0x446E20 (step 3).
extern "C" void __cdecl Boss25_End(void) {
    const unsigned hp = B(at::kSavedHp);
    const unsigned ap = B(at::kSavedAp);
    SetWord(At(at::kMember0Hp), hp);
    SetWord(At(at::kMember0Ap), ap);
    B(at::kChapterStep) = 4;
    BH_AT(Handler, at::kEndThird)();
}

// original 0x43C1B0: the exit hook. Round-flag bit 1 set; Transition_Start(4);
// then the count byte 0x939A02 read, Field_MemberCount = 3, and that many bytes
// copied from 0x939A10 to the party list 0x904065 (rep movsd, rep movsb:
// forward, the two never overlap).
extern "C" void __cdecl Boss25_Exit(void) {
    B(at::kFlags) |= 2;
    BH_CALL(Transition_Start)(4);
    const unsigned n = B(at::kPartyCopyCount);
    Field_MemberCount = 3;
    if (n != 0) std::memcpy(At(at::kPartyList), At(at::kPartyCopyFrom), n);
}

// original 0x43C200: Boss_SetupTable[26]. The hooks: end Boss26_End, exit
// BossHook_ExitTransition4, event Boss26_Event; then the two sums 0x939A08 and
// 0x939A04 and the count 0x939A0C = 0.
extern "C" void __cdecl Boss26_Setup(void) {
    StoreHooks(bof3::addr::Boss26_End, bof3::addr::BossHook_ExitTransition4, bof3::addr::Boss26_Event);
    SetLong(At(at::kSumMember), 0);
    SetLong(At(at::kSumEnemy), 0);
    B(at::kCountFlagged) = 0;
}

// original 0x43C230: the event hook, by the code's low byte (a jump table of
// four inside the function); al 0 on every path.
//   0: enemy 0 acting (0x904B34 3) on a member (the target 0x904B44 0..2) whose
//      s16 +0x128 is above 0 - that added to the sum 0x939A08. Otherwise, the
//      target enemy 0 (3) or a side (bit 6 or 7): with enemy 0's +0x110 bit 1
//      the count 0x939A0C up by one, else its s16 +0x108, when above 0, added
//      to the sum 0x939A04.
//   1: 0x904AE2 at 1 and enemy 0's +0x8E at 1 - enemy 0's +0x110 |= 2, +1 =
//      2, +2 = 0, +0x105 = 2.
//   2: the turn counter 0x904B90 at 0x15 - 0x904AE8 |= 4.
//   3: 0x904AAD bit 5 clear and phase 1 - BattleTask_Create(0, 8) and bit 5 set
//      (0x904AAD read again after the call).
//   above 3: nothing.
extern "C" unsigned char __cdecl Boss26_Event(unsigned code) {
    switch (code & 0xFF) {
    case 0: {
        const unsigned char actor = B(at::kActor);
        const unsigned char target = B(at::kTarget);
        if (actor == 3 && target <= 2) {
            const auto w = static_cast<std::int16_t>(Word(At(at::kMember0Word128 + at::kPartyStride * target)));
            if (w > 0) {
                SetLong(At(at::kSumMember), S(static_cast<U>(Long(At(at::kSumMember))) + static_cast<U>(static_cast<std::int32_t>(w))));
                return 0;
            }
        }
        if (target != 3 && (target & 0xC0) == 0) return 0;
        if (B(at::kEnemy0Flags) & 2) {
            B(at::kCountFlagged) = static_cast<unsigned char>(B(at::kCountFlagged) + 1);
            return 0;
        }
        const auto v = static_cast<std::int16_t>(Word(At(at::kEnemy0Word108)));
        if (v > 0)
            SetLong(At(at::kSumEnemy), S(static_cast<U>(Long(At(at::kSumEnemy))) + static_cast<U>(static_cast<std::int32_t>(v))));
        return 0;
    }
    case 1:
        if (B(at::kRoundSlot) == 1 && B(at::kEnemy0Flag8E) == 1) {
            const U flags = static_cast<U>(Long(At(at::kEnemy0Flags))) | 2;
            B(at::kEnemy0State1) = 2;
            SetLong(At(at::kEnemy0Flags), S(flags));
            B(at::kEnemy0State2) = 0;
            B(at::kEnemy0Byte105) = 2;
        }
        return 0;
    case 2:
        if (static_cast<U>(Long(At(at::kTurn))) == 0x15) B(at::kBattleEnd) |= 4;
        return 0;
    case 3:
        if ((B(at::kScript) & 0x20) == 0 && B(at::kPhase) == 1) {
            BH_CALL(BattleTask_Create)(0, 8);
            B(at::kScript) |= 0x20;
        }
        return 0;
    default: return 0;
    }
}

// original 0x43C370: the end hook. For each party member 0..2 (ObjTrio) with
// +0 bit 0: Battle_RemoveFromTurnOrder(its +5); its +0x90 read after the call;
// Sprite_Current = the member; Sprite_PoseFromSet(its +8 + 0x1C with +0x90 bit
// 14, else + 4 - a byte, 0x8C5D80, 0x1800). Then variable 3 = 4 and a tail jump
// to 0x446E20 (step 3).
extern "C" void __cdecl Boss26_End(void) {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(at::kParty + m * at::kPartyStride);
        if ((p[0] & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(p[5]);
        const U flags = static_cast<U>(Long(p + 0x90));
        Sprite_Current = p;
        const unsigned pose = static_cast<unsigned char>(p[8] + ((flags & 0x4000) != 0 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(at::kPoseSet), 0x1800);
    }
    B(at::kScriptVar3) = 4;
    BH_AT(Handler, at::kEndThird)();
}

// original 0x43E790: the exit hook of set-ups 26 and 35 (35 is group BSH's):
// round-flag bit 1 set, Transition_Start(4).
extern "C" void __cdecl BossHook_ExitTransition4(void) {
    B(at::kFlags) |= 2;
    BH_CALL(Transition_Start)(4);
}

void BossSe_Inject() {
    if (bof3::WantsShadow("boss_se")) boss_se::SelfTest();
    BOF3_INJECT(Boss22_Setup);
    BOF3_INJECT(Boss22_End);
    BOF3_INJECT(Boss22_Exit);
    BOF3_INJECT(BossBully_Dispatch);
    BOF3_INJECT(BossBully_Enter);
    BOF3_INJECT(BossBully_Hook);
    BOF3_INJECT(Boss23_Setup);
    BOF3_INJECT(BossHook_ExitClearActors012);
    BOF3_INJECT(BossStallion_Dispatch);
    BOF3_INJECT(BossStallion_Enter);
    BOF3_INJECT(BossStallion_Hook);
    BOF3_INJECT(BossSample10_Dispatch);
    BOF3_INJECT(BossSample10_Enter);
    BOF3_INJECT(BossSample10_Hook);
    BOF3_INJECT(Boss24_Setup);
    BOF3_INJECT(Boss24_End);
    BOF3_INJECT(Boss24_Exit);
    BOF3_INJECT(Boss48_Setup);
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
    BOF3_INJECT(Boss25_Setup);
    BOF3_INJECT(Boss25_Event);
    BOF3_INJECT(Boss25_End);
    BOF3_INJECT(Boss25_Exit);
    BOF3_INJECT(Boss26_Setup);
    BOF3_INJECT(Boss26_Event);
    BOF3_INJECT(Boss26_End);
    BOF3_INJECT(BossHook_ExitTransition4);
    BOF3_INJECT(Boss30_Setup);
    BOF3_INJECT(Boss30_End);
}
