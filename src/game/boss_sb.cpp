// Group BSB of the boss round: fights 4..10 and 13 and enemy kinds 3, 4, 5
// and 8..11 (chapters 0 and 2) - 52 functions of 0x438290..0x43A022, the ones
// tools/boss_rows.py's group column gives BSB (analysis/boss_funcs.tsv,
// 2026-09-28), each read to its last instruction with capstone (2026-09-28)
// and taken through the boss harness (boss_harness.h). docs/boss_sb.md has
// them one row each.
//
// Names come from the US disc's area records as tools/boss_rows.py --disc
// prints them, never from memory of the game: kind 3 is one script for three
// enemies the tool names Engineer, Foreman and Miner (area 24), named for the
// first; kind 4 Worker (areas 2 and 32), kind 5 Operator (area 2), kinds 8..11
// Torast, Kassen, Galtel, Doksen (area 27). A set-up is Boss<id>_*, its fight
// in its evidence string.
//
//   - the kinds' dispatchers: `jmp [table + 4 * Sprite_Current +1]` through
//     the kind's state table, and each kind's +0xF4 hook, `jmp [table + 4 *
//     (word & 0xFF)]` through its hook table; kind 3's two +2 dispatchers;
//   - the kinds' state 0 (the entrance): the enemy's +0xFC / +0xF4 / +0xF8
//     stored, +1 = 2, Sprite_ScriptTick;
//   - kind 3's hit sequence (its +1 entry 11, where the generic table has
//     EnemyOp_HitPose): pose 4, a wait of 0x3C frames, then a step of 1.5
//     away along x (z in fight 5), the enemy's +0xFC re-pointed, 0x904AE8
//     bit 2 and the enemy's bit in 0x904AAD set;
//   - kind 5's hook entry 1 (the hit): the target block's word +4 cleared;
//   - the set-ups (the three hook stores), their event hooks (a seven-way
//     jump table by the phase code), end hooks (the chapter's step and the
//     way out) and exit hooks (the field actors posed from the enemies).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables where the original jumps through
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6); nothing reaches it. A dispatcher hands its handler the word the
// original's jmp leaves on the stack, and answers what the handler answers.
// Every call goes through the harness (BH_CALL / BH_AT), so the start-up fuzz
// can stand recorders in for the callees.
#include "game/boss_sb.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sb_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sb::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(at::kCurrentEnemy)))); }
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// A table entry as the original's jmp reaches it: the word its caller left on
// the stack, eax back.
using Entry = unsigned long (__cdecl*)(unsigned);
unsigned long CallEntry(U table, unsigned index, unsigned word) {
    // the entry as read: the fuzz swaps the table's cells for its recorders
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))))(word);
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
unsigned long Dispatch(const char* who, U table, unsigned entries, unsigned at, unsigned word) {
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
unsigned long HookDispatch(const char* who, U table, unsigned word) {
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
    SetLong(Enemy() + 0xFC, static_cast<std::int32_t>(fc));
    SetLong(Enemy() + 0xF4, static_cast<std::int32_t>(hook));
    SetLong(Enemy() + 0xF8, static_cast<std::int32_t>(f8));
}
unsigned char Enter(U fc, U hook, U f8) {
    StoreTables(fc, hook, f8);
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

void Setup(U end, U exit, U event) {
    SetLong(At(at::kHookEnd), static_cast<std::int32_t>(end));
    SetLong(At(at::kHookExit), static_cast<std::int32_t>(exit));
    SetLong(At(at::kHookEvent), static_cast<std::int32_t>(event));
}

// The event hooks' shared cases (the same code in B04, B05, B06 and B07).
// Phase 0: with round-flag bit 0x40 and the target 0, 0x904AAD bit 0.
void EventRoundFlag() {
    if ((B(0x904AA8) & 0x40) && B(at::kTarget) == 0) B(at::kPoseBits) |= 1;
}
// The action phase's test: actor 0, command kind 4, command id 0x78.
bool LeaderCommand78() {
    if (B(at::kActor) != 0 || B(at::kCommandKind) != 4) return false;
    return Word(At(static_cast<U>(Long(At(at::kCommand)))) + 2) == 0x78;
}
// Phase 3: the leader's HP 0 clears its +0x134 bit 1.
void EventLeaderDown() {
    if (Word(At(at::kLeaderHp)) == 0) B(at::kLeaderFlags) &= 0xFD;
}
// Phase 5: with 0x904AAD bit 0, the leader's command is kind 4, id 0x78, at
// target 0x40, and the battle's command kind and target the same.
void EventLeaderCommand() {
    if (!(B(at::kPoseBits) & 1)) return;
    SetWord(At(at::kLeaderCmdId), 0x78);
    B(at::kLeaderCmdKind) = 4;
    B(at::kCommandKind) = 4;
    B(at::kLeaderTarget) = 0x40;
    B(at::kTarget) = 0x40;
}

// The event hook of set-ups 4, 5 and 6 (three copies of one body).
unsigned char Kind3Event(unsigned code) {
    switch (code & 0xFF) {
    case 0: EventRoundFlag(); break;
    case 1:
        if (LeaderCommand78()) {
            SetWord(At(at::kEnemy0Hp), 1);
            SetWord(At(at::kEnemy0B0), 1);
            SetWord(At(at::kEnemy1Hp), 1);
            SetWord(At(at::kEnemy1B0), 1);
            B(at::kLeaderFlags) &= 0xFD;
        }
        break;
    case 3: EventLeaderDown(); break;
    case 5: EventLeaderCommand(); break;
    case 6: B(at::kLeaderFlags) |= 2; break;
    default: break;   // 2, 4, and anything past 6 (the original's ja)
    }
    return 0;
}

// The end hook of set-ups 4 / 6 and 5: the loss (0x904AE8 bit 0) - the byte
// 0x92BF18 = `pick`, 0x904AE5 |= 0x80, 0x446E20, then the chapter's step =
// 0x32; otherwise the step up by one and 0x446E20.
void EndChapter(unsigned char pick) {
    if (B(at::kBattleEnd) & 1) {
        B(at::kLeaderPick) = pick;
        B(at::kMusicFlags) |= 0x80;
        BH_AT(Handler, at::kEndThird)();
        B(at::kChapterStep) = 0x32;
        return;
    }
    B(at::kChapterStep) = static_cast<unsigned char>(B(at::kChapterStep) + 1);
    BH_AT(Handler, at::kEndThird)();
}

// BossActor_Find(tag) into Sprite_Current. The original writes through the
// answer untested; with no field actor carrying the tag it writes near
// address 0 and faults. Ours aborts there with a message (the owner's rule,
// round9 doc section 6; docs/boss_sb.md section 6), as BH's spawn writers do.
void FindActor(const char* who, unsigned tag) {
    unsigned char* const actor = BH_CALL(BossActor_Find)(tag);
    if (actor == nullptr)
        bof3::Fatal("%s: no field actor carries tag %u - the original writes through the null answer (docs/boss_sb.md section 6)",
                    who, tag);
    Sprite_Current = actor;
}

// Field actor `tag` posed from an enemy: bit 0x40 cleared, the enemy's place
// copied, Sprite_Current = the actor, animation bank 0x5F, +0x48 = 0, the
// flip +0x2A, animation 1, and the enemy's words +0x58 / +0x5A (Sprite_Current
// read after each call).
void PoseActor(unsigned tag, U enemy, U pose, unsigned char flip) {
    BH_CALL(BossActor_ClearBit40)(tag);
    BH_CALL(BossActor_CopyFrom)(tag, At(enemy), 1);
    FindActor("PoseActor", tag);
    BH_CALL(Sprite_SetAnimationBank)(0x5F);
    Sprite_Current[0x48] = 0;
    Sprite_Current[0x2A] = flip;
    BH_CALL(Sprite_SetAnimation)(1);
    SetWord(Sprite_Current + 0x58, Word(At(pose)));
    SetWord(Sprite_Current + 0x5A, Word(At(pose + 2)));
}

// The exit hook of set-ups 4, 5, 6: the loss clears bit 0x40 of the two
// actors; otherwise each is posed from its enemy (0 and 1).
void ExitPosePair(unsigned tag0, unsigned char flip) {
    if (B(at::kBattleEnd) & 1) {
        BH_CALL(BossActor_ClearBit40)(tag0);
        BH_CALL(BossActor_ClearBit40)(tag0 + 1);
        return;
    }
    PoseActor(tag0, at::kEnemy0, at::kEnemy0Pose, flip);
    PoseActor(tag0 + 1, at::kEnemy1, at::kEnemy1Pose, flip);
}

// The exit hook of set-ups 8, 9, 10: actor `tag` shown, found, bank 0x83,
// (the flip 1 in 9 and 10), animation 0, enemy 0's words +0x58 / +0x5A.
void ExitPoseOne(unsigned tag, bool flip) {
    BH_CALL(BossActor_ClearBit40)(tag);
    FindActor("ExitPoseOne", tag);
    BH_CALL(Sprite_SetAnimationBank)(0x83);
    if (flip) Sprite_Current[0x2A] = 1;
    BH_CALL(Sprite_SetAnimation)(0);
    SetWord(Sprite_Current + 0x58, Word(At(at::kEnemy0Pose)));
    SetWord(Sprite_Current + 0x5A, Word(At(at::kEnemy0Pose + 2)));
}

// The end hook of set-ups 8, 9, 10: the win (0x904AE8 bit 1) - the movement
// script's variable 3 = `var`, 0x446DE0; otherwise 0x446E00.
void EndVar(unsigned char var) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kScriptVar3) = var;
        BH_AT(Handler, at::kEndWin)();
        return;
    }
    BH_AT(Handler, at::kEndOther)();
}

}  // namespace

