// Moved here by tools/move_funcs.py from menu_lists.cpp, rest_2d.cpp (docs/source-layout-round.md).
#include "game/field/field_menu.h"

#include <cstdint>
#include <cstring>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/lang_layout.h"
#include "game/menu_lists.h"
#include "game/menu_lists_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2d.h"
#include "game/rest_2d_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_2d::at;

}  // namespace

using namespace menu_lists;

namespace menu_lists {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

}  // namespace menu_lists

namespace {

using Step = void (__cdecl*)();

// Entry `index` of a code-pointer table in .data, read afresh and unchecked.
Step Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Step>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(menu_lists::Long(menu_lists::At(table + index * 4)))));
}

unsigned char& Byte(std::uint32_t address) { return At(address)[0]; }

// The low byte of Party_Count's answer: every caller here tests al or masks
// eax with 0xFF.
unsigned Members() { return static_cast<unsigned>(g.party_count(0)) & 0xFF; }

using U = std::uint32_t;

using move_script::Word;

unsigned char& BRest2d(U a) { return move_script::At(a)[0]; }

}  // namespace

// menu_lists's callee seam, moved here from menu_lists.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace menu_lists {
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
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(kExitGateway.v)),
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(kCampCell.v)),
    Input_AutoRepeat,
    Sound_PlayEffect,
    Window_ResetAll,
    Fn<void (__cdecl*)(int, int, unsigned, unsigned, int)>(::bof3::Code(kMemberBody.v)),
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_GetClut,
    Fn<void (__cdecl*)(int, int, unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(kMemberFace.v)),
    Menu_DrawMoneyBox,
    Fn<void (__cdecl*)(int, int)>(::bof3::Code(kTimeBox.v)),
    Menu_DrawIcon,
    Menu_DrawTitleBox,
    Msg_SystemPtr,
    Text_CharCount,
    Text_DrawAt,
};
Callees g = kOriginals;

}  // namespace menu_lists

