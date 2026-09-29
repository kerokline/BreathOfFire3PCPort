// Group BSF of the boss round: fights 27 and 28, kinds 33 (Gazer) and 62
// (Myria), fight 26's count task and the Gazer's effect task - 54 functions of
// 0x43C480..0x4406D7 (tools/boss_rows.py, 2026-09-28; analysis/boss_funcs.tsv's
// group column BSF), each read to its last instruction with capstone
// (2026-09-28) and taken through the boss harness (boss_harness.h). Round
// eleven, wave two; docs/boss_sf.md has them one row each. The ten functions
// of the group already ours (the Paralyzer and Head Cracker rows) are not
// here and nothing here calls them.
//
// The names are the disc's (tools/boss_rows.py --disc, the US disc's area
// records), the fights the tool's rows:
//
//   set-up 27   BOSS027, row 6 - one of the five set-ups the tool leaves open
//               (no kinds of its own; plan section 7): Boss27_*
//   FB8         BattleFx_Dispatch's slot 8: fight 26's count (created by
//               Boss26_Event, group BSE's): Boss26Fx_*
//   kind 33     Gazer (area 77)             set-up 28  BOSS028, area 77 row 7
//   F2          BattleBossFx_Dispatch's slot 2: the Gazer's effect task (its
//               state 4 creates it): BossGazerFx_*
//   kind 62     Myria (area 198; its set-up 55 is group BSJ's): BossMyria_*
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers and hook tables abort past their tables and the three task
// spawns abort on BattleTask_Create's 0xFF (all 48 slots taken), where the
// original jumps through or writes past whatever follows (the owner's rule for
// an unchecked index, round9 doc section 6; nothing reaches it). Every call
// goes through the harness (BH_CALL / BH_AT), so the start-up fuzz can stand
// recorders in for the callees; a hook or table a function stores is the same
// literal address.
#include "game/boss_sf.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sf_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sf::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(at::kCurrentEnemy)))); }
unsigned char* Owner() { return At(static_cast<U>(Long(At(at::kOwner)))); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void SetCell(unsigned char* p, U value) { SetLong(p, static_cast<std::int32_t>(value)); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll and
// BattleTask_RunAll push nothing; an entry that reads it - Port_DroppedCall's
// byte, a hook's code - reads that word), its eax passed back.
using Forward = U (__cdecl*)(unsigned);
Forward Entry(U table, unsigned index) { return reinterpret_cast<Forward>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index))))); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, as
// read (the fuzz swaps the cells for recorders); a Fatal past them (the
// original jumps through the dword after).
U Dispatch(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sf.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return Entry(table, state)(passed);
}

// call [table + 4 * Sprite_Current[at]]: the same, as a call (the caller goes
// on after it); the entries read no word.
void Step(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original calls through the dword after "
                    "(docs/boss_sf.md section 6)",
                    who, at, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
}

// An enemy's +0xF4 hook: jmp [table + 4 * (word & 0xFF)], three entries, a
// Fatal past them; the entry gets the hook's word.
U HookDispatch(const char* who, U table, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= 3)
        bof3::Fatal("%s: hook word %u, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sf.md section 6)",
                    who, i, (unsigned)table);
    return Entry(table, i)(word);
}

// BattleTask_Create's slot: a Fatal on 0xFF (all 48 taken), where the original
// writes at 0x93A000 + 0xFF * 0x84, past the pool (docs/boss_sf.md section 6).
unsigned char* TaskSlot(const char* who, unsigned n) {
    if (n >= at::kTaskCount)
        bof3::Fatal("%s: BattleTask_Create answered slot %u (all 48 taken) - the original writes at 0x93A000 + %u * 0x84, "
                    "past the pool (docs/boss_sf.md section 6)",
                    who, n, n);
    return At(at::kTaskSlots + n * at::kTaskStride);
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    SetCell(At(at::kHookEnd), end);
    SetCell(At(at::kHookExit), exit);
    SetCell(At(at::kHookEvent), event);
}

// The area's enemy data record the current enemy's +0xF0 names, byte `field`
// (+0x8A the state 4 count, +0x8B state 7's).
unsigned char EnemyDataByte(unsigned field) {
    const unsigned index = Enemy()[0xF0];
    return At(at::kEnemyData + index * at::kEnemyDataStride)[field];
}

// The byte of ability record `id` (24 bytes each) at `at`.
unsigned char AbilityByte(U base, unsigned id) { return At(base + id * at::kAbilityStride)[0]; }

// An enemy state's sound: 0x437450 with the first word of the current
// enemy's +0xF8 table.
void EnemySound() { BH_AT(void (__cdecl*)(unsigned), at::kEnemySound)(Word(At(static_cast<U>(Long(Enemy() + 0xF8))))); }

// The Gazer effect's move: +0x3E (a word) up by the word +0x14, then the
// dword +0x14 up by +0x20 - Sprite_Current read for each - then the ground
// at (+0x34, +0x38) (AreaMap_Elevation's low word, signed).
std::int32_t Move() {
    unsigned char* s = Sprite_Current;
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
    s = Sprite_Current;
    SetCell(s + 0x14, static_cast<U>(Long(s + 0x14)) + static_cast<U>(Long(s + 0x20)));
    s = Sprite_Current;
    return static_cast<std::int16_t>(BH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38)));
}
std::int32_t Height() { return static_cast<std::int16_t>(Word(Sprite_Current + 0x3E)); }

}  // namespace

// ===========================================================================
// Set-up 27 (BOSS027, row 6: the tool resolves no kinds for it)
// ===========================================================================

// original 0x43C480: Boss_SetupTable[27]. The hooks: end Boss27_End, exit
// Boss27_Exit, event Boss27_Event.
extern "C" void __cdecl Boss27_Setup(void) { StoreHooks(bof3::addr::Boss27_End, bof3::addr::Boss27_Exit, bof3::addr::Boss27_Event); }

