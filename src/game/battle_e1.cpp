// Round twelve group BE1 (docs/takeover-queue-field-battle.md section 3;
// analysis/round12_cut.tsv's BE1 rows and the two starts the cut lacks):
// 41 functions of the battle engine's first run 0x42D7A0..0x432B6A, each
// read to its last instruction with capstone (2026-09-29) and taken through
// the boss harness's engine frame (boss_harness.h, docs/boss_harness.md
// section 10). docs/battle_e1.md has every function one row each.
//
//   - BATE (game mode 9's overlay, the byte block 0x929F00): state 3's tally
//     - six counters counted up to what 0x939A04 / 0x939A08 / 0x939A0C
//     hold, character record 7's stat words raised by them - and state 2's
//     equipment screen over the same record's two equipment bytes, with
//     their dispatchers by 0x929F01 / 0x929F02;
//   - the command menu's held-button steps (Battle_InputSteps[3] and its
//     three), auto battle (Battle_MenuSteps[5], the PSX Cmd_AutoBattle_Begin
//     by the sibling's notes, verified by reading);
//   - action kind 3's dispatcher and two steps, the random ability pick of
//     actions 0x24 / 0x25 / 0x8C, the ability notice (AfterSteps[1]);
//   - the end's party restore (0x64AF7C[2]), the result's EXP-share count,
//     zenny-bonus test, level-up notice, and a character's level gain from
//     its EXP (the result windows' and BE7's);
//   - the loss screen (0x904AE8 bit 0's way out): its dispatcher, five steps
//     and four draws.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables where the original jumps through
// whatever follows (round9 doc section 6); every other unchecked index is
// reproduced and described in the doc (section 7). Every call goes through
// the harness (BH_CALL / BH_AT), so the start-up fuzz can stand recorders in
// for the callees.
#include "game/battle_e1.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e1_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e1::at;
using U = std::uint32_t;
using boss_harness::Handler;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
U L(U address) { return static_cast<U>(Long(At(address))); }
unsigned char* PtrAt(U address) { return At(L(address)); }
unsigned char* Packet() { return At(L(at::kPacketNext)); }
unsigned char* Member(U index) { return At(at::kMembers + index * at::kMemberSize); }
unsigned short* Stat(U address) { return reinterpret_cast<unsigned short*>(At(address)); }
unsigned Pressed() { return Word(At(at::kInputPressed)); }
// fild dword / fstp dword: the int to a float, nearest.
void PutFloat(unsigned char* p, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
void Rgb(unsigned char* p, unsigned at_, unsigned char v) { p[at_] = p[at_ + 1] = p[at_ + 2] = v; }

// jmp [table + 4 * index]: the entry as read (the fuzz swaps the table's
// cells for its recorders), a Fatal past `entries` (the original jumps
// through the dword after: docs/battle_e1.md section 7). The jmp leaves the
// caller's stack word and the entry's eax in place: ours hands the word on
// and answers the entry's eax (round 11 doc section 5.1).
using Entry = unsigned long (__cdecl*)(unsigned long);
unsigned long Dispatch(const char* who, U table, unsigned index, unsigned entries, unsigned long through) {
    if (index >= entries)
        bof3::Fatal("%s: the step byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e1.md section 7)",
                    who, index, entries, (unsigned)table);
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(L(table + 4 * index)))(through);
}

// The CRT's memcpy (Capcom's, by address).
void Memcpy(U dst, U src, unsigned n) {
    BH_AT(void* (__cdecl*)(void*, const void*, unsigned), at::kMemcpy)(At(dst), At(src), n);
}
void Backdrop() { BH_CALL(Menu_DrawBackdrop)(B(at::kMenuShade)); }

