// The field menu's item effects, and the few small functions next to them in
// the takeover queue. docs/item-use.md.
//
//   - ItemUse_Dispatch 0x497680 and the 33 handlers of ItemUse_Handlers
//     (0x496CC0..0x497670): pointer-reached, so in no function list until
//     2026-09-22 - pe_funcs.py had folded them into 0x496AD0's 0xBAA bytes.
//     The PSX twins are START.EMI's 0x801E7E34..0x801E8C1C, read side by side.
//   - The character-stat helpers under them, 0x590CE0..0x590F60 (the PSX
//     boot EXE's 0x80165C70..0x801660DC).
//   - The message box's two commits behind the area descriptors, 0x4981C0 and
//     0x4983C0 (group H left them: docs/msgbox.md section 4), and the sixteen
//     handlers MsgBox_SystemChoice 0x498A30 reaches for choice ids 0x80..0x8F.
//   - The pad auto-repeat Input_AutoRepeat 0x461EB0.
//
// Every call goes through item_use::g (item_use_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike.
#include "game/item_use.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/item_use_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace item_use {

using move_script::At;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Char_HealHp,
    Char_HealAp,
    Char_ClearStatus,
    Stat_AddCap999,
    Stat_AddCap99,
    Stat_AddClamped,
    Party_Count,
    Rand,
    Char_RecalcStats,
    Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(kInventoryAdd),
    Flags_Test,
    Fn<void (__cdecl*)()>(kSystemChoice),
};
Callees g = kOriginals;

}  // namespace item_use

using namespace item_use;

namespace {

// The record a (id, battle) pair names, as every function here computes it:
// the battle flag's low byte picks the persistent record or the member's
// working copy, the id's low byte indexes it, unchecked.
unsigned char* CharRecord(unsigned id, unsigned battle) {
    const unsigned i = id & 0xFF;
    return (battle & 0xFF) == 0 ? At(at::kCharRecords + i * at::kCharStride) : At(at::kWorkRecords + i * at::kWorkStride);
}

// The handlers' common tail: `neg al; sbb eax, eax; and al, 0xFD; add eax, 3`
// - 0 when the callee answered anything in al, 3 when it answered 0.
unsigned char UsedOr3(unsigned char answer) { return answer != 0 ? 0 : 3; }

// Party member i's record index: 0x66972C[0x904062[i]] (the party list's
// entry, then its record).
unsigned Member(unsigned char i) { return At(at::kMemberRecord)[At(at::kPartyLists)[i]]; }

// The party walk of 0x496D50 / 0x497410 / 0x497490 / 0x497510: Party_Count(0)
// asked before the first member and again after each, compared as bytes. The
// member's index is stored into the low byte of the id argument's own slot
// and that dword pushed, so the upper bytes are the caller's: `id` is that
// slot here.
template <typename Each> unsigned char PartyWalk(unsigned id, Each each) {
    unsigned char result = 0;
    unsigned char i = 0;
    if (static_cast<unsigned char>(g.party_count(0)) != 0) {
        do {
            id = (id & 0xFFFFFF00u) | Member(i);
            result = static_cast<unsigned char>(result | each(id));
            ++i;
        } while (i < static_cast<unsigned char>(g.party_count(0)));
    }
    return result;
}

// The stat-up tail of 0x497040..0x4972E0: 0 if either call answered non-zero
// in al, else 4. The field menu treats 0 and 4 alike (item used).
unsigned char UsedOr4(unsigned char first, unsigned char second) { return (first != 0 || second != 0) ? 0 : 4; }

}  // namespace

// ===========================================================================
// The dispatcher

// original 0x497680 (PSX 0x801E8C1C): 2 for an item whose flags byte is 0,
// else its handler's answer. The table is read from memory, as the original
// does; both arguments reach the handler as the caller pushed them. The call
// is not a tail call, as it is not in the original (the handler's caller is
// this function, for the call trace).
extern "C" __attribute__((disable_tail_calls)) unsigned char __cdecl ItemUse_Dispatch(unsigned id, unsigned item, unsigned battle) {
    const unsigned it = item & 0xFF;
    if (At(at::kItemFlags)[it * at::kItemStride] == 0) return 2;
    const auto handler = reinterpret_cast<Handler>(ItemUse_Handlers[ItemUse_HandlerIndex[it]]);
    const unsigned char answer = handler(id, battle);
    return answer;
}

