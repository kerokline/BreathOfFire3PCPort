// Moved here by tools/move_funcs.py from rest_2d.cpp (docs/source-layout-round.md).
#include "game/field/field_menu_status.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2d.h"
#include "game/rest_2d_callees.h"
#include "game/scenario_harness.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_2d::at;

}  // namespace

namespace {

using U = std::uint32_t;

// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& BRest2d(U a) { return move_script::At(a)[0]; }

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (its reader's
// reach, docs/rest_2d.md section 3); past it the original jumps through what
// follows the table, which no handler of the screen writes - ours aborts. The
// entry is called as the cell holds it: Capcom's address in the game (Inject's
// jmp to ours where it is ours), a recorder while the fuzz runs.
using Handler = void (__cdecl*)();

void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: step %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(move_script::At(table + 4 * index)))))();
}

// A record the original writes by an index it never bounds. Past the array it
// writes into the next block; no path in play gives such an index (section 5),
// so ours aborts there.
U RecordChecked(U base, U stride, unsigned count, unsigned index, const char* array, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: %s index %u is past its %u records; the original writes at 0x%X", who, array, index, count,
                    static_cast<unsigned>(::bof3::VaOf(base + index * stride)));
    return base + index * stride;
}

U Window(unsigned index, const char* who) {
    return RecordChecked(::bof3::Img(WindowRecords_addr), rest_2d::at::kWindowStride, rest_2d::at::kWindowCount, index, "WindowRecords", who);
}

U RecordOf(unsigned member) { return ::bof3::Img(CharacterRecords_addr) + member * rest_2d::at::kRecordStride; }

const unsigned char* StatusRecord() {
    const auto c = BattleActorCopies_ItemActor;
    return move_script::At(RecordOf(Character_IndexById[Cond_Flags[0xd2 + c]]));
}

// The detail's and the list's windows: for each member i, window 4 + i's state
// `mine` and window 12 + i's `mine_list` when i is the cursor (signed), else
// `other` and `other_list`.
void MemberWindowStates(unsigned char mine, unsigned char mine_list, unsigned char other, unsigned char other_list,
                        const char* who) {
    for (unsigned char i = 0; i < static_cast<unsigned char>(SH_CALL(Party_Count)(0));) {
        const bool at_cursor = static_cast<int>(i) == BattleActorCopies_ItemActor;
        rv::As<records::WindowRecord>(move_script::At(Window(4 + i, who)))->state = at_cursor ? mine : other;
        BRest2d(Window(12 + i, who) + 3) = at_cursor ? mine_list : other_list;
        ++i;
    }
}

}  // namespace

// original 0x58A4C0 (0xE bytes), FieldMenu_States[6]: jmp [FieldMenuStatus_States
// + 4 * the menu state], unchecked.
extern "C" void __cdecl FieldMenuStatus_ByState(void) {
    Jump(::bof3::Img(FieldMenuStatus_States_addr), rest_2d::at::kStatusStateCount, FieldS_State, "FieldMenuStatus_ByState");
}

// original 0x58A4D0 (0x3A bytes), state 0: Menu_DrawBackdrop, the windows placed;
// the cursor 0, the timer 8, the state on; Sound_PlayEffect(0x102).
extern "C" void __cdecl FieldMenuStatus_Open(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    SH_CALL(FieldMenuStatus_PlaceWindows)();
    const auto state = static_cast<unsigned char>(FieldS_State + 1);
    BattleActorCopies_ItemActor = 0;
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = 8;
    FieldS_State = state;
    SH_CALL(Sound_PlayEffect)(0x102);
}

// original 0x58A510 (0x5F bytes), state 1: the cursor's member's name record
// (TextRecord_Set(0, 8, its record)), window 11's word +0x10 = 0x12,
// Menu_DrawBackdrop; the timer down, the state on when it was 0.
extern "C" void __cdecl FieldMenuStatus_SlideIn(void) {
    SH_CALL(TextRecord_Set)(0, 8, StatusRecord());
    const unsigned char kind = Config_Background;
    SetWord(move_script::At(::bof3::Img(0x8032FC)), 0x12);
    SH_CALL(Menu_DrawBackdrop)(kind);
    const unsigned char t = BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v));
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = static_cast<unsigned char>(t - 1);
    if (t == 0) FieldS_State = static_cast<unsigned char>(FieldS_State + 1);
}