// A texture page and draw mode for the next primitives: Gpu_GetTPage(tp, abr,
// x, y), Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, page & 0xFFFF, 0) - the fifth
// word is the GetTPage push the caller's `add esp, 0x10` leaves -, then
// Gfx_CommitPrim(slot, 0xC).
void DrawMode(unsigned tp, unsigned abr, int x, int y, unsigned slot) {
    const unsigned page = BH_CALL(Gpu_GetTPage)(tp, abr, x, y);
    BH_CALL(Gpu_SetDrawMode)(Packet(), 0, 0, page & 0xFFFF, 0);
    BH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// The loss screen's sprites: CLUT (0, 0x1FA), shade, x / y floats (as bit
// patterns), u, v, w, h - the fields stored before Gpu_SetSprt, as the
// original does - then Gfx_CommitPrim(2, 0x1C).
void LossSprite(unsigned char shade, U xf, U yf, unsigned char u, unsigned char v, unsigned w, unsigned h) {
    unsigned char* const p = Packet();
    const unsigned clut = BH_CALL(Gpu_GetClut)(0, 0x1FA);
    SetWord(p + 0x16, clut);
    Rgb(p, 4, shade);
    SetLong(p + 8, static_cast<std::int32_t>(xf));
    SetLong(p + 0xC, static_cast<std::int32_t>(yf));
    p[0x14] = u;
    p[0x15] = v;
    SetWord(p + 0x18, w);
    SetWord(p + 0x1A, h);
    BH_CALL(Gpu_SetSprt)(p);
    BH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// A CLUT run of 0x100 words greyed: each colour's three 5-bit channels
// averaged (the sum / 3, rounded down: the original's multiply by 0x55555556)
// into all three, bit 15 kept.
void GreyClut(U at_) {
    for (unsigned k = 0; k < 0x100; ++k) {
        unsigned char* const w = At(at_ + 2 * k);
        const unsigned c = Word(w);
        const unsigned q = (((c >> 10) & 0x1F) + ((c >> 5) & 0x1F) + (c & 0x1F)) / 3;
        SetWord(w, (q << 10) | (q << 5) | q | (c & 0x8000));
    }
}

// The party's +0x130 dword (the loss steps' and the notices' flag bits).
U MemberFlags(U index) { return static_cast<U>(Long(Member(index) + 0x130)); }

}  // namespace

// ============================================================================
// BATE: state 3, the tally (0x42D760 by 0x929F01, 0x42D770 by 0x929F02)
// ============================================================================

// original 0x42D7A0 (0x64ADC8[1]): the backdrop and the tally at (0x37,
// 0x37); once no transition runs, the tally's counts set up and applied,
// 0x929F08 = 0, 0x929F02 = 0, 0x929F01 + 1 (read after the call).
extern "C" void __cdecl BattleExtra_TallyOpen(void) {
    Backdrop();
    BH_CALL(BattleExtra_DrawTally)(0x37, 0x37);
    if (Word(At(at::kWaitWord)) != 0) return;
    BH_CALL(BattleExtra_ApplyTally)();
    const unsigned char step = B(at::kModeStep);
    B(at::kTallyRow) = 0;
    B(at::kModeSub) = 0;
    B(at::kModeStep) = static_cast<unsigned char>(step + 1);
}

// original 0x42D7F0 (0x64ADBC[1]): the backdrop and the tally; the counter
// 0x929F08 (signed; unchecked below 0 - docs/battle_e1.md section 7) counted
// up by its step, or set to its target when it gets there or a button is
// pressed (the low byte of Input_Pressed), then the next; after the sixth, a
// press starts Transition_Start(2) and 0x929F01 + 1.
extern "C" void __cdecl BattleExtra_TallyCount(void) {
    Backdrop();
    BH_CALL(BattleExtra_DrawTally)(0x37, 0x37);
    const auto row = static_cast<signed char>(B(at::kTallyRow));
    if (row >= 6) {
        if (B(at::kInputPressed) == 0) return;
        BH_CALL(Transition_Start)(2);
        B(at::kModeStep) = static_cast<unsigned char>(B(at::kModeStep) + 1);
        return;
    }
    const U shown_at = at::kTallyShown + 4 * static_cast<U>(static_cast<std::int32_t>(row));
    const U shown = L(shown_at) + B(at::kTallyStep + static_cast<U>(static_cast<std::int32_t>(row)));
    SetLong(At(shown_at), static_cast<std::int32_t>(shown));
    const U target = L(at::kTallyTarget + 4 * static_cast<U>(static_cast<std::int32_t>(row)));
    if (shown < target && B(at::kInputPressed) == 0) return;
    SetLong(At(shown_at), static_cast<std::int32_t>(target));
    B(at::kTallyRow) = static_cast<unsigned char>(row + 1);
}

// original 0x42D880 (0x64ADBC[2]): once no transition runs, 0x929F04 = 0,
// state 1 (0x42D750: Window_ResetAll, Game_Step + 1), steps 0; else the
// backdrop and the tally.
extern "C" void __cdecl BattleExtra_TallyClose(void) {
    if (Word(At(at::kWaitWord)) == 0) {
        B(at::kModeTimer) = 0;
        B(at::kMode) = 1;
        B(at::kModeStep) = 0;
        B(at::kModeSub) = 0;
        return;
    }
    Backdrop();
    BH_CALL(BattleExtra_DrawTally)(0x37, 0x37);
}

// original 0x42D8C0: the tally window at (x, y): Menu_DrawBox(x - 4, y - 8,
// 0xDC, 0x8C, 0xF0, colour 0x903A5A) and its border, the title message
// 0xE7; three rows 32 apart, each two labels (messages 0xE8 + i, 0xEB + i),
// the counters 0x675E88[i] and 0x675E94[i] through Crt_sprintf into
// 0x904BA0 and Text_DrawFont12, and BattleExtra_DrawPlus between them; then
// message 0xF0 and message 0xEF twice. The row's y is the original's
// y + 32 * esi, where esi's low half is the row and its upper half is left
// over from the previous row's y (y + 6 before the first): the same for any
// y below 0x10000 - 0x6B, kept exact for the others.
extern "C" void __cdecl BattleExtra_DrawTally(int x_arg, int y_arg) {
    const U x = static_cast<U>(x_arg), y = static_cast<U>(y_arg);
    const auto I = [](U v) { return static_cast<int>(v); };
    BH_CALL(Menu_DrawBox)(I(x - 4), I(y - 8), 0xDC, 0x8C, 0xF0, B(at::kBoxColour));
    BH_CALL(Menu_DrawBorder)(I(x - 4), I(y - 8), 0x1A, 0x10);
    const unsigned char* const title = BH_CALL(Msg_SystemPtr)(0xE7);
    BH_CALL(Text_DrawAt)(I(x + 0x45), I(y + 6), 0, 0xFF, title);
    const U right = x + 0x94;
    U esi = y + 6;
    for (U i = 0; i < 3; ++i) {
        const U e = (esi & 0xFFFF0000u) | i;
        const U row = y + (e << 5);
        const unsigned char* const left_label = BH_CALL(Msg_SystemPtr)(e + 0xE8);
        BH_CALL(Text_DrawAt)(I(x + 6), I(row + 0x1E), 0, 0xFF, left_label);
        const unsigned char* const right_label = BH_CALL(Msg_SystemPtr)(e + 0xEB);
        BH_CALL(Text_DrawAt)(I(right), I(row + 0x1E), 0, 0xFF, right_label);
        char* const text = reinterpret_cast<char*>(At(at::kTallyText));
        const unsigned char* const utext = At(at::kTallyText);
        BH_CALL(Crt_sprintf)(text, reinterpret_cast<const char*>(At(at::kTallyFmtWide)), L(at::kTallyShown + 4 * i));
        BH_CALL(Text_DrawFont12)(I(x + 6), I(row + 0x2B), 0, utext);
        BH_CALL(Crt_sprintf)(text, reinterpret_cast<const char*>(At(at::kTallyFmtNarrow)), L(at::kTallyShown + 0xC + 4 * i));
        BH_CALL(Text_DrawFont12)(I(right), I(row + 0x2B), 0, utext);
        BH_CALL(BattleExtra_DrawPlus)(I(right), I(row + 0x2B));
        esi = row + 0x2B;
    }
    const U lx = x + 0x5A;
    const unsigned char* const total = BH_CALL(Msg_SystemPtr)(0xF0);
    BH_CALL(Text_DrawAt)(I(lx), I(y + 0x2B), 0, 0xFF, total);
    const unsigned char* const small1 = BH_CALL(Msg_SystemPtr)(0xEF);
    BH_CALL(Text_DrawSmall)(I(lx), I(y + 0x4F), 0, 0xFF, small1);
    const unsigned char* const small2 = BH_CALL(Msg_SystemPtr)(0xEF);
    BH_CALL(Text_DrawSmall)(I(lx), I(y + 0x6F), 0, 0xFF, small2);
}

// original 0x42DA60: the tally's counters: shown 0x675E88[0..5] = 0;
// targets [0] and [3] the count byte 0x939A0C, [1] the sum 0x939A04, [2] the
// sum 0x939A08, [4] 0x939A04 / 20, [5] 2 / 4 / 6 by 0x939A08 below 0x32 /
// 0x50 / above; then character record 7's words raised: +0x46 and +0x26 by
// the count and target [3] (Stat_AddClamped), +0x40 and +0x20 by target [4]
// (Stat_AddCap999), +0x44 and +0x24 by target [5] - each target re-read from
// its cell after the call before it.
extern "C" void __cdecl BattleExtra_ApplyTally(void) {
    const U sum_a = L(at::kSumA);
    for (U k = 0; k < 6; ++k) SetLong(At(at::kTallyShown + 4 * k), 0);
    const U count = B(at::kCount);
    SetLong(At(at::kTallyTarget + 4), static_cast<std::int32_t>(sum_a));
    SetLong(At(at::kTallyTarget), static_cast<std::int32_t>(count));
    const U sum_b = L(at::kSumB);
    SetLong(At(at::kTallyTarget + 8), static_cast<std::int32_t>(sum_b));
    SetLong(At(at::kTallyTarget + 0xC), static_cast<std::int32_t>(count));
    SetLong(At(at::kTallyTarget + 0x10), static_cast<std::int32_t>(sum_a / 20));
    SetLong(At(at::kTallyTarget + 0x14), sum_b < 0x32 ? 2 : sum_b < 0x50 ? 4 : 6);
    BH_CALL(Stat_AddClamped)(Stat(at::kGuestStat46), count);
    BH_CALL(Stat_AddClamped)(Stat(at::kGuestStat26), Word(At(at::kTallyTarget + 0xC)));
    BH_CALL(Stat_AddCap999)(Stat(at::kGuestHpBase), Word(At(at::kTallyTarget + 0x10)));
    BH_CALL(Stat_AddCap999)(Stat(at::kGuestHpMax), Word(At(at::kTallyTarget + 0x10)));
    BH_CALL(Stat_AddClamped)(Stat(at::kGuestStat44), Word(At(at::kTallyTarget + 0x14)));
    BH_CALL(Stat_AddClamped)(Stat(at::kGuestStat24), Word(At(at::kTallyTarget + 0x14)));
}

// original 0x42DB40: a plus sign of two semi-transparent LINE_F2s in grey
// 0x80 at the s16 (x, y): (x + 5, y) to (x + 5, y + 10), then (x, y + 5) to
// (x + 10, y + 5), each Gfx_CommitPrim(1, 0x20).
extern "C" void __cdecl BattleExtra_DrawPlus(int x_arg, int y_arg) {
    const int x = static_cast<std::int16_t>(x_arg), y = static_cast<std::int16_t>(y_arg);
    unsigned char* p = Packet();
    BH_CALL(Gpu_SetLineF2)(p);
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, x + 5);
    Rgb(p, 4, 0x80);
    PutFloat(p + 0xC, y);
    PutFloat(p + 0x14, x + 5);
    PutFloat(p + 0x18, y + 10);
    BH_CALL(Gfx_CommitPrim)(1, 0x20);
    p = Packet();
    BH_CALL(Gpu_SetLineF2)(p);
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, x);
    Rgb(p, 4, 0x80);
    PutFloat(p + 0xC, y + 5);
    PutFloat(p + 0x14, x + 10);
    PutFloat(p + 0x18, y + 5);
    BH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// ============================================================================
