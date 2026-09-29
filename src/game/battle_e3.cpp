// Group BE3 of round twelve (wave one, stage B): 49 functions of the battle
// engine's resident code, 0x437030..0x442F97 - analysis/round12_cut.tsv's 47
// rows for BE3, the unlisted 0x437230 (tools/band_rows.py's flag) and
// 0x441510 (docs/boss_harness.md section 10.7) - each read to its last
// instruction with capstone (2026-09-29) and taken through the boss harness's
// engine frame (boss_harness.h, Group::engine). docs/battle_e3.md has them
// one row each.
//
//   the enemy ops round eleven left   EnemyOp_Steps entries 7, 8 and 9 (the
//                                     cast, its end, the leave) with their
//                                     sub-tables, the action's end 0x4376A0,
//                                     the roll 0x4376F0, the sound 0x437450
//   the party objects' states         BattleObj_StateTable's states 6 (the hit
//                                     taken, with its five sub-trees), 10, 11
//                                     and 26, state 8's sub-state 3, the pose
//                                     helper 0x441510
//   two party helpers                 the stat pass 0x442310 and the loss test
//                                     0x442420
//   a fixed-point helper              0x441090
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables, the task spawns abort on
// BattleTask_Create's 0xFF (all 48 slots taken), the party loops abort on a
// party count past ObjTrio's three and the special attack's copy on an actor
// past them, where the original jumps through, writes past or reads past
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6; nothing reaches it). Every call goes through the harness
// (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in for the
// callees; the callees of other groups of this wave are raw addresses in
// battle_e3_callees.h.
//
// Sprite_Current, Field_State and 0x939AD8 are each re-read wherever the
// original re-reads it (after every call at least). A value the original
// pushes with a register's leftover upper bytes goes out as the byte or word
// its callee reads (each callee read, docs/battle_e3.md section 3), except
// Battle_ApplyDamage's target, whose upper bytes are Field_State's own (the
// register held that pointer) and are passed as such.
#include "game/battle_e3.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e3_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e3::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* PtrAt(U cell) { return At(static_cast<U>(Long(At(cell)))); }
void SetPtrAt(U cell, const void* p) { SetLong(At(cell), static_cast<std::int32_t>(Key(p))); }
unsigned char* S() { return Sprite_Current; }
unsigned char* F() { return Field_State; }
unsigned char* Enemy() { return PtrAt(at::kCurrentEnemy); }
U ULong(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetULong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
short SWord(const unsigned char* p) { return static_cast<short>(Word(p)); }
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// The ability record `id` (24 bytes at 0x65C4D8): a flag byte, or the word +4.
unsigned AbilityId() { return Word(At(at::kAbility)); }
unsigned char AbilityByte(U base) { return At(base + AbilityId() * at::kAbilityStride)[0]; }
unsigned AbilityWord4() { return Word(At(at::kAbilityWord4 + AbilityId() * at::kAbilityStride)); }

// CharacterRecords' record for a member's character byte +0x148 (unchecked, as
// the original: a character byte, not a table index of this group's).
unsigned char* CharRecord(unsigned character) { return At(at::kCharRecords + character * at::kCharStride); }
unsigned char* Member(unsigned i) { return At(at::kParty + i * at::kPartyStride); }

// A table entry called as the original's jmp enters it: with the word the
// dispatcher's own caller left at [esp + 4] (BattleEnemy_RunAll pushes
// nothing; an entry that reads it reads that word), its eax passed back.
using Forward = U (__cdecl*)(unsigned);
U Entry(U table, unsigned index) { return ULong(At(table + 4 * index)); }

// jmp [table + 4 * Sprite_Current[at]]: the table's `entries` handlers, as read
// (the fuzz swaps the cells for recorders); a Fatal past them (the original
// jumps through the dword after, another table's entry or data).
unsigned CheckedState(const char* who, U table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e3.md section 6)",
                    who, at, state, entries, (unsigned)table);
    return state;
}
U Dispatch(const char* who, U table, unsigned entries, unsigned at, unsigned passed) {
    return reinterpret_cast<Forward>(static_cast<std::uintptr_t>(Entry(table, CheckedState(who, table, entries, at))))(passed);
}
// The party objects' dispatchers: the same jmp, with nothing forwarded -
// BattleObj_RunState's entries read no word, and their eax reaches only
// callers that drop it (battle_obj_states.md section 2).
void Run(const char* who, U table, unsigned entries, unsigned at) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(table, CheckedState(who, table, entries, at))))();
}

// BattleTask_Create's slot: a Fatal on 0xFF (all 48 taken), where the original
// writes at 0x93A000 + 0xFF * 0x84, past the pool (docs/battle_e3.md section 6).
unsigned char* TaskSlot(const char* who, unsigned n) {
    n &= 0xFF;
    if (n >= at::kTaskCount)
        bof3::Fatal("%s: BattleTask_Create answered slot %u (all 48 taken) - the original writes at 0x93A000 + %u * 0x84, "
                    "past the pool (docs/battle_e3.md section 6)",
                    who, n, n);
    return At(at::kTasks + n * at::kTaskStride);
}

// The party count 0x904AB0 as the loops over ObjTrio read it: a Fatal past its
// three members, where the original walks on into the window records.
unsigned PartyCount(const char* who) {
    const unsigned n = B(at::kPartyCount);
    if (n > at::kPartyMax)
        bof3::Fatal("%s: the party count 0x904AB0 is %u, past ObjTrio's three members - the original walks on into "
                    "WindowRecords (docs/battle_e3.md section 6)",
                    who, n);
    return n;
}

// The two moves the objects make: +0x34 / +0x38 by the velocity +0xC / +0x10,
// forward or back.
void MoveBy(unsigned char* s, bool back) {
    const U x = ULong(s + 0x34), vx = ULong(s + 0xC);
    SetULong(s + 0x34, back ? x - vx : x + vx);
    const U y = ULong(s + 0x38), vy = ULong(s + 0x10);
    SetULong(s + 0x38, back ? y - vy : y + vy);
}

// 0x904AA8 bit 14 cleared when 0x904B8A is the object's actor (the fall's two
// sites; the object read after the flags, as the original).
void ClearFormFlag() {
    const bool bit = (Word(At(at::kRoundFlags)) & 0x4000) != 0;
    unsigned char* const s = S();
    if (bit && B(at::kFormActor) == s[5]) SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xBFFF);
}

}  // namespace

// ===========================================================================
// The enemy ops: EnemyOp_Steps entries 7 (the cast), 8 (its end), 9 (the leave)
// ===========================================================================

// original 0x437030: EnemyOp_Steps[7] - jmp [EnemyOp_CastSubs + 4 * +2] (3:
// EnemyOp_CastStart, EnemyOp_CastCue, BossMyria_State7End).
extern "C" unsigned __cdecl EnemyOp_CastDispatch(unsigned passed) {
    return Dispatch("EnemyOp_CastDispatch", AddressOf(EnemyOp_CastSubs), 3, 2, passed);
}