// ===========================================================================
// The 33 handlers, in table order (ItemUse_Handlers entry in brackets)

// original 0x496CC0 [0]: no field effect.
extern "C" unsigned char __cdecl ItemUse_None(unsigned, unsigned) { return 2; }

// original 0x496CD0 [1], 0x496CF0 [2], 0x496D10 [3], 0x496D30 [4] (0 is a
// full heal), 0x4973D0 [23], 0x4973F0 [25], 0x4975D0 [31]: Char_HealHp by a
// constant.
extern "C" unsigned char __cdecl ItemUse_HealHp20(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 0x14, battle)); }
extern "C" unsigned char __cdecl ItemUse_HealHp40(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 0x28, battle)); }
extern "C" unsigned char __cdecl ItemUse_HealHp100(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 0x64, battle)); }
extern "C" unsigned char __cdecl ItemUse_HealHpFull(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 0, battle)); }
extern "C" unsigned char __cdecl ItemUse_HealHp1(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 1, battle)); }
extern "C" unsigned char __cdecl ItemUse_HealHp80(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 0x50, battle)); }
extern "C" unsigned char __cdecl ItemUse_HealHp5(unsigned id, unsigned battle) { return UsedOr3(g.heal_hp(id, 5, battle)); }

// original 0x496DD0 [6], 0x496DF0 [7], 0x497590 [29], 0x4975B0 [24, 30]:
// Char_HealAp by a constant.
extern "C" unsigned char __cdecl ItemUse_HealAp20(unsigned id, unsigned battle) { return UsedOr3(g.heal_ap(id, 0x14, battle)); }
extern "C" unsigned char __cdecl ItemUse_HealAp100(unsigned id, unsigned battle) { return UsedOr3(g.heal_ap(id, 0x64, battle)); }
extern "C" unsigned char __cdecl ItemUse_HealAp40(unsigned id, unsigned battle) { return UsedOr3(g.heal_ap(id, 0x28, battle)); }
extern "C" unsigned char __cdecl ItemUse_HealAp5(unsigned id, unsigned battle) { return UsedOr3(g.heal_ap(id, 5, battle)); }

// original 0x496EA0 [9], 0x496ED0 [10], 0x496EF0 [11], 0x496F20 [12]:
// Char_ClearStatus by a constant mask.
extern "C" unsigned char __cdecl ItemUse_Cure80(unsigned id, unsigned battle) { return UsedOr3(g.clear_status(id, 0x80, battle)); }
extern "C" unsigned char __cdecl ItemUse_Cure08(unsigned id, unsigned battle) { return UsedOr3(g.clear_status(id, 8, battle)); }
extern "C" unsigned char __cdecl ItemUse_Cure100(unsigned id, unsigned battle) { return UsedOr3(g.clear_status(id, 0x100, battle)); }
extern "C" unsigned char __cdecl ItemUse_CureA0(unsigned id, unsigned battle) { return UsedOr3(g.clear_status(id, 0xA0, battle)); }

// original 0x496D50 [5], 0x497410 [26], 0x497490 [27]: Char_HealHp for each
// party member; 0x497510 [28]: Char_ClearStatus(0x80) for each.
extern "C" unsigned char __cdecl ItemUse_PartyHealHp100(unsigned id, unsigned battle) {
    return UsedOr3(PartyWalk(id, [battle](unsigned m) { return g.heal_hp(m, 0x64, battle); }));
}
extern "C" unsigned char __cdecl ItemUse_PartyHealHp80(unsigned id, unsigned battle) {
    return UsedOr3(PartyWalk(id, [battle](unsigned m) { return g.heal_hp(m, 0x50, battle); }));
}
extern "C" unsigned char __cdecl ItemUse_PartyHealHp240(unsigned id, unsigned battle) {
    return UsedOr3(PartyWalk(id, [battle](unsigned m) { return g.heal_hp(m, 0xF0, battle); }));
}
extern "C" unsigned char __cdecl ItemUse_PartyCure80(unsigned id, unsigned battle) {
    return UsedOr3(PartyWalk(id, [battle](unsigned m) { return g.clear_status(m, 0x80, battle); }));
}