// BATE: state 2, the equipment screen (0x42DC00 by 0x929F01)
// ============================================================================

// original 0x42DC00 (0x64ADAC[2], state 2): by 0x929F01 through
// BattleExtra_EquipSteps 0x64ADE0 (3: the opening, the screen, the leaving).
extern "C" unsigned long __cdecl BattleExtra_EquipDispatch(unsigned long through) {
    return Dispatch("BattleExtra_EquipDispatch", at::kEquipSteps, B(at::kModeStep), 3, through);
}

// original 0x42DC10 (0x64ADE0[0]): by 0x929F02 through 0x64ADEC (3: 0x42D780
// Transition_Start(3) and on, BattleExtra_EquipOpen, BattleExtra_EquipFadeIn).
extern "C" unsigned long __cdecl BattleExtra_EquipOpenDispatch(unsigned long through) {
    return Dispatch("BattleExtra_EquipOpenDispatch", at::kEquipOpenSteps, B(at::kModeSub), 3, through);
}

// original 0x42DC20 (0x64ADEC[1]): the backdrop; once no transition runs,
// Sound_PlayEffect(0x102), 0x42E0E0(), 0x929F04 = 4, 0x929F02 + 1 (read after
// the calls), and the five bytes 0x669CE8 copied over character record 7's
// first five.
extern "C" void __cdecl BattleExtra_EquipOpen(void) {
    Backdrop();
    if (Word(At(at::kWaitWord)) != 0) return;
    BH_CALL(Sound_PlayEffect)(0x102);
    BH_AT(void (__cdecl*)(), at::kEquipOpenHelper)();
    const unsigned char sub = B(at::kModeSub);
    B(at::kModeTimer) = 4;
    B(at::kModeSub) = static_cast<unsigned char>(sub + 1);
    Memcpy(at::kGuest, at::kGuestHeadIn, 5);
}

// original 0x42DC80 (0x64ADEC[2]): the backdrop; 0x929F04 - 1, and at 0:
// 0x675EBF = 0, 0x675EC0 = 1, 0x675EBE = 0, 0x929F01 + 1, 0x929F02 = 0,
// 0x929F09 = 0.
extern "C" void __cdecl BattleExtra_EquipFadeIn(void) {
    Backdrop();
    const auto timer = static_cast<unsigned char>(B(at::kModeTimer) - 1);
    B(at::kModeTimer) = timer;
    if (timer != 0) return;
    const unsigned char step = B(at::kModeStep);
    B(at::kEquipOpenBytes + 1) = 0;
    B(at::kEquipOpenBytes + 2) = 1;
    B(at::kEquipOpenBytes) = 0;
    B(at::kModeStep) = static_cast<unsigned char>(step + 1);
    B(at::kModeSub) = 0;
    B(at::kModeByte9) = 0;
}

// original 0x42DCD0 (0x64ADE0[1]): `call [0x64ADF8 + 4 * 0x929F02]` (2: the
// slot input, the list input; a Fatal past them), then the backdrop.
extern "C" void __cdecl BattleExtra_EquipRun(void) {
    const unsigned sub = B(at::kModeSub);
    if (sub >= 2)
        bof3::Fatal("BattleExtra_EquipRun: 0x929F02 is %u, past the 2 entries of 0x64ADF8 - the original calls through "
                    "the dword after (docs/battle_e1.md section 7)",
                    sub);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(at::kEquipRunSteps + 4 * sub)))();
    Backdrop();
}

// original 0x42DCF0 (0x64ADF8[0]): window 4 (the hand) at window 1's x + 6
// and y + 0x57 + 13 * its cursor +0xA; Input_AutoRepeat(Input_Pressed &
// 0xF000); window 1's +0xB = 0xFF, +0xD = 1; up or down (0x5000) plays 0x101
// and flips the cursor between its two values (xor 3); window 0's +9 = the
// cursor; window 2's help word = Item_HelpMessage(2, record 7's +0x15) on 3,
// else (1, +0x12); window 3's +0x10 = 0xDC. Then Input_Pressed: confirm -
// 0x103, window 0's cursor +0xB = its +0xA, window 1's +0xB = the cursor,
// 0x929F02 + 1; cancel - 0x106, 0x102, windows 0..3's +3 = 1, windows 4 and
// 21's +0 = 0, 0x929F04 = 5, 0x929F01 + 1, 0x929F02 = 0.
extern "C" void __cdecl BattleExtra_EquipSlotInput(void) {
    SetWord(At(at::kWin4 + 4), L(at::kWin1 + 4) + 6);
    B(at::kWin4) = 1;
    const U cursor = B(at::kWin1 + 0xA);
    SetWord(At(at::kWin4 + 6), 13 * cursor + Word(At(at::kWin1 + 6)) + 0x57);
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(Pressed() & 0xF000);
    B(at::kWin1 + 0xB) = 0xFF;
    B(at::kWin1 + 0xD) = 1;
    if (repeat & 0x5000) {
        BH_CALL(Sound_PlayEffect)(0x101);
        B(at::kWin1 + 0xA) = static_cast<unsigned char>(B(at::kWin1 + 0xA) ^ 3);
    }
    const unsigned char now = B(at::kWin1 + 0xA);
    B(at::kWin0 + 9) = now;
    const bool second = now == 3;
    const unsigned item = B(second ? at::kGuestEquipB : at::kGuestEquipA);
    SetWord(At(at::kWin2 + 0x10), BH_CALL(Item_HelpMessage)(second ? 2 : 1, item));
    const unsigned pressed = Pressed();
    SetWord(At(at::kWin3 + 0x10), 0xDC);
    if (Word(At(at::kConfirmButtons)) & pressed) {
        BH_CALL(Sound_PlayEffect)(0x103);
        const unsigned char top = B(at::kWin0 + 0xA);
        const unsigned char was = B(at::kWin1 + 0xA);
        B(at::kWin0 + 0xB) = top;
        const unsigned char sub = B(at::kModeSub);
        B(at::kWin1 + 0xB) = was;
        B(at::kModeSub) = static_cast<unsigned char>(sub + 1);
        return;
    }
    if (Word(At(at::kCancelButtons)) & pressed) {
        BH_CALL(Sound_PlayEffect)(0x106);
        BH_CALL(Sound_PlayEffect)(0x102);
        const unsigned char step = B(at::kModeStep);
        B(at::kWin0 + 3) = 1;
        B(at::kWin1 + 3) = 1;
        B(at::kWin2 + 3) = 1;
        B(at::kWin3 + 3) = 1;
        B(at::kWin4) = 0;
        B(at::kWin21) = 0;
        B(at::kModeTimer) = 5;
        B(at::kModeStep) = static_cast<unsigned char>(step + 1);
        B(at::kModeSub) = 0;
    }
}

