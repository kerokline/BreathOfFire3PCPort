// Moved here by tools/move_funcs.py from battle_e1.cpp, rest_3a.cpp (docs/source-layout-round.md).
#include "game/battle/battle_extra.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle/battle_extra_callees.h"
#include "game/battle_e1.h"
#include "game/battle_e1_callees.h"
#include "game/boss_harness.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3a.h"
#include "game/rest_3a_callees.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_extra::at;

}  // namespace

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using U = std::uint32_t;

using boss_harness::Handler;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& BBattleE1(U address) { return move_script::At(address)[0]; }

U LBattleE1(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

unsigned char* Packet() { return Gfx_PacketNext; }

unsigned short* Stat(U address) { return reinterpret_cast<unsigned short*>(At(address)); }

unsigned Pressed() { return Input_Pressed; }

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
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(LBattleE1(table + 4 * index)))(through);
}

// The CRT's strncpy (Crt_strncpy, ours since crt_rest: a NUL-stopping, zero-padding copy).
void Memcpy(U dst, U src, unsigned n) {
    BH_CALL(Crt_strncpy)(reinterpret_cast<char*>(At(dst)), reinterpret_cast<const char*>(At(src)), n);
}

void Backdrop() { BH_CALL(Menu_DrawBackdrop)(Config_Background); }

}  // namespace

// Capcom's copy of five bytes over character record 7's name, or - DIV-0118's
// amendment for D280 - the name and its NUL when they are longer (a ja-JP
// layer's names, two bytes a character), at most the nine-byte field. Every
// name of the PC's and of a Latin layer is at most five with its NUL, so the
// count stays five for them.
static U NameCopy(U name) {
    const std::size_t len = std::strlen(reinterpret_cast<const char*>(static_cast<std::uintptr_t>(name)));
    if (len + 1 > 9) bof3::Fatal("BattleExtra: the name copied over character 7's is %u bytes, the field holds 9",
                                 (unsigned)len + 1);
    return len + 1 > 5 ? static_cast<U>(len + 1) : 5u;
}

namespace {

unsigned char& B(U address) { return move_script::At(address)[0]; }

U LRest3a(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

unsigned long DispatchRest3a(const char* who, U table, unsigned index, unsigned entries, unsigned long through) {
    if (index >= entries)
        bof3::Fatal("%s: the step byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3a.md section 7)",
                    who, index, entries, (unsigned)::bof3::VaOf(table));
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(LRest3a(table + 4 * index)))(through);
}

void W16(U address, unsigned v) { SetWord(At(address), v); }

}  // namespace

// original 0x42D710 (Mode8_Step5's call): jmp [BattleExtra_States + 4 *
// (dword 0x929F00 & 0xFF)], 4 entries (the run of code pointers to
// BattleExtra_TallySteps; ours aborts past them).
extern "C" unsigned long __cdecl BattleExtra_Dispatch(unsigned long through) {
    return DispatchRest3a("BattleExtra_Dispatch", ::bof3::Img(BattleExtra_States_addr), LRest3a(::bof3::Img(at::kMode.v)) & 0xFF, 4, through);
}

// original 0x42D730 (BattleExtra_States[0]): the state 0x904C9F + 2, the step
// and the sub-step 0.
extern "C" void __cdecl BattleExtra_Start(void) {
    const auto state = static_cast<unsigned char>(ScenaSc6_Byte904C9F + 2);
    BattleE1_Mode = state;
    FieldS_State = 0;
    FieldS_Step = 0;
}

// original 0x42D750 (BattleExtra_States[1]): Window_ResetAll, Game_Step + 1.
extern "C" void __cdecl BattleExtra_Leave(void) {
    BH_CALL(Window_ResetAll)();
    Game_Step = Game_Step + 1;
}

// original 0x42D760 (BattleExtra_States[3]): jmp [BattleExtra_TallySteps + 4
// * byte 0x929F01], 3 (BattleExtra_TallyOpenDispatch, BattleExtra_TallyCount,
// BattleExtra_TallyClose).
extern "C" unsigned long __cdecl BattleExtra_TallyDispatch(unsigned long through) {
    return DispatchRest3a("BattleExtra_TallyDispatch", ::bof3::Img(BattleExtra_TallySteps_addr), FieldS_State, 3, through);
}