// original 0x43C4A0: the event hook, by the code's low byte; al 0 on every
// path.
//   0: enemy 0's +0x92 bit 0x4000 - the script bits 0x904AAD |= 1 and
//      0x904AE8 |= 4; else enemy 1's - 0x904AAD |= 2 and 0x904AE8 |= 4; else,
//      with 0x904AE8 0 and 0x904AAD bit 3 clear, the actor before the turn
//      order's cursor (0x904ACB + 0x904AE2): a member (0..2) whose +0x130 has
//      neither bit 0 nor 1 - 0x904AE8 = 4 and 0x904AAD |= 4. Then (those two
//      paths too) 0x904AAD bit 5 clears bits 5 and 3.
//   2: without 0x904AE8 bit 2, with 0x904AAD bit 4 - bit 4 cleared,
//      Sprite_Current = enemy 1, Sprite_EnsureAnimation(0xC), then 0x904AAD
//      (read again) |= 8.
//   others: nothing.
// The original keeps the actor byte in its own argument slot ([esp + 4]); no
// caller reads it back.
extern "C" unsigned char __cdecl Boss27_Event(unsigned code) {
    switch (code & 0xFF) {
    case 0: {
        if (static_cast<U>(Long(At(at::kEnemy0Status))) & 0x4000) {
            B(at::kScript) |= 1;
            B(at::kBattleEnd) |= 4;
            return 0;
        }
        if (static_cast<U>(Long(At(at::kEnemy1Status))) & 0x4000) {
            B(at::kScript) |= 2;
            B(at::kBattleEnd) |= 4;
            return 0;
        }
        if (B(at::kBattleEnd) == 0 && (B(at::kScript) & 8) == 0) {
            const unsigned char who = B(at::kOrderBefore + B(at::kCursor));
            if (who <= 2 && (At(at::kParty + who * at::kPartyStride)[0x130] & 3) == 0) {
                const unsigned char script = B(at::kScript);
                B(at::kBattleEnd) = 4;
                B(at::kScript) = static_cast<unsigned char>(script | 4);
            }
        }
        const unsigned char script = B(at::kScript);
        if (script & 0x20) B(at::kScript) = static_cast<unsigned char>(script & 0xD7);
        return 0;
    }
    case 2: {
        if (B(at::kBattleEnd) & 4) return 0;
        const unsigned char script = B(at::kScript);
        if ((script & 0x10) == 0) return 0;
        B(at::kScript) = static_cast<unsigned char>(script & 0xEF);
        Sprite_Current = At(at::kEnemy1);
        BH_CALL(Sprite_EnsureAnimation)(0xC);
        B(at::kScript) |= 8;
        return 0;
    }
    default: return 0;
    }
}

// original 0x43C5C0: the end hook. For each party member 0..2 (ObjTrio) with
// +0 bit 0: Battle_RemoveFromTurnOrder(its +5); its +0x90 read after the call;
// Sprite_Current = the member; Sprite_PoseFromSet(its +8 + 0x1C with +0x90 bit
// 14, else + 4 - a byte, 0x8C5D80, 0x1800) - set-up 26's loop (Boss26_End).
// Then the script bits 0x904AAD, read once: bit 0 the chapter's step 0xF, bit
// 1 step 7, bit 2 step 0x14 (each stored in turn, the last set bit's
// standing); a tail jump to 0x446E20 (step 3).
extern "C" void __cdecl Boss27_End(void) {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(at::kParty + m * at::kPartyStride);
        if ((p[0] & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(p[5]);
        const U flags = static_cast<U>(Long(p + 0x90));
        Sprite_Current = p;
        const unsigned pose = static_cast<unsigned char>(p[8] + ((flags & 0x4000) != 0 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(at::kPoseSet), 0x1800);
    }
    const unsigned char script = B(at::kScript);
    if (script & 1) B(at::kChapterStep) = 0xF;
    if (script & 2) B(at::kChapterStep) = 7;
    if (script & 4) B(at::kChapterStep) = 0x14;
    BH_AT(Handler, at::kEndThird)();
}

// original 0x43C6F0: the exit hook. With 0x904AAD bit 0,
// BossActor_CopyFrom(1, enemy 0, 0) (its pose); BossActor_ClearBit40(1); with
// 0x904AAD (read again) bit 1, BossActor_CopyFrom(2, enemy 1, 0);
// BossActor_ClearBit40(2).
extern "C" void __cdecl Boss27_Exit(void) {
    if (B(at::kScript) & 1) BH_CALL(BossActor_CopyFrom)(1, At(at::kEnemies), 0);
    BH_CALL(BossActor_ClearBit40)(1);
    if (B(at::kScript) & 2) BH_CALL(BossActor_CopyFrom)(2, At(at::kEnemy1), 0);
    BH_CALL(BossActor_ClearBit40)(2);
}

// ===========================================================================
// FB8: BattleFx_Dispatch's slot 8, fight 26's count (Boss26_Event creates it)
// ===========================================================================

// original 0x43C740: jmp [Boss26Fx_States + 4 * Sprite_Current +1] (2).
extern "C" unsigned __cdecl Boss26Fx_Dispatch(unsigned passed) {
    return Dispatch("Boss26Fx_Dispatch", AddressOf(Boss26Fx_States), 2, 1, passed);
}

// original 0x43C760: state 0 - with the window byte 0x803433 clear and
// 0x904AE9 bit 1 clear, +1 up by one.
extern "C" void __cdecl Boss26Fx_Wait(void) {
    if (B(at::kWindowUp) == 0 && (B(at::kBanner) & 2) == 0) Sprite_Current[1] += 1;
}

// original 0x43C780: state 1 - BattleWin_DrawMediumBox(0x6B, 0x10); the text
// at [0x669D20] at (0x7A, 0x12); Crt_sprintf(Text_Records[1],
// Boss26Fx_CountFormat, 0x15 - the turn counter 0x904B90) and
// Text_DrawFont12(0x92, 0x12, 4, it); the text at [0x669D24] at (0xAA, 0x12).
// Then with 0x803433 set or 0x904AE9 bit 1, +1 down by one (back to state 0);
// at phase 5 (0x904AA0) a tail jump to BattleTask_FreeCurrent.
extern "C" void __cdecl Boss26Fx_DrawCount(void) {
    BH_CALL(BattleWin_DrawMediumBox)(0x6B, 0x10);
    BH_CALL(Text_DrawAt)(0x7A, 0x12, 0, 4, At(static_cast<U>(Long(At(at::kCountText0)))));
    const auto left = static_cast<std::int32_t>(0x15u - static_cast<U>(Long(At(at::kTurn))));   // x86's wrap
    BH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kTextRow1)), reinterpret_cast<const char*>(At(at::kCountFormat)), left);
    BH_CALL(Text_DrawFont12)(0x92, 0x12, 4, At(at::kTextRow1));
    BH_CALL(Text_DrawAt)(0xAA, 0x12, 0, 4, At(static_cast<U>(Long(At(at::kCountText1)))));
    if (B(at::kWindowUp) != 0 || (B(at::kBanner) & 2) != 0) Sprite_Current[1] -= 1;
    if (B(at::kPhase) == 5) BH_CALL(BattleTask_FreeCurrent)();
}

