// Group BSG of the boss round: fights 29, 31, 32 and 33, kinds 34..38 and 40,
// and the kind-3 effect task F6 - 53 functions of 0x43CDE0..0x43E535
// (tools/boss_rows.py, 2026-09-28), each read to its last instruction with
// capstone (2026-09-28) and taken through the boss harness (boss_harness.h).
// Round eleven, wave two; docs/boss_sg.md has them one row each.
//
// The names are the disc's (tools/boss_rows.py --disc, the US disc's area
// records), the fights the tool's rows:
//
//   kind 34     Dolphin (area 175)          set-up 29  BOSS029, area 175 row 7
//   kinds 35, 36  Gisshan, Charyb / Scylla (area 103; set-up 30 is BSE's)
//   kind 37     Garr, the second fight (area 85)   set-up 31  BOSS031, area 85 row 7
//   kind 38     D>Zombie (area 108)         set-up 32  BOSS032, area 108 row 7
//   set-up 33   BOSS033, area 35 row 7 (kind 39, Weretigr, is BSA's)
//   F6          BattleBossFx_Dispatch's slot 6: Weretigr's turn, the task
//               BossWeretigr_State4Fx (BSA's) creates (BattleTask_Create(3, 6))
//   kind 40     Mikba (area 43; set-ups 34 and 41 are BSH's)
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers and hook tables abort past their tables where the original
// jumps through whatever follows (the owner's rule for an unchecked index,
// round9 doc section 6; nothing reaches it). Every call goes through the
// harness (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in for
// the callees; a hook or table a function stores is the same literal address.
#include "game/boss_sg.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/boss_sg_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = boss_sg::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Enemy() { return At(static_cast<U>(Long(At(at::kCurrentEnemy)))); }
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
U L(const unsigned char* p) { return static_cast<U>(Long(p)); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// A table entry as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll and
// BattleTask_RunAll push nothing; an entry that reads it - Port_DroppedCall's
// byte, a hook's word - reads that word), its eax the dispatcher's.
using Forward = U (__cdecl*)(unsigned);
Forward Entry(U table, unsigned index) {
    return reinterpret_cast<Forward>(static_cast<std::uintptr_t>(L(At(table + 4 * index))));   // as read: the fuzz swaps the cells
}

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, a
// Fatal past them (the original jumps through the dword after).
U Dispatch(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sg.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return Entry(table, state)(passed);
}

// An enemy's +0xF4 hook: mov eax, [esp + 4]; and eax, 0xFF; jmp [table + 4 *
// eax] - three entries (the words 0, 1, 2 the engine passes), a Fatal past
// them; the entry gets the caller's word whole.
U HookTable(const char* who, U table, unsigned word) {
    const unsigned i = word & 0xFF;
    if (i >= 3)
        bof3::Fatal("%s(0x%X): past the 3 entries of 0x%X - the original jumps through the dword after "
                    "(docs/boss_sg.md section 6)",
                    who, word, (unsigned)table);
    return Entry(table, i)(word);
}

// call [table + 4 * Sprite_Current +2] (no argument), then, the Sprite_Current
// of after the call with +0 not 0, a tail jump to Sprite_UpdateScreen: the
// effect task's two step dispatchers.
void TaskSteps(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: state byte +2 is %u, past the %u entries of 0x%X - the original calls through the dword after "
                    "(docs/boss_sg.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(At(table + 4 * state))))();
    if (Sprite_Current[0] != 0) BH_CALL(Sprite_UpdateScreen)();
}

// The three set-up stores (BattleHook_End / _Exit / _Event).
void StoreHooks(U end, U exit, U event) {
    SetLong(At(at::kHookEnd), S(end));
    SetLong(At(at::kHookExit), S(exit));
    SetLong(At(at::kHookEvent), S(event));
}

// A kind's entry: 0x939AD8's +0xFC (its animation bytes), +0xF4 (its hook)
// and +0xF8 (its sound words), 0x939AD8 read for each store.
void StoreKind(U fc, U f4, U f8) {
    SetLong(Enemy() + 0xFC, S(fc));
    SetLong(Enemy() + 0xF4, S(f4));
    SetLong(Enemy() + 0xF8, S(f8));
}

// The entry's end: Sprite_Current +1 = 2 (the idle step) and a tail jump to
// Sprite_ScriptTick, whose al is the answer.
unsigned char Idle() {
    Sprite_Current[1] = 2;
    return BH_CALL(Sprite_ScriptTick)();
}

// Kinds 37 and 38's death's tail, after Battle_EnemyDefeated: 0x939AD8's
// +0x110 |= 0x1000, then the Sprite_Current of after the calls +0 &= 0xBF, +1
// = 3, +2 = 0, +3 = 0.
void Fallen() {
    unsigned char* const e = Enemy();
    SetLong(e + 0x110, S(L(e + 0x110) | 0x1000u));
    unsigned char* const s = Sprite_Current;
    s[0] &= 0xBF;
    s[1] = 3;
    s[2] = 0;
    s[3] = 0;
}

// The exit hooks of set-ups 31 and 32: BossActor_ClearBit40(0), the actor
// BossActor_Find(0) made Field_ActiveMember and Sprite_Current,
// Sprite_SetAnimation(animation).
void ActorZero(unsigned animation) {
    BH_CALL(BossActor_ClearBit40)(0);
    unsigned char* const actor = BH_CALL(BossActor_Find)(0);
    Field_ActiveMember = actor;
    Sprite_Current = actor;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
}

// An enemy object by a battle index (3..10 an enemy), as the originals index
// it: 0x93B960 + (index - 3) * 0x128, wrapping - unchecked.
unsigned char* EnemyByIndex(unsigned index) { return At(at::kEnemies + (index - 3u) * at::kEnemyStride); }

}  // namespace

