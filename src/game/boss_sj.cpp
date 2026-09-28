// Group BSJ of the boss round: fights 52, 54 and 55, kinds 59 and 61, and the
// effect tasks of BattleBossFx_Dispatch's slots 4 and 5 - 44 functions of
// 0x43F7A0..0x441087 (tools/boss_rows.py, 2026-09-28; analysis/boss_funcs.tsv's
// units K59, B52, F4, K61, B54, B55, F5), each read to its last instruction
// with capstone (2026-09-28) and taken through the boss harness
// (boss_harness.h). Round eleven, wave two; docs/boss_sj.md has them one row
// each. The group's fifth unit, FB93 (the Head Cracker rock), was ours
// already (magic rows, round nine) and nothing here calls it.
//
// The names are the disc's (tools/boss_rows.py --disc, the US disc's area
// records), the fights the tool's rows:
//
//   kind 59   D>Lord (area 172)      set-up 52  BOSS052, area 172 row 7
//   slot 4    D>Lord's effect task (BossDLord_HookFx creates it)
//   kind 61   Shroom (area 119)      set-up 54  BOSS054, area 119 row 6
//   set-up 55 BOSS055, area 198 row 7 (kind 62, Myria - group BSF's)
//   slot 5    Myria's effect task (0x440660, kind 62's, creates it)
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original indexes past a table - a dispatcher's state byte, a hook's
// word, a stack table by +2, the ten-byte pose tables by the word 0x904B7E,
// BossMyriaFx_Drift by +1, a task slot of 0xFF - ours aborts with a Fatal
// naming the function (the owner's rule, round9 doc section 6); nothing
// reaches it. Every call goes through the harness (BH_CALL / BH_AT / Phase),
// so the start-up fuzz can stand recorders in for the callees; a hook or
// table a function stores is the same literal address.
#include "game/boss_sj.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sj_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sj::at;
using U = std::uint32_t;
using boss_harness::Handler;
using boss_harness::Phase;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using HookFn = void (__cdecl*)(unsigned);
using Forward = U (__cdecl*)(U);
using RoundHighFn = U (__cdecl*)(U, U);

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(at::kCurrentEnemy)))); }
unsigned char* Owner() { return At(static_cast<U>(Long(At(at::kOwner)))); }
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after). The jmp
// leaves the caller's stack word in place and answers what the entry
// answers, so ours hands the word on and answers the entry's eax
// (docs/boss_sc.md section 1.2).
U Dispatch(const char* who, U table, unsigned entries, unsigned at, U through) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sj.md section 6)",
                    who, at, state, entries, (unsigned)table);
    // the entry as read: the fuzz swaps the table's cells for its recorders
    return reinterpret_cast<Forward>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))(through);
}

// An effect task's dispatcher: the same jmp by +1 through a table of the
// task's states, which take no word.
void DispatchTask(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sj.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
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

// A stack table: sub esp, 4n; mov [esp + 4i], handler i; call [esp + 4 * +2].
// The handlers are the group's own (or BattleFx_FreeTask), called by their
// addresses; past n the original calls through its caller's frame.
void Steps(const char* who, const U* handlers, unsigned n) {
    const unsigned step = Sprite_Current[2];
    if (step >= n)
        bof3::Fatal("%s: step byte +2 is %u, past its stack table of %u - the original calls through its caller's frame "
                    "(docs/boss_sj.md section 6)",
                    who, step, n);
    Phase(handlers[step])();
}

// The three hooks a set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    SetLong(At(at::kHookEnd), S(end));
    SetLong(At(at::kHookExit), S(exit));
    SetLong(At(at::kHookEvent), S(event));
}

