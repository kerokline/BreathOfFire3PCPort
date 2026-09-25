// The field menu and the list draws (round eight, group DH).
// docs/menu_lists.md.
//
// The field menu is mode 3's per-frame call 0x589970 (docs/menu-screens.md
// section 1): a dispatch on the state byte 0x929F00, the set-up (state 0),
// the top bar (state 1) with its three steps, and the two helpers the top
// bar calls - the window placement 0x589E60 and the party check on leaving,
// 0x589FE0. The PSX's START.EMI, compiled into the exe.
//
// The list draws are record handler 6 of Field_RunTaskRecords (0x599B50):
// the top bar's five windows - the three member panels, the money box, the
// play-time box, the icon row and the screen title - by the record's kind,
// each running its state first and then drawing; and six of the slide steps
// the state tables of handlers 1, 6 and 7 share (0x59A3A0..0x59A680).
//
// Every call goes through menu_lists::g (menu_lists_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. The .data dispatch tables are read afresh and
// unchecked, as the originals read them: a state or kind past a table's end
// reaches the next table's entries in both (they are data, so ours reads the
// same dword the original would). The current window record 0x905B84 is
// re-read wherever the original re-reads it.
//
// Upper halves. The originals push coordinates as 16-bit registers and ids
// and colours as byte registers whose upper bits are whatever the register
// held; every callee here reads only the low 16 bits of a coordinate and the
// low byte of an id, count, flag or colour (the stand-ins record exactly
// that; the evidence for each callee is in symbols.toml and
// docs/menu_lists.md section 2), so ours passes the value as C++ computes it.
//
// Everything here is a faithful replacement: no DIVERGENCE.md entry is owed.
// One divergence has patch sites inside these bodies and survives in ours:
// DIV-0041 (widescreen.cpp) widens the bounds of MenuSlide_LeftOff and
// MenuSlide_RightOff; ours reads the bound from the operand it patches.
#include "game/menu_lists.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/menu_lists_callees.h"
#include "game/move_script_bytes.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace menu_lists {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    FieldMenu_PlaceWindows,
    FieldMenu_ReconcileParty,
    MenuList_DrawMemberPanel,
    Menu_DrawBackdrop,
    Char_RecalcStats,
    Party_Count,
    Fn<unsigned char (__cdecl*)()>(kExitGateway),
    Fn<unsigned char (__cdecl*)()>(kCampCell),
    Input_AutoRepeat,
    Sound_PlayEffect,
    Window_ResetAll,
    Fn<void (__cdecl*)(int, int, unsigned, unsigned, int)>(kMemberBody),
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_GetClut,
    Fn<void (__cdecl*)(int, int, unsigned, unsigned, unsigned, unsigned)>(kMemberFace),
    Menu_DrawMoneyBox,
    Fn<void (__cdecl*)(int, int)>(kTimeBox),
    Menu_DrawIcon,
    Menu_DrawTitleBox,
    Msg_SystemPtr,
    Text_CharCount,
    Text_DrawAt,
};
Callees g = kOriginals;

}  // namespace menu_lists

using namespace menu_lists;

namespace {

using Step = void (__cdecl*)();

// Entry `index` of a code-pointer table in .data, read afresh and unchecked.
Step Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Step>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(table + index * 4)))));
}

// The record the window layer is running, re-read from 0x905B84 wherever the
// original re-reads it. Volatile so that ours cannot cache it either: the
// fuzz's stand-ins repoint it between calls.
unsigned char* Rec() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(
        *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(at::kCurrent))));
}

unsigned char& Byte(std::uint32_t address) { return At(address)[0]; }
short S(const unsigned char* p) { return static_cast<short>(Word(p)); }

// The low byte of Party_Count's answer: every caller here tests al or masks
// eax with 0xFF.
unsigned Members() { return static_cast<unsigned>(g.party_count(0)) & 0xFF; }

}  // namespace