// ===========================================================================
// Kind 34 (Dolphin, area 175) and set-up 29 (BOSS029, area 175 row 7)
// ===========================================================================

// original 0x43CDE0: BossKind_Table[34]: jmp [BossDolphin_States + 4 * +1] (12).
extern "C" U __cdecl BossDolphin_Dispatch(unsigned passed) {
    return Dispatch("BossDolphin_Dispatch", AddressOf(BossDolphin_States), 12, 1, passed);
}

// original 0x43CE00: state 0. 0x939AD8's +0xFC = BossDolphin_Anims, +0xF4 =
// BossDolphin_Hook, +0xF8 = BossDolphin_Sounds; Sprite_Current +1 = 2; a tail
// jump to Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossDolphin_Enter(void) {
    StoreKind(AddressOf(BossDolphin_Anims), bof3::addr::BossDolphin_Hook, AddressOf(BossDolphin_Sounds));
    return Idle();
}

// original 0x43CE40: the +0xF4 hook through BossDolphin_Hooks (3, all BareRet).
extern "C" U __cdecl BossDolphin_Hook(unsigned word) { return HookTable("BossDolphin_Hook", AddressOf(BossDolphin_Hooks), word); }

// original 0x43CE50: Boss_SetupTable[29]. The hooks: end Boss29_End, exit
// BossHook_ExitClearActor0, event BareRetZero.
extern "C" void __cdecl Boss29_Setup(void) {
    StoreHooks(bof3::addr::Boss29_End, bof3::addr::BossHook_ExitClearActor0, bof3::addr::BareRetZero);
}

// original 0x43CE70: the end hook. Won (0x904AE8 bit 1): the chapter's step
// 0x8034E5 = 0x1C, 0x446DE0 (the end phase, step 1) called, then Music_Track =
// 0x62. Otherwise a tail jump to 0x446E00 (step 2).
extern "C" void __cdecl Boss29_End(void) {
    if ((B(at::kBattleEnd) & 2) == 0) {
        BH_AT(Handler, at::kEndOther)();
        return;
    }
    B(at::kChapterStep) = 0x1C;
    BH_AT(Handler, at::kEndWin)();
    Music_Track = 0x62;
}

// ===========================================================================
// Kinds 35 (Gisshan) and 36 (Charyb / Scylla), area 103 - set-up 30 is BSE's
// ===========================================================================

// original 0x43CEA0: BossKind_Table[35]: jmp [BossGisshan_States + 4 * +1] (12).
extern "C" U __cdecl BossGisshan_Dispatch(unsigned passed) {
    return Dispatch("BossGisshan_Dispatch", AddressOf(BossGisshan_States), 12, 1, passed);
}

// original 0x43CEC0: state 0: BossGisshan_Anims, BossGisshan_Hook,
// BossGisshan_Sounds; +1 = 2; tail Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossGisshan_Enter(void) {
    StoreKind(AddressOf(BossGisshan_Anims), bof3::addr::BossGisshan_Hook, AddressOf(BossGisshan_Sounds));
    return Idle();
}

// original 0x43CF00: the +0xF4 hook through BossGisshan_Hooks (3, all BareRet).
extern "C" U __cdecl BossGisshan_Hook(unsigned word) { return HookTable("BossGisshan_Hook", AddressOf(BossGisshan_Hooks), word); }

// original 0x43CF10: BossKind_Table[36]: jmp [BossScylla_States + 4 * +1] (12).
extern "C" U __cdecl BossScylla_Dispatch(unsigned passed) {
    return Dispatch("BossScylla_Dispatch", AddressOf(BossScylla_States), 12, 1, passed);
}

// original 0x43CF30: state 0: BossScylla_Anims, BossScylla_Hook,
// BossScylla_Sounds (the two byte tables lie in kind 35's header); +1 = 2;
// tail Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossScylla_Enter(void) {
    StoreKind(AddressOf(BossScylla_Anims), bof3::addr::BossScylla_Hook, AddressOf(BossScylla_Sounds));
    return Idle();
}

// original 0x43CF70: the +0xF4 hook through BossScylla_Hooks (3, all BareRet).
extern "C" U __cdecl BossScylla_Hook(unsigned word) { return HookTable("BossScylla_Hook", AddressOf(BossScylla_Hooks), word); }

// ===========================================================================
// Kind 37 (Garr, area 85) and set-up 31 (BOSS031, area 85 row 7)
// ===========================================================================

// original 0x43CFD0: BossKind_Table[37]: jmp [BossGarr2_States + 4 * +1] (12).
extern "C" U __cdecl BossGarr2_Dispatch(unsigned passed) {
    return Dispatch("BossGarr2_Dispatch", AddressOf(BossGarr2_States), 12, 1, passed);
}

// original 0x43CFF0: state 0: BossGarr2_Anims, BossGarr2_Hook,
// BossGarr2_Sounds; +1 = 2; tail Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossGarr2_Enter(void) {
    StoreKind(AddressOf(BossGarr2_Anims), bof3::addr::BossGarr2_Hook, AddressOf(BossGarr2_Sounds));
    return Idle();
}

// original 0x43D030: state 6: jmp [BossGarr2_ActSubs + 4 * +2] (6: the generic
// action entries with the kind's death at 4).
extern "C" U __cdecl BossGarr2_ActDispatch(unsigned passed) {
    return Dispatch("BossGarr2_ActDispatch", AddressOf(BossGarr2_ActSubs), 6, 2, passed);
}

