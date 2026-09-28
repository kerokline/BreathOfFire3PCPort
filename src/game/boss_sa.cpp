// Round eleven group BSA: 49 functions of the boss band's first units
// (tools/boss_rows.py --groups, 2026-09-28; analysis/boss_funcs.tsv's group
// column BSA), each read to its last instruction with capstone (2026-09-28)
// and taken through the boss harness (boss_harness.h). docs/boss_sa.md has
// them one row each. The enemy and fight names are the disc's, as
// tools/boss_rows.py --disc prints them - not memory of the game:
//
//   - fight 1 (BOSS001, area 24 row 4): its set-up and three hooks, and its
//     two kinds, 6 (Gary) and 7 (Mogu): each a state table on EnemyOp_Steps'
//     shape with its own entrance (state 0), its own action table (state 6,
//     the generic subs with BossOp_Death at 4) and its own state 11 - a
//     scripted end, Mogu counting 0x3C frames in 0x904B7E, Gary waiting for
//     that count, both then stepping their +0x38 and taking a new pose;
//   - kinds 1 and 2 (Nue, areas 23 and 22) and 46 (Sample 1, area 158), and
//     the three set-ups of BOSS002 (fights 2, 3 and 39, row 7 of those
//     areas: which fight is which area the tool leaves to the reader, and
//     this code never reads 0x904AAA). Kind 1 has a state 9 of its own (the
//     field actor tagged 0 walked by MoveCmd_OpE9, then the win) and a hook
//     table: at the action pick, an HP of 0xFFFF sets action kind 3; at the
//     hit, HP less the pending damage at or below a quarter of the maximum
//     sets 0x904AAD bit 0 and the HP to 0xFFFF;
//   - kind 39 (Weretigr, area 35, fight 33's): its state 4 (an effect task of
//     the kind-3 dispatcher, two cues, the turn closed) and states 9's end
//     like kind 1's, which pays the enemy's experience; its hook table's hit
//     entry is kind 1's body (0x438030, the one function two units share).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers index their tables unchecked in the original; ours abort past
// them with a Fatal naming the function (the owner's rule for an unchecked
// index, round9 doc section 6; no route reaches it). Every call goes through
// the harness (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in
// for the callees; every hook and table pointer the originals store is stored
// here as the same literal.
#include "game/boss_sa.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sa_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sa::at;
using U = std::uint32_t;
using boss_harness::AnswerHandler;
using boss_harness::Handler;
using boss_harness::Hook;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(at::kCurrentEnemy)))); }
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, as
// read (the fuzz swaps the cells for recorders); a Fatal past them (the
// original jumps through whatever follows). The handler's eax passes through
// the jmp; EnemyRunAll does not read it.
U Dispatch(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sa.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return reinterpret_cast<AnswerHandler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
}

// An enemy's +0xF4 hook: jmp [table + 4 * (word & 0xFF)] - the handler gets
// the same word (the jmp leaves the caller's argument in place).
U HookDispatch(const char* who, U table, unsigned entries, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= entries)
        bof3::Fatal("%s: hook word %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sa.md section 6)",
                    who, i, entries, (unsigned)table);
    return reinterpret_cast<Hook>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * i)))))(word);
}

// A kind's state 0 (0x437A30's shape, five kinds): 0x939AD8's +0xFC (the
// animation bytes), +0xF4 (the hook), +0xF8 (the cue words) - 0x939AD8 read
// before each store -, Sprite_Current +1 = 2 (the idle step), and a tail jump
// to Sprite_ScriptTick (its al the answer).
void Install(U anims, U hook, U cues) {
    SetLong(Enemy() + 0xFC, static_cast<std::int32_t>(anims));
    SetLong(Enemy() + 0xF4, static_cast<std::int32_t>(hook));
    SetLong(Enemy() + 0xF8, static_cast<std::int32_t>(cues));
}
unsigned char EnterTick() {
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// Gary's and Mogu's end: Sprite_Current's +0x38 up by 0x8000 (half a unit),
// its ground word +0x3E the map's elevation there, animation bank 0x5F,
// 0x939AD8's +0xFC the kind's second animation bytes, +0x2A (the flip) 0,
// then the animation - Sprite_Current read again after each call.
void EndStep(U anims, unsigned animation) {
    AddLong(Sprite_Current + 0x38, 0x8000);
    unsigned char* const s = Sprite_Current;
    const long ground = BH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground) & 0xFFFF);
    BH_CALL(Sprite_SetAnimationBank)(0x5F);
    SetLong(Enemy() + 0xFC, static_cast<std::int32_t>(anims));
    Sprite_Current[0x2A] = 0;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
}

