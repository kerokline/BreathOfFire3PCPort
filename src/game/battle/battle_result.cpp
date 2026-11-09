// Moved here by tools/move_funcs.py from battle_e1.cpp, battle_result.cpp (docs/source-layout-round.md).
#include "game/battle/battle_result.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/battle_e1_callees.h"
#include "game/battle_result.h"
#include "game/battle_result_callees.h"
#include "game/boss_harness.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_e1::at;

}  // namespace

using namespace battle_result;

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& BBattleE1(U address) { return move_script::At(address)[0]; }

unsigned char* Member(U index) { return move_script::At(::bof3::Img(ObjTrio_addr) + index * battle_e1::at::kMemberSize); }

unsigned Pressed() { return Input_Pressed; }

}  // namespace

namespace battle_result {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_result

namespace {

unsigned char& Byte(std::uint32_t address) { return battle_result::At(address)[0]; }

std::uint32_t Dword(std::uint32_t address) { return static_cast<std::uint32_t>(Long(At(address))); }

void SetDword(std::uint32_t address, std::uint32_t v) { SetLong(At(address), static_cast<std::int32_t>(v)); }

void Advance(std::uint32_t address) { Byte(address) = static_cast<unsigned char>(Byte(address) + 1); }

unsigned char* MemberBattleResult(unsigned slot) { return battle_result::At(::bof3::Img(battle_result::at::kMembers.v) + (slot & 0xFF) * battle_result::at::kMemberSize); }

unsigned char* WindowSlot(unsigned slot) { return At(::bof3::Img(WindowRecords_addr) + slot * battle_result::at::kWindowSize); }

// MemberBattleResult() is +0x80 into the member record (0x802DC0); the record, and its
// typed view, starts 0x80 before.
char* TextBattleResult(std::uint32_t address) { return reinterpret_cast<char*>(battle_result::At(address)); }

const char* Format(std::uint32_t address) { return reinterpret_cast<const char*>(battle_result::At(address)); }

// The tick step of an amount: 1 below 30, else amount / 30 - kept as a word,
// as the original stores it (`mov word [0x904B70], dx`).
void SetTickStep(std::uint32_t amount) { BattleResult_TickStep = amount < 30 ? 1u : amount / 30; }

// The .data step tables, by the byte 0x904AA4, called through the dword the
// table holds when the step runs. As the original's `jmp [eax * 4 + table]`
// has it, the index is not checked against the table's length: 0..255 all
// read inside .data, and a slot past the end is the next table's entry.
void RunStep(std::uint32_t table) {
    const unsigned index = BattleE4_Step4;
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Dword(table + index * 4)))();
}

}  // namespace

// original 0x431940 (PSX 0x801EECD4, paired): the EXP total 0x904AEC split
// among the party - rounded up, (total + n - 1) / n with n the count BattleResult_CountExpShares 0x4319B0
// answers, the sum wrapping at 32 bits; 0 when the total or the count is 0
// (the count is not asked for a total of 0). The share goes back into
// 0x904AEC and, through sprintf("%d"), into text record 0; the battle
// message 0x93B8E4 = Msg_SystemPtr(5); PartySet_Select(set 0x90412C & 0x7F,
// mode 0); the step 0x904AA4 one on.
//
// As the original has it: the share is stored before the sprintf; the party
// set byte is read after Msg_SystemPtr (the original loads it there), and
// the step's increment is a read-modify-write after PartySet_Select.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_SplitExp(void) {
    std::uint32_t share = 0;
    if (Battle_ExpTotal != 0) {
        const unsigned n = g.count_members();
        if (n != 0) share = (Battle_ExpTotal + n - 1) / n;
    }
    Battle_ExpTotal = share;
    g.crt_sprintf(TextBattleResult(::bof3::Img(battle_result::at::kText0.v)), Format(::bof3::Img(battle_result::at::kFormatD.v)), share);
    const unsigned char* const msg = g.msg_system(5);
    const unsigned set = AreaW0b_PartySet & 0x7Fu;
    SetDword(::bof3::Img(battle_result::at::kMsgCurrent.v), Low32(msg));
    g.party_set_select(set, 0);
    Advance(::bof3::Img(BattleE4_Step4_addr));
}

