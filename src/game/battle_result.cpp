// The battle result: the PSX BATL_END.EMI's code compiled into the exe
// (docs/battle_result.md). Three .data tables of steps by the byte 0x904AA4,
// under the result phase's own table 0x64AFA0 (by 0x904AA3, dispatched by
// group CC's 0x431910):
//   - 0x64AFAC (entry 0 of 0x64AFA0, through CC's 0x431920): the EXP - split
//     it, open its window, wait for a button, tick it into the party, wait
//     again (0x431940, 0x431A20, 0x431A90, 0x431AB0, 0x431B30);
//   - 0x64AFC0 (entry 1, through BattleResult_LevelUpStep 0x431B60): the
//     level-up search (0x431B80) and its announcement (0x431C10, Capcom's
//     still - no route reaches it);
//   - 0x64AFC8 (entry 2, through BattleResult_RewardStep 0x431D50): the
//     level-ups applied, the zenny and the drops' windows (0x431D70), wait
//     (0x432050), tick the zenny in (0x432070), the drops into the
//     inventory (0x4320F0).
// And under the window-kind handler 0x597F60 (group CM's), the result
// window's two states and their draws (0x598570..0x598749).
//
// Every call goes through battle_result::g (battle_result_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Everything here is a faithful replacement. The
// EXP and zenny multipliers (DIV-0045) are applied before any of this, when
// Battle_EnemyDefeated (battle_flow.cpp) adds an enemy's yield to the
// totals 0x904AEC / 0x904AF0 that these functions read.
#include "game/battle_result.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_result_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_result {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    {Fn<Handler>(0x5986F0), Fn<Handler>(0x5985A0)},
    {Fn<Handler>(0x5986F0), Fn<Handler>(0x598700)},
    Fn<unsigned char (__cdecl*)()>(kCountMembers),
    Fn<unsigned char (__cdecl*)()>(kZennyBonus),
    Fn<void (__cdecl*)(unsigned)>(kAddExp),
    Fn<unsigned char (__cdecl*)(unsigned)>(kRosterIndex),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(kLevelUpPending),
    Fn<void (__cdecl*)(unsigned)>(kLevelUp),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(kAddZenny),
    Fn<void (__cdecl*)(int, int, int, int)>(kDrawFrame),
    Fn<unsigned (__cdecl*)(unsigned)>(kExpToNext),
    Crt_sprintf, Msg_SystemPtr, PartySet_Select, Window_Alloc, File_LoadDone, Snd_LoadBankFile,
    Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(0x590BB0),   // Inventory_Add, ours
    Text_DrawAt, Text_DrawFont12, BattleWin_DrawMediumBox,
};
Callees g = kOriginals;

}  // namespace battle_result

using namespace battle_result;

namespace {

unsigned char& Byte(std::uint32_t address) { return At(address)[0]; }
std::uint32_t Dword(std::uint32_t address) { return static_cast<std::uint32_t>(Long(At(address))); }
void SetDword(std::uint32_t address, std::uint32_t v) { SetLong(At(address), static_cast<std::int32_t>(v)); }
void Advance(std::uint32_t address) { Byte(address) = static_cast<unsigned char>(Byte(address) + 1); }
unsigned char* Member(unsigned slot) { return At(at::kMembers + (slot & 0xFF) * at::kMemberSize); }
unsigned char* WindowSlot(unsigned slot) { return At(at::kWindows + slot * at::kWindowSize); }
// The record the window task is running, re-read wherever the original reads it.
unsigned char* CurrentWindow() { return At(Dword(at::kWindowCurrent)); }
char* Text(std::uint32_t address) { return reinterpret_cast<char*>(At(address)); }
const char* Format(std::uint32_t address) { return reinterpret_cast<const char*>(At(address)); }

// The tick step of an amount: 1 below 30, else amount / 30 - kept as a word,
// as the original stores it (`mov word [0x904B70], dx`).
void SetTickStep(std::uint32_t amount) { SetWord(At(at::kTickStep), amount < 30 ? 1u : amount / 30); }

// The .data step tables, by the byte 0x904AA4, called through the dword the
// table holds when the step runs. As the original's `jmp [eax * 4 + table]`
// has it, the index is not checked against the table's length: 0..255 all
// read inside .data, and a slot past the end is the next table's entry.
void RunStep(std::uint32_t table) {
    const unsigned index = Byte(at::kStep);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Dword(table + index * 4)))();
}

}  // namespace