// Kinds 1 and 39's end: Sprite_ScriptTickOnce (its answer not read); then
// MoveCmd_OpE9 on the field actor tagged 0 (BossActor_Index(0), its object +
// 0x80) with (0, -14, 0x40, 0xFC00, 0xFF, 0); while it answers al not 0,
// nothing more. Then Battle_ClearActorBit(Sprite_Current +5) and 0x904AE8
// bit 1 (the win). Answers whether the walk is over.
bool EndWalk() {
    BH_CALL(Sprite_ScriptTickOnce)();
    const unsigned index = BH_CALL(BossActor_Index)(0);
    unsigned char* const actor = At(at::kFieldActors + index * at::kObjectStride);
    if (BH_CALL(MoveCmd_OpE9)(actor, 0, -0xE, 0x40, 0xFC00, 0xFF, 0) != 0) return false;
    BH_CALL(Battle_ClearActorBit)(Sprite_Current[5]);
    return true;
}

// The hooks' action-pick entry of kinds 1 and 39: an HP of 0xFFFF (the mark
// the hit entry leaves) makes the action's kind 3 and Sprite_Current +4 0.
bool PickSpent() {
    if (Word(Enemy() + 0xA4) != 0xFFFF) return false;
    unsigned char* const s = Sprite_Current;
    B(at::kActionKind) = 3;
    s[4] = 0;
    return true;
}

// Set-ups 2 and 3's end hook: without 0x904AE8 bits 1 and 2 the end phase's
// step 2 (0x446E00); with either, what `with` does, MoveScript counter 0 and
// the music track, and step 1 (0x446DE0).
template <typename F> void EndByBits(F with, unsigned counter, unsigned track) {
    if ((B(at::kBattleEnd) & 6) == 0) {
        BH_AT(Handler, at::kEndOther)();
        return;
    }
    with();
    B(at::kMoveVar3) = static_cast<unsigned char>(counter);
    Music_Track = static_cast<unsigned char>(track);
    BH_AT(Handler, at::kEndWin)();
}

void SetHooks(U end, U exit, U event) {
    SetLong(At(boss_harness::at::kHookEnd), static_cast<std::int32_t>(end));
    SetLong(At(boss_harness::at::kHookExit), static_cast<std::int32_t>(exit));
    SetLong(At(boss_harness::at::kHookEvent), static_cast<std::int32_t>(event));
}

}  // namespace

// ============================================================================
// Kind 6 (Gary, area 24; fight 1)
// ============================================================================

// original 0x437A10: BossKind_Table[6]: jmp [BossGary_Steps + 4 * +1] (12).
extern "C" U __cdecl BossGary_Dispatch(void) { return Dispatch("BossGary_Dispatch", AddressOf(BossGary_Steps), 12, 1); }

// original 0x437A30: BossGary_Steps 0 - +0xFC BossGary_Anims, +0xF4
// BossGary_Hook, +0xF8 BossGary_Cues, +1 = 2, Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossGary_Enter(void) {
    Install(AddressOf(BossGary_Anims), at::kGaryHook, AddressOf(BossGary_Cues));
    return EnterTick();
}

// original 0x437A70: BossGary_Steps 6 - jmp [BossGary_ActSubs + 4 * +2] (6).
extern "C" U __cdecl BossGary_ActDispatch(void) { return Dispatch("BossGary_ActDispatch", AddressOf(BossGary_ActSubs), 6, 2); }

// original 0x437A90: BossGary_Steps 11 - jmp [BossGary_EndSteps + 4 * +2] (3).
extern "C" U __cdecl BossGary_EndDispatch(void) { return Dispatch("BossGary_EndDispatch", AddressOf(BossGary_EndSteps), 3, 2); }

// original 0x437AB0: BossGary_EndSteps 0 - BattleEnemy_SetAnimation(4),
// MoveCmd_TestFB(0x63, 0x14) (its answer not read), Sprite_Current (read
// after the calls) +2 up by one.
extern "C" void __cdecl BossGary_EndStart(void) {
    BH_CALL(BattleEnemy_SetAnimation)(4);
    BH_CALL(MoveCmd_TestFB)(0x63, 0x14);
    Sprite_Current[2] += 1;
}