// A kind's entry state (kinds 59 and 61 alike): 0x939AD8's +0xFC (its
// animation bytes), +0xF4 (its hook) and +0xF8 (its sound words), then its
// +0x114 |= 8 - 0x939AD8 read for each; Sprite_Current +1 = 2; a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char Enter(U fc, U f4, U f8) {
    SetLong(Enemy() + 0xFC, S(fc));
    SetLong(Enemy() + 0xF4, S(f4));
    SetLong(Enemy() + 0xF8, S(f8));
    unsigned char* const e = Enemy();
    SetLong(e + 0x114, S(static_cast<U>(Long(e + 0x114)) | 8));
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// The winning end hooks of set-ups 52 and 55: each party member whose +0 has
// bit 0 - Battle_RemoveFromTurnOrder(+5); then (read after the call) its +0x90
// and +8: Sprite_Current = the member, Sprite_PoseFromSet(+8 + 0x1C with
// +0x90 bit 14, else + 4 - a byte, 0x8C5D80, 0x1800).
void PoseParty() {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(at::kParty + m * at::kPartyStride);
        if ((p[0] & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(p[5]);
        const U flags = static_cast<U>(Long(p + 0x90));
        Sprite_Current = p;
        const unsigned pose = static_cast<unsigned char>(p[8] + ((flags & 0x4000) != 0 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(at::kPoseSet), 0x1800);
    }
}

// The ten-byte pose tables Myria's effect builds on its stack, indexed by the
// word 0x904B7E: 10 and above read the frame's two unset bytes and then its
// caller's stack.
unsigned char Pose(const char* who, const unsigned char (&poses)[10]) {
    const unsigned i = Word(At(at::kPoseIndex));
    if (i >= 10)
        bof3::Fatal("%s: the pose index 0x904B7E is %u, past its ten-byte stack table - the original reads its own frame "
                    "and its caller's (docs/boss_sj.md section 6)",
                    who, i);
    return poses[i];
}

constexpr U kFollow = bof3::addr::BossMyriaFx_Follow;   // BossMyriaFx_Follow, called directly by the states

// Myria's effect, steps 0 of states 0..2: Sprite_SetAnimationBank(bank); the
// Sprite_Current of after the call +0x48 = 0, +0x24 = 0, the dword +0x3C = 0;
// the state's loop called; +2 up by one (Sprite_Current read after).
void FxEnter(unsigned bank, U loop) {
    BH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(bank));
    Sprite_Current[0x48] = 0;
    Sprite_Current[0x24] = 0;
    SetLong(Sprite_Current + 0x3C, 0);
    Phase(loop)();
    ++Sprite_Current[2];
}

// Steps 1 of states 0..2: with the owner's +0xB set, Sprite_SetAnimation of
// the state's pose; BattleEnemy_ScriptTick, Sprite_QueueOverlay,
// BossMyriaFx_Follow.
void FxLoop(const char* who, const unsigned char (&poses)[10]) {
    if (Owner()[0xB] != 0) BH_CALL(Sprite_SetAnimation)(Pose(who, poses));
    BH_CALL(BattleEnemy_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    Phase(kFollow)();
}

// Steps 0 of states 3..6: bank; +0x48, +0x24, +0x3C = 0; Sprite_SetAnimation
// of the pose (always); the tick (once or not), Sprite_QueueOverlay,
// BossMyriaFx_Follow; +2 up by one (Sprite_Current read after the calls).
void FxEnterPosed(const char* who, unsigned bank, const unsigned char (&poses)[10], bool once) {
    BH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(bank));
    Sprite_Current[0x48] = 0;
    Sprite_Current[0x24] = 0;
    SetLong(Sprite_Current + 0x3C, 0);
    BH_CALL(Sprite_SetAnimation)(Pose(who, poses));
    if (once)
        BH_CALL(BattleEnemy_ScriptTickOnce)();
    else
        BH_CALL(BattleEnemy_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    Phase(kFollow)();
    ++Sprite_Current[2];
}

// Sprite_QueueOverlay, then a tail jump to BossMyriaFx_Follow.
void QueueAndFollow() {
    BH_CALL(Sprite_QueueOverlay)();
    Phase(kFollow)();
}

// One axis of BossMyriaFx_Follow: the s16 at BossMyriaFx_Drift[2 * +1 + axis]
// of the Sprite_Current read now, scaled (with +0x48) by the dword `scale_at`
// and handed to 0x441090 with it, or shifted to the high word and handed with
// 0x10000; ax added to the word `word_at` of that same sprite (the original
// takes the word's address before the call).
void Drift(bool scaled, unsigned axis, unsigned scale_at, unsigned word_at) {
    unsigned char* const s = Sprite_Current;
    const unsigned state = s[1];
    if (state >= 7)
        bof3::Fatal("BossMyriaFx_Follow: state byte +1 is %u, past the 7 pairs of BossMyriaFx_Drift - the original reads "
                    "the .data after (docs/boss_sj.md section 6)",
                    state);
    const auto d = static_cast<U>(static_cast<std::int32_t>(BossMyriaFx_Drift[2 * state + axis]));
    unsigned char* const w = s + word_at;
    U answer;
    if (scaled) {
        const U scale = static_cast<U>(Long(s + scale_at));
        answer = BH_AT(RoundHighFn, at::kRoundHigh)(d * scale, scale);
    } else {
        answer = BH_AT(RoundHighFn, at::kRoundHigh)(d << 16, 0x10000);
    }
    SetWord(w, static_cast<unsigned>(Word(w) + answer) & 0xFFFF);
}

}  // namespace

// ===========================================================================
// Kind 59 (D>Lord, area 172) and set-up 52 (BOSS052, area 172 row 7)
// ===========================================================================

// original 0x43F7A0: BossKind_Table[59]. jmp [BossDLord_States + 4 *
// Sprite_Current +1] (12: its entry, the generic enemy states, its act table
// at 6).
extern "C" unsigned long __cdecl BossDLord_Dispatch(unsigned long through) {
    return Dispatch("BossDLord_Dispatch", AddressOf(BossDLord_States), 12, 1, through);
}

// original 0x43F7C0: state 0 (BossDLord_Anims, _Hook, _Sounds; +0x114 |= 8;
// +1 = 2; tail Sprite_ScriptTick).
extern "C" unsigned char __cdecl BossDLord_Enter(void) {
    return Enter(AddressOf(BossDLord_Anims), bof3::addr::BossDLord_Hook, AddressOf(BossDLord_Sounds));
}

// original 0x43F820: state 6: jmp [BossDLord_ActSubs + 4 * +2] (6: the
// generic act table with BossDLord_Death at 4).
extern "C" unsigned long __cdecl BossDLord_ActDispatch(unsigned long through) {
    return Dispatch("BossDLord_ActDispatch", AddressOf(BossDLord_ActSubs), 6, 2, through);
}

// original 0x43F840: BossDLord_ActSubs 4, the death: Sprite_EnsureAnimation(3),
// Sprite_ScriptTickOnce, Battle_EnemyDefeated; then (read after the calls)
// 0x939AD8's +0x110 |= 0x1000, Sprite_Current +0 &= 0xBF, +1 = 3, +2 = 0,
// +3 = 0. The original leaves al 0; nothing reads it.
extern "C" void __cdecl BossDLord_Death(void) {
    BH_CALL(Sprite_EnsureAnimation)(3);
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    unsigned char* const e = Enemy();
    SetLong(e + 0x110, S(static_cast<U>(Long(e + 0x110)) | 0x1000));
    unsigned char* const s = Sprite_Current;
    s[0] &= 0xBF;
    s[1] = 3;
    s[2] = 0;
    s[3] = 0;
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
    const U actor = B(at::kActor);
    const unsigned char* const from = At(at::kEnemies + (actor - 3u) * at::kEnemyStride);
    unsigned char* const to = At(at::kTasks + slot * at::kTaskStride);
    for (unsigned i = 0; i < 0x80; i += 4) SetLong(to + i, Long(from + i));
    to[1] = 0;
    to[2] = 0;
    to[6] = 3;
    to[5] = 4;
    to[9] = 0;
    to[0x29] = 3;
}

// original 0x43F950: Boss_SetupTable[52]. The hooks: end Boss52_End, exit
// Boss52_Exit, event BareRetZero.
extern "C" void __cdecl Boss52_Setup(void) {
    StoreHooks(bof3::addr::Boss52_End, bof3::addr::Boss52_Exit, bof3::addr::BareRetZero);
}

// original 0x43F970: the end hook. Not won (0x904AE8 bit 1 clear): a tail jump
// to 0x446E00 (the end phase, step 2). Won: the party posed (PoseParty), the
// chapter step 0x8034E5 = 0x1A, a tail jump to 0x446DE0 (step 1).
extern "C" void __cdecl Boss52_End(void) {
    if ((B(at::kBattleEnd) & 2) == 0) {
        BH_AT(Handler, at::kEndOther)();
        return;
    }
    PoseParty();
    B(at::kChapterStep) = 0x1A;
    BH_AT(Handler, at::kEndWin)();
}

// original 0x43FA90: the exit hook. BossActor_ClearBit40(0); BossActor_Find(0)
// into Field_ActiveMember and Sprite_Current; Sprite_SetAnimation(5).
extern "C" void __cdecl Boss52_Exit(void) {
    BH_CALL(BossActor_ClearBit40)(0);
    unsigned char* const actor = BH_CALL(BossActor_Find)(0);
    Field_ActiveMember = actor;
    Sprite_Current = actor;
    BH_CALL(Sprite_SetAnimation)(5);
}

// ===========================================================================
// D>Lord's effect task (BattleBossFx_Dispatch slot 4)
// ===========================================================================

// original 0x43FAC0: slot 4: jmp [BossDLordFx_States + 4 * Sprite_Current +1]
// (one entry).
extern "C" void __cdecl BossDLordFx_Dispatch(void) { DispatchTask("BossDLordFx_Dispatch", AddressOf(BossDLordFx_States), 1); }

// original 0x43FAE0: BossDLordFx_States 0: by +2 through a stack table of
// BossDLordFx_Start, BossDLordFx_Count, BattleFx_FreeTask.
extern "C" void __cdecl BossDLordFx_StepDispatch(void) {
    static const U kSteps[] = {bof3::addr::BossDLordFx_Start, bof3::addr::BossDLordFx_Count, bof3::addr::BattleFx_FreeTask};
    Steps("BossDLordFx_StepDispatch", kSteps, 3);
}

// original 0x43FB10: step 0. Sprite_Current +0xB = 0; +9 = the effect size of
// enemy 0's data record (the byte 0x8C5652 + 0x8C * enemy 0's +0xF0, as
// BattleActor_FxSize reads it - unchecked, the record index is the enemy's
// own); Sprite_SetAnimation(4), Sprite_ScriptTick, Sprite_QueueOverlay; +2 up
// by one (Sprite_Current read after the calls).
extern "C" void __cdecl BossDLordFx_Start(void) {
    Sprite_Current[0xB] = 0;
    const unsigned type = B(at::kEnemy0Type);
    Sprite_Current[9] = At(at::kEnemyFxSize + type * at::kEnemyDataStride)[0];
    BH_CALL(Sprite_SetAnimation)(4);
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    ++Sprite_Current[2];
}

// original 0x43FB60: step 1. The count +9: at 0xFF nothing; at 0
// Sound_PlayEffect(0x603) and (Sprite_Current read after the call) +9 = 0xFF;
// else down by one. Sprite_ScriptTick, Sprite_QueueOverlay; with round-flag
// bit 2 (0x904AA8) +2 up by one.
extern "C" void __cdecl BossDLordFx_Count(void) {
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[9];
    if (count != 0xFF) {
        if (count == 0) {
            BH_CALL(Sound_PlayEffect)(0x603);
            Sprite_Current[9] = 0xFF;
        } else {
            s[9] = static_cast<unsigned char>(count - 1);
        }
    }
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    if (B(at::kFlags) & 4) ++Sprite_Current[2];
}

// ===========================================================================
// Kind 61 (Shroom, area 119) and set-up 54 (BOSS054, area 119 row 6)
// ===========================================================================

// original 0x43FBB0: BossKind_Table[61]. jmp [BossShroom_States + 4 * +1] (12).
extern "C" unsigned long __cdecl BossShroom_Dispatch(unsigned long through) {
    return Dispatch("BossShroom_Dispatch", AddressOf(BossShroom_States), 12, 1, through);
}

// original 0x43FBD0: state 0 (BossShroom_Anims, _Hook, _Sounds; +0x114 |= 8;
// +1 = 2; tail Sprite_ScriptTick).
extern "C" unsigned char __cdecl BossShroom_Enter(void) {
    return Enter(AddressOf(BossShroom_Anims), bof3::addr::BossShroom_Hook, AddressOf(BossShroom_Sounds));
}

// original 0x43FC30: the +0xF4 hook: BossShroom_Hooks (3, BareRet each).
extern "C" void __cdecl BossShroom_Hook(unsigned word) { DispatchHook("BossShroom_Hook", AddressOf(BossShroom_Hooks), word); }

// original 0x43FC40: Boss_SetupTable[54]. The hooks: end Boss54_End, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss54_Setup(void) {
    StoreHooks(bof3::addr::Boss54_End, bof3::addr::BareRet, bof3::addr::BareRetZero);
}