// original 0x42DE50 (0x64ADF8[1]): window 0's list (x +4, y +6, category +8,
// scroll +0xA, cursor +0xB, count +0xD, the scroll animation word +0x10):
// window 4's hand at x + 7 and y + 13 * (cursor - scroll + 2), window 1's +0xD
// = 0; window 2's help word = Item_HelpMessage(category, +0xD); window 3's
// +0x10 = 0xDD; Input_AutoRepeat(Input_Pressed & 0xF00C); 0x42E2F0(); then
// by the answer - 0x1000 cursor - 1 (not below 0; above the scroll no more:
// +0x10 = 0xF0), 0x4000 cursor + 1 (below 0x7F; at scroll + 9: +0x10 =
// 0x10), 4 a page back (9), 8 a page on (to the scroll 0x77, the cursor 0x7F)
// - Sound_PlayEffect(0x100) when the cursor moved. With no scroll running,
// Input_Pressed again: confirm - with a count, 0x104, 0x42E250(), the
// cursor 0xFF and 0x929F02 - 1, without, 0x107; cancel - 0x106, the cursor
// 0xFF, 0x929F02 - 1.
extern "C" void __cdecl BattleExtra_EquipListInput(void) {
    const U x = L(at::kWin0 + 4) + 7;
    B(at::kWin1 + 0xD) = 0;
    SetWord(At(at::kWin4 + 4), x);
    const U scroll = B(at::kWin0 + 0xA), cursor = B(at::kWin0 + 0xB);
    const unsigned count = B(at::kWin0 + 0xD), category = B(at::kWin0 + 8);
    SetWord(At(at::kWin4 + 6), 13 * (cursor - scroll + 2) + Word(At(at::kWin0 + 6)));
    SetWord(At(at::kWin2 + 0x10), BH_CALL(Item_HelpMessage)(category, count));
    const unsigned pressed = Pressed() & 0xF00C;
    SetWord(At(at::kWin3 + 0x10), 0xDD);
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pressed);
    BH_AT(unsigned char (__cdecl*)(), at::kEquipFrameHelper)();
    auto now = B(at::kWin0 + 0xB);
    const unsigned char was = now;
    if (repeat & 0x1000) {
        if (now > 0) B(at::kWin0 + 0xB) = --now;
        if (now < B(at::kWin0 + 0xA)) SetWord(At(at::kWin0 + 0x10), 0xF0);
    } else if (repeat & 0x4000) {
        if (now < 0x7F) B(at::kWin0 + 0xB) = ++now;
        if (!(static_cast<int>(now) < static_cast<int>(B(at::kWin0 + 0xA)) + 9)) SetWord(At(at::kWin0 + 0x10), 0x10);
    } else if (repeat & 4) {
        auto top = B(at::kWin0 + 0xA);
        if (top == 0) {
            now = 0;
            B(at::kWin0 + 0xB) = 0;
        } else if (top < 9) {
            now = static_cast<unsigned char>(now - top);
            B(at::kWin0 + 0xA) = 0;
            B(at::kWin0 + 0xB) = now;
        } else {
            now = static_cast<unsigned char>(now - 9);
            top = static_cast<unsigned char>(top - 9);
            B(at::kWin0 + 0xB) = now;
            B(at::kWin0 + 0xA) = top;
        }
    } else if (repeat & 8) {
        auto top = B(at::kWin0 + 0xA);
        if (top == 0x77) {
            now = 0x7F;
            B(at::kWin0 + 0xB) = 0x7F;
        } else if (now > 0x6E) {
            const auto by = static_cast<unsigned char>(0x77 - top);
            B(at::kWin0 + 0xA) = 0x77;
            now = static_cast<unsigned char>(now + by);
            B(at::kWin0 + 0xB) = now;
        } else {
            now = static_cast<unsigned char>(now + 9);
            top = static_cast<unsigned char>(top + 9);
            B(at::kWin0 + 0xB) = now;
            B(at::kWin0 + 0xA) = top;
        }
    }
    if (was != now) BH_CALL(Sound_PlayEffect)(0x100);
    if (Word(At(at::kWin0 + 0x10)) != 0) return;
    const unsigned again = Pressed();
    if (Word(At(at::kConfirmButtons)) & again) {
        if (B(at::kWin0 + 0xD) == 0) {
            BH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        BH_CALL(Sound_PlayEffect)(0x104);
        BH_AT(void (__cdecl*)(), at::kEquipConfirmHelper)();
        const unsigned char sub = B(at::kModeSub);
        B(at::kWin0 + 0xB) = 0xFF;
        B(at::kModeSub) = static_cast<unsigned char>(sub - 1);
        return;
    }
    if (Word(At(at::kCancelButtons)) & again) {
        BH_CALL(Sound_PlayEffect)(0x106);
        const unsigned char sub = B(at::kModeSub);
        B(at::kWin0 + 0xB) = 0xFF;
        B(at::kModeSub) = static_cast<unsigned char>(sub - 1);
    }
}

// original 0x42E040 (0x64ADE0[2]): by 0x929F02 through 0x64AE00 (2).
extern "C" unsigned long __cdecl BattleExtra_EquipLeaveDispatch(unsigned long through) {
    return Dispatch("BattleExtra_EquipLeaveDispatch", at::kEquipLeaveSteps, B(at::kModeSub), 2, through);
}

// original 0x42E050 (0x64AE00[0]): the backdrop; 0x929F04 - 1, and at 0
// Transition_Start(2) and 0x929F02 + 1 (read after the call).
extern "C" void __cdecl BattleExtra_EquipFadeOut(void) {
    Backdrop();
    const auto timer = static_cast<unsigned char>(B(at::kModeTimer) - 1);
    B(at::kModeTimer) = timer;
    if (timer != 0) return;
    BH_CALL(Transition_Start)(2);
    B(at::kModeSub) = static_cast<unsigned char>(B(at::kModeSub) + 1);
}

// original 0x42E090 (0x64AE00[1]): once no transition runs, 0x929F04 = 0,
// state 1, steps 0, and the five bytes 0x669CE0 over character record 7's
// first five (DIV-0020 writes that slot; its check reads this function's
// `push 0x669CE0` at 0x42E09D, which the inject's jmp at 0x42E090 leaves in
// place); else the backdrop.
extern "C" void __cdecl BattleExtra_EquipLeave(void) {
    if (Word(At(at::kWaitWord)) == 0) {
        B(at::kModeTimer) = 0;
        B(at::kMode) = 1;
        B(at::kModeStep) = 0;
        B(at::kModeSub) = 0;
        Memcpy(at::kGuest, at::kGuestHeadOut, 5);
        return;
    }
    Backdrop();
}

// ============================================================================
// The command menu: the held-button steps and auto battle
// ============================================================================

// original 0x42ED90 (Battle_InputSteps[3]): by 0x904AA2 through
// BattleHold_Steps 0x64AE48 (3).
extern "C" unsigned long __cdecl BattleHold_Dispatch(unsigned long through) {
    return Dispatch("BattleHold_Dispatch", at::kHoldSteps, B(at::kSubStep), 3, through);
}