// ===========================================================================
// The field menu (START.EMI)
// ===========================================================================

// original 0x589970 (recorded start, 38 calls on the world-map route): mode
// 3's per-frame call from 0x5172F0, `jmp [0x6672B4 + 4 (0x929F00 & 0xFF)]` -
// state 0 FieldMenu_Open, 1 FieldMenu_TopBar, 2 Items 0x58AAE0, 3 Ability
// 0x58D7C0, 4 Equipment 0x58C2C0, 5 Tactics 0x58F080, 6 Status 0x58A4C0,
// 7 Config 0x5902E0, 8 0x5902D0 (docs/menu-screens.md section 1). A jump, as
// the original's: the state's function returns to 0x5172F0.
extern "C" void __cdecl FieldMenu_Run(void) {
    const Step step = Entry(at::kStates, Byte(at::kMenu));
    [[clang::musttail]] return step();
}

// original 0x589990 (PSX 0x801D17B0), state 0: the menu opened.
//   - 0x7E0674..0x7E0676, the cursor 0x929F05, 0x937F8E / 0x937F8F, 0x905D90
//     and 0x905BA1 zeroed;
//   - Char_RecalcStats for each of the Field_MemberCount members (the count
//     re-read after each), the record of 0x66972C[party[i]];
//   - the party and the reserve copied to 0x6BDF98 / 0x6BDF9B for
//     FieldMenu_ReconcileParty, one pair a Party_Count(0) (asked again each
//     time), the rest of the three 0xFF;
//   - 0x929F11 cleared; each of the eight character records' word +0x10
//     gets bit 13 when its +0x20 / 4 is above its +0x18, else loses it (HP
//     under a quarter, a guess from the fields);
//   - nineteen of the menu's globals 0x939880..0x9398CC zeroed;
//   - then 0x929F10 = 1 opens straight on state 5 step 5 (both bytes 5);
//     anything else places the windows and goes to the next state, step 0.
//
// As the original has it: both counters are signed bytes compared as ints
// (the member loop's index into the party list is the signed byte), so a
// count of 128 or more never ends either loop; a Party_Count above 3 copies
// past the three pairs, the reserve's bytes over the party's. The state is
// re-read after the placement.
extern "C" void __cdecl FieldMenu_Open(void) {
    Byte(0x7E0675) = 0;
    Byte(0x7E0674) = 0;
    Byte(0x7E0676) = 0;
    const unsigned char count = Byte(at::kMemberCount);
    Byte(at::kCursor) = 0;
    Byte(0x937F8F) = 0;
    Byte(0x937F8E) = 0;
    Byte(0x905D90) = 0;
    Byte(0x905BA1) = 0;
    if (count != 0) {
        signed char i = 0;
        do {
            const unsigned member = At(at::kParty + static_cast<std::int32_t>(i))[0];
            g.recalc_stats(At(at::kCharRecords + At(at::kMemberRecord)[member] * at::kCharRecordSize));
            i = static_cast<signed char>(i + 1);
        } while (static_cast<int>(i) < static_cast<int>(Byte(at::kMemberCount)));
    }
    signed char n = 0;
    if (Members() != 0) {
        int k = 0;
        do {
            const unsigned char member = At(at::kParty)[k];
            const unsigned char reserve = At(at::kReserve)[k];
            At(at::kSavedParty)[k] = member;
            n = static_cast<signed char>(n + 1);
            At(at::kSavedReserve)[k] = reserve;
            k = n;
        } while (k < static_cast<int>(Members()));
    }
    if (n < 3) {
        for (int k = n; k < 3; ++k) {
            At(at::kSavedParty)[k] = 0xFF;
            At(at::kSavedReserve)[k] = 0xFF;
        }
    }
    Byte(at::kPartyChanged) = 0;
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const r = At(at::kCharRecords + 0x10 + i * at::kCharRecordSize);
        if (static_cast<unsigned>(Word(r + 0x10) >> 2) > Word(r + 8)) r[1] = static_cast<unsigned char>(r[1] | 0x20);
        else SetWord(r, Word(r) & 0xDFFF);
    }
    static constexpr std::uint32_t kDwords[] = {0x9398B8, 0x939898, 0x9398A0, 0x9398A4, 0x9398A8, 0x939880, 0x9398AC,
                                                0x939884, 0x9398C0, 0x939888, 0x9398C4, 0x93988C, 0x9398C8};
    for (const std::uint32_t a : kDwords) move_script::SetLong(At(a), 0);
    SetWord(At(0x9398B0), 0);
    SetWord(At(0x939890), 0);
    Byte(0x9398BC) = 0;
    Byte(0x93989C) = 0;
    Byte(0x939892) = 0;
    Byte(0x9398CC) = 0;
    if (Byte(at::kOpenedAt) == 1) {
        Byte(at::kMenu) = 5;
        Byte(at::kStep) = 5;
        return;
    }
    g.place_windows();
    const unsigned char state = Byte(at::kMenu);
    Byte(at::kStep) = 0;
    Byte(at::kMenu) = static_cast<unsigned char>(state + 1);
}