// ===========================================================================
// The EXP (0x64AFAC)
// ===========================================================================

// original 0x431940 (PSX 0x801EECD4, paired): the EXP total 0x904AEC split
// among the party - rounded up, (total + n - 1) / n with n the count 0x4319B0
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
    if (Dword(at::kExpTotal) != 0) {
        const unsigned n = g.count_members();
        if (n != 0) share = (Dword(at::kExpTotal) + n - 1) / n;
    }
    SetDword(at::kExpTotal, share);
    g.crt_sprintf(Text(at::kText0), Format(at::kFormatD), share);
    const unsigned char* const msg = g.msg_system(5);
    const unsigned set = Byte(at::kPartySet) & 0x7Fu;
    SetDword(at::kMsgCurrent, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(msg)));
    g.party_set_select(set, 0);
    Advance(at::kStep);
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
    const auto height = static_cast<unsigned char>((Byte(at::kPartyCount) << 4) + 4);
    const std::uint32_t exp = Dword(at::kExpTotal);
    unsigned char* const w = WindowSlot(1);
    w[2] = 4;
    w[3] = 0;
    w[9] = height;
    SetTickStep(exp);
    Advance(at::kStep);
}

// original 0x431A90 (PSX 0x801EEEE4, paired): the step 0x904AA4 one on while
// any button is held (Input_Held 0x7E1BE8, the word).
extern "C" void __cdecl BattleResult_ExpWaitHeld(void) {
    if (Word(At(at::kHeld)) != 0) Advance(at::kStep);
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
    if (Word(At(at::kHeld)) != 0 && g.file_load_done() != 0) {
        amount = Dword(at::kExpTotal);
    } else {
        const std::uint32_t step = Dword(at::kTickStep) & 0xFFFF;
        const std::uint32_t exp = Dword(at::kExpTotal);
        if (static_cast<std::int32_t>(exp - step) > 0) {
            g.add_exp(step);
            SetDword(at::kExpTotal, Dword(at::kExpTotal) - (Dword(at::kTickStep) & 0xFFFF));
            return;
        }
        amount = exp;
    }
    g.add_exp(amount);
    const auto next = static_cast<unsigned char>(Byte(at::kStep) + 1);
    SetDword(at::kExpTotal, 0);
    Byte(at::kStep) = next;
}

// original 0x431B30 (PSX 0x801EEFD0, paired): once File_LoadDone, the result
// phase 0x904AA3 one on - to the level-up search - with 0x904AA5 and the
// step 0x904AA4 0.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_ExpDone(void) {
    if (g.file_load_done() == 0) return;
    const auto next = static_cast<unsigned char>(Byte(at::kPhase2) + 1);
    Byte(at::kMember) = 0;
    Byte(at::kPhase2) = next;
    Byte(at::kStep) = 0;
}

// ===========================================================================
// The level-ups (0x64AFC0)
// ===========================================================================

// original 0x431B60 (PSX 0x801EF01C, paired): entry 1 of the result phase's
// table 0x64AFA0 - `jmp [0x64AFC0 + (0x904AA4 & 0xFF) * 4]`: 0x431B80, 0x431C10.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_LevelUpStep(void) { RunStep(at::kLevelUpSteps); }