// original 0x4319B0: of the 0x904AB0 party slots (re-read each pass), those
// Battle_ActorIsOut answers 0 for whose +0x134 lacks 0x400: the count, al.
// BattleResult_SplitExp's divisor.
extern "C" unsigned char __cdecl BattleResult_CountExpShares(void) {
    unsigned char count = 0;
    if (BBattleE1(::bof3::Img(battle_e1::at::kPartyCount.v)) == 0) return 0;
    unsigned char i = 0;
    do {
        if (!BH_CALL(Battle_ActorIsOut)(i) && !(static_cast<std::int32_t>(rv::As<records::PartyMember>(Member(i))->flags_134) & 0x400)) ++count;
        ++i;
    } while (i < BBattleE1(::bof3::Img(battle_e1::at::kPartyCount.v)));
    return count;
}

// original 0x431A20 (PSX 0x801EEE50, paired): Window_Alloc(1, 4) - the
// result window, kind 4 (0x597F60) - then its state +2 = 4 (the EXP state,
// BattleResultWin_ExpState), its step +3 = 0 and its height +9 = party
// count * 16 + 4 (a byte); the tick step 0x904B70 from the share; the step
// 0x904AA4 one on.
//
// As the original has it: the count and the share are read after
// Window_Alloc.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_OpenExpWindow(void) {
    g.window_alloc(1, 4);
    const auto height = static_cast<unsigned char>((Byte(::bof3::Img(battle_result::at::kPartyCount.v)) << 4) + 4);
    const std::uint32_t exp = Battle_ExpTotal;
    unsigned char* const w = WindowSlot(1);
    auto* const window_rec = rv::As<records::WindowRecord>(w);
    window_rec->kind = 4;
    window_rec->state = 0;
    w[9] = height;
    SetTickStep(exp);
    Advance(::bof3::Img(BattleE4_Step4_addr));
}

// original 0x431A90 (PSX 0x801EEEE4, paired): the step 0x904AA4 one on while
// any button is held (Input_Held 0x7E1BE8, the word).
extern "C" void __cdecl BattleResult_ExpWaitHeld(void) {
    if (Input_Held != 0) Advance(::bof3::Img(BattleE4_Step4_addr));
}

// original 0x431AB0 (PSX 0x801EEF18 paired; the sibling's
// BattleResult_ExpTick 0x801EEF58): with a button held and File_LoadDone,
// the whole remaining share at once; otherwise, while the share is more than
// the tick step (signed: share - step > 0), BattleResult_AddExp(step) and
// the share less the step; else the rest. After the whole or the rest, the
// share 0 and the step 0x904AA4 one on.
//
// As the original has it: File_LoadDone is asked only with a button held;
// the step is the dword 0x904B70's low word; share and step are read afresh
// after the call for the subtraction, and the step byte after the last call.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_ExpTick(void) {
    std::uint32_t amount;
    if (Input_Held != 0 && g.file_load_done() != 0) {
        amount = Battle_ExpTotal;
    } else {
        const std::uint32_t step = Dword(::bof3::Img(battle_result::at::kTickStep.v)) & 0xFFFF;
        const std::uint32_t exp = Battle_ExpTotal;
        if (static_cast<std::int32_t>(exp - step) > 0) {
            g.add_exp(step);
            Battle_ExpTotal = Battle_ExpTotal - (Dword(::bof3::Img(battle_result::at::kTickStep.v)) & 0xFFFF);
            return;
        }
        amount = exp;
    }
    g.add_exp(amount);
    const auto next = static_cast<unsigned char>(BattleE4_Step4 + 1);
    Battle_ExpTotal = 0;
    BattleE4_Step4 = next;
}