// original 0x437AD0: BossGary_EndSteps 1 - once Mogu's count 0x904B7E is 0:
// the end step (BossGary_AnimsEnd, animation 1), 0x904AE8 |= 4 (read after
// the call), MoveCmd_TestFB(0x64, 0x14), +2 up by one. Every frame a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossGary_EndAwait(void) {
    if (Word(At(at::kEndCount)) == 0) {
        EndStep(AddressOf(BossGary_AnimsEnd), 1);
        B(at::kBattleEnd) |= 4;
        BH_CALL(MoveCmd_TestFB)(0x64, 0x14);
        Sprite_Current[2] += 1;
    }
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x437B60: Gary's +0xF4 hook - jmp [BossGary_Hooks + 4 * (word &
// 0xFF)] (3, all BareRet).
extern "C" U __cdecl BossGary_Hook(unsigned word) { return HookDispatch("BossGary_Hook", AddressOf(BossGary_Hooks), 3, word); }

// ============================================================================
// Kind 7 (Mogu, area 24; fight 1)
// ============================================================================

// original 0x437B70: BossKind_Table[7]: jmp [BossMogu_Steps + 4 * +1] (12).
extern "C" U __cdecl BossMogu_Dispatch(void) { return Dispatch("BossMogu_Dispatch", AddressOf(BossMogu_Steps), 12, 1); }

// original 0x437B90: BossMogu_Steps 0 - +0xFC BossMogu_Anims, +0xF4
// BossMogu_Hook, +0xF8 BossMogu_Cues, +1 = 2, Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossMogu_Enter(void) {
    Install(AddressOf(BossMogu_Anims), at::kMoguHook, AddressOf(BossMogu_Cues));
    return EnterTick();
}

// original 0x437BD0: BossMogu_Steps 6 - jmp [BossMogu_ActSubs + 4 * +2] (6).
extern "C" U __cdecl BossMogu_ActDispatch(void) { return Dispatch("BossMogu_ActDispatch", AddressOf(BossMogu_ActSubs), 6, 2); }

// original 0x437BF0: BossMogu_Steps 11 - jmp [BossMogu_EndSteps + 4 * +2] (3).
extern "C" U __cdecl BossMogu_EndDispatch(void) { return Dispatch("BossMogu_EndDispatch", AddressOf(BossMogu_EndSteps), 3, 2); }

// original 0x437C10: BossMogu_EndSteps 0 - BattleEnemy_SetAnimation(4), the
// count 0x904B7E = 0x3C, Sprite_Current (read after the call) +2 up by one.
extern "C" void __cdecl BossMogu_EndStart(void) {
    BH_CALL(BattleEnemy_SetAnimation)(4);
    unsigned char* const s = Sprite_Current;
    SetWord(At(at::kEndCount), 0x3C);
    s[2] += 1;
}

// original 0x437C30: BossMogu_EndSteps 1 - the count 0x904B7E down by one;
// at 0 the end step (BossMogu_AnimsEnd, animation 0) and +2 up by one. Every
// frame a tail jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossMogu_EndCount(void) {
    const unsigned count = (Word(At(at::kEndCount)) - 1u) & 0xFFFF;
    SetWord(At(at::kEndCount), count);
    if (count == 0) {
        EndStep(AddressOf(BossMogu_AnimsEnd), 0);
        Sprite_Current[2] += 1;
    }
    return BH_CALL(Sprite_ScriptTick)();
}

// original 0x437CB0: Mogu's +0xF4 hook - jmp [BossMogu_Hooks + 4 * (word &
// 0xFF)] (3, all BareRet).
extern "C" U __cdecl BossMogu_Hook(unsigned word) { return HookDispatch("BossMogu_Hook", AddressOf(BossMogu_Hooks), 3, word); }

// ============================================================================
// Fight 1 (BOSS001, area 24 row 4: kinds 6 and 7)
// ============================================================================

// original 0x437CD0: Boss_SetupTable[1] - BattleHook_End Boss01_End,
// BattleHook_Exit Boss01_Exit, BattleHook_Event Boss01_Event.
extern "C" void __cdecl Boss01_Setup(void) { SetHooks(at::kBoss01End, at::kBoss01Exit, at::kBoss01Event); }

