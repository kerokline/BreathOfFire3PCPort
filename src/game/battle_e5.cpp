// Round twelve group BE5 (docs/takeover-queue-field-battle.md section 3,
// analysis/round12_cut.tsv's group column): the battle engine's band
// 0x44B240..0x451480, each function read to its last instruction with
// capstone (2026-09-29) and taken through the boss harness's engine frame
// (boss_harness.h, docs/boss_harness.md section 10). docs/battle_e5.md has
// every function one row each.
//
//   - the enemy AI's row helpers: the other rows' test EnemyAI_TurnCheck's
//     condition 0x26 uses, the done bit, the element test of conditions 0..8
//     and 0x21..0x23, a row applied (seven kinds through a jump table, then
//     the row's common tail and the enemy messages), its stat scaling and its byte
//     setter (the latter two not in the cut: the cut's 0x44B8D0 is a case of
//     the setter's switch), the enemy messages deduplicated (not in the cut);
//   - three Effect_Handlers slots: 22 the HP drain, 23 the AP drain (not in
//     the cut; also 0x44EB50's tail jump), 38 the quarter-chance attack;
//   - BattleForm_ApplyStats: the transformation's block 0x939EE0.. into each
//     party member flagged +0x134 bit 1;
//   - the Dragon command's run below 0x44FF00 (not ours): six dispatchers by
//     0x904AA4 and their 33 steps (two of them not in the cut), two helpers
//     that price a slot against the member's AP.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The six
// dispatchers abort past their tables where the original jumps through
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6); EnemyAI_DedupMessages aborts where the original would write a
// ninth entry past its eight-dword stack buffer (onto its return address).
// Every call goes through the harness (BH_CALL / BH_AT), so the start-up fuzz
// can stand recorders in for the callees.
#include "game/battle_e5.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_e5_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e5::at;
using U = std::uint32_t;
using S32 = std::int32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
std::uint16_t W(U address) { return Word(At(address)); }
void SetW(U address, unsigned v) { SetWord(At(address), v); }
U L(U address) { return static_cast<U>(Long(At(address))); }
S32 S16(std::uint16_t v) { return static_cast<std::int16_t>(v); }
unsigned char* Ptr(U cell) { return At(L(cell)); }
unsigned char* Party(unsigned member) { return At(at::kParty + member * at::kPartyStride); }
unsigned char* EnemyObj(unsigned n) { return At(at::kEnemies + n * at::kEnemyStride); }

// The callees nobody owns yet, by address (battle_e5_callees.h).
using Void0 = void (__cdecl*)();
using U1 = U (__cdecl*)(U);
using U2 = U (__cdecl*)(U, U);
using U3 = U (__cdecl*)(U, U, U);

// ===========================================================================
// The enemy AI's row helpers
// ===========================================================================

// (a * 1000) / 10000 as the original computes it: the product and the
// thousandfold in 32 bits (wrapping), then a signed division truncated
// toward 0 (imul 0x68DB8BAD, sar 12, + the sign bit).
S32 Tenth(U a) { return static_cast<S32>(a * 1000u) / 10000; }

// A scaled word capped: stored when the result (as unsigned) is at most
// `cap`, else `cap` (cmp edx, cap; ja).
void ScaleCapped(unsigned char* word, U a, U cap) {
    const U q = static_cast<U>(Tenth(a));
    SetWord(word, q > cap ? cap : q);
}

}  // namespace

// original 0x44B240 (EnemyAI_TurnCheck's sibling EnemyAI_ChooseActions,
// condition 0x26): 1 when every row of enemy `enemy`'s script but `row` is
// done - rows 0..3 in turn, skipping `row`: a row whose byte 0 is 0x63 answers
// 0, and so does one EnemyAI_RowDone says is not done. The script byte +0xF0
// is read again for each row. al out, eax above it the caller's.
extern "C" unsigned char __cdecl EnemyAI_OtherRowsDone(unsigned row, unsigned enemy) {
    const unsigned skip = row & 0xFF;
    unsigned char* const obj = EnemyObj(enemy & 0xFF);
    for (unsigned i = 0; i < 4; ++i) {
        if (i == skip) continue;
        const unsigned script = obj[0xF0];
        if (B(at::kAiScripts + script * 0x8C + i * 16) == 0x63) return 0;
        if ((BH_CALL(EnemyAI_RowDone)(obj, i) & 0xFF) == 0) return 0;
    }
    return 1;
}

// original 0x44B2E0: the enemy's done bit for `row` (+0xF1 bit row & 31 -
// `shl al, cl` on a byte, so rows 8..31 name no bit) set when `on`'s byte is
// not 0, else cleared.
extern "C" void __cdecl EnemyAI_SetRowDone(unsigned char* enemy, unsigned row, unsigned on) {
    const unsigned shift = row & 31;
    const unsigned char bit = static_cast<unsigned char>(shift < 8 ? 1u << shift : 0u);
    if (on & 0xFF) enemy[0xF1] = static_cast<unsigned char>(enemy[0xF1] | bit);
    else enemy[0xF1] = static_cast<unsigned char>(enemy[0xF1] & ~bit);
}

// original 0x44B320: whether the action that just hit carries one of `mask`'s
// elements. 0 unless the current enemy's word +0x108 is not 0; then by the
// acting kind 0x904B35: 4 (an ability) - the ability's element word (24-byte
// rows at 0x65C4DC by the word 0x904B80) against the mask's low word; 1 (an
// attack) by a party member 0..2 and the mask's bit 8 clear - the member's
// weapon +0x92 and its element byte (0x657463, 28 bytes a weapon) against
// the mask's low byte. Anything else 0. al out.
extern "C" unsigned char __cdecl EnemyAI_CondElement(unsigned mask) {
    if (Word(Ptr(at::kCurrentEnemy) + 0x108) == 0) return 0;
    const unsigned actor = B(at::kActor);
    const unsigned kind = B(at::kActingKind);
    if (kind == 4) {
        const unsigned ability = L(at::kAbility) & 0xFFFF;
        return (W(at::kAbilityRows + ability * 24) & mask & 0xFFFF) != 0;
    }
    if (kind != 1 || actor > 2 || (mask & 0x100) != 0) return 0;
    const unsigned weapon = Party(actor)[0x92];
    return (B(at::kWeaponElement + weapon * 28) & mask & 0xFF) != 0;
}

// original 0x44B5E0 (called by EnemyAI_ApplyAction for kind 3): one of the
// enemy's stats scaled by factor / 10 (Tenth): which 0 HP +0xA4 capped at
// +0xD0, 1 AP +0xA6 capped at +0xD2, 2..5 +0xD4 / +0xD6 / +0xD8 / +0xDA
// capped at 999, 6 +0x98 by the factor squared, capped at 999; others
// nothing.
extern "C" void __cdecl EnemyAI_ScaleStat(unsigned char* enemy, unsigned which, unsigned factor) {
    const U f = factor & 0xFF;
    switch (which & 0xFF) {
    case 0: ScaleCapped(enemy + 0xA4, Word(enemy + 0xA4) * f, Word(enemy + 0xD0)); break;
    case 1: ScaleCapped(enemy + 0xA6, Word(enemy + 0xA6) * f, Word(enemy + 0xD2)); break;
    case 2: ScaleCapped(enemy + 0xD4, Word(enemy + 0xD4) * f, 999); break;
    case 3: ScaleCapped(enemy + 0xD6, Word(enemy + 0xD6) * f, 999); break;
    case 4: ScaleCapped(enemy + 0xD8, Word(enemy + 0xD8) * f, 999); break;
    case 5: ScaleCapped(enemy + 0xDA, Word(enemy + 0xDA) * f, 999); break;
    case 6: ScaleCapped(enemy + 0x98, Word(enemy + 0x98) * f * f, 999); break;
    default: break;
    }
}

