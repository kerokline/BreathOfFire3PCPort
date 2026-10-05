// Round fourteen group R3B (docs/takeover-queue-round14.md, wave three;
// analysis/round14_cut.tsv's group column): the battle engine's band
// 0x4468B0..0x44CFF4, each function read to its last instruction with
// capstone (2026-10-04) and taken through the boss harness's engine frame
// (boss_harness.h, docs/boss_harness.md section 10). docs/rest_3b.md has
// every function one row each.
//
//   - the result screen's EXP: BattleResult_AddExp (PSX 0x801DD564, the
//     sibling's name, verified), the member test it runs per slot, and
//     CharId_ToRosterIndex (PSX 0x801DD774, the sibling's, verified);
//   - three percent clamps, (value * percent) / 100 within 0..999, 0..9999
//     and 0..100 (the stat rebuilds' and the transformation's);
//   - the command menus' steps no earlier group took: BattleTarget_Picks' 2
//     and 4, BattleItem_Steps' 2 and 5 (a dispatcher, with its table
//     0x64E43C named here) and that table's three, BattleAttackCmd_States' 3,
//     BattleItemCmd_States' 2, 5, 6 (two dispatchers) and the target cancel,
//     and Battle_MenuSteps' 7 (the escape's dispatcher);
//   - 45 Effect_Handlers slots (0..49 less slots 8, 22, 23, 31 and 38, which
//     are battle_odds' and battle_e5's): void (void), as Effect_ApplyResult
//     calls them, filling the result record *0x904B60 (+4 the HP delta,
//     positive is damage; +6 the AP delta; +8 flags).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// four dispatchers abort past their tables where the original jumps through
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6). The handlers index the party and the enemies by an actor or a
// target byte unchecked, as the original does (docs/rest_3b.md section 5):
// ours reads and writes exactly where the original would. Every call goes
// through the harness (BH_CALL / BH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/rest_3b.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/rest_3b_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3b::at;
using U = std::uint32_t;
using S32 = std::int32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
std::uint16_t W(U address) { return Word(At(address)); }
U L(U address) { return static_cast<U>(Long(At(address))); }
S32 S16(std::uint16_t v) { return static_cast<std::int16_t>(v); }
U Key(const unsigned char* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Ptr(U cell) { return At(L(cell)); }
unsigned char* Result() { return Ptr(at::kResult); }

// The records as the originals index them: a party record by the byte
// itself (0x802D40 + 0x14C n, any n), an enemy object by the byte less 3
// (0x93B960 + 0x128 (n - 3), the difference in 32 bits - a byte below 3
// lands before the enemies). Neither is checked, as in the original.
unsigned char* Party(unsigned n) { return At(at::kParty + (n & 0xFF) * at::kPartyStride); }
unsigned char* Enemy(unsigned n) { return At(at::kEnemies + static_cast<U>(static_cast<S32>(n & 0xFF) - 3) * at::kEnemyStride); }
unsigned char* CharRecord(unsigned roster) { return At(at::kCharRecords + (roster & 0xFF) * at::kCharStride); }
unsigned char* AbilityRow(unsigned id) { return At(at::kAbilityRows + (id & 0xFFFF) * at::kAbilityStride); }

// R3D's callees, ours, by address (named in rest_3b_callees.h).
using Void0 = void (__cdecl*)();
using U1 = U (__cdecl*)(U);
using U2 = U (__cdecl*)(U, U);
using Handler = void (__cdecl*)();

void MissTail() { BH_AT(Void0, at::kMissTail)(); }
void StatMod(U which) { BH_AT(U1, at::kStatMod)(which); }
void InflictMiss(U status) { BH_AT(U1, at::kInflictMiss)(status); }
void Inflict(U status) { BH_AT(U1, at::kInflict)(status); }

// jmp [table + 4 * byte]: the byte below `entries`, else a Fatal where the
// original jumps through the dword after its table.
void Dispatch(const char* who, U cell, U table, unsigned entries) {
    const unsigned state = B(cell);
    if (state >= entries)
        bof3::Fatal("%s: the state byte 0x%X is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/rest_3b.md section 5)",
                    who, (unsigned)cell, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * state)))();
}

// The power word Battle_CalcDamage reads, from the acting actor's record:
// a member's word at `party_at`, an enemy's at `enemy_at` (the actor the
// dword 0x904B34's low byte, below 3 a member).
std::uint16_t ActorWord(unsigned actor, U party_at, U enemy_at) {
    return actor < 3 ? Word(Party(actor) + party_at) : Word(Enemy(actor) + enemy_at);
}

// Battle_CalcDamage(actor, target, element): the two bytes read here, the
// answer's word into the result record's HP delta (the pointer read after the
// call).
std::uint16_t Hit(unsigned element) {
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    const short d = BH_CALL(Battle_CalcDamage)(actor, target, element);
    SetWord(Result() + 4, static_cast<std::uint16_t>(d));
    return static_cast<std::uint16_t>(d);
}

}  // namespace

// ===========================================================================
// The result screen's EXP
// ===========================================================================

// original 0x4469D0 (PSX CharId_ToRosterIndex 0x801DD774, the sibling's
// name; read): the byte at 0x66972C + the id's low byte, 7 answered as 0.
// eax is the byte zero-extended (the callee's `and eax, 0xFF` before the
// load); BattleResult_FindLevelUp passes the whole of it on.
extern "C" unsigned __cdecl CharId_ToRosterIndex(unsigned id) {
    const unsigned r = B(at::kRosterOf + (id & 0xFF));
    return r == 7 ? 0u : r;
}