// original 0x42D770 (BattleExtra_TallySteps[0]): jmp [BattleExtra_TallyOpenSteps
// + 4 * byte 0x929F02], 2 (BattleExtra_OpenTransition, BattleExtra_TallyOpen).
extern "C" unsigned long __cdecl BattleExtra_TallyOpenDispatch(unsigned long through) {
    return DispatchRest3a("BattleExtra_TallyOpenDispatch", ::bof3::Img(BattleExtra_TallyOpenSteps_addr), FieldS_Step, 2, through);
}

// original 0x42D780 (BattleExtra_TallyOpenSteps[0], BattleExtra_EquipOpenSteps[0]):
// Transition_Start(3), the sub-step + 1 (read after the call).
extern "C" void __cdecl BattleExtra_OpenTransition(void) {
    BH_CALL(Transition_Start)(3);
    FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x42D7A0 (0x64ADC8[1]): the backdrop and the tally at (0x37,
// 0x37); once no transition runs, the tally's counts set up and applied,
// 0x929F08 = 0, 0x929F02 = 0, 0x929F01 + 1 (read after the call).
extern "C" void __cdecl BattleExtra_TallyOpen(void) {
    Backdrop();
    BH_CALL(BattleExtra_DrawTally)(0x37, 0x37);
    if (Transition_Busy != 0) return;
    BH_CALL(BattleExtra_ApplyTally)();
    const unsigned char step = FieldS_State;
    FieldS_Column = 0;
    FieldS_Step = 0;
    FieldS_State = static_cast<unsigned char>(step + 1);
}

// original 0x42D7F0 (0x64ADBC[1]): the backdrop and the tally; the counter
// 0x929F08 (signed; unchecked below 0 - docs/battle_e1.md section 7) counted
// up by its step, or set to its target when it gets there or a button is
// pressed (the low byte of Input_Pressed), then the next; after the sixth, a
// press starts Transition_Start(2) and 0x929F01 + 1.
extern "C" void __cdecl BattleExtra_TallyCount(void) {
    Backdrop();
    BH_CALL(BattleExtra_DrawTally)(0x37, 0x37);
    const auto row = FieldS_Column;
    if (row >= 6) {
        if (BBattleE1(::bof3::Img(at::kInputPressed.v)) == 0) return;
        BH_CALL(Transition_Start)(2);
        FieldS_State = static_cast<unsigned char>(FieldS_State + 1);
        return;
    }
    const U shown_at = ::bof3::Img(at::kTallyShown.v) + 4 * static_cast<U>(static_cast<std::int32_t>(row));
    const U shown = LBattleE1(shown_at) + BBattleE1(::bof3::Img(at::kTallyStep.v) + static_cast<U>(static_cast<std::int32_t>(row)));
    SetLong(At(shown_at), static_cast<std::int32_t>(shown));
    const U target = LBattleE1(::bof3::Img(at::kTallyTarget.v) + 4 * static_cast<U>(static_cast<std::int32_t>(row)));
    if (shown < target && BBattleE1(::bof3::Img(at::kInputPressed.v)) == 0) return;
    SetLong(At(shown_at), static_cast<std::int32_t>(target));
    FieldS_Column = static_cast<unsigned char>(row + 1);
}

// original 0x42D880 (0x64ADBC[2]): once no transition runs, 0x929F04 = 0,
// state 1 (0x42D750: Window_ResetAll, Game_Step + 1), steps 0; else the
// backdrop and the tally.
extern "C" void __cdecl BattleExtra_TallyClose(void) {
    if (Transition_Busy == 0) {
        BBattleE1(::bof3::Img(at::kModeTimer.v)) = 0;
        BattleE1_Mode = 1;
        FieldS_State = 0;
        FieldS_Step = 0;
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
    BH_CALL(Menu_DrawBox)(I(x - 4), I(y - 8), 0xDC, 0x8C, 0xF0, static_cast<unsigned char>(Config_WindowColour));
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
        char* const text = reinterpret_cast<char*>(At(::bof3::Img(at::kTallyText.v)));
        const unsigned char* const utext = At(::bof3::Img(at::kTallyText.v));
        BH_CALL(Crt_sprintf)(text, reinterpret_cast<const char*>(At(::bof3::Img(at::kTallyFmtWide.v))), LBattleE1(::bof3::Img(at::kTallyShown.v) + 4 * i));
        BH_CALL(Text_DrawFont12)(I(x + 6), I(row + 0x2B), 0, utext);
        BH_CALL(Crt_sprintf)(text, reinterpret_cast<const char*>(At(::bof3::Img(at::kTallyFmtNarrow.v))), LBattleE1(::bof3::Img(at::kTallyShown.v) + 0xC + 4 * i));
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
    const U sum_a = BattleE1_SumA;
    for (U k = 0; k < 6; ++k) SetLong(At(::bof3::Img(at::kTallyShown.v) + 4 * k), 0);
    const U count = BattleE1_Count;
    SetLong(At(::bof3::Img(at::kTallyTarget.v) + 4), static_cast<std::int32_t>(sum_a));
    SetLong(At(::bof3::Img(at::kTallyTarget.v)), static_cast<std::int32_t>(count));
    const U sum_b = BattleE1_SumB;
    SetLong(At(::bof3::Img(at::kTallyTarget.v) + 8), static_cast<std::int32_t>(sum_b));
    SetLong(At(::bof3::Img(at::kTallyTarget.v) + 0xC), static_cast<std::int32_t>(count));
    SetLong(At(::bof3::Img(at::kTallyTarget.v) + 0x10), static_cast<std::int32_t>(sum_a / 20));
    SetLong(At(::bof3::Img(at::kTallyTarget.v) + 0x14), sum_b < 0x32 ? 2 : sum_b < 0x50 ? 4 : 6);
    BH_CALL(Stat_AddClamped)(Stat(::bof3::Img(BattleE1_GuestStat46_addr)), count);
    BH_CALL(Stat_AddClamped)(Stat(::bof3::Img(BossSe_CarryB6_addr)), Word(At(::bof3::Img(at::kTallyTarget.v) + 0xC)));
    BH_CALL(Stat_AddCap999)(Stat(::bof3::Img(BattleE1_GuestHpBase_addr)), Word(At(::bof3::Img(at::kTallyTarget.v) + 0x10)));
    BH_CALL(Stat_AddCap999)(Stat(::bof3::Img(at::kGuestHpMax.v)), Word(At(::bof3::Img(at::kTallyTarget.v) + 0x10)));
    BH_CALL(Stat_AddClamped)(Stat(::bof3::Img(BattleE1_GuestStat44_addr)), Word(At(::bof3::Img(at::kTallyTarget.v) + 0x14)));
    BH_CALL(Stat_AddClamped)(Stat(::bof3::Img(BossSe_CarryB4_addr)), Word(At(::bof3::Img(at::kTallyTarget.v) + 0x14)));
}

// original 0x42DB40: a plus sign of two semi-transparent LINE_F2s in grey
// 0x80 at the s16 (x, y): (x + 5, y) to (x + 5, y + 10), then (x, y + 5) to
// (x + 10, y + 5), each Gfx_CommitPrim(1, 0x20).
extern "C" void __cdecl BattleExtra_DrawPlus(int x_arg, int y_arg) {
    const int x = static_cast<std::int16_t>(x_arg), y = static_cast<std::int16_t>(y_arg);
    unsigned char* p = Packet();
    BH_CALL(Gpu_SetLineF2)(p);
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    rv::Store<std::int32_t>(rv::As<records::GpuLineF2>(p)->x0, x + 5);
    Rgb(p, 4, 0x80);
    rv::Store<std::int32_t>(rv::As<records::GpuLineF2>(p)->y0, y);
    rv::Store<std::int32_t>(rv::As<records::GpuLineF2>(p)->x1, x + 5);
    rv::Store<std::int32_t>(rv::As<records::GpuLineF2>(p)->y1, y + 10);
    BH_CALL(Gfx_CommitPrim)(1, 0x20);
    p = Packet();
    BH_CALL(Gpu_SetLineF2)(p);
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    rv::Store<std::int32_t>(rv::As<records::GpuLineF2>(p)->x0, x);
    Rgb(p, 4, 0x80);
    rv::Store<std::int32_t>(rv::As<records::GpuLineF2>(p)->y0, y + 5);
    rv::Store<std::int32_t>(rv::As<records::GpuLineF2>(p)->x1, x + 10);
    rv::Store<std::int32_t>(rv::As<records::GpuLineF2>(p)->y1, y + 5);
    BH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x42DC00 (0x64ADAC[2], state 2): by 0x929F01 through
// BattleExtra_EquipSteps 0x64ADE0 (3: the opening, the screen, the leaving).
extern "C" unsigned long __cdecl BattleExtra_EquipDispatch(unsigned long through) {
    return Dispatch("BattleExtra_EquipDispatch", ::bof3::Img(BattleExtra_EquipSteps_addr), FieldS_State, 3, through);
}

// original 0x42DC10 (0x64ADE0[0]): by 0x929F02 through 0x64ADEC (3: 0x42D780
// Transition_Start(3) and on, BattleExtra_EquipOpen, BattleExtra_EquipFadeIn).
extern "C" unsigned long __cdecl BattleExtra_EquipOpenDispatch(unsigned long through) {
    return Dispatch("BattleExtra_EquipOpenDispatch", ::bof3::Img(BattleExtra_EquipOpenSteps_addr), FieldS_Step, 3, through);
}

// original 0x42DC20 (0x64ADEC[1]): the backdrop; once no transition runs,
// Sound_PlayEffect(0x102), 0x42E0E0(), 0x929F04 = 4, 0x929F02 + 1 (read after
// the calls), and the five bytes 0x669CE8 copied over character record 7's
// first five.
//
// DIVERGENCE DIV-0118 (DIV-0064's group 17): under a language overlay that
// has written the guest's name, the five bytes come from the overlay's
// string (BATE.EMI's Beyd) instead of 0x669CE8's shipped Chinese name.
// Amended for D280 (NameCopy): a name longer than five bytes with its NUL -
// the Japan disc's, two bytes a character in the ja-JP font - is copied
// whole, its NUL included, at most the record's nine-byte name field.
extern "C" void __cdecl BattleExtra_EquipOpen(void) {
    Backdrop();
    if (Transition_Busy != 0) return;
    BH_CALL(Sound_PlayEffect)(0x102);
    BH_AT(void (__cdecl*)(), at::kEquipOpenHelper.v)();
    const unsigned char sub = FieldS_Step;
    BBattleE1(::bof3::Img(at::kModeTimer.v)) = 4;
    FieldS_Step = static_cast<unsigned char>(sub + 1);
    const U name = Labels_Written(17)
        ? Low32(Labels_Slot(17, 0))
        : ::bof3::Img(at::kGuestHeadIn.v);
    Memcpy(::bof3::Img(CharacterRecords_addr) + 0x47c, name, NameCopy(name));
}

// original 0x42DC80 (0x64ADEC[2]): the backdrop; 0x929F04 - 1, and at 0:
// 0x675EBF = 0, 0x675EC0 = 1, 0x675EBE = 0, 0x929F01 + 1, 0x929F02 = 0,
// 0x929F09 = 0.
extern "C" void __cdecl BattleExtra_EquipFadeIn(void) {
    Backdrop();
    const auto timer = static_cast<unsigned char>(BBattleE1(::bof3::Img(at::kModeTimer.v)) - 1);
    BBattleE1(::bof3::Img(at::kModeTimer.v)) = timer;
    if (timer != 0) return;
    const unsigned char step = FieldS_State;
    BBattleE1(::bof3::Img(at::kEquipOpenBytes.v) + 1) = 0;
    BBattleE1(::bof3::Img(at::kEquipOpenBytes.v) + 2) = 1;
    BattleE1_EquipOpenBytes = 0;
    FieldS_State = static_cast<unsigned char>(step + 1);
    FieldS_Step = 0;
    BattleE1_ModeByte9 = 0;
}

// original 0x42DCD0 (0x64ADE0[1]): `call [0x64ADF8 + 4 * 0x929F02]` (2: the
// slot input, the list input; a Fatal past them), then the backdrop.
extern "C" void __cdecl BattleExtra_EquipRun(void) {
    const unsigned sub = FieldS_Step;
    if (sub >= 2)
        bof3::Fatal("BattleExtra_EquipRun: 0x929F02 is %u, past the 2 entries of 0x64ADF8 - the original calls through "
                    "the dword after (docs/battle_e1.md section 7)",
                    sub);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(BattleExtra_EquipRunSteps[sub]))();
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
    WindowRecords[4].x = LBattleE1(::bof3::Img(at::kWin1.v) + 4) + 6;
    WindowRecords[4].in_use = 1;
    const U cursor = BBattleE1(::bof3::Img(at::kWin1.v) + 0xA);
    WindowRecords[4].y = 13 * cursor + WindowRecords[1].y + 0x57;
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(Pressed() & 0xF000);
    BBattleE1(::bof3::Img(at::kWin1.v) + 0xB) = 0xFF;
    BBattleE1(::bof3::Img(at::kWin1.v) + 0xD) = 1;
    if (repeat & 0x5000) {
        BH_CALL(Sound_PlayEffect)(0x101);
        BBattleE1(::bof3::Img(at::kWin1.v) + 0xA) = static_cast<unsigned char>(BBattleE1(::bof3::Img(at::kWin1.v) + 0xA) ^ 3);
    }
    const unsigned char now = BBattleE1(::bof3::Img(at::kWin1.v) + 0xA);
    BBattleE1(::bof3::Img(at::kWin0.v) + 9) = now;
    const bool second = now == 3;
    const unsigned item = BBattleE1(second ? ::bof3::Img(BattleE1_GuestEquipB_addr) : ::bof3::Img(Rest3a_GuestEquip_addr));
    SetWord(At(::bof3::Img(Window2_addr) + 0x10), BH_CALL(Item_HelpMessage)(second ? 2 : 1, item));
    const unsigned pressed = Pressed();
    SetWord(At(::bof3::Img(WindowRecords_addr) + 0x6c + 0x10), 0xDC);
    if (Field_ConfirmButtons & pressed) {
        BH_CALL(Sound_PlayEffect)(0x103);
        const unsigned char top = BBattleE1(::bof3::Img(at::kWin0.v) + 0xA);
        const unsigned char was = BBattleE1(::bof3::Img(at::kWin1.v) + 0xA);
        BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = top;
        const unsigned char sub = FieldS_Step;
        BBattleE1(::bof3::Img(at::kWin1.v) + 0xB) = was;
        FieldS_Step = static_cast<unsigned char>(sub + 1);
        return;
    }
    if (Field_CancelButtons & pressed) {
        BH_CALL(Sound_PlayEffect)(0x106);
        BH_CALL(Sound_PlayEffect)(0x102);
        const unsigned char step = FieldS_State;
        WindowRecords[0].state = 1;
        BBattleE1(::bof3::Img(at::kWin1.v) + 3) = 1;
        BBattleE1(::bof3::Img(at::kWin2.v) + 3) = 1;
        WindowRecords[3].state = 1;
        WindowRecords[4].in_use = 0;
        WindowRecords[21].in_use = 0;
        BBattleE1(::bof3::Img(at::kModeTimer.v)) = 5;
        FieldS_State = static_cast<unsigned char>(step + 1);
        FieldS_Step = 0;
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
    const U x = LBattleE1(::bof3::Img(at::kWin0.v) + 4) + 7;
    BBattleE1(::bof3::Img(at::kWin1.v) + 0xD) = 0;
    WindowRecords[4].x = x;
    const U scroll = BBattleE1(::bof3::Img(at::kWin0.v) + 0xA), cursor = BBattleE1(::bof3::Img(at::kWin0.v) + 0xB);
    const unsigned count = BBattleE1(::bof3::Img(at::kWin0.v) + 0xD), category = BBattleE1(::bof3::Img(at::kWin0.v) + 8);
    WindowRecords[4].y = 13 * (cursor - scroll + 2) + WindowRecords[0].y;
    SetWord(At(::bof3::Img(Window2_addr) + 0x10), BH_CALL(Item_HelpMessage)(category, count));
    const unsigned pressed = Pressed() & Sigma(0xF00C);
    SetWord(At(::bof3::Img(WindowRecords_addr) + 0x6c + 0x10), 0xDD);
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pressed);
    BH_AT(unsigned char (__cdecl*)(), at::kEquipFrameHelper.v)();
    auto now = BBattleE1(::bof3::Img(at::kWin0.v) + 0xB);
    const unsigned char was = now;
    if (repeat & 0x1000) {
        if (now > 0) BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = --now;
        if (now < BBattleE1(::bof3::Img(at::kWin0.v) + 0xA)) SetWord(At(::bof3::Img(WindowRecords_addr) + 0x10), 0xF0);
    } else if (repeat & 0x4000) {
        if (now < 0x7F) BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = ++now;
        if (!(static_cast<int>(now) < static_cast<int>(BBattleE1(::bof3::Img(at::kWin0.v) + 0xA)) + 9)) SetWord(At(::bof3::Img(WindowRecords_addr) + 0x10), 0x10);
    } else if (repeat & Sigma(4)) {
        auto top = BBattleE1(::bof3::Img(at::kWin0.v) + 0xA);
        if (top == 0) {
            now = 0;
            BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = 0;
        } else if (top < 9) {
            now = static_cast<unsigned char>(now - top);
            BBattleE1(::bof3::Img(at::kWin0.v) + 0xA) = 0;
            BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = now;
        } else {
            now = static_cast<unsigned char>(now - 9);
            top = static_cast<unsigned char>(top - 9);
            BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = now;
            BBattleE1(::bof3::Img(at::kWin0.v) + 0xA) = top;
        }
    } else if (repeat & Sigma(8)) {
        auto top = BBattleE1(::bof3::Img(at::kWin0.v) + 0xA);
        if (top == 0x77) {
            now = 0x7F;
            BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = 0x7F;
        } else if (now > 0x6E) {
            const auto by = static_cast<unsigned char>(0x77 - top);
            BBattleE1(::bof3::Img(at::kWin0.v) + 0xA) = 0x77;
            now = static_cast<unsigned char>(now + by);
            BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = now;
        } else {
            now = static_cast<unsigned char>(now + 9);
            top = static_cast<unsigned char>(top + 9);
            BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = now;
            BBattleE1(::bof3::Img(at::kWin0.v) + 0xA) = top;
        }
    }
    if (was != now) BH_CALL(Sound_PlayEffect)(0x100);
    if (Word(At(::bof3::Img(WindowRecords_addr) + 0x10)) != 0) return;
    const unsigned again = Pressed();
    if (Field_ConfirmButtons & again) {
        if (BBattleE1(::bof3::Img(at::kWin0.v) + 0xD) == 0) {
            BH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        BH_CALL(Sound_PlayEffect)(0x104);
        BH_AT(void (__cdecl*)(), at::kEquipConfirmHelper.v)();
        const unsigned char sub = FieldS_Step;
        BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = 0xFF;
        FieldS_Step = static_cast<unsigned char>(sub - 1);
        return;
    }
    if (Field_CancelButtons & again) {
        BH_CALL(Sound_PlayEffect)(0x106);
        const unsigned char sub = FieldS_Step;
        BBattleE1(::bof3::Img(at::kWin0.v) + 0xB) = 0xFF;
        FieldS_Step = static_cast<unsigned char>(sub - 1);
    }
}

// original 0x42E040 (0x64ADE0[2]): by 0x929F02 through 0x64AE00 (2).
extern "C" unsigned long __cdecl BattleExtra_EquipLeaveDispatch(unsigned long through) {
    return Dispatch("BattleExtra_EquipLeaveDispatch", ::bof3::Img(BattleExtra_EquipLeaveSteps_addr), FieldS_Step, 2, through);
}

// original 0x42E050 (0x64AE00[0]): the backdrop; 0x929F04 - 1, and at 0
// Transition_Start(2) and 0x929F02 + 1 (read after the call).
extern "C" void __cdecl BattleExtra_EquipFadeOut(void) {
    Backdrop();
    const auto timer = static_cast<unsigned char>(BBattleE1(::bof3::Img(at::kModeTimer.v)) - 1);
    BBattleE1(::bof3::Img(at::kModeTimer.v)) = timer;
    if (timer != 0) return;
    BH_CALL(Transition_Start)(2);
    FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x42E090 (0x64AE00[1]): once no transition runs, 0x929F04 = 0,
// state 1, steps 0, and the five bytes 0x669CE0 over character record 7's
// first five (DIV-0020 writes that slot; its check reads this function's
// `push 0x669CE0` at 0x42E09D, which the inject's jmp at 0x42E090 leaves in
// place); else the backdrop.
extern "C" void __cdecl BattleExtra_EquipLeave(void) {
    if (Transition_Busy == 0) {
        BBattleE1(::bof3::Img(at::kModeTimer.v)) = 0;
        BattleE1_Mode = 1;
        FieldS_State = 0;
        FieldS_Step = 0;
        Memcpy(::bof3::Img(CharacterRecords_addr) + 0x47c, ::bof3::Img(BattleE1_GuestHeadOut_addr),
               NameCopy(::bof3::Img(BattleE1_GuestHeadOut_addr)));   // DIV-0020 / DIV-0118, amended for D280
        return;
    }
    Backdrop();
}

// original 0x42E0E0 (BattleExtra_EquipOpen's call): window records 0..4 set
// up as the screen's - their in-use, kind and state bytes, positions,
// cursors, counts, the list pointer 0x675EB8 in records 0 and 1 - and record
// 0's +0xC the first of the party list's three entries (0x904062) that is 0
// or 7 (left as it was when none is). Each store as the original orders it.
extern "C" void __cdecl BattleExtra_EquipSetupWindows(void) {
    B(::bof3::Img(0x803161)) = 2;
    B(::bof3::Img(0x803162)) = 2;
    B(::bof3::Img(0x803163)) = 2;
    B(::bof3::Img(0x803160)) = 1;
    W16(::bof3::Img(0x803164), 0x140);
    W16(::bof3::Img(0x803166), 0x3E);
    for (unsigned i = 0;;) {
        const unsigned char who = B(::bof3::Img(at::kPartyList.v) + i);
        if (who == 0 || who == 7) {
            Window0_Cursor = static_cast<unsigned char>(i);
            break;
        }
        i = (i + 1) & 0xFF;
        if (i >= 3) break;
    }
    SetLong(At(::bof3::Img(0x803180)), static_cast<std::int32_t>(::bof3::Img(at::kChosen.v)));
    SetLong(At(::bof3::Img(0x8031A4)), static_cast<std::int32_t>(::bof3::Img(at::kChosen.v)));
    B(::bof3::Img(0x8031AA)) = 4;
    W16(::bof3::Img(0x8031AE), 0xFFEC);
    W16(::bof3::Img(0x8031D2), 0xFFEC);
    B(::bof3::Img(0x8031CE)) = 4;
    B(::bof3::Img(0x803169)) = 0;
    B(::bof3::Img(0x80316B)) = 0;
    Window0_ParamA = 0;
    Window0_Category = 0;
    Window0_Item = 0;
    Window0_Arrows = 0;
    B(::bof3::Img(0x803185)) = 2;
    WindowRecords[1].kind = 3;
    WindowRecords[1].state = 2;
    WindowRecords[1].in_use = 1;
    W16(::bof3::Img(0x803188), 0xFF56);
    W16(::bof3::Img(0x80318A), 0x3E);
    B(::bof3::Img(0x803190)) = 7;
    Window1_ParamA = 0;
    B(::bof3::Img(0x80318F)) = 0xFF;
    Window1_Master = 1;
    B(::bof3::Img(0x8031A9)) = 2;
    WindowRecords[2].state = 2;
    WindowRecords[2].in_use = 1;
    W16(::bof3::Img(0x8031AC), 0x14);
    W16(::bof3::Img(0x8031B8), 0);
    W16(::bof3::Img(0x8031BA), 0x118);
    B(::bof3::Img(0x8031CD)) = 2;
    B(::bof3::Img(0x8031CF)) = 3;
    B(::bof3::Img(0x8031CC)) = 1;
    W16(::bof3::Img(0x8031D0), 0xA4);
    W16(::bof3::Img(0x8031DC), 0);
    W16(::bof3::Img(0x8031DE), 0x80);
    B(::bof3::Img(0x8031F1)) = 2;
    B(::bof3::Img(0x8031F2)) = 1;
    B(::bof3::Img(0x8031F0)) = 0;
}

// original 0x42E250 (BattleExtra_EquipListInput's confirm): for each of the
// six equipment slots, a chosen byte 0x675EB8[i] that is not 0 and not what
// record 7 holds at +0x12 + i: Inventory_Remove(category, chosen, 1) - the
// category 0x64AE20[i] - and Inventory_Add(category, the held byte read again,
// 1) (each with a fourth word 0 the callees do not read), then the chosen
// byte, read again, into the record. Then Char_RecalcStats(record 7).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleExtra_EquipCommit(void) {
    for (unsigned i = 0; i < at::kChosenCount; ++i) {
        const unsigned char chosen = B(::bof3::Img(at::kChosen.v) + i);
        if (chosen == 0) continue;
        unsigned char* const held = At(::bof3::Img(Rest3a_GuestEquip_addr) + i);
        if (chosen == held[0]) continue;
        BH_CALL(Inventory_Remove)(B(::bof3::Img(at::kCategories.v) + i), chosen, 1);
        const unsigned char back = held[0];
        BH_CALL(Inventory_Add)(B(::bof3::Img(at::kCategories.v) + i), back, 1);
        held[0] = B(::bof3::Img(at::kChosen.v) + i);
    }
    BH_CALL(Char_RecalcStats)(At(::bof3::Img(CharacterRecords_addr) + 0x47c));
}

// original 0x42E2F0 (BattleExtra_EquipListInput, each frame): window 13's
// +0xD = Item_EquipMask(window 0's +8, +0xD)'s bit 0 clear; record 7's
// bytes +0x14, +0x12, +0x13, +0x15, +0x17 into 0x675EBA, B8, B9, BB, BC (+0x16
// is not copied; 0x675EBC takes +0x17); then 0x675EB8[window 1's +0xA] =
// window 0's +0xD. al: +0x17 (the last byte loaded; the caller does not read
// it). The cursor indexes unchecked; ours aborts past the six bytes.
extern "C" unsigned char __cdecl BattleExtra_EquipRefresh(void) {
    const unsigned char category = Window0_Category;
    const unsigned char item = Window0_Item;
    const unsigned fits = BH_CALL(Item_EquipMask)(category, item);
    const unsigned char e14 = B(::bof3::Img(at::kGuestEquip.v) + 2);
    const unsigned char e12 = CharacterRecords[7].weapon;
    B(::bof3::Img(at::kWin13Dim.v)) = static_cast<unsigned char>(~fits & 1);
    const unsigned char e13 = B(::bof3::Img(at::kGuestEquip.v) + 1);
    B(::bof3::Img(at::kChosen.v) + 2) = e14;
    const unsigned slot = Window1_ParamA;
    B(::bof3::Img(at::kChosen.v) + 0) = e12;
    const unsigned char e15 = B(::bof3::Img(at::kGuestEquip.v) + 3);
    B(::bof3::Img(at::kChosen.v) + 1) = e13;
    const unsigned char e17 = B(::bof3::Img(at::kGuestEquip.v) + 5);
    B(::bof3::Img(at::kChosen.v) + 3) = e15;
    B(::bof3::Img(at::kChosen.v) + 4) = e17;
    if (slot >= at::kChosenCount)
        bof3::Fatal("BattleExtra_EquipRefresh: window 1's slot cursor is %u, past the %u chosen bytes - the original writes on "
                    "(docs/rest_3a.md section 7)",
                    slot, at::kChosenCount);
    B(::bof3::Img(at::kChosen.v) + slot) = item;
    return e17;
}

void BattleExtra_Inject() {
    BOF3_INJECT(BattleExtra_Dispatch);
    BOF3_INJECT(BattleExtra_Start);
    BOF3_INJECT(BattleExtra_Leave);
    BOF3_INJECT(BattleExtra_TallyDispatch);
    BOF3_INJECT(BattleExtra_TallyOpenDispatch);
    BOF3_INJECT(BattleExtra_OpenTransition);
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
    BOF3_INJECT(BattleExtra_EquipSetupWindows);
    BOF3_INJECT(BattleExtra_EquipCommit);
    BOF3_INJECT(BattleExtra_EquipRefresh);
}