// original 0x44B870 (called by EnemyAI_ApplyAction for kind 4): the enemy's
// byte +0xDF + which for which 0..6, +0xE7 for 7 (+0xE6 is skipped), set to
// the value's byte; others nothing. The cut's 0x44B8D0 is case 5 (+0xE4).
extern "C" void __cdecl EnemyAI_SetAttrByte(unsigned char* enemy, unsigned which, unsigned value) {
    const unsigned w = which & 0xFF;
    if (w <= 6) enemy[0xDF + w] = static_cast<unsigned char>(value);
    else if (w == 7) enemy[0xE7] = static_cast<unsigned char>(value);
}

// original 0x44B3A0: an AI row applied to an enemy object. By the row's kind
// (byte +1, 1..7 through the jump table 0x44B5C4):
//   1  the word +0x90 = the row's byte +2;
//   2  Sprite_Current the enemy for Battle_ClearStatus(enemy +5, 0xFFFF),
//      then 0x44F1D0(Sprite_Current +5, +0x92 | row +2) with the upper half
//      of ClearStatus's eax in both words, as the original's registers carry
//      it; Sprite_Current put back;
//   3  for each bit of row +2 from the top, EnemyAI_ScaleStat(enemy, bit,
//      row +3);
//   4  the same through EnemyAI_SetAttrByte;
//   5  row +2 bit 1: +0xAA = row +3; bit 0: +0xAE = row +3;
//   6  row +2 bit 1: the word +0x96 scaled by row +3 / 10, bit 0: +0x94,
//      each capped at 0xFFFF;
//   7  0x904B97 = row +2;
// then always: +0x8E = row +4, row +8..+15 into +0x9C..+0xA3,
// 0x453300(enemy +5) (the word's upper bytes are row + 16's, the copy's
// pointer), and - HP +0xA4 and the row's word +6 both not 0, the message
// count 0x93C2A2 below 8 - an enemy message: enemy +5 minus 3, the word (a
// system message BattleAction_EnemyMessages / BattleCommit_QueueMessages show).
extern "C" void __cdecl EnemyAI_ApplyAction(unsigned char* enemy, const unsigned char* row) {
    switch (row[1]) {
    case 1:
        SetWord(enemy + 0x90, row[2]);
        break;
    case 2: {
        unsigned char* const saved = Sprite_Current;
        Sprite_Current = enemy;
        const U cleared = BH_CALL(Battle_ClearStatus)(enemy[5], 0xFFFF);
        const U status = enemy[0x92];
        const unsigned char* const current = Sprite_Current;
        const U word = (cleared & 0xFFFF0000u) | status | row[2];
        BH_AT(U2, at::kInflictStatus)((word & 0xFFFFFF00u) | current[5], word);
        Sprite_Current = saved;
        break;
    }
    case 3: {
        unsigned bits = row[2];
        for (unsigned i = 0; i < 8; ++i, bits <<= 1)
            if (bits & 0x80) BH_CALL(EnemyAI_ScaleStat)(enemy, i, row[3]);
        break;
    }
    case 4: {
        unsigned bits = row[2];
        for (unsigned i = 0; i < 8; ++i, bits <<= 1)
            if (bits & 0x80) BH_CALL(EnemyAI_SetAttrByte)(enemy, i, row[3]);
        break;
    }
    case 5:
        if (row[2] & 2) enemy[0xAA] = row[3];
        if (row[2] & 1) enemy[0xAE] = row[3];
        break;
    case 6:
        if (row[2] & 2) ScaleCapped(enemy + 0x96, static_cast<U>(row[3]) * Word(enemy + 0x96), 0xFFFF);
        if (row[2] & 1) ScaleCapped(enemy + 0x94, static_cast<U>(row[3]) * Word(enemy + 0x94), 0xFFFF);
        break;
    case 7:
        B(at::kAiFlag) = row[2];
        break;
    default:
        break;
    }
    enemy[0x8E] = row[4];
    for (unsigned i = 0; i < 8; ++i) enemy[0x9C + i] = row[8 + i];
    const U tail = static_cast<U>(reinterpret_cast<std::uintptr_t>(row + 16));
    BH_AT(U1, at::kStatusPick)((tail & 0xFFFFFF00u) | enemy[5]);
    if (Word(enemy + 0xA4) != 0 && Word(row + 6) != 0) {
        const unsigned n = B(at::kEnemyMessageCount);
        if (n < 8) {
            B(at::kEnemyMessages + n * 4) = static_cast<unsigned char>(enemy[5] - 3);
            SetW(at::kEnemyMessages + n * 4 + 2, Word(row + 6));
            B(at::kEnemyMessageCount) = static_cast<unsigned char>(n + 1);
        }
    }
}

// original 0x44B920 (EnemyAI_TurnCheck's and EnemyAI_ChooseActions' tail):
// the enemy messages 0x939FC0 (0x93C2A2 entries) with each one dropped whose
// enemy's +0x8C byte and message word match one kept before it; the kept ones
// written back in order and counted. The original keeps them in an eight-dword
// stack buffer: a ninth kept entry would land on its return address, so ours
// aborts there. Its eax is not read (the one caller of EnemyAI_TurnCheck,
// 0x436908, loads eax again at once).
extern "C" void __cdecl EnemyAI_DedupMessages(void) {
    const unsigned n = B(at::kEnemyMessageCount);
    U kept[8];
    unsigned k = 0;
    for (unsigned i = 0; i < n; ++i) {
        const U entry = at::kEnemyMessages + 4 * i;
        bool dup = false;
        if (k != 0) {
            const unsigned char group = EnemyObj(B(entry))[0x8C];
            for (unsigned j = 0; j < k && !dup; ++j)
                dup = group == EnemyObj(kept[j] & 0xFF)[0x8C] && W(entry + 2) == (kept[j] >> 16);
        }
        if (dup) continue;
        if (k == 8)
            bof3::Fatal("EnemyAI_DedupMessages: a ninth distinct entry of %u - the original writes it past its eight-dword "
                        "buffer, onto its return address (docs/battle_e5.md section 7)",
                        n);
        kept[k++] = L(entry);
    }
    B(at::kEnemyMessageCount) = static_cast<unsigned char>(k);
    for (unsigned i = 0; i < k; ++i) SetLong(At(at::kEnemyMessages + 4 * i), static_cast<S32>(kept[i]));
}

// ===========================================================================
// Effect_Handlers slots 22, 23 and 38
// ===========================================================================