// ===========================================================================
// Kind 3 (Engineer, Foreman, Miner - one script; area 24, fights 4, 5, 6)
// ===========================================================================

// original 0x438290: BossKind_Table[3]: by Sprite_Current +1 through
// BossEngineer_Steps (12).
extern "C" unsigned long __cdecl BossEngineer_Dispatch(unsigned word) {
    return Dispatch("BossEngineer_Dispatch", AddressOf(BossEngineer_Steps), 12, 1, word);
}

// original 0x4382B0: state 0: +0xFC = BossEngineer_Anims, +0xF4 =
// BossEngineer_Hook, +0xF8 = BossEngineer_Cues; +1 = 2; Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossEngineer_Enter(void) {
    return Enter(AddressOf(BossEngineer_Anims), bof3::addr::BossEngineer_Hook, AddressOf(BossEngineer_Cues));
}

// original 0x4382F0: BossEngineer_Steps 6: by +2 through BossEngineer_ActSubs
// (6: EnemyOp_ActSubs with BossOp_Death at 4).
extern "C" unsigned long __cdecl BossEngineer_ActDispatch(unsigned word) {
    return Dispatch("BossEngineer_ActDispatch", AddressOf(BossEngineer_ActSubs), 6, 2, word);
}

// original 0x438310: BossEngineer_Steps 11: by +2 through
// BossEngineer_HitSteps (3: HitStart, HitStep, BossOp_ScriptTick).
extern "C" unsigned long __cdecl BossEngineer_HitDispatch(unsigned word) {
    return Dispatch("BossEngineer_HitDispatch", AddressOf(BossEngineer_HitSteps), 3, 2, word);
}