// original 0x431B30 (PSX 0x801EEFD0, paired): once File_LoadDone, the result
// phase 0x904AA3 one on - to the level-up search - with 0x904AA5 and the
// step 0x904AA4 0.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_ExpDone(void) {
    if (g.file_load_done() == 0) return;
    const auto next = static_cast<unsigned char>(BattleE4_Step3 + 1);
    BossSb_Countdown = 0;
    BattleE4_Step3 = next;
    BattleE4_Step4 = 0;
}

// original 0x431B60 (PSX 0x801EF01C, paired): entry 1 of the result phase's
// table 0x64AFA0 - `jmp [0x64AFC0 + (0x904AA4 & 0xFF) * 4]`: 0x431B80, 0x431C10 (BattleResult_LevelUpNotice).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_LevelUpStep(void) { RunStep(::bof3::Img(BattleResult_LevelUpSteps_addr)); }

// original 0x431B80 (PSX 0x801EF058, paired): from party slot 0x904AA5 on,
// while it is below the count 0x904AB0: the roster index of the slot's
// character (0x4469D0 of byte +9) into 0x904AA7, the slot one on, and when
// Char_LevelUpGain 0x432170(index, 0) answers a non-zero word, 0x904AE8 |= 0x20 and the step
// 0x904AA4 one on (to 0x431C10, the announcement) - that is all for the
// frame. With no slot left, the result phase 0x904AA3 one on with the step
// 0x904AA4 0.
//
// As the original has it: the slot byte is read again after 0x4469D0 for
// its increment, and slot and count afresh after Char_LevelUpGain; the index passed
// on is 0x4469D0's whole answer (a byte, zero-extended by the callee).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_FindLevelUp(void) {
    while (BossSb_Countdown < Byte(::bof3::Img(battle_result::at::kPartyCount.v))) {
        const unsigned index = g.roster_index(rv::As<records::PartyMember>(MemberBattleResult(BossSb_Countdown) - 0x80)->char_id);
        const auto next = static_cast<unsigned char>(BossSb_Countdown + 1);
        BattleE1_Roster = static_cast<unsigned char>(index);
        BossSb_Countdown = next;
        if ((g.level_up_pending(index, 0) & 0xFFFF) != 0) {
            Battle_EndFlags = static_cast<unsigned char>(Battle_EndFlags | 0x20);
            Advance(::bof3::Img(BattleE4_Step4_addr));
            return;
        }
    }
    BattleE4_Step4 = 0;
    Advance(::bof3::Img(BattleE4_Step3_addr));
}