namespace {

U Key(const unsigned char* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Result() { return Ptr(at::kResult); }

// The drains' shared body (0x44C3D0 HP: the result's word +4, the party's
// +0x98 / +0xA0, the enemy's +0xA4 / +0xB0, flag 4, Battle_SetDamagePopup;
// 0x44C5C0 AP: +6, +0x9A / +0xA2, +0xA6 / +0xB2, flag 8, 0x453EB0), after
// the resist test and before the stat add:
//   the target 0x904B54 (read again after the test): its stat / 5 as the
//   result's delta, 0 for a target with flag 0x10000 (the party's +0x130, the
//   enemy's +0x110);
//   then the acting sprite 0x904B40's +8 (and for AP the result's +8) marked;
//   the actor 0x904B34: when its stat plus the delta (signed) runs past its
//   maximum, the acting sprite's +8 gets `flag`; 0x590E80(&stat, cap,
//   delta) - the cap word's upper half is the result pointer's, as the
//   original's register holds it -, its answer negated to the pop-up with the
//   actor, and the acting sprite's +8 cleared.
struct Drain {
    U at;
    U party_stat, party_max;
    U enemy_stat, enemy_max;
    unsigned char flag, mark;
    bool ap;
};

void DrainTarget(const Drain& d) {
    const unsigned t = B(at::kTarget);
    if (t <= 2) {
        unsigned char* const p = Party(t);
        SetWord(Result() + d.at, (L(Key(p) + 0x130) & 0x10000) ? 0u : Word(p + d.party_stat) / 5u);
    } else {
        unsigned char* const e = EnemyObj(t - 3);
        SetWord(Result() + d.at, (L(Key(e) + 0x110) & 0x10000) ? 0u : Word(e + d.enemy_stat) / 5u);
    }
}

void DrainActor(const Drain& d) {
    const unsigned a = B(at::kActor);
    unsigned char* stat;
    U max_at;
    if (a <= 2) {
        stat = Party(a) + d.party_stat;
        max_at = Key(Party(a)) + d.party_max;
    } else {
        stat = EnemyObj(a - 3) + d.enemy_stat;
        max_at = Key(EnemyObj(a - 3)) + d.enemy_max;
    }
    if (static_cast<S32>(Word(stat)) + S16(Word(Result() + d.at)) > static_cast<S32>(W(max_at)))
        Ptr(at::kActingSprite2)[8] = static_cast<unsigned char>(Ptr(at::kActingSprite2)[8] | d.flag);
    const U result = L(at::kResult);
    const U change = BH_AT(U3, at::kStatAdd)(Key(stat), (result & 0xFFFF0000u) | W(max_at), Word(At(result) + d.at));
    if (d.ap) BH_AT(U2, at::kApPopup)(0u - change, B(at::kActor));
    else BH_CALL(Battle_SetDamagePopup)(0u - change, B(at::kActor));
    Ptr(at::kActingSprite2)[8] = 0;
}

constexpr Drain kDrainHp = {4, 0x98, 0xA0, 0xA4, 0xB0, 4, 1, false};
constexpr Drain kDrainAp = {6, 0x9A, 0xA2, 0xA6, 0xB2, 8, 2, true};

}  // namespace

// original 0x44C3D0 (Effect_Handlers slot 22): the HP drain. 0x44F6A0(actor,
// target) resisting: the result's +8 = 1, nothing else. Else the target's HP
// / 5 as the HP delta, the acting sprite's +8 = 1, and the actor's HP raised
// by it (Drain above).
extern "C" void __cdecl Effect_DrainHp(void) {
    if (BH_AT(U2, at::kResisted)(B(at::kActor), B(at::kTarget)) & 0xFF) {
        Result()[8] = 1;
        return;
    }
    DrainTarget(kDrainHp);
    Ptr(at::kActingSprite2)[8] = kDrainHp.mark;
    DrainActor(kDrainHp);
}

// original 0x44C5C0 (Effect_Handlers slot 23; 0x44EB50, slot 124, sets the
// acting kind 4 and the ability 0x4E and jumps here): the AP drain, as the
// HP drain on AP, with the result's +8 = 2 as well and the pop-up 0x453EB0.
extern "C" void __cdecl Effect_DrainAp(void) {
    if (BH_AT(U2, at::kResisted)(B(at::kActor), B(at::kTarget)) & 0xFF) {
        Result()[8] = 2;
        return;
    }
    DrainTarget(kDrainAp);
    Ptr(at::kActingSprite2)[8] = kDrainAp.mark;
    Result()[8] = kDrainAp.mark;
    DrainActor(kDrainAp);
}

// original 0x44CCA0 (Effect_Handlers slot 38): Rand & 0x7F below 0x20 (a
// quarter): the round flags |= 0x80 and the HP delta Battle_CalcDamage(actor,
// target, 0xFFFF). Else a miss: the result's +8 = 1, Battle_SetDamagePopup(0,
// target), then a tail jump to 0x44FB30.
extern "C" void __cdecl Effect_QuarterAttack(void) {
    if ((BH_CALL(Rand)() & 0x7F) < 0x20) {
        const unsigned target = B(at::kTarget);
        const unsigned actor = B(at::kActor);
        B(at::kFlags) = static_cast<unsigned char>(B(at::kFlags) | 0x80);
        const short delta = BH_CALL(Battle_CalcDamage)(actor, target, 0xFFFF);
        SetWord(Result() + 4, static_cast<std::uint16_t>(delta));
        return;
    }
    Result()[8] = 1;
    BH_CALL(Battle_SetDamagePopup)(0, B(at::kTarget));
    BH_AT(Void0, at::kMissTail)();
}

// ===========================================================================
// The transformation's stats
// ===========================================================================

// original 0x44FDE0 (called by 0x442310 (BE3), 0x449A00 / 0x449C70 (BE4),
// 0x44D450, 0x44DFD0, 0x44E720): for each party member whose +0x134 has bit
// 1, the transformation's block 0x939EE0.. over its stats. With c the
// member's +0x9E: c +0x22 and +2 = w0 - (c[0] * w0 + 5) / 10 (w0 the signed
// word 0x939EE0, the division signed and truncated); the words c +0x26,
// +0x28, +0x2A, +0x2C raised by the words 0x939EE2, 0x939EE4, 0x939EE6,
// 0x939EE8 and copied to c +6, +8, +0xA, +0xC; the bytes 0x939EEA..0x939EF2
// into c +0x31..+0x39 and c +0x11..+0x19. The words 0x939EE0 / EE4 / EE8
// and the dword 0x939EF0 are read once, before the members (no call in the
// loop, and the members do not overlap them).
extern "C" void __cdecl BattleForm_ApplyStats(void) {
    const U ef0 = L(0x939EF0);
    const std::uint16_t ee8 = W(0x939EE8);
    const std::uint16_t ee0 = W(0x939EE0);
    const std::uint16_t ee4 = W(0x939EE4);
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const c = Party(m) + 0x9E;
        if ((c[0x96] & 2) == 0) continue;
        const S32 product = static_cast<S32>(static_cast<U>(c[0]) * static_cast<U>(S16(ee0)) + 5u);
        const std::uint16_t hp = static_cast<std::uint16_t>(S16(ee0) - product / 10);
        SetWord(c + 0x22, hp);
        SetWord(c + 2, hp);
        SetWord(c + 0x26, Word(c + 0x26) + W(0x939EE2));
        SetWord(c + 6, Word(c + 0x26));
        SetWord(c + 0x28, Word(c + 0x28) + ee4);
        SetWord(c + 8, Word(c + 0x28));
        SetWord(c + 0x2A, Word(c + 0x2A) + W(0x939EE6));
        SetWord(c + 0xA, Word(c + 0x2A));
        SetWord(c + 0x2C, Word(c + 0x2C) + ee8);
        SetWord(c + 0xC, Word(c + 0x2C));
        for (unsigned i = 0; i < 6; ++i) {
            c[0x31 + i] = B(0x939EEA + i);
            c[0x11 + i] = c[0x31 + i];
        }
        const unsigned char last = B(0x939EF2);
        c[0x37] = static_cast<unsigned char>(ef0);
        c[0x17] = static_cast<unsigned char>(ef0);
        c[0x38] = static_cast<unsigned char>(ef0 >> 8);
        c[0x18] = static_cast<unsigned char>(ef0 >> 8);
        c[0x39] = last;
        c[0x19] = last;
    }
}

// ===========================================================================
// The Dragon command's run (0x44FF00 by 0x904AA3 into 0x64ECCC; not ours)
// ===========================================================================

namespace {

using Entry = U (__cdecl*)(U);
Entry EntryAt(U table, unsigned index) { return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(L(table + 4 * index))); }

// jmp [table + 4 * byte 0x904AA4]: the table's `entries` steps, a Fatal past
// them (the original jumps through whatever follows: the next table's steps,
// or for 0x64ED50 the data after it). The jmp leaves the caller's stack word
// and the step's eax as they were: ours hands the word on and answers the
// step's eax.
U Dispatch(const char* who, U table, unsigned entries, U through) {
    const unsigned step = B(at::kStep4);
    if (step >= entries)
        bof3::Fatal("%s: 0x904AA4 is %u, past the %u steps of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e5.md section 7)",
                    who, step, entries, (unsigned)table);
    return EntryAt(table, step)(through);   // the step as read: the fuzz swaps the table's cells for its recorders
}