// ===========================================================================
// Kind 33 (Gazer, area 77)
// ===========================================================================

// original 0x43C810: BossKind_Table[33]. jmp [BossGazer_States + 4 *
// Sprite_Current +1] (12: its entry, its states 4 and 5, the generic enemy
// states).
extern "C" unsigned __cdecl BossGazer_Dispatch(unsigned passed) {
    return Dispatch("BossGazer_Dispatch", AddressOf(BossGazer_States), 12, 1, passed);
}

// original 0x43C830: state 0. 0x939AD8's +0xFC = BossGazer_Anims, +0xF4 =
// BossGazer_Hook, +0xF8 = BossGazer_Sounds (0x939AD8 read for each);
// Sprite_Current +1 = 2; a tail jump to Sprite_ScriptTick, whose al is the
// answer.
extern "C" unsigned char __cdecl BossGazer_Enter(void) {
    SetCell(Enemy() + 0xFC, AddressOf(BossGazer_Anims));
    SetCell(Enemy() + 0xF4, bof3::addr::BossGazer_Hook);
    SetCell(Enemy() + 0xF8, AddressOf(BossGazer_Sounds));
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43C870: state 4 (the generic table's turn start). jmp
// [BossGazer_State4Steps + 4 * Sprite_Current +2] (3).
extern "C" unsigned __cdecl BossGazer_State4Dispatch(unsigned passed) {
    return Dispatch("BossGazer_State4Dispatch", AddressOf(BossGazer_State4Steps), 3, 2, passed);
}

// original 0x43C890: state 4 step 0 - +9 = the enemy data record's +0x8A (the
// record the current enemy's +0xF0 names); BattleEnemy_SetAnimation(2);
// +2 up by one.
extern "C" void __cdecl BossGazer_State4Start(void) {
    Sprite_Current[9] = EnemyDataByte(0x8A);
    BH_CALL(BattleEnemy_SetAnimation)(2);
    Sprite_Current[2] += 1;
}

// original 0x43C8D0: state 4 step 1 - +9 down by one; at 0 the enemy's sound
// (0x437450 with its +0xF8 table's first word). Then BattleEnemy_ScriptTick;
// when it answers, BattleTask_Create(3, 2) - the Gazer's effect task (F2) -
// with its +1 = 0 and its owner +0x80 = Sprite_Current (read after the call),
// and +2 up by one.
extern "C" void __cdecl BossGazer_State4Wait(void) {
    Sprite_Current[9] -= 1;
    if (Sprite_Current[9] == 0) EnemySound();
    if (BH_CALL(BattleEnemy_ScriptTick)() == 0) return;
    unsigned char* const task = TaskSlot("BossGazer_State4Wait", BH_CALL(BattleTask_Create)(3, 2) & 0xFF);
    unsigned char* const s = Sprite_Current;
    task[1] = 0;
    SetCell(task + 0x80, Key(s));
    s[2] += 1;
}

// original 0x43C950: state 5. jmp [BossGazer_State5Steps + 4 * Sprite_Current
// +2] (2).
extern "C" unsigned __cdecl BossGazer_State5Dispatch(unsigned passed) {
    return Dispatch("BossGazer_State5Dispatch", AddressOf(BossGazer_State5Steps), 2, 2, passed);
}

// original 0x43C970: state 5 step 0 - Sprite_SetAnimation(2); the target
// 0x904B44 = 0x80 and Battle_SetTargetFlag40(0x80); +2 up by one.
extern "C" void __cdecl BossGazer_State5Start(void) {
    BH_CALL(Sprite_SetAnimation)(2);
    B(at::kTarget) = 0x80;
    BH_CALL(Battle_SetTargetFlag40)(0x80);
    Sprite_Current[2] += 1;
}

// original 0x43C9A0: state 5 step 1 (and Myria's, BossMyria_State5Steps 1) -
// when BattleEnemy_ScriptTick answers: 0x904AA8 |= 4, 0x4376F0, and a tail
// jump to 0x4376A0 (the action's end: +1 = 2, +2 = 0).
extern "C" void __cdecl BossGazer_State5Close(void) {
    if (BH_CALL(BattleEnemy_ScriptTick)() == 0) return;
    B(at::kRoundFlags) |= 4;
    BH_AT(Handler, at::kEnemyActChance)();
    BH_AT(Handler, at::kEnemyActEnd)();
}

// original 0x43C9C0: the +0xF4 hook: by the word's low byte through
// BossGazer_Hooks (3, all BareRet).
extern "C" unsigned __cdecl BossGazer_Hook(unsigned word) { return HookDispatch("BossGazer_Hook", AddressOf(BossGazer_Hooks), word); }

// ===========================================================================
// Set-up 28 (BOSS028, area 77 row 7: the Gazer)
// ===========================================================================

// original 0x43C9D0: Boss_SetupTable[28]. The hooks: end Boss28_End, exit
// BossHook_ExitClearActor0, event BareRetZero (BH's).
extern "C" void __cdecl Boss28_Setup(void) {
    StoreHooks(bof3::addr::Boss28_End, bof3::addr::BossHook_ExitClearActor0, bof3::addr::BareRetZero);
}

// original 0x43CA00: the end hook. The win (0x904AE8 bit 1): movement-script
// variable 3 = 0x1E and a tail jump to 0x446DE0 (step 1); else 0x446E00.
extern "C" void __cdecl Boss28_End(void) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kScriptVar3) = 0x1E;
        BH_AT(Handler, at::kEndWin)();
        return;
    }
    BH_AT(Handler, at::kEndOther)();
}