// original 0x589970 (recorded start, 38 calls on the world-map route): mode
// 3's per-frame call from 0x5172F0, `jmp [0x6672B4 + 4 (0x929F00 & 0xFF)]` -
// state 0 FieldMenu_Open, 1 FieldMenu_TopBar, 2 Items 0x58AAE0, 3 Ability
// 0x58D7C0, 4 Equipment 0x58C2C0, 5 Tactics 0x58F080, 6 Status 0x58A4C0,
// 7 Config 0x5902E0, 8 0x5902D0 (docs/menu-screens.md section 1). A jump, as
// the original's: the state's function returns to 0x5172F0.
extern "C" void __cdecl FieldMenu_Run(void) {
    const Step step = Entry(::bof3::Img(FieldMenu_States_addr), BattleE1_Mode);
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
    Byte(::bof3::Img(0x7E0675)) = 0;
    Byte(::bof3::Img(0x7E0674)) = 0;
    Byte(::bof3::Img(0x7E0676)) = 0;
    const unsigned char count = Field_MemberCount;
    MenuLists_Cursor = 0;
    Byte(::bof3::Img(0x937F8F)) = 0;
    Byte(::bof3::Img(0x937F8E)) = 0;
    Byte(::bof3::Img(0x905D90)) = 0;
    Byte(::bof3::Img(0x905BA1)) = 0;
    if (count != 0) {
        signed char i = 0;
        do {
            const unsigned member = At(::bof3::Img(Cond_Flags_addr) + 0xd2 + static_cast<std::int32_t>(i))[0];
            g.recalc_stats(At(::bof3::Img(CharacterRecords_addr) + Character_IndexById[member] * menu_lists::at::kCharRecordSize));
            i = static_cast<signed char>(i + 1);
        } while (static_cast<int>(i) < Field_MemberCount);
    }
    signed char n = 0;
    if (Members() != 0) {
        int k = 0;
        do {
            const unsigned char member = Cond_Flags[0xd2 + k];
            const unsigned char reserve = Cond_Flags[0xd5 + k];
            At(::bof3::Img(menu_lists::at::kSavedParty.v))[k] = member;
            n = static_cast<signed char>(n + 1);
            At(::bof3::Img(menu_lists::at::kSavedReserve.v))[k] = reserve;
            k = n;
        } while (k < static_cast<int>(Members()));
    }
    if (n < 3) {
        for (int k = n; k < 3; ++k) {
            At(::bof3::Img(menu_lists::at::kSavedParty.v))[k] = 0xFF;
            At(::bof3::Img(menu_lists::at::kSavedReserve.v))[k] = 0xFF;
        }
    }
    MenuLists_PartyChanged = 0;
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const r = At(::bof3::Img(CharacterRecords_addr) + 0x10 + i * menu_lists::at::kCharRecordSize);
        // r is the character record's +0x10, its status word (records::CharRecord, CharacterRecords 0x903A70)
        auto* const c = rv::As<records::CharRecord>(r - 0x10);
        if (static_cast<unsigned>(c->stats.max_hp >> 2) > c->hp) r[1] = static_cast<unsigned char>(r[1] | 0x20);
        else rv::Store<unsigned>(c->status, c->status & 0xDFFF);
    }
    static constexpr std::uint32_t kDwords[] = {0x9398B8, 0x939898, 0x9398A0, 0x9398A4, 0x9398A8, 0x939880, 0x9398AC,
                                                0x939884, 0x9398C0, 0x939888, 0x9398C4, 0x93988C, 0x9398C8};
    for (const std::uint32_t a : kDwords) move_script::SetLong(At(::bof3::Img(a)), 0);
    SetWord(At(::bof3::Img(0x9398B0)), 0);
    SetWord(At(::bof3::Img(0x939890)), 0);
    Byte(::bof3::Img(0x9398BC)) = 0;
    Byte(::bof3::Img(0x93989C)) = 0;
    Rest2e_AbilityViewCursor = 0;
    Rest2e_AbilityViewTop = 0;
    if (ScenaSc6_Load10 == 1) {
        BattleE1_Mode = 5;
        FieldS_State = 5;
        return;
    }
    g.place_windows();
    const unsigned char state = BattleE1_Mode;
    FieldS_State = 0;
    BattleE1_Mode = static_cast<unsigned char>(state + 1);
}