void Sound(unsigned id) { BH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
void NextStep() { B(at::kStep4) = static_cast<unsigned char>(B(at::kStep4) + 1); }
const unsigned char* Line(unsigned id) { return BH_CALL(Msg_SystemPtr)(id); }
// The message window's last line (0x93C2C4 + ((0x93C2A1 - 1) & 15) * 8),
// the count read after the Msg_SystemPtr call that made the text.
void ShowLine(const unsigned char* text) {
    SetLong(At(at::kMsgText + ((B(at::kMsgCount) - 1u) & 0xF) * 8), static_cast<S32>(Key(text)));
}
unsigned char* Member() { return Ptr(at::kMenuActor); }
void MarkMember() {
    unsigned char* const m = Member();
    SetLong(m + 0x134, static_cast<S32>(static_cast<U>(Long(m + 0x134)) | 4u));
}
U Pressed() { return Input_Pressed; }
bool Cancel(U pressed) { return (Field_CancelButtons & pressed & 0xFFFF) != 0; }
bool Confirm(U pressed) { return (Field_ConfirmButtons & pressed & 0xFFFF) != 0; }
U Repeat(U mask) { return BH_CALL(Input_AutoRepeat)(Pressed() & mask); }
void CloseList(U window) { B(window + 3) = 2; }

// A list's slot: 4 bytes at `table` + 4 * (the cursor's two signed words,
// summed: +0x12 and +0x10 of its window record).
U SlotOf(U table, U window) { return table + 4u * static_cast<U>(S16(W(window + 0x12)) + S16(W(window + 0x10))); }

// A list's cursor move (the four copies at 0x450412, 0x450C64, 0x45105F,
// 0x45137E): a = the window's +0x12, b = its +0x10, by the repeat keys -
//   0x1000  a + b above 0: b 0 -> a - 1; b above 0 -> b - 1;
//   0x0004  a at least 3 -> a - 3; a 0 -> b = 0; else a = 0;
//   0x4000  a + b below `sum_max`: b 2 -> a + 1; b below 2 -> b + 1;
//   0x0008  a `a_max` -> b = 2; else a + 3, or `a_max` when a + b + 3 runs past it;
// the first that holds, then 0x101 when a + b (signed) is not the old sum
// (the old one as an unsigned 16-bit value, as the original compares them).
void MoveCursor(U window, U keys, S32 sum_max, S32 a_max) {
    const U wa = window + 0x12, wb = window + 0x10;
    std::uint16_t a = W(wa), b = W(wb);
    const U before = static_cast<std::uint16_t>(a + b);
    if (keys & 0x1000) {
        if (S16(a) + S16(b) > 0) {
            if (b == 0) SetW(wa, a = static_cast<std::uint16_t>(a - 1));
            else if (S16(b) > 0) SetW(wb, b = static_cast<std::uint16_t>(b - 1));
        }
    } else if (keys & 4) {
        if (S16(a) >= 3) SetW(wa, a = static_cast<std::uint16_t>(a - 3));
        else if (a == 0) SetW(wb, b = 0);
        else SetW(wa, a = 0);
    } else if (keys & 0x4000) {
        if (S16(a) + S16(b) < sum_max) {
            if (b == 2) SetW(wa, a = static_cast<std::uint16_t>(a + 1));
            else if (S16(b) < 2) SetW(wb, b = static_cast<std::uint16_t>(b + 1));
        }
    } else if (keys & 8) {
        if (S16(a) == a_max) {
            SetW(wb, b = 2);
        } else {
            a = S16(a) + S16(b) + 3 > a_max ? static_cast<std::uint16_t>(a_max) : static_cast<std::uint16_t>(a + 3);
            SetW(wa, a);
        }
    }
    if (static_cast<U>(S16(a) + S16(b)) != before) Sound(0x101);
}

// A list's slot priced (0x450510, 0x450D60): 0 for an empty slot (+3 0xFF);
// else the cost bytes 0x64EC9C of its gene +0 and of +1 / +2 unless 0xFF,
// summed in a byte, against the member's AP (0x802DDA by the menu actor's
// +5): 1 when the member has it.
unsigned char Affordable(U slot) {
    if (B(slot + 3) == 0xFF) return 0;
    unsigned char cost = B(at::kGeneCostTable + B(slot));
    if (B(slot + 1) != 0xFF) cost = static_cast<unsigned char>(cost + B(at::kGeneCostTable + B(slot + 1)));
    if (B(slot + 2) != 0xFF) cost = static_cast<unsigned char>(cost + B(at::kGeneCostTable + B(slot + 2)));
    return cost <= W(0x802DDA + Member()[5] * at::kPartyStride);
}

// A slot's genes into 0x904B84..86 (0x450610, 0x450E70): the count 0x904B87
// zeroed, then each of the slot's three bytes not 0xFF stored at its own
// place and counted.
void TakeSlot(U slot) {
    B(at::kGeneCount) = 0;
    for (unsigned j = 0; j < 3; ++j) {
        const unsigned char g = B(slot + j);
        if (g == 0xFF) continue;
        B(at::kGenes + j) = g;
        B(at::kGeneCount) = static_cast<unsigned char>(B(at::kGeneCount) + 1);
    }
}

// The list windows' opening and closing (0x4502C0 / 0x450B60, 0x450680 /
// 0x450EF0, 0x4505A0 / 0x450DF0): see each.
void OpenList(U slot, U window, unsigned char kind, unsigned x, unsigned line) {
    if (B(at::kWin21) & 1) return;
    B(at::kWin21) = 0;
    BH_CALL(Window_Alloc)(slot, 5);
    B(window + 2) = kind;
    B(window + 3) = 0;
    SetW(window + 4, x);
    SetW(window + 6, 0x3F);
    SetW(window + 0x10, 0);
    SetW(window + 0x12, 0);
    ShowLine(Line(line));
    B(at::kPick) = static_cast<unsigned char>(B(at::kPick) | 0x80);
    NextStep();
}

void ReopenMenu(unsigned x) {
    BH_CALL(Window_Alloc)(0x15, 5);
    B(at::kWin21 + 2) = 0;
    B(at::kWin21 + 3) = 0;
    SetW(at::kWin21 + 4, x);
    SetW(at::kWin21 + 6, 0x3F);
}

void BackToMenu(U window) {
    if (B(window) & 1) return;
    const unsigned char pick = B(at::kPick);
    B(at::kStep3) = 2;
    B(at::kStep4) = 0;
    B(at::kPick) = static_cast<unsigned char>(pick & 0x7F);
}

}  // namespace

// --- part 0 (0x904AA3 0): the load --------------------------------------------

// original 0x44FF10 (0x64ECCC[0]): jmp [0x64ECE8 + 4 * 0x904AA4], two steps.
extern "C" unsigned long __cdecl DragonCmd_LoadDispatch(unsigned long through) {
    return Dispatch("DragonCmd_LoadDispatch", at::kLoadSteps, 2, through);
}

// original 0x44FF30 (0x64ECE8[0]): window 16's +3 zero: LoadDatFile(0xCC) and
// the next step; else nothing.
extern "C" void __cdecl DragonCmd_LoadStart(void) {
    if (B(at::kWin16 + 3) != 0) return;
    BH_CALL(LoadDatFile)(0xCC);
    NextStep();
}

// original 0x44FF60 (0x64ECE8[1]; not in the cut): File_LoadDone: the CLUT
// rows 0x1A and 0x1B copied, Gfx_ClutStripDirty + 1, the next part (0x904AA3
// + 1, 0x904AA4 = 0).
extern "C" void __cdecl DragonCmd_LoadWait(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Gfx_ClutStripCopyRow)(0x1A);
    BH_CALL(Gfx_ClutStripCopyRow)(0x1B);
    const unsigned char dirty = static_cast<unsigned char>(Gfx_ClutStripDirty + 1);
    const unsigned char part = static_cast<unsigned char>(B(at::kStep3) + 1);
    Gfx_ClutStripDirty = dirty;
    B(at::kStep3) = part;
    B(at::kStep4) = 0;
}

// --- part 1: the opening ---------------------------------------------------------