// ===========================================================================
// F2: BattleBossFx_Dispatch's slot 2, the Gazer's effect task
// ===========================================================================

// original 0x43CA20: jmp [BossGazerFx_States + 4 * Sprite_Current +1] (2).
extern "C" unsigned __cdecl BossGazerFx_Dispatch(unsigned passed) {
    return Dispatch("BossGazerFx_Dispatch", AddressOf(BossGazerFx_States), 2, 1, passed);
}

// original 0x43CA40: state 0 - call [BossGazerFx_BounceSteps + 4 * +2] (4);
// then with Sprite_Current (read again) bit 0, a tail jump to
// Sprite_UpdateScreen.
extern "C" void __cdecl BossGazerFx_BounceDispatch(void) {
    Step("BossGazerFx_BounceDispatch", AddressOf(BossGazerFx_BounceSteps), 4, 2);
    if (Sprite_Current[0] & 1) BH_CALL(Sprite_UpdateScreen)();
}

// original 0x43CA70: bounce step 0 - +0x24, +0x2A, +0x48 = 0, +0x29 = 2;
// Sprite_SetAnimationBank(0x15C), Sprite_SetAnimation(4); from the owner
// 0x93B940 (read for each): the word +0x36 + 7, the dword +0x38, the word +0x3E
// + 0x1000; the dword +0x14 = 0, +0x20 = -0x20, +9 = 6 (the bounces). Then
// BattleTask_Create(3, 2) - a second task of the same kind at state 1 (its
// +1 = 1) owned by this one (+0x80 = Sprite_Current, read after the call) -
// and +2 up by one.
extern "C" void __cdecl BossGazerFx_BounceStart(void) {
    Sprite_Current[0x24] = 0;
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x48] = 0;
    Sprite_Current[0x29] = 2;
    BH_CALL(Sprite_SetAnimationBank)(0x15C);
    BH_CALL(Sprite_SetAnimation)(4);
    SetWord(Sprite_Current + 0x36, Word(Owner() + 0x36) + 7);
    SetLong(Sprite_Current + 0x38, Long(Owner() + 0x38));
    SetWord(Sprite_Current + 0x3E, Word(Owner() + 0x3E) + 0x1000);
    SetCell(Sprite_Current + 0x14, 0);
    SetCell(Sprite_Current + 0x20, 0xFFFFFFE0u);
    Sprite_Current[9] = 6;
    unsigned char* const task = TaskSlot("BossGazerFx_BounceStart", BH_CALL(BattleTask_Create)(3, 2) & 0xFF);
    unsigned char* const s = Sprite_Current;
    task[1] = 1;
    SetCell(task + 0x80, Key(s));
    s[2] += 1;
}

// original 0x43CB50: bounce step 1 - the move (+0x3E by +0x14, +0x14 by +0x20);
// while the signed +0x3E is at or above the ground + 0x200, nothing more
// (control F79 refused > for >=). Else, once it is below, the
// ground asked again: +0x3E = it + 0x200 (a word), +0x14 = 0, +0x20 = 0x40,
// Sound_PlayById(0x600); then (Sprite_Current read again) +9, when not 0, down
// by one and +2 up by one (step 2); at 0, +2 up by two (step 3).
extern "C" void __cdecl BossGazerFx_Bounce(void) {
    const std::int32_t limit = Move() + 0x200;
    unsigned char* s = Sprite_Current;
    if (Height() >= limit) return;
    const U ground = static_cast<U>(BH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38)));
    SetWord(Sprite_Current + 0x3E, ground + 0x200);
    SetCell(Sprite_Current + 0x14, 0);
    SetCell(Sprite_Current + 0x20, 0x40);
    BH_CALL(Sound_PlayById)(0x600);
    s = Sprite_Current;
    if (s[9] != 0) {
        s[9] -= 1;
        Sprite_Current[2] += 1;
        return;
    }
    s[2] += 2;
}

// original 0x43CC00: bounce step 2 - the move; while the signed +0x3E is at or
// below the ground + 0x400, nothing more. Else the ground asked again: +0x3E =
// it + 0x400, +0x14 = 0, +0x20 = -0x40, +2 down by one (step 1 again).
extern "C" void __cdecl BossGazerFx_BounceBack(void) {
    const std::int32_t limit = Move() + 0x400;
    unsigned char* const s = Sprite_Current;
    if (Height() <= limit) return;
    const U ground = static_cast<U>(BH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38)));
    SetWord(Sprite_Current + 0x3E, ground + 0x400);
    SetCell(Sprite_Current + 0x14, 0);
    SetCell(Sprite_Current + 0x20, 0xFFFFFFC0u);
    Sprite_Current[2] -= 1;
}