// original 0x431C10 (BattleResult_LevelUpSteps[1]): nothing until a button
// is pressed; then BE4's 0x44A910(0x904AA5 - 1) (the name), the old level
// (record 0x904AA7's +0xA) to 0x904B72, the new one (Char_LevelUpGain(roster,
// 0) + the level, the roster re-read after) to 0x904B74 and through
// Crt_sprintf into Text_Records + 0x20; 0x93B8E4 = message 7; the window's
// rows: 0x10 for each stat 1..6 Char_LevelUpGain answers non-zero for, and
// for each of the bytes +6 / +7 of the old level's Char_ExpTable row;
// Window_Alloc(1, 4); window 1's +0xA = the roster, +2 = +3 = 0, +9 = the
// rows + 4, +0xB = the old level; 0x904AA4 - 1 (back to the search).
extern "C" void __cdecl BattleResult_LevelUpNotice(void) {
    if (Pressed() == 0) return;
    BH_AT(void (__cdecl*)(unsigned), battle_e1::at::kBe4MemberName.v)(static_cast<unsigned char>(BossSb_Countdown - 1));
    const unsigned char roster = BattleE1_Roster;
    SetWord(At(::bof3::Img(battle_e1::at::kOldLevel.v)), BBattleE1(::bof3::Img(battle_e1::at::kCharRecords.v) + roster * battle_e1::at::kCharSize + 0xA));
    const unsigned gain = BH_CALL(Char_LevelUpGain)(roster, 0);
    const unsigned char again = BattleE1_Roster;
    SetWord(At(::bof3::Img(battle_e1::at::kNewLevel.v)), (gain & 0xFFFF) + BBattleE1(::bof3::Img(battle_e1::at::kCharRecords.v) + again * battle_e1::at::kCharSize + 0xA));
    BH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(bof3::addr::Text_Records) + 0x20)),
                         reinterpret_cast<const char*>(At(::bof3::Img(bof3::addr::Area08_MessageFormat))), Word(At(::bof3::Img(battle_e1::at::kNewLevel.v))));
    SetLong(At(::bof3::Img(battle_e1::at::kLevelMsgPtr.v)), static_cast<std::int32_t>(Low32(BH_CALL(Msg_SystemPtr)(7))));
    U rows = 0;
    for (unsigned k = 1; k <= 6; ++k)
        if ((BH_CALL(Char_LevelUpGain)(BattleE1_Roster, k) & 0xFFFF) != 0) rows += 0x10;
    const U row = ::bof3::Img(battle_e1::at::kCharExpTable.v) + (BattleE1_Roster * 99u + Word(At(::bof3::Img(battle_e1::at::kOldLevel.v)))) * 8;
    if (BBattleE1(row + 6) != 0) rows += 0x10;
    if (BBattleE1(row + 7) != 0) rows += 0x10;
    BH_CALL(Window_Alloc)(1, 4);
    const unsigned char who = BattleE1_Roster;
    const unsigned char old = BBattleE1(::bof3::Img(battle_e1::at::kOldLevel.v));
    BBattleE1(::bof3::Img(battle_e1::at::kWin1.v) + 0xA) = who;
    const unsigned char step = BattleE4_Step4;
    BBattleE1(::bof3::Img(battle_e1::at::kWin1.v) + 2) = 0;
    BBattleE1(::bof3::Img(battle_e1::at::kWin1.v) + 3) = 0;
    BBattleE1(::bof3::Img(battle_e1::at::kWin1.v) + 9) = static_cast<unsigned char>(rows + 4);
    BBattleE1(::bof3::Img(battle_e1::at::kWin1.v) + 0xB) = old;
    BattleE4_Step4 = static_cast<unsigned char>(step - 1);
}

// original 0x431D50 (PSX 0x801EF310, paired): entry 2 of 0x64AFA0 -
// `jmp [0x64AFC8 + (0x904AA4 & 0xFF) * 4]`: 0x431D70, 0x432050, 0x432070,
// 0x4320F0.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_RewardStep(void) { RunStep(::bof3::Img(BattleResult_RewardSteps_addr)); }