// original 0x431B80 (PSX 0x801EF058, paired): from party slot 0x904AA5 on,
// while it is below the count 0x904AB0: the roster index of the slot's
// character (0x4469D0 of byte +9) into 0x904AA7, the slot one on, and when
// 0x432170(index, 0) answers a non-zero word, 0x904AE8 |= 0x20 and the step
// 0x904AA4 one on (to 0x431C10, the announcement) - that is all for the
// frame. With no slot left, the result phase 0x904AA3 one on with the step
// 0x904AA4 0.
//
// As the original has it: the slot byte is read again after 0x4469D0 for
// its increment, and slot and count afresh after 0x432170; the index passed
// on is 0x4469D0's whole answer (a byte, zero-extended by the callee).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_FindLevelUp(void) {
    while (Byte(at::kMember) < Byte(at::kPartyCount)) {
        const unsigned index = g.roster_index(Member(Byte(at::kMember))[9]);
        const auto next = static_cast<unsigned char>(Byte(at::kMember) + 1);
        Byte(at::kRoster) = static_cast<unsigned char>(index);
        Byte(at::kMember) = next;
        if ((g.level_up_pending(index, 0) & 0xFFFF) != 0) {
            Byte(at::kBattleEnd) = static_cast<unsigned char>(Byte(at::kBattleEnd) | 0x20);
            Advance(at::kStep);
            return;
        }
    }
    Byte(at::kStep) = 0;
    Advance(at::kPhase2);
}

// ===========================================================================
// The zenny and the drops (0x64AFC8)
// ===========================================================================

// original 0x431D50 (PSX 0x801EF310, paired): entry 2 of 0x64AFA0 -
// `jmp [0x64AFC8 + (0x904AA4 & 0xFF) * 4]`: 0x431D70, 0x432050, 0x432070,
// 0x4320F0.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_RewardStep(void) { RunStep(at::kRewardSteps); }

// original 0x431D70 (PSX 0x801EF34C paired; the sibling's BattleResult_Setup
// 0x801EF390): nothing until a button is held (Input_Held) - and, when
// 0x904AE8 has 0x20 (a level-up was shown), until one is newly pressed
// (Input_Pressed) too. Then:
//   1. each party slot whose roster index 0x432170 answers non-zero for
//      levels up: 0x498DE0(index);
//   2. the zenny total 0x904AF0 half as much again (total += total >> 1)
//      when 0x431FE0 says so;
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
    if ((Byte(at::kBattleEnd) & 0x20) != 0 && Word(At(at::kPressed)) == 0) return;
    if (Word(At(at::kHeld)) == 0) return;

    for (unsigned char slot = 0; slot < Byte(at::kPartyCount); ++slot) {
        const unsigned index = g.roster_index(Member(slot)[9]);
        if ((g.level_up_pending(index, 0) & 0xFFFF) != 0) g.level_up(g.roster_index(Member(slot)[9]));
    }
    if (g.zenny_bonus() != 0) SetDword(at::kZennyTotal, Dword(at::kZennyTotal) + (Dword(at::kZennyTotal) >> 1));

    WindowSlot(1)[0] = 0;
    g.window_alloc(1, 4);
    const std::uint32_t zenny = Dword(at::kZennyTotal);
    WindowSlot(1)[2] = 5;
    WindowSlot(1)[3] = 0;
    g.crt_sprintf(Text(at::kText0), Format(at::kFormatD), zenny);
    const unsigned char* const msg = g.msg_system(6);
    const std::uint32_t total = Dword(at::kZennyTotal);
    SetDword(at::kMsgCurrent, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(msg)));
    SetTickStep(total);

    if (Byte(at::kDropCount) != 0) {
        g.window_alloc(0x15, 4);
        unsigned char n = Byte(at::kDropCount);
        unsigned char* const w = WindowSlot(0x15);
        const int last = static_cast<int>(n) - 1;
        w[2] = 1;
        SetWord(w + 4, 0x14);
        SetWord(w + 6, 0x46);
        w[9] = static_cast<unsigned char>((last / 2) * 13 + 0x12);
        if (n > 1) {
            unsigned char i = 0;
            do {
                const auto next = static_cast<unsigned char>(i + 1);
                for (unsigned char j = next; j < n; ++j) {
                    unsigned char* const a = At(at::kDropItems + i * 2u);
                    unsigned char* const b = At(at::kDropItems + j * 2u);
                    const unsigned ai = Word(a);
                    if (ai <= Word(b)) continue;
                    // the original's xor swap, store for store
                    SetWord(a, ai ^ Word(b));
                    SetWord(b, Word(b) ^ Word(a));
                    SetWord(a, Word(a) ^ Word(b));
                    unsigned char* const ci = At(at::kDropCounts + i);
                    unsigned char* const cj = At(at::kDropCounts + j);
                    ci[0] = static_cast<unsigned char>(ci[0] ^ cj[0]);
                    cj[0] = static_cast<unsigned char>(cj[0] ^ ci[0]);
                    ci[0] = static_cast<unsigned char>(ci[0] ^ cj[0]);
                    n = Byte(at::kDropCount);
                }
                i = next;
            } while (static_cast<int>(i) < static_cast<int>(n) - 1);
        }
    }
    g.snd_load_bank(Word(At(at::kBankIndex)) + 3u);
    Advance(at::kStep);
}