// original 0x44FFA0 (0x64ECCC[1]): window 21 (the menu: kind 5, +2 / +3 0, at
// 0x142, 0x3F, +0xA 0) and window 17 (+2 1, +3 0) claimed; sound 0x102; the
// 18 gene bits of 0x904650 spread into 0x939A60; Input_AutoRepeat's latch,
// 0x904AA6, 0x904AA7, the words 0x904B72 / 0x904B74, 0x904B87, 0x904B78 zeroed
// and the menu's line 0x904AA5 = 1; 0x4525B0; the next part.
extern "C" void __cdecl DragonCmd_Open(void) {
    BH_CALL(Window_Alloc)(0x15, 5);
    B(at::kWin21 + 2) = 0;
    B(at::kWin21 + 3) = 0;
    SetW(at::kWin21 + 4, 0x142);
    SetW(at::kWin21 + 6, 0x3F);
    B(at::kWin21 + 0xA) = 0;
    BH_CALL(Window_Alloc)(0x11, 5);
    B(at::kWin17 + 2) = 1;
    B(at::kWin17 + 3) = 0;
    Sound(0x102);
    const U genes = L(at::kGeneBits);
    for (unsigned i = 0; i < 18; ++i) B(at::kGeneFlags + i) = static_cast<unsigned char>((genes >> i) & 1);
    SetW(at::kRepeatLatch, 0);
    B(at::kPick) = 1;
    B(at::kGeneRow) = 0;
    B(at::kGeneCol) = 0;
    SetW(at::kWord72, 0);
    SetW(at::kAsk, 0);
    B(at::kGeneCount) = 0;
    B(at::kGeneCost) = 0;
    BH_AT(Void0, at::kDragonTask)();
    const unsigned char part = static_cast<unsigned char>(B(at::kStep3) + 1);
    B(at::kStep4) = 0;
    B(at::kStep3) = part;
}

// --- part 2: the menu --------------------------------------------------------------

// original 0x450070 (0x64ECCC[2]): jmp [0x64ECF0 + 4 * 0x904AA4], five steps.
extern "C" unsigned long __cdecl DragonCmd_MenuDispatch(unsigned long through) {
    return Dispatch("DragonCmd_MenuDispatch", at::kMenuSteps, 5, through);
}

// original 0x450090 (0x64ECF0[0]): the menu's line 0x42 + 0x904AA5 shown;
// the repeat keys of 0xE000; cancel: sound 0x106, two steps on (the close);
// confirm: sound 0x104, the next step (the pick); else 0x2000 / 0x8000 move
// the line down / up (sound 0x101 each), 0x4000 picks line 1 at once (sound
// 0x104, the next step); the line wrapped: below 0 (signed) 2, above 2 0.
extern "C" void __cdecl DragonCmd_MenuInput(void) {
    ShowLine(Line(static_cast<std::uint16_t>(B(at::kPick) + 0x42)));
    const U keys = Repeat(0xE000);
    const U pressed = Pressed();
    if (Cancel(pressed)) {
        Sound(0x106);
        B(at::kStep4) = static_cast<unsigned char>(B(at::kStep4) + 2);
        return;
    }
    if (Confirm(pressed)) {
        Sound(0x104);
        NextStep();
        return;
    }
    if (keys & 0x2000) {
        Sound(0x101);
        B(at::kPick) = static_cast<unsigned char>(B(at::kPick) + 1);
    }
    if (keys & 0x8000) {
        Sound(0x101);
        B(at::kPick) = static_cast<unsigned char>(B(at::kPick) - 1);
    }
    if (keys & 0x4000) {
        Sound(0x104);
        const unsigned char step = static_cast<unsigned char>(B(at::kStep4) + 1);
        B(at::kPick) = 1;
        B(at::kStep4) = step;
        return;
    }
    const unsigned char pick = B(at::kPick);
    if (static_cast<signed char>(pick) < 0) B(at::kPick) = 2;
    else if (pick > 2) B(at::kPick) = 0;
}

// original 0x4501B0 (0x64ECF0[1]): the line picked - 0x904AA5 |= 0x80, the
// part 3 + the line (0x904AA3), 0x904AA4 and 0x904AA6 zeroed.
extern "C" void __cdecl DragonCmd_MenuPick(void) {
    const unsigned char pick = B(at::kPick);
    B(at::kPick) = static_cast<unsigned char>(pick | 0x80);
    B(at::kStep3) = static_cast<unsigned char>(pick + 3);
    B(at::kStep4) = 0;
    B(at::kGeneRow) = 0;
}

// original 0x4501E0 (0x64ECF0[2]): window 21's +3 = 2 (closing), the next step.
extern "C" void __cdecl DragonCmd_MenuClose(void) {
    CloseList(at::kWin21);
    NextStep();
}

// original 0x450200 (0x64ECF0[3]): window 21 closed (+0 bit 0 clear): 0x447F40,
// windows 17 and 21's +0 zeroed, BattleBanner_ShowName(the menu actor),
// window 4's +3 = 1, LoadDatFile(0xCF), the next step.
extern "C" void __cdecl DragonCmd_MenuCancel(void) {
    if (B(at::kWin21) & 1) return;
    BH_AT(Void0, at::kTargetPrompt)();
    B(at::kWin17) = 0;
    B(at::kWin21) = 0;
    BH_CALL(BattleBanner_ShowName)(Member());
    B(at::kWin4 + 3) = 1;
    BH_CALL(LoadDatFile)(0xCF);
    NextStep();
}

// original 0x450250 (0x64ECF0[4]; not in the cut): File_LoadDone: CLUT row
// 0x1B copied, Gfx_ClutStripDirty = 1, the command step 0x904AA2 and the part
// 0x904AA3 = 1, 0x904AA4 = 0.
extern "C" void __cdecl DragonCmd_MenuCancelLoad(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Gfx_ClutStripCopyRow)(0x1B);
    Gfx_ClutStripDirty = 1;
    B(at::kStep2) = 1;
    B(at::kStep3) = 1;
    B(at::kStep4) = 0;
}

// --- part 3: the first list (0x904608, window 18) --------------------------------------

// original 0x450280 (0x64ECCC[3]): jmp [0x64ED04 + 4 * 0x904AA4], seven steps.
extern "C" unsigned long __cdecl DragonCmd_SlotsDispatch(unsigned long through) {
    return Dispatch("DragonCmd_SlotsDispatch", at::kSlotsSteps, 7, through);
}

// original 0x4502A0 (0x64ED04[0]): window 21's +3 = 2, +0xA = 0; the next step.
extern "C" void __cdecl DragonCmd_SlotsCloseMenu(void) {
    const unsigned char step = static_cast<unsigned char>(B(at::kStep4) + 1);
    B(at::kWin21 + 3) = 2;
    B(at::kWin21 + 0xA) = 0;
    B(at::kStep4) = step;
}

// original 0x4502C0 (0x64ED04[1]): window 21 closed: its +0 zeroed, window 18
// claimed (kind 5; +2 2, +3 0, at 0xFF5B, 0x3F, its cursor 0, 0), line 0x59,
// 0x904AA5 |= 0x80, the next step.
extern "C" void __cdecl DragonCmd_SlotsOpen(void) { OpenList(0x12, at::kWin18, 2, 0xFF5B, 0x59); }

// original 0x450340 (0x64ED04[2]): the first list's cursor, once window 18's
// x is 0x5B. The repeat keys of 0x500C; cancel: sound 0x106, the next step
// (3, back); confirm: the slot affordable (DragonCmd_SlotAffordable) - sound
// 0x104, step 5 (take it) - else sound 0x107 and Input_Pressed read again;
// then its bit 0x10: the slot not empty - sound 0x104, part 6 (the store),
// step 0 - else sound 0x107; then the cursor moved in six slots (MoveCursor:
// a + b below 5, a at most 3).
extern "C" void __cdecl DragonCmd_SlotsCursor(void) {
    if (W(at::kWin18 + 4) != 0x5B) return;
    const U keys = Repeat(0x500C);
    U pressed = Pressed();
    if (Cancel(pressed)) {
        Sound(0x106);
        NextStep();
        return;
    }
    if (Confirm(pressed)) {
        if (BH_CALL(DragonCmd_SlotAffordable)() & 0xFF) {
            Sound(0x104);
            B(at::kStep4) = 5;
            return;
        }
        Sound(0x107);
        pressed = Pressed();
    }
    if (pressed & 0x10) {
        if (B(SlotOf(at::kSlots, at::kWin18) + 3) != 0xFF) {
            Sound(0x104);
            B(at::kStep3) = 6;
            B(at::kStep4) = 0;
            return;
        }
        Sound(0x107);
    }
    MoveCursor(at::kWin18, keys, 5, 3);
}