// original 0x437050: the cast's step 0 - nothing (eax 0) until File_LoadDone
// answers (its whole eax tested). Then by 0x939AD8's +0x105: 4 and an event
// battle (0x904AAA) - Battle_LoadSoundByKey(its kind +0x100 + 0x20, the set
// 0x904B8D): +9 = 0 when it answers, 0x1E when not; 4 outside one, +9 kept;
// not 4, +9 = 0. Then by the ability 0x904B80: its record byte +5 bit 3 - +1 up
// by one (straight to state 8); else +9 = the enemy data record's +0x8B by
// 0x939AD8's +0xF0 (read again), BattleEnemy_SetAnimation(6), +2 up by one.
// Both end in a tail jump to BattleEnemy_ScriptTickOnce, whose al is the answer.
extern "C" unsigned char __cdecl EnemyOp_CastStart(void) {
    if (BH_CALL(File_LoadDone)() == 0) return 0;
    unsigned char* const e = Enemy();
    if (e[0x105] == 4) {
        if (B(at::kFight) != 0) {
            const unsigned key = static_cast<unsigned char>(e[0x100] + 0x20);
            const bool loaded = BH_CALL(Battle_LoadSoundByKey)(key, B(at::kSoundSet)) != 0;
            S()[9] = loaded ? 0 : 0x1E;
        }
    } else {
        S()[9] = 0;
    }
    if (AbilityByte(at::kAbilityFlags5) & 8) {
        S()[1] += 1;
    } else {
        const unsigned record = Enemy()[0xF0];
        S()[9] = At(at::kEnemyData8B + record * at::kEnemyDataStride)[0];
        BH_CALL(BattleEnemy_SetAnimation)(6);
        S()[2] += 1;
    }
    return BH_CALL(BattleEnemy_ScriptTickOnce)();
}

// original 0x437120: the cast's step 1 - +9, when not 0, down by one; at 0 the
// cue: in an event battle Sound_PlayEffectUnlessNone(the word +2 of 0x939AD8's
// +0xF8 table), else Sound_PlayEffect(0x601 + 2 * its +0xF0), then +2 up by one
// (Sprite_Current read again). Both end in a tail jump to
// BattleEnemy_ScriptTickOnce, whose al is the answer.
extern "C" unsigned char __cdecl EnemyOp_CastCue(void) {
    unsigned char* const s = S();
    if (s[9] != 0) {
        s[9] -= 1;
        return BH_CALL(BattleEnemy_ScriptTickOnce)();
    }
    const bool fight = B(at::kFight) != 0;
    unsigned char* const e = Enemy();
    if (fight)
        BH_CALL(Sound_PlayEffectUnlessNone)(Word(PtrAt(Key(e + 0xF8)) + 2));
    else
        BH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(0x601 + 2 * e[0xF0]));
    S()[2] += 1;
    return BH_CALL(BattleEnemy_ScriptTickOnce)();
}

// original 0x437180: EnemyOp_Steps[8] - call [EnemyOp_CastDoneSubs + 4 * +2]
// (3: BossMyria_State8Tick, EnemyOp_CastDoneCost, EnemyOp_CastDoneTickUnless),
// then a tail jump to EnemyOp_CastDoneCheck.
extern "C" void __cdecl EnemyOp_CastDoneDispatch(void) {
    const unsigned state = CheckedState("EnemyOp_CastDoneDispatch", AddressOf(EnemyOp_CastDoneSubs), 3, 2);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(AddressOf(EnemyOp_CastDoneSubs), state)))();
    BH_CALL(EnemyOp_CastDoneCheck)();
}

// original 0x4371A0: the cast's end, step 1 - with 0x939AD8's +0x105 at 4, its
// word +0xA6 down by the byte 0x904B88 (0x939AD8 read again); 0x904AA8 |=
// 0x800; that object's +0x105 = 0; unless the ability's word +4 has bit 11,
// BattleEnemy_ScriptTickOnce; +2 up by one. (eax on return is Sprite_Current:
// nobody reads it - EnemyOp_CastDoneDispatch drops it.)
extern "C" void __cdecl EnemyOp_CastDoneCost(void) {
    unsigned char* e = Enemy();
    if (e[0x105] == 4) {
        SetWord(e + 0xA6, Word(e + 0xA6) - B(at::kCost));
        e = Enemy();
    }
    SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) | 0x800);
    e[0x105] = 0;
    if ((AbilityWord4() & 0x800) == 0) BH_CALL(BattleEnemy_ScriptTickOnce)();
    S()[2] += 1;
}

// original 0x437200: the cast's end, step 2 - unless the ability's byte +5 has
// bit 3, BattleEnemy_ScriptTickOnce; then +9 up by one (each frame).
extern "C" void __cdecl EnemyOp_CastDoneTickUnless(void) {
    if ((AbilityByte(at::kAbilityFlags5) & 8) == 0) BH_CALL(BattleEnemy_ScriptTickOnce)();
    S()[9] += 1;
}

// original 0x437230 (in no start list; after 0x437200's padding, reached only
// by 0x437180's tail jump): with 0x904AA8 bit 2, a tail jump to
// EnemyOp_EndAction.
extern "C" void __cdecl EnemyOp_CastDoneCheck(void) {
    if (B(at::kRoundFlags) & 4) BH_CALL(EnemyOp_EndAction)();
}

// original 0x437240: EnemyOp_Steps[9] - jmp [EnemyOp_LeaveSubs + 4 * +2] (3:
// EnemyOp_LeaveStart, EnemyOp_LeaveStep, EnemyOp_LeaveEnd).
extern "C" unsigned __cdecl EnemyOp_LeaveDispatch(unsigned passed) {
    return Dispatch("EnemyOp_LeaveDispatch", AddressOf(EnemyOp_LeaveSubs), 3, 2, passed);
}

// original 0x437260: the leave's step 0 - the velocity +0x18 = -0x1000, +0x1C =
// 0, turned by the pose (0x4467C0, BE4's); the pose's bit 1 flipped; +0xC /
// +0x10 = 0; +9 = 0x10, +0xA = 8; BattleEnemy_SetAnimation(0); the tint
// Sprite_SetTint(Sprite_Current, -4, -4, -4, 1); +0 |= 0x20; +0x5C = 2, the
// colour +0x5D..+0x5F = 0; Sound_PlayEffect(0x102); BattleEnemy_ScriptTick; +2
// up by one. Sprite_Current read for every store, as the original.
extern "C" void __cdecl EnemyOp_LeaveStart(void) {
    SetULong(S() + 0x18, 0xFFFFF000u);
    SetULong(S() + 0x1C, 0);
    BH_AT(void (__cdecl*)(unsigned char*), at::kTurnVelocity18)(S());
    S()[8] ^= 2;
    SetULong(S() + 0xC, 0);
    SetULong(S() + 0x10, 0);
    S()[9] = 0x10;
    S()[0xA] = 8;
    BH_CALL(BattleEnemy_SetAnimation)(0);
    BH_CALL(Sprite_SetTint)(S(), 0xFC, 0xFC, 0xFC, 1);
    S()[0] |= 0x20;
    S()[0x5C] = 2;
    S()[0x5D] = 0;
    S()[0x5E] = 0;
    S()[0x5F] = 0;
    BH_CALL(Sound_PlayEffect)(0x102);
    BH_CALL(BattleEnemy_ScriptTick)();
    S()[2] += 1;
}

// original 0x437320: the leave's step 1 - +9, when not 0, down by one (a wait);
// at 0, +0xA down by one; at 0, +2 up by one; else the colour +0x5D..+0x5F each
// down by 0x10, the velocity +0xC / +0x10 up by +0x18 / +0x1C, the position
// +0x34 / +0x38 up by it, and a tail jump to BattleEnemy_ScriptTick. (eax on
// return is dropped by BattleEnemy_RunAll's dispatchers.)
extern "C" void __cdecl EnemyOp_LeaveStep(void) {
    unsigned char* const s = S();
    if (s[9] != 0) {
        s[9] -= 1;
        return;
    }
    s[0xA] -= 1;
    if (s[0xA] == 0) {
        s[2] += 1;
        return;
    }
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0xF0);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xF0);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xF0);
    SetULong(s + 0xC, ULong(s + 0xC) + ULong(s + 0x18));
    SetULong(s + 0x10, ULong(s + 0x10) + ULong(s + 0x1C));
    MoveBy(s, false);
    BH_CALL(BattleEnemy_ScriptTick)();
}