// original 0x43CC90: bounce step 3 - the move; while the signed +0x3E is at or
// below the ground + 0x1000, nothing more. Else the owner (0x93B940, read for
// each) +1 = 5 and +2 = 0 - the Gazer to its state 5 - and a tail jump to
// BattleTask_FreeCurrent.
extern "C" void __cdecl BossGazerFx_Leave(void) {
    const std::int32_t limit = Move() + 0x1000;
    if (Height() <= limit) return;
    Owner()[1] = 5;
    Owner()[2] = 0;
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x43CD00: state 1 - call [BossGazerFx_MarkSteps + 4 * +2] (3); then
// with Sprite_Current (read again) bit 0, a tail jump to Sprite_UpdateScreen.
extern "C" void __cdecl BossGazerFx_MarkDispatch(void) {
    Step("BossGazerFx_MarkDispatch", AddressOf(BossGazerFx_MarkSteps), 3, 2);
    if (Sprite_Current[0] & 1) BH_CALL(Sprite_UpdateScreen)();
}

// original 0x43CD30: mark step 0 - +0x24, +0x2A, +0x48 = 0, +0x29 = 5;
// Sprite_SetAnimationBank(0x15C), Sprite_SetAnimation(6); the owner's dwords
// +0x34 and +0x38 (0x93B940 and Sprite_Current read for each); +0x3E = the
// ground there (a word); +2 up by one.
extern "C" void __cdecl BossGazerFx_MarkStart(void) {
    Sprite_Current[0x24] = 0;
    Sprite_Current[0x2A] = 0;
    Sprite_Current[0x48] = 0;
    Sprite_Current[0x29] = 5;
    BH_CALL(Sprite_SetAnimationBank)(0x15C);
    BH_CALL(Sprite_SetAnimation)(6);
    SetLong(Sprite_Current + 0x34, Long(Owner() + 0x34));
    SetLong(Sprite_Current + 0x38, Long(Owner() + 0x38));
    unsigned char* const s = Sprite_Current;
    const U ground = static_cast<U>(BH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38)));
    SetWord(Sprite_Current + 0x3E, ground);
    Sprite_Current[2] += 1;
}

// original 0x43CDC0: mark step 1 - once the owner's +0 bit 0 is clear (the
// bouncing task freed), +2 up by one (step 2: BattleFx_FreeTask); a tail jump
// to Sprite_ScriptTick, whose al is the answer.
extern "C" unsigned char __cdecl BossGazerFx_MarkWait(void) {
    if ((Owner()[0] & 1) == 0) Sprite_Current[2] += 1;
    return BH_CALL(Sprite_ScriptTick)();
}

// ===========================================================================
// Kind 62 (Myria, area 198; its set-up 55 is group BSJ's)
// ===========================================================================

// original 0x440080: BossKind_Table[62]. jmp [BossMyria_States + 4 *
// Sprite_Current +1] (12).
extern "C" unsigned __cdecl BossMyria_Dispatch(unsigned passed) {
    return Dispatch("BossMyria_Dispatch", AddressOf(BossMyria_States), 12, 1, passed);
}