// original 0x437CF0: fight 1's event hook, by the phase code's low byte (a
// jump table of 7 inside the function); al 0 on every path.
//   0: with 0x904AA8 bit 0x40 and no target (0x904B44 0), 0x904AAD |= 1.
//   1: with the acting actor 0 (0x904B34), action kind 4 (0x904B35) and the
//      action's id (+2 of [0x904B40]) 0x78: both enemies' words +0xA4 and
//      +0xB0 = 1 (enemies 0 and 1), the leader's +0x134 bit 1 cleared.
//   5: with 0x904AAD bit 0: the leader's action id +0x126 = 0x78, its kind
//      +0x125 = 4 and 0x904B35 = 4, its target +0x124 = 0x40 and 0x904B44 =
//      0x40.
//   6: the leader's +0x134 bit 1 set.
//   2, 3, 4 and above 6: nothing.
extern "C" unsigned char __cdecl Boss01_Event(unsigned phase) {
    switch (phase & 0xFF) {
    case 0:
        if ((B(at::kRoundFlags) & 0x40) && B(at::kTarget) == 0) B(at::kEventFlags) |= 1;
        break;
    case 1:
        if (B(at::kActor) == 0 && B(at::kActionKind) == 4 && Word(At(static_cast<U>(Long(At(at::kAction)))) + 2) == 0x78) {
            for (const U enemy : {at::kEnemies, at::kEnemy1}) {
                SetWord(At(enemy + 0xA4), 1);
                SetWord(At(enemy + 0xB0), 1);
            }
            SetLong(At(at::kLeaderFlags), static_cast<std::int32_t>(static_cast<U>(Long(At(at::kLeaderFlags))) & ~2u));
        }
        break;
    case 5:
        if (B(at::kEventFlags) & 1) {
            SetWord(At(at::kLeaderActionId), 0x78);
            B(at::kLeaderAction) = 4;
            B(at::kActionKind) = 4;
            B(at::kLeaderTarget) = 0x40;
            B(at::kTarget) = 0x40;
        }
        break;
    case 6:
        SetLong(At(at::kLeaderFlags), static_cast<std::int32_t>(static_cast<U>(Long(At(at::kLeaderFlags))) | 2u));
        break;
    default:
        break;
    }
    return 0;
}

// original 0x437DE0: fight 1's end hook. With 0x904AE8 bit 0 (the loss): the
// chapter's run 0x8034E4 = 6 and step 0x8034E5 = 0x32; otherwise the step up
// by one. Then a tail jump to 0x446E20 (the end phase, step 3) either way.
extern "C" void __cdecl Boss01_End(void) {
    if (B(at::kBattleEnd) & 1) {
        B(at::kChapterRun) = 6;
        B(at::kChapterStep) = 0x32;
    } else {
        B(at::kChapterStep) += 1;
    }
    BH_AT(Handler, at::kEndThird)();
}

// original 0x437E10: fight 1's exit hook. For the field actors tagged 6 and 7
// and enemies 0 and 1: BossActor_ClearBit40(tag), BossActor_CopyFrom(tag,
// the enemy's object, 1) (its place), Sprite_Current = BossActor_Find(tag),
// Sprite_SetAnimationBank(0x5F), the actor's +0x2A and +0x48 = 0,
// Sprite_SetAnimation(1 for tag 6, 0 for tag 7), then the actor's words
// +0x58 / +0x5A the enemy's - Sprite_Current read again at each store.
extern "C" void __cdecl Boss01_Exit(void) {
    const struct { unsigned tag; U enemy; unsigned animation; } kActors[] = {{6, at::kEnemies, 1}, {7, at::kEnemy1, 0}};
    for (const auto& a : kActors) {
        BH_CALL(BossActor_ClearBit40)(a.tag);
        BH_CALL(BossActor_CopyFrom)(a.tag, At(a.enemy), 1);
        Sprite_Current = BH_CALL(BossActor_Find)(a.tag);
        BH_CALL(Sprite_SetAnimationBank)(0x5F);
        Sprite_Current[0x2A] = 0;
        Sprite_Current[0x48] = 0;
        BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(a.animation));
        SetWord(Sprite_Current + 0x58, Word(At(a.enemy + 0x58)));
        SetWord(Sprite_Current + 0x5A, Word(At(a.enemy + 0x5A)));
    }
}

// ============================================================================
// Kind 1 (Nue, area 23)
// ============================================================================

// original 0x437EE0: BossKind_Table[1]: jmp [BossNue_Steps + 4 * +1] (12).
extern "C" U __cdecl BossNue_Dispatch(void) { return Dispatch("BossNue_Dispatch", AddressOf(BossNue_Steps), 12, 1); }

