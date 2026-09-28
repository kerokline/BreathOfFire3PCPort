// Round eleven group BSD: the boss band's fights 17..21 and kinds 18, 21..27 -
// 52 functions of 0x43A590..0x43B74A (tools/boss_rows.py --groups,
// analysis/boss_funcs.tsv's group column BSD), each read to its last
// instruction with capstone (2026-09-28) and taken through the boss harness
// (boss_harness.h). docs/boss_sd.md has them one row each. The enemy names are
// tools/boss_rows.py --disc's (the US disc's area records):
//
//   - kind 18 (Mutant, area 52) and set-up 17 (its fight, row 7);
//   - kinds 21, 22, 23, 26 (Claw, Cawer, Patrio, Dodai 1 / 2, area 79) and
//     set-ups 18, 19, 20 (one image on the PSX - the sibling's dedup
//     BOSS018 = BOSS019 = BOSS020 - rows 7, 6, 5; three copies of one set of
//     hooks differing in the tag of the actor they place and the chapter
//     steps they hand back);
//   - kinds 24, 25 (Emitai, Golem, area 81) and set-up 21 (row 7), whose end
//     hook set-up 23 (BSE's) installs too;
//   - kind 27 (Garr, area 80; set-up 22 is BSE's).
//
// A kind is a dispatcher (BossKind_Table[kind]) by the sprite's +1 through the
// kind's twelve-entry step table - EnemyOp_Steps with the kind's own entrance
// at 0 and, for some kinds, their own action dispatcher at 6 - its entrance
// (the enemy's +0xFC / +0xF4 / +0xF8 pointed at the kind's tables), an action
// dispatcher by +2 with the kind's death at 4, and the enemy's +0xF4 hook: a
// three-entry table by the hook's word. A set-up stores the fight's three
// hooks (BattleHook_End / _Exit / _Event) and returns.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers and hooks abort past their tables where the original would jump
// through whatever follows, and set-up 21's exit hook aborts where the
// original writes through BossActor_Find's null (the owner's rule for an
// unchecked index or pointer, round9 doc section 6; nothing reaches either).
// Every call goes through the harness (BH_CALL / BH_AT / Phase), so the
// start-up fuzz can stand recorders in for the callees.
#include "game/boss_sd.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sd_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sd::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(at::kCurrentEnemy)))); }
void SetCell(unsigned char* p, U value) { SetLong(p, static_cast<std::int32_t>(value)); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll pushes
// nothing; an entry that reads it - Port_DroppedCall's byte, a hook's code -
// reads that word).
using Forward = void (__cdecl*)(unsigned);
Forward Entry(U table, unsigned index) { return reinterpret_cast<Forward>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index))))); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
void Dispatch(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sd.md section 6)",
                    who, at, state, entries, (unsigned)table);
    Entry(table, state)(passed);   // as read: the fuzz swaps the table's cells for its recorders
}

// An enemy's +0xF4 hook: jmp [table + 4 * (code & 0xFF)], three entries, a
// Fatal past them; the entry gets the hook's word.
void HookTable(const char* who, U table, unsigned code) {
    const unsigned index = code & 0xFF;
    if (index >= 3)
        bof3::Fatal("%s: hook code %u, past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sd.md section 6)",
                    who, index, (unsigned)table);
    Entry(table, index)(code);
}

// The entrance's three stores into 0x939AD8's object (read again for each):
// +0xFC, +0xF4 (the hook), +0xF8.
void PointEnemy(U fc, U hook, U f8) {
    SetCell(Enemy() + 0xFC, fc);
    SetCell(Enemy() + 0xF4, hook);
    SetCell(Enemy() + 0xF8, f8);
}

// The three set-ups' stores.
void StoreHooks(U end, U exit, U event) {
    SetCell(At(at::kHookEnd), end);
    SetCell(At(at::kHookExit), exit);
    SetCell(At(at::kHookEvent), event);
}

// Kinds 21..23's entrance: with chapter flag `flag` set, 0x939AD8's +0xA4 and
// +0xA6 (the object read again for each) from the words set-ups 18..20 kept;
// without, the flag set. Then the three stores.
void RestoreOrMark(unsigned flag, U fc, U hook, U f8) {
    if (BH_CALL(Flags_Test)(At(static_cast<U>(Long(At(at::kFlagBits)))), flag) & 0xFF) {
        SetWord(Enemy() + 0xA4, Word(At(at::kKeptA4)));
        SetWord(Enemy() + 0xA6, Word(At(at::kKeptA6)));
    } else {
        BH_CALL(Flags_Set)(At(static_cast<U>(Long(At(at::kFlagBits)))), flag);
    }
    PointEnemy(fc, hook, f8);
}