// original 0x43D050: +2 entry 4, the death: Sprite_EnsureAnimation(1), the
// Sprite_Current of after the call +0x2A = 1, Sprite_ScriptTickOnce (its answer
// not read), Battle_EnemyDefeated, then Fallen.
extern "C" void __cdecl BossGarr2_Death(void) {
    BH_CALL(Sprite_EnsureAnimation)(1);
    Sprite_Current[0x2A] = 1;
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    Fallen();
}

// original 0x43D0B0: the +0xF4 hook through BossGarr2_Hooks (3, all BareRet).
extern "C" U __cdecl BossGarr2_Hook(unsigned word) { return HookTable("BossGarr2_Hook", AddressOf(BossGarr2_Hooks), word); }

// original 0x43D0C0: Boss_SetupTable[31]. The hooks: end Boss31_End, exit
// Boss31_Exit, event BareRetZero.
extern "C" void __cdecl Boss31_Setup(void) {
    StoreHooks(bof3::addr::Boss31_End, bof3::addr::Boss31_Exit, bof3::addr::BareRetZero);
}

// original 0x43D0E0: the end hook. Won (0x904AE8 bit 1): the chapter's step =
// 0x14 and a tail jump to 0x446DE0 (step 1). Otherwise 0x904AE5 &= 0xBF (the
// battle's music not kept) and a tail jump to 0x446E00 (step 2).
extern "C" void __cdecl Boss31_End(void) {
    if (B(at::kBattleEnd) & 2) {
        B(at::kChapterStep) = 0x14;
        BH_AT(Handler, at::kEndWin)();
        return;
    }
    B(at::kMusicFlags) &= 0xBF;
    BH_AT(Handler, at::kEndOther)();
}

// original 0x43D110: the exit hook: ActorZero(1), then the Sprite_Current of
// after the call +0x2A = 1.
extern "C" void __cdecl Boss31_Exit(void) {
    ActorZero(1);
    Sprite_Current[0x2A] = 1;
}

// ===========================================================================
// Kind 38 (D>Zombie, area 108) and set-up 32 (BOSS032, area 108 row 7)
// ===========================================================================

// original 0x43D140: BossKind_Table[38]: jmp [BossDZombie_States + 4 * +1] (12).
extern "C" U __cdecl BossDZombie_Dispatch(unsigned passed) {
    return Dispatch("BossDZombie_Dispatch", AddressOf(BossDZombie_States), 12, 1, passed);
}

// original 0x43D160: state 0: BossDZombie_Anims, BossDZombie_Hook,
// BossDZombie_Sounds, then 0x939AD8's +0x114 |= 8; +1 = 2; tail
// Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossDZombie_Enter(void) {
    StoreKind(AddressOf(BossDZombie_Anims), bof3::addr::BossDZombie_Hook, AddressOf(BossDZombie_Sounds));
    unsigned char* const e = Enemy();
    SetLong(e + 0x114, S(L(e + 0x114) | 8u));
    return Idle();
}

// original 0x43D1C0: state 6: jmp [BossDZombie_ActSubs + 4 * +2] (6).
extern "C" U __cdecl BossDZombie_ActDispatch(unsigned passed) {
    return Dispatch("BossDZombie_ActDispatch", AddressOf(BossDZombie_ActSubs), 6, 2, passed);
}

// original 0x43D1E0: +2 entry 4, the death: Sprite_EnsureAnimation(0),
// Sprite_ScriptTickOnce (answer not read), Battle_EnemyDefeated, then Fallen.
extern "C" void __cdecl BossDZombie_Death(void) {
    BH_CALL(Sprite_EnsureAnimation)(0);
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Battle_EnemyDefeated)();
    Fallen();
}

// original 0x43D240: the +0xF4 hook through BossDZombie_Hooks (3, all BareRet).
extern "C" U __cdecl BossDZombie_Hook(unsigned word) { return HookTable("BossDZombie_Hook", AddressOf(BossDZombie_Hooks), word); }

// original 0x43D250: Boss_SetupTable[32]. The hooks: end Boss32_End, exit
// Boss32_Exit, event BareRetZero.
extern "C" void __cdecl Boss32_Setup(void) {
    StoreHooks(bof3::addr::Boss32_End, bof3::addr::Boss32_Exit, bof3::addr::BareRetZero);
}

// original 0x43D270: the end hook. Won (0x904AE8 bit 1): for each party member
// 0..2 (ObjTrio) with +0 bit 0 - Battle_RemoveFromTurnOrder(its +5), its +0x90
// read after the call, Sprite_Current = the member, Sprite_PoseFromSet(its +8
// + 0x1C with +0x90 bit 14, else + 4 - a byte, 0x8C5D80, 0x1800); then
// 0x904AE8 read, Music_Track = 0x6B, the chapter's step = 0xE, 0x904AE8 = the
// value read | 8, a tail jump to 0x446DE0 (step 1). Otherwise a tail jump to
// 0x446E00 (step 2).
extern "C" void __cdecl Boss32_End(void) {
    if ((B(at::kBattleEnd) & 2) == 0) {
        BH_AT(Handler, at::kEndOther)();
        return;
    }
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = At(at::kParty + m * at::kPartyStride);
        if ((p[0] & 1) == 0) continue;
        BH_CALL(Battle_RemoveFromTurnOrder)(p[5]);
        const U flags = L(p + 0x90);
        Sprite_Current = p;
        const unsigned pose = static_cast<unsigned char>(p[8] + ((flags & 0x4000) != 0 ? 0x1C : 4));
        BH_CALL(Sprite_PoseFromSet)(pose, At(at::kPoseSet), 0x1800);
    }
    const unsigned char end = B(at::kBattleEnd);
    Music_Track = 0x6B;
    B(at::kChapterStep) = 0xE;
    B(at::kBattleEnd) = static_cast<unsigned char>(end | 8);
    BH_AT(Handler, at::kEndWin)();
}