// original 0x446990: 0 when Battle_ActorIsOut(slot) (the word pushed whole,
// the callee reads its byte); else 1 when party record (slot & 0xFF)'s
// dword +0x134 has bit 10 clear. al only (the caller tests it).
extern "C" unsigned char __cdecl BattleResult_MemberTakesExp(unsigned slot) {
    if (BH_CALL(Battle_ActorIsOut)(slot) & 0xFF) return 0;
    return static_cast<unsigned char>(((L(Key(Party(slot)) + 0x134) >> 10) & 1) ^ 1);
}

// original 0x4468B0 (PSX BattleResult_AddExp 0x801DD564, the sibling's
// name; read): for each party slot below the count 0x904AB0 (read again each
// pass) that BattleResult_MemberTakesExp answers for: the slot's character
// (+0x89) to its roster index (CharId_ToRosterIndex, read again for each of
// its calls), that character record's EXP dword (+0xC) + exp below 9999999
// (unsigned): exp added to the record the second call names; else that
// record's EXP set to 9999999. The slot counter is a byte stored over the
// caller's ecx slot: the word handed to the member test carries those upper
// bytes (masked by the callee). Answers exp (eax, on both paths).
extern "C" unsigned __cdecl BattleResult_AddExp(unsigned exp) {
    for (unsigned slot = 0; (slot & 0xFF) < B(at::kPartyCount); slot = (slot + 1) & 0xFF) {
        const unsigned char takes = BH_CALL(BattleResult_MemberTakesExp)(slot);
        if (takes == 0) continue;
        const unsigned r1 = BH_CALL(CharId_ToRosterIndex)(Party(slot)[0x89]) & 0xFF;
        const U sum = L(Key(CharRecord(r1)) + 0xC) + exp;
        if (sum < 9999999u) {
            const unsigned r2 = BH_CALL(CharId_ToRosterIndex)(Party(slot)[0x89]) & 0xFF;
            unsigned char* const e = CharRecord(r2) + 0xC;
            SetLong(e, static_cast<S32>(static_cast<U>(Long(e)) + exp));
        } else {
            const unsigned r3 = BH_CALL(CharId_ToRosterIndex)(Party(slot)[0x89]) & 0xFF;
            SetLong(CharRecord(r3) + 0xC, 9999999);
        }
    }
    return exp;
}

// ===========================================================================
// The percent clamps
// ===========================================================================

namespace {
// (value * percent) / 100: the product in 32 bits (wrapping), the division
// signed and truncated (imul 0x51EB851F, sar 5, + the sign bit); above `cap`
// the cap, below 0 zero.
int Percent(int value, int percent, int cap) {
    const S32 q = static_cast<S32>(static_cast<U>(value) * static_cast<U>(percent)) / 100;
    if (q > cap) return cap;
    return q < 0 ? 0 : q;
}
}  // namespace

// original 0x446F20 (PSX 0x801DE074, paired; unnamed there): clamped to 0..999.
extern "C" int __cdecl Stat_PercentCap999(int value, int percent) { return Percent(value, percent, 999); }
// original 0x446F50 (PSX 0x801DE0C0): clamped to 0..9999.
extern "C" int __cdecl Stat_PercentCap9999(int value, int percent) { return Percent(value, percent, 9999); }
// original 0x446F80 (PSX 0x801DE10C): clamped to 0..100.
extern "C" int __cdecl Stat_PercentCap100(int value, int percent) { return Percent(value, percent, 100); }

// ===========================================================================
// The command menus' steps
// ===========================================================================