// The end phase's steps (nobody's; the harness's standard set).
void EndStep(U step) { BH_AT(Handler, step)(); }

// Set-ups 18..20's end hooks: the win hands the chapter the step `win_step`
// (by 0x904AAD bit 1: `step_if_1` or `step_if_not`) and takes step 1; enemy 2
// fallen (0x904AAD bit 0) takes step 2; otherwise flag 0x23 and 0x24 of the
// chapter's bits: 0x23 and 0x24 step 2, 0x23 alone the chapter step 0x3A, not
// 0x23 the step 0x1C, each with Boss_SetByLeaderId (before the step's store,
// or after it for set-up 20: `pick_first`) and step 3. Then the four words
// kept: enemy 1's HP, enemy 2's, enemy 0's +0xA4 and +0xA6.
void EndAreaFight(unsigned char step_if_1, unsigned char step_if_not, bool pick_first) {
    const unsigned char fell = B(at::kWhoFell);   // read once, before any call
    if (B(at::kBattleEnd) & 2) {
        B(at::kChapterStep) = fell & 2 ? step_if_1 : step_if_not;
        EndStep(at::kEndWin);
    } else if (fell & 1) {
        EndStep(at::kEndOther);
    } else {
        const bool f23 = BH_CALL(Flags_Test)(At(static_cast<U>(Long(At(at::kFlagBits)))), 0x23) & 0xFF;
        if (f23 && (BH_CALL(Flags_Test)(At(static_cast<U>(Long(At(at::kFlagBits)))), 0x24) & 0xFF)) {
            EndStep(at::kEndOther);
        } else {
            const unsigned char step = f23 ? 0x3A : 0x1C;
            if (pick_first) {
                BH_CALL(Boss_SetByLeaderId)();
                B(at::kChapterStep) = step;
            } else {
                B(at::kChapterStep) = step;
                BH_CALL(Boss_SetByLeaderId)();
            }
            EndStep(at::kEndThird);
        }
    }
    const unsigned hp1 = Word(At(at::kEnemy1Hp));
    const unsigned hp2 = Word(At(at::kEnemy2Hp));
    const unsigned a4 = Word(At(at::kEnemy0Hp));
    SetWord(At(at::kKeptHp1), hp1);
    const unsigned a6 = Word(At(at::kEnemy0A6));
    SetWord(At(at::kKeptHp2), hp2);
    SetWord(At(at::kKeptA4), a4);
    SetWord(At(at::kKeptA6), a6);
}

// Set-ups 18..20's event hooks (the three are one body): phase 0 - enemy 0's
// +0x93 bit 0x40 sets the win bit 1 of 0x904AE8; enemy 1's sets 0x904AAD and
// 0x904AE8 bit 1; enemy 2's sets both bit 0; then with 0x904AE8 not 0,
// BossMap_UpdateFromEnemies. Phase 5: BossMap_UpdateFromEnemies. al 0.
unsigned char EventAreaFight(unsigned code) {
    const unsigned char phase = static_cast<unsigned char>(code);
    if (phase == 5) {
        BH_CALL(BossMap_UpdateFromEnemies)();
        return 0;
    }
    if (phase != 0) return 0;
    if (static_cast<U>(Long(At(at::kEnemy0Status))) & 0x4000) B(at::kBattleEnd) |= 2;
    if (static_cast<U>(Long(At(at::kEnemy1Status))) & 0x4000) {
        B(at::kWhoFell) |= 2;
        B(at::kBattleEnd) |= 2;
    }
    if (static_cast<U>(Long(At(at::kEnemy2Status))) & 0x4000) {
        B(at::kWhoFell) |= 1;
        B(at::kBattleEnd) |= 1;
    }
    if (B(at::kBattleEnd) != 0) BH_CALL(BossMap_UpdateFromEnemies)();
    return 0;
}