// original 0x43D3A0: the exit hook: ActorZero(0).
extern "C" void __cdecl Boss32_Exit(void) { ActorZero(0); }

// ===========================================================================
// Set-up 33 (BOSS033, area 35 row 7; kind 39, Weretigr, is BSA's)
// ===========================================================================

// original 0x43D670: Boss_SetupTable[33]. The hooks: end Boss33_End, exit
// BareRet, event BareRetZero.
extern "C" void __cdecl Boss33_Setup(void) { StoreHooks(bof3::addr::Boss33_End, bof3::addr::BareRet, bof3::addr::BareRetZero); }

// original 0x43D690: the end hook. With neither 0x904AE8 bit 1 nor bit 2: a
// tail jump to 0x446E00 (step 2). Otherwise BossActor_Clear(0); after it,
// Field_Kind2X = party member 0's +0x34 and Field_Kind2Z = its +0x38 (the
// dwords); the chapter's step = 8; a tail jump to 0x446DE0 (step 1).
extern "C" void __cdecl Boss33_End(void) {
    if ((B(at::kBattleEnd) & 6) == 0) {
        BH_AT(Handler, at::kEndOther)();
        return;
    }
    BH_CALL(BossActor_Clear)(0);
    const std::int32_t x = Long(At(at::kMember0X));
    const std::int32_t z = Long(At(at::kMember0Z));
    Field_Kind2X = x;
    Field_Kind2Z = z;
    B(at::kChapterStep) = 8;
    BH_AT(Handler, at::kEndWin)();
}

// ===========================================================================
// F6: BattleBossFx_Dispatch's slot 6 - Weretigr's turn (a copy of the acting
// enemy's object made a task by BossWeretigr_State4Fx, group BSA's) and its
// trail (copies of the task made by its strike). Sprite_Current is the task's
// slot throughout.
// ===========================================================================

// original 0x43D6D0: the task: jmp [BossWeretigrFx_Steps + 4 * +1] (2: the
// turn, the trail).
extern "C" U __cdecl BossWeretigrFx_Task(unsigned passed) {
    return Dispatch("BossWeretigrFx_Task", AddressOf(BossWeretigrFx_Steps), 2, 1, passed);
}

// original 0x43D6F0: +1 = 0, the turn: TaskSteps through
// BossWeretigrFx_MainSteps (7).
extern "C" void __cdecl BossWeretigrFx_Main(void) { TaskSteps("BossWeretigrFx_Main", AddressOf(BossWeretigrFx_MainSteps), 7); }