// original 0x4373C0: the leave's step 2 (also BossNina_WalkSteps[2]) -
// Battle_RemoveFromTurnOrder(+5); the enemies left 0x904AB3 down by one, and at
// 0 0x904AE8 |= 2 (the win); Sprite_ReleaseTint(Sprite_Current);
// Battle_ClearActorBit(+5); window record 4's +3 (0x8031F3) = 2;
// BattleBanner_ClearAll; a tail jump to Effect_Release. No Battle_EnemyDefeated:
// the enemy leaves the count without being defeated.
extern "C" void __cdecl EnemyOp_LeaveEnd(void) {
    BH_CALL(Battle_RemoveFromTurnOrder)(S()[5]);
    const auto left = static_cast<unsigned char>(B(at::kEnemiesLeft) - 1);
    B(at::kEnemiesLeft) = left;
    if (left == 0) B(at::kBattleEnd) |= 2;
    BH_CALL(Sprite_ReleaseTint)(S());
    BH_CALL(Battle_ClearActorBit)(S()[5]);
    B(at::kWindow4State) = 2;
    BH_CALL(BattleBanner_ClearAll)();
    BH_CALL(Effect_Release)();
}

// original 0x437450 (sound): Sound_PlayEffect(sound) unless its low word is
// 0xFFFF (the whole dword handed on; Sound_PlayEffect reads the word). An
// enemy state's or an actor's cue (twelve callers, eleven of them ours).
extern "C" void __cdecl Sound_PlayEffectUnlessNone(unsigned sound) {
    if ((sound & 0xFFFF) == 0xFFFF) return;
    BH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(sound));
}

// original 0x4376A0: the end of an enemy's action - Sprite_Current +1 = 2, +2 =
// 0; Battle_ClearActorBit(+5); 0x939AD8's +0x110 &= ~0x200; its +0x105 = 0
// unless 0x904AA8 bit 6.
extern "C" void __cdecl EnemyOp_EndAction(void) {
    S()[1] = 2;
    S()[2] = 0;
    BH_CALL(Battle_ClearActorBit)(S()[5]);
    unsigned char* const e = Enemy();
    SetULong(e + 0x110, ULong(e + 0x110) & ~0x200u);
    if ((B(at::kRoundFlags) & 0x40) == 0) Enemy()[0x105] = 0;
}

// original 0x4376F0: with 0x939AD8's +0x90 bit 3 and Rand's bit 0, 0x904AA8 |=
// 0x80 and BattleTask_Create(0, 2) (its slot not used) - the enemy side of
// BattleObj_SwingEnd's bit 7 and task.
extern "C" void __cdecl EnemyOp_RollBit80Task(void) {
    if ((Enemy()[0x90] & 8) == 0) return;
    if ((BH_CALL(Rand)() & 1) == 0) return;
    B(at::kRoundFlags) |= 0x80;
    BH_CALL(BattleTask_Create)(0, 2);
}

// original 0x441090 (value, sign): the high word of the 16.16 value - one more
// when sign is not negative and the value's low word is not 0 - in ax; the
// high half of eax is sign's (the original loads it there first).
extern "C" unsigned __cdecl Fixed_HighRoundUp(unsigned value, unsigned sign) {
    const unsigned high = value >> 16;
    if (static_cast<int>(sign) < 0 || (value & 0xFFFF) == 0) return (sign & 0xFFFF0000u) | high;
    return (sign & 0xFFFF0000u) | ((high + 1) & 0xFFFF);
}

// ===========================================================================
// The party objects: state 6, the hit taken
// ===========================================================================

// original 0x441A10: BattleObj_StateTable[6] - jmp [BattleObj_HitSubs + 4 * +2]
// (6: the entry twice, the receive, the step, the fall, the HP change).
extern "C" void __cdecl BattleObj_StateHit(void) { Run("BattleObj_StateHit", AddressOf(BattleObj_HitSubs), 6, 2); }

// original 0x441A30: BattleObj_HitSubs[0] and [2] - Field_State +0x12F =
// Sprite_Current +0x4B, the word +0x140 = its word +0x58; +4 = 0, +2 = 1.
extern "C" void __cdecl BattleObj_HitEnter(void) {
    F()[0x12F] = S()[0x4B];
    SetWord(F() + 0x140, Word(S() + 0x58));
    S()[4] = 0;
    S()[2] = 1;
}

// original 0x441A70: BattleObj_HitSubs[1] - jmp [BattleObj_HitReceiveSubs + 4 *
// +3] (3: BattleObj_HitReceive, _HitWaitPose, _HitEnd).
extern "C" void __cdecl BattleObj_HitReceiveDispatch(void) {
    Run("BattleObj_HitReceiveDispatch", AddressOf(BattleObj_HitReceiveSubs), 3, 3);
}