// Set-ups 18..20's exit hooks: unless enemy 0's +0x93 has 0x40, the actor
// tagged `tag` takes enemy 0's place (BossActor_CopyFrom(tag, enemy 0, 1)) and
// loses bit 0x40; then actors 3 and 4 take enemies 1 and 2's poses (what 0)
// and lose bit 0x40.
void ExitAreaFight(unsigned tag) {
    if (!(static_cast<U>(Long(At(at::kEnemy0Status))) & 0x4000)) {
        BH_CALL(BossActor_CopyFrom)(tag, At(at::kEnemy0), 1);
        BH_CALL(BossActor_ClearBit40)(tag);
    }
    BH_CALL(BossActor_CopyFrom)(3, At(at::kEnemy1), 0);
    BH_CALL(BossActor_ClearBit40)(3);
    BH_CALL(BossActor_CopyFrom)(4, At(at::kEnemy2), 0);
    BH_CALL(BossActor_ClearBit40)(4);
}

// Set-ups 17 and 21's end hooks: the win sets the field's move-script counter
// 0 (`counter`) and takes the end phase's step 1, else step 2.
void EndCounter(unsigned char counter) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kCounter0) = counter;
        EndStep(at::kEndWin);
        return;
    }
    EndStep(at::kEndOther);
}

// The death of kinds 24 and 26's +2 tables: Sprite_Current's +0 &= 0xBF, +1
// = 3, +2 = 0, +3 = 0 (Sprite_Current read again for each).
void DeathStates() {
    Sprite_Current[0] &= 0xBF;
    Sprite_Current[1] = 3;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
}

// The two ground words: enemy 0's +0x3E and the leader's, AreaMap_Elevation
// of their (+0x34, +0x38).
void GroundWords() {
    const long e = BH_CALL(AreaMap_Elevation)(Long(At(at::kEnemy0X)), Long(At(at::kEnemy0Z)));
    SetWord(At(at::kEnemy0Ground), static_cast<unsigned>(e) & 0xFFFF);
    const long l = BH_CALL(AreaMap_Elevation)(Long(At(at::kLeaderX)), Long(At(at::kLeaderZ)));
    SetWord(At(at::kLeaderGround), static_cast<unsigned>(l) & 0xFFFF);
}

}  // namespace

// === Kind 18 (Mutant, area 52) =====================================================

// original 0x43A590: BossKind_Table[18]: jmp [BossMutant_Steps + 4 * +1].
extern "C" void __cdecl BossMutant_Dispatch(unsigned passed) {
    Dispatch("BossMutant_Dispatch", AddressOf(BossMutant_Steps), 12, 1, passed);
}