// original 0x4400A0: state 0. The word 0x904B7E = 0; 0x939AD8's +0xFC =
// BossMyria_Anims, +0xF4 = BossMyria_Hook, +0xF8 = BossMyria_Sounds, +0x114 |=
// 8, +0x29 = 6 (0x939AD8 read for each); BattleEnemy_SetAnimation(0);
// BossMyria_SpawnFx(0, 6), (1, 6), (2, 6); 0x455290(Sprite_Current, 0x64DD04);
// Sprite_Current +1 = 2; a tail jump to Sprite_ScriptTick, whose al is the
// answer.
extern "C" unsigned char __cdecl BossMyria_Enter(void) {
    unsigned char* const e = Enemy();
    SetWord(At(at::kMyriaWait), 0);
    SetCell(e + 0xFC, AddressOf(BossMyria_Anims));
    SetCell(Enemy() + 0xF4, bof3::addr::BossMyria_Hook);
    SetCell(Enemy() + 0xF8, AddressOf(BossMyria_Sounds));
    unsigned char* const f = Enemy();
    SetCell(f + 0x114, static_cast<U>(Long(f + 0x114)) | 8);
    Enemy()[0x29] = 6;
    BH_CALL(BattleEnemy_SetAnimation)(0);
    BH_CALL(BossMyria_SpawnFx)(0, 6);
    BH_CALL(BossMyria_SpawnFx)(1, 6);
    BH_CALL(BossMyria_SpawnFx)(2, 6);
    BH_AT(void (__cdecl*)(unsigned char*, U), at::kSlotStart)(Sprite_Current, at::kMyriaSlotScript);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x440140: state 2 (the generic table's idle). jmp
// [BossMyria_IdleSteps + 4 * Sprite_Current +2] (2).
extern "C" unsigned __cdecl BossMyria_IdleDispatch(unsigned passed) {
    return Dispatch("BossMyria_IdleDispatch", AddressOf(BossMyria_IdleSteps), 2, 2, passed);
}

// original 0x440160: idle step 0 - +0xB = 1, the word 0x904B7E = 0,
// BattleEnemy_SetAnimation(0), BattleEnemy_ScriptTick (its al not read), +2
// up by one.
extern "C" void __cdecl BossMyria_IdleStart(void) {
    Sprite_Current[0xB] = 1;
    SetWord(At(at::kMyriaWait), 0);
    BH_CALL(BattleEnemy_SetAnimation)(0);
    BH_CALL(BattleEnemy_ScriptTick)();
    Sprite_Current[2] += 1;
}

// original 0x440190: idle step 1 - +0xB = 0, BattleEnemy_ScriptTick (its al
// not read), +1 up by one (state 3, the generic wait), +2 = 0.
extern "C" void __cdecl BossMyria_IdleEnd(void) {
    Sprite_Current[0xB] = 0;
    BH_CALL(BattleEnemy_ScriptTick)();
    Sprite_Current[1] += 1;
    Sprite_Current[2] = 0;
}

// original 0x4401C0: state 4. jmp [BossMyria_State4Steps + 4 * +2] (2).
extern "C" unsigned __cdecl BossMyria_State4Dispatch(unsigned passed) {
    return Dispatch("BossMyria_State4Dispatch", AddressOf(BossMyria_State4Steps), 2, 2, passed);
}

// original 0x4401E0: state 4 step 0 - BossMyria_SpawnFxAndWait(4, 2, 2); +9 =
// the enemy data record's +0x8A; +2 up by one.
extern "C" void __cdecl BossMyria_State4Start(void) {
    BH_CALL(BossMyria_SpawnFxAndWait)(4, 2, 2);
    Sprite_Current[9] = EnemyDataByte(0x8A);
    Sprite_Current[2] += 1;
}

// original 0x440220: state 4 step 1 - +9 down by one; at 0 the enemy's sound;
// +0xB = 0. The step stays (what moves Myria on from here is not in it).
extern "C" void __cdecl BossMyria_State4Wait(void) {
    Sprite_Current[9] -= 1;
    if (Sprite_Current[9] == 0) EnemySound();
    Sprite_Current[0xB] = 0;
}

// original 0x440260: state 5. jmp [BossMyria_State5Steps + 4 * +2] (2).
extern "C" unsigned __cdecl BossMyria_State5Dispatch(unsigned passed) {
    return Dispatch("BossMyria_State5Dispatch", AddressOf(BossMyria_State5Steps), 2, 2, passed);
}

// original 0x440280: state 5 step 0 - Battle_SetTargetFlag40(the target
// 0x904B44); +2 up by one. (Step 1 is BossGazer_State5Close.)
extern "C" void __cdecl BossMyria_State5Start(void) {
    BH_CALL(Battle_SetTargetFlag40)(B(at::kTarget));
    Sprite_Current[2] += 1;
}

// original 0x4402A0: state 6 (an action taken or received). jmp
// [BossMyria_ActSubs + 4 * +2] (6).
extern "C" unsigned __cdecl BossMyria_ActDispatch(unsigned passed) {
    return Dispatch("BossMyria_ActDispatch", AddressOf(BossMyria_ActSubs), 6, 2, passed);
}

// original 0x4402C0: action step 0 - unless the acting kind 0x904B35 is 4 (an
// ability) whose record byte (0x65C4D8 + 24 * 0x904B80) has bit 2 clear,
// BossMyria_SpawnFxAndWait(5, 4, 4); then +2 = 1.
extern "C" void __cdecl BossMyria_ActPick(void) {
    if (B(at::kActKind) != 4 || (AbilityByte(at::kAbilityFlags4, Word(At(at::kAbility))) & 4) != 0) {
        BH_CALL(BossMyria_SpawnFxAndWait)(5, 4, 4);
    }
    Sprite_Current[2] = 1;
}

// original 0x440310: action step 4 (the death) - Sprite_ScriptTickOnce,
// Battle_EnemyDefeated, 0x454A80(Sprite_Current); 0x939AD8's +0x110 |=
// 0x1000; Sprite_Current (read for each) +0 &= 0xBF, +1 = 2, +2 = 0, +3 = 0.
extern "C" void __cdecl BossMyria_Death(void) {
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    BH_AT(void (__cdecl*)(unsigned char*), at::kSlotsReleaseFor)(Sprite_Current);
    unsigned char* const e = Enemy();
    SetCell(e + 0x110, static_cast<U>(Long(e + 0x110)) | 0x1000);
    Sprite_Current[0] &= 0xBF;
    Sprite_Current[1] = 2;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
}

// original 0x440370: state 7. jmp [BossMyria_State7Steps + 4 * +2] (3).
extern "C" unsigned __cdecl BossMyria_State7Dispatch(unsigned passed) {
    return Dispatch("BossMyria_State7Dispatch", AddressOf(BossMyria_State7Steps), 3, 2, passed);
}

// original 0x440390: state 7 step 0 - nothing (eax 0) until File_LoadDone
// answers (its whole eax tested). Then by 0x939AD8's +0x105: 4 and an event
// battle (0x904AAA) - Battle_LoadSoundByKey(its kind +0x100 + 0x20, the set
// 0x904B8D): answered, the word 0x904B7E = 0, else +9 = 0x1E; 4 outside one,
// nothing; not 4, +9 = 0. Then by the ability 0x904B80: its record byte
// (0x65C4DD + 24 * id) bit 3 - +1 up by one; else +9 = the enemy data record's
// +0x8B and BossMyria_SpawnFxAndWait with BattleEnemy_SetAnimation by the id:
// 0x3A (5, 4, 7) and 0xA, 0x81 (6, 4, 6) and 0xA, 0x82 (3, 4, 5) and 6, any
// other (3, 4, 3) and 0xA; +2 up by one. Both end in a tail jump to
// BattleEnemy_ScriptTickOnce, whose al is the answer.
extern "C" unsigned char __cdecl BossMyria_State7Start(void) {
    if (BH_CALL(File_LoadDone)() == 0) return 0;
    unsigned char* const e = Enemy();
    if (e[0x105] == 4) {
        if (B(at::kFight) != 0) {
            const unsigned key = static_cast<unsigned char>(e[0x100] + 0x20);
            if (BH_CALL(Battle_LoadSoundByKey)(key, B(at::kSoundSet)) != 0)
                SetWord(At(at::kMyriaWait), 0);
            else
                Sprite_Current[9] = 0x1E;
        }
    } else {
        Sprite_Current[9] = 0;
    }
    const unsigned id = Word(At(at::kAbility));
    if (AbilityByte(at::kAbilityFlags8, id) & 8) {
        Sprite_Current[1] += 1;
        return BH_CALL(BattleEnemy_ScriptTickOnce)();
    }
    Sprite_Current[9] = EnemyDataByte(0x8B);
    switch (id) {
    case 0x3A:
        BH_CALL(BossMyria_SpawnFxAndWait)(5, 4, 7);
        BH_CALL(BattleEnemy_SetAnimation)(0xA);
        break;
    case 0x81:
        BH_CALL(BossMyria_SpawnFxAndWait)(6, 4, 6);
        BH_CALL(BattleEnemy_SetAnimation)(0xA);
        break;
    case 0x82:
        BH_CALL(BossMyria_SpawnFxAndWait)(3, 4, 5);
        BH_CALL(BattleEnemy_SetAnimation)(6);
        break;
    default:
        BH_CALL(BossMyria_SpawnFxAndWait)(3, 4, 3);
        BH_CALL(BattleEnemy_SetAnimation)(0xA);
        break;
    }
    Sprite_Current[2] += 1;
    return BH_CALL(BattleEnemy_ScriptTickOnce)();
}

// original 0x4404A0: state 7 step 1 - +0xB = 0; +9 (Sprite_Current read
// again), when not 0, down by one; at 0: with the word 0x904B7E at 3 or 5,
// Sound_PlayEffect(0x602) (Sprite_Current read after it), and +2 up by one.
// Both end in a tail jump to BattleEnemy_ScriptTickOnce, whose al is the
// answer.
extern "C" unsigned char __cdecl BossMyria_State7Count(void) {
    Sprite_Current[0xB] = 0;
    unsigned char* s = Sprite_Current;
    if (s[9] != 0) {
        s[9] -= 1;
        return BH_CALL(BattleEnemy_ScriptTickOnce)();
    }
    const unsigned wait = Word(At(at::kMyriaWait));
    if (wait == 3 || wait == 5) {
        BH_CALL(Sound_PlayEffect)(0x602);
        s = Sprite_Current;
    }
    s[2] += 1;
    return BH_CALL(BattleEnemy_ScriptTickOnce)();
}

// original 0x4404F0: state 7 step 2 - when BattleEnemy_ScriptTickOnce answers,
// +1 up by one (state 8) and +2 = 0.
extern "C" void __cdecl BossMyria_State7End(void) {
    if (BH_CALL(BattleEnemy_ScriptTickOnce)() == 0) return;
    Sprite_Current[1] += 1;
    Sprite_Current[2] = 0;
}

// original 0x440510: state 8 - call [BossMyria_State8Steps + 4 * +2] (5);
// then with Sprite_Current (read again) +1 still 8 and +2 at most 2, a tail
// jump to BossMyria_State8Check.
extern "C" void __cdecl BossMyria_State8Dispatch(void) {
    Step("BossMyria_State8Dispatch", AddressOf(BossMyria_State8Steps), 5, 2);
    unsigned char* const s = Sprite_Current;
    if (s[1] == 8 && s[2] <= 2) BH_CALL(BossMyria_State8Check)();
}

// original 0x440540: state 8 step 0 - BattleEnemy_ScriptTickOnce (its al not
// read), +2 up by one.
extern "C" void __cdecl BossMyria_State8Tick(void) {
    BH_CALL(BattleEnemy_ScriptTickOnce)();
    Sprite_Current[2] += 1;
}

// original 0x440550: state 8 step 1 - BattleEnemy_ScriptTickOnce; with
// 0x939AD8's +0x105 at 4, its word +0xA6 down by the byte 0x904B88 (the cost
// shown) and 0x939AD8 read again; 0x904AA9 |= 8; that object's +0x105 = 0; +2
// up by one.
extern "C" void __cdecl BossMyria_State8Cost(void) {
    BH_CALL(BattleEnemy_ScriptTickOnce)();
    unsigned char* e = Enemy();
    if (e[0x105] == 4) {
        SetWord(e + 0xA6, Word(e + 0xA6) - B(at::kCost));
        e = Enemy();
    }
    B(at::kRoundFlagsHi) |= 8;
    e[0x105] = 0;
    Sprite_Current[2] += 1;
}

// original 0x440590: state 8 step 2 - unless the ability's record byte
// (0x65C4DD + 24 * 0x904B80) has bit 3, a tail jump to
// BattleEnemy_ScriptTickOnce. The original's eax is that answer, or 3 * the id
// (its index arithmetic); al is both.
extern "C" unsigned char __cdecl BossMyria_State8TickUnless(void) {
    const unsigned id = Word(At(at::kAbility));
    if ((AbilityByte(at::kAbilityFlags8, id) & 8) == 0) return BH_CALL(BattleEnemy_ScriptTickOnce)();
    return static_cast<unsigned char>(3 * id);
}

// original 0x4405B0: state 8 step 3 - +0xB = 0.
extern "C" void __cdecl BossMyria_State8Clear(void) { Sprite_Current[0xB] = 0; }

// original 0x4405C0: state 8 step 4 - 0x4376A0 (the action's end), then +1 =
// 2 and +2 = 0 (Sprite_Current read for each).
extern "C" void __cdecl BossMyria_State8Close(void) {
    BH_AT(Handler, at::kEnemyActEnd)();
    Sprite_Current[1] = 2;
    Sprite_Current[2] = 0;
}

// original 0x4405E0: state 8's check - nothing without 0x904AA8 bit 2; with
// it, the ability 0x904B80 at 0x81: +0xB = 1, the word 0x904B7E = 8, +2 = 3;
// any other: a tail jump to 0x4376A0 (the action's end).
extern "C" void __cdecl BossMyria_State8Check(void) {
    if ((B(at::kRoundFlags) & 4) == 0) return;
    if (Word(At(at::kAbility)) != 0x81) {
        BH_AT(Handler, at::kEnemyActEnd)();
        return;
    }
    Sprite_Current[0xB] = 1;
    unsigned char* const s = Sprite_Current;
    SetWord(At(at::kMyriaWait), 8);
    s[2] = 3;
}

// original 0x440620: the +0xF4 hook: by the word's low byte through
// BossMyria_Hooks (3, all BareRet).
extern "C" unsigned __cdecl BossMyria_Hook(unsigned word) { return HookDispatch("BossMyria_Hook", AddressOf(BossMyria_Hooks), word); }

// original 0x440630 (state, part, wait): BossMyria_SpawnFx(state, part); then
// Sprite_Current +0xB = 1 and the word 0x904B7E = the wait's low byte.
extern "C" void __cdecl BossMyria_SpawnFxAndWait(unsigned state, unsigned part, unsigned wait) {
    BH_CALL(BossMyria_SpawnFx)(state, part);
    Sprite_Current[0xB] = 1;
    SetWord(At(at::kMyriaWait), wait & 0xFF);
}

// original 0x440660 (state, part): BattleTask_Create(3, 5) - an effect task of
// BattleBossFx_Dispatch's slot 5 (F5, group BSJ's) - its first 0x80 bytes
// copied from enemy 0's object, then its +1 = the state's low byte, +2 = 0,
// +6 = 3, +5 = 5, +9 = 0, the owner +0x80 = enemy 0, +0x29 = the part's low
// byte.
extern "C" void __cdecl BossMyria_SpawnFx(unsigned state, unsigned part) {
    unsigned char* const task = TaskSlot("BossMyria_SpawnFx", BH_CALL(BattleTask_Create)(3, 5) & 0xFF);
    for (U i = 0; i < 0x80; i += 4) SetLong(task + i, Long(At(at::kEnemies + i)));
    task[1] = static_cast<unsigned char>(state);
    task[2] = 0;
    task[6] = 3;
    task[5] = 5;
    task[9] = 0;
    SetCell(task + 0x80, at::kEnemies);
    task[0x29] = static_cast<unsigned char>(part);
}

void BossSf_Inject() {
    if (bof3::WantsShadow("boss_sf")) boss_sf::SelfTest();
    BOF3_INJECT(Boss27_Setup);
    BOF3_INJECT(Boss27_Event);
    BOF3_INJECT(Boss27_End);
    BOF3_INJECT(Boss27_Exit);
    BOF3_INJECT(Boss26Fx_Dispatch);
    BOF3_INJECT(Boss26Fx_Wait);
    BOF3_INJECT(Boss26Fx_DrawCount);
    BOF3_INJECT(BossGazer_Dispatch);
    BOF3_INJECT(BossGazer_Enter);
    BOF3_INJECT(BossGazer_State4Dispatch);
    BOF3_INJECT(BossGazer_State4Start);
    BOF3_INJECT(BossGazer_State4Wait);
    BOF3_INJECT(BossGazer_State5Dispatch);
    BOF3_INJECT(BossGazer_State5Start);
    BOF3_INJECT(BossGazer_State5Close);
    BOF3_INJECT(BossGazer_Hook);
    BOF3_INJECT(Boss28_Setup);
    BOF3_INJECT(Boss28_End);
    BOF3_INJECT(BossGazerFx_Dispatch);
    BOF3_INJECT(BossGazerFx_BounceDispatch);
    BOF3_INJECT(BossGazerFx_BounceStart);
    BOF3_INJECT(BossGazerFx_Bounce);
    BOF3_INJECT(BossGazerFx_BounceBack);
    BOF3_INJECT(BossGazerFx_Leave);
    BOF3_INJECT(BossGazerFx_MarkDispatch);
    BOF3_INJECT(BossGazerFx_MarkStart);
    BOF3_INJECT(BossGazerFx_MarkWait);
    BOF3_INJECT(BossMyria_Dispatch);
    BOF3_INJECT(BossMyria_Enter);
    BOF3_INJECT(BossMyria_IdleDispatch);
    BOF3_INJECT(BossMyria_IdleStart);
    BOF3_INJECT(BossMyria_IdleEnd);
    BOF3_INJECT(BossMyria_State4Dispatch);
    BOF3_INJECT(BossMyria_State4Start);
    BOF3_INJECT(BossMyria_State4Wait);
    BOF3_INJECT(BossMyria_State5Dispatch);
    BOF3_INJECT(BossMyria_State5Start);
    BOF3_INJECT(BossMyria_ActDispatch);
    BOF3_INJECT(BossMyria_ActPick);
    BOF3_INJECT(BossMyria_Death);
    BOF3_INJECT(BossMyria_State7Dispatch);
    BOF3_INJECT(BossMyria_State7Start);
    BOF3_INJECT(BossMyria_State7Count);
    BOF3_INJECT(BossMyria_State7End);
    BOF3_INJECT(BossMyria_State8Dispatch);
    BOF3_INJECT(BossMyria_State8Tick);
    BOF3_INJECT(BossMyria_State8Cost);
    BOF3_INJECT(BossMyria_State8TickUnless);
    BOF3_INJECT(BossMyria_State8Clear);
    BOF3_INJECT(BossMyria_State8Close);
    BOF3_INJECT(BossMyria_State8Check);
    BOF3_INJECT(BossMyria_Hook);
    BOF3_INJECT(BossMyria_SpawnFxAndWait);
    BOF3_INJECT(BossMyria_SpawnFx);
}