// original 0x437F00: BossNue_Steps 0 - +0xFC BossNue_Anims, +0xF4
// BossNue_Hook, +0xF8 BossNue_Cues, +1 = 2, Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossNue_Enter(void) {
    Install(AddressOf(BossNue_Anims), at::kNueHook, AddressOf(BossNue_Cues));
    return EnterTick();
}

// original 0x437F40: BossNue_Steps 9 - jmp [BossNue_EndSteps + 4 * +2] (2).
extern "C" U __cdecl BossNue_EndDispatch(void) { return Dispatch("BossNue_EndDispatch", AddressOf(BossNue_EndSteps), 2, 2); }

// original 0x437F60: BossNue_EndSteps 0 - Sprite_SetAnimation(6),
// Sprite_Current (read after the call) +2 up by one.
extern "C" void __cdecl BossNue_EndPose(void) {
    BH_CALL(Sprite_SetAnimation)(6);
    Sprite_Current[2] += 1;
}

// original 0x437F80: BossNue_EndSteps 1 - the walk (EndWalk); when it is over,
// 0x904AE8 |= 2 (the win, read after the calls) and a tail jump to
// Effect_Release.
extern "C" void __cdecl BossNue_EndMove(void) {
    if (!EndWalk()) return;
    B(at::kBattleEnd) |= 2;
    BH_CALL(Effect_Release)();
}

// original 0x437FF0: Nue's +0xF4 hook - jmp [BossNue_Hooks + 4 * (word &
// 0xFF)] (3: BossNue_HookPick, BossNue_HookHit, BareRet).
extern "C" U __cdecl BossNue_Hook(unsigned word) { return HookDispatch("BossNue_Hook", AddressOf(BossNue_Hooks), 3, word); }

// original 0x438000: BossNue_Hooks 0 (the hook's call at the action pick,
// 0x435BF5): with HP 0xFFFF, the action's kind 0x904B35 = 3, Sprite_Current
// +4 = 0 and Sound_PlayById(0x602). The word is not read.
extern "C" void __cdecl BossNue_HookPick(unsigned) {
    if (PickSpent()) BH_CALL(Sound_PlayById)(0x602);
}

// original 0x438030: BossNue_Hooks 1 and BossWeretigr_Hooks 1 (the hook's call
// at the hit, 0x4367EE): 0x939AD8's HP +0xA4 less the signed word +0x108 (the
// pending damage), when at or below a quarter of +0xB0 (the maximum, shifted
// right twice; a signed compare): 0x904AAD |= 1 and HP = 0xFFFF. The word is
// not read.
extern "C" void __cdecl BossNue_HookHit(unsigned) {
    unsigned char* const e = Enemy();
    const std::int32_t left = static_cast<std::int32_t>(Word(e + 0xA4)) - static_cast<std::int16_t>(Word(e + 0x108));
    const std::int32_t quarter = static_cast<std::int32_t>(Word(e + 0xB0) >> 2);
    if (left > quarter) return;
    B(at::kEventFlags) |= 1;
    SetWord(e + 0xA4, 0xFFFF);
}

// ============================================================================
// Kind 2 (Nue, area 22)
// ============================================================================

// original 0x438070: BossKind_Table[2]: jmp [BossNue2_Steps + 4 * +1] (12).
extern "C" U __cdecl BossNue2_Dispatch(void) { return Dispatch("BossNue2_Dispatch", AddressOf(BossNue2_Steps), 12, 1); }

// original 0x438090: BossNue2_Steps 0 - +0xFC BossNue_Anims, +0xF4
// BossNue2_Hook, +0xF8 BossNue_Cues (kind 1's two tables), +1 = 2,
// Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossNue2_Enter(void) {
    Install(AddressOf(BossNue_Anims), at::kNue2Hook, AddressOf(BossNue_Cues));
    return EnterTick();
}

// original 0x4380D0: BossNue2_Steps 6 - jmp [BossNue2_ActSubs + 4 * +2] (6).
extern "C" U __cdecl BossNue2_ActDispatch(void) { return Dispatch("BossNue2_ActDispatch", AddressOf(BossNue2_ActSubs), 6, 2); }