// original 0x450510 (called by DragonCmd_SlotsCursor): the first list's slot
// at the cursor priced against the member's AP (Affordable). al out.
extern "C" unsigned char __cdecl DragonCmd_SlotAffordable(void) { return Affordable(SlotOf(at::kSlots, at::kWin18)); }

// original 0x4505A0 (0x64ED04[3]): window 21 claimed again (+2 / +3 0, at
// 0x142, 0x3F), window 18's +3 = 2, the next step.
extern "C" void __cdecl DragonCmd_SlotsCancel(void) {
    ReopenMenu(0x142);
    const unsigned char step = static_cast<unsigned char>(B(at::kStep4) + 1);
    B(at::kWin18 + 3) = 2;
    B(at::kStep4) = step;
}

// original 0x4505E0 (0x64ED04[4]): window 18 closed: back to the menu (part 2,
// step 0, 0x904AA5's bit 7 cleared).
extern "C" void __cdecl DragonCmd_SlotsBack(void) { BackToMenu(at::kWin18); }

// original 0x450610 (0x64ED04[5]): the slot at the cursor taken (TakeSlot),
// 0x4525B0, window 18's +3 = 2, the member's +0x134 |= 4, the next step.
extern "C" void __cdecl DragonCmd_SlotsConfirm(void) {
    TakeSlot(SlotOf(at::kSlots, at::kWin18));
    BH_AT(Void0, at::kDragonTask)();
    unsigned char* const m = Member();
    B(at::kWin18 + 3) = 2;
    SetLong(m + 0x134, static_cast<S32>(static_cast<U>(Long(m + 0x134)) | 4u));
    NextStep();
}

// original 0x450680 (0x64ED04[6]): window 18 closed: 0x447F40, windows 17,
// 18, 16, 19's +0 zeroed, LoadDatFile(0xCF), then part 4's step 3 (the
// gene grid's close and commit).
extern "C" void __cdecl DragonCmd_SlotsClose(void) {
    if (B(at::kWin18) & 1) return;
    BH_AT(Void0, at::kTargetPrompt)();
    B(at::kWin17) = 0;
    B(at::kWin18) = 0;
    B(at::kWin16) = 0;
    B(at::kWin19) = 0;
    BH_CALL(LoadDatFile)(0xCF);
    B(at::kStep3) = 4;
    B(at::kStep4) = 3;
}

// --- part 4: the gene grid ------------------------------------------------------------

// original 0x4506C0 (0x64ECCC[4]): jmp [0x64ED20 + 4 * 0x904AA4], five steps.
extern "C" unsigned long __cdecl DragonCmd_GenesDispatch(unsigned long through) {
    return Dispatch("DragonCmd_GenesDispatch", at::kGenesSteps, 5, through);
}

// original 0x4506E0 (0x64ED20[0]): window 17's +3 = 2, window 4's +3 = 1, the
// next step.
extern "C" void __cdecl DragonCmd_GenesOpen(void) {
    const unsigned char step = static_cast<unsigned char>(B(at::kStep4) + 1);
    B(at::kWin17 + 3) = 2;
    B(at::kWin4 + 3) = 1;
    B(at::kStep4) = step;
}

// original 0x450700 (0x64ED20[1]): the gene grid, three rows of six (0x904AA6,
// 0x904AA7; row 0xFF the "done" line), genes picked into 0x904B84.. (three at
// most).
//   The banner: line 0x4172 + 6 * row + column for a gene held, else 0x4000,
//   through BattleBanner_Set(0, 1, 1, 0, 0xFF, text).
//   Cancel: a gene picked - the last put back (its flag set again), 0x4525B0,
//   sound 0x106; none - windows 17 / 4's +3 = 1 / 2, back to the menu (part
//   2), sound 0x106.
//   Confirm on the done line: genes picked and the member's AP (+0x9A) at
//   least 0x904B78 - the next step; else sound 0x107. On a gene: not held -
//   sound 0x107, nothing more; three picked - sound 0x106; else the gene
//   (6 * row + column) picked, its flag cleared, 0x4525B0, sound 0x103, and
//   at three the row set to the done line with line 0x57.
//   The message line: on the done line 0x57 with genes picked, 0x58 without;
//   on a gene 0x45 + 6 * row + column when held, else 0x4000.
//   The repeat keys of 0xF000: 0x1000 a row up while it is 0..0x7F, 0x4000 a
//   row down while it is below 2 (signed: from 0xFF to 0); off the done line
//   0x2000 / 0x8000 a column right below 5 / left above 0; sound 0x101 each.
extern "C" void __cdecl DragonCmd_GenesPick(void) {
    {
        unsigned id = 0x4000;
        const unsigned char row = B(at::kGeneRow);
        if (row != 0xFF) {
            const unsigned col = B(at::kGeneCol);
            if (B(at::kGeneFlags + col + 6u * row) != 0) id = (col + 6u * row + 0x4172) & 0xFFFF;
        }
        const unsigned char* const text = Line(id);
        BH_CALL(BattleBanner_Set)(0, 1, 1, 0, 0xFF, reinterpret_cast<const char*>(text));
    }
    const U pressed = Pressed();
    bool shown = false;
    if (Cancel(pressed)) {
        const unsigned char n = B(at::kGeneCount);
        if (n != 0) {
            const unsigned char last = static_cast<unsigned char>(n - 1);
            B(at::kGeneCount) = last;
            B(at::kGeneFlags + B(at::kGenes + last)) = 1;
            BH_AT(Void0, at::kDragonTask)();
            Sound(0x106);
            return;
        }
        const unsigned char pick = static_cast<unsigned char>(B(at::kPick) & 0x7F);
        B(at::kWin17 + 3) = 1;
        B(at::kWin4 + 3) = 2;
        B(at::kPick) = pick;
        B(at::kStep3) = 2;
        B(at::kStep4) = 0;
        Sound(0x106);
        return;
    }
    if (Confirm(pressed)) {
        const unsigned char row = B(at::kGeneRow);
        if (row == 0xFF) {
            if (B(at::kGeneCount) != 0 && Word(Member() + 0x9A) >= B(at::kGeneCost)) {
                NextStep();
                return;
            }
            Sound(0x107);
        } else {
            const unsigned char col = B(at::kGeneCol);
            unsigned char* const flag = At(at::kGeneFlags + col + 6u * row);
            if (*flag == 0) {
                Sound(0x107);
                return;
            }
            const unsigned char n = B(at::kGeneCount);
            if (n >= 3) {
                Sound(0x106);
            } else {
                *flag = 0;
                B(at::kGenes + n) = static_cast<unsigned char>(row * 6 + col);
                B(at::kGeneCount) = static_cast<unsigned char>(B(at::kGeneCount) + 1);
                BH_AT(Void0, at::kDragonTask)();
                Sound(0x103);
                if (B(at::kGeneCount) == 3) {
                    B(at::kGeneRow) = 0xFF;
                    ShowLine(Line(0x57));
                    shown = true;
                }
            }
        }
    }
    if (!shown) {
        const unsigned char row = B(at::kGeneRow);
        unsigned id;
        if (row == 0xFF) {
            id = B(at::kGeneCount) != 0 ? 0x57 : 0x58;
        } else {
            const unsigned char col = B(at::kGeneCol);
            id = B(at::kGeneFlags + col + 6u * row) != 0 ? (col + 6u * row + 0x45) & 0xFFFF : 0x4000;
        }
        ShowLine(Line(id));
    }
    const U keys = Repeat(0xF000);
    if ((keys & 0x1000) && static_cast<signed char>(B(at::kGeneRow)) > -1) {
        Sound(0x101);
        B(at::kGeneRow) = static_cast<unsigned char>(B(at::kGeneRow) - 1);
    }
    if ((keys & 0x4000) && static_cast<signed char>(B(at::kGeneRow)) < 2) {
        Sound(0x101);
        B(at::kGeneRow) = static_cast<unsigned char>(B(at::kGeneRow) + 1);
    }
    if (B(at::kGeneRow) == 0xFF) return;
    if ((keys & 0x2000) && B(at::kGeneCol) < 5) {
        Sound(0x101);
        B(at::kGeneCol) = static_cast<unsigned char>(B(at::kGeneCol) + 1);
    }
    if ((keys & 0x8000) && B(at::kGeneCol) != 0) {
        Sound(0x101);
        B(at::kGeneCol) = static_cast<unsigned char>(B(at::kGeneCol) - 1);
    }
}