// original 0x496FB0 [14]: for each member Char_ClearStatus(0xA0) then a full
// Char_HealHp, every answer ORed.
extern "C" unsigned char __cdecl ItemUse_PartyRestore(unsigned id, unsigned battle) {
    return UsedOr3(PartyWalk(id, [battle](unsigned m) {
        const unsigned char cured = g.clear_status(m, 0xA0, battle);
        return static_cast<unsigned char>(cured | g.heal_hp(m, 0, battle));
    }));
}

// original 0x496E10 [8]: 5 HP; then, with a bit of 0xA0 in the status byte
// (read after the heal), the item counts as used and a 30 % roll clears 0xA0.
// As the original has it: the roll is Rand() % 100 by signed division, and
// "used" does not depend on it.
extern "C" unsigned char __cdecl ItemUse_HealHp5Cure(unsigned id, unsigned battle) {
    unsigned char r = g.heal_hp(id, 5, battle);
    if (CharRecord(id, battle)[at::kStatus] & 0xA0) {
        r = static_cast<unsigned char>(r | 1);
        if (g.rand() % 100 < 30) g.clear_status(id, 0xA0, battle);
    }
    return UsedOr3(r);
}

// original 0x496F50 [13]: HP 1, then Char_ClearStatus(0x4000). As the
// original has it (and the PSX): the HP is set before the status is tested,
// so on a member without 0x4000 the HP is 1 and the answer is 3 - D21, kept.
extern "C" unsigned char __cdecl ItemUse_Revive(unsigned id, unsigned battle) {
    SetWord(CharRecord(id, battle) + at::kHp, 1);
    return UsedOr3(g.clear_status(id, 0x4000, battle));
}

// original 0x497040 [15], 0x4970B0 [16]: Stat_AddCap999 on a base stat and its
// effective copy 0x20 below.
extern "C" unsigned char __cdecl ItemUse_MaxHpUp(unsigned id, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    const unsigned char base = g.add_cap999(reinterpret_cast<unsigned short*>(r + 0x40), 1);
    return UsedOr4(base, g.add_cap999(reinterpret_cast<unsigned short*>(r + 0x20), 1));
}
extern "C" unsigned char __cdecl ItemUse_MaxApUp(unsigned id, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    const unsigned char base = g.add_cap999(reinterpret_cast<unsigned short*>(r + 0x42), 1);
    return UsedOr4(base, g.add_cap999(reinterpret_cast<unsigned short*>(r + 0x22), 1));
}

// original 0x497120 [17], 0x497190 [18], 0x497200 [19], 0x497270 [20]:
// Stat_AddClamped on a base stat and its effective copy, each answer's low
// byte kept. As the original has it: Stat_AddClamped answers 0 unless it
// clamped, so these answer 4 for any stat below 999 - "used" either way.
namespace {
unsigned char ClampedUp(unsigned id, unsigned battle, unsigned base_at) {
    unsigned char* const r = CharRecord(id, battle);
    const auto base = static_cast<unsigned char>(g.add_clamped(reinterpret_cast<unsigned short*>(r + base_at), 1));
    const auto eff = static_cast<unsigned char>(g.add_clamped(reinterpret_cast<unsigned short*>(r + base_at - 0x20), 1));
    return UsedOr4(base, eff);
}
}  // namespace
extern "C" unsigned char __cdecl ItemUse_AtkUp(unsigned id, unsigned battle) { return ClampedUp(id, battle, 0x44); }
extern "C" unsigned char __cdecl ItemUse_DefUp(unsigned id, unsigned battle) { return ClampedUp(id, battle, 0x46); }
extern "C" unsigned char __cdecl ItemUse_AgiUp(unsigned id, unsigned battle) { return ClampedUp(id, battle, 0x48); }
extern "C" unsigned char __cdecl ItemUse_IntUp(unsigned id, unsigned battle) { return ClampedUp(id, battle, 0x4A); }

// original 0x4972E0 [21]: Stat_AddCap99 on the byte +0x4E and its copy +0x2E.
extern "C" unsigned char __cdecl ItemUse_Stat2EUp(unsigned id, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    const unsigned char base = g.add_cap99(r + 0x4E, 1);
    return UsedOr4(base, g.add_cap99(r + 0x2E, 1));
}