// original 0x431D70 (PSX 0x801EF34C paired; the sibling's BattleResult_Setup
// 0x801EF390): nothing until a button is held (Input_Held) - and, when
// 0x904AE8 has 0x20 (a level-up was shown), until one is newly pressed
// (Input_Pressed) too. Then:
//   1. each party slot whose roster index Char_LevelUpGain 0x432170 answers non-zero for
//      levels up: 0x498DE0(index);
//   2. the zenny total 0x904AF0 half as much again (total += total >> 1)
//      when BattleResult_ZennyBonus 0x431FE0 says so;
//   3. the result window freed (slot 1's byte +0 = 0) and taken again,
//      Window_Alloc(1, 4), in the zenny state 5, step 0; the total through
//      sprintf("%d") into text record 0; 0x93B8E4 = Msg_SystemPtr(6); the
//      tick step from the total;
//   4. with drops (0x904AE7): the drop window, Window_Alloc(0x15, 4), in
//      state 1 at (0x14, 0x46), height ((n - 1) / 2) * 13 + 0x12 as a byte;
//      with two or more, the drop list sorted by item word, ascending,
//      the counts moved with their items (an exchange sort: entry i against
//      every later j, swapped when greater, equal ones kept in order);
//   5. Snd_LoadBankFile(word 0x904EFC + 3); the step 0x904AA4 one on.
//
// As the original has it: slot and count are re-read after the calls;
// the character byte again for 0x4469D0's second call; the slot's byte
// +0 is cleared before Window_Alloc, its state and step set after; the
// total re-read after Window_Alloc and after Msg_SystemPtr; the drop count
// re-read after the drop window's Window_Alloc and after every swap - the
// swaps themselves xor in place in the original's order, so a count past 16
// (which reaches into the counts with the items, and past them) is sorted
// exactly as the original does it.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_Setup(void) {
    if ((Battle_EndFlags & 0x20) != 0 && Input_Pressed == 0) return;
    if (Input_Held == 0) return;

    for (unsigned char slot = 0; slot < Byte(::bof3::Img(battle_result::at::kPartyCount.v)); ++slot) {
        const unsigned index = g.roster_index(rv::As<records::PartyMember>(MemberBattleResult(slot) - 0x80)->char_id);
        if ((g.level_up_pending(index, 0) & 0xFFFF) != 0) g.level_up(g.roster_index(rv::As<records::PartyMember>(MemberBattleResult(slot) - 0x80)->char_id));
    }
    if (g.zenny_bonus() != 0) BattleFlow_ZennyTotal = BattleFlow_ZennyTotal + (BattleFlow_ZennyTotal >> 1);

    rv::As<records::WindowRecord>(WindowSlot(1))->in_use = 0;
    g.window_alloc(1, 4);
    const std::uint32_t zenny = BattleFlow_ZennyTotal;
    rv::As<records::WindowRecord>(WindowSlot(1))->kind = 5;
    rv::As<records::WindowRecord>(WindowSlot(1))->state = 0;
    g.crt_sprintf(TextBattleResult(::bof3::Img(battle_result::at::kText0.v)), Format(::bof3::Img(battle_result::at::kFormatD.v)), zenny);
    const unsigned char* const msg = g.msg_system(6);
    const std::uint32_t total = BattleFlow_ZennyTotal;
    SetDword(::bof3::Img(battle_result::at::kMsgCurrent.v), Low32(msg));
    SetTickStep(total);

    if (Battle_DropCount != 0) {
        g.window_alloc(0x15, 4);
        unsigned char n = Battle_DropCount;
        unsigned char* const w = WindowSlot(0x15);
        auto* const window_rec = rv::As<records::WindowRecord>(w);
        const int last = static_cast<int>(n) - 1;
        window_rec->kind = 1;
        window_rec->x = 0x14;
        window_rec->y = 0x46;
        w[9] = static_cast<unsigned char>((last / 2) * 13 + 0x12);
        if (n > 1) {
            unsigned char i = 0;
            do {
                const auto next = static_cast<unsigned char>(i + 1);
                for (unsigned char j = next; j < n; ++j) {
                    unsigned char* const a = At(::bof3::Img(battle_result::at::kDropItems.v) + i * 2u);
                    unsigned char* const b = At(::bof3::Img(battle_result::at::kDropItems.v) + j * 2u);
                    const unsigned ai = Word(a);
                    if (ai <= Word(b)) continue;
                    // the original's xor swap, store for store
                    SetWord(a, ai ^ Word(b));
                    SetWord(b, Word(b) ^ Word(a));
                    SetWord(a, Word(a) ^ Word(b));
                    unsigned char* const ci = At(::bof3::Img(battle_result::at::kDropCounts.v) + i);
                    unsigned char* const cj = At(::bof3::Img(battle_result::at::kDropCounts.v) + j);
                    ci[0] = static_cast<unsigned char>(ci[0] ^ cj[0]);
                    cj[0] = static_cast<unsigned char>(cj[0] ^ ci[0]);
                    ci[0] = static_cast<unsigned char>(ci[0] ^ cj[0]);
                    n = Battle_DropCount;
                }
                i = next;
            } while (static_cast<int>(i) < static_cast<int>(n) - 1);
        }
    }
    g.snd_load_bank(Game_AreaNumber + 3u);
    Advance(::bof3::Img(BattleE4_Step4_addr));
}