// original 0x447290 (BattleTarget_Picks entry 2): the pick among the party,
// BattleTarget_PickEnemy's twin. Input_Pressed read once: cancel -> sub-state
// 4, confirm -> 3. Else Input_AutoRepeat(it & 0xF000): 0x5000 back to the
// enemies (Battle_DefaultTarget(3) into the command's +0, the sub-state one
// down, cue 0x101) and done; else 0x2000 Battle_DefaultTarget(
// Battle_WrapIndex(count - 1, 0, target + 1)), and 0x8000
// Battle_PrevTarget(Battle_WrapIndex(count - 1, 0, target - 1)), each into
// the command's +0 (the pointer read again after the calls) with cue 0x101;
// both may run. The count is the byte 0x904AB0 (the party's), the target the
// command's +0 signed.
extern "C" void __cdecl BattleTarget_PickParty(void) {
    const unsigned pressed = Input_Pressed;
    if (Field_CancelButtons & pressed) {
        B(at::kStep4) = 4;
        return;
    }
    if (Field_ConfirmButtons & pressed) {
        B(at::kStep4) = 3;
        return;
    }
    const U held = BH_CALL(Input_AutoRepeat)(pressed & 0xF000);
    if (held & 0x5000) {
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(3);
        Ptr(at::kCommand)[0] = t;
        B(at::kStep4) = static_cast<unsigned char>(B(at::kStep4) - 1);
        BH_CALL(Sound_PlayEffect)(0x101);
        return;
    }
    if (held & 0x2000) {
        const long value = static_cast<signed char>(Ptr(at::kCommand)[0]) + 1;
        const long high = static_cast<long>(B(at::kPartyCount)) - 1;
        const long wrapped = BH_CALL(Battle_WrapIndex)(high, 0, value);
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(static_cast<unsigned>(wrapped));
        Ptr(at::kCommand)[0] = t;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
    if (held & 0x8000) {
        const long value = static_cast<signed char>(Ptr(at::kCommand)[0]) - 1;
        const long high = static_cast<long>(B(at::kPartyCount)) - 1;
        const long wrapped = BH_CALL(Battle_WrapIndex)(high, 0, value);
        const unsigned char t = BH_CALL(Battle_PrevTarget)(static_cast<unsigned>(wrapped));
        Ptr(at::kCommand)[0] = t;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
}

namespace {
// The cancels' common head: cue 0x106, then (for the two that close a
// window) records 3's and 2's +3 = 1, then BattleBanner_ShowName(the menu
// actor, the pointer read after the cue) and the pick flag cleared.
void CancelHead(bool windows) {
    BH_CALL(Sound_PlayEffect)(0x106);
    if (windows) {
        B(at::kWin3State) = 1;
        B(at::kWin2State) = 1;
    }
    BH_CALL(BattleBanner_ShowName)(Ptr(at::kMenuActor));
    B(at::kPicking) = 0;
}
}  // namespace

// original 0x4473E0 (BattleTarget_Picks entry 4): the pick cancelled - back
// to the command menu: step 2, the command, step and sub-state 0.
extern "C" void __cdecl BattleTarget_Cancel(void) {
    CancelHead(true);
    B(at::kStep1) = 2;
    B(at::kStep2) = 0;
    B(at::kStep3) = 0;
    B(at::kStep4) = 0;
}

// original 0x448140 (BattleAttackCmd_States entry 3): the attack's pick
// cancelled - as BattleTarget_Cancel, the sub-state left as it is.
extern "C" void __cdecl BattleAttackCmd_Cancel(void) {
    CancelHead(true);
    B(at::kStep1) = 2;
    B(at::kStep2) = 0;
    B(at::kStep3) = 0;
}

namespace {
// The closing waits (BattleItem_Steps' 2 and BattleItemCmd_States' 2): once
// record 16's +3 is 0, step 1, the command and step 0, and a tail jump to
// ItemMenu_FreeWindows.
void CloseWait() {
    if (B(at::kWin16Closing) != 0) return;
    B(at::kStep1) = 1;
    B(at::kStep2) = 0;
    B(at::kStep3) = 0;
    BH_CALL(ItemMenu_FreeWindows)();
}
}  // namespace

// original 0x447880 (BattleItem_Steps entry 2).
extern "C" void __cdecl BattleItem_CloseWait(void) { CloseWait(); }
// original 0x448600 (BattleItemCmd_States entry 2).
extern "C" void __cdecl BattleItemCmd_CloseWait(void) { CloseWait(); }

// original 0x447D30 (BattleItem_TargetSteps entry 4, and 0x64E43C's entry
// 3): the ability's pick cancelled - back to the list: cue 0x106, the
// actor's name, the pick flag cleared, step 4, the command 1, step 0,
// sub-state 1.
extern "C" void __cdecl BattleItem_TargetCancel(void) {
    CancelHead(false);
    B(at::kStep1) = 4;
    B(at::kStep2) = 1;
    B(at::kStep3) = 0;
    B(at::kStep4) = 1;
}

// original 0x448B40 (BattleItemCmd_TargetSteps entry 4, BattleItemCmd_SideSteps
// entry 3): the item's pick cancelled - step 4, the command 2, step 0,
// sub-state 1.
extern "C" void __cdecl BattleItemCmd_TargetCancel(void) {
    CancelHead(false);
    B(at::kStep1) = 4;
    B(at::kStep2) = 2;
    B(at::kStep3) = 0;
    B(at::kStep4) = 1;
}

// original 0x447D70 (BattleItem_Steps entry 5): jmp [0x64E43C + 4 * (dword
// 0x904AA4 & 0xFF)] - BattleItem_SideSteps' four.
extern "C" void __cdecl BattleItem_SideDispatch(void) {
    Dispatch("BattleItem_SideDispatch", at::kStep4, at::kItemSideSteps, 4);
}

// original 0x448B80 (BattleItemCmd_States entry 5): through BattleItemCmd_SideSteps (4).
extern "C" void __cdecl BattleItemCmd_SideDispatch(void) {
    Dispatch("BattleItemCmd_SideDispatch", at::kStep4, at::kItemCmdSideSteps, 4);
}

// original 0x448C80 (BattleItemCmd_States entry 6): through BattleItemCmd_EquipSteps (4).
extern "C" void __cdecl BattleItemCmd_EquipDispatch(void) {
    Dispatch("BattleItemCmd_EquipDispatch", at::kStep4, at::kItemCmdEquipSteps, 4);
}

// original 0x44A000 (Battle_MenuSteps entry 7; PSX 0x8009823C, paired): jmp
// [0x64E4FC + 4 * byte 0x904AA3] - Escape_States' three.
extern "C" void __cdecl Escape_Dispatch(void) { Dispatch("Escape_Dispatch", at::kStep3, at::kEscapeStates, 3); }

// original 0x447D90 (BattleItem_SideSteps entry 0): a whole side as the
// target, by byte +0 of the command's word +2's NameTable_Abilities row: bit
// 0x20 the enemies (0x40), else the party (0x80); sub-state 1, the repeat
// latch 0, the pick flag set. BattleItemCmd_SideBegin's twin.
extern "C" void __cdecl BattleItem_SideBegin(void) {
    unsigned char* const c = Ptr(at::kCommand);
    const unsigned char flags = AbilityRow(Word(c + 2))[0];
    c[0] = static_cast<unsigned char>((flags & 0x20) ? 0x40 : 0x80);
    B(at::kStep4) = 1;
    SetWord(At(at::kRepeatLatch), 0);
    B(at::kPicking) = 1;
}

// original 0x447DD0 (BattleItem_SideSteps entry 1): the side pick. Cancel ->
// sub-state 3, confirm -> 2. Else a direction Input_AutoRepeat(Input_Pressed
// & 0xF000) lets through (0xF000), with byte +0 bit 0x80 of the ability
// under the window's cursor - Char_AbilityList(byte 0x929F06, record 16's
// +0xB, 1)[record 16's dword +0xC & 0xFF] - flips the side (the command's +0
// ^= 0xC0) with cue 0x101. BattleItemCmd_SidePick's twin.
extern "C" void __cdecl BattleItem_SidePick(void) {
    const unsigned pressed = Input_Pressed;
    if (Field_CancelButtons & pressed) {
        B(at::kStep4) = 3;
        return;
    }
    if (Field_ConfirmButtons & pressed) {
        B(at::kStep4) = 2;
        return;
    }
    const U held = BH_CALL(Input_AutoRepeat)(pressed & 0xF000);
    if ((held & 0xF000) == 0) return;
    const unsigned char* const list = BH_CALL(Char_AbilityList)(B(at::kAbilityActor), B(at::kWin16Page), 1);
    const unsigned id = list[L(at::kWin16Cursor) & 0xFF];
    if ((AbilityRow(id)[0] & 0x80) == 0) return;
    unsigned char* const c = Ptr(at::kCommand);
    c[0] = static_cast<unsigned char>(c[0] ^ 0xC0);
    BH_CALL(Sound_PlayEffect)(0x101);
}

// ===========================================================================
// Effect_Handlers slots 0..49 (less 8, 22, 23, 31, 38)
// ===========================================================================
//
// Each is called by Effect_ApplyResult through Effect_Handlers 0x64E73C with no
// argument, its eax not read. "The actor" is the byte 0x904B34, "the target"
// the byte 0x904B54, "the result" the record *0x904B60 (read again after each
// call); a member's record 0x802D40 + 0x14C n, an enemy's object 0x93B960 +
// 0x128 (n - 3).

// original 0x44BED0 (slot 0): a tail jump to 0x44FB30, the miss tail.
extern "C" void __cdecl EffectSlot00_Miss(void) { MissTail(); }

// original 0x44BEE0 (slot 1): the power word 0x939FE4 = the actor's +0xA4
// (a member) or +0xB4 (an enemy); Rand & 3: 0 halves it, 3 adds its half
// (16 bits), 1 and 2 leave it; then the HP delta Battle_CalcDamage(actor,
// target, 0xFFFF).
extern "C" void __cdecl EffectSlot01_VariedHit(void) {
    SetWord(At(at::kPower), ActorWord(B(at::kActor), 0xA4, 0xB4));
    const U roll = static_cast<U>(BH_CALL(Rand)()) & 3;
    if (roll == 0) {
        SetWord(At(at::kPower), W(at::kPower) >> 1);
    } else if (roll == 3) {
        const std::uint16_t p = W(at::kPower);
        SetWord(At(at::kPower), static_cast<std::uint16_t>(p + (p >> 1)));
    }
    Hit(0xFFFF);
}

// original 0x44BF70 (slot 2): the HP delta Battle_CalcDamage(actor, target,
// 4); then (the target read again) when the delta, signed, is below the
// target's HP (+0x98 a member, +0xA4 an enemy) and not 0: 0x44FCA0(4).
extern "C" void __cdecl EffectSlot02_HitInflict4(void) {
    Hit(4);
    const unsigned t = B(at::kTarget);
    const S32 hp = t < 3 ? Word(Party(t) + 0x98) : Word(Enemy(t) + 0xA4);
    const std::uint16_t d = Word(Result() + 4);
    if (S16(d) < hp && d != 0) Inflict(4);
}

namespace {
// Half a hit: Battle_CalcDamage(actor, target, 0xFFFF) / 2 (signed, toward
// 0) as the HP delta; when that (read again) is not 0, 0x44FCA0(status).
void HalfHitInflict(U status) {
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    const short d = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<std::uint16_t>(static_cast<S32>(d) / 2));
    if (Word(Result() + 4) != 0) Inflict(status);
}

// Effect_SkillDamage(actor, target, power, 0) as the HP delta.
void SkillHit(unsigned power) {
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    const int d = BH_CALL(Effect_SkillDamage)(actor, target, power, 0);
    SetWord(Result() + 4, static_cast<std::uint16_t>(d));
}
}  // namespace

// original 0x44BFF0 (slot 3): half a hit, then 0x44FCA0(0x20).
extern "C" void __cdecl EffectSlot03_HalfHitInflict20(void) { HalfHitInflict(0x20); }

// original 0x44C040 (slot 4; R3C's slots 50, 51, 52, 55, 56, 58, 60 and 0x44D680,
// and slots 44, 45, 46, 49 below, jump here after setting the ability): not in
// the cut. Effect_SkillDamage(actor, target, the ability's power byte
// (NameTable_Abilities +3), 0) as the HP delta.
extern "C" void __cdecl EffectSlot04_SkillPower(void) {
    const unsigned ability = L(at::kAbility) & 0xFFFF;
    const unsigned target = B(at::kTarget);
    const unsigned power = AbilityRow(ability)[3];
    const unsigned actor = B(at::kActor);
    const int d = BH_CALL(Effect_SkillDamage)(actor, target, power, 0);
    SetWord(Result() + 4, static_cast<std::uint16_t>(d));
}

namespace {
// Battle_ClearStatus(target, mask), then a tail jump to 0x44FB30 (slots 5, 13..16).
void ClearThenMiss(U mask) {
    BH_CALL(Battle_ClearStatus)(B(at::kTarget), mask);
    MissTail();
}
}  // namespace

// original 0x44C080 (slot 5): Battle_ClearStatus(target, 0x80), the miss tail.
extern "C" void __cdecl EffectSlot05_Clear80(void) { ClearThenMiss(0x80); }

// original 0x44C0A0 (slot 6): when the actor's party record (by the actor
// byte, whichever side) has character +0x89 0x0A: 0x44FB30, then the HP
// delta 9999. Else 0x44FCE0(3) less the target's enemy object's +0xB6 (by
// the target byte, whichever side; 16 bits). Then a negative delta is 0.
extern "C" void __cdecl EffectSlot06_HpThirdHit(void) {
    if (Party(B(at::kActor))[0x89] == 0x0A) {
        MissTail();
        SetWord(Result() + 4, 9999);
    } else {
        const U share = BH_AT(U1, at::kHpShare)(3);
        const unsigned t = B(at::kTarget);
        SetWord(Result() + 4, static_cast<std::uint16_t>(share - Word(Enemy(t) + 0xB6)));
    }
    if (S16(Word(Result() + 4)) < 0) SetWord(Result() + 4, 0);
}

// original 0x44C120 (slot 7; R3C's 0x44D120 jumps here): not in the cut.
// Effect_HealAmount(actor, target) as the HP delta.
extern "C" void __cdecl EffectSlot07_Heal(void) {
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    const int d = BH_CALL(Effect_HealAmount)(actor, target);
    SetWord(Result() + 4, static_cast<std::uint16_t>(d));
}

// original 0x44C150 (slot 9): the HP delta -40.
extern "C" void __cdecl EffectSlot09_Heal40(void) { SetWord(Result() + 4, 0xFFD8); }
// original 0x44C160 (slot 10): the HP delta -100.
extern "C" void __cdecl EffectSlot10_Heal100(void) { SetWord(Result() + 4, 0xFF9C); }

// original 0x44C170 (slot 11; R3C's 0x44D6A0 jumps here): not in the cut. A
// member target: the HP delta minus its maximum HP (+0xA0). An enemy: its
// +0xB0 0xFFFF gives the delta 0xFFFF and its +0x10C |= 4 (the target read
// again); else minus the +0xB0.
extern "C" void __cdecl EffectSlot11_HealFull(void) {
    const unsigned t = B(at::kTarget);
    if (t < 3) {
        SetWord(Result() + 4, static_cast<std::uint16_t>(0u - Word(Party(t) + 0xA0)));
        return;
    }
    const std::uint16_t max = Word(Enemy(t) + 0xB0);
    if (max == 0xFFFF) {
        SetWord(Result() + 4, 0xFFFF);
        unsigned char* const e = Enemy(B(at::kTarget));
        e[0x10C] = static_cast<unsigned char>(e[0x10C] | 4);
        return;
    }
    SetWord(Result() + 4, static_cast<std::uint16_t>(0u - max));
}

// original 0x44C1F0 (slot 12): the HP delta -5; then (Rand & 0x7F) at most
// 0x26: Battle_ClearStatus(target, 0x68).
extern "C" void __cdecl EffectSlot12_Heal5Clear68(void) {
    SetWord(Result() + 4, 0xFFFB);
    const U roll = static_cast<U>(BH_CALL(Rand)()) & 0x7F;
    if (roll <= 0x26) BH_CALL(Battle_ClearStatus)(B(at::kTarget), 0x68);
}

// original 0x44C220 (slot 13): as slot 5 (the same bytes).
extern "C" void __cdecl EffectSlot13_Clear80(void) { ClearThenMiss(0x80); }
// original 0x44C240 (slot 14): Battle_ClearStatus(target, 8), the miss tail.
extern "C" void __cdecl EffectSlot14_Clear8(void) { ClearThenMiss(8); }
// original 0x44C260 (slot 15): ... 0x100.
extern "C" void __cdecl EffectSlot15_Clear100(void) { ClearThenMiss(0x100); }
// original 0x44C280 (slot 16): ... 0xBFC.
extern "C" void __cdecl EffectSlot16_ClearBFC(void) { ClearThenMiss(0xBFC); }

// original 0x44C2A0 (slot 17): a target whose status word (+0x90 a member,
// +0x92 an enemy) has 0x4000 (the bit Battle_ActorIsOut reads as out): the HP
// delta 0xFFFF, Battle_ClearStatus(target, 0x4000), then (the target read
// again) 0x904AB1 (a member) or 0x904AB3 (an enemy) one up. Else the delta 0.
extern "C" void __cdecl EffectSlot17_RaiseDown(void) {
    const unsigned t = B(at::kTarget);
    const std::uint16_t status = t <= 2 ? Word(Party(t) + 0x90) : Word(Enemy(t) + 0x92);
    if ((status & 0x4000) == 0) {
        SetWord(Result() + 4, 0);
        return;
    }
    SetWord(Result() + 4, 0xFFFF);
    BH_CALL(Battle_ClearStatus)(B(at::kTarget), 0x4000);
    if (B(at::kTarget) <= 2) B(at::kPartyUp) = static_cast<unsigned char>(B(at::kPartyUp) + 1);
    else B(at::kEnemiesLeft) = static_cast<unsigned char>(B(at::kEnemiesLeft) + 1);
}

// original 0x44C330 (slot 18): half a hit, then 0x44FCA0(0x80).
extern "C" void __cdecl EffectSlot18_HalfHitInflict80(void) { HalfHitInflict(0x80); }

// original 0x44C380 (slot 19): 0x44FBB0(0).
extern "C" void __cdecl EffectSlot19_StatMod0(void) { StatMod(0); }

// original 0x44C390 (slot 20): the result's +8 = 2, the AP delta -20.
extern "C" void __cdecl EffectSlot20_ApHeal20(void) {
    Result()[8] = 2;
    SetWord(Result() + 6, 0xFFEC);
}
// original 0x44C3B0 (slot 21): the result's +8 = 2, the AP delta -100.
extern "C" void __cdecl EffectSlot21_ApHeal100(void) {
    Result()[8] = 2;
    SetWord(Result() + 6, 0xFF9C);
}

// original 0x44C7C0 (slot 24): 0x44FBB0(1).
extern "C" void __cdecl EffectSlot24_StatMod1(void) { StatMod(1); }
// original 0x44C7D0 (slot 25): 0x44FBB0(2).
extern "C" void __cdecl EffectSlot25_StatMod2(void) { StatMod(2); }
// original 0x44C7E0 (slot 26): 0x44FC60(0x40).
extern "C" void __cdecl EffectSlot26_Inflict40(void) { InflictMiss(0x40); }
// original 0x44C7F0 (slot 27): 0x44FC60(0x20).
extern "C" void __cdecl EffectSlot27_Inflict20(void) { InflictMiss(0x20); }

// original 0x44C800 (slot 28): a member target only. Its status word +0x90
// read; then Rand & 3 not 0 and the word's 0x4000: (the target read again)
// the HP delta minus a quarter of its maximum (a member's +0xA0, an enemy's
// +0xB0: negated, then shifted right 2, signed), 0x904AB1 / 0x904AB3 one up,
// and Battle_ClearStatus(target, 0x4000). Else the result's +8 = 1.
extern "C" void __cdecl EffectSlot28_RaiseQuarter(void) {
    if (B(at::kTarget) > 2) return;
    const std::uint16_t status = Word(Party(B(at::kTarget)) + 0x90);
    const U roll = static_cast<U>(BH_CALL(Rand)());
    if ((roll & 3) == 0 || (status & 0x4000) == 0) {
        Result()[8] = 1;
        return;
    }
    const unsigned t = B(at::kTarget);
    if (t <= 2) {
        const S32 max = Word(Party(t) + 0xA0);
        SetWord(Result() + 4, static_cast<std::uint16_t>((0 - max) >> 2));
        B(at::kPartyUp) = static_cast<unsigned char>(B(at::kPartyUp) + 1);
    } else {
        const S32 max = Word(Enemy(t) + 0xB0);
        SetWord(Result() + 4, static_cast<std::uint16_t>((0 - max) >> 2));
        B(at::kEnemiesLeft) = static_cast<unsigned char>(B(at::kEnemiesLeft) + 1);
    }
    BH_CALL(Battle_ClearStatus)(B(at::kTarget), 0x4000);
}

// original 0x44C8E0 (slot 29): a member target only. Its +0x91 bit 0x40
// (status 0x4000): the HP delta -10000, 0x904AB1 one up (stored before the
// call), Battle_ClearStatus(target, 0x4000). Else the delta 0.
extern "C" void __cdecl EffectSlot29_RaiseFull(void) {
    if (B(at::kTarget) > 2) return;
    if ((Party(B(at::kTarget))[0x91] & 0x40) == 0) {
        SetWord(Result() + 4, 0);
        return;
    }
    SetWord(Result() + 4, 0xD8F0);
    const unsigned char up = static_cast<unsigned char>(B(at::kPartyUp) + 1);
    const unsigned t = B(at::kTarget);
    B(at::kPartyUp) = up;
    BH_CALL(Battle_ClearStatus)(t, 0x4000);
}

// original 0x44C940 (slot 30): half a hit, then 0x44FCA0(8).
extern "C" void __cdecl EffectSlot30_HalfHitInflict8(void) { HalfHitInflict(8); }

// original 0x44C9C0 (slot 32): 0x904AA9 |= 0x20; the HP delta
// Battle_CalcDamage(actor, target, 0xFFFF) shifted right 1 (16 bits,
// signed), plus 1.
extern "C" void __cdecl EffectSlot32_HalfHitPlus1(void) {
    const unsigned target = B(at::kTarget);
    const unsigned actor = B(at::kActor);
    B(at::kRoundFlags2) = static_cast<unsigned char>(B(at::kRoundFlags2) | 0x20);
    const short d = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<std::uint16_t>((d >> 1) + 1));
}