// original 0x497350 [22]: 3 at full HP; else the max-HP scale byte + 1 while
// below 9, the stats recomputed and HP filled to the new max; at 9, HP filled
// if below max, else 3 (HP above max).
extern "C" unsigned char __cdecl ItemUse_HpScaleUp(unsigned id, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    const std::uint16_t hp = Word(r + at::kHp), max = Word(r + at::kMaxHp);
    if (hp == max) return 3;
    const unsigned char scale = r[at::kHpScale];
    if (scale < 9) {
        r[at::kHpScale] = static_cast<unsigned char>(scale + 1);
        g.recalc(r);
        SetWord(r + at::kHp, Word(r + at::kMaxHp));
        return 0;
    }
    if (hp < max) {
        SetWord(r + at::kHp, max);
        return 0;
    }
    return 3;
}

// original 0x4975F0 [32]: gives the Faerie Tiara (0x57) back, moves the
// menu on, counts the clear story flags from 0xFF down to the first set one
// (at most ten) off 0xB9, and keeps the leader's position and the area. The
// arguments are not read; Inventory_Add gets a fourth 0 it does not read.
extern "C" unsigned char __cdecl ItemUse_FaerieTiara(unsigned, unsigned) {
    g.inventory_add(0, 0x57, 1, 0);
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
    At(at::kMenuState)[0] = 0xA;
    At(at::kTiaraCount)[0] = 0xB9;
    for (int flag = 0xFF;;) {
        if (g.flags_test(At(at::kStoryFlags), static_cast<unsigned>(flag)) != 0) break;
        const auto count = static_cast<unsigned char>(At(at::kTiaraCount)[0] - 1);
        --flag;
        At(at::kTiaraCount)[0] = count;
        if (flag < 0xF6) break;
    }
    std::memcpy(At(at::kReturnPoint), At(at::kLeaderXZ), 8);
    SetWord(At(at::kReturnPoint + 8), Game_AreaNumber);
    return 0;
}

// original 0x497670 [33].
extern "C" unsigned char __cdecl ItemUse_WaterJug(unsigned, unsigned) {
    At(at::kWaterJug)[0] = 0xF0;
    return 0;
}

// ===========================================================================
// The character-stat helpers

// original 0x590CE0 (PSX 0x80165C70): 0 at full HP. Else HP += the amount's
// low word, filled to max when it passes max or the amount is 0, and the
// status bit 0x2000 cleared unless HP is below a quarter of max; 1.
// As the original has it: the addition wraps at 16 bits (HP 0xFFF0 + 0x20 is
// 0x10, below max, kept), and Char_ClearStatus gets the id and battle dwords
// as they were pushed.
extern "C" unsigned char __cdecl Char_HealHp(unsigned id, unsigned amount, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    if (Word(r + at::kHp) == Word(r + at::kMaxHp)) return 0;
    SetWord(r + at::kHp, Word(r + at::kHp) + amount);
    if (Word(r + at::kHp) > Word(r + at::kMaxHp) || static_cast<std::uint16_t>(amount) == 0)
        SetWord(r + at::kHp, Word(r + at::kMaxHp));
    if (static_cast<std::uint16_t>(Word(r + at::kMaxHp) >> 2) <= Word(r + at::kHp)) g.clear_status(id, 0x2000, battle);
    return 1;
}

// original 0x590D70 (PSX 0x80165D3C): Char_HealHp on AP, without the status.
extern "C" unsigned char __cdecl Char_HealAp(unsigned id, unsigned amount, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    if (Word(r + at::kAp) == Word(r + at::kMaxAp)) return 0;
    SetWord(r + at::kAp, Word(r + at::kAp) + amount);
    if (Word(r + at::kAp) > Word(r + at::kMaxAp) || static_cast<std::uint16_t>(amount) == 0)
        SetWord(r + at::kAp, Word(r + at::kMaxAp));
    return 1;
}