// original 0x43FC60: the end hook. Won: movement-script variable 3 (0x903848)
// = 0x32, a tail jump to 0x446DE0; else to 0x446E00.
extern "C" void __cdecl Boss54_End(void) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kScriptVar3) = 0x32;
        BH_AT(Handler, at::kEndWin)();
    } else {
        BH_AT(Handler, at::kEndOther)();
    }
}

// ===========================================================================
// Set-up 55 (BOSS055, area 198 row 7; kind 62, Myria, is group BSF's)
// ===========================================================================

// original 0x4406E0: Boss_SetupTable[55]. The hooks: end Boss55_End, exit
// BossHook_ExitActor0Bit40, event BareRetZero.
extern "C" void __cdecl Boss55_Setup(void) {
    StoreHooks(bof3::addr::Boss55_End, bof3::addr::BossHook_ExitActor0Bit40, bof3::addr::BareRetZero);
}

// original 0x440700: the end hook. Not won: a tail jump to 0x446E00. Won: the
// party posed (PoseParty), the chapter step 0x8034E5 = 0xF, 0x446E20 called
// (the end phase's step 3), then Music_Track = 0xFF.
extern "C" void __cdecl Boss55_End(void) {
    if ((B(at::kBattleEnd) & 2) == 0) {
        BH_AT(Handler, at::kEndOther)();
        return;
    }
    PoseParty();
    B(at::kChapterStep) = 0xF;
    BH_AT(Handler, at::kEndThird)();
    Music_Track = 0xFF;
}