// original 0x431FE0: 1 when a party slot (of 0x904AB0, re-read) that
// Battle_ActorIsOut answers 0 for holds 7 in its +0x96 or +0x97; else 0. al.
extern "C" unsigned char __cdecl BattleResult_ZennyBonus(void) {
    if (BBattleE1(::bof3::Img(battle_e1::at::kPartyCount.v)) == 0) return 0;
    unsigned char i = 0;
    do {
        if (!BH_CALL(Battle_ActorIsOut)(i)) {
            auto* const member_rec = rv::As<records::PartyMember>(Member(i));
            if (member_rec->accessory[0] == 7 || member_rec->accessory[1] == 7) return 1;
        }
        ++i;
    } while (i < BBattleE1(::bof3::Img(battle_e1::at::kPartyCount.v)));
    return 0;
}

// original 0x432050 (PSX 0x801EF780, paired): with a button held, 0x904AA5
// 0 and the step 0x904AA4 one on.
extern "C" void __cdecl BattleResult_ZennyWaitHeld(void) {
    if (Input_Held == 0) return;
    const auto next = static_cast<unsigned char>(BattleE4_Step4 + 1);
    BossSb_Countdown = 0;
    BattleE4_Step4 = next;
}

// original 0x432070 (PSX 0x801EF7B8 paired; the sibling's
// BattleResult_ZennyTick 0x801EF810): BattleResult_ExpTick's shape for the
// zenny total 0x904AF0, through Zenny_Add(amount, 0) - which also counts it
// into 0x904138.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_ZennyTick(void) {
    std::uint32_t amount;
    if (Input_Held != 0 && g.file_load_done() != 0) {
        amount = BattleFlow_ZennyTotal;
    } else {
        const std::uint32_t step = Dword(::bof3::Img(battle_result::at::kTickStep.v)) & 0xFFFF;
        const std::uint32_t zenny = BattleFlow_ZennyTotal;
        if (static_cast<std::int32_t>(zenny - step) > 0) {
            g.add_zenny(step, 0);
            BattleFlow_ZennyTotal = BattleFlow_ZennyTotal - (Dword(::bof3::Img(battle_result::at::kTickStep.v)) & 0xFFFF);
            return;
        }
        amount = zenny;
    }
    g.add_zenny(amount, 0);
    const auto next = static_cast<unsigned char>(BattleE4_Step4 + 1);
    BattleFlow_ZennyTotal = 0;
    BattleE4_Step4 = next;
}

// original 0x4320F0 (PSX 0x801EF874; the sibling's BattleResult_AwardDrops):
// once File_LoadDone, each of the 0x904AE7 drops into the inventory -
// Inventory_Add(category = item word >> 8, item = its low byte, its count,
// 0) - and then the battle's mode bytes 0x904AA1..0x904AA4 = 4, 0, 0, 0:
// the result is over.
//
// As the original has it: the count is re-read after every Inventory_Add;
// the fourth argument, 0, is pushed (the PSX's), and ours reads three.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_AwardDrops(void) {
    if (g.file_load_done() == 0) return;
    for (unsigned char i = 0; i < Battle_DropCount; ++i) {
        const unsigned count = At(::bof3::Img(battle_result::at::kDropCounts.v) + i)[0];
        const unsigned item = At(::bof3::Img(battle_result::at::kDropItems.v) + i * 2u)[0];
        const unsigned category = At(::bof3::Img(battle_result::at::kDropItems.v) + i * 2u)[1];
        g.inventory_add(category, item, count, 0);
    }
    BattleActions_Step = 4;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
}