// original 0x58A570 (0x1B8 bytes), state 2: Menu_DrawBackdrop; window 19 on at the
// cursor's member's window (4 + cursor)'s place; the name record, window 11's
// +0x10 = 0x12. Input_AutoRepeat(Input_Pressed & 0x5000): 0x1000 the cursor
// back (below 0 to the last), else 0x4000 on (past the last to 0) - signed
// against Party_Count(0); a cursor that differs from the one read before
// (zero-extended against sign-extended) Sound_PlayEffect(0x100). Confirm -
// 0x103, 0x102, the detail's windows, window 19 off, the timer 8, the state
// on; cancel (Input_Pressed read again) - 0x106, 0x102, windows 12 + i state 1
// for each member, window 11 state 1, window 19 off, the timer 8, state + 2.
extern "C" void __cdecl FieldMenuStatus_Choose(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    const auto c = BattleActorCopies_ItemActor;
    WindowRecords[19].in_use = 1;
    const U member_window = ::bof3::Img(WindowRecords_addr) + rest_2d::at::kWindowStride * static_cast<U>(4 + c);
    SetWord(move_script::At(::bof3::Img(0x803410)), rv::As<records::WindowRecord>(move_script::At(member_window))->x);
    WindowRecords[19].y = rv::As<records::WindowRecord>(move_script::At(member_window))->y;
    SH_CALL(TextRecord_Set)(0, 8, move_script::At(RecordOf(Character_IndexById[Cond_Flags[0xd2 + c]])));
    const unsigned pressed = Input_Pressed;
    SetWord(move_script::At(::bof3::Img(0x8032FC)), 0x12);
    const U held = SH_CALL(Input_AutoRepeat)(pressed & 0x5000);
    const auto count = static_cast<unsigned char>(SH_CALL(Party_Count)(0));
    const unsigned char old = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    unsigned char cursor = old;
    if (held & 0x1000) {
        cursor = static_cast<unsigned char>(cursor - 1);
        BattleActorCopies_ItemActor = cursor;
        if (static_cast<signed char>(cursor) < 0) {
            cursor = static_cast<unsigned char>(count - 1);
            BattleActorCopies_ItemActor = cursor;
        }
    } else if (held & 0x4000) {
        cursor = static_cast<unsigned char>(cursor + 1);
        BattleActorCopies_ItemActor = cursor;
        if (static_cast<int>(static_cast<signed char>(cursor)) >= static_cast<int>(count)) {
            cursor = 0;
            BattleActorCopies_ItemActor = 0;
        }
    }
    if (static_cast<int>(old) != static_cast<int>(static_cast<signed char>(cursor))) SH_CALL(Sound_PlayEffect)(0x100);
    if (Field_ConfirmButtons & Input_Pressed) {
        SH_CALL(Sound_PlayEffect)(0x103);
        SH_CALL(Sound_PlayEffect)(0x102);
        SH_CALL(FieldMenuStatus_DetailWindows)();
        const unsigned char state = FieldS_State;
        WindowRecords[19].in_use = 0;
        BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = 8;
        FieldS_State = static_cast<unsigned char>(state + 1);
    }
    if (Field_CancelButtons & Input_Pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(Sound_PlayEffect)(0x102);
        int i = 0;
        if (static_cast<unsigned char>(SH_CALL(Party_Count)(0)) != 0) {
            do {
                BRest2d(Window(12 + static_cast<unsigned>(i), "FieldMenuStatus_Choose") + 3) = 1;
                ++i;
            } while (i < static_cast<int>(static_cast<unsigned char>(SH_CALL(Party_Count)(0))));
        }
        const unsigned char state = FieldS_State;
        BRest2d(::bof3::Img(0x8032EF)) = 1;
        WindowRecords[19].in_use = 0;
        BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = 8;
        FieldS_State = static_cast<unsigned char>(state + 2);
    }
}