// original 0x42EDA0 (BattleHold_Steps[0]): the menu member's cross size
// 0x904ABC[0x904AB4] down by 2 while above 0, else 0x904AA2 + 1; then its
// command label, the party status at window 1's (x, y) and the command cross
// at window 2's. The original pushes the words in whole registers
// whose upper halves are left over (the callees keep the low halves: the
// fuzz compares those); ours passes the words.
extern "C" void __cdecl BattleHold_Shrink(void) {
    unsigned char& size = B(at::kCrossGrow + B(at::kMenuMember));
    if (size > 0)
        size = static_cast<unsigned char>(size - 2);
    else
        B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) + 1);
    BH_CALL(BattleWin_DrawCommandLabel)(B(at::kMenuMember));
    BH_CALL(BattleWin_DrawPartyStatus)(Word(At(at::kWin1 + 4)), Word(At(at::kWin1 + 6)));
    BH_CALL(BattleWin_DrawCommandCross)(Word(At(at::kWin2 + 4)), Word(At(at::kWin2 + 6)));
}

// original 0x42EE00 (BattleHold_Steps[1]): while Input_Held has no 0x100,
// windows 2, 1 and 3's +3 = 1 and 0x904AA2 + 1; then the party status,
// BE4's 0x444660(), and all seven command labels with their icons
// (Menu_DrawIcon(i, 0x88 + 0x64E2AC[2i], 0x58 + 0x64E2AD[2i], 0x10, 0x10,
// 0x80)).
extern "C" void __cdecl BattleHold_ShowAll(void) {
    if (!(Word(At(at::kInputHeld)) & 0x100)) {
        B(at::kWin2 + 3) = 1;
        B(at::kWin1 + 3) = 1;
        B(at::kWin3 + 3) = 1;
        B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) + 1);
    }
    BH_CALL(BattleWin_DrawPartyStatus)(Word(At(at::kWin1 + 4)), Word(At(at::kWin1 + 6)));
    BH_AT(void (__cdecl*)(), at::kBe4Draw444660)();
    for (unsigned i = 0; i < 7; ++i) {
        BH_CALL(BattleWin_DrawCommandLabel)(i);
        BH_CALL(Menu_DrawIcon)(i, B(at::kHoldIcons + 2 * i) + 0x88u, B(at::kHoldIcons + 2 * i + 1) + 0x58u, 0x10, 0x10, 0x80);
    }
}

// original 0x42EEA0 (BattleHold_Steps[2]): the menu member's cross size up by
// 4 while below 8, else 0x904AA1 = 2 (BattleMenu_CommandSelect) and
// 0x904AA2 = 0.
extern "C" void __cdecl BattleHold_Regrow(void) {
    unsigned char& size = B(at::kCrossGrow + B(at::kMenuMember));
    if (size < 8) {
        size = static_cast<unsigned char>(size + 4);
        return;
    }
    B(at::kStep) = 2;
    B(at::kSubStep) = 0;
}

// original 0x42EF50 (Battle_MenuSteps[5]; PSX Cmd_AutoBattle_Begin
// 0x801D2598 by the sibling's names/functions.toml, read and agreeing):
// window 4's +3 = 2; for each of the s8 0x904AC3 entries, the FIRST entry
// 0x904AB6 every time (the index is never added - docs/battle_e1.md section
// 7): not 0xFF and not out, and its command +0x125 is 5 (an item), BE4's
// 0x446D90(+0x12E, word +0x126) gives the item back; then each member not
// out (0x904AB0 of them, re-read) - with 0x904B8E set only one whose +0x134
// has 0x10 - goes to state 2 (+1); round flags |= 0x10, LoadDatFile(0xD1),
// 0x904AA1 = 0x904AA2 = 0.
extern "C" void __cdecl Cmd_AutoBattle(void) {
    const auto entries = static_cast<signed char>(B(at::kEntryCount));
    B(at::kWin4 + 3) = 2;
    if (entries > 0) {
        unsigned i = 0;
        do {
            const unsigned char actor = B(at::kEntryFirst);
            if (actor != 0xFF && !BH_CALL(Battle_ActorIsOut)(actor)) {
                unsigned char* const m = Member(actor);
                if (m[0x125] == 5)
                    BH_AT(unsigned char (__cdecl*)(unsigned, unsigned), at::kBe4ReturnItem)(m[0x12E], Word(m + 0x126));
            }
            i = (i + 1) & 0xFF;
        } while (static_cast<int>(i) < static_cast<signed char>(B(at::kEntryCount)));
    }
    if (B(at::kPartyCount) != 0) {
        unsigned char i = 0;
        do {
            if (!BH_CALL(Battle_ActorIsOut)(i)) {
                if (B(at::kAutoTest) == 0 || (Member(i)[0x134] & 0x10)) Member(i)[1] = 2;
            }
            ++i;
        } while (i < B(at::kPartyCount));
    }
    B(at::kFlags) = static_cast<unsigned char>(B(at::kFlags) | 0x10);
    BH_CALL(LoadDatFile)(0xD1);
    B(at::kStep) = 0;
    B(at::kSubStep) = 0;
}

// ============================================================================
// Action kind 3, the random pick, the ability notice
// ============================================================================

// original 0x42F5E0 (BattleAction_KindSteps[3]): by 0x904AA3 through
// BattleAction_Kind3Steps 0x64AEB4 (5: the two below, then
// BattleAction_AbilitySteps' three).
extern "C" unsigned long __cdecl BattleAction_Kind3Dispatch(unsigned long through) {
    return Dispatch("BattleAction_Kind3Dispatch", at::kKind3Steps, B(at::kSubStep2), 5, through);
}

// original 0x42F5F0 (Kind3Steps[0]): Battle_ClearActingFlags;
// BattleBanner_Add(1, 0, 0, 0x1E, the string 0x669E08 points at); window 4's
// +3 = 1; Battle_SetActorBit(0x904B34); the acting sprite's +1 = 9;
// 0x904AA3 + 1.
extern "C" void __cdecl BattleAction_Kind3Banner(void) {
    BH_CALL(Battle_ClearActingFlags)();
    BH_CALL(BattleBanner_Add)(1, 0, 0, 0x1E, reinterpret_cast<const char*>(PtrAt(at::kBannerText)));
    const unsigned char actor = B(at::kActor);
    B(at::kWin4 + 3) = 1;
    BH_CALL(Battle_SetActorBit)(actor);
    PtrAt(at::kActing)[1] = 9;
    B(at::kSubStep2) = static_cast<unsigned char>(B(at::kSubStep2) + 1);
}

// original 0x42F640 (Kind3Steps[1]): once the word 0x904B82 is 0, round
// flags |= 4, 0x904AA1 = 3, 0x904AA2 = 0x904AA3 = 0.
extern "C" void __cdecl BattleAction_Kind3Wait(void) {
    if (Word(At(at::kEffectBits)) != 0) return;
    B(at::kFlags) = static_cast<unsigned char>(B(at::kFlags) | 4);
    B(at::kStep) = 3;
    B(at::kSubStep) = 0;
    B(at::kSubStep2) = 0;
}

