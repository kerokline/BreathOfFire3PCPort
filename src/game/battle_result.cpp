// The battle result: the PSX BATL_END.EMI's code compiled into the exe
// (docs/battle_result.md). Three .data tables of steps by the byte 0x904AA4,
// under the result phase's own table 0x64AFA0 (by 0x904AA3, dispatched by
// group CC's 0x431910):
//   - 0x64AFAC (entry 0 of 0x64AFA0, through CC's 0x431920): the EXP - split
//     it, open its window, wait for a button, tick it into the party, wait
//     again (0x431940, 0x431A20, 0x431A90, 0x431AB0, 0x431B30);
//   - 0x64AFC0 (entry 1, through BattleResult_LevelUpStep 0x431B60): the
//     level-up search (0x431B80) and its announcement (0x431C10,
//     BattleResult_LevelUpNotice, round twelve BE1's - no route reaches it);
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

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_result_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_result {

using move_script::At;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    {Fn<Handler>(::bof3::Code(bof3::addr::BattleResultWin_NextStep)), Fn<Handler>(::bof3::Code(bof3::addr::BattleResultWin_DrawExp))},
    {Fn<Handler>(::bof3::Code(bof3::addr::BattleResultWin_NextStep)), Fn<Handler>(::bof3::Code(bof3::addr::BattleResultWin_DrawZenny))},
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(kCountMembers.v)),
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(kZennyBonus.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kAddExp.v)),
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(kRosterIndex.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kLevelUpPending.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kLevelUp.v)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kAddZenny.v)),
    Fn<void (__cdecl*)(int, int, int, int)>(::bof3::Code(kDrawFrame.v)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kExpToNext.v)),
    Crt_sprintf, Msg_SystemPtr, PartySet_Select, Window_Alloc, File_LoadDone, Snd_LoadBankFile,
    Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::Inventory_Add)),   // Inventory_Add, ours
    Text_DrawAt, Text_DrawFont12, BattleWin_DrawMediumBox,
};
Callees g = kOriginals;

}  // namespace battle_result

using namespace battle_result;

namespace {

unsigned char& Byte(std::uint32_t address) { return battle_result::At(address)[0]; }
unsigned char* MemberBattleResult(unsigned slot) { return battle_result::At(::bof3::Img(battle_result::at::kMembers.v) + (slot & 0xFF) * battle_result::at::kMemberSize); }
// The record the window task is running, re-read wherever the original reads it.
unsigned char* CurrentWindow() { return battle_result::At(BattleWinStates_Current); }
// MemberBattleResult() is +0x80 into the member record (0x802DC0); the record, and its
// typed view, starts 0x80 before.
char* TextBattleResult(std::uint32_t address) { return reinterpret_cast<char*>(battle_result::At(address)); }
const char* Format(std::uint32_t address) { return reinterpret_cast<const char*>(battle_result::At(address)); }

}  // namespace

// ===========================================================================
// The EXP (0x64AFAC)
// ===========================================================================

// ===========================================================================
// The level-ups (0x64AFC0)
// ===========================================================================

// ===========================================================================
// The zenny and the drops (0x64AFC8)
// ===========================================================================

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
    const unsigned index = rv::As<records::WindowRecord>(CurrentWindow())->state;
    if (index >= 2) bof3::Fatal("BattleResultWin_ExpState: step %u, past the two-entry table", index);
    battle_result::g.exp_window[index]();
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
//
// DIVERGENCE DIV-0060: under a Latin language overlay the line takes the US
// release's layout - the EXP still to go right-aligned before the sentence,
// the sentence, the next level at the right edge: "%6d" at 0x36, the
// message at 0x82, "%2d" at 0x114 (read off the owner's photograph of the
// US screen, 2026-09-27; the level-99 message at 0x82 too). The Chinese
// layout puts the sentence at 0x55 and the level at 0x85, which the US
// sentence "EXP to next level:" (144 units) runs over - the owner's "a
// digit hidden in 'next'". Same calls, same order, other x. 0 until
// BattleResult_Inject patches it to 1 under the name BattleResultExpLayout.
unsigned char g_exp_layout_us = 0;

extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_DrawExp(void) {
    const unsigned char* const w = CurrentWindow();
    battle_result::g.draw_frame(0x14, 0x28, 0x118, w[9]);
    const bool us = g_exp_layout_us != 0;
    const int x_msg = us ? 0x82 : 0x55, x_level = us ? 0x114 : 0x85, x_exp = us ? 0x36 : 0xE3;
    unsigned char y = 0x2C;
    for (unsigned char slot = 0; slot < Byte(::bof3::Img(at::kPartyCount.v)); ++slot, y = static_cast<unsigned char>(y + 0x10)) {
        unsigned char* const m = MemberBattleResult(slot);
        auto* const member_rec = rv::As<records::PartyMember>(m - 0x80);
        battle_result::g.text_draw_at(0x19, y, 0, 5, m);
        const unsigned level = member_rec->level;
        if (level == 0x63) {
            battle_result::g.text_draw_at(x_msg, y, 0, 0xFF, battle_result::g.msg_system(0x41));
            continue;
        }
        battle_result::g.crt_sprintf(TextBattleResult(::bof3::Img(at::kText2.v)), Format(::bof3::Img(Boss26Fx_CountFormat_addr)), level + 1);
        battle_result::g.text_draw_font12(x_level, y, 0, battle_result::At(::bof3::Img(at::kText2.v)));
        battle_result::g.crt_sprintf(TextBattleResult(::bof3::Img(at::kText2.v)), Format(::bof3::Img(at::kFormat6d.v)), battle_result::g.exp_to_next(slot));
        battle_result::g.text_draw_font12(x_exp, y, 0, battle_result::At(::bof3::Img(at::kText2.v)));
        battle_result::g.text_draw_at(x_msg, y, 0, 0xFF, battle_result::g.msg_system(0x15));
    }
}

// original 0x5986C0 (no PSX twin paired): state 5 of window kind 4 (the
// zenny): the same two-entry stack table - 0x5986F0, 0x598700 (the zenny
// line). Ours aborts past it, as BattleResultWin_ExpState.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_ZennyState(void) {
    const unsigned index = rv::As<records::WindowRecord>(CurrentWindow())->state;
    if (index >= 2) bof3::Fatal("BattleResultWin_ZennyState: step %u, past the two-entry table", index);
    battle_result::g.zenny_window[index]();
}

// original 0x5986F0 (no PSX twin paired): the record 0x905B84's step +3 one
// on - both states' entry 0, the frame the window opens.
extern "C" void __cdecl BattleResultWin_NextStep(void) {
    auto* const window_rec = rv::As<records::WindowRecord>(CurrentWindow());
    window_rec->state = static_cast<unsigned char>(window_rec->state + 1);
}

// original 0x598700 (no PSX twin paired): the zenny window - the medium box
// at (0x6E, 0x28), the party's zenny 0x904058 in "%7d" at (0x70, 0x2A) and
// the text 0x66A31C after it at 0xC4.
//
// As the original has it: the zenny is read after the box.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_DrawZenny(void) {
    battle_result::g.draw_medium_box(0x6E, 0x28);
    battle_result::g.crt_sprintf(TextBattleResult(::bof3::Img(at::kText2.v)), Format(::bof3::Img(at::kFormat7d.v)), Party_Zenny);
    battle_result::g.text_draw_font12(0x70, 0x2A, 0, battle_result::At(::bof3::Img(at::kText2.v)));
    battle_result::g.text_draw_at(0xC4, 0x2A, 0, 0xFF, battle_result::At(::bof3::Img(at::kZennyUnit.v)));
}

// ===========================================================================

void BattleResult_Inject() {
    if (bof3::WantsShadow("battle_result")) battle_result::SelfTest();
    // DIVERGENCE DIV-0060: a Latin language overlay only, as MenuLists_Inject
    // tests it for DIV-0058.
    {
        if (Lang_Latin()) {
            static const std::uint8_t was = 0, is = 1;
            bof3::PatchBytes("BattleResultExpLayout",
                             Low32(&g_exp_layout_us),
                             &was, &is, 1);
            bof3::Log("DIV-0060    EXP line in the US layout: %s", g_exp_layout_us ? "on" : "off");
        }
    }
    BOF3_INJECT(BattleResultWin_ExpState);
    BOF3_INJECT(BattleResultWin_DrawExp);
    BOF3_INJECT(BattleResultWin_ZennyState);
    BOF3_INJECT(BattleResultWin_NextStep);
    BOF3_INJECT(BattleResultWin_DrawZenny);
}