// ===========================================================================
// Myria's effect task (BattleBossFx_Dispatch slot 5)
// ===========================================================================

// original 0x440830: slot 5: 0x939AD8 = enemy 0's object, then jmp
// [BossMyriaFx_States + 4 * Sprite_Current +1] (7: states 0..6).
extern "C" void __cdecl BossMyriaFx_Dispatch(void) {
    SetLong(At(at::kCurrentEnemy), S(at::kEnemies));
    DispatchTask("BossMyriaFx_Dispatch", AddressOf(BossMyriaFx_States), 7);
}

// State 0: a stack table by +2 of Enter / Loop (0x440850); Enter (0x440880):
// bank 0x30D; Loop (0x4408C0): poses 0, 1, 1, 1, 0, 1, 1, 1, 1, 1.
extern "C" void __cdecl BossMyriaFx_State0(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State0Enter, bof3::addr::BossMyriaFx_State0Loop};
    Steps("BossMyriaFx_State0", kSteps, 2);
}
extern "C" void __cdecl BossMyriaFx_State0Enter(void) { FxEnter(0x30D, bof3::addr::BossMyriaFx_State0Loop); }
extern "C" void __cdecl BossMyriaFx_State0Loop(void) {
    static const unsigned char kPoses[10] = {0, 1, 1, 1, 0, 1, 1, 1, 1, 1};
    FxLoop("BossMyriaFx_State0Loop", kPoses);
}