// original 0x4380F0: kind 2's +0xF4 hook - jmp [BossNue2_Hooks + 4 * (word &
// 0xFF)] (3, all BareRet).
extern "C" U __cdecl BossNue2_Hook(unsigned word) { return HookDispatch("BossNue2_Hook", AddressOf(BossNue2_Hooks), 3, word); }

// ============================================================================
// Kind 46 (Sample 1, area 158)
// ============================================================================

// original 0x438100: BossKind_Table[46]: jmp [BossSample1_Steps + 4 * +1] (12).
extern "C" U __cdecl BossSample1_Dispatch(void) {
    return Dispatch("BossSample1_Dispatch", AddressOf(BossSample1_Steps), 12, 1);
}

// original 0x438120: BossSample1_Steps 0 - +0xFC BossNue_Anims, +0xF4
// BossSample1_Hook, +0xF8 BossSample1_Cues, then 0x939AD8's +0x114 |= 8,
// +1 = 2, Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossSample1_Enter(void) {
    Install(AddressOf(BossNue_Anims), at::kSample1Hook, AddressOf(BossSample1_Cues));
    unsigned char* const e = Enemy();
    SetLong(e + 0x114, static_cast<std::int32_t>(static_cast<U>(Long(e + 0x114)) | 8u));
    return EnterTick();
}

// original 0x438180: kind 46's +0xF4 hook - jmp [BossSample1_Hooks + 4 *
// (word & 0xFF)] (3, all BareRet).
extern "C" U __cdecl BossSample1_Hook(unsigned word) {
    return HookDispatch("BossSample1_Hook", AddressOf(BossSample1_Hooks), 3, word);
}

// ============================================================================
// Fights 2, 3 and 39 (BOSS002, row 7 of areas 22, 23 and 158)
// ============================================================================

// original 0x438190: Boss_SetupTable[2] - BattleHook_End Boss02_End,
// BattleHook_Exit BareRet, BattleHook_Event Boss02_Event.
extern "C" void __cdecl Boss02_Setup(void) { SetHooks(at::kBoss02End, at::kBareRet, at::kBoss02Event); }

// original 0x4381B0: fight 2's event hook: at phase code 0 (its low byte)
// with 0x904AE8 bit 1 (the win), the window records' pass 0x802D20 = 2 and
// Msg_OpenScript(0x19). al 0.
extern "C" unsigned char __cdecl Boss02_Event(unsigned phase) {
    if ((phase & 0xFF) == 0 && (B(at::kBattleEnd) & 2)) {
        B(at::kWindowPass) = 2;
        BH_CALL(Msg_OpenScript)(0x19);
    }
    return 0;
}

// original 0x4381E0: fight 2's end hook: with 0x904AE8 bit 1 or 2,
// BossActor_Clear(0), counter 0x24, track 0x1B, step 1; else step 2.
extern "C" void __cdecl Boss02_End(void) {
    EndByBits([] { BH_CALL(BossActor_Clear)(0); }, 0x24, 0x1B);
}

// original 0x438210: Boss_SetupTable[3] - BattleHook_End Boss03_End,
// BattleHook_Exit BossHook_ExitActor0Bit40, BattleHook_Event BareRetZero.
extern "C" void __cdecl Boss03_Setup(void) { SetHooks(at::kBoss03End, at::kExitActor0Bit40, at::kBareRetZero); }

// original 0x438230: fight 3's end hook: with 0x904AE8 bit 1 or 2,
// BossActor_CopyFrom(0, enemy 0's object, 0) (its pose), counter 0x6D, track
// 0x18, step 1; else step 2.
extern "C" void __cdecl Boss03_End(void) {
    EndByBits([] { BH_CALL(BossActor_CopyFrom)(0, At(at::kEnemies), 0); }, 0x6D, 0x18);
}

// original 0x438270: Boss_SetupTable[39] - BattleHook_End
// BossHook_EndPickWay, BattleHook_Exit BareRet, BattleHook_Event BareRetZero.
extern "C" void __cdecl Boss39_Setup(void) { SetHooks(at::kEndPickWay, at::kBareRet, at::kBareRetZero); }

// ============================================================================
// Kind 39 (Weretigr, area 35; fight 33)
// ============================================================================

// original 0x43D3D0: BossKind_Table[39]: jmp [BossWeretigr_Steps + 4 * +1] (12).
extern "C" U __cdecl BossWeretigr_Dispatch(void) {
    return Dispatch("BossWeretigr_Dispatch", AddressOf(BossWeretigr_Steps), 12, 1);
}