// original 0x43D720: step 0. The height to reach +0x20 = +0x3C + 0x4000000;
// Sprite_SetAnimation(2); the Sprite_Current of after the call +0xB = 0x10
// and +2 up by one.
extern "C" void __cdecl BossWeretigrFx_Begin(void) {
    unsigned char* const s = Sprite_Current;
    SetLong(s + 0x20, S(L(s + 0x3C) + 0x4000000u));
    BH_CALL(Sprite_SetAnimation)(2);
    Sprite_Current[0xB] = 0x10;
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

// original 0x43D750: step 1. When Sprite_ScriptTickOnce answers al not 0:
// Sprite_SetAnimation(3) and the Sprite_Current of after +2 up by one.
extern "C" void __cdecl BossWeretigrFx_AwaitPose(void) {
    if ((BH_CALL(Sprite_ScriptTickOnce)() & 0xFF) == 0) return;
    BH_CALL(Sprite_SetAnimation)(3);
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

// original 0x43D770: step 2. While the s32 +0x3C is below +0x20: +0x3C +=
// 0x1000000. Then, once: the facing f = 0x904AAC & 0xFF - with the target
// 0x904B44 a member (below 3) +8 ^= 2 and f ^= 2 -; +0x34 = Field_Kind2X + the
// signed byte at 0x64E4F4 + 2 f, +0x38 = Field_Kind2Z + the one after it;
// Sprite_SetAnimation(4); the Sprite_Current of after +2 up by one. Both ways a
// tail jump to Sprite_ScriptTickOnce, whose al is the answer.
extern "C" unsigned char __cdecl BossWeretigrFx_Rise(void) {
    unsigned char* const s = Sprite_Current;
    const U y = L(s + 0x3C);
    if (S(y) < Long(s + 0x20)) {
        SetLong(s + 0x3C, S(y + 0x1000000u));
        return BH_CALL(Sprite_ScriptTickOnce)();
    }
    U f;
    if (B(at::kTarget) >= 3) {
        f = L(At(at::kFacing)) & 0xFF;
        SetLong(s + 0x34, S(static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kFacingOffsets + 2 * f)))) +
                            static_cast<U>(Field_Kind2X)));
        f = L(At(at::kFacing)) & 0xFF;
    } else {
        s[8] ^= 2;
        f = (L(At(at::kFacing)) & 0xFF) ^ 2;
        SetLong(s + 0x34, S(static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kFacingOffsets + 2 * f)))) +
                            static_cast<U>(Field_Kind2X)));
        f = (L(At(at::kFacing)) & 0xFF) ^ 2;
    }
    SetLong(s + 0x38, S(static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kFacingOffsets + 2 * f + 1)))) +
                        static_cast<U>(Field_Kind2Z)));
    BH_CALL(Sprite_SetAnimation)(4);
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x43D830: step 3. A trail copy: slot n = BattleTask_Create(3, 6);
// unless 0xFF - the task's first 0x80 bytes copied into slot n (rep movsd),
// then its +0 &= 0xBF, kind +6 = 3, +5 = 6, +1 = 1, +2..+4 = 0, owner +0x80 =
// the task, +0xB = the task's +0xB, and the task's +0xB down by one unless 0.
// Then toward the target 0x904B44 (a member 0..2 - ObjTrio - else an enemy
// object, 0x93B960 + (target - 3) * 0x128): MagicFx_StepToward(it, 0x50); the
// target read again for MagicFx_NearSprite3D(it, 0x10000) - near:
// Sprite_SetAnimation(5), Battle_SetTargetFlag40(the target read again), the
// Sprite_Current of after +2 up by one. Last, Sprite_ScriptTickOnce, whose al
// is the answer.
extern "C" unsigned char __cdecl BossWeretigrFx_Strike(void) {
    const unsigned n = BH_CALL(BattleTask_Create)(3, 6) & 0xFF;
    if (n != 0xFF) {
        unsigned char* const s = Sprite_Current;
        unsigned char* const t = At(at::kTasks + n * at::kTaskStride);
        std::memmove(t, s, 0x80);
        t[0] &= 0xBF;
        t[6] = 3;
        t[5] = 6;
        t[1] = 1;
        t[2] = 0;
        t[3] = 0;
        t[4] = 0;
        SetLong(t + 0x80, S(static_cast<U>(reinterpret_cast<std::uintptr_t>(s))));
        t[0xB] = s[0xB];
        if (s[0xB] != 0) s[0xB] = static_cast<unsigned char>(s[0xB] - 1);
    }
    const unsigned target = B(at::kTarget);
    bool near;
    if (target <= 2) {
        BH_CALL(MagicFx_StepToward)(At(at::kParty + target * at::kPartyStride), 0x50);
        near = BH_CALL(MagicFx_NearSprite3D)(At(at::kParty + B(at::kTarget) * at::kPartyStride), 0x10000) != 0;
    } else {
        BH_CALL(MagicFx_StepToward)(EnemyByIndex(target), 0x50);
        near = BH_CALL(MagicFx_NearSprite3D)(EnemyByIndex(B(at::kTarget)), 0x10000) != 0;
    }
    if (near) {
        BH_CALL(Sprite_SetAnimation)(5);
        BH_CALL(Battle_SetTargetFlag40)(B(at::kTarget));
        Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
    }
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x43D990: step 4. When Sprite_ScriptTickOnce answers al not 0: the
// leap back to the acting enemy (0x904B34, its object 0x93B960 + (actor - 3) *
// 0x128): the Sprite_Current of after the call +0xC = (its +0x34 - +0x34) / 16,
// +0x10 = (its +0x38 - +0x38) / 16 (signed, toward zero, on the wrapped
// difference), +0x14 = 0x40, +0x20 = -8, +0x18 = (its +0x3C - +0x3C) / 16, +2
// up by one, and a tail jump to Sprite_ScriptTickOnce. Its al (the first or
// the second tick's) is the answer.
extern "C" unsigned char __cdecl BossWeretigrFx_Leap(void) {
    const U first = BH_CALL(Sprite_ScriptTickOnce)();
    if ((first & 0xFF) == 0) return static_cast<unsigned char>(first);
    auto sixteenth = [](U to, U from) { return S(static_cast<U>(S(to - from) / 16)); };
    unsigned char* s = Sprite_Current;
    SetLong(s + 0xC, sixteenth(L(EnemyByIndex(B(at::kActor)) + 0x34), L(s + 0x34)));
    s = Sprite_Current;
    SetLong(s + 0x10, sixteenth(L(EnemyByIndex(B(at::kActor)) + 0x38), L(s + 0x38)));
    SetLong(Sprite_Current + 0x14, 0x40);
    SetLong(Sprite_Current + 0x20, -8);
    s = Sprite_Current;
    SetLong(s + 0x18, sixteenth(L(EnemyByIndex(B(at::kActor)) + 0x3C), L(s + 0x3C)));
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x43DA60: step 5, the flight: +0x34 += +0xC, +0x38 += +0x10, the
// word +0x3E += the word +0x14, then the dword +0x3C += +0x18 (it holds that
// word), +0x14 += +0x20; at +0x14 = -0x40 +2 up by one. A tail jump to
// Sprite_ScriptTickOnce.
extern "C" unsigned char __cdecl BossWeretigrFx_Fly(void) {
    unsigned char* const s = Sprite_Current;
    SetLong(s + 0x34, S(L(s + 0x34) + L(s + 0xC)));
    SetLong(s + 0x38, S(L(s + 0x38) + L(s + 0x10)));
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
    SetLong(s + 0x3C, S(L(s + 0x3C) + L(s + 0x18)));
    SetLong(s + 0x14, S(L(s + 0x14) + L(s + 0x20)));
    if (Long(s + 0x14) == -0x40) s[2] = static_cast<unsigned char>(s[2] + 1);
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x43DAC0: step 6. The acting enemy's object (0x904B34) +0 &= 0xBF;
// Sprite_Current = the task's owner 0x93B940 for Sprite_SetAnimation(0), then
// back to the task (the one held before); BattleTask_FreeCurrent.
extern "C" void __cdecl BossWeretigrFx_Finish(void) {
    unsigned char* const task = Sprite_Current;
    EnemyByIndex(B(at::kActor))[0] &= 0xBF;
    Sprite_Current = At(L(At(at::kOwner)));
    BH_CALL(Sprite_SetAnimation)(0);
    Sprite_Current = task;
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x43DB10: +1 = 1, the trail: TaskSteps through
// BossWeretigrFx_TrailSteps (2).
extern "C" void __cdecl BossWeretigrFx_Trail(void) { TaskSteps("BossWeretigrFx_Trail", AddressOf(BossWeretigrFx_TrailSteps), 2); }

// The trail's shade: +0x5D, +0x5F, +0x5E = the low byte of +0xB * 0xF0 (the
// byte read again for each).
void Shade() {
    Sprite_Current[0x5D] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
    Sprite_Current[0x5F] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
    Sprite_Current[0x5E] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
}

// original 0x43DB40: trail step 0: +0 |= 0x20, +0x5C = 3, Shade, +9 = 8, +2 up
// by one; a tail jump to Sprite_ScriptTickOnce.
extern "C" unsigned char __cdecl BossWeretigrFx_TrailBegin(void) {
    Sprite_Current[0] |= 0x20;
    Sprite_Current[0x5C] = 3;
    Shade();
    Sprite_Current[9] = 8;
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x43DBA0: trail step 1: Shade; Sprite_ScriptTickOnce; the
// Sprite_Current of after +9 down by one - at 0 before it, a tail jump to
// BattleTask_FreeCurrent.
extern "C" void __cdecl BossWeretigrFx_TrailFade(void) {
    Shade();
    BH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[9];
    s[9] = static_cast<unsigned char>(count - 1);
    if (count == 0) BH_CALL(BattleTask_FreeCurrent)();
}

// ===========================================================================
// Kind 40 (Mikba, area 43) - set-ups 34 and 41 are BSH's
// ===========================================================================

// original 0x43DBF0: BossKind_Table[40]: jmp [BossMikba_States + 4 * +1] (12).
extern "C" U __cdecl BossMikba_Dispatch(unsigned passed) {
    return Dispatch("BossMikba_Dispatch", AddressOf(BossMikba_States), 12, 1, passed);
}

// original 0x43DC10: state 0: BossMikba_Anims, BossMikba_Hook,
// BossMikba_Sounds; +1 = 2; tail Sprite_ScriptTick.
extern "C" unsigned char __cdecl BossMikba_Enter(void) {
    StoreKind(AddressOf(BossMikba_Anims), bof3::addr::BossMikba_Hook, AddressOf(BossMikba_Sounds));
    return Idle();
}

// original 0x43DC50: state 6: jmp [BossMikba_ActSubs + 4 * +2] (6: the generic
// action entries with the kind's death dispatcher at 4).
extern "C" U __cdecl BossMikba_ActDispatch(unsigned passed) {
    return Dispatch("BossMikba_ActDispatch", AddressOf(BossMikba_ActSubs), 6, 2, passed);
}

// original 0x43DC70: +2 entry 4: jmp [BossMikba_DeathSteps + 4 * +3] (7: the six
// steps below, then BossOp_ScriptTick).
extern "C" U __cdecl BossMikba_DeathDispatch(unsigned passed) {
    return Dispatch("BossMikba_DeathDispatch", AddressOf(BossMikba_DeathSteps), 7, 3, passed);
}

// original 0x43DC90: death step 0. When Sprite_ScriptTick answers al not 0:
// the Sprite_Current of after +9 = 0x40, +0xA = 0x20, +3 = 1.
extern "C" void __cdecl BossMikba_DeathWait(void) {
    if ((BH_CALL(Sprite_ScriptTick)() & 0xFF) == 0) return;
    Sprite_Current[9] = 0x40;
    Sprite_Current[0xA] = 0x20;
    Sprite_Current[3] = 1;
}

// original 0x43DCC0: death step 1, the flashes. With +9 equal to +0xA: unless
// 0x904AAD bit 2, Sound_PlayEffect(0x601) and 0x904AAD (read after) |= 4;
// BattleWin_DrawTileRgb(0, 0, 0xB, 0xFFFF, 1); the Sprite_Current of after
// +0xA halved (a byte). Otherwise +9 down by one. Then, +9 at 0 (the
// Sprite_Current of after), +3 up by one.
extern "C" void __cdecl BossMikba_DeathFlash(void) {
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[9];
    if (count == s[0xA]) {
        if ((B(at::kScript) & 4) == 0) {
            BH_CALL(Sound_PlayEffect)(0x601);
            B(at::kScript) |= 4;
        }
        BH_CALL(BattleWin_DrawTileRgb)(0, 0, 0xB, 0xFFFF, 1);
        Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] >> 1);
    } else {
        s[9] = static_cast<unsigned char>(count - 1);
    }
    if (Sprite_Current[9] == 0) Sprite_Current[3] = static_cast<unsigned char>(Sprite_Current[3] + 1);
}

// original 0x43DD30: death step 2. Sprite_ScriptTickOnce (answer not read);
// Gfx_ClearRect(0x340, 0x100, 0xC0, 0x100); the Sprite_Current of after: +0x18
// = the s16 +0x2E, +0x1C = the s16 +0x30, +0x20 = 0, +0xC = 0xBE, +0x10 = 0x68,
// the words +0x2E = 0x64 and +0x30 = 0x50, +0xA = 1, +0x24 |= 0x88;
// Sound_PlayEffect(0x602); BossMikba_DrawQuad(the Sprite_Current of after
// +9 - the original pushes it with the Sprite_Current's upper half above, which
// the draw never reads); the Sprite_Current of after +3 up by one.
extern "C" void __cdecl BossMikba_DeathOpen(void) {
    BH_CALL(Sprite_ScriptTickOnce)();
    BH_CALL(Gfx_ClearRect)(0x340, 0x100, 0xC0, 0x100);
    unsigned char* const s = Sprite_Current;
    SetLong(s + 0x18, static_cast<std::int16_t>(Word(s + 0x2E)));
    SetLong(s + 0x1C, static_cast<std::int16_t>(Word(s + 0x30)));
    SetLong(s + 0x20, 0);
    SetLong(s + 0xC, 0xBE);
    SetLong(s + 0x10, 0x68);
    SetWord(s + 0x2E, 0x64);
    SetWord(s + 0x30, 0x50);
    s[0xA] = 1;
    s[0x24] |= 0x88;
    BH_CALL(Sound_PlayEffect)(0x602);
    BH_CALL(BossMikba_DrawQuad)(Sprite_Current[9]);
    Sprite_Current[3] = static_cast<unsigned char>(Sprite_Current[3] + 1);
}

// original 0x43DDE0: death step 3. +0x20 += the byte +0xA; the s32 +0xC above
// 0x28: down by one, and on an odd Frame_Counter +0x1C up by one; the s32
// +0x10 above 0x28: down by one; BossMikba_DrawQuad(+0x20); then, the
// Sprite_Current of after: with Frame_Counter & 7 not 0, +0xA up by one; at
// +0xA 0x40 or above (a byte), +3 up by one.
extern "C" void __cdecl BossMikba_DeathSpread(void) {
    unsigned char* const s = Sprite_Current;
    SetLong(s + 0x20, S(L(s + 0x20) + s[0xA]));
    const std::int32_t w = Long(s + 0xC);
    if (w > 0x28) {
        SetLong(s + 0xC, w - 1);
        if (Frame_Counter & 1) SetLong(s + 0x1C, S(L(s + 0x1C) + 1u));
    }
    const std::int32_t h = Long(s + 0x10);
    if (h > 0x28) SetLong(s + 0x10, h - 1);
    BH_CALL(BossMikba_DrawQuad)(L(s + 0x20));
    if (Frame_Counter & 7) Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] + 1);
    if (Sprite_Current[0xA] >= 0x40) Sprite_Current[3] = static_cast<unsigned char>(Sprite_Current[3] + 1);
}

// original 0x43DE60: death step 4. Sprite_SetAnimationBank(0x1C1); the
// Sprite_Current of after +0x2A = 1; Sprite_SetAnimation(8); the Sprite_Current
// of after +0x24 &= 0x77; Sprite_ScriptTick (answer not read);
// LoadDatFile(0xD0); the Sprite_Current of after +3 up by one.
extern "C" void __cdecl BossMikba_DeathLoad(void) {
    BH_CALL(Sprite_SetAnimationBank)(0x1C1);
    Sprite_Current[0x2A] = 1;
    BH_CALL(Sprite_SetAnimation)(8);
    Sprite_Current[0x24] &= 0x77;
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(LoadDatFile)(0xD0);
    Sprite_Current[3] = static_cast<unsigned char>(Sprite_Current[3] + 1);
}

// original 0x43DEA0: death step 5. Sprite_ScriptTick (answer not read); when
// File_LoadDone answers (its eax) not 0: Battle_EnemyDefeated, then the
// Sprite_Current of after +0 &= 0xBF, +1 = 6, +2 = 4, +3 = 6 (BossOp_ScriptTick
// from then on).
extern "C" void __cdecl BossMikba_DeathEnd(void) {
    BH_CALL(Sprite_ScriptTick)();
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Battle_EnemyDefeated)();
    unsigned char* const s = Sprite_Current;
    s[0] &= 0xBF;
    s[1] = 6;
    s[2] = 4;
    s[3] = 6;
}