// original 0x450A30 (0x64ED20[2]): sound 0x104, window 21's +3 = 2, the
// member's +0x134 |= 4, the next step.
extern "C" void __cdecl DragonCmd_GenesConfirm(void) {
    Sound(0x104);
    unsigned char* const m = Member();
    B(at::kWin21 + 3) = 2;
    SetLong(m + 0x134, static_cast<S32>(static_cast<U>(Long(m + 0x134)) | 4u));
    NextStep();
}

// original 0x450A70 (0x64ED20[3]): window 21 closed: 0x447F40, windows 17, 21,
// 16, 19's +0 zeroed, LoadDatFile(0xCF), the next step.
extern "C" void __cdecl DragonCmd_GenesClose(void) {
    if (B(at::kWin21) & 1) return;
    BH_AT(Void0, at::kTargetPrompt)();
    B(at::kWin17) = 0;
    B(at::kWin21) = 0;
    B(at::kWin16) = 0;
    B(at::kWin19) = 0;
    BH_CALL(LoadDatFile)(0xCF);
    NextStep();
}

// original 0x450AB0 (0x64ED20[4]): File_LoadDone: CLUT row 0x1B copied; the
// command record 0x939FA0's +1 = 4, Gfx_ClutStripDirty = 1, 0x904AAF = 0, the
// menu actor's +1 = 2, 0x904AC3 + 1, the phase's step 0x904AA1 = 1 and
// 0x904AA2..AA4 zeroed, the message window's last line's +1 byte = 1.
extern "C" void __cdecl DragonCmd_Commit(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Gfx_ClutStripCopyRow)(0x1B);
    unsigned char* const command = Ptr(at::kCommandRecord);
    Gfx_ClutStripDirty = 1;
    B(at::kCommitClear) = 0;
    command[1] = 4;
    Member()[1] = 2;
    const unsigned char count = static_cast<unsigned char>(B(at::kCommitCount) + 1);
    B(at::kStep1) = 1;
    B(at::kCommitCount) = count;
    const unsigned line = (B(at::kMsgCount) - 1u) & 0xF;
    B(at::kStep2) = 0;
    B(at::kStep3) = 0;
    B(at::kStep4) = 0;
    B(at::kMsgFlag + line * 8) = 1;
}

// --- part 5: the second list (0x904620, window 19) ------------------------------------

// original 0x450B20 (0x64ECCC[5]): jmp [0x64ED34 + 4 * 0x904AA4], seven steps.
extern "C" unsigned long __cdecl DragonCmd_Slots2Dispatch(unsigned long through) {
    return Dispatch("DragonCmd_Slots2Dispatch", at::kSlots2Steps, 7, through);
}

// original 0x450B40 (0x64ED34[0]): window 21's +3 = 2, +0xA = 1; the next step.
extern "C" void __cdecl DragonCmd_Slots2CloseMenu(void) {
    const unsigned char step = static_cast<unsigned char>(B(at::kStep4) + 1);
    B(at::kWin21 + 3) = 2;
    B(at::kWin21 + 0xA) = 1;
    B(at::kStep4) = step;
}

// original 0x450B60 (0x64ED34[1]): as DragonCmd_SlotsOpen for window 19 (slot
// 0x13; +2 3, at 0x15B) and line 0x63.
extern "C" void __cdecl DragonCmd_Slots2Open(void) { OpenList(0x13, at::kWin19, 3, 0x15B, 0x63); }

// original 0x450BE0 (0x64ED34[2]): the second list's cursor, once window 19's
// x is 0x5B: as DragonCmd_SlotsCursor without the bit-0x10 branch - cancel:
// sound 0x106, the next step; confirm: DragonCmd_Slot2Affordable - sound
// 0x104, step 5 - else sound 0x107; the cursor in twelve slots (a + b below
// 0xB, a at most 9).
extern "C" void __cdecl DragonCmd_Slots2Cursor(void) {
    if (W(at::kWin19 + 4) != 0x5B) return;
    const U keys = Repeat(0x500C);
    const U pressed = Pressed();
    if (Cancel(pressed)) {
        Sound(0x106);
        NextStep();
        return;
    }
    if (Confirm(pressed)) {
        if (BH_CALL(DragonCmd_Slot2Affordable)() & 0xFF) {
            Sound(0x104);
            B(at::kStep4) = 5;
            return;
        }
        Sound(0x107);
    }
    MoveCursor(at::kWin19, keys, 0xB, 9);
}

// original 0x450D60 (called by DragonCmd_Slots2Cursor): the second list's
// slot at the cursor priced against the member's AP. al out.
extern "C" unsigned char __cdecl DragonCmd_Slot2Affordable(void) { return Affordable(SlotOf(at::kSlots2, at::kWin19)); }

// original 0x450DF0 (0x64ED34[3]): window 21 claimed again (at 0xFF42, 0x3F,
// +0xA 1), window 19's +3 = 2, the next step.
extern "C" void __cdecl DragonCmd_Slots2Cancel(void) {
    ReopenMenu(0xFF42);
    const unsigned char step = static_cast<unsigned char>(B(at::kStep4) + 1);
    B(at::kWin21 + 0xA) = 1;
    B(at::kWin19 + 3) = 2;
    B(at::kStep4) = step;
}

// original 0x450E40 (0x64ED34[4]): window 19 closed: back to the menu.
extern "C" void __cdecl DragonCmd_Slots2Back(void) { BackToMenu(at::kWin19); }

// original 0x450E70 (0x64ED34[5]): the second list's slot taken (TakeSlot),
// 0x4525B0, sound 0x104, window 19's +3 = 2, the member's +0x134 |= 4, the
// next step.
extern "C" void __cdecl DragonCmd_Slots2Confirm(void) {
    TakeSlot(SlotOf(at::kSlots2, at::kWin19));
    BH_AT(Void0, at::kDragonTask)();
    Sound(0x104);
    unsigned char* const m = Member();
    B(at::kWin19 + 3) = 2;
    SetLong(m + 0x134, static_cast<S32>(static_cast<U>(Long(m + 0x134)) | 4u));
    NextStep();
}

// original 0x450EF0 (0x64ED34[6]): window 18 closed (sic: the first list's
// record is the one tested): 0x447F40, windows 17, 16, 19's +0 zeroed,
// LoadDatFile(0xCF), part 4's step 3.
extern "C" void __cdecl DragonCmd_Slots2Close(void) {
    if (B(at::kWin18) & 1) return;
    BH_AT(Void0, at::kTargetPrompt)();
    B(at::kWin17) = 0;
    B(at::kWin16) = 0;
    B(at::kWin19) = 0;
    BH_CALL(LoadDatFile)(0xCF);
    B(at::kStep3) = 4;
    B(at::kStep4) = 3;
}

// --- part 6: the store (a first-list slot into the second list) --------------------------

// original 0x450F30 (0x64ECCC[6]): jmp [0x64ED50 + 4 * 0x904AA4], seven steps
// (the last, 0x451480, BE6's).
extern "C" unsigned long __cdecl DragonCmd_StoreDispatch(unsigned long through) {
    return Dispatch("DragonCmd_StoreDispatch", at::kStoreSteps, 7, through);
}

// original 0x450F50 (0x64ED50[0]): window 18's +3 = 3, window 19 claimed
// (slot 0x13: +3 0, its cursor 0, 0, +2 4, at 0x15B, 0x3F), the next step.
extern "C" void __cdecl DragonCmd_StoreOpen(void) {
    B(at::kWin18 + 3) = 3;
    BH_CALL(Window_Alloc)(0x13, 5);
    B(at::kWin19 + 3) = 0;
    SetW(at::kWin19 + 0x10, 0);
    SetW(at::kWin19 + 0x12, 0);
    const unsigned char step = static_cast<unsigned char>(B(at::kStep4) + 1);
    B(at::kWin19 + 2) = 4;
    SetW(at::kWin19 + 4, 0x15B);
    SetW(at::kWin19 + 6, 0x3F);
    B(at::kStep4) = step;
}