// original 0x438330: BossEngineer_HitSteps 0: BattleEnemy_SetAnimation(4);
// then (Sprite_Current read after the call) +9 = 0x3C and +2 up by one.
extern "C" void __cdecl BossEngineer_HitStart(void) {
    BH_CALL(BattleEnemy_SetAnimation)(4);
    Sprite_Current[9] = 0x3C;
    Sprite_Current[2] += 1;
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
        const bool fight5 = B(at::kFight) == 5;
        Sprite_Current[0x2A] = fight5 ? 0 : 1;
        unsigned char* const axis = Sprite_Current + (fight5 ? 0x38 : 0x34);
        SetLong(axis, static_cast<std::int32_t>(static_cast<U>(Long(axis)) + 0x18000u));
        const unsigned char* const s = Sprite_Current;
        const long ground = BH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
        SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground) & 0xFFFF);
        SetLong(Enemy() + 0xFC, static_cast<std::int32_t>(AddressOf(BossEngineer_AnimsHit)));
        BH_CALL(Sprite_SetAnimation)(1);
        B(at::kBattleEnd) |= 4;
        unsigned char* const t = Sprite_Current;
        const unsigned count = t[5] & 0x1F;
        B(at::kPoseBits) |= static_cast<unsigned char>(count < 8 ? 1u << count : 0u);
        t[2] += 1;
    }
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x438420: kind 3's +0xF4 hook: by the word through
// BossEngineer_Hooks (3, all BareRet).
extern "C" unsigned long __cdecl BossEngineer_Hook(unsigned word) {
    return HookDispatch("BossEngineer_Hook", AddressOf(BossEngineer_Hooks), word);
}