// original 0x432170 (PSX 0x801EF92C by the catalogue's pair; the sibling's
// name there is an unrelated overlay's): the level character record
// `roster`'s EXP (+0xC, signed) reaches - the first L of 1..98 whose
// Char_ExpTable words (the roster's 99 rows of 8) summed over 1..L exceed
// it, else 99 - and by `what`: 0 the levels gained over its level +0xA (0 if
// none); 1..6 the gain of one stat over those levels (the rows from the
// level on: 1 byte +2, 2 byte +3, 3 byte +4's high nibble, 4 its low, 5 byte
// +5's high, 6 its low; each plus the record's s8 +0x89..+0x8E, summed in 16
// bits, 0 if negative, 0 with no level gained); anything else 0. ax out.
// Neither byte is checked.
extern "C" unsigned short __cdecl Char_LevelUpGain(unsigned roster_arg, unsigned what_arg) {
    const U roster = roster_arg & 0xFF;
    const U base = roster * 99;
    const unsigned char* const record = At(::bof3::Img(CharacterRecords_addr) + roster * battle_e1::at::kCharSize);
    static_assert(battle_e1::at::kCharSize == sizeof(records::CharRecord));
    const unsigned char level = record[0xA];
    unsigned char reached = 1;
    U sum = 0;
    for (;;) {
        sum += Word(At(::bof3::Img(battle_e1::at::kCharExpTable.v) + (base + reached) * 8));
        if (static_cast<std::int32_t>(sum) > rv::As<records::CharRecord>(record)->exp) break;
        ++reached;
        if (reached >= 0x63) break;
    }
    const U what = what_arg & 0xFF;
    if (what > 6) return 0;
    if (what == 0) {
        const std::int32_t gained = static_cast<std::int32_t>(reached) - static_cast<std::int32_t>(level);
        return static_cast<unsigned short>(gained > 0 ? gained : 0);
    }
    if (level >= reached) return 0;
    struct Growth { unsigned char stat, byte; int nibble; };   // nibble: 0 the byte, 1 high, 2 low
    static const Growth kGrowth[7] = {{0, 0, 0}, {0x89, 2, 0}, {0x8A, 3, 0}, {0x8B, 4, 1}, {0x8C, 4, 2}, {0x8D, 5, 1}, {0x8E, 5, 2}};
    const Growth& g = kGrowth[what];
    const auto bonus = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<signed char>(record[g.stat])));
    std::uint16_t total = 0;
    for (U lv = level; lv < reached; ++lv) {
        const unsigned char b = BBattleE1(::bof3::Img(battle_e1::at::kCharExpTable.v) + (base + lv) * 8 + g.byte);
        const unsigned v = g.nibble == 1 ? b >> 4 : g.nibble == 2 ? b & 0xF : b;
        total = static_cast<std::uint16_t>(total + v + bonus);
    }
    return static_cast<std::int16_t>(total) < 0 ? 0 : total;
}

void BattleBattleResult_Inject() {
    BOF3_INJECT(BattleResult_SplitExp);
    BOF3_INJECT(BattleResult_CountExpShares);
    BOF3_INJECT(BattleResult_OpenExpWindow);
    BOF3_INJECT(BattleResult_ExpWaitHeld);
    BOF3_INJECT(BattleResult_ExpTick);
    BOF3_INJECT(BattleResult_ExpDone);
    BOF3_INJECT(BattleResult_LevelUpStep);
    BOF3_INJECT(BattleResult_FindLevelUp);
    BOF3_INJECT(BattleResult_LevelUpNotice);
    BOF3_INJECT(BattleResult_RewardStep);
    BOF3_INJECT(BattleResult_Setup);
    BOF3_INJECT(BattleResult_ZennyBonus);
    BOF3_INJECT(BattleResult_ZennyWaitHeld);
    BOF3_INJECT(BattleResult_ZennyTick);
    BOF3_INJECT(BattleResult_AwardDrops);
    BOF3_INJECT(Char_LevelUpGain);
}