// original 0x589B60, state 1: `jmp [0x6672D8 + 4 0x929F01]` - step 0
// FieldMenu_TopBarInput, 1 the countdown into a screen 0x589E00 (unread, in no
// group), 2 FieldMenu_BackdropStep. The byte unchecked; a jump, as the
// original's.
extern "C" void __cdecl FieldMenu_TopBar(void) {
    const Step step = Entry(at::kTopBarSteps, Byte(at::kStep));
    [[clang::musttail]] return step();
}

// original 0x589B70, the top bar's step 0:
//   - the backdrop (Config's "Background" byte);
//   - the icon row's record 9 gets the cursor (+0xA), the title's record 10
//     the title text 0x6672E4[cursor] (+0x10, a word);
//   - Camp is refused (record 9's +0xC = 0x8032B0 = 1) unless, with
//     Sprite_Current on ObjTrio: 0x904152 is set and the leader's cell
//     passes 0x589FB0; or Field_InputFlags bit 0 or area 0xBD, and either
//     the gateway exit 0x531820 answers non-zero or (0x904152 read again)
//     the cell passes;
//   - left / right (0x8000 / 0x2000 of Input_AutoRepeat(Input_Pressed &
//     0xA000)) move the cursor round 0..6 with sound 0x101, the countdown 0;
//   - cancel: the party check, step + 2 and Game_Step + 1 (the menu closes);
//   - confirm on Camp: refused with sound 0x107, or the party check, Game_Step
//     + 1, 0x905B60 = 1, step + 1;
//   - confirm elsewhere: sound 0x104, the countdown 5, the four windows 7..10
//     to state 1 (they slide), each member panel (one a Party_Count, asked
//     each time) to state 1 for Items, 7 for Tactics, 2 for the rest; step + 1.
//
// As the original has it: the title's index is the cursor as a signed byte;
// the cursor is re-read after each sound, left wraps any value that turns
// negative to 6 and right any above 6 (signed) to 0; Input_Pressed is read
// again after the moves; the state and step bytes after each call.
extern "C" void __cdecl FieldMenu_TopBarInput(void) {
    g.backdrop(Byte(at::kBackground));
    const unsigned char cursor = Byte(at::kCursor);
    unsigned char* const saved = Sprite_Current;
    Byte(at::kIconWindow + 0xA) = cursor;
    const unsigned char camp = Byte(at::kCampFlag);
    SetWord(At(at::kTitleWindow + 0x10), At(at::kTitleIds + static_cast<std::int32_t>(static_cast<signed char>(cursor)))[0]);
    Sprite_Current = At(at::kObjTrio);
    bool open;
    if (camp != 0) open = g.camp_cell() != 0;
    else if ((Byte(at::kInputFlags) & 1) == 0 && Word(At(at::kArea)) != 0xBD) open = false;
    else if (g.exit_gateway() != 0) open = true;
    else if (Byte(at::kCampFlag) == 0) open = false;
    else open = g.camp_cell() != 0;
    Byte(at::kIconWindow + 0xC) = open ? 0 : 1;
    const unsigned pressed = Word(At(at::kPressed)) & 0xA000u;
    Sprite_Current = saved;
    const unsigned held = g.auto_repeat(pressed);
    if (held & 0x8000) {
        g.sound(0x101);
        const auto c = static_cast<unsigned char>(Byte(at::kCursor) - 1);
        Byte(at::kCountdown) = 0;
        Byte(at::kCursor) = c;
        if (static_cast<signed char>(c) < 0) Byte(at::kCursor) = 6;
    }
    if (held & 0x2000) {
        g.sound(0x101);
        const auto c = static_cast<unsigned char>(Byte(at::kCursor) + 1);
        Byte(at::kCountdown) = 0;
        Byte(at::kCursor) = c;
        if (static_cast<signed char>(c) > 6) Byte(at::kCursor) = 0;
    }
    const unsigned in = Word(At(at::kPressed));
    if ((Word(At(at::kCancel)) & in) != 0) {
        g.reconcile();
        const unsigned char step = Byte(at::kStep);
        SetWord(At(at::kGameStep), Word(At(at::kGameStep)) + 1);
        Byte(at::kStep) = static_cast<unsigned char>(step + 2);
        return;
    }
    if ((Word(At(at::kConfirm)) & in) == 0) return;
    if (Byte(at::kCursor) == 6) {
        if (Byte(at::kIconWindow + 0xC) == 0) {
            g.reconcile();
            const unsigned char step = Byte(at::kStep);
            SetWord(At(at::kGameStep), Word(At(at::kGameStep)) + 1);
            Byte(at::kCampChosen) = 1;
            Byte(at::kStep) = static_cast<unsigned char>(step + 1);
            return;
        }
        g.sound(0x107);
        return;
    }
    g.sound(0x104);
    const unsigned char chosen = Byte(at::kCursor);
    Byte(at::kCountdown) = 5;
    Byte(at::kMoneyWindow + 3) = 1;
    Byte(at::kTimeWindow + 3) = 1;
    Byte(at::kIconWindow + 3) = 1;
    Byte(at::kTitleWindow + 3) = 1;
    const unsigned char state = chosen == 0 ? 1 : chosen == 5 ? 7 : 2;
    std::uint16_t i = 0;
    if (Members() != 0) {
        do {
            At(at::kMemberPanels + 3 + static_cast<std::uint32_t>(i) * at::kRecordSize)[0] = state;
            ++i;
        } while (i < static_cast<std::uint16_t>(Members()));
    }
    Byte(at::kStep) = static_cast<unsigned char>(Byte(at::kStep) + 1);
}