// original 0x432050 (PSX 0x801EF780, paired): with a button held, 0x904AA5
// 0 and the step 0x904AA4 one on.
extern "C" void __cdecl BattleResult_ZennyWaitHeld(void) {
    if (Word(At(at::kHeld)) == 0) return;
    const auto next = static_cast<unsigned char>(Byte(at::kStep) + 1);
    Byte(at::kMember) = 0;
    Byte(at::kStep) = next;
}

// original 0x432070 (PSX 0x801EF7B8 paired; the sibling's
// BattleResult_ZennyTick 0x801EF810): BattleResult_ExpTick's shape for the
// zenny total 0x904AF0, through Zenny_Add(amount, 0) - which also counts it
// into 0x904138.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResult_ZennyTick(void) {
    std::uint32_t amount;
    if (Word(At(at::kHeld)) != 0 && g.file_load_done() != 0) {
        amount = Dword(at::kZennyTotal);
    } else {
        const std::uint32_t step = Dword(at::kTickStep) & 0xFFFF;
        const std::uint32_t zenny = Dword(at::kZennyTotal);
        if (static_cast<std::int32_t>(zenny - step) > 0) {
            g.add_zenny(step, 0);
            SetDword(at::kZennyTotal, Dword(at::kZennyTotal) - (Dword(at::kTickStep) & 0xFFFF));
            return;
        }
        amount = zenny;
    }
    g.add_zenny(amount, 0);
    const auto next = static_cast<unsigned char>(Byte(at::kStep) + 1);
    SetDword(at::kZennyTotal, 0);
    Byte(at::kStep) = next;
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
    for (unsigned char i = 0; i < Byte(at::kDropCount); ++i) {
        const unsigned count = At(at::kDropCounts + i)[0];
        const unsigned item = At(at::kDropItems + i * 2u)[0];
        const unsigned category = At(at::kDropItems + i * 2u)[1];
        g.inventory_add(category, item, count, 0);
    }
    Byte(at::kMode1) = 4;
    Byte(at::kMode2) = 0;
    Byte(at::kPhase2) = 0;
    Byte(at::kStep) = 0;
}

// ===========================================================================
// The result window's states (under the window-kind handler 0x597F60)
// ===========================================================================

// original 0x598570 (PSX 0x801F065C, paired): state 4 of window kind 4 (the
// EXP): entry (byte +3 of the record 0x905B84) of a two-entry table the
// original builds on its own stack - 0x5986F0 (the step one on), 0x5985A0
// (the EXP list). The index is not checked: 2..255 would call through the
// words above the table on the original's stack; ours aborts (CLAUDE.md
// rule 4, as battle_flow's Battle_PhaseDispatch).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_ExpState(void) {
    const unsigned index = CurrentWindow()[3];
    if (index >= 2) bof3::Fatal("BattleResultWin_ExpState: step %u, past the two-entry table", index);
    g.exp_window[index]();
}