// original 0x58A730 (0x95 bytes), state 3: Menu_DrawBackdrop, the name record,
// window 11's +0x10 = 0x12; while the timer runs it counts down; then cancel -
// 0x106, 0x102, the list's windows, the timer 8, the state back two.
extern "C" void __cdecl FieldMenuStatus_Detail(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    SH_CALL(TextRecord_Set)(0, 8, StatusRecord());
    const unsigned char t = BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v));
    SetWord(move_script::At(::bof3::Img(0x8032FC)), 0x12);
    if (t != 0) {
        BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = static_cast<unsigned char>(BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) - 1);
        return;
    }
    if ((Input_Pressed & Field_CancelButtons) == 0) return;
    SH_CALL(Sound_PlayEffect)(0x106);
    SH_CALL(Sound_PlayEffect)(0x102);
    SH_CALL(FieldMenuStatus_ListWindows)();
    const unsigned char state = FieldS_State;
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = 8;
    FieldS_State = static_cast<unsigned char>(state - 2);
}

// original 0x58A7D0 (0x7F bytes), state 4: Menu_DrawBackdrop; the timer down, at
// 0 the screen's windows cleared, windows 4 + i state 4 for each member, the
// timer 0, windows 7..10 state 2, back to the top bar (mode 1, state 0).
extern "C" void __cdecl FieldMenuStatus_Close(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    const auto t = static_cast<unsigned char>(BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) - 1);
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = t;
    if (t != 0) return;
    SH_CALL(FieldMenuStatus_ClearWindows)();
    int i = 0;
    if (static_cast<unsigned char>(SH_CALL(Party_Count)(0)) != 0) {
        do {
            BRest2d(Window(4 + static_cast<unsigned>(i), "FieldMenuStatus_Close") + 3) = 4;
            ++i;
        } while (i < static_cast<int>(static_cast<unsigned char>(SH_CALL(Party_Count)(0))));
    }
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = 0;
    BRest2d(::bof3::Img(0x80325F)) = 2;
    BRest2d(::bof3::Img(0x803283)) = 2;
    BRest2d(::bof3::Img(0x8032A7)) = 2;
    BRest2d(::bof3::Img(0x8032CB)) = 2;
    BattleE1_Mode = 1;
    FieldS_State = 0;
}

// original 0x58A850 (0xCF bytes): for each member i (Party_Count(0) asked each
// pass), window 12 + i: +1 6, +2 5, +3 2, +0 1, +4 0x140, +0xA i, +6 window
// (4 + i)'s +6; window 11: +1 6, +2 9, +3 2, +0 1, +4 0x14, +6 -20, +0x10 0;
// window 19: +1 6, +2 0xA, +0 0, +0xA 0, +0xB 1.
extern "C" void __cdecl FieldMenuStatus_PlaceWindows(void) {
    for (unsigned char i = 0; i < static_cast<unsigned char>(SH_CALL(Party_Count)(0));) {
        const U w = Window(12 + i, "FieldMenuStatus_PlaceWindows");
        rv::As<records::WindowRecord>(move_script::At(w))->handler = 6;
        rv::As<records::WindowRecord>(move_script::At(w))->kind = 5;
        rv::As<records::WindowRecord>(move_script::At(w))->state = 2;
        rv::As<records::WindowRecord>(move_script::At(w))->in_use = 1;
        SetWord(move_script::At(w + 4), 0x140);
        const unsigned short y = Word(move_script::At(::bof3::Img(WindowRecords_addr) + rest_2d::at::kWindowStride * (4u + i) + 6));
        BRest2d(w + 0xA) = i;
        ++i;
        SetWord(move_script::At(w + 6), y);
    }
    BRest2d(::bof3::Img(0x8032ED)) = 6;
    BRest2d(::bof3::Img(0x8032EE)) = 9;
    BRest2d(::bof3::Img(0x8032EF)) = 2;
    BRest2d(::bof3::Img(0x8032EC)) = 1;
    SetWord(move_script::At(::bof3::Img(0x8032F0)), 0x14);
    SetWord(move_script::At(::bof3::Img(0x8032F2)), 0xFFEC);
    SetWord(move_script::At(::bof3::Img(0x8032FC)), 0);
    BRest2d(::bof3::Img(0x80340D)) = 6;
    BRest2d(::bof3::Img(0x80340E)) = 0xA;
    WindowRecords[19].in_use = 0;
    BRest2d(::bof3::Img(0x803416)) = 0;
    BRest2d(::bof3::Img(0x803417)) = 1;
}