// State 1 (0x440930): Enter (0x440960) bank 0x30C; Loop (0x4409A0) poses 1, 3,
// 5, 3, 3, 3, 3, 3, 3, 3.
extern "C" void __cdecl BossMyriaFx_State1(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State1Enter, bof3::addr::BossMyriaFx_State1Loop};
    Steps("BossMyriaFx_State1", kSteps, 2);
}
extern "C" void __cdecl BossMyriaFx_State1Enter(void) { FxEnter(0x30C, bof3::addr::BossMyriaFx_State1Loop); }
extern "C" void __cdecl BossMyriaFx_State1Loop(void) {
    static const unsigned char kPoses[10] = {1, 3, 5, 3, 3, 3, 3, 3, 3, 3};
    FxLoop("BossMyriaFx_State1Loop", kPoses);
}

// State 2 (0x440A10): Enter (0x440A40) bank 0x30C; Loop (0x440A80) poses 0, 2,
// 4, 2, 2, 2, 2, 2, 2, 2.
extern "C" void __cdecl BossMyriaFx_State2(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State2Enter, bof3::addr::BossMyriaFx_State2Loop};
    Steps("BossMyriaFx_State2", kSteps, 2);
}
extern "C" void __cdecl BossMyriaFx_State2Enter(void) { FxEnter(0x30C, bof3::addr::BossMyriaFx_State2Loop); }
extern "C" void __cdecl BossMyriaFx_State2Loop(void) {
    static const unsigned char kPoses[10] = {0, 2, 4, 2, 2, 2, 2, 2, 2, 2};
    FxLoop("BossMyriaFx_State2Loop", kPoses);
}