// original 0x42F9D0: for the action word 0x904B80 0x24, an id from
// 0x64AEC8[Rand() & 0x1F]; for 0x25 or 0x8C (the word re-read after that
// Rand), from 0x64AEE8[Rand() & 0x1F]; any other gives 0. The id to
// 0x904B80 and the action record's (0x904B40, read first) +2; then by the
// id's flags byte (NameTable_Abilities + 24 id): 0x10 - 0xC0 for 0x80
// without 0x40, else a side by the actor (party: 0x40 with 0x20, 0x80
// without; enemy: 0x80 with, 0x40 without); no 0x10 and no 0x40 - the actor
// 0x904B34; 0x40 - 0x452F10() or 0x452EB0() by the actor's side and 0x20.
// al out.
extern "C" unsigned char __cdecl BattleAction_PickRandomAbility(void) {
    unsigned char id = 0;
    unsigned action = Word(At(at::kActionId));
    if (action == 0x24) {
        id = B(at::kRandomIds24 + (static_cast<U>(BH_CALL(Rand)()) & 0x1F));
        action = Word(At(at::kActionId));
    }
    if (action == 0x25 || action == 0x8C) id = B(at::kRandomIds25 + (static_cast<U>(BH_CALL(Rand)()) & 0x1F));
    unsigned char* const record = PtrAt(at::kActing2);
    SetWord(At(at::kActionId), id);
    const unsigned char flags = B(bof3::addr::NameTable_Abilities + 24u * id);
    SetWord(record + 2, id);
    const bool party = B(at::kActor) <= 2;
    if (flags & 0x10) {
        if ((flags & 0x80) && !(flags & 0x40)) return 0xC0;
        if (party) return (flags & 0x20) ? 0x40 : 0x80;
        return (flags & 0x20) ? 0x80 : 0x40;
    }
    if (!(flags & 0x40)) return B(at::kActor);
    const bool second = party ? (flags & 0x20) != 0 : (flags & 0x20) == 0;
    return BH_AT(unsigned char (__cdecl*)(), second ? at::kSideTargetB : at::kSideTargetA)();
}

// original 0x42FE20 (BattleAction_AfterSteps[1]): nothing while the message
// window is up (0x939F60); else from the party slot after 0x904AA5 (stored
// as it goes; none left past slot 2: 0x904AA2 + 1) the first member whose
// +0x130 has bit 3: the bit cleared, and by the action's ability (the
// record 0x904B40's word +2) flag 0x200 in Ability_Records +0x14 - BE4's
// 0x44A910(member) (the name into Text_Records), Text_Records + 0x20 zeroed
// (32 bytes) and the acting enemy's ability record (+0x106 of enemy
// 0x904B34 - 3, unchecked) copied in (16 bytes), BattleQueue_Push(1, 0,
// message 0x24); without it, BattleQueue_Push(1, 0, message 0x23).
extern "C" void __cdecl BattleAction_AbilityNotice(void) {
    if (B(at::kMessageUp) != 0) return;
    unsigned char slot = B(at::kMemberAt);
    if (slot <= 2) {
        const unsigned char* const record = PtrAt(at::kActing2);
        do {
            ++slot;
            B(at::kMemberAt) = slot;
            const U id = Word(record + 2);
            const bool learns = (Word(At(0x65C4DC + 24 * id)) & 0x200) != 0;
            unsigned char* const flags = At(0x802D24 + slot * at::kMemberSize);
            if (!(flags[0] & 8)) continue;
            SetLong(flags, static_cast<std::int32_t>(static_cast<U>(Long(flags)) & ~8u));
            if (!learns) {
                BH_CALL(BattleQueue_Push)(1, 0, static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(BH_CALL(Msg_SystemPtr)(0x23))));
                return;
            }
            BH_AT(void (__cdecl*)(unsigned), at::kBe4MemberName)(slot - 1u);
            std::memset(At(bof3::addr::Text_Records + 0x20), 0, 32);
            const U enemy = static_cast<U>(B(at::kActor)) - 3;
            const U ability = Word(At(0x93BA66 + enemy * 0x128));
            const U from = bof3::addr::Ability_Records + 24 * ability;
            SetLong(At(bof3::addr::Text_Records + 0x20), Long(At(from)));
            SetLong(At(bof3::addr::Text_Records + 0x24), Long(At(from + 4)));
            SetLong(At(bof3::addr::Text_Records + 0x28), Long(At(from + 8)));
            SetLong(At(bof3::addr::Text_Records + 0x2C), Long(At(from + 0xC)));
            BH_CALL(BattleQueue_Push)(1, 0, static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(BH_CALL(Msg_SystemPtr)(0x24))));
            return;
        } while (slot <= 2);
    }
    B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) + 1);
}

// ============================================================================
// The end and the result
// ============================================================================

// original 0x4315C0 (0x64AF7C[2], the third way out's steps by 0x904AA2):
// nothing while a member (of 0x904AB0, read once) has 0x2000 in its +0x130;
// back a step (0x904AA2 - 1) while 0x904AA5 is short of the count; then
// Port_DroppedCall(0); each member Battle_ClearStatus(i, word +0x90 &
// 0xBF5F) and Sprite_ReleaseTint; each member with +0 bit 0, as
// Sprite_Current, Sprite_AnimFromSet(+0x4B, word +0x58 - 2, 0x8C5D80,
// 0x1800) - the count re-read after each of those calls only -;
// PartySet_Select(0x90412C & 0x7F, 0); 0x904AA2 + 1.
extern "C" void __cdecl BattleEnd_AwaitRestore(void) {
    const unsigned char n = B(at::kPartyCount);
    for (unsigned char i = 0; i < n; ++i)
        if (MemberFlags(i) & 0x2000) return;
    if (B(at::kMemberAt) != n) {
        B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) - 1);
        return;
    }
    BH_CALL(Port_DroppedCall)(0);
    unsigned char count = B(at::kPartyCount);
    for (unsigned char i = 0; i < count;) {
        unsigned char* const m = Member(i);
        BH_CALL(Battle_ClearStatus)(i, Word(m + 0x90) & 0xBF5Fu);
        BH_CALL(Sprite_ReleaseTint)(m);
        count = B(at::kPartyCount);
        ++i;
    }
    for (unsigned char i = 0; i < count;) {
        unsigned char* const m = Member(i);
        if (m[0] & 1) {
            Sprite_Current = m;
            BH_CALL(Sprite_AnimFromSet)(m[0x4B], static_cast<std::uint16_t>(Word(m + 0x58) - 2), At(at::kAnimBuffer), 0x1800);
            count = B(at::kPartyCount);
        }
        ++i;
    }
    BH_CALL(PartySet_Select)(B(at::kPartySetByte) & 0x7Fu, 0);
    B(at::kSubStep) = static_cast<unsigned char>(B(at::kSubStep) + 1);
}

// original 0x4319B0: of the 0x904AB0 party slots (re-read each pass), those
// Battle_ActorIsOut answers 0 for whose +0x134 lacks 0x400: the count, al.
// BattleResult_SplitExp's divisor.
extern "C" unsigned char __cdecl BattleResult_CountExpShares(void) {
    unsigned char count = 0;
    if (B(at::kPartyCount) == 0) return 0;
    unsigned char i = 0;
    do {
        if (!BH_CALL(Battle_ActorIsOut)(i) && !(Long(Member(i) + 0x134) & 0x400)) ++count;
        ++i;
    } while (i < B(at::kPartyCount));
    return count;
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
    BH_AT(void (__cdecl*)(unsigned), at::kBe4MemberName)(static_cast<unsigned char>(B(at::kMemberAt) - 1));
    const unsigned char roster = B(at::kRoster);
    SetWord(At(at::kOldLevel), B(at::kCharRecords + roster * at::kCharSize + 0xA));
    const unsigned gain = BH_CALL(Char_LevelUpGain)(roster, 0);
    const unsigned char again = B(at::kRoster);
    SetWord(At(at::kNewLevel), (gain & 0xFFFF) + B(at::kCharRecords + again * at::kCharSize + 0xA));
    BH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(bof3::addr::Text_Records + 0x20)),
                         reinterpret_cast<const char*>(At(bof3::addr::Area08_MessageFormat)), Word(At(at::kNewLevel)));
    SetLong(At(at::kLevelMsgPtr), static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(BH_CALL(Msg_SystemPtr)(7))));
    U rows = 0;
    for (unsigned k = 1; k <= 6; ++k)
        if ((BH_CALL(Char_LevelUpGain)(B(at::kRoster), k) & 0xFFFF) != 0) rows += 0x10;
    const U row = at::kCharExpTable + (B(at::kRoster) * 99u + Word(At(at::kOldLevel))) * 8;
    if (B(row + 6) != 0) rows += 0x10;
    if (B(row + 7) != 0) rows += 0x10;
    BH_CALL(Window_Alloc)(1, 4);
    const unsigned char who = B(at::kRoster);
    const unsigned char old = B(at::kOldLevel);
    B(at::kWin1 + 0xA) = who;
    const unsigned char step = B(at::kSubStep3);
    B(at::kWin1 + 2) = 0;
    B(at::kWin1 + 3) = 0;
    B(at::kWin1 + 9) = static_cast<unsigned char>(rows + 4);
    B(at::kWin1 + 0xB) = old;
    B(at::kSubStep3) = static_cast<unsigned char>(step - 1);
}