// original 0x58A920 (0x109 bytes): the detail's windows - the members' windows 5 /
// 3 at the cursor, 7 / 5 elsewhere; windows 15, 16, 17 state 2, +1 6, +2 6 / 7
// / 8, +0 1, (+4, +6) (0x140, 0x74) / (-100, 0x78) / (-100, 0xA8), +0xA the
// cursor.
extern "C" void __cdecl FieldMenuStatus_DetailWindows(void) {
    MemberWindowStates(5, 3, 7, 5, "FieldMenuStatus_DetailWindows");
    const unsigned char cursor = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    BRest2d(::bof3::Img(0x80337F)) = 2;
    WindowRecords[16].state = 2;
    BRest2d(::bof3::Img(0x8033C7)) = 2;
    BRest2d(::bof3::Img(0x80337D)) = 6;
    BRest2d(::bof3::Img(0x80337E)) = 6;
    BRest2d(::bof3::Img(0x80337C)) = 1;
    SetWord(move_script::At(::bof3::Img(0x803380)), 0x140);
    SetWord(move_script::At(::bof3::Img(0x803382)), 0x74);
    BRest2d(::bof3::Img(0x803386)) = cursor;
    BRest2d(::bof3::Img(0x8033A1)) = 6;
    BRest2d(::bof3::Img(0x8033A2)) = 7;
    WindowRecords[16].in_use = 1;
    SetWord(move_script::At(::bof3::Img(0x8033A4)), 0xFF9C);
    WindowRecords[16].y = 0x78;
    Window16_Owner = cursor;
    BRest2d(::bof3::Img(0x8033C5)) = 6;
    BRest2d(::bof3::Img(0x8033C6)) = 8;
    WindowRecords[17].in_use = 1;
    SetWord(move_script::At(::bof3::Img(0x8033C8)), 0xFF9C);
    SetWord(move_script::At(::bof3::Img(0x8033CA)), 0xA8);
    BRest2d(::bof3::Img(0x8033CE)) = cursor;
}

// original 0x58AA30 (0x80 bytes): back to the list - the members' windows 6 / 4
// at the cursor, 8 / 6 elsewhere; windows 15, 16, 17 state 1.
extern "C" void __cdecl FieldMenuStatus_ListWindows(void) {
    MemberWindowStates(6, 4, 8, 6, "FieldMenuStatus_ListWindows");
    BRest2d(::bof3::Img(0x80337F)) = 1;
    WindowRecords[16].state = 1;
    BRest2d(::bof3::Img(0x8033C7)) = 1;
}

// original 0x58AAB0 (0x21 bytes): windows 12..17's +0 cleared.
extern "C" void __cdecl FieldMenuStatus_ClearWindows(void) {
    BRest2d(::bof3::Img(0x803310)) = 0;
    BRest2d(::bof3::Img(0x803334)) = 0;
    BRest2d(::bof3::Img(0x803358)) = 0;
    BRest2d(::bof3::Img(0x80337C)) = 0;
    WindowRecords[16].in_use = 0;
    WindowRecords[17].in_use = 0;
}

void FieldMenuStatus_Inject() {
    BOF3_INJECT(FieldMenuStatus_ByState);
    BOF3_INJECT(FieldMenuStatus_Open);
    BOF3_INJECT(FieldMenuStatus_SlideIn);
    BOF3_INJECT(FieldMenuStatus_Choose);
    BOF3_INJECT(FieldMenuStatus_Detail);
    BOF3_INJECT(FieldMenuStatus_Close);
    BOF3_INJECT(FieldMenuStatus_PlaceWindows);
    BOF3_INJECT(FieldMenuStatus_DetailWindows);
    BOF3_INJECT(FieldMenuStatus_ListWindows);
    BOF3_INJECT(FieldMenuStatus_ClearWindows);
}