// original 0x441A90: the hit received (the party twin of
// EnemyOp_ReceiveAction; the sibling's hypothesis for the PSX twin is
// Battle_ResolveAction_Party).
//   1. The words 0x904B9A / 0x904B98 = 0; 0x904B5C = Sprite_Current, 0x904B54
//      = its +5; 0x904B60 = Field_State + 0x124, whose words +4 / +6 and byte
//      +8 are zeroed; 32 bytes of Field_State +0xA0 to 0x939F80.
//   2. The member's +0x90 bit 11: the word 0x939F86 = 1. Its +0x130 bit 0:
//      0x939F9B up by 25, to 100 at most.
//   3. By the action's kind 0x904B35: 1 - the word Battle_ApplyDamage(the
//      actor 0x904B34, 0x904B54) to 0x904B60's +4; 4, 5 -
//      Effect_ApplyResult; else nothing.
//   4. The member's +0x12C bit 0 with its word +0x128 at 0: +2 = 3, and done.
//   5. Unless its +0x130 bit 9: the pose BattleObj_HitPose for kind 1, for
//      kind 4 when the ability's byte +0 lacks bit 2, for kind 5 when
//      0x591810(0x904B81, 0x904B80) lacks bit 2.
//   6. +0x12C bit 0: Battle_SetDamagePopup(+0x128, Sprite_Current +5); bit 1:
//      0x453EB0(+0x12A, +5) (BE6's).
//   7. +0x90 |= 0x904B98, which is zeroed.
//   8. The s16 +0x128 above 0: with the word +0x98 (HP) at 0, +0x91 |= 0x40
//      under +0x134 bit 1, else +0x90 = 0x4000, then Battle_PlayActorCue(1)
//      unless +0x134 bit 1 and Sprite_ReleaseTint(Sprite_Current); HP left,
//      Battle_PlayActorCue(0). Either s16 +0x128 or +0x12A below 0:
//      Sound_PlayById(0x206).
//   9. Unless 0x904AA8 bit 13: Battle_PlayHitSound and Battle_SetHitPopup for
//      kind 1, and for kind 4 when the ability's word +4 has bit 11;
//      0x904AA8 &= ~0x80.
//  10. BattleObj_ScriptTick; +3 up by one.
// Field_State is read again after every call, and 0x904B35 at each test.
extern "C" void __cdecl BattleObj_HitReceive(void) {
    unsigned char* const s = S();
    unsigned char* f = F();
    SetWord(At(at::kStatusOr2), 0);
    SetWord(At(at::kStatusOr), 0);
    const unsigned char actor = s[5];
    SetPtrAt(at::kHitSprite, s);
    B(at::kHitActor) = actor;
    SetPtrAt(at::kResult, f + 0x124);
    SetWord(f + 0x124 + 4, 0);
    SetWord(PtrAt(at::kResult) + 6, 0);
    PtrAt(at::kResult)[8] = 0;
    std::memcpy(At(at::kStatCopy), f + 0xA0, 32);
    if (Word(f + 0x90) & 0x800) SetWord(At(at::kStatFlag), 1);
    if (f[0x130] & 1) {
        if (B(at::kEvadeBoost) + 0x19u >= 0x64u)
            B(at::kEvadeBoost) = 0x64;
        else
            B(at::kEvadeBoost) += 0x19;
    }
    unsigned kind = B(at::kActKind);
    if (kind == 1) {
        // the target goes out in eax with Field_State's upper bytes (the
        // register held the pointer); the actor's in ecx, whose upper bytes the
        // original had zeroed
        const U target = (Key(f) & 0xFFFFFF00u) | B(at::kHitActor);
        const short dealt = BH_CALL(Battle_ApplyDamage)(B(at::kActor), target);
        SetWord(PtrAt(at::kResult) + 4, static_cast<unsigned short>(dealt));
        kind = B(at::kActKind);
        f = F();
    } else if (kind >= 4 && kind <= 5) {
        BH_CALL(Effect_ApplyResult)();
        kind = B(at::kActKind);
        f = F();
    }
    if ((f[0x12C] & 1) && Word(f + 0x128) == 0) {
        S()[2] = 3;
        return;
    }
    if ((ULong(f + 0x130) & 0x200) == 0) {
        if (kind == 1) {
            BH_CALL(BattleObj_HitPose)();
            f = F();
        } else if (kind == 4) {
            if ((AbilityByte(at::kAbilityFlags0) & 4) == 0) {
                BH_CALL(BattleObj_HitPose)();
                f = F();
            }
        } else if (kind == 5) {
            const unsigned char cls = BH_AT(unsigned char (__cdecl*)(unsigned, unsigned), at::kItemClass)(B(at::kItemCategory),
                                                                                                        B(at::kAbility));
            if ((cls & 4) == 0) BH_CALL(BattleObj_HitPose)();
            f = F();
        }
    }
    if (f[0x12C] & 1) {
        BH_CALL(Battle_SetDamagePopup)(Word(f + 0x128), S()[5]);
        f = F();
    }
    if (f[0x12C] & 2) {
        BH_AT(void (__cdecl*)(unsigned, unsigned), at::kSecondPopup)(Word(f + 0x12A), S()[5]);
        f = F();
    }
    SetWord(f + 0x90, Word(f + 0x90) | Word(At(at::kStatusOr)));
    f = F();
    SetWord(At(at::kStatusOr), 0);
    if (SWord(f + 0x128) > 0) {
        if (Word(f + 0x98) == 0) {
            if (f[0x134] & 2)
                f[0x91] |= 0x40;
            else
                SetWord(f + 0x90, 0x4000);
            if ((F()[0x134] & 2) == 0) BH_CALL(Battle_PlayActorCue)(1);
            BH_CALL(Sprite_ReleaseTint)(S());
        } else {
            BH_CALL(Battle_PlayActorCue)(0);
        }
        f = F();
    }
    if (SWord(f + 0x128) < 0 || SWord(f + 0x12A) < 0) BH_CALL(Sound_PlayById)(0x206);
    if ((Word(At(at::kRoundFlags)) & 0x2000) == 0) {
        if (B(at::kActKind) == 1) {
            BH_CALL(Battle_PlayHitSound)();
            BH_CALL(Battle_SetHitPopup)();
        }
        if (B(at::kActKind) == 4 && (AbilityWord4() & 0x800) != 0) {
            BH_CALL(Battle_PlayHitSound)();
            BH_CALL(Battle_SetHitPopup)();
        }
        SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xFF7F);
    }
    BH_CALL(BattleObj_ScriptTick)();
    S()[3] += 1;
}

// original 0x441D50: BattleObj_HitReceiveSubs[1] and BattleObj_HpSubs[1] -
// when Sprite_ScriptTickOnce answers, +3 up by one and, unless Field_State
// +0x90 bit 2, a tail jump to BattleObj_PickPose.
extern "C" void __cdecl BattleObj_HitWaitPose(void) {
    if (BH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    S()[3] += 1;
    if (F()[0x90] & 4) return;
    BH_CALL(BattleObj_PickPose)();
}

// original 0x441D80: BattleObj_HitReceiveSubs[2] and BattleObj_HitStepSubs[4]
// (the party twin of EnemyOp_HitEnd) - BattleObj_ScriptTickOnce; then
//   +2 not 3 and Field_State +0x91 bit 6: 0x904AA8 &= ~0x40, +1..+3 = 6, 4, 0
//   (the fall), and done;
//   0x904AA8 bit 6: the word 0x904AA8 = (it & ~0x40) | 0x1000;
//   else, unless the target 0x904B44 has bit 6 or 7: 0x904AA8 |= 0x40 when
//   0x446810 (BE4's) answers for kind 1, and for kind 4 when the ability's
//   byte +5 has bit 3, 0x446810 answers and its byte +0 lacks bit 4;
// then Battle_ClearActorBit(+5); the member's +0x130 &= ~0x70;
// BattleParty_AllDown sets 0x904AE8 bit 0 (the loss); +0x130 &= ~0x200;
// +1..+3 = 2, 0, 0.
extern "C" void __cdecl BattleObj_HitEnd(void) {
    BH_CALL(BattleObj_ScriptTickOnce)();
    unsigned char* const s = S();
    if (s[2] != 3 && (F()[0x91] & 0x40)) {
        SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xFFBF);
        s[1] = 6;
        S()[2] = 4;
        S()[3] = 0;
        return;
    }
    if (B(at::kRoundFlags) & 0x40) {
        SetWord(At(at::kRoundFlags), (Word(At(at::kRoundFlags)) & 0xFFBF) | 0x1000);
    } else if ((B(at::kTarget) & 0xC0) == 0) {
        if (B(at::kActKind) == 1 && BH_AT(unsigned char (__cdecl*)(), at::kRollB9)() != 0) B(at::kRoundFlags) |= 0x40;
        if (B(at::kActKind) == 4 && (AbilityByte(at::kAbilityFlags5) & 8) != 0 &&
            BH_AT(unsigned char (__cdecl*)(), at::kRollB9)() != 0 && (AbilityByte(at::kAbilityFlags0) & 0x10) == 0)
            B(at::kRoundFlags) |= 0x40;
    }
    BH_CALL(Battle_ClearActorBit)(S()[5]);
    SetULong(F() + 0x130, ULong(F() + 0x130) & 0xFFFFFF8Fu);
    if (BH_CALL(BattleParty_AllDown)() != 0) B(at::kBattleEnd) |= 1;
    SetULong(F() + 0x130, ULong(F() + 0x130) & ~0x200u);
    S()[1] = 2;
    S()[2] = 0;
    S()[3] = 0;
}