// original 0x590DE0 (PSX 0x80165E5C): 0 at 999; else add (wrapping at 16
// bits), 999 if above, 1.
extern "C" unsigned char __cdecl Stat_AddCap999(unsigned short* stat, unsigned amount) {
    auto* const p = reinterpret_cast<unsigned char*>(stat);
    const std::uint16_t v = Word(p);
    if (v == 999) return 0;
    const auto sum = static_cast<std::uint16_t>(v + amount);
    SetWord(p, sum);
    if (sum > 999) SetWord(p, 999);
    return 1;
}

// original 0x590E10 (PSX 0x80165EA0): the same on a byte, capped at 99.
extern "C" unsigned char __cdecl Stat_AddCap99(unsigned char* stat, unsigned amount) {
    const unsigned char v = *stat;
    if (v == 99) return 0;
    const auto sum = static_cast<unsigned char>(v + amount);
    *stat = sum;
    if (sum > 99) *stat = 99;
    return 1;
}

// original 0x590E30 (PSX 0x80165EE4): the delta as s16. Up: 0 at 999, else
// add and, past 999 (unsigned), store 999 and answer 999 - old. Down: 0 at 0,
// else add and, below 0 (s16), store 0 and answer -old. 0 otherwise - an
// unclamped change answers 0, as on the PSX. Only the low word of the answer
// is defined in the original (ax), so a short.
extern "C" short __cdecl Stat_AddClamped(unsigned short* stat, unsigned delta) {
    auto* const p = reinterpret_cast<unsigned char*>(stat);
    const auto d = static_cast<std::int16_t>(delta);
    const std::uint16_t v = Word(p);
    if (d > 0) {
        if (v == 999) return 0;
        const auto sum = static_cast<std::uint16_t>(v + static_cast<std::uint16_t>(d));
        SetWord(p, sum);
        if (sum <= 999) return 0;
        SetWord(p, 999);
        return static_cast<short>(999 - v);
    }
    if (d == 0) return 0;
    if (v == 0) return 0;
    const auto sum = static_cast<std::uint16_t>(v + static_cast<std::uint16_t>(d));
    SetWord(p, sum);
    if (static_cast<std::int16_t>(sum) >= 0) return 0;
    SetWord(p, 0);
    return static_cast<short>(-static_cast<int>(v));
}

// original 0x590F60 (PSX 0x801660DC): clear the mask's bits from the status
// word if any is set, and say so. Only the mask's low word reaches the word.
extern "C" unsigned char __cdecl Char_ClearStatus(unsigned id, unsigned mask, unsigned battle) {
    unsigned char* const r = CharRecord(id, battle);
    const std::uint16_t status = Word(r + at::kStatus);
    if ((status & mask & 0xFFFF) == 0) return 0;
    SetWord(r + at::kStatus, status & ~mask);
    return 1;
}

// ===========================================================================
// The message box's two commits

namespace {
// Choice ids below 0x80 through the area's own table, the rest through
// MsgBox_SystemChoice. The area descriptor is re-read for each call, as in
// the original: Game_AreaNumber zero-extended, the id's low byte.
void CommitChoice() {
    const unsigned id = At(at::kChoiceId)[0];
    if (id < 0x80) {
        const unsigned char* const desc = Area_Descriptors[Game_AreaNumber];
        const auto table = reinterpret_cast<Choice const*>(static_cast<std::uintptr_t>(move_script::Long(desc + 0x34)));
        table[id]();
    } else {
        g.system_choice();
    }
}
}  // namespace

// original 0x4981C0 (PSX 0x80151494): MsgBox_State4's entry 5. After the
// commit, window 1's state 3; with a message left (0x7DEE48 not 0xFFFF)
// window 0's state 1 and the next sub-state, else sub-state 7.
extern "C" void __cdecl MsgBox_ChoiceCommit(void) {
    CommitChoice();
    const bool more = Word(At(at::kMessage)) != 0xFFFF;
    At(at::kWindow1State)[0] = 3;
    if (more) {
        const unsigned char sub = At(at::kSubState)[0];
        At(at::kWindow0State)[0] = 1;
        At(at::kColorHigh)[0] = 0;
        At(at::kSubState)[0] = static_cast<unsigned char>(sub + 1);
        return;
    }
    At(at::kSubState)[0] = 7;
    At(at::kColorHigh)[0] = 0;
}