// original 0x43DEE0: the +0xF4 hook through BossMikba_Hooks (3, all BareRet).
extern "C" U __cdecl BossMikba_Hook(unsigned word) { return HookTable("BossMikba_Hook", AddressOf(BossMikba_Hooks), word); }

// original 0x43E290: the death's quad, turned by the argument. One
// draw-mode primitive at Gfx_PacketNext (read after the page is made):
// Gpu_SetDrawMode(it, 0, 0, Gpu_GetTPage(2, 0, 0x340, 0x100) & 0xFFFF, 0),
// Gfx_CommitPrim(1, 0xC). Gte_PushMatrix; the rotation of the SVECTOR (0, 0,
// turn << 4) - Gte_RotMatrixYXZ into a matrix on the stack, Gte_SetRotMatrix;
// its translation zeroed (Gte_TransMatrix from a zero vector - the same three
// longs the corners' RotTrans writes into), Gte_SetTransMatrix. Then a PolyFT4
// at Gfx_PacketNext (read again): its tpage word +0x26 = Gpu_GetTPage(2, 0,
// 0x340, 0x100), colour 0x80 x3; four corners, each (x, y) = (-/+ +0xC / 2,
// -/+ +0x10 / 2) (s32 halved toward zero, then a short) in the order (-,-),
// (+,-), (-,+), (+,+) - with the SVECTOR's z left holding the turn, as the
// original's stack reuse has it - through Gte_RotTrans, the vertex floats x =
// Sprite_Current +0x18 + out x, y = +0x1C + out y - 0x28 (the Sprite_Current
// of after each RotTrans); the uv bytes (0, 0), (0xBF, 0), (0, 0x68), (0xBF,
// 0x68); Gpu_SetPolyFT4, Gfx_CommitPrim(1, 0x48), Gte_PopMatrix. The original's
// flag argument to RotTrans (a third push) is dropped: ours takes two.
extern "C" void __cdecl BossMikba_DrawQuad(unsigned turn) {
    const unsigned page = BH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100) & 0xFFFF;
    BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
    BH_CALL(Gte_PushMatrix)();
    short vector[4] = {0, 0, static_cast<short>(turn << 4), 0};
    alignas(4) unsigned char matrix[32] = {};
    BH_CALL(Gte_RotMatrixYXZ)(vector, reinterpret_cast<short*>(matrix));
    BH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    long out[3] = {0, 0, 0};
    BH_CALL(Gte_TransMatrix)(reinterpret_cast<unsigned long*>(matrix), reinterpret_cast<const unsigned long*>(out));
    BH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    unsigned char* const prim = Gfx_PacketNext;
    SetWord(prim + 0x26, BH_CALL(Gpu_GetTPage)(2, 0, 0x340, 0x100));
    prim[4] = 0x80;
    prim[5] = 0x80;
    prim[6] = 0x80;
    auto half = [](const unsigned char* p) { return Long(p) / 2; };
    static constexpr int kSign[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    static constexpr unsigned kVertex[4] = {8, 0x18, 0x28, 0x38};
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char* const s = Sprite_Current;
        vector[0] = static_cast<short>(kSign[i][0] * half(s + 0xC));
        vector[1] = static_cast<short>(kSign[i][1] * half(s + 0x10));
        BH_CALL(Gte_RotTrans)(vector, out);
        const float x = static_cast<float>(S(L(Sprite_Current + 0x18) + static_cast<U>(out[0])));
        const float y = static_cast<float>(S(L(Sprite_Current + 0x1C) + static_cast<U>(out[1]) - 0x28u));
        std::memcpy(prim + kVertex[i], &x, 4);
        std::memcpy(prim + kVertex[i] + 4, &y, 4);
    }
    prim[0x14] = 0;
    prim[0x15] = 0;
    prim[0x25] = 0;
    prim[0x24] = 0xBF;
    prim[0x34] = 0;
    prim[0x35] = 0x68;
    prim[0x44] = 0xBF;
    prim[0x45] = 0x68;
    BH_CALL(Gpu_SetPolyFT4)(prim);
    BH_CALL(Gfx_CommitPrim)(1, 0x48);
    BH_CALL(Gte_PopMatrix)();
}