// original 0x441EB0: BattleObj_HitSubs[3] - jmp [BattleObj_HitStepSubs + 4 * +3]
// (5: _HitStepStart, _HitStepOut, _HitStepBack, _HitStepPose, BattleObj_HitEnd).
extern "C" void __cdecl BattleObj_HitStepDispatch(void) {
    Run("BattleObj_HitStepDispatch", AddressOf(BattleObj_HitStepSubs), 5, 3);
}

// original 0x441ED0: the step's start - with Field_State +0x91 bit 6, +3 = 2
// and +0xA = 0; else the word +0x128 = 0, the velocity +0xC = 0, +0x10 =
// -0x2000 turned by the pose (0x446770, BE4's), +0xA = 4,
// BattleObj_ScriptTickOnce, Sound_PlayById(0x205), +3 up by one.
extern "C" void __cdecl BattleObj_HitStepStart(void) {
    unsigned char* const f = F();
    if (f[0x91] & 0x40) {
        S()[3] = 2;
        S()[0xA] = 0;
        return;
    }
    SetWord(f + 0x128, 0);
    SetULong(S() + 0xC, 0);
    SetULong(S() + 0x10, 0xFFFFE000u);
    BH_AT(void (__cdecl*)(unsigned char*), at::kTurnVelocityC)(S());
    S()[0xA] = 4;
    BH_CALL(BattleObj_ScriptTickOnce)();
    BH_CALL(Sound_PlayById)(0x205);
    S()[3] += 1;
}

// original 0x441F50: the step out - +0xA down by one; at 0, +0xA = 4 and +3 up
// by one; the position +0x34 / +0x38 up by the velocity; a tail jump to
// BattleObj_ScriptTickOnce.
extern "C" void __cdecl BattleObj_HitStepOut(void) {
    unsigned char* const s = S();
    s[0xA] -= 1;
    if (s[0xA] == 0) {
        s[0xA] = 4;
        s[3] += 1;
    }
    MoveBy(s, false);
    BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x441FA0: the step back - +0xA not 0: the position down by the
// velocity, +0xA down by one, BattleObj_ScriptTickOnce. At 0: with Field_State
// +0x12C bit 4, Battle_SetDamagePopup(its word +0x128, +5) and +3 up by one;
// else BattleTask_Create(0, 1) - the slot's owner +0x80 = Sprite_Current, its
// +7 = 3, +0x27 = 0 - and +3 up by one.
extern "C" void __cdecl BattleObj_HitStepBack(void) {
    unsigned char* const s = S();
    if (s[0xA] != 0) {
        MoveBy(s, true);
        s[0xA] -= 1;
        BH_CALL(BattleObj_ScriptTickOnce)();
        return;
    }
    unsigned char* const f = F();
    if (f[0x12C] & 0x10) {
        BH_CALL(Battle_SetDamagePopup)(Word(f + 0x128), s[5]);
        S()[3] += 1;
        return;
    }
    unsigned char* const t = TaskSlot("BattleObj_HitStepBack", BH_CALL(BattleTask_Create)(0, 1));
    unsigned char* const owner = S();
    SetPtrAt(Key(t + 0x80), owner);
    t[7] = 3;
    t[0x27] = 0;
    owner[3] += 1;
}

// original 0x442050: the step's pose - +3 up by one; unless Field_State +0x90
// bit 2, a tail jump to BattleObj_PickPose.
extern "C" void __cdecl BattleObj_HitStepPose(void) {
    S()[3] += 1;
    if (F()[0x90] & 4) return;
    BH_CALL(BattleObj_PickPose)();
}

// ===========================================================================
// State 6's fall (+2 = 4)
// ===========================================================================

// original 0x442080: BattleObj_HitSubs[4] - jmp [BattleObj_FallSubs + 4 * +3]
// (7: _Fall, _Revive, _ReviveEnd, _ReviveByEquip, _FallTaskDispatch,
// _ReviveByEquip, _ReviveByEquip).
extern "C" void __cdecl BattleObj_FallDispatch(void) { Run("BattleObj_FallDispatch", AddressOf(BattleObj_FallSubs), 7, 3); }

// original 0x4420A0: the member falls - Battle_ReturnQueuedItem(+5),
// Battle_RemoveFromTurnOrder(+5); then by Field_State:
//   +0x130 bit 2: +9 = 0x1E, +3 = 1 (the revive), +4 = 0, BattleObj_ScriptTick;
//   +0x134 bit 1: 0x904AA8 bit 14 cleared for this actor, +3 = 4 (the task),
//     BattleObj_ScriptTick;
//   +0x96 at 0x18: its character record's +0x16 = 0, +9 = 0x1E, +3 = 3;
//   +0x97 at 0x18: the record's +0x17 = 0, +9 = 0x1E, +3 = 6;
//   +0x95 at 0x43: the record's +0x15 = 0, +9 = 0x1E, +3 = 5 (each then
//     BattleObj_ScriptTick);
//   else: 0x904AB1 down by one; +0x138..+0x13F zeroed; +0x134 &= 0xFFFB84FF;
//     +0x12D, +0x142..+0x145 = 0; 0x453300(+5) (BE6's); BattleParty_AllDown or
//     0x904AB1 at 0 sets 0x904AE8 bit 0 (the loss); 0x904AA8 bit 14 cleared
//     for this actor; with +0x134 bit 0 +3 = 4 and BattleObj_ScriptTick; else
//     Battle_ClearActorBit(+5), +0x130 &= ~0x70 and ~0x200, +1..+3 = 3, 0, 0,
//     BattleObj_ScriptTick.
extern "C" void __cdecl BattleObj_Fall(void) {
    BH_CALL(Battle_ReturnQueuedItem)(S()[5]);
    BH_CALL(Battle_RemoveFromTurnOrder)(S()[5]);
    unsigned char* const f = F();
    if (f[0x130] & 4) {
        S()[9] = 0x1E;
        S()[3] = 1;
        S()[4] = 0;
        BH_CALL(BattleObj_ScriptTick)();
        return;
    }
    if (f[0x134] & 2) {
        ClearFormFlag();
        S()[3] = 4;
        BH_CALL(BattleObj_ScriptTick)();
        return;
    }
    struct Equip { unsigned at; unsigned char value; unsigned record_at; unsigned char next; };
    static const Equip kEquip[] = {{0x96, 0x18, 0x16, 3}, {0x97, 0x18, 0x17, 6}, {0x95, 0x43, 0x15, 5}};
    for (const Equip& q : kEquip) {
        if (f[q.at] != q.value) continue;
        CharRecord(f[0x148])[q.record_at] = 0;
        S()[9] = 0x1E;
        S()[3] = q.next;
        BH_CALL(BattleObj_ScriptTick)();
        return;
    }
    B(at::kMembersUp) -= 1;
    for (unsigned i = 0; i < 8; ++i) F()[0x138 + i] = 0;
    SetULong(F() + 0x134, ULong(F() + 0x134) & 0xFFFB84FFu);
    F()[0x12D] = 0;
    F()[0x142] = 0;
    F()[0x143] = 0;
    F()[0x144] = 0;
    F()[0x145] = 0;
    BH_AT(void (__cdecl*)(unsigned), at::kPass453300)(S()[5]);
    if (BH_CALL(BattleParty_AllDown)() != 0 || B(at::kMembersUp) == 0) B(at::kBattleEnd) |= 1;
    ClearFormFlag();
    unsigned char* const s = S();
    if (F()[0x134] & 1) {
        s[3] = 4;
        BH_CALL(BattleObj_ScriptTick)();
        return;
    }
    BH_CALL(Battle_ClearActorBit)(s[5]);
    SetULong(F() + 0x130, ULong(F() + 0x130) & 0xFFFFFF8Fu);
    SetULong(F() + 0x130, ULong(F() + 0x130) & ~0x200u);
    S()[1] = 3;
    S()[2] = 0;
    S()[3] = 0;
    BH_CALL(BattleObj_ScriptTick)();
}

// original 0x442310: the party's stats again - Char_RecalcStats(Field_State's
// character record, by its +0x148); for each member below the party count
// 0x904AB0, its +0x92..+0x97 from its record's +0x12..+0x17 and 32 bytes of
// +0xC0 from the record's +0x20; Formation_ApplyStatMods; for each member, 32
// bytes of +0xC0 to +0xA0; 0x44FDE0 (BE5's); 0x453300(member) (BE6's) for each
// member. The count is read again after each call.
extern "C" void __cdecl BattleParty_RecalcStats(void) {
    BH_CALL(Char_RecalcStats)(CharRecord(F()[0x148]));
    unsigned n = PartyCount("BattleParty_RecalcStats");
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const m = Member(i);
        const unsigned char* const r = CharRecord(m[0x148]);
        for (unsigned k = 0; k < 6; ++k) m[0x92 + k] = r[0x12 + k];
        std::memcpy(m + 0xC0, r + 0x20, 32);
    }
    BH_CALL(Formation_ApplyStatMods)();
    n = PartyCount("BattleParty_RecalcStats");
    for (unsigned i = 0; i < n; ++i) std::memmove(Member(i) + 0xA0, Member(i) + 0xC0, 32);
    BH_AT(void (__cdecl*)(), at::kPass44FDE0)();
    if (B(at::kPartyCount) == 0) return;
    unsigned member = 0;
    do {
        BH_AT(void (__cdecl*)(unsigned), at::kPass453300)(member);
        member = static_cast<unsigned char>(member + 1);
    } while (member < B(at::kPartyCount));
}