// original 0x589E50 (13 bytes): the backdrop alone - the top bar's step 2,
// and a slot of the table at 0x66734C (0x667350) another screen steps
// through.
extern "C" void __cdecl FieldMenu_BackdropStep(void) { g.backdrop(Byte(at::kBackground)); }

// original 0x589E60 (PSX 0x801D2010): the top bar's windows placed, after
// Window_ResetAll. Each member (one a Party_Count(0), asked each time) gets
// window record 4 + i: in use, handler 6, kind 0, state 0, at (0x62, 0x3E +
// 0x36 i), +0xA the member, +0xB and +0xC 0. Then records 7..10, handler 6:
// kind 1 the money box at (0xC8, 0x10), kind 2 the play time at (0x10,
// 0x10), kind 3 the icon row at (0x68, 0x2A) with +0xA / +0xB / +0xC and the
// word +0x10 zero, kind 4 the title at (0x73, 0x10) with the text word +0x10
// zero; all state 0.
//
// As the original has it: the loop compares unsigned bytes, so a Party_Count
// of 18 or more writes records past the 22.
extern "C" void __cdecl FieldMenu_PlaceWindows(void) {
    g.window_reset();
    unsigned char i = 0;
    if (Members() != 0) {
        do {
            unsigned char* const r = At(at::kMemberPanels + i * at::kRecordSize);
            r[0xA] = i;
            r[1] = 6;
            r[2] = 0;
            r[3] = 0;
            r[0] = 1;
            SetWord(r + 4, 0x62);
            SetWord(r + 6, i * 0x36u + 0x3E);
            r[0xB] = 0;
            r[0xC] = 0;
            ++i;
        } while (i < Members());
    }
    struct Place { std::uint32_t at; unsigned char kind; std::uint16_t x, y; };
    static constexpr Place kPlaces[] = {{at::kMoneyWindow, 1, 0xC8, 0x10},
                                        {at::kTimeWindow, 2, 0x10, 0x10},
                                        {at::kIconWindow, 3, 0x68, 0x2A},
                                        {at::kTitleWindow, 4, 0x73, 0x10}};
    for (const Place& p : kPlaces) {
        unsigned char* const r = At(p.at);
        r[1] = 6;
        r[2] = p.kind;
        r[3] = 0;
        r[0] = 1;
        SetWord(r + 4, p.x);
        SetWord(r + 6, p.y);
    }
    Byte(at::kIconWindow + 0xA) = 0;
    Byte(at::kIconWindow + 0xB) = 0;
    Byte(at::kIconWindow + 0xC) = 0;
    SetWord(At(at::kIconWindow + 0x10), 0);
    SetWord(At(at::kTitleWindow + 0x10), 0);
}