void BossSg_Inject() {
    if (bof3::WantsShadow("boss_sg")) boss_sg::SelfTest();
    BOF3_INJECT(BossDolphin_Dispatch);
    BOF3_INJECT(BossDolphin_Enter);
    BOF3_INJECT(BossDolphin_Hook);
    BOF3_INJECT(Boss29_Setup);
    BOF3_INJECT(Boss29_End);
    BOF3_INJECT(BossGisshan_Dispatch);
    BOF3_INJECT(BossGisshan_Enter);
    BOF3_INJECT(BossGisshan_Hook);
    BOF3_INJECT(BossScylla_Dispatch);
    BOF3_INJECT(BossScylla_Enter);
    BOF3_INJECT(BossScylla_Hook);
    BOF3_INJECT(BossGarr2_Dispatch);
    BOF3_INJECT(BossGarr2_Enter);
    BOF3_INJECT(BossGarr2_ActDispatch);
    BOF3_INJECT(BossGarr2_Death);
    BOF3_INJECT(BossGarr2_Hook);
    BOF3_INJECT(Boss31_Setup);
    BOF3_INJECT(Boss31_End);
    BOF3_INJECT(Boss31_Exit);
    BOF3_INJECT(BossDZombie_Dispatch);
    BOF3_INJECT(BossDZombie_Enter);
    BOF3_INJECT(BossDZombie_ActDispatch);
    BOF3_INJECT(BossDZombie_Death);
    BOF3_INJECT(BossDZombie_Hook);
    BOF3_INJECT(Boss32_Setup);
    BOF3_INJECT(Boss32_End);
    BOF3_INJECT(Boss32_Exit);
    BOF3_INJECT(Boss33_Setup);
    BOF3_INJECT(Boss33_End);
    BOF3_INJECT(BossWeretigrFx_Task);
    BOF3_INJECT(BossWeretigrFx_Main);
    BOF3_INJECT(BossWeretigrFx_Begin);
    BOF3_INJECT(BossWeretigrFx_AwaitPose);
    BOF3_INJECT(BossWeretigrFx_Rise);
    BOF3_INJECT(BossWeretigrFx_Strike);
    BOF3_INJECT(BossWeretigrFx_Leap);
    BOF3_INJECT(BossWeretigrFx_Fly);
    BOF3_INJECT(BossWeretigrFx_Finish);
    BOF3_INJECT(BossWeretigrFx_Trail);
    BOF3_INJECT(BossWeretigrFx_TrailBegin);
    BOF3_INJECT(BossWeretigrFx_TrailFade);
    BOF3_INJECT(BossMikba_Dispatch);
    BOF3_INJECT(BossMikba_Enter);
    BOF3_INJECT(BossMikba_ActDispatch);
    BOF3_INJECT(BossMikba_DeathDispatch);
    BOF3_INJECT(BossMikba_DeathWait);
    BOF3_INJECT(BossMikba_DeathFlash);
    BOF3_INJECT(BossMikba_DeathOpen);
    BOF3_INJECT(BossMikba_DeathSpread);
    BOF3_INJECT(BossMikba_DeathLoad);
    BOF3_INJECT(BossMikba_DeathEnd);
    BOF3_INJECT(BossMikba_Hook);
    BOF3_INJECT(BossMikba_DrawQuad);
}