// original 0x43A5B0: BossMutant_Steps 0, the entrance: 0x939AD8's +0xFC =
// 0x64CE74, +0xF4 = BossMutant_Hook, +0xF8 = 0x64CE98; Sprite_Current +1 = 2;
// jmp Sprite_ScriptTick (its al the answer).
extern "C" unsigned char __cdecl BossMutant_Enter(void) {
    PointEnemy(0x64CE74, 0x43A610, 0x64CE98);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A5F0: BossMutant_Steps 6: jmp [BossMutant_ActSubs + 4 * +2].
extern "C" void __cdecl BossMutant_ActDispatch(unsigned passed) {
    Dispatch("BossMutant_ActDispatch", AddressOf(BossMutant_ActSubs), 6, 2, passed);
}

// original 0x43A610: the +0xF4 hook: jmp [BossMutant_Hooks + 4 * (code & 0xFF)].
extern "C" void __cdecl BossMutant_Hook(unsigned code) { HookTable("BossMutant_Hook", AddressOf(BossMutant_Hooks), code); }

// === Set-up 17 (Mutant, area 52 row 7) ===================================================

// original 0x43A620: Boss_SetupTable[17]: BattleHook_End = Boss17_End,
// BattleHook_Exit = BossHook_ExitActor0Bit40, BattleHook_Event = BareRetZero.
extern "C" void __cdecl Boss17_Setup(void) { StoreHooks(0x43A640, at::kExitActor0Bit40, at::kBareRetZero); }

// original 0x43A640: the end hook: the win (0x904AE8 bit 1) sets the field's
// move-script counter 0 to 0x14 and jumps to the end phase's step 1; else step 2.
extern "C" void __cdecl Boss17_End(void) { EndCounter(0x14); }

// === Kinds 21, 22, 23 (Claw, Cawer, Patrio, area 79) =========================================

// original 0x43A660: BossKind_Table[21]: jmp [BossClaw_Steps + 4 * +1].
extern "C" void __cdecl BossClaw_Dispatch(unsigned passed) {
    Dispatch("BossClaw_Dispatch", AddressOf(BossClaw_Steps), 12, 1, passed);
}

// original 0x43A680: BossClaw_Steps 0, the entrance: with chapter flag 0x35,
// 0x939AD8's +0xA4 / +0xA6 = the words 0x939A1A / 0x939A14; without, flag
// 0x35 set. Then +0xFC = 0x64CF04, +0xF4 = BossClaw_Hook, +0xF8 = 0x64CF58,
// Sprite_Current +1 = 1, Sprite_EnsureAnimation(2), jmp Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossClaw_Enter(void) {
    RestoreOrMark(0x35, 0x64CF04, 0x43A740, 0x64CF58);
    Sprite_Current[1] = 1;
    BH_CALL(Sprite_EnsureAnimation)(2);
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A740: the +0xF4 hook through BossClaw_Hooks.
extern "C" void __cdecl BossClaw_Hook(unsigned code) { HookTable("BossClaw_Hook", AddressOf(BossClaw_Hooks), code); }

// original 0x43A770: BossKind_Table[22]: jmp [BossCawer_Steps + 4 * +1].
extern "C" void __cdecl BossCawer_Dispatch(unsigned passed) {
    Dispatch("BossCawer_Dispatch", AddressOf(BossCawer_Steps), 12, 1, passed);
}

// original 0x43A790: BossCawer_Steps 0: BossClaw_Enter's body with flag 0x36,
// +0xFC = 0x64CF10, +0xF4 = BossCawer_Hook, +0xF8 = 0x64CF60.
extern "C" unsigned char __cdecl BossCawer_Enter(void) {
    RestoreOrMark(0x36, 0x64CF10, 0x43A830, 0x64CF60);
    Sprite_Current[1] = 1;
    BH_CALL(Sprite_EnsureAnimation)(2);
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A830: the +0xF4 hook through BossCawer_Hooks.
extern "C" void __cdecl BossCawer_Hook(unsigned code) { HookTable("BossCawer_Hook", AddressOf(BossCawer_Hooks), code); }

// original 0x43A840: BossKind_Table[23]: jmp [BossPatrio_Steps + 4 * +1].
extern "C" void __cdecl BossPatrio_Dispatch(unsigned passed) {
    Dispatch("BossPatrio_Dispatch", AddressOf(BossPatrio_Steps), 12, 1, passed);
}

// original 0x43A860: BossPatrio_Steps 0: flag 0x37, +0xFC = 0x64CF1C, +0xF4 =
// BossPatrio_Hook, +0xF8 = 0x64CF68; Sprite_Current +1 = 2 (no animation),
// jmp Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossPatrio_Enter(void) {
    RestoreOrMark(0x37, 0x64CF1C, 0x43A8F0, 0x64CF68);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43A8F0: the +0xF4 hook through BossPatrio_Hooks.
extern "C" void __cdecl BossPatrio_Hook(unsigned code) { HookTable("BossPatrio_Hook", AddressOf(BossPatrio_Hooks), code); }

// === Kind 26 (Dodai 1 / 2, area 79) =============================================================

// original 0x43A900: BossKind_Table[26]: first 0x939AD8's +0xFC by the
// sprite's +5 and a chapter flag - +5 == 4: flag 0x34 ? 0x64CF34 : 0x64CF28;
// else flag 0x33 ? 0x64CF4C : 0x64CF40 - then (Sprite_Current read again)
// jmp [BossDodai_Steps + 4 * +1].
extern "C" void __cdecl BossDodai_Dispatch(unsigned passed) {
    if (Sprite_Current[5] == 4) {
        const bool f = BH_CALL(Flags_Test)(At(static_cast<U>(Long(At(at::kFlagBits)))), 0x34) & 0xFF;
        SetCell(Enemy() + 0xFC, f ? 0x64CF34 : 0x64CF28);
    } else {
        const bool f = BH_CALL(Flags_Test)(At(static_cast<U>(Long(At(at::kFlagBits)))), 0x33) & 0xFF;
        SetCell(Enemy() + 0xFC, f ? 0x64CF4C : 0x64CF40);
    }
    Dispatch("BossDodai_Dispatch", AddressOf(BossDodai_Steps), 12, 1, passed);
}

// original 0x43A990: BossDodai_Steps 0, the entrance: with chapter flag 0x1E
// or (asked only without it) 0x23, 0x939AD8's +0xA4 = the word 0x939A18 when
// Sprite_Current's +5 is 4, else 0x939A16. Then by Sprite_Current's +5 == 5:
// its +8 |= 2 and +0xFC = 0x64CF40, else +0xFC = 0x64CF28; +0xF4 =
// BossDodai_Hook, +0xF8 = 0x64CF70; Sprite_Current +1 = 2; jmp Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossDodai_Enter(void) {
    bool back = BH_CALL(Flags_Test)(At(static_cast<U>(Long(At(at::kFlagBits)))), 0x1E) & 0xFF;
    if (!back) back = BH_CALL(Flags_Test)(At(static_cast<U>(Long(At(at::kFlagBits)))), 0x23) & 0xFF;
    if (back) {
        const unsigned hp = Sprite_Current[5] == 4 ? Word(At(at::kKeptHp1)) : Word(At(at::kKeptHp2));
        SetWord(Enemy() + 0xA4, hp);
    }
    unsigned char* const s = Sprite_Current;
    if (s[5] == 5) {
        s[8] |= 2;
        SetCell(Enemy() + 0xFC, 0x64CF40);
    } else {
        SetCell(Enemy() + 0xFC, 0x64CF28);
    }
    SetCell(Enemy() + 0xF4, 0x43AB40);
    SetCell(Enemy() + 0xF8, 0x64CF70);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43AA50: BossDodai_Steps 6: jmp [BossDodai_ActSubs + 4 * +2].
extern "C" void __cdecl BossDodai_ActDispatch(unsigned passed) {
    Dispatch("BossDodai_ActDispatch", AddressOf(BossDodai_ActSubs), 6, 2, passed);
}

// original 0x43AA70: BossDodai_ActSubs 4, the death: Sprite_SetAnimation(4),
// Battle_EnemyDefeated, BossMap_SetCorners(Sprite_Current's +5 == 4 ? 1 : 0,
// 2), the ground words of enemy 0 and the leader, then the states 3, 0, 0 with
// +0 &= 0xBF.
extern "C" void __cdecl BossDodai_Death(void) {
    BH_CALL(Sprite_SetAnimation)(4);
    BH_CALL(Battle_EnemyDefeated)();
    BH_CALL(BossMap_SetCorners)(Sprite_Current[5] == 4 ? 1 : 0, 2);
    GroundWords();
    DeathStates();
}

// original 0x43AB00: BossDodai_Steps 11 (where EnemyOp_Steps has
// EnemyOp_HitPose): jmp [BossDodai_HitPoseSubs + 4 * +2].
extern "C" void __cdecl BossDodai_HitPoseDispatch(unsigned passed) {
    Dispatch("BossDodai_HitPoseDispatch", AddressOf(BossDodai_HitPoseSubs), 3, 2, passed);
}

// original 0x43AB20: BossDodai_HitPoseSubs 0: Sprite_Current's dword +0x38 +=
// (Frame_Counter & 1) << 9 - a shake of 0x200 every other frame.
extern "C" void __cdecl BossDodai_HitShake(void) {
    const U add = (Frame_Counter & 1) << 9;
    unsigned char* const s = Sprite_Current;
    SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(s + 0x38)) + add));
}