// original 0x44C9F0 (slot 33): 0x904AA9 |= 0x20; the power word = the
// actor's +0xA4 / +0xB4 plus a quarter of it (16 bits); the HP delta
// Battle_CalcDamage(actor, target, 0xFFFF). When that (read again) is not 0
// and 0x44F6A0(actor, target) answers al 0: Battle_ReturnQueuedItem(target),
// Battle_RemoveFromTurnOrder(target) (the target read again for each).
extern "C" void __cdecl EffectSlot33_HitDropTurn(void) {
    const unsigned actor = B(at::kActor);
    B(at::kRoundFlags2) = static_cast<unsigned char>(B(at::kRoundFlags2) | 0x20);
    const std::uint16_t base = ActorWord(L(at::kActor) & 0xFF, 0xA4, 0xB4);
    SetWord(At(at::kPower), static_cast<std::uint16_t>(base + (base >> 2)));
    const unsigned target = B(at::kTarget);
    const short d = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<std::uint16_t>(d));
    if (Word(Result() + 4) == 0) return;
    const unsigned t2 = B(at::kTarget);
    const unsigned a2 = B(at::kActor);
    if (BH_AT(U2, at::kResisted)(a2, t2) & 0xFF) return;
    BH_CALL(Battle_ReturnQueuedItem)(B(at::kTarget));
    BH_CALL(Battle_RemoveFromTurnOrder)(B(at::kTarget));
}