// original 0x589FE0: the party check as the menu closes (the top bar's
// cancel and Camp). Each character of the party the menu opened on
// (0x6BDF98, three bytes, 0xFF none) that is not in the party now (0x904062)
// sets 0x929F11. Then each reserve slot now (0x904065) takes the three bytes
// at 0x9045FC + 3 j that the same character had in slot j when the menu
// opened (0x6BDF9B), or zeros when it was not in the reserve; an empty slot
// (0xFF) keeps its bytes. The nine bytes are read once, before any is
// written - they move with their characters.
//
// As the original has it: a character is found by its first match; 0xFF in
// the list searched never matches.
extern "C" void __cdecl FieldMenu_ReconcileParty(void) {
    unsigned char missing = 0;
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned char m = At(at::kSavedParty)[k];
        if (m == 0xFF) continue;
        unsigned char add = 1;
        for (unsigned j = 0; j < 3; ++j) {
            const unsigned char p = At(at::kParty)[j];
            if (p != 0xFF && m == p) {
                add = 0;
                break;
            }
        }
        missing = static_cast<unsigned char>(missing + add);
    }
    if (missing != 0) Byte(at::kPartyChanged) = 1;
    unsigned char before[9];
    for (unsigned b = 0; b < 9; ++b) before[b] = At(at::kReserveData)[b];
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned char m = At(at::kReserve)[k];
        if (m == 0xFF) continue;
        unsigned char* const out = At(at::kReserveData + 3 * k);
        unsigned j = 0;
        for (; j < 3; ++j) {
            const unsigned char s = At(at::kSavedReserve)[j];
            if (s != 0xFF && m == s) break;
        }
        if (j < 3) {
            out[0] = before[3 * j];
            out[1] = before[3 * j + 1];
            out[2] = before[3 * j + 2];
        } else {
            out[0] = 0;
            out[1] = 0;
            out[2] = 0;
        }
    }
}

// ===========================================================================
// Record handler 6: the top bar's windows
// ===========================================================================