// original 0x43AB40: the +0xF4 hook through BossDodai_Hooks.
extern "C" void __cdecl BossDodai_Hook(unsigned code) { HookTable("BossDodai_Hook", AddressOf(BossDodai_Hooks), code); }

// original 0x43AB50: entry 0 of kinds 24 and 26's hook tables (the hook called
// with 0, the action pick): the action kind 0x904B35 = 0. The word is not read.
extern "C" void __cdecl BossHook_ActKindNone(unsigned) { B(at::kActKind) = 0; }

// original 0x43AB60: BossDodai_Hooks 1 (the hook called with 1, the hit):
// when 0x939AD8's +0x108 is above 0 (signed word), Sound_PlayById(0x600) or,
// by Rand's bit 0 clear, 0x601.
extern "C" void __cdecl BossDodai_HitSound(unsigned) {
    if (static_cast<std::int16_t>(Word(Enemy() + 0x108)) <= 0) return;
    BH_CALL(Sound_PlayById)(BH_CALL(Rand)() & 1 ? 0x600 : 0x601);
}

// === Set-ups 18, 19, 20 (area 79: Claw, Cawer, Patrio, Dodai; rows 7, 6, 5) ==========================

// original 0x43ABA0: Boss_SetupTable[18]: End = Boss18_End, Exit = Boss18_Exit,
// Event = Boss18_Event.
extern "C" void __cdecl Boss18_Setup(void) { StoreHooks(0x43AC50, 0x43AD10, 0x43ABC0); }