// original 0x431FE0: 1 when a party slot (of 0x904AB0, re-read) that
// Battle_ActorIsOut answers 0 for holds 7 in its +0x96 or +0x97; else 0. al.
extern "C" unsigned char __cdecl BattleResult_ZennyBonus(void) {
    if (B(at::kPartyCount) == 0) return 0;
    unsigned char i = 0;
    do {
        if (!BH_CALL(Battle_ActorIsOut)(i)) {
            const unsigned char* const m = Member(i);
            if (m[0x96] == 7 || m[0x97] == 7) return 1;
        }
        ++i;
    } while (i < B(at::kPartyCount));
    return 0;
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
    const unsigned char* const record = At(at::kCharRecords + roster * at::kCharSize);
    const unsigned char level = record[0xA];
    unsigned char reached = 1;
    U sum = 0;
    for (;;) {
        sum += Word(At(at::kCharExpTable + (base + reached) * 8));
        if (static_cast<std::int32_t>(sum) > Long(record + 0xC)) break;
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
        const unsigned char b = B(at::kCharExpTable + (base + lv) * 8 + g.byte);
        const unsigned v = g.nibble == 1 ? b >> 4 : g.nibble == 2 ? b & 0xF : b;
        total = static_cast<std::uint16_t>(total + v + bonus);
    }
    return static_cast<std::int16_t>(total) < 0 ? 0 : total;
}

// ============================================================================
// The loss screen (0x904AE8 bit 0's way out: BattleEnd_Steps[2], its table
// 0x64AF70 by 0x904AA2, entry 2)
// ============================================================================

// original 0x432430 (0x64AF70[2]): by 0x904AA3 through BattleLoss_Steps
// 0x64AFD8 (5).
extern "C" unsigned long __cdecl BattleLoss_Dispatch(unsigned long through) {
    return Dispatch("BattleLoss_Dispatch", at::kLossSteps, B(at::kSubStep2), 5, through);
}

// original 0x432440 (BattleLoss_Steps[0]): Transition_Start(0x13),
// Gfx_ClutStripCopyRow(0x1A), 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_FadeOut(void) {
    BH_CALL(Transition_Start)(0x13);
    BH_CALL(Gfx_ClutStripCopyRow)(0x1A);
    B(at::kSubStep2) = static_cast<unsigned char>(B(at::kSubStep2) + 1);
}

// The loss screen's member reset: Sprite_Current the member,
// Sprite_EnsureAnimation(+8 + 0x1C), +0x29 = 1, +0x24 |= 0x80 (read after
// the call), +0x2E / +0x30 the words of BattleLoss_Steps' neighbour rows
// (0x64AFD8 + 24 * the count, re-read, + 8 * the member) - data read in
// place, past the five code pointers -, Sprite_ReleaseTint(member).
namespace {
void LossMember(U index) {
    unsigned char* const m = Member(index);
    Sprite_Current = m;
    BH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(m[8] + 0x1C));
    const auto flags = static_cast<unsigned char>(m[0x24] | 0x80);
    m[0x29] = 1;
    m[0x24] = flags;
    const U row = at::kLossSteps + 24 * static_cast<U>(B(at::kPartyCount)) + 8 * index;
    SetWord(m + 0x2E, Word(At(row)));
    SetWord(m + 0x30, Word(At(row + 4)));
    BH_CALL(Sprite_ReleaseTint)(m);
}
}  // namespace

// original 0x432460 (BattleLoss_Steps[1]): once no transition runs, window
// 0's +0 = 0; members 0, 1, 2 reset (LossMember) while the count (re-read)
// is at least 1, 2, exactly 3; then 0x494E70() (the enemies' state bytes),
// BattleTask_ClearAll, Transition_Start(0xE), 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_ResetParty(void) {
    if (Word(At(at::kWaitWord)) != 0) return;
    const unsigned char n = B(at::kPartyCount);
    B(at::kWin0) = 0;
    if (n >= 1) {
        LossMember(0);
        if (B(at::kPartyCount) >= 2) {
            LossMember(1);
            if (B(at::kPartyCount) == 3) LossMember(2);
        }
    }
    BH_AT(void (__cdecl*)(), at::kEnemiesClear)();
    BH_CALL(BattleTask_ClearAll)();
    BH_CALL(Transition_Start)(0xE);
    B(at::kSubStep2) = static_cast<unsigned char>(B(at::kSubStep2) + 1);
}

// original 0x4325F0 (BattleLoss_Steps[2]): the black screen and the panels
// (shade 0x80); once no transition runs, the bar 0x904B70 = 0x2C and
// 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_Show(void) {
    BH_CALL(BattleLoss_DrawBlack)();
    BH_CALL(BattleLoss_DrawPanels)(0x80);
    if (Word(At(at::kWaitWord)) != 0) return;
    SetWord(At(at::kLossBar), 0x2C);
    B(at::kSubStep2) = static_cast<unsigned char>(B(at::kSubStep2) + 1);
}

// original 0x432630 (BattleLoss_Steps[3]): the black screen, the panels, the
// caption; the bar up by 4 while below 0xF4, drawn; on any held button both
// CLUT runs 0x812980 / 0x811380 greyed, Gfx_ClutStripDirty = 1,
// Transition_Start(0xD), 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_BarGrow(void) {
    BH_CALL(BattleLoss_DrawBlack)();
    BH_CALL(BattleLoss_DrawPanels)(0x80);
    BH_CALL(BattleLoss_DrawCaption)(0x80);
    unsigned width = Word(At(at::kLossBar));
    if (width < 0xF4) {
        width += 4;
        SetWord(At(at::kLossBar), width);
    }
    BH_CALL(BattleLoss_DrawBar)(width);
    if (Word(At(at::kInputHeld)) == 0) return;
    GreyClut(at::kClutA);
    GreyClut(at::kClutB);
    Gfx_ClutStripDirty = 1;
    BH_CALL(Transition_Start)(0xD);
    B(at::kSubStep2) = static_cast<unsigned char>(B(at::kSubStep2) + 1);
}