// original 0x599B50: record handler 6 of Field_RunTaskRecords, `jmp
// [0x66AF94 + 4 kind]` on the current record's +2 - 0 MenuList_MemberPanel,
// 1 MenuList_MoneyBox, 2 MenuList_TimeBox, 3 MenuList_TopBarIcons, 4
// MenuList_TitleBox, and sixteen more kinds of other screens (5..20). The
// kind unchecked; a jump, as the original's.
extern "C" void __cdecl MenuList_Run(void) {
    const Step step = Entry(at::kKinds, Rec()[2]);
    [[clang::musttail]] return step();
}

// original 0x599B70, kind 0: the record's state from 0x66AFE8 (+3,
// unchecked), then MenuList_DrawMemberPanel of the record (re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl MenuList_MemberPanel(void) {
    Entry(at::kPanelStates, Rec()[3])();
    g.draw_member(Rec());
}

// original 0x599B90 (recorded start, 114 calls on the world-map route): a
// member's panel at the record's (+4, +6): 0x573560(x, y, the record of
// 0x66972C[0x904062[3 +0xB + +0xA]], +0xC, 0); then the draw mode (page 0xF)
// and the member's number, the 8 x 8 cell (+0xA + 3, 0x1E) at (x + 9, y + 5)
// in CLUT (0x10, 0x1E0), shade 0x80, through 0x5744B0.
//
// As the original has it: the record is the argument, not 0x905B84; its
// fields are read again after the calls; Gfx_PacketNext after the panel.
extern "C" void __cdecl MenuList_DrawMemberPanel(unsigned char* rec) {
    const unsigned member = At(at::kParty + rec[0xB] * 3u + rec[0xA])[0];
    const unsigned record = At(at::kMemberRecord + member)[0];
    g.member_body(Word(rec + 4), Word(rec + 6), record, rec[0xC], 0);
    g.draw_mode(Gfx_PacketNext, 0, 0, 0xF, 0);
    g.commit(1, 0xC);
    const unsigned clut = g.get_clut(0x10, 0x1E0);
    g.member_face(Word(rec + 4) + 9, Word(rec + 6) + 5, rec[0xA] + 3u, 0x1E, clut, 0x80);
}

// original 0x599D50, kind 1: the state from 0x66B014, then Menu_DrawMoneyBox
// at the record's (+4, +6) with the money 0x904058.
extern "C" void __cdecl MenuList_MoneyBox(void) {
    Entry(at::kMoneyStates, Rec()[3])();
    const unsigned char* const r = Rec();
    g.money_box(Word(r + 4), Word(r + 6), 0, static_cast<unsigned>(Long(At(at::kMoney))));
}

// original 0x599DC0, kind 2: the state from 0x66B020, then the play-time box
// 0x5746C0 at the record's (+4, +6).
extern "C" void __cdecl MenuList_TimeBox(void) {
    Entry(at::kTimeStates, Rec()[3])();
    const unsigned char* const r = Rec();
    g.time_box(Word(r + 4), Word(r + 6));
}