// original 0x43ABC0: the event hook (EventAreaFight above).
extern "C" unsigned char __cdecl Boss18_Event(unsigned code) { return EventAreaFight(code); }

// original 0x43AC50: the end hook: the win's chapter step 0x1A (0x904AAD bit
// 1) or 0x1B; the rest as EndAreaFight above, Boss_SetByLeaderId before the
// step.
extern "C" void __cdecl Boss18_End(void) { EndAreaFight(0x1A, 0x1B, true); }

// original 0x43AD10: the exit hook: the actor tagged 0 (ExitAreaFight above).
extern "C" void __cdecl Boss18_Exit(void) { ExitAreaFight(0); }

// original 0x43AD60: Boss_SetupTable[19]: End = Boss19_End, Exit =
// Boss19_Exit, Event = Boss19_Event.
extern "C" void __cdecl Boss19_Setup(void) { StoreHooks(0x43AE10, 0x43AED0, 0x43AD80); }

// original 0x43AD80: Boss18_Event's body.
extern "C" unsigned char __cdecl Boss19_Event(unsigned code) { return EventAreaFight(code); }

// original 0x43AE10: the win's chapter step 0x1A (0x904AAD bit 1) or 0x23.
extern "C" void __cdecl Boss19_End(void) { EndAreaFight(0x1A, 0x23, true); }

// original 0x43AED0: the actor tagged 1.
extern "C" void __cdecl Boss19_Exit(void) { ExitAreaFight(1); }

// original 0x43AF20: Boss_SetupTable[20]: End = Boss20_End, Exit =
// Boss20_Exit, Event = Boss20_Event.
extern "C" void __cdecl Boss20_Setup(void) { StoreHooks(0x43AFD0, 0x43B080, 0x43AF40); }

// original 0x43AF40: Boss18_Event's body.
extern "C" unsigned char __cdecl Boss20_Event(unsigned code) { return EventAreaFight(code); }

// original 0x43AFD0: the win's chapter step 0x1A (0x904AAD bit 1) or 0x29;
// the chapter step stored before Boss_SetByLeaderId.
extern "C" void __cdecl Boss20_End(void) { EndAreaFight(0x1A, 0x29, false); }

// original 0x43B080: the actor tagged 2.
extern "C" void __cdecl Boss20_Exit(void) { ExitAreaFight(2); }

// === Kinds 24, 25 (Emitai, Golem, area 81) ========================================================

// original 0x43B280: BossKind_Table[24]: jmp [BossEmitai_Steps + 4 * +1].
extern "C" void __cdecl BossEmitai_Dispatch(unsigned passed) {
    Dispatch("BossEmitai_Dispatch", AddressOf(BossEmitai_Steps), 12, 1, passed);
}