// original 0x4983C0 (PSX 0x80151838): MsgBox_State5's entry 3.
extern "C" void __cdecl MsgBox_MenuCommit(void) {
    CommitChoice();
    const unsigned char sub = At(at::kSubState)[0];
    At(at::kColorHigh)[0] = 0;
    At(at::kWindow1State)[0] = 3;
    At(at::kSubState)[0] = static_cast<unsigned char>(sub + 1);
}

// ===========================================================================
// The sixteen system choices (MsgBox_SystemChoice's table, ids 0x80..0x8F)

namespace {
// The message word from the id's pair at 0x658F08 + 4 * k, by the cursor
// read as s8 (the original's movsx): a cursor outside 0..1 reads the next
// pair or the one before, as the original does. Returns the cursor.
int SysMessage(unsigned k) {
    const int cursor = static_cast<signed char>(At(at::kCursor)[0]);
    SetWord(At(at::kMessage), Word(At(at::kSysMessages + 4 * k + 2 * cursor)));
    return cursor;
}
void SysCounter(unsigned k, unsigned char if0, unsigned char if1) {
    const int cursor = SysMessage(k);
    if (cursor == 0) At(at::kSysCounter)[0] = if0;
    else if (cursor == 1) At(at::kSysCounter)[0] = if1;
}
}  // namespace

// original 0x498AD0: cursor 0 sets the byte 0x929F0B, cursor 1 clears it.
extern "C" void __cdecl MsgBox_SysChoice80(void) {
    const int cursor = SysMessage(0);
    if (cursor == 0) At(at::kSysFlag)[0] = 1;
    else if (cursor == 1) At(at::kSysFlag)[0] = 0;
}
// original 0x498B00, 0x498B30, 0x498B60, 0x498B90: MoveScript variable 3
// (0x903848) by the cursor.
extern "C" void __cdecl MsgBox_SysChoice81(void) { SysCounter(1, 0x1E, 0x28); }
extern "C" void __cdecl MsgBox_SysChoice82(void) { SysCounter(2, 0x1E, 0x32); }
extern "C" void __cdecl MsgBox_SysChoice83(void) { SysCounter(3, 0x3C, 0x46); }
extern "C" void __cdecl MsgBox_SysChoice84(void) { SysCounter(4, 0x1E, 0x32); }
// original 0x498BC0..0x498D00, 0x20 apart: the message word only.
extern "C" void __cdecl MsgBox_SysChoice85(void) { SysMessage(5); }
extern "C" void __cdecl MsgBox_SysChoice86(void) { SysMessage(6); }
extern "C" void __cdecl MsgBox_SysChoice87(void) { SysMessage(7); }
extern "C" void __cdecl MsgBox_SysChoice88(void) { SysMessage(8); }
extern "C" void __cdecl MsgBox_SysChoice89(void) { SysMessage(9); }
extern "C" void __cdecl MsgBox_SysChoice8A(void) { SysMessage(10); }
extern "C" void __cdecl MsgBox_SysChoice8B(void) { SysMessage(11); }
extern "C" void __cdecl MsgBox_SysChoice8C(void) { SysMessage(12); }
extern "C" void __cdecl MsgBox_SysChoice8D(void) { SysMessage(13); }
extern "C" void __cdecl MsgBox_SysChoice8E(void) { SysMessage(14); }
extern "C" void __cdecl MsgBox_SysChoice8F(void) { SysMessage(15); }

// ===========================================================================
// The pad auto-repeat

// original 0x461EB0 (PSX 0x8014E534): a new press among the bits asked for
// starts a 12-frame wait and latches Input_Held; while a latched bit stays
// held the wait counts down and re-fires every 3 frames. The answer is the
// dword at Input_Held - Input_Previous in the upper half, as the original
// loads it - with the low word 0 when nothing fires.
extern "C" unsigned __cdecl Input_AutoRepeat(unsigned pressed) {
    const std::uint32_t held = static_cast<std::uint32_t>(move_script::Long(reinterpret_cast<unsigned char*>(&Input_Held)));
    if (static_cast<std::uint16_t>(pressed) != 0) {
        SetWord(At(at::kRepeatTimer), 0xC);
        SetWord(At(at::kRepeatLatch), held);
        return held;
    }
    if ((Word(At(at::kRepeatLatch)) & held & 0xFFFF) != 0) {
        const auto timer = static_cast<std::uint16_t>(Word(At(at::kRepeatTimer)) - 1);
        SetWord(At(at::kRepeatTimer), timer);
        if (timer == 0) {
            SetWord(At(at::kRepeatTimer), 3);
            SetWord(At(at::kRepeatLatch), held);
            return held;
        }
    }
    return held & 0xFFFF0000u;
}