// ===========================================================================
// Set-ups 4, 5, 6 (BOSS004: area 24 rows 7, 6, 5; kind 3)
// ===========================================================================

// original 0x438430: Boss_SetupTable[4]: End Boss04_End, Exit Boss04_Exit,
// Event Boss04_Event.
extern "C" void __cdecl Boss04_Setup(void) { Setup(bof3::addr::Boss04_End, bof3::addr::Boss04_Exit, bof3::addr::Boss04_Event); }

// original 0x438450: set-up 4's event hook: by the phase code's low byte, a
// jump table of 7 (past 6 nothing) - 0: with 0x904AA8 bit 0x40 and the
// target 0, 0x904AAD |= 1; 1: when actor 0 gives command kind 4 with id 0x78
// (the word +2 of [0x904B40]), enemies 0 and 1's words +0xA4 and +0xB0 = 1 and
// the leader's +0x134 bit 1 cleared; 3: the leader's HP 0 clears that bit; 5:
// with 0x904AAD bit 0, the leader's command (+0x124 target 0x40, +0x125 kind
// 4, +0x126 id 0x78) and the battle's kind and target the same; 6: the bit
// set. al 0.
extern "C" unsigned char __cdecl Boss04_Event(unsigned code) { return Kind3Event(code); }

// original 0x438560: set-up 4's exit hook: the loss (0x904AE8 bit 0) clears
// bit 0x40 of actors 0 and 1; otherwise actors 0 and 1 posed from enemies 0
// and 1 (flip 1).
extern "C" void __cdecl Boss04_Exit(void) { ExitPosePair(0, 1); }

// original 0x4389E0: the end hook of set-ups 4 and 6: the loss - 0x92BF18 =
// 7, 0x904AE5 |= 0x80, 0x446E20, the chapter's step 0x8034E5 = 0x32;
// otherwise the step up by one and 0x446E20.
extern "C" void __cdecl Boss04_End(void) { EndChapter(7); }

// original 0x438650: Boss_SetupTable[5].
extern "C" void __cdecl Boss05_Setup(void) { Setup(bof3::addr::Boss05_End, bof3::addr::Boss05_Exit, bof3::addr::Boss05_Event); }

// original 0x438670: set-up 5's event hook, the body of Boss04_Event.
extern "C" unsigned char __cdecl Boss05_Event(unsigned code) { return Kind3Event(code); }

// original 0x438780: set-up 5's end hook: Boss04_End's with 0x92BF18 = 8.
extern "C" void __cdecl Boss05_End(void) { EndChapter(8); }

// original 0x4387C0: set-up 5's exit hook: actors 2 and 3 (flip 0).
extern "C" void __cdecl Boss05_Exit(void) { ExitPosePair(2, 0); }

// original 0x4388B0: Boss_SetupTable[6] (its end hook Boss04_End).
extern "C" void __cdecl Boss06_Setup(void) { Setup(bof3::addr::Boss04_End, bof3::addr::Boss06_Exit, bof3::addr::Boss06_Event); }

// original 0x4388D0: set-up 6's event hook, the body of Boss04_Event.
extern "C" unsigned char __cdecl Boss06_Event(unsigned code) { return Kind3Event(code); }

// original 0x438A20: set-up 6's exit hook: actors 4 and 5 (flip 1); after
// actor 5 (not on the loss's path) its dwords +0x34 / +0x38 / +0x3C = enemy
// 1's.
extern "C" void __cdecl Boss06_Exit(void) {
    if (B(at::kBattleEnd) & 1) {
        BH_CALL(BossActor_ClearBit40)(4);
        BH_CALL(BossActor_ClearBit40)(5);
        return;
    }
    PoseActor(4, at::kEnemy0, at::kEnemy0Pose, 1);
    PoseActor(5, at::kEnemy1, at::kEnemy1Pose, 1);
    SetLong(Sprite_Current + 0x34, Long(At(at::kEnemy1Place)));
    SetLong(Sprite_Current + 0x38, Long(At(at::kEnemy1Place + 4)));
    SetLong(Sprite_Current + 0x3C, Long(At(at::kEnemy1Place + 8)));
}