// original 0x5985A0 (PSX 0x801F06D4, paired): the EXP window's frame -
// 0x5982D0(0x14, 0x28, 0x118, the record's height +9) - and one line per
// party slot, y = 0x2C + 16 n (a byte): the name (Text_DrawAt at 0x19, 5
// characters); at level 99 (byte +0xA of the slot's record is 0x63)
// Msg_SystemPtr(0x41) at 0x55; otherwise the next level ("%2d" of level +
// 1, Text_DrawFont12 at 0x85), the EXP still to go ("%6d" of 0x598810(n),
// at 0xE3) and Msg_SystemPtr(0x15) at 0x55. Text record 2 is the scratch.
//
// As the original has it: the record 0x905B84 is read once, before the
// frame; the count 0x904AB0 after the frame and after each line; the level
// after the name. The original's frame height and y carry the upper half of
// a register the caller left (ecx, esi) - 0x5982D0, Text_DrawAt and
// Text_DrawFont12 read those arguments as words, and ours passes them
// zero-extended; 0x598810 reads its slot as a byte.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_DrawExp(void) {
    const unsigned char* const w = CurrentWindow();
    g.draw_frame(0x14, 0x28, 0x118, w[9]);
    unsigned char y = 0x2C;
    for (unsigned char slot = 0; slot < Byte(at::kPartyCount); ++slot, y = static_cast<unsigned char>(y + 0x10)) {
        unsigned char* const m = Member(slot);
        g.text_draw_at(0x19, y, 0, 5, m);
        const unsigned level = m[0xA];
        if (level == 0x63) {
            g.text_draw_at(0x55, y, 0, 0xFF, g.msg_system(0x41));
            continue;
        }
        g.crt_sprintf(Text(at::kText2), Format(at::kFormat2d), level + 1);
        g.text_draw_font12(0x85, y, 0, At(at::kText2));
        g.crt_sprintf(Text(at::kText2), Format(at::kFormat6d), g.exp_to_next(slot));
        g.text_draw_font12(0xE3, y, 0, At(at::kText2));
        g.text_draw_at(0x55, y, 0, 0xFF, g.msg_system(0x15));
    }
}

// original 0x5986C0 (no PSX twin paired): state 5 of window kind 4 (the
// zenny): the same two-entry stack table - 0x5986F0, 0x598700 (the zenny
// line). Ours aborts past it, as BattleResultWin_ExpState.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_ZennyState(void) {
    const unsigned index = CurrentWindow()[3];
    if (index >= 2) bof3::Fatal("BattleResultWin_ZennyState: step %u, past the two-entry table", index);
    g.zenny_window[index]();
}

// original 0x5986F0 (no PSX twin paired): the record 0x905B84's step +3 one
// on - both states' entry 0, the frame the window opens.
extern "C" void __cdecl BattleResultWin_NextStep(void) {
    unsigned char* const w = CurrentWindow();
    w[3] = static_cast<unsigned char>(w[3] + 1);
}

// original 0x598700 (no PSX twin paired): the zenny window - the medium box
// at (0x6E, 0x28), the party's zenny 0x904058 in "%7d" at (0x70, 0x2A) and
// the text 0x66A31C after it at 0xC4.
//
// As the original has it: the zenny is read after the box.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_DrawZenny(void) {
    g.draw_medium_box(0x6E, 0x28);
    g.crt_sprintf(Text(at::kText2), Format(at::kFormat7d), Dword(at::kPartyZenny));
    g.text_draw_font12(0x70, 0x2A, 0, At(at::kText2));
    g.text_draw_at(0xC4, 0x2A, 0, 0xFF, At(at::kZennyUnit));
}

// ===========================================================================

void BattleResult_Inject() {
    if (bof3::WantsShadow("battle_result")) battle_result::SelfTest();
    BOF3_INJECT(BattleResult_SplitExp);
    BOF3_INJECT(BattleResult_OpenExpWindow);
    BOF3_INJECT(BattleResult_ExpWaitHeld);
    BOF3_INJECT(BattleResult_ExpTick);
    BOF3_INJECT(BattleResult_ExpDone);
    BOF3_INJECT(BattleResult_LevelUpStep);
    BOF3_INJECT(BattleResult_FindLevelUp);
    BOF3_INJECT(BattleResult_RewardStep);
    BOF3_INJECT(BattleResult_Setup);
    BOF3_INJECT(BattleResult_ZennyWaitHeld);
    BOF3_INJECT(BattleResult_ZennyTick);
    BOF3_INJECT(BattleResult_AwardDrops);
    BOF3_INJECT(BattleResultWin_ExpState);
    BOF3_INJECT(BattleResultWin_DrawExp);
    BOF3_INJECT(BattleResultWin_ZennyState);
    BOF3_INJECT(BattleResultWin_NextStep);
    BOF3_INJECT(BattleResultWin_DrawZenny);
}