// ===========================================================================

void ItemUse_Inject() {
    if (bof3::WantsShadow("item_use")) item_use::SelfTest();
    BOF3_INJECT(ItemUse_Dispatch);
    BOF3_INJECT(ItemUse_None);
    BOF3_INJECT(ItemUse_HealHp20);
    BOF3_INJECT(ItemUse_HealHp40);
    BOF3_INJECT(ItemUse_HealHp100);
    BOF3_INJECT(ItemUse_HealHpFull);
    BOF3_INJECT(ItemUse_PartyHealHp100);
    BOF3_INJECT(ItemUse_HealAp20);
    BOF3_INJECT(ItemUse_HealAp100);
    BOF3_INJECT(ItemUse_HealHp5Cure);
    BOF3_INJECT(ItemUse_Cure80);
    BOF3_INJECT(ItemUse_Cure08);
    BOF3_INJECT(ItemUse_Cure100);
    BOF3_INJECT(ItemUse_CureA0);
    BOF3_INJECT(ItemUse_Revive);
    BOF3_INJECT(ItemUse_PartyRestore);
    BOF3_INJECT(ItemUse_MaxHpUp);
    BOF3_INJECT(ItemUse_MaxApUp);
    BOF3_INJECT(ItemUse_AtkUp);
    BOF3_INJECT(ItemUse_DefUp);
    BOF3_INJECT(ItemUse_AgiUp);
    BOF3_INJECT(ItemUse_IntUp);
    BOF3_INJECT(ItemUse_Stat2EUp);
    BOF3_INJECT(ItemUse_HpScaleUp);
    BOF3_INJECT(ItemUse_HealHp1);
    BOF3_INJECT(ItemUse_HealHp80);
    BOF3_INJECT(ItemUse_PartyHealHp80);
    BOF3_INJECT(ItemUse_PartyHealHp240);
    BOF3_INJECT(ItemUse_PartyCure80);
    BOF3_INJECT(ItemUse_HealAp40);
    BOF3_INJECT(ItemUse_HealAp5);
    BOF3_INJECT(ItemUse_HealHp5);
    BOF3_INJECT(ItemUse_FaerieTiara);
    BOF3_INJECT(ItemUse_WaterJug);
    BOF3_INJECT(Char_HealHp);
    BOF3_INJECT(Char_HealAp);
    BOF3_INJECT(Stat_AddCap999);
    BOF3_INJECT(Stat_AddCap99);
    BOF3_INJECT(Stat_AddClamped);
    BOF3_INJECT(Char_ClearStatus);
    BOF3_INJECT(MsgBox_ChoiceCommit);
    BOF3_INJECT(MsgBox_MenuCommit);
    BOF3_INJECT(MsgBox_SysChoice80);
    BOF3_INJECT(MsgBox_SysChoice81);
    BOF3_INJECT(MsgBox_SysChoice82);
    BOF3_INJECT(MsgBox_SysChoice83);
    BOF3_INJECT(MsgBox_SysChoice84);
    BOF3_INJECT(MsgBox_SysChoice85);
    BOF3_INJECT(MsgBox_SysChoice86);
    BOF3_INJECT(MsgBox_SysChoice87);
    BOF3_INJECT(MsgBox_SysChoice88);
    BOF3_INJECT(MsgBox_SysChoice89);
    BOF3_INJECT(MsgBox_SysChoice8A);
    BOF3_INJECT(MsgBox_SysChoice8B);
    BOF3_INJECT(MsgBox_SysChoice8C);
    BOF3_INJECT(MsgBox_SysChoice8D);
    BOF3_INJECT(MsgBox_SysChoice8E);
    BOF3_INJECT(MsgBox_SysChoice8F);
    BOF3_INJECT(Input_AutoRepeat);
}