// ===========================================================================
// Kinds 4 and 5 (Worker, areas 2 and 32; Operator, area 2; fight 7)
// ===========================================================================

// original 0x438B30: BossKind_Table[4]: by +1 through BossWorker_Steps (12).
extern "C" unsigned long __cdecl BossWorker_Dispatch(unsigned word) {
    return Dispatch("BossWorker_Dispatch", AddressOf(BossWorker_Steps), 12, 1, word);
}

// original 0x438B50: state 0: +0xFC = BossWorker_Anims, +0xF4 =
// BossWorker_Hook, +0xF8 = BossWorker_Cues; +1 = 2; Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossWorker_Enter(void) {
    return Enter(AddressOf(BossWorker_Anims), bof3::addr::BossWorker_Hook, AddressOf(BossWorker_Cues));
}

// original 0x438B90: kind 4's +0xF4 hook through BossWorker_Hooks (3, BareRet).
extern "C" unsigned long __cdecl BossWorker_Hook(unsigned word) {
    return HookDispatch("BossWorker_Hook", AddressOf(BossWorker_Hooks), word);
}

// original 0x438BA0: BossKind_Table[5]: by +1 through BossOperator_Steps (12).
extern "C" unsigned long __cdecl BossOperator_Dispatch(unsigned word) {
    return Dispatch("BossOperator_Dispatch", AddressOf(BossOperator_Steps), 12, 1, word);
}

// original 0x438BC0: state 0: +0xFC = BossOperator_Anims, +0xF4 =
// BossOperator_Hook, +0xF8 = BossOperator_Cues; Sprite_Current +8 = 0, +1 =
// 2; Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossOperator_Enter(void) {
    StoreTables(AddressOf(BossOperator_Anims), bof3::addr::BossOperator_Hook, AddressOf(BossOperator_Cues));
    Sprite_Current[8] = 0;
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x438C10: kind 5's +0xF4 hook through BossOperator_Hooks (3:
// BareRet, BossOperator_HookHit, BareRet).
extern "C" unsigned long __cdecl BossOperator_Hook(unsigned word) {
    return HookDispatch("BossOperator_Hook", AddressOf(BossOperator_Hooks), word);
}

// original 0x438C20: BossOperator_Hooks 1 (the hook's word 1, the hit
// 0x4367EE): the word +4 of the target block [0x904B50] = 0. The word is not
// read.
extern "C" void __cdecl BossOperator_HookHit(unsigned) { SetWord(At(static_cast<U>(Long(At(at::kTargetBlock)))) + 4, 0); }

// ===========================================================================
// Set-up 7 (BOSS007: row 7, kinds 4 and 5) and set-up 13 (BOSS013: row 7)
// ===========================================================================

// original 0x438C30: Boss_SetupTable[7].
extern "C" void __cdecl Boss07_Setup(void) { Setup(bof3::addr::Boss07_End, bof3::addr::Boss07_Exit, bof3::addr::Boss07_Event); }