// original 0x43D3F0: BossWeretigr_Steps 0 - +0xFC BossWeretigr_Anims, +0xF4
// BossWeretigr_Hook, +0xF8 BossWeretigr_Cues, +1 = 2, Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossWeretigr_Enter(void) {
    Install(AddressOf(BossWeretigr_Anims), at::kWeretigrHook, AddressOf(BossWeretigr_Cues));
    return EnterTick();
}

// original 0x43D430: BossWeretigr_Steps 4 - jmp [BossWeretigr_State4Steps + 4
// * +2] (5).
extern "C" U __cdecl BossWeretigr_State4Dispatch(void) {
    return Dispatch("BossWeretigr_State4Dispatch", AddressOf(BossWeretigr_State4Steps), 5, 2);
}

// original 0x43D450: BossWeretigr_State4Steps 0 - a slot n = BattleTask_Create(3,
// 6) (its al: the kind-3 dispatcher BattleBossFx_Dispatch's slot 6), and in
// it: the first 0x80 bytes of the acting actor's enemy object (0x93B960 +
// (0x904B34 - 3) * 0x128) copied a dword at a time (rep movsd), the kind +6 =
// 3, +5 = 6, the state bytes +1..+4 = 0, +0xB = n, the owner +0x80 =
// Sprite_Current. Then Sprite_Current +9 = n, +0xA = 0x10, +0 |= 0x40, +2 up
// by one.
extern "C" void __cdecl BossWeretigr_State4Fx(void) {
    const unsigned n = BH_CALL(BattleTask_Create)(3, 6) & 0xFF;
    if (n >= 48)
        bof3::Fatal("BossWeretigr_State4Fx: BattleTask_Create answered slot %u (all 48 taken) - the original copies into "
                    "0x93A000 + %u * 0x84, past the pool (docs/boss_sa.md section 6)",
                    n, n);
    unsigned char* const task = At(at::kTaskSlots + n * at::kTaskStride);
    const U from = at::kEnemies + static_cast<U>(static_cast<std::int32_t>(B(at::kActor)) - 3) * at::kEnemyStride;
    for (U i = 0; i < 0x80; i += 4) SetLong(task + i, Long(At(from + i)));
    task[6] = 3;
    task[5] = 6;
    task[1] = 0;
    task[2] = 0;
    task[3] = 0;
    task[4] = 0;
    unsigned char* const s = Sprite_Current;
    task[0xB] = static_cast<unsigned char>(n);
    SetLong(task + 0x80, static_cast<std::int32_t>(Key(s)));
    s[9] = static_cast<unsigned char>(n);
    Sprite_Current[0xA] = 0x10;
    Sprite_Current[0] |= 0x40;
    Sprite_Current[2] += 1;
}

// original 0x43D500: BossWeretigr_State4Steps 1 - +0xA down by one; at 0 the
// cue at 0x939AD8's +0xF8 (its first word) and that word + 4 through 0x437450
// (0x939AD8 and its +0xF8 read again for the second), +2 up by one.
extern "C" void __cdecl BossWeretigr_State4Cue(void) {
    Sprite_Current[0xA] -= 1;
    if (Sprite_Current[0xA] != 0) return;
    using Cue = void (__cdecl*)(unsigned);
    BH_AT(Cue, at::kPlayCue)(Word(At(static_cast<U>(Long(Enemy() + 0xF8)))));
    BH_AT(Cue, at::kPlayCue)((Word(At(static_cast<U>(Long(Enemy() + 0xF8)))) + 4u) & 0xFFFF);
    Sprite_Current[2] += 1;
}

// original 0x43D560: BossWeretigr_State4Steps 2 - once Sprite_Current +0 has
// lost bit 0x40 (the effect task clears it, by the step before's set):
// 0x904AA8 |= 4, 0x4376F0 (a chance of 0x904AA8 bit 7 and a task), and a tail
// jump to 0x4376A0 (the turn closed: +1 = 2, +2 = 0).
extern "C" void __cdecl BossWeretigr_State4End(void) {
    if (Sprite_Current[0] & 0x40) return;
    B(at::kRoundFlags) |= 4;
    BH_AT(Handler, at::kTurnChance)();
    BH_AT(Handler, at::kTurnClose)();
}

// original 0x43D580: BossWeretigr_State4Steps 3 - Sprite_SetAnimation(0),
// Sprite_Current (read after the call) +2 up by one.
extern "C" void __cdecl BossWeretigr_EndPose(void) {
    BH_CALL(Sprite_SetAnimation)(0);
    Sprite_Current[2] += 1;
}