// original 0x44CAB0 (slot 34): the actor's status word (+0x90 / +0x92) bit 8
// kept and cleared; 0x904AA8 |= 0x80, 0x939FFC = 100; the HP delta
// Battle_CalcDamage(actor, target, 0xFFFF); then the kept bit put back into
// the status word of the actor read again.
extern "C" void __cdecl EffectSlot34_HitIgnore8(void) {
    const unsigned actor = B(at::kActor);
    unsigned char* const word = actor < 3 ? Party(actor) + 0x90 : Enemy(actor) + 0x92;
    const std::uint16_t status = Word(word);
    const std::uint16_t kept = status & 8;
    SetWord(word, status & 0xFFF7);
    const unsigned target = B(at::kTarget);
    B(at::kRoundFlags) = static_cast<unsigned char>(B(at::kRoundFlags) | 0x80);
    B(at::kHitRate) = 0x64;
    const short d = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
    SetWord(Result() + 4, static_cast<std::uint16_t>(d));
    const unsigned a2 = B(at::kActor);
    unsigned char* const back = a2 < 3 ? Party(a2) + 0x90 : Enemy(a2) + 0x92;
    SetWord(back, Word(back) | kept);
}

namespace {
// Slots 35 and 41: the actor's stat word - a member's character record
// (roster 0x66972C[+0x89]) +0x44, an enemy's +0xD4; the hit rate 0x939FFC =
// 100, or the word 0x939FE8 halved plus 30 when that is at most 100; the
// power word 0x939FE4 = 0, then Stat_AddClamped(0x939FE4, the addend's word +
// the stat); the HP delta Battle_CalcDamage(actor, target, 0xFFFF) (both
// read again).
void StatSumHit(U addend) {
    const unsigned a = B(at::kActor);
    B(at::kRoundFlags2) = static_cast<unsigned char>(B(at::kRoundFlags2) | 0x20);
    const unsigned actor = L(at::kActor) & 0xFF;
    const std::uint16_t stat =
        a <= 2 ? Word(CharRecord(B(at::kRosterOf + Party(actor)[0x89])) + 0x44) : Word(Enemy(actor) + 0xD4);
    const std::uint16_t half = static_cast<std::uint16_t>(W(at::kPowerAdd2) >> 1);
    B(at::kHitRate) = static_cast<unsigned char>(half + 0x1E > 0x64 ? 0x64 : half + 0x1E);
    const std::uint16_t add = W(addend);
    SetWord(At(at::kPower), 0);
    BH_CALL(Stat_AddClamped)(reinterpret_cast<unsigned short*>(At(at::kPower)), static_cast<std::uint16_t>(add + stat));
    Hit(0xFFFF);
}
}  // namespace