// original 0x432750 (BattleLoss_Steps[4]): the black screen, the panels, the
// caption; once no transition runs, the three members' tints released,
// Window_ResetAll, 0x904AE9 / 0x904AE5 / the fight byte / the phase and its
// four step bytes 0, 0x494E70(), BattleTask_ClearAll, Sound_StopChannels,
// Task_Restart(Boot_Task) (which does not return in the game).
extern "C" void __cdecl BattleLoss_Restart(void) {
    BH_CALL(BattleLoss_DrawBlack)();
    BH_CALL(BattleLoss_DrawPanels)(0x80);
    BH_CALL(BattleLoss_DrawCaption)(0x80);
    if (Word(At(at::kWaitWord)) != 0) return;
    BH_CALL(Sprite_ReleaseTint)(Member(0));
    BH_CALL(Sprite_ReleaseTint)(Member(1));
    BH_CALL(Sprite_ReleaseTint)(Member(2));
    BH_CALL(Window_ResetAll)();
    B(0x904AE9) = 0;
    B(0x904AE5) = 0;
    B(0x904AAA) = 0;
    B(0x904AA0) = 0;
    B(at::kStep) = 0;
    B(at::kSubStep) = 0;
    B(at::kSubStep2) = 0;
    B(at::kSubStep3) = 0;
    BH_AT(void (__cdecl*)(), at::kEnemiesClear)();
    BH_CALL(BattleTask_ClearAll)();
    BH_CALL(Sound_StopChannels)();
    BH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(bof3::addr::Boot_Task)));
}

// original 0x4327F0: three SPRTs of page (0x340, 0x100) and CLUT (0, 0x1FA)
// in the shade byte: 0x100 x 0xA0 from (0, 0x60) at (16, 60); 0x20 x 0x60
// from (0xC0, 0) at (272, 60); 0x20 x 0x40 from (0xE0, 0) at (272, 156).
extern "C" void __cdecl BattleLoss_DrawPanels(unsigned shade) {
    DrawMode(1, 0, 0x340, 0x100, 2);
    const auto s = static_cast<unsigned char>(shade);
    LossSprite(s, 0x41800000u, 0x42700000u, 0, 0x60, 0x100, 0xA0);
    LossSprite(s, 0x43880000u, 0x42700000u, 0xC0, 0, 0x20, 0x60);
    LossSprite(s, 0x43880000u, 0x431C0000u, 0xE0, 0, 0x20, 0x40);
}

// original 0x432930: page (0x3C0, 0), then a black TILE over (0, 0) ..
// (320, 240), Gfx_CommitPrim(2, 0x1C).
extern "C" void __cdecl BattleLoss_DrawBlack(void) {
    DrawMode(0, 0, 0x3C0, 0, 2);
    unsigned char* const p = Packet();
    BH_CALL(Gpu_SetTile)(p);
    SetLong(p + 8, 0);
    SetLong(p + 0xC, 0);
    SetLong(p + 0x14, 0x43A00000);
    SetLong(p + 0x18, 0x43700000);
    Rgb(p, 4, 0);
    BH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// original 0x4329A0: page (0x340, 0x100), one SPRT 0xA8 x 0x18 from (0, 0)
// at (76, 48) in the shade byte, CLUT (0, 0x1FA).
extern "C" void __cdecl BattleLoss_DrawCaption(unsigned shade) {
    DrawMode(1, 0, 0x340, 0x100, 2);
    LossSprite(static_cast<unsigned char>(shade), 0x42980000u, 0x42400000u, 0, 0, 0xA8, 0x18);
}

// original 0x432A30: the bar of the low word w: page (0x3C0, 0), a black
// TILE at (w + 0x20, 48) of 168 x 24 (Gfx_CommitPrim(1, 0x1C)); page
// (0x3C0, 0) with abr 2, a semi-transparent POLY_G4 (w, 48) .. (w + 0x22,
// 72), black on the left, white on the right (Gfx_CommitPrim(1, 0x44)).
extern "C" void __cdecl BattleLoss_DrawBar(unsigned width) {
    const auto w = static_cast<std::int32_t>(width & 0xFFFF);
    DrawMode(0, 0, 0x3C0, 0, 1);
    unsigned char* p = Packet();
    BH_CALL(Gpu_SetTile)(p);
    SetLong(p + 0xC, 0x42400000);
    SetLong(p + 0x14, 0x43280000);
    SetLong(p + 0x18, 0x41C00000);
    PutFloat(p + 8, w + 0x20);
    Rgb(p, 4, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
    DrawMode(0, 2, 0x3C0, 0, 1);
    p = Packet();
    BH_CALL(Gpu_SetPolyG4)(p);
    Rgb(p, 4, 0);
    PutFloat(p + 8, w);
    Rgb(p, 0x14, 0xFF);
    Rgb(p, 0x24, 0);
    Rgb(p, 0x34, 0xFF);
    SetLong(p + 0xC, 0x42400000);
    PutFloat(p + 0x18, w + 0x22);
    PutFloat(p + 0x28, w);
    SetLong(p + 0x1C, 0x42400000);
    SetLong(p + 0x2C, 0x42900000);
    PutFloat(p + 0x38, w + 0x22);
    SetLong(p + 0x3C, 0x42900000);
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    BH_CALL(Gfx_CommitPrim)(1, 0x44);
}

// ============================================================================

void BattleE1_Inject() {
    if (bof3::WantsShadow("battle_e1")) battle_e1::SelfTest();
    BOF3_INJECT(BattleExtra_TallyOpen);
    BOF3_INJECT(BattleExtra_TallyCount);
    BOF3_INJECT(BattleExtra_TallyClose);
    BOF3_INJECT(BattleExtra_DrawTally);
    BOF3_INJECT(BattleExtra_ApplyTally);
    BOF3_INJECT(BattleExtra_DrawPlus);
    BOF3_INJECT(BattleExtra_EquipDispatch);
    BOF3_INJECT(BattleExtra_EquipOpenDispatch);
    BOF3_INJECT(BattleExtra_EquipOpen);
    BOF3_INJECT(BattleExtra_EquipFadeIn);
    BOF3_INJECT(BattleExtra_EquipRun);
    BOF3_INJECT(BattleExtra_EquipSlotInput);
    BOF3_INJECT(BattleExtra_EquipListInput);
    BOF3_INJECT(BattleExtra_EquipLeaveDispatch);
    BOF3_INJECT(BattleExtra_EquipFadeOut);
    BOF3_INJECT(BattleExtra_EquipLeave);
    BOF3_INJECT(BattleHold_Dispatch);
    BOF3_INJECT(BattleHold_Shrink);
    BOF3_INJECT(BattleHold_ShowAll);
    BOF3_INJECT(BattleHold_Regrow);
    BOF3_INJECT(Cmd_AutoBattle);
    BOF3_INJECT(BattleAction_Kind3Dispatch);
    BOF3_INJECT(BattleAction_Kind3Banner);
    BOF3_INJECT(BattleAction_Kind3Wait);
    BOF3_INJECT(BattleAction_PickRandomAbility);
    BOF3_INJECT(BattleAction_AbilityNotice);
    BOF3_INJECT(BattleEnd_AwaitRestore);
    BOF3_INJECT(BattleResult_CountExpShares);
    BOF3_INJECT(BattleResult_LevelUpNotice);
    BOF3_INJECT(BattleResult_ZennyBonus);
    BOF3_INJECT(Char_LevelUpGain);
    BOF3_INJECT(BattleLoss_Dispatch);
    BOF3_INJECT(BattleLoss_FadeOut);
    BOF3_INJECT(BattleLoss_ResetParty);
    BOF3_INJECT(BattleLoss_Show);
    BOF3_INJECT(BattleLoss_BarGrow);
    BOF3_INJECT(BattleLoss_Restart);
    BOF3_INJECT(BattleLoss_DrawPanels);
    BOF3_INJECT(BattleLoss_DrawBlack);
    BOF3_INJECT(BattleLoss_DrawCaption);
    BOF3_INJECT(BattleLoss_DrawBar);
}