// original 0x438C50: set-up 7's event hook, by the phase code's low byte (a
// jump table of 7; past 6 nothing): 0, 3, 5 and 6 as Boss04_Event's; 1: on
// actor 0's command kind 4 id 0x78, 0x904AE8 |= 4, the leader's +0x134 bit 1
// cleared, Str_CopyN(0x904EA0, 0x65D008, 0x10), BattleBanner_Add(1, 1, 0,
// 0x1E, 0x904EA0), 0x8031F3 = 1, Sprite_Current = the leader,
// Sprite_SetAnimation(0x2C), then the phase 0x904AA0 = 4, 0x904AA5 = 0x3C,
// the step 0x904AA1 = 2; 2: with 0x904AE8 bit 2, 0x904AA5 down by one - al
// 0xFF while it is not 0 (BattleRoundEnd_NextRound holds the round). al 0
// otherwise.
extern "C" unsigned char __cdecl Boss07_Event(unsigned code) {
    switch (code & 0xFF) {
    case 0: EventRoundFlag(); break;
    case 1:
        if (LeaderCommand78()) {
            B(at::kBattleEnd) |= 4;
            B(at::kLeaderFlags) &= 0xFD;
            char* const text = reinterpret_cast<char*>(At(at::kBannerText));
            BH_CALL(Str_CopyN)(text, reinterpret_cast<const char*>(At(at::kBannerSource)), 0x10);
            BH_CALL(BattleBanner_Add)(1, 1, 0, 0x1E, text);
            B(at::kWindow4Flag) = 1;
            Sprite_Current = At(at::kLeader);
            BH_CALL(Sprite_SetAnimation)(0x2C);
            B(at::kPhase) = 4;
            B(at::kCountdown) = 0x3C;
            B(at::kStep) = 2;
        }
        break;
    case 2:
        if (B(at::kBattleEnd) & 4) {
            const auto left = static_cast<unsigned char>(B(at::kCountdown) - 1);
            B(at::kCountdown) = left;
            if (left != 0) return 0xFF;
        }
        break;
    case 3: EventLeaderDown(); break;
    case 5: EventLeaderCommand(); break;
    case 6: B(at::kLeaderFlags) |= 2; break;
    default: break;   // 4, and anything past 6
    }
    return 0;
}

// original 0x438DD0: set-up 7's end hook: the loss (0x904AE8 bit 0) - the
// chapter's step 0x32, 0x92BF18 = 6, the chapter's run 0x8034E4 = 6;
// otherwise the step up by one, 0x904AE8 |= 8, Draw_PassFlags 0, 0x92BF18 =
// 2, Music_Track 4. Then 0x446E20.
extern "C" void __cdecl Boss07_End(void) {
    if (B(at::kBattleEnd) & 1) {
        B(at::kChapterStep) = 0x32;
        B(at::kLeaderPick) = 6;
        B(at::kChapterRun) = 6;
    } else {
        const auto step = static_cast<unsigned char>(B(at::kChapterStep) + 1);
        const auto end = static_cast<unsigned char>(B(at::kBattleEnd) | 8);
        B(at::kDrawPassFlags) = 0;
        B(at::kLeaderPick) = 2;
        B(at::kChapterStep) = step;
        B(at::kBattleEnd) = end;
        B(at::kMusicTrack) = 4;
    }
    BH_AT(Handler, at::kEndThird)();
}

// original 0x438E30: the exit hook of set-ups 7 and 13: bit 0x40 of actors 0
// and 1 cleared.
extern "C" void __cdecl Boss07_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(0);
    BH_CALL(BossActor_ClearBit40)(1);
}

// original 0x439FE0: Boss_SetupTable[13]: End Boss13_End, Exit Boss07_Exit,
// Event BareRetZero.
extern "C" void __cdecl Boss13_Setup(void) { Setup(bof3::addr::Boss13_End, bof3::addr::Boss07_Exit, bof3::addr::BareRetZero); }

// original 0x43A000: set-up 13's end hook: the movement script's variable 3
// = 0x32; the win (0x904AE8 bit 1, read before) - 0x446DE0; otherwise
// 0x446E20, then 0x92BF18 = 3.
extern "C" void __cdecl Boss13_End(void) {
    const unsigned char end = B(at::kBattleEnd);
    B(at::kScriptVar3) = 0x32;
    if (end & 2) {
        BH_AT(Handler, at::kEndWin)();
        return;
    }
    BH_AT(Handler, at::kEndThird)();
    B(at::kLeaderPick) = 3;
}

// ===========================================================================
// Kinds 8..11 (Torast, Kassen, Galtel, Doksen; area 27) - the dispatchers,
// entrances and hooks; their shared death is group BH's (BossTorast_*)
// ===========================================================================