// State 3 (0x440AF0): Enter (0x440B20) bank 0x30D, poses 0, 2, 0, 4, 0, 5, 0,
// 0, 0, 0, the tick once; Wait (0x440BB0): BattleEnemy_ScriptTick, then with
// the owner's +1 at 2 (read after the call) a tail jump to
// BattleTask_FreeCurrent, else Sprite_QueueOverlay and BossMyriaFx_Follow.
extern "C" void __cdecl BossMyriaFx_State3(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State3Enter, bof3::addr::BossMyriaFx_State3Wait};
    Steps("BossMyriaFx_State3", kSteps, 2);
}
extern "C" void __cdecl BossMyriaFx_State3Enter(void) {
    static const unsigned char kPoses[10] = {0, 2, 0, 4, 0, 5, 0, 0, 0, 0};
    FxEnterPosed("BossMyriaFx_State3Enter", 0x30D, kPoses, true);
}
extern "C" void __cdecl BossMyriaFx_State3Wait(void) {
    BH_CALL(BattleEnemy_ScriptTick)();
    if (Owner()[1] == 2) {
        BH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    QueueAndFollow();
}

// State 4 (0x440BD0): Enter (0x440C00) bank 0x318, poses 0, 0, 5, 0, 0, 0, 0,
// 0, 0, 0, the tick once; Wait (0x440C90): when BattleEnemy_ScriptTickOnce
// answers al not 0, the owner's +1 up by one and +2 = 0 and a tail jump to
// BattleTask_FreeCurrent; else Sprite_QueueOverlay and BossMyriaFx_Follow.
extern "C" void __cdecl BossMyriaFx_State4(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State4Enter, bof3::addr::BossMyriaFx_State4Wait};
    Steps("BossMyriaFx_State4", kSteps, 2);
}
extern "C" void __cdecl BossMyriaFx_State4Enter(void) {
    static const unsigned char kPoses[10] = {0, 0, 5, 0, 0, 0, 0, 0, 0, 0};
    FxEnterPosed("BossMyriaFx_State4Enter", 0x318, kPoses, true);
}
extern "C" void __cdecl BossMyriaFx_State4Wait(void) {
    if (static_cast<unsigned char>(BH_CALL(BattleEnemy_ScriptTickOnce)()) != 0) {
        unsigned char* const o = Owner();
        ++o[1];
        Owner()[2] = 0;
        BH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    QueueAndFollow();
}

// State 5 (0x440CC0): Enter (0x440CF0) bank 0x318, poses 0, 0, 0, 0, 4, 0, 0,
// 2, 0, 3, the tick each frame; Wait (0x440D80): with the owner's +1 at 2 a
// tail jump to BattleTask_FreeCurrent; else BattleEnemy_ScriptTick,
// Sprite_QueueOverlay, BossMyriaFx_Follow.
extern "C" void __cdecl BossMyriaFx_State5(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State5Enter, bof3::addr::BossMyriaFx_State5Wait};
    Steps("BossMyriaFx_State5", kSteps, 2);
}
extern "C" void __cdecl BossMyriaFx_State5Enter(void) {
    static const unsigned char kPoses[10] = {0, 0, 0, 0, 4, 0, 0, 2, 0, 3};
    FxEnterPosed("BossMyriaFx_State5Enter", 0x318, kPoses, false);
}
extern "C" void __cdecl BossMyriaFx_State5Wait(void) {
    if (Owner()[1] == 2) {
        BH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    BH_CALL(BattleEnemy_ScriptTick)();
    QueueAndFollow();
}

// State 6 (0x440DA0): a stack table of three - Enter (0x440DD0) bank 0x318,
// poses 0, 0, 0, 0, 0, 0, 0, 2, 1, 3, the tick each frame; Loop (0x440E60):
// with the owner's +0xB set, Sprite_SetAnimation of the same poses and +2 up by
// one (Sprite_Current read after the call), then BattleEnemy_ScriptTick,
// Sprite_QueueOverlay, BossMyriaFx_Follow; Wait (0x440ED0): when
// BattleEnemy_ScriptTickOnce answers al not 0 the owner's +2 up by one and a
// tail jump to BattleTask_FreeCurrent, else Sprite_QueueOverlay and
// BossMyriaFx_Follow.
extern "C" void __cdecl BossMyriaFx_State6(void) {
    static const U kSteps[] = {bof3::addr::BossMyriaFx_State6Enter, bof3::addr::BossMyriaFx_State6Loop,
                               bof3::addr::BossMyriaFx_State6Wait};
    Steps("BossMyriaFx_State6", kSteps, 3);
}
extern "C" void __cdecl BossMyriaFx_State6Enter(void) {
    static const unsigned char kPoses[10] = {0, 0, 0, 0, 0, 0, 0, 2, 1, 3};
    FxEnterPosed("BossMyriaFx_State6Enter", 0x318, kPoses, false);
}
extern "C" void __cdecl BossMyriaFx_State6Loop(void) {
    static const unsigned char kPoses[10] = {0, 0, 0, 0, 0, 0, 0, 2, 1, 3};
    if (Owner()[0xB] != 0) {
        BH_CALL(Sprite_SetAnimation)(Pose("BossMyriaFx_State6Loop", kPoses));
        ++Sprite_Current[2];
    }
    BH_CALL(BattleEnemy_ScriptTick)();
    QueueAndFollow();
}
extern "C" void __cdecl BossMyriaFx_State6Wait(void) {
    if (static_cast<unsigned char>(BH_CALL(BattleEnemy_ScriptTickOnce)()) != 0) {
        ++Owner()[2];
        BH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    QueueAndFollow();
}

// original 0x440EF0 (0x440EF0..0x441087; the tool's cut at 0x44103A is this
// function's own branch): the owner's +0, +0x48, dwords +0x40 / +0x44, +0x27,
// +0x28, +0x2A, words +0x2E / +0x30 / +0x32, +0x5C..+0x5F copied to
// Sprite_Current, in that order; then by its +0x48 the words +0x2E and +0x30
// drift by BossMyriaFx_Drift[+1] (Drift above), the second axis on the
// Sprite_Current of after the first call. Area198_EffectA6Follow (0x42D580)
// is the same shape.
extern "C" void __cdecl BossMyriaFx_Follow(void) {
    {
        const unsigned char* const o = Owner();
        unsigned char* const s = Sprite_Current;
        s[0] = o[0];
        s[0x48] = o[0x48];
        SetLong(s + 0x40, Long(o + 0x40));
        SetLong(s + 0x44, Long(o + 0x44));
        s[0x27] = o[0x27];
        s[0x28] = o[0x28];
        s[0x2A] = o[0x2A];
        SetWord(s + 0x2E, Word(o + 0x2E));
        SetWord(s + 0x30, Word(o + 0x30));
        SetWord(s + 0x32, Word(o + 0x32));
        s[0x5C] = o[0x5C];
        s[0x5D] = o[0x5D];
        s[0x5E] = o[0x5E];
        s[0x5F] = o[0x5F];
    }
    const bool scaled = Sprite_Current[0x48] != 0;
    Drift(scaled, 0, 0x40, 0x2E);
    Drift(scaled, 1, 0x44, 0x30);
}

void BossSj_Inject() {
    if (bof3::WantsShadow("boss_sj")) boss_sj::SelfTest();
    BOF3_INJECT(BossDLord_Dispatch);
    BOF3_INJECT(BossDLord_Enter);
    BOF3_INJECT(BossDLord_ActDispatch);
    BOF3_INJECT(BossDLord_Death);
    BOF3_INJECT(BossDLord_Hook);
    BOF3_INJECT(BossDLord_HookFx);
    BOF3_INJECT(Boss52_Setup);
    BOF3_INJECT(Boss52_End);
    BOF3_INJECT(Boss52_Exit);
    BOF3_INJECT(BossDLordFx_Dispatch);
    BOF3_INJECT(BossDLordFx_StepDispatch);
    BOF3_INJECT(BossDLordFx_Start);
    BOF3_INJECT(BossDLordFx_Count);
    BOF3_INJECT(BossShroom_Dispatch);
    BOF3_INJECT(BossShroom_Enter);
    BOF3_INJECT(BossShroom_Hook);
    BOF3_INJECT(Boss54_Setup);
    BOF3_INJECT(Boss54_End);
    BOF3_INJECT(Boss55_Setup);
    BOF3_INJECT(Boss55_End);
    BOF3_INJECT(BossMyriaFx_Dispatch);
    BOF3_INJECT(BossMyriaFx_State0);
    BOF3_INJECT(BossMyriaFx_State0Enter);
    BOF3_INJECT(BossMyriaFx_State0Loop);
    BOF3_INJECT(BossMyriaFx_State1);
    BOF3_INJECT(BossMyriaFx_State1Enter);
    BOF3_INJECT(BossMyriaFx_State1Loop);
    BOF3_INJECT(BossMyriaFx_State2);
    BOF3_INJECT(BossMyriaFx_State2Enter);
    BOF3_INJECT(BossMyriaFx_State2Loop);
    BOF3_INJECT(BossMyriaFx_State3);
    BOF3_INJECT(BossMyriaFx_State3Enter);
    BOF3_INJECT(BossMyriaFx_State3Wait);
    BOF3_INJECT(BossMyriaFx_State4);
    BOF3_INJECT(BossMyriaFx_State4Enter);
    BOF3_INJECT(BossMyriaFx_State4Wait);
    BOF3_INJECT(BossMyriaFx_State5);
    BOF3_INJECT(BossMyriaFx_State5Enter);
    BOF3_INJECT(BossMyriaFx_State5Wait);
    BOF3_INJECT(BossMyriaFx_State6);
    BOF3_INJECT(BossMyriaFx_State6Enter);
    BOF3_INJECT(BossMyriaFx_State6Loop);
    BOF3_INJECT(BossMyriaFx_State6Wait);
    BOF3_INJECT(BossMyriaFx_Follow);
}