// original 0x442420: al 0 if any in-use member (+0 bit 0) of the three is up -
// its +0x90 has neither bit 2 nor 14, or its +0x130 bit 2 or +0x134 bit 1 is
// set, or with +0x90 bit 14 its +0x96 or +0x97 is 0x18 or its +0x95 0x43 (the
// states BattleObj_Fall revives from); else 1 (the party is down). The upper
// bytes of eax are the caller's.
extern "C" unsigned char __cdecl BattleParty_AllDown(void) {
    for (unsigned i = 0; i < at::kPartyMax; ++i) {
        const unsigned char* const m = Member(i);
        if ((m[0] & 1) == 0) continue;
        const unsigned w = Word(m + 0x90);
        if ((w & 0x4004) == 0) return 0;
        if (m[0x130] & 4) return 0;
        if (m[0x134] & 2) return 0;
        if ((w & 0x4000) != 0 && (m[0x96] == 0x18 || m[0x97] == 0x18 || m[0x95] == 0x43)) return 0;
    }
    return 1;
}

// original 0x4424A0: BattleObj_FallSubs[1], the revive - +9 down by one; at 0:
// Field_State +0x90 &= ~0x4000, +0x130 &= ~4, the word +0x98 = 1, +0x9C
// halved; BattleTask_Create(0, 1) - its owner Sprite_Current, +7 = 1, +0x27 =
// 6; Sprite_SetAnimation(+8 + 4); Sound_PlayById(0x206); unless 0x904AE9,
// Battle_OpenMsgWindow, 0x44A910(+5) and 0x44AA90(0, Field_State +0x89) (BE4's);
// +3 = 2; BattleObj_ScriptTick.
extern "C" void __cdecl BattleObj_Revive(void) {
    S()[9] -= 1;
    if (S()[9] != 0) return;
    SetWord(F() + 0x90, Word(F() + 0x90) & 0xBFFF);
    SetULong(F() + 0x130, ULong(F() + 0x130) & 0xFFFFFFFBu);
    SetWord(F() + 0x98, 1);
    F()[0x9C] >>= 1;
    unsigned char* const t = TaskSlot("BattleObj_Revive", BH_CALL(BattleTask_Create)(0, 1));
    unsigned char* const s = S();
    SetPtrAt(Key(t + 0x80), s);
    t[7] = 1;
    t[0x27] = 6;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(s[8] + 4));
    BH_CALL(Sound_PlayById)(0x206);
    if (B(at::kKindsSeen) == 0) {
        BH_CALL(Battle_OpenMsgWindow)();
        BH_AT(void (__cdecl*)(unsigned), at::kNameToText)(S()[5]);
        BH_AT(void (__cdecl*)(unsigned, unsigned), at::kBannerByPair)(0, F()[0x89]);
    }
    S()[3] = 2;
    BH_CALL(BattleObj_ScriptTick)();
}

// original 0x4425A0: BattleObj_FallSubs[2] - Battle_ClearActorBit(+5);
// Field_State +0x130's low byte &= 0x0F and its bit 9 cleared; +1..+3 = 3, 0, 0
// (standing again).
extern "C" void __cdecl BattleObj_ReviveEnd(void) {
    BH_CALL(Battle_ClearActorBit)(S()[5]);
    SetULong(F() + 0x130, ULong(F() + 0x130) & 0xFFFFFF0Fu);
    SetULong(F() + 0x130, ULong(F() + 0x130) & ~0x200u);
    S()[1] = 3;
    S()[2] = 0;
    S()[3] = 0;
}

// original 0x442600: BattleObj_FallSubs[3], [5], [6] - +9 down by one; at 0: by
// +3, 3 - Field_State +0x96 = 0; 5 - +0x95 = 0 and BattleParty_RecalcStats;
// 6 - +0x97 = 0 (Sprite_Current read again for each test); then +0x90 &=
// ~0x4000, the word +0x98 = its +0xA0, +0x9C = +0xAE; BattleTask_Create(0, 1)
// - owner Sprite_Current, +7 = 2, +0x27 = 6; Sprite_SetAnimation(+8 + 4);
// Sound_PlayById(0x206); unless 0x904AE9, Battle_OpenMsgWindow and
// BattleBanner_Add(2, 0, 0, 0x2D, Msg_SystemPtr(0x2E for +3 at 5, else 0x3B));
// +3 = 2; BattleObj_ScriptTick.
extern "C" void __cdecl BattleObj_ReviveByEquip(void) {
    S()[9] -= 1;
    unsigned char* s = S();
    if (s[9] != 0) return;
    if (s[3] == 3) {
        F()[0x96] = 0;
        s = S();
    }
    if (s[3] == 5) {
        F()[0x95] = 0;
        BH_CALL(BattleParty_RecalcStats)();
        s = S();
    }
    if (s[3] == 6) F()[0x97] = 0;
    SetWord(F() + 0x90, Word(F() + 0x90) & 0xBFFF);
    SetWord(F() + 0x98, Word(F() + 0xA0));
    F()[0x9C] = F()[0xAE];
    unsigned char* const t = TaskSlot("BattleObj_ReviveByEquip", BH_CALL(BattleTask_Create)(0, 1));
    s = S();
    SetPtrAt(Key(t + 0x80), s);
    t[7] = 2;
    t[0x27] = 6;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(s[8] + 4));
    BH_CALL(Sound_PlayById)(0x206);
    if (B(at::kKindsSeen) == 0) {
        BH_CALL(Battle_OpenMsgWindow)();
        const unsigned id = S()[3] == 5 ? 0x2E : 0x3B;
        const unsigned char* const text = BH_CALL(Msg_SystemPtr)(id);
        BH_CALL(BattleBanner_Add)(2, 0, 0, 0x2D, reinterpret_cast<const char*>(text));
    }
    S()[3] = 2;
    BH_CALL(BattleObj_ScriptTick)();
}