// original 0x44CB90 (slot 35): StatSumHit with the addend 0x939FE6.
extern "C" void __cdecl EffectSlot35_StatSumHit(void) { StatSumHit(at::kPowerAdd); }

// original 0x44CC60 (slot 36): Effect_SkillDamage(actor, target, 0x14, 0).
extern "C" void __cdecl EffectSlot36_Skill20(void) { SkillHit(0x14); }

// original 0x44CC90 (slot 37): 0x44FBB0(3).
extern "C" void __cdecl EffectSlot37_StatMod3(void) { StatMod(3); }

// original 0x44CD00 (slot 39): Battle_CalcDamage(actor, target, the ability's
// element word (NameTable_Abilities +4) & 0x1FF) as the HP delta.
extern "C" void __cdecl EffectSlot39_ElementHit(void) {
    const unsigned element = Word(AbilityRow(L(at::kAbility)) + 4) & 0x1FF;
    Hit(element);
}

// original 0x44CD40 (slot 40): the power word = the actor's +0xAA (a member)
// or +0xBA (an enemy); Battle_CalcDamage(actor, target, 0xFFFF).
extern "C" void __cdecl EffectSlot40_StatAAHit(void) {
    SetWord(At(at::kPower), ActorWord(L(at::kActor) & 0xFF, 0xAA, 0xBA));
    Hit(0xFFFF);
}