// original 0x599E50, kind 3 (the body is the tail jump's target 0x599E70,
// reached from nowhere else): the state from 0x66B02C, then the top bar's
// seven icons, 0x6672AC[i] at (x + 16 i, y), 16 x 16. The one under the
// cursor (+0xA) is drawn last and grows: +0x10 counts the frames since the
// cursor moved (reset when +0xB, the cursor last drawn, differs; at most 3)
// and the icon is (16 + 2 n) square, n up and left. Camp's (6) is shaded
// 0x40 while +0xC refuses it, else 0x80. +0xB takes the cursor.
//
// As the original has it: the record is re-read after every call and every
// store; the count is compared as a signed word; the size is the count's low
// byte plus 8, doubled, as a byte.
extern "C" void __cdecl MenuList_TopBarIcons(void) {
    Entry(at::kIconStates, Rec()[3])();
    unsigned char* r = Rec();
    if (r[0xB] != r[0xA]) {
        SetWord(r + 0x10, 0);
        r = Rec();
    }
    const short count = S(r + 0x10);
    if (count < 3) {
        SetWord(r + 0x10, static_cast<std::uint16_t>(count + 1));
        r = Rec();
    }
    for (unsigned i = 0; i < 7; ++i) {
        if (i == r[0xA]) continue;
        const unsigned shade = i == 6 && r[0xC] != 0 ? 0x40 : 0x80;
        const unsigned y = Word(r + 6);
        const unsigned x = ((i << 4) + Word(r + 4)) & 0xFFFF;
        g.icon(At(at::kIconIds + i)[0], x, y, 0x10, 0x10, shade);
        r = Rec();
    }
    const unsigned sel = r[0xA];
    const unsigned shade = sel == 6 && r[0xC] != 0 ? 0x40 : 0x80;
    const unsigned grow = Word(r + 0x10);
    const unsigned size = static_cast<unsigned char>((r[0x10] + 8) << 1);
    const unsigned y = (Word(r + 6) - grow) & 0xFFFF;
    const unsigned x = ((((sel << 4) + Word(r + 4)) & 0xFFFF) - grow) & 0xFFFF;
    g.icon(At(at::kIconIds + sel)[0], x, y, size, size, shade);
    r = Rec();
    r[0xB] = r[0xA];
}

// original 0x599FA0, kind 4: the state from 0x66B038, then the screen title -
// Menu_DrawTitleBox(x, y, 0x48, 0x13, Config's colour) at the record's (+4,
// +6) and the system text +0x10 (Msg_SystemPtr) through Text_DrawAt at (x +
// 0x25 - 6 n, y + 3), colour 0, count n, n its Text_CharCount. The Chinese
// title of each top-bar entry (docs/menu_lists.md section 3): centred for
// 12-pixel glyphs on the box's middle, x + 0x25.
//
// As the original has it: the record is re-read after the box, after the
// first count (for y) and after the second (for x); the text is counted
// twice, the first answer the count, the second the offset.
extern "C" void __cdecl MenuList_TitleBox(void) {
    Entry(at::kTitleStates, Rec()[3])();
    const unsigned char colour = Byte(at::kColour);
    const unsigned char* r = Rec();
    g.title_box(Word(r + 4), Word(r + 6), 0x48, 0x13, colour);
    r = Rec();
    const unsigned char* const text = g.msg(Word(r + 0x10));
    const unsigned count = g.char_count(text);
    r = Rec();
    const unsigned y = (Word(r + 6) + 3u) & 0xFFFF;
    const unsigned width = g.char_count(text);
    r = Rec();
    const int x = static_cast<int>((Word(r + 4) - width * 6 + 0x25u) & 0xFFFF);
    g.text(x, static_cast<int>(y), 0, static_cast<int>(count), text);
}

// ===========================================================================
// The slide steps
// ===========================================================================
//
// Six states the window records' state tables share (handlers 1, 6, 7 and
// the shop's, symbols.toml): the record's x (+4) or y (+6) moves by a step a
// frame until it passes a bound, where it is held and the state (+3) goes to
// 0. As the originals have them: the word is compared signed after the move;
// the record is re-read after the move and after the clamp.
namespace {
void SlideY(int step, short bound, bool down) {
    unsigned char* const moved = Rec();
    SetWord(moved + 6, Word(moved + 6) + static_cast<unsigned>(step));
    unsigned char* const r = Rec();
    if (down ? S(r + 6) > bound : S(r + 6) < bound) {
        SetWord(r + 6, static_cast<std::uint16_t>(bound));
        Rec()[3] = 0;
    }
}
void SlideX(int step, short bound, bool right) {
    unsigned char* const moved = Rec();
    SetWord(moved + 4, Word(moved + 4) + static_cast<unsigned>(step));
    unsigned char* const r = Rec();
    if (right ? S(r + 4) > bound : S(r + 4) < bound) {
        SetWord(r + 4, static_cast<std::uint16_t>(bound));
        Rec()[3] = 0;
    }
}
}  // namespace