// original 0x43B2A0: BossEmitai_Steps 0: +0xFC = 0x64D0A0, +0xF4 =
// BossEmitai_Hook, +0xF8 = 0x64D0B8; Sprite_Current +1 = 2; jmp Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossEmitai_Enter(void) {
    PointEnemy(0x64D0A0, 0x43B360, 0x64D0B8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43B2E0: BossEmitai_Steps 6: jmp [BossEmitai_ActSubs + 4 * +2].
extern "C" void __cdecl BossEmitai_ActDispatch(unsigned passed) {
    Dispatch("BossEmitai_ActDispatch", AddressOf(BossEmitai_ActSubs), 6, 2, passed);
}

// original 0x43B300: BossEmitai_ActSubs 4, the death: Sprite_EnsureAnimation(4),
// Sprite_Current (read after) +0x2A = 0, Sprite_ScriptTickOnce (its answer
// not read), Battle_EnemyDefeated, 0x939AD8's +0x110 |= 0x1000, then the
// states 3, 0, 0 with +0 &= 0xBF.
extern "C" void __cdecl BossEmitai_Death(void) {
    BH_CALL(Sprite_EnsureAnimation)(4);
    Sprite_Current[0x2A] = 0;
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    unsigned char* const e = Enemy();
    SetLong(e + 0x110, static_cast<std::int32_t>(static_cast<U>(Long(e + 0x110)) | 0x1000));
    DeathStates();
}

// original 0x43B360: the +0xF4 hook through BossEmitai_Hooks.
extern "C" void __cdecl BossEmitai_Hook(unsigned code) { HookTable("BossEmitai_Hook", AddressOf(BossEmitai_Hooks), code); }

// original 0x43B370: BossKind_Table[25]: jmp [BossGolem_Steps + 4 * +1].
extern "C" void __cdecl BossGolem_Dispatch(unsigned passed) {
    Dispatch("BossGolem_Dispatch", AddressOf(BossGolem_Steps), 12, 1, passed);
}

// original 0x43B390: BossGolem_Steps 0: +0xFC = 0x64D0AC, +0xF4 =
// BossGolem_Hook, +0xF8 = 0x64D0C0; Sprite_Current +1 = 2; jmp Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossGolem_Enter(void) {
    PointEnemy(0x64D0AC, 0x43B3D0, 0x64D0C0);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x43B3D0: the +0xF4 hook through BossGolem_Hooks (three BareRet).
extern "C" void __cdecl BossGolem_Hook(unsigned code) { HookTable("BossGolem_Hook", AddressOf(BossGolem_Hooks), code); }

// === Set-up 21 (Emitai, Golem, area 81 row 7) =====================================================

// original 0x43B3E0: Boss_SetupTable[21]: End = Boss21_End, Exit =
// Boss21_Exit, Event = Boss21_Event.
extern "C" void __cdecl Boss21_Setup(void) { StoreHooks(0x43B730, 0x43B480, 0x43B400); }

// original 0x43B400: the event hook: phase 3 - every actor 0..10 that
// Battle_ActorIsOut answers al 0 for gets bit 0x10: members 0..2 in their
// +0x90 (ObjTrio), enemies 3..10 in their object's +0x92. al 0 always. The
// original writes each actor into its own argument's low byte and pushes that
// dword (the caller's word above it); ours passes the same word.
extern "C" unsigned char __cdecl Boss21_Event(unsigned code) {
    if ((code & 0xFF) != 3) return 0;
    for (unsigned i = 0; i <= 2; ++i)
        if ((BH_CALL(Battle_ActorIsOut)((code & 0xFFFFFF00u) | i) & 0xFF) == 0)
            At(at::kPartyFlags + i * at::kPartyStride)[0] |= 0x10;
    for (unsigned i = 3; i <= 10; ++i)
        if ((BH_CALL(Battle_ActorIsOut)((code & 0xFFFFFF00u) | i) & 0xFF) == 0)
            At(at::kEnemy0Status + (i - 3) * at::kEnemyStride)[0] |= 0x10;
    return 0;
}

// original 0x43B480: the exit hook: BossActor_ClearBit40(0); Sprite_Current =
// BossActor_Find(0), its +0x2A = 0 (the answer untested: null faults, ours
// aborts), Sprite_SetAnimation(4); BossActor_Clear(1), BossActor_Clear(2).
extern "C" void __cdecl Boss21_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(0);
    unsigned char* const actor = BH_CALL(BossActor_Find)(0);
    Sprite_Current = actor;
    if (actor == nullptr)
        bof3::Fatal("Boss21_Exit: no field actor tagged 0 - the original writes +0x2A through the null (docs/boss_sd.md "
                    "section 6)");
    actor[0x2A] = 0;
    BH_CALL(Sprite_SetAnimation)(4);
    BH_CALL(BossActor_Clear)(1);
    BH_CALL(BossActor_Clear)(2);
}

// original 0x43B730: the end hook of set-ups 21 and 23: the win sets the
// field's move-script counter 0 to 0xA and jumps to the end phase's step 1;
// else step 2.
extern "C" void __cdecl Boss21_End(void) { EndCounter(0xA); }

// === Kind 27 (Garr, area 80) ======================================================================

// original 0x43B4B0: BossKind_Table[27]: jmp [BossGarr_Steps + 4 * +1].
extern "C" void __cdecl BossGarr_Dispatch(unsigned passed) {
    Dispatch("BossGarr_Dispatch", AddressOf(BossGarr_Steps), 12, 1, passed);
}