// original 0x589B60, state 1: `jmp [0x6672D8 + 4 0x929F01]` - step 0
// FieldMenu_TopBarInput, 1 the countdown into a screen 0x589E00 (unread, in no
// group), 2 FieldMenu_BackdropStep. The byte unchecked; a jump, as the
// original's.
extern "C" void __cdecl FieldMenu_TopBar(void) {
    const Step step = Entry(::bof3::Img(FieldMenu_TopBarSteps_addr), FieldS_State);
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
    g.backdrop(Config_Background);
    const unsigned char cursor = static_cast<unsigned char>(MenuLists_Cursor);
    unsigned char* const saved = Sprite_Current;
    Byte(::bof3::Img(menu_lists::at::kIconWindow.v) + 0xA) = cursor;
    const unsigned char camp = AreaW3c_CampFlag;
    SetWord(At(::bof3::Img(Window10_addr) + 0x10), At(::bof3::Img(FieldMenu_TitleIds_addr) + static_cast<std::int32_t>(static_cast<signed char>(cursor)))[0]);
    Sprite_Current = At(::bof3::Img(ObjTrio_addr));
    bool open;
    if (camp != 0) open = g.camp_cell() != 0;
    else if ((Field_InputFlags & 1) == 0 && Game_AreaNumber != 0xBD) open = false;
    else if (g.exit_gateway() != 0) open = true;
    else if (AreaW3c_CampFlag == 0) open = false;
    else open = g.camp_cell() != 0;
    Byte(::bof3::Img(menu_lists::at::kIconWindow.v) + 0xC) = open ? 0 : 1;
    const unsigned pressed = Input_Pressed & 0xA000u;
    Sprite_Current = saved;
    const unsigned held = g.auto_repeat(pressed);
    if (held & 0x8000) {
        g.sound(0x101);
        const auto c = static_cast<unsigned char>(static_cast<unsigned char>(MenuLists_Cursor) - 1);
        Byte(::bof3::Img(menu_lists::at::kCountdown.v)) = 0;
        MenuLists_Cursor = c;
        if (static_cast<signed char>(c) < 0) MenuLists_Cursor = 6;
    }
    if (held & 0x2000) {
        g.sound(0x101);
        const auto c = static_cast<unsigned char>(static_cast<unsigned char>(MenuLists_Cursor) + 1);
        Byte(::bof3::Img(menu_lists::at::kCountdown.v)) = 0;
        MenuLists_Cursor = c;
        if (static_cast<signed char>(c) > 6) MenuLists_Cursor = 0;
    }
    const unsigned in = Input_Pressed;
    if ((Field_CancelButtons & in) != 0) {
        g.reconcile();
        const unsigned char step = FieldS_State;
        Game_Step = Game_Step + 1;
        FieldS_State = static_cast<unsigned char>(step + 2);
        return;
    }
    if ((Field_ConfirmButtons & in) == 0) return;
    if (static_cast<unsigned char>(MenuLists_Cursor) == 6) {
        if (Byte(::bof3::Img(menu_lists::at::kIconWindow.v) + 0xC) == 0) {
            g.reconcile();
            const unsigned char step = FieldS_State;
            Game_Step = Game_Step + 1;
            MenuLists_CampChosen = 1;
            FieldS_State = static_cast<unsigned char>(step + 1);
            return;
        }
        g.sound(0x107);
        return;
    }
    g.sound(0x104);
    const unsigned char chosen = static_cast<unsigned char>(MenuLists_Cursor);
    Byte(::bof3::Img(menu_lists::at::kCountdown.v)) = 5;
    Byte(::bof3::Img(menu_lists::at::kMoneyWindow.v) + 3) = 1;
    Byte(::bof3::Img(menu_lists::at::kTimeWindow.v) + 3) = 1;
    Byte(::bof3::Img(menu_lists::at::kIconWindow.v) + 3) = 1;
    Byte(::bof3::Img(menu_lists::at::kTitleWindow.v) + 3) = 1;
    const unsigned char state = chosen == 0 ? 1 : chosen == 5 ? 7 : 2;
    std::uint16_t i = 0;
    if (Members() != 0) {
        do {
            At(::bof3::Img(WindowRecords_addr) + 0x90 + 3 + static_cast<std::uint32_t>(i) * menu_lists::at::kRecordSize)[0] = state;
            ++i;
        } while (i < static_cast<std::uint16_t>(Members()));
    }
    FieldS_State = static_cast<unsigned char>(FieldS_State + 1);
}

// original 0x589E00 (0x44 bytes), FieldMenu_TopBarSteps[1]: Menu_DrawBackdrop;
// the timer down, at 0 the screen of the top bar's entry: the mode = the
// entry + 2, the state, step and 0x929F03 0, the timer 5.
extern "C" void __cdecl FieldMenu_TopBarCountdown(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    const auto t = static_cast<unsigned char>(BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) - 1);
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = t;
    if (t != 0) return;
    const auto mode = static_cast<unsigned char>(static_cast<unsigned char>(MenuLists_Cursor) + 2);
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = 5;
    BattleE1_Mode = mode;
    FieldS_State = 0;
    FieldS_Step = 0;
    Rest2d_MenuSub = 0;
}