// original 0x438E50: BossKind_Table[8]: by +1 through BossTorast_Steps (12).
extern "C" unsigned long __cdecl BossTorast_Dispatch(unsigned word) {
    return Dispatch("BossTorast_Dispatch", AddressOf(BossTorast_Steps), 12, 1, word);
}
// original 0x438E70: kind 8's state 0: +0xFC = BossTorast_Anims, +0xF4 =
// BossTorast_Hook, +0xF8 = BossTorast_Cues; +1 = 2; Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossTorast_Enter(void) {
    return Enter(AddressOf(BossTorast_Anims), bof3::addr::BossTorast_Hook, AddressOf(BossTorast_Cues));
}
// original 0x4390E0: kind 8's +0xF4 hook through BossTorast_Hooks (3, BareRet).
extern "C" unsigned long __cdecl BossTorast_Hook(unsigned word) {
    return HookDispatch("BossTorast_Hook", AddressOf(BossTorast_Hooks), word);
}

// original 0x4390F0: BossKind_Table[9]: by +1 through BossKassen_Steps (12).
extern "C" unsigned long __cdecl BossKassen_Dispatch(unsigned word) {
    return Dispatch("BossKassen_Dispatch", AddressOf(BossKassen_Steps), 12, 1, word);
}
// original 0x439110: kind 9's state 0: kind 8's tables, +0xF4 = BossKassen_Hook.
extern "C" unsigned char __cdecl BossKassen_Enter(void) {
    return Enter(AddressOf(BossTorast_Anims), bof3::addr::BossKassen_Hook, AddressOf(BossTorast_Cues));
}
// original 0x439150: kind 9's +0xF4 hook through BossKassen_Hooks (3, BareRet).
extern "C" unsigned long __cdecl BossKassen_Hook(unsigned word) {
    return HookDispatch("BossKassen_Hook", AddressOf(BossKassen_Hooks), word);
}

// original 0x439160: BossKind_Table[10]: by +1 through BossGaltel_Steps (12).
extern "C" unsigned long __cdecl BossGaltel_Dispatch(unsigned word) {
    return Dispatch("BossGaltel_Dispatch", AddressOf(BossGaltel_Steps), 12, 1, word);
}
// original 0x439180: kind 10's state 0: kind 8's tables, +0xF4 = BossGaltel_Hook.
extern "C" unsigned char __cdecl BossGaltel_Enter(void) {
    return Enter(AddressOf(BossTorast_Anims), bof3::addr::BossGaltel_Hook, AddressOf(BossTorast_Cues));
}
// original 0x4391C0: kind 10's +0xF4 hook through BossGaltel_Hooks (3, BareRet).
extern "C" unsigned long __cdecl BossGaltel_Hook(unsigned word) {
    return HookDispatch("BossGaltel_Hook", AddressOf(BossGaltel_Hooks), word);
}

// original 0x4391D0: BossKind_Table[11]: by +1 through BossDoksen_Steps (12).
extern "C" unsigned long __cdecl BossDoksen_Dispatch(unsigned word) {
    return Dispatch("BossDoksen_Dispatch", AddressOf(BossDoksen_Steps), 12, 1, word);
}
// original 0x4391F0: kind 11's state 0: kind 8's tables, +0xF4 = BossDoksen_Hook.
extern "C" unsigned char __cdecl BossDoksen_Enter(void) {
    return Enter(AddressOf(BossTorast_Anims), bof3::addr::BossDoksen_Hook, AddressOf(BossTorast_Cues));
}
// original 0x439230: kind 11's +0xF4 hook through BossDoksen_Hooks (3, BareRet).
extern "C" unsigned long __cdecl BossDoksen_Hook(unsigned word) {
    return HookDispatch("BossDoksen_Hook", AddressOf(BossDoksen_Hooks), word);
}

// ===========================================================================
// Set-ups 8, 9, 10 (BOSS008: area 27 rows 7, 6, 5)
// ===========================================================================

// original 0x439240: Boss_SetupTable[8]: Event BareRetZero.
extern "C" void __cdecl Boss08_Setup(void) { Setup(bof3::addr::Boss08_End, bof3::addr::Boss08_Exit, bof3::addr::BareRetZero); }
// original 0x439260: set-up 8's end hook: the win - variable 3 = 0x11, 0x446DE0;
// otherwise 0x446E00.
extern "C" void __cdecl Boss08_End(void) { EndVar(0x11); }
// original 0x439280: set-up 8's exit hook: actor 0 from enemy 0's pose words
// (bank 0x83, animation 0, no flip).
extern "C" void __cdecl Boss08_Exit(void) { ExitPoseOne(0, false); }