// original 0x43B4D0: BossGarr_Steps 0: +0xFC = 0x64D158, +0xF4 =
// BossGarr_Hook, +0xF8 = 0x64D164; Sprite_Current +1 = 1;
// Sprite_SetAnimationBank(0x153); Sprite_Current (read after) +0x2A = 0;
// Sprite_SetAnimation(0). Its eax is the last call's (no caller reads it).
extern "C" void __cdecl BossGarr_Enter(void) {
    PointEnemy(0x64D158, 0x43B5A0, 0x64D164);
    Sprite_Current[1] = 1;
    BH_CALL(Sprite_SetAnimationBank)(0x153);
    Sprite_Current[0x2A] = 0;
    BH_CALL(Sprite_SetAnimation)(0);
}

// original 0x43B530: BossGarr_Steps 6: jmp [BossGarr_ActSubs + 4 * +2].
extern "C" void __cdecl BossGarr_ActDispatch(unsigned passed) {
    Dispatch("BossGarr_ActDispatch", AddressOf(BossGarr_ActSubs), 6, 2, passed);
}

// original 0x43B5A0: the +0xF4 hook through BossGarr_Hooks (three BareRet).
extern "C" void __cdecl BossGarr_Hook(unsigned code) { HookTable("BossGarr_Hook", AddressOf(BossGarr_Hooks), code); }

void BossSd_Inject() {
    if (bof3::WantsShadow("boss_sd")) boss_sd::SelfTest();
    BOF3_INJECT(BossMutant_Dispatch);
    BOF3_INJECT(BossMutant_Enter);
    BOF3_INJECT(BossMutant_ActDispatch);
    BOF3_INJECT(BossMutant_Hook);
    BOF3_INJECT(Boss17_Setup);
    BOF3_INJECT(Boss17_End);
    BOF3_INJECT(BossClaw_Dispatch);
    BOF3_INJECT(BossClaw_Enter);
    BOF3_INJECT(BossClaw_Hook);
    BOF3_INJECT(BossCawer_Dispatch);
    BOF3_INJECT(BossCawer_Enter);
    BOF3_INJECT(BossCawer_Hook);
    BOF3_INJECT(BossPatrio_Dispatch);
    BOF3_INJECT(BossPatrio_Enter);
    BOF3_INJECT(BossPatrio_Hook);
    BOF3_INJECT(BossDodai_Dispatch);
    BOF3_INJECT(BossDodai_Enter);
    BOF3_INJECT(BossDodai_ActDispatch);
    BOF3_INJECT(BossDodai_Death);
    BOF3_INJECT(BossDodai_HitPoseDispatch);
    BOF3_INJECT(BossDodai_HitShake);
    BOF3_INJECT(BossDodai_Hook);
    BOF3_INJECT(BossHook_ActKindNone);
    BOF3_INJECT(BossDodai_HitSound);
    BOF3_INJECT(Boss18_Setup);
    BOF3_INJECT(Boss18_Event);
    BOF3_INJECT(Boss18_End);
    BOF3_INJECT(Boss18_Exit);
    BOF3_INJECT(Boss19_Setup);
    BOF3_INJECT(Boss19_Event);
    BOF3_INJECT(Boss19_End);
    BOF3_INJECT(Boss19_Exit);
    BOF3_INJECT(Boss20_Setup);
    BOF3_INJECT(Boss20_Event);
    BOF3_INJECT(Boss20_End);
    BOF3_INJECT(Boss20_Exit);
    BOF3_INJECT(BossEmitai_Dispatch);
    BOF3_INJECT(BossEmitai_Enter);
    BOF3_INJECT(BossEmitai_ActDispatch);
    BOF3_INJECT(BossEmitai_Death);
    BOF3_INJECT(BossEmitai_Hook);
    BOF3_INJECT(BossGolem_Dispatch);
    BOF3_INJECT(BossGolem_Enter);
    BOF3_INJECT(BossGolem_Hook);
    BOF3_INJECT(Boss21_Setup);
    BOF3_INJECT(Boss21_Event);
    BOF3_INJECT(Boss21_Exit);
    BOF3_INJECT(Boss21_End);
    BOF3_INJECT(BossGarr_Dispatch);
    BOF3_INJECT(BossGarr_Enter);
    BOF3_INJECT(BossGarr_ActDispatch);
    BOF3_INJECT(BossGarr_Hook);
}