// original 0x44CDB0 (slot 41): StatSumHit with the addend 0x939FE8.
extern "C" void __cdecl EffectSlot41_StatSumHit2(void) { StatSumHit(at::kPowerAdd2); }

// original 0x44CE80 (slot 42): the power word = the actor's +0xA4 / +0xB4
// doubled (16 bits); Battle_CalcDamage(actor, target, 0x20).
extern "C" void __cdecl EffectSlot42_DoubleHit20(void) {
    SetWord(At(at::kPower), static_cast<std::uint16_t>(ActorWord(L(at::kActor) & 0xFF, 0xA4, 0xB4) << 1));
    Hit(0x20);
}

// original 0x44CEF0 (slot 43): 0x44FC60(0x10).
extern "C" void __cdecl EffectSlot43_Inflict10(void) { InflictMiss(0x10); }

namespace {
// Slots 44..46 and 49: the acting kind 4, the ability word, a tail jump to
// slot 4.
void AsAbility(unsigned id) {
    B(at::kActingKind) = 4;
    SetWord(At(at::kAbility), id);
    BH_CALL(EffectSlot04_SkillPower)();
}
}  // namespace

// original 0x44CF00 (slot 44): as ability 0x66, slot 4.
extern "C" void __cdecl EffectSlot44_Skill66(void) { AsAbility(0x66); }
// original 0x44CF20 (slot 45): as ability 0x65.
extern "C" void __cdecl EffectSlot45_Skill65(void) { AsAbility(0x65); }
// original 0x44CF40 (slot 46): as ability 0x62.
extern "C" void __cdecl EffectSlot46_Skill62(void) { AsAbility(0x62); }