// original 0x442740: BattleObj_FallSubs[4] - jmp [BattleObj_FallTaskSubs + 4 *
// +4] (2: _FallTaskStart, _FallTaskWait).
extern "C" void __cdecl BattleObj_FallTaskDispatch(void) {
    Run("BattleObj_FallTaskDispatch", AddressOf(BattleObj_FallTaskSubs), 2, 4);
}

// original 0x442760: nothing while any member below the party count 0x904AB0
// has +0x130 bit 13; else Field_State +0x130 |= 0x2000, BattleTask_Create(0,
// 0xB for the character +0x89 at 4, else 0xC) - its owner Sprite_Current - and
// +4 up by one.
extern "C" void __cdecl BattleObj_FallTaskStart(void) {
    const unsigned n = PartyCount("BattleObj_FallTaskStart");
    for (unsigned i = 0; i < n; ++i)
        if (ULong(Member(i) + 0x130) & 0x2000) return;
    SetULong(F() + 0x130, ULong(F() + 0x130) | 0x2000);
    const unsigned kind = F()[0x89] == 4 ? 0xB : 0xC;
    unsigned char* const t = TaskSlot("BattleObj_FallTaskStart", BH_CALL(BattleTask_Create)(0, kind));
    unsigned char* const s = S();
    SetPtrAt(Key(t + 0x80), s);
    s[4] += 1;
}

// original 0x442800: nothing while Field_State +0x130 has bit 13 (the task
// clears it); then +0x130 &= ~0x70 and ~0x200, Battle_ClearActorBit(+5), +1..+4
// = 2, 0, 0, 0.
extern "C" void __cdecl BattleObj_FallTaskWait(void) {
    unsigned char* const f = F();
    const U flags = ULong(f + 0x130);
    if (flags & 0x2000) return;
    SetULong(f + 0x130, flags & 0xFFFFFF8Fu);
    SetULong(F() + 0x130, ULong(F() + 0x130) & ~0x200u);
    BH_CALL(Battle_ClearActorBit)(S()[5]);
    S()[1] = 2;
    S()[2] = 0;
    S()[3] = 0;
    S()[4] = 0;
}

// ===========================================================================
// State 6's HP change (+2 = 5)
// ===========================================================================

// original 0x442870: BattleObj_HitSubs[5] - jmp [BattleObj_HpSubs + 4 * +3] (3:
// _HpApply, BattleObj_HitWaitPose, _HpEnd).
extern "C" void __cdecl BattleObj_HpDispatch(void) { Run("BattleObj_HpDispatch", AddressOf(BattleObj_HpSubs), 3, 3); }

// original 0x442890: the change applied - with the word +0x98 (HP) at or below
// the s16 +0x128 (signed compare): +0x90 = 0x4000, +0x98 = 0,
// Battle_PlayActorCue(1); else Battle_PlayActorCue(0) for +0x128 not negative,
// Sound_PlayById(0x206) for negative, +0x98 -= +0x128, and at most +0xA0.
// Then +0x12C bit 0: Battle_SetDamagePopup(+0x128, +5); bit 1: 0x453EB0(+0x12A,
// +5) (BE6's); BattleObj_ScriptTick; +3 up by one.
extern "C" void __cdecl BattleObj_HpApply(void) {
    unsigned char* f = F();
    const int change = SWord(f + 0x128);
    if (static_cast<int>(Word(f + 0x98)) <= change) {
        SetWord(f + 0x90, 0x4000);
        SetWord(F() + 0x98, 0);
        BH_CALL(Battle_PlayActorCue)(1);
    } else {
        if (change >= 0)
            BH_CALL(Battle_PlayActorCue)(0);
        else
            BH_CALL(Sound_PlayById)(0x206);
        f = F();
        SetWord(f + 0x98, Word(f + 0x98) - Word(f + 0x128));
        f = F();
        if (Word(f + 0x98) > Word(f + 0xA0)) SetWord(f + 0x98, Word(f + 0xA0));
    }
    f = F();
    if (f[0x12C] & 1) {
        BH_CALL(Battle_SetDamagePopup)(Word(f + 0x128), S()[5]);
        f = F();
    }
    if (f[0x12C] & 2) BH_AT(void (__cdecl*)(unsigned, unsigned), at::kSecondPopup)(Word(f + 0x12A), S()[5]);
    BH_CALL(BattleObj_ScriptTick)();
    S()[3] += 1;
}

// original 0x442980: BattleObj_HpSubs[2] - BattleObj_ScriptTick; with
// Field_State +0x91 bit 6, +1..+3 = 6, 4, 0 (the fall); else
// Battle_ClearActorBit(+5) and +1..+3 = 3, 0, 0.
extern "C" void __cdecl BattleObj_HpEnd(void) {
    BH_CALL(BattleObj_ScriptTick)();
    unsigned char* const f = F();
    unsigned char* const s = S();
    if (f[0x91] & 0x40) {
        s[1] = 6;
        S()[2] = 4;
        S()[3] = 0;
        return;
    }
    BH_CALL(Battle_ClearActorBit)(s[5]);
    S()[1] = 3;
    S()[2] = 0;
    S()[3] = 0;
}

// ===========================================================================
// States 8 (sub-state 3), 10, 11 and 26
// ===========================================================================

// original 0x442C40: BattleObj_CastDoneSubs[3] - a tail jump to
// BattleObj_ScriptTick (its answer dropped by BattleObj_StateCastDone's call).
extern "C" void __cdecl BattleObj_CastDoneScript(void) { BH_CALL(BattleObj_ScriptTick)(); }

// original 0x442C70: BattleObj_StateTable[10] - jmp [BattleObj_State10Subs + 4
// * +2] (2: _State10Task, _State10End).
extern "C" void __cdecl BattleObj_State10(void) { Run("BattleObj_State10", AddressOf(BattleObj_State10Subs), 2, 2); }

// original 0x442C90: BattleTask_Create(0, 5); the slot's words +0x2E / +0x30 =
// Sprite_Current's +0x2E / +0x30 less the s8 pair at 0x6699A0 + 2 * the
// character Field_State +0x89 (each read again); +2 up by one.
extern "C" void __cdecl BattleObj_State10Task(void) {
    unsigned char* const t = TaskSlot("BattleObj_State10Task", BH_CALL(BattleTask_Create)(0, 5));
    const auto dx = static_cast<signed char>(At(at::kSpecialOffsets + 2 * F()[0x89])[0]);
    SetWord(t + 0x2E, static_cast<unsigned>(Word(S() + 0x2E) - dx) & 0xFFFF);
    const auto dy = static_cast<signed char>(At(at::kSpecialOffsets + 1 + 2 * F()[0x89])[0]);
    SetWord(t + 0x30, static_cast<unsigned>(Word(S() + 0x30) - dy) & 0xFFFF);
    S()[2] += 1;
}

// original 0x442D10: +1 = 2, +2 = 0.
extern "C" void __cdecl BattleObj_State10End(void) {
    S()[1] = 2;
    S()[2] = 0;
}