// original 0x450FA0 (0x64ED50[1]): window 19's x 0xA3: the next step.
extern "C" void __cdecl DragonCmd_StoreWait(void) {
    if (W(at::kWin19 + 4) == 0xA3) NextStep();
}

// original 0x450FC0 (0x64ED50[2]): the destination's cursor: line 0x61; the
// repeat keys of 0x500C; cancel: sound 0x106, step 5; confirm: step 4 for an
// empty second-list slot, else the prompt's answer 0x904B74 = 0 and step 3,
// sound 0x104 - and the cursor still moves; the cursor in twelve slots.
extern "C" void __cdecl DragonCmd_StoreCursor(void) {
    ShowLine(Line(0x61));
    const U keys = Repeat(0x500C);
    const U pressed = Pressed();
    if (Cancel(pressed)) {
        Sound(0x106);
        B(at::kStep4) = 5;
        return;
    }
    if (Confirm(pressed)) {
        if (B(SlotOf(at::kSlots2, at::kWin19) + 3) == 0xFF) {
            B(at::kStep4) = 4;
        } else {
            SetW(at::kAsk, 0);
            B(at::kStep4) = 3;
        }
        Sound(0x104);
    }
    MoveCursor(at::kWin19, keys, 0xB, 9);
}

// original 0x451160 (0x64ED50[3]): the overwrite prompt: line 0x62; the
// repeat keys of 0xA000; cancel, or confirm on answer 1: sound 0x106, a step
// back; confirm on 0: sound 0x104, the next step; else a repeat key flips the
// answer (0x904B74 ^= 1); the hand at 0xD5 + 0x22 * answer (the dword), 0x18.
extern "C" void __cdecl DragonCmd_StoreAsk(void) {
    ShowLine(Line(0x62));
    const U keys = Repeat(0xA000);
    const U pressed = Pressed();
    if (Cancel(pressed) || (Confirm(pressed) && W(at::kAsk) != 0)) {
        Sound(0x106);
        B(at::kStep4) = static_cast<unsigned char>(B(at::kStep4) - 1);
        return;
    }
    if (Confirm(pressed)) {
        Sound(0x104);
        NextStep();
        return;
    }
    if (keys & 0xA000) SetW(at::kAsk, W(at::kAsk) ^ 1u);
    const U answer = L(at::kAsk);
    BH_CALL(Menu_DrawHand)(static_cast<int>(answer * 0x22 + 0xD5), 0x18, 0);
}

// original 0x451220 (0x64ED50[4]): the first list's slot at its cursor copied
// into the second list's at its cursor, byte by byte (each read before its
// write, as the original), the next step.
extern "C" void __cdecl DragonCmd_StoreCopy(void) {
    const U from = SlotOf(at::kSlots, at::kWin18);
    const U to = SlotOf(at::kSlots2, at::kWin19);
    for (unsigned j = 0; j < 4; ++j) B(to + j) = B(from + j);
    NextStep();
}

// original 0x451290 (0x64ED50[5]): the source's cursor on the first list:
// line 0x60; the repeat keys of 0x500C; cancel: sound 0x106, the next step
// (BE6's 0x451480); confirm (with bit 0x10 of the pressed keys counted as
// one): the slot not empty - sound 0x104, step 2 - else sound 0x107 and
// Input_Pressed read again; then its bit 0x10 the same test once more; the
// cursor in six slots.
extern "C" void __cdecl DragonCmd_StoreSource(void) {
    ShowLine(Line(0x60));
    const U keys = Repeat(0x500C);
    U pressed = Pressed();
    if (Cancel(pressed)) {
        Sound(0x106);
        NextStep();
        return;
    }
    if (((Field_ConfirmButtons | 0x10u) & pressed & 0xFFFF) != 0) {
        if (B(SlotOf(at::kSlots, at::kWin18) + 3) != 0xFF) {
            Sound(0x104);
            B(at::kStep4) = 2;
            return;
        }
        Sound(0x107);
        pressed = Pressed();
    }
    if (pressed & 0x10) {
        if (B(SlotOf(at::kSlots, at::kWin18) + 3) != 0xFF) {
            Sound(0x104);
            B(at::kStep4) = 2;
            return;
        }
        Sound(0x107);
    }
    MoveCursor(at::kWin18, keys, 5, 3);
}

// ============================================================================

void BattleE5_Inject() {
    if (bof3::WantsShadow("battle_e5")) battle_e5::SelfTest();
    BOF3_INJECT(EnemyAI_OtherRowsDone);
    BOF3_INJECT(EnemyAI_SetRowDone);
    BOF3_INJECT(EnemyAI_CondElement);
    BOF3_INJECT(EnemyAI_ApplyAction);
    BOF3_INJECT(EnemyAI_ScaleStat);
    BOF3_INJECT(EnemyAI_SetAttrByte);
    BOF3_INJECT(EnemyAI_DedupMessages);
    BOF3_INJECT(Effect_DrainHp);
    BOF3_INJECT(Effect_DrainAp);
    BOF3_INJECT(Effect_QuarterAttack);
    BOF3_INJECT(BattleForm_ApplyStats);
    BOF3_INJECT(DragonCmd_LoadDispatch);
    BOF3_INJECT(DragonCmd_LoadStart);
    BOF3_INJECT(DragonCmd_LoadWait);
    BOF3_INJECT(DragonCmd_Open);
    BOF3_INJECT(DragonCmd_MenuDispatch);
    BOF3_INJECT(DragonCmd_MenuInput);
    BOF3_INJECT(DragonCmd_MenuPick);
    BOF3_INJECT(DragonCmd_MenuClose);
    BOF3_INJECT(DragonCmd_MenuCancel);
    BOF3_INJECT(DragonCmd_MenuCancelLoad);
    BOF3_INJECT(DragonCmd_SlotsDispatch);
    BOF3_INJECT(DragonCmd_SlotsCloseMenu);
    BOF3_INJECT(DragonCmd_SlotsOpen);
    BOF3_INJECT(DragonCmd_SlotsCursor);
    BOF3_INJECT(DragonCmd_SlotAffordable);
    BOF3_INJECT(DragonCmd_SlotsCancel);
    BOF3_INJECT(DragonCmd_SlotsBack);
    BOF3_INJECT(DragonCmd_SlotsConfirm);
    BOF3_INJECT(DragonCmd_SlotsClose);
    BOF3_INJECT(DragonCmd_GenesDispatch);
    BOF3_INJECT(DragonCmd_GenesOpen);
    BOF3_INJECT(DragonCmd_GenesPick);
    BOF3_INJECT(DragonCmd_GenesConfirm);
    BOF3_INJECT(DragonCmd_GenesClose);
    BOF3_INJECT(DragonCmd_Commit);
    BOF3_INJECT(DragonCmd_Slots2Dispatch);
    BOF3_INJECT(DragonCmd_Slots2CloseMenu);
    BOF3_INJECT(DragonCmd_Slots2Open);
    BOF3_INJECT(DragonCmd_Slots2Cursor);
    BOF3_INJECT(DragonCmd_Slot2Affordable);
    BOF3_INJECT(DragonCmd_Slots2Cancel);
    BOF3_INJECT(DragonCmd_Slots2Back);
    BOF3_INJECT(DragonCmd_Slots2Confirm);
    BOF3_INJECT(DragonCmd_Slots2Close);
    BOF3_INJECT(DragonCmd_StoreDispatch);
    BOF3_INJECT(DragonCmd_StoreOpen);
    BOF3_INJECT(DragonCmd_StoreWait);
    BOF3_INJECT(DragonCmd_StoreCursor);
    BOF3_INJECT(DragonCmd_StoreAsk);
    BOF3_INJECT(DragonCmd_StoreCopy);
    BOF3_INJECT(DragonCmd_StoreSource);
}