// original 0x43D5A0: BossWeretigr_State4Steps 4 - the walk (EndWalk); when it
// is over, 0x904AE8 |= 2 (the win), the experience 0x904AEC += 0x939AD8's
// word +0x96 (both read after the calls), and a tail jump to Effect_Release.
extern "C" void __cdecl BossWeretigr_EndMove(void) {
    if (!EndWalk()) return;
    const unsigned exp = Word(Enemy() + 0x96);
    B(at::kBattleEnd) |= 2;
    AddLong(At(at::kExp), exp);
    BH_CALL(Effect_Release)();
}

// original 0x43D630: Weretigr's +0xF4 hook - jmp [BossWeretigr_Hooks + 4 *
// (word & 0xFF)] (3: BossWeretigr_HookPick, BossNue_HookHit, BareRet).
extern "C" U __cdecl BossWeretigr_Hook(unsigned word) {
    return HookDispatch("BossWeretigr_Hook", AddressOf(BossWeretigr_Hooks), 3, word);
}

// original 0x43D640: BossWeretigr_Hooks 0 (the action pick): with HP 0xFFFF,
// 0x904B35 = 3 and Sprite_Current +4 = 0 (BossNue_HookPick without its cue).
extern "C" void __cdecl BossWeretigr_HookPick(unsigned) { PickSpent(); }

void BossSa_Inject() {
    if (bof3::WantsShadow("boss_sa")) boss_sa::SelfTest();
    BOF3_INJECT(BossGary_Dispatch);
    BOF3_INJECT(BossGary_Enter);
    BOF3_INJECT(BossGary_ActDispatch);
    BOF3_INJECT(BossGary_EndDispatch);
    BOF3_INJECT(BossGary_EndStart);
    BOF3_INJECT(BossGary_EndAwait);
    BOF3_INJECT(BossGary_Hook);
    BOF3_INJECT(BossMogu_Dispatch);
    BOF3_INJECT(BossMogu_Enter);
    BOF3_INJECT(BossMogu_ActDispatch);
    BOF3_INJECT(BossMogu_EndDispatch);
    BOF3_INJECT(BossMogu_EndStart);
    BOF3_INJECT(BossMogu_EndCount);
    BOF3_INJECT(BossMogu_Hook);
    BOF3_INJECT(Boss01_Setup);
    BOF3_INJECT(Boss01_Event);
    BOF3_INJECT(Boss01_End);
    BOF3_INJECT(Boss01_Exit);
    BOF3_INJECT(BossNue_Dispatch);
    BOF3_INJECT(BossNue_Enter);
    BOF3_INJECT(BossNue_EndDispatch);
    BOF3_INJECT(BossNue_EndPose);
    BOF3_INJECT(BossNue_EndMove);
    BOF3_INJECT(BossNue_Hook);
    BOF3_INJECT(BossNue_HookPick);
    BOF3_INJECT(BossNue_HookHit);
    BOF3_INJECT(BossNue2_Dispatch);
    BOF3_INJECT(BossNue2_Enter);
    BOF3_INJECT(BossNue2_ActDispatch);
    BOF3_INJECT(BossNue2_Hook);
    BOF3_INJECT(BossSample1_Dispatch);
    BOF3_INJECT(BossSample1_Enter);
    BOF3_INJECT(BossSample1_Hook);
    BOF3_INJECT(Boss02_Setup);
    BOF3_INJECT(Boss02_Event);
    BOF3_INJECT(Boss02_End);
    BOF3_INJECT(Boss03_Setup);
    BOF3_INJECT(Boss03_End);
    BOF3_INJECT(Boss39_Setup);
    BOF3_INJECT(BossWeretigr_Dispatch);
    BOF3_INJECT(BossWeretigr_Enter);
    BOF3_INJECT(BossWeretigr_State4Dispatch);
    BOF3_INJECT(BossWeretigr_State4Fx);
    BOF3_INJECT(BossWeretigr_State4Cue);
    BOF3_INJECT(BossWeretigr_State4End);
    BOF3_INJECT(BossWeretigr_EndPose);
    BOF3_INJECT(BossWeretigr_EndMove);
    BOF3_INJECT(BossWeretigr_Hook);
    BOF3_INJECT(BossWeretigr_HookPick);
}