// original 0x44CF60 (slot 47; R3C's 0x44D160 jumps here): not in the cut.
// 0x44FB30; then, by the ACTOR's side, the TARGET's second flags |= 0x200: a
// member actor the party record by the target byte (+0x134), an enemy actor
// the enemy object by it (+0x114) - unchecked, as the original (section 5).
extern "C" void __cdecl EffectSlot47_MissMark200(void) {
    MissTail();
    const unsigned actor = B(at::kActor);
    const unsigned t = L(at::kTarget) & 0xFF;
    unsigned char* const flags = actor <= 2 ? Party(t) + 0x134 : Enemy(t) + 0x114;
    SetLong(flags, static_cast<S32>(static_cast<U>(Long(flags)) | 0x200));
}

// original 0x44CFC0 (slot 48): the acting kind 4, the ability 0x55, 0x44FBB0(0).
extern "C" void __cdecl EffectSlot48_StatMod0Skill55(void) {
    B(at::kActingKind) = 4;
    SetWord(At(at::kAbility), 0x55);
    StatMod(0);
}

// original 0x44CFE0 (slot 49): as ability 0x5D, slot 4.
extern "C" void __cdecl EffectSlot49_Skill5D(void) { AsAbility(0x5D); }

// ============================================================================

void Rest3B_Inject() {
    if (bof3::WantsShadow("rest_3b")) rest_3b::SelfTest();
    BOF3_INJECT(BattleResult_AddExp);
    BOF3_INJECT(BattleResult_MemberTakesExp);
    BOF3_INJECT(CharId_ToRosterIndex);
    BOF3_INJECT(Stat_PercentCap999);
    BOF3_INJECT(Stat_PercentCap9999);
    BOF3_INJECT(Stat_PercentCap100);
    BOF3_INJECT(BattleTarget_PickParty);
    BOF3_INJECT(BattleTarget_Cancel);
    BOF3_INJECT(BattleItem_CloseWait);
    BOF3_INJECT(BattleItem_TargetCancel);
    BOF3_INJECT(BattleItem_SideDispatch);
    BOF3_INJECT(BattleItem_SideBegin);
    BOF3_INJECT(BattleItem_SidePick);
    BOF3_INJECT(BattleAttackCmd_Cancel);
    BOF3_INJECT(BattleItemCmd_CloseWait);
    BOF3_INJECT(BattleItemCmd_TargetCancel);
    BOF3_INJECT(BattleItemCmd_SideDispatch);
    BOF3_INJECT(BattleItemCmd_EquipDispatch);
    BOF3_INJECT(Escape_Dispatch);
    BOF3_INJECT(EffectSlot00_Miss);
    BOF3_INJECT(EffectSlot01_VariedHit);
    BOF3_INJECT(EffectSlot02_HitInflict4);
    BOF3_INJECT(EffectSlot03_HalfHitInflict20);
    BOF3_INJECT(EffectSlot04_SkillPower);
    BOF3_INJECT(EffectSlot05_Clear80);
    BOF3_INJECT(EffectSlot06_HpThirdHit);
    BOF3_INJECT(EffectSlot07_Heal);
    BOF3_INJECT(EffectSlot09_Heal40);
    BOF3_INJECT(EffectSlot10_Heal100);
    BOF3_INJECT(EffectSlot11_HealFull);
    BOF3_INJECT(EffectSlot12_Heal5Clear68);
    BOF3_INJECT(EffectSlot13_Clear80);
    BOF3_INJECT(EffectSlot14_Clear8);
    BOF3_INJECT(EffectSlot15_Clear100);
    BOF3_INJECT(EffectSlot16_ClearBFC);
    BOF3_INJECT(EffectSlot17_RaiseDown);
    BOF3_INJECT(EffectSlot18_HalfHitInflict80);
    BOF3_INJECT(EffectSlot19_StatMod0);
    BOF3_INJECT(EffectSlot20_ApHeal20);
    BOF3_INJECT(EffectSlot21_ApHeal100);
    BOF3_INJECT(EffectSlot24_StatMod1);
    BOF3_INJECT(EffectSlot25_StatMod2);
    BOF3_INJECT(EffectSlot26_Inflict40);
    BOF3_INJECT(EffectSlot27_Inflict20);
    BOF3_INJECT(EffectSlot28_RaiseQuarter);
    BOF3_INJECT(EffectSlot29_RaiseFull);
    BOF3_INJECT(EffectSlot30_HalfHitInflict8);
    BOF3_INJECT(EffectSlot32_HalfHitPlus1);
    BOF3_INJECT(EffectSlot33_HitDropTurn);
    BOF3_INJECT(EffectSlot34_HitIgnore8);
    BOF3_INJECT(EffectSlot35_StatSumHit);
    BOF3_INJECT(EffectSlot36_Skill20);
    BOF3_INJECT(EffectSlot37_StatMod3);
    BOF3_INJECT(EffectSlot39_ElementHit);
    BOF3_INJECT(EffectSlot40_StatAAHit);
    BOF3_INJECT(EffectSlot41_StatSumHit2);
    BOF3_INJECT(EffectSlot42_DoubleHit20);
    BOF3_INJECT(EffectSlot43_Inflict10);
    BOF3_INJECT(EffectSlot44_Skill66);
    BOF3_INJECT(EffectSlot45_Skill65);
    BOF3_INJECT(EffectSlot46_Skill62);
    BOF3_INJECT(EffectSlot47_MissMark200);
    BOF3_INJECT(EffectSlot48_StatMod0Skill55);
    BOF3_INJECT(EffectSlot49_Skill5D);
}