// original 0x442D30: BattleObj_StateTable[11] - BattleObj_HitPose; then by
// Field_State +0x130: bit 4 a tail jump to BattleObj_ScriptTick, else bit 5 to
// BattleObj_ScriptTickOnce.
extern "C" void __cdecl BattleObj_State11(void) {
    BH_CALL(BattleObj_HitPose)();
    const U flags = ULong(F() + 0x130);
    if (flags & 0x10)
        BH_CALL(BattleObj_ScriptTick)();
    else if (flags & 0x20)
        BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x441510 (in no start list; after BattleObj_PickPose's switch
// table): Sprite_EnsureAnimation(+8 + 0x34) with Field_State +0x91 bit 3, else
// (+8 + 0x10). Called by BattleObj_HitReceive and BattleObj_State11.
extern "C" void __cdecl BattleObj_HitPose(void) {
    if (F()[0x91] & 8)
        BH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(S()[8] + 0x34));
    else
        BH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(S()[8] + 0x10));
}

// original 0x442E40: BattleObj_StateTable[26] (the special attack: state 4's
// jump under +0x134 bit 0, and the character table's twelfth entry) - jmp
// [BattleObj_SpecialSubs + 4 * +2] (3).
extern "C" void __cdecl BattleObj_StateSpecial(void) { Run("BattleObj_StateSpecial", AddressOf(BattleObj_SpecialSubs), 3, 2); }

// original 0x442E60: BattleTask_Create(0, 0xF); the slot's first 0x80 bytes
// copied from the acting member 0x904B34's object, then its +6 = 0, +5 = 0xF,
// +1..+4 = 0, +0xB = the slot, the owner +0x80 = Sprite_Current; Sprite_Current
// +0 |= 0x40, +9 = +0xA = 0x10, +2 up by one.
extern "C" void __cdecl BattleObj_SpecialStart(void) {
    const unsigned char slot = BH_CALL(BattleTask_Create)(0, 0xF);
    unsigned char* const t = TaskSlot("BattleObj_SpecialStart", slot);
    const unsigned actor = B(at::kActor);
    if (actor >= at::kPartyMax)
        bof3::Fatal("BattleObj_SpecialStart: the actor 0x904B34 is %u, past ObjTrio's three members - the original copies "
                    "from past them (docs/battle_e3.md section 6)",
                    actor);
    std::memmove(t, Member(actor), 0x80);
    t[6] = 0;
    t[5] = 0xF;
    t[1] = 0;
    t[2] = 0;
    t[3] = 0;
    t[4] = 0;
    t[0xB] = slot;
    unsigned char* const s = S();
    SetPtrAt(Key(t + 0x80), s);
    s[0] |= 0x40;
    S()[9] = 0x10;
    S()[0xA] = 0x10;
    S()[2] += 1;
}

// original 0x442F10: +9 down by one; at 0 the cue - 3 when
// Battle_RollPendingFlag answers, else Rand() % 100 (signed) below the member's
// +0xBA (read after Rand) sets 0x904AA8 bit 7 and cue 3, otherwise cue 2 -
// then cue 4, +2 up by one. BattleObj_SwingCue without its leading tick (the
// sibling's Battle_SwingCue_Step2 says the same of the PSX twin).
extern "C" void __cdecl BattleObj_SpecialCue(void) {
    S()[9] -= 1;
    if (S()[9] != 0) return;
    unsigned cue;
    if (BH_CALL(Battle_RollPendingFlag)() != 0) {
        cue = 3;
    } else {
        const int roll = static_cast<int>(BH_CALL(Rand)()) % 100;
        if (static_cast<int>(F()[0xBA]) > roll) {
            B(at::kRoundFlags) |= 0x80;
            cue = 3;
        } else {
            cue = 2;
        }
    }
    BH_CALL(Battle_PlayActorCue)(cue);
    BH_CALL(Battle_PlayActorCue)(4);
    S()[2] += 1;
}

// original 0x442F80: once Sprite_Current +0 has lost bit 6 (the task clears
// it), 0x904AA8 |= 4 and a tail jump to BattleObj_EndAction.
extern "C" void __cdecl BattleObj_SpecialWait(void) {
    if (S()[0] & 0x40) return;
    B(at::kRoundFlags) |= 4;
    BH_CALL(BattleObj_EndAction)();
}

void BattleE3_Inject() {
    if (bof3::WantsShadow("battle_e3")) battle_e3::SelfTest();
    BOF3_INJECT(EnemyOp_CastDispatch);
    BOF3_INJECT(EnemyOp_CastStart);
    BOF3_INJECT(EnemyOp_CastCue);
    BOF3_INJECT(EnemyOp_CastDoneDispatch);
    BOF3_INJECT(EnemyOp_CastDoneCost);
    BOF3_INJECT(EnemyOp_CastDoneTickUnless);
    BOF3_INJECT(EnemyOp_CastDoneCheck);
    BOF3_INJECT(EnemyOp_LeaveDispatch);
    BOF3_INJECT(EnemyOp_LeaveStart);
    BOF3_INJECT(EnemyOp_LeaveStep);
    BOF3_INJECT(EnemyOp_LeaveEnd);
    BOF3_INJECT(Sound_PlayEffectUnlessNone);
    BOF3_INJECT(EnemyOp_EndAction);
    BOF3_INJECT(EnemyOp_RollBit80Task);
    BOF3_INJECT(Fixed_HighRoundUp);
    BOF3_INJECT(BattleObj_HitPose);
    BOF3_INJECT(BattleObj_StateHit);
    BOF3_INJECT(BattleObj_HitEnter);
    BOF3_INJECT(BattleObj_HitReceiveDispatch);
    BOF3_INJECT(BattleObj_HitReceive);
    BOF3_INJECT(BattleObj_HitWaitPose);
    BOF3_INJECT(BattleObj_HitEnd);
    BOF3_INJECT(BattleObj_HitStepDispatch);
    BOF3_INJECT(BattleObj_HitStepStart);
    BOF3_INJECT(BattleObj_HitStepOut);
    BOF3_INJECT(BattleObj_HitStepBack);
    BOF3_INJECT(BattleObj_HitStepPose);
    BOF3_INJECT(BattleObj_FallDispatch);
    BOF3_INJECT(BattleObj_Fall);
    BOF3_INJECT(BattleParty_RecalcStats);
    BOF3_INJECT(BattleParty_AllDown);
    BOF3_INJECT(BattleObj_Revive);
    BOF3_INJECT(BattleObj_ReviveEnd);
    BOF3_INJECT(BattleObj_ReviveByEquip);
    BOF3_INJECT(BattleObj_FallTaskDispatch);
    BOF3_INJECT(BattleObj_FallTaskStart);
    BOF3_INJECT(BattleObj_FallTaskWait);
    BOF3_INJECT(BattleObj_HpDispatch);
    BOF3_INJECT(BattleObj_HpApply);
    BOF3_INJECT(BattleObj_HpEnd);
    BOF3_INJECT(BattleObj_CastDoneScript);
    BOF3_INJECT(BattleObj_State10);
    BOF3_INJECT(BattleObj_State10Task);
    BOF3_INJECT(BattleObj_State10End);
    BOF3_INJECT(BattleObj_State11);
    BOF3_INJECT(BattleObj_StateSpecial);
    BOF3_INJECT(BattleObj_SpecialStart);
    BOF3_INJECT(BattleObj_SpecialCue);
    BOF3_INJECT(BattleObj_SpecialWait);
}