// original 0x59A3A0: up 0x10 a frame to y -20 (off the top).
extern "C" void __cdecl MenuSlide_UpOff(void) { SlideY(-0x10, -20, false); }
// original 0x59A3D0: down 0x10 a frame to y 0x10.
extern "C" void __cdecl MenuSlide_DownTo16(void) { SlideY(0x10, 0x10, true); }
// original 0x59A580: left 0x20 a frame to x -200 (off the left); the bound is
// the operand DIV-0041 widens by the columns added (-253 in the wide view).
extern "C" void __cdecl MenuSlide_LeftOff(void) {
    SlideX(-0x20, static_cast<short>(Long(At(at::kLeftOffBound))), false);
}
// original 0x59A5B0: right 0x20 a frame to x 0x11.
extern "C" void __cdecl MenuSlide_RightTo17(void) { SlideX(0x20, 0x11, true); }
// original 0x59A5E0: right 0x20 a frame to x 0x140 (off the right); DIV-0041
// widens the bound (373).
extern "C" void __cdecl MenuSlide_RightOff(void) {
    SlideX(0x20, static_cast<short>(Long(At(at::kRightOffBound))), true);
}
// original 0x59A680: down 0x10 a frame to y 0x26.
extern "C" void __cdecl MenuSlide_DownTo38(void) { SlideY(0x10, 0x26, true); }

// ===========================================================================

// DIV-0041's two patch sites inside our bodies: MenuSlide_LeftOff and
// MenuSlide_RightOff read their bound from the imm32 of the original's `mov
// ecx, imm32` (widescreen.cpp kSlides, 0x59A586 and 0x59A5E6), so that the
// widened bound survives our takeover. Refused unless the byte before each is
// that instruction's B9 and the bound is the original's or the widened one.
namespace {
void CheckSlideBound(std::uint32_t operand, int original) {
    const int wide = original < 0 ? original - static_cast<int>(Widescreen_Live()) : original + static_cast<int>(Widescreen_Live());
    const int bound = Long(At(operand));
    if (At(operand - 1)[0] != 0xB9 || (bound != original && bound != wide))
        bof3::Fatal("menu_lists: 0x%X should be `mov ecx, %d` (or %d, DIV-0041), holds %02X then %d", operand - 1, original, wide,
                    At(operand - 1)[0], bound);
}
}  // namespace

void MenuLists_Inject() {
    CheckSlideBound(at::kLeftOffBound, -200);
    CheckSlideBound(at::kRightOffBound, 320);
    if (bof3::WantsShadow("menu_lists")) menu_lists::SelfTest();
    BOF3_INJECT(FieldMenu_Run);
    BOF3_INJECT(FieldMenu_Open);
    BOF3_INJECT(FieldMenu_TopBar);
    BOF3_INJECT(FieldMenu_TopBarInput);
    BOF3_INJECT(FieldMenu_BackdropStep);
    BOF3_INJECT(FieldMenu_PlaceWindows);
    BOF3_INJECT(FieldMenu_ReconcileParty);
    BOF3_INJECT(MenuList_Run);
    BOF3_INJECT(MenuList_MemberPanel);
    BOF3_INJECT(MenuList_DrawMemberPanel);
    BOF3_INJECT(MenuList_MoneyBox);
    BOF3_INJECT(MenuList_TimeBox);
    BOF3_INJECT(MenuList_TopBarIcons);
    BOF3_INJECT(MenuList_TitleBox);
    BOF3_INJECT(MenuSlide_UpOff);
    BOF3_INJECT(MenuSlide_DownTo16);
    BOF3_INJECT(MenuSlide_LeftOff);
    BOF3_INJECT(MenuSlide_RightTo17);
    BOF3_INJECT(MenuSlide_RightOff);
    BOF3_INJECT(MenuSlide_DownTo38);
}