// original 0x589E50 (13 bytes): the backdrop alone - the top bar's step 2,
// and a slot of the table at 0x66734C (0x667350) another screen steps
// through.
extern "C" void __cdecl FieldMenu_BackdropStep(void) { g.backdrop(Config_Background); }

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
            unsigned char* const r = At(::bof3::Img(WindowRecords_addr) + 0x90 + i * menu_lists::at::kRecordSize);
            r[0xA] = i;
            r[1] = 6;
            r[2] = 0;
            r[3] = 0;
            r[0] = 1;
            rv::As<records::WindowRecord>(r)->x = 0x62;
            rv::As<records::WindowRecord>(r)->y = i * 0x36u + 0x3E;
            r[0xB] = 0;
            r[0xC] = 0;
            ++i;
        } while (i < Members());
    }
    struct Place { std::uint32_t at; unsigned char kind; std::uint16_t x, y; };
    static const Place kPlaces[] = {{::bof3::Img(Window7_addr), 1, 0xC8, 0x10},
                                        {::bof3::Img(Window8_addr), 2, 0x10, 0x10},
                                        {::bof3::Img(Window9_addr), 3, 0x68, 0x2A},
                                        {::bof3::Img(Window10_addr), 4, 0x73, 0x10}};
    for (const Place& p : kPlaces) {
        unsigned char* const r = At(p.at);
        r[1] = 6;
        r[2] = p.kind;
        r[3] = 0;
        r[0] = 1;
        rv::As<records::WindowRecord>(r)->x = p.x;
        rv::As<records::WindowRecord>(r)->y = p.y;
    }
    Byte(::bof3::Img(menu_lists::at::kIconWindow.v) + 0xA) = 0;
    Byte(::bof3::Img(menu_lists::at::kIconWindow.v) + 0xB) = 0;
    Byte(::bof3::Img(menu_lists::at::kIconWindow.v) + 0xC) = 0;
    SetWord(At(::bof3::Img(Window9_addr) + 0x10), 0);
    SetWord(At(::bof3::Img(Window10_addr) + 0x10), 0);
}

// original 0x589FB0 (0x2F bytes): al 0 when the leader's cell (AreaMap_ByteAt of
// ObjTrio's x / z words) masked 0xF0 is 0xA0 - the original also compares
// 0xA1, 0xAF and 0x91, which the mask makes unreachable (docs/rest_2d.md
// section 5) - else 1.
extern "C" unsigned char __cdecl FieldMenu_CampAllowedCell(void) {
    const unsigned char cell =
        static_cast<unsigned char>(SH_CALL(AreaMap_ByteAt)(static_cast<short>(Word(rv::Bytes(ObjTrio) + 0x36)), static_cast<short>(Word(rv::Bytes(ObjTrio) + 0x3A))) & 0xF0);
    if (cell == 0xA0 || cell == 0xA1 || cell == 0xAF || cell == 0x91) return 0;
    return 1;
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
        const unsigned char m = At(::bof3::Img(menu_lists::at::kSavedParty.v))[k];
        if (m == 0xFF) continue;
        unsigned char add = 1;
        for (unsigned j = 0; j < 3; ++j) {
            const unsigned char p = Cond_Flags[0xd2 + j];
            if (p != 0xFF && m == p) {
                add = 0;
                break;
            }
        }
        missing = static_cast<unsigned char>(missing + add);
    }
    if (missing != 0) MenuLists_PartyChanged = 1;
    unsigned char before[9];
    for (unsigned b = 0; b < 9; ++b) before[b] = At(::bof3::Img(menu_lists::at::kReserveData.v))[b];
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned char m = Cond_Flags[0xd5 + k];
        if (m == 0xFF) continue;
        unsigned char* const out = At(::bof3::Img(menu_lists::at::kReserveData.v) + 3 * k);
        unsigned j = 0;
        for (; j < 3; ++j) {
            const unsigned char s = At(::bof3::Img(menu_lists::at::kSavedReserve.v))[j];
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

void FieldMenu_Inject() {
    BOF3_INJECT(FieldMenu_Run);
    BOF3_INJECT(FieldMenu_Open);
    BOF3_INJECT(FieldMenu_TopBar);
    BOF3_INJECT(FieldMenu_TopBarInput);
    BOF3_INJECT(FieldMenu_TopBarCountdown);
    BOF3_INJECT(FieldMenu_BackdropStep);
    BOF3_INJECT(FieldMenu_PlaceWindows);
    BOF3_INJECT(FieldMenu_CampAllowedCell);
    BOF3_INJECT(FieldMenu_ReconcileParty);
}