// original 0x4392D0: Boss_SetupTable[9].
extern "C" void __cdecl Boss09_Setup(void) { Setup(bof3::addr::Boss09_End, bof3::addr::Boss09_Exit, bof3::addr::BareRetZero); }
// original 0x4392F0: set-up 9's end hook: variable 3 = 0x20 on the win.
extern "C" void __cdecl Boss09_End(void) { EndVar(0x20); }
// original 0x439310: set-up 9's exit hook: actor 1, the flip 1, enemy 0's words.
extern "C" void __cdecl Boss09_Exit(void) { ExitPoseOne(1, true); }

// original 0x439370: Boss_SetupTable[10].
extern "C" void __cdecl Boss10_Setup(void) { Setup(bof3::addr::Boss10_End, bof3::addr::Boss10_Exit, bof3::addr::BareRetZero); }
// original 0x439390: set-up 10's end hook: variable 3 = 0x30 on the win.
extern "C" void __cdecl Boss10_End(void) { EndVar(0x30); }
// original 0x4393B0: set-up 10's exit hook: actor 2, the flip 1, enemy 0's words.
extern "C" void __cdecl Boss10_Exit(void) { ExitPoseOne(2, true); }

void BossSb_Inject() {
    if (bof3::WantsShadow("boss_sb")) boss_sb::SelfTest();
    BOF3_INJECT(BossEngineer_Dispatch);
    BOF3_INJECT(BossEngineer_Enter);
    BOF3_INJECT(BossEngineer_ActDispatch);
    BOF3_INJECT(BossEngineer_HitDispatch);
    BOF3_INJECT(BossEngineer_HitStart);
    BOF3_INJECT(BossEngineer_HitStep);
    BOF3_INJECT(BossEngineer_Hook);
    BOF3_INJECT(Boss04_Setup);
    BOF3_INJECT(Boss04_Event);
    BOF3_INJECT(Boss04_Exit);
    BOF3_INJECT(Boss04_End);
    BOF3_INJECT(Boss05_Setup);
    BOF3_INJECT(Boss05_Event);
    BOF3_INJECT(Boss05_End);
    BOF3_INJECT(Boss05_Exit);
    BOF3_INJECT(Boss06_Setup);
    BOF3_INJECT(Boss06_Event);
    BOF3_INJECT(Boss06_Exit);
    BOF3_INJECT(BossWorker_Dispatch);
    BOF3_INJECT(BossWorker_Enter);
    BOF3_INJECT(BossWorker_Hook);
    BOF3_INJECT(BossOperator_Dispatch);
    BOF3_INJECT(BossOperator_Enter);
    BOF3_INJECT(BossOperator_Hook);
    BOF3_INJECT(BossOperator_HookHit);
    BOF3_INJECT(Boss07_Setup);
    BOF3_INJECT(Boss07_Event);
    BOF3_INJECT(Boss07_End);
    BOF3_INJECT(Boss07_Exit);
    BOF3_INJECT(Boss13_Setup);
    BOF3_INJECT(Boss13_End);
    BOF3_INJECT(BossTorast_Dispatch);
    BOF3_INJECT(BossTorast_Enter);
    BOF3_INJECT(BossTorast_Hook);
    BOF3_INJECT(BossKassen_Dispatch);
    BOF3_INJECT(BossKassen_Enter);
    BOF3_INJECT(BossKassen_Hook);
    BOF3_INJECT(BossGaltel_Dispatch);
    BOF3_INJECT(BossGaltel_Enter);
    BOF3_INJECT(BossGaltel_Hook);
    BOF3_INJECT(BossDoksen_Dispatch);
    BOF3_INJECT(BossDoksen_Enter);
    BOF3_INJECT(BossDoksen_Hook);
    BOF3_INJECT(Boss08_Setup);
    BOF3_INJECT(Boss08_End);
    BOF3_INJECT(Boss08_Exit);
    BOF3_INJECT(Boss09_Setup);
    BOF3_INJECT(Boss09_End);
    BOF3_INJECT(Boss09_Exit);
    BOF3_INJECT(Boss10_Setup);
    BOF3_INJECT(Boss10_End);
    BOF3_INJECT(Boss10_Exit);
}
