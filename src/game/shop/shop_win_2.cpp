// Moved here by tools/move_funcs.py from menu_draw_helpers.cpp, menu_windows.cpp, rest_2h.cpp (docs/source-layout-round.md).
#include "game/shop/shop_win_2.h"

#include <cstdint>
#include <cstring>
#include <dinput.h>

#include "bof3/symbols.gen.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_draw_helpers.h"
#include "game/menu_draw_helpers_callees.h"
#include "game/menu_frame.h"
#include "game/menu_verbs.h"
#include "game/menu_windows.h"
#include "game/menu_windows_callees.h"
#include "game/move_script_bytes.h"
#include "game/msgbox.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2h.h"
#include "game/rest_2h_callees.h"
#include "game/scenario_harness.h"
#include "game/shoulder_glyphs.h"
#include "game/text_advance.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_2h::at;

}  // namespace

namespace {

using namespace menu_draw_helpers;

}  // namespace

using namespace menu_windows;

namespace {

constexpr auto& AtMenuDrawHelpers = move_script::At;

constexpr auto& LongMenuDrawHelpers = move_script::Long;

constexpr auto& WordMenuDrawHelpers = move_script::Word;

using Handler = void (__cdecl*)();

unsigned char* RecMenuDrawHelpers() {
    return AtMenuDrawHelpers(*reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr))));
}

// rv::As<records::WindowRecord>(p) views a window record
// (docs/typed-records.md; WindowRecords
// 0x803160, the records BattleWinStates_Current points at). Only +0..+7 are
// typed: +8..+0x1F mean something else in each handler (typed-records 3).
short S(const unsigned char* p) { return static_cast<short>(WordMenuDrawHelpers(p)); }

unsigned X(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->x; }

unsigned Y(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->y; }

// `call / jmp [table + index * 4]`: the .data dword read at the call, the
// index unbounded as in the original (a byte, zero-extended).
void Dispatch(menu_draw_helpers::U table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<menu_draw_helpers::U>(LongMenuDrawHelpers(AtMenuDrawHelpers(table + index * 4)))))();
}

// The step: the current record's byte +3 through a step table.
void Step(menu_draw_helpers::U table) { Dispatch(table, RecMenuDrawHelpers()[3]); }

// A party slot's character record: 0x66972C[0x904062[slot]], both bytes, the
// slot unbounded.
unsigned Member(unsigned slot) { return AtMenuDrawHelpers(::bof3::Img(Character_IndexById_addr) + AtMenuDrawHelpers(::bof3::Img(Cond_Flags_addr) + 0xd2 + slot)[0])[0]; }

// The slides: x (+4) or y (+6) stepped by `by`; past `bound` (signed 16-bit;
// `below` says which side) it is set to `to` and the step +3 goes to 0.
void SlideX(int by, short bound, bool below, short to) {
    unsigned char* const w = RecMenuDrawHelpers();
    rv::As<records::WindowRecord>(w)->x = static_cast<unsigned>(rv::As<records::WindowRecord>(w)->x + by);
    const short x = S(w + 4);
    if (below ? x < bound : x > bound) {
        rv::As<records::WindowRecord>(w)->x = static_cast<std::uint16_t>(to);
        w[3] = 0;
    }
}

void SlideY(int by, short bound, bool below, short to) {
    unsigned char* const w = RecMenuDrawHelpers();
    rv::As<records::WindowRecord>(w)->y = static_cast<unsigned>(rv::As<records::WindowRecord>(w)->y + by);
    const short y = S(w + 6);
    if (below ? y < bound : y > bound) {
        rv::As<records::WindowRecord>(w)->y = static_cast<std::uint16_t>(to);
        w[3] = 0;
    }
}

}  // namespace

namespace menu_windows {

using move_script::At;

}  // namespace menu_windows

namespace {

using move_script::Long;

std::uint32_t Addr(const void* p) { return Low32(p); }

unsigned char* Ptr(std::uint32_t at) { return menu_windows::At(static_cast<std::uint32_t>(move_script::Long(menu_windows::At(at)))); }

unsigned char* Next() { return Gfx_PacketNext; }

char* PrintBufMenuWindows() { return reinterpret_cast<char*>(menu_windows::At(::bof3::Img(menu_windows::at::kPrintBuf.v))); }

const char* FormatMenuWindows(std::uint32_t at) { return reinterpret_cast<const char*>(menu_windows::At(at)); }

// The draw mode with the page GetTPage(0, 0, page_x, 0x100) that the piece
// draws open with.
void PiecePage(int page_x) {
    const unsigned tpage = menu_windows::g.get_tpage(0, 0, page_x, 0x100);
    menu_windows::g.draw_mode(Next(), 0, 0, tpage & 0xFFFF, 0);
    menu_windows::g.commit(1, 0xC);
}

// The detail windows' frame: page (0x380, 0x100); 20 pieces 0x29 along the top
// and 0x3B along y + bottom; `rows` pieces 0x2B and 0x2C down the sides at x
// and x + 0xA8; the corners 0x28, 0x2A, 0x3A, 0x3C.
void DetailFrame(const unsigned char* w, int bottom, unsigned rows) {
    PiecePage(0x380);
    for (unsigned b = 0; b < 0x14; ++b) {
        menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + static_cast<int>(b * 8) + 8, rv::As<records::WindowRecord>(w)->y, 0x29, 0);
        menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + static_cast<int>(b * 8) + 8, rv::As<records::WindowRecord>(w)->y + bottom, 0x3B, 0);
    }
    for (unsigned b = 0; b < rows; ++b) {
        menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x, rv::As<records::WindowRecord>(w)->y + static_cast<int>(b * 8) + 8, 0x2B, 0);
        menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + 0xA8, rv::As<records::WindowRecord>(w)->y + static_cast<int>(b * 8) + 8, 0x2C, 0);
    }
    menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x, rv::As<records::WindowRecord>(w)->y, 0x28, 0);
    menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + 0xA8, rv::As<records::WindowRecord>(w)->y, 0x2A, 0);
    menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x, rv::As<records::WindowRecord>(w)->y + bottom, 0x3A, 0);
    menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + 0xA8, rv::As<records::WindowRecord>(w)->y + bottom, 0x3C, 0);
}

using U = std::uint32_t;

constexpr auto& AtRest2h = move_script::At;

constexpr auto& LongRest2h = move_script::Long;

using move_script::SetWord;

constexpr auto& WordRest2h = move_script::Word;

unsigned char& BRest2h(U address) { return *AtRest2h(address); }

U LRest2h(U address) { return static_cast<U>(LongRest2h(AtRest2h(address))); }

int S8Rest2h(unsigned v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

// The current window record, re-read at every use (the originals reload
// 0x905B84 after each call).
unsigned char* RecRest2h() { return AtRest2h(*reinterpret_cast<volatile U*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr)))); }

int XRest2h(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->x; }

int YRest2h(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->y; }

unsigned char Style() { return static_cast<unsigned char>(Config_WindowColour); }

// The step: the current record's +3 through a step table, called through the
// word the table holds (the fuzz swaps the words for recorders); past the
// table's own run of handlers the original jumps through whatever follows,
// ours aborts.
void StepRest2h(const char* who, U table, unsigned entries) {
    const unsigned index = RecRest2h()[3];
    if (index >= entries)
        bof3::Fatal("%s: the window's step +3 is %u, past the %u entries of 0x%X - the original jumps through the "
                    "dword after (docs/rest_2h.md section 7)",
                    who, index, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(LRest2h(table + 4 * index)))();
}

// A slide: x (+4) or y (+6) moved by `by`; past `bound` (signed 16-bit; `below`
// says which side) it is set to `to` and the step +3 goes to 0. The originals
// re-read 0x905B84 between the stores with nothing called in between.
void Slide(unsigned at_word, int by, short bound, bool below, short to) {
    unsigned char* const w = RecRest2h();
    move_script::SetWord(w + at_word, static_cast<unsigned>(WordRest2h(w + at_word) + by));
    const auto v = static_cast<short>(WordRest2h(w + at_word));
    if (below ? v < bound : v > bound) {
        move_script::SetWord(w + at_word, static_cast<std::uint16_t>(to));
        w[3] = 0;
    }
}

// CharacterRecords' record n (0xA4 each), unchecked; a member id's record
// index, 0x66972C[id] read in place (the byte is unbounded, the table is 24).
unsigned char* Record(U n) { return AtRest2h(::bof3::Img(CharacterRecords_addr) + n * rest_2h::at::kRecordStride); }

const unsigned char* TextRest2h(U address) { return AtRest2h(address); }

const unsigned char* Message(unsigned id) { return SH_CALL(Msg_SystemPtr)(id & 0xFFFF); }

void PutFloat(unsigned char* at, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

void Piece(int x, int y, unsigned id) { SH_CALL(Menu_DrawPiece)(x, y, id, 1); }

void Pieces(int x, int y, U list) { SH_CALL(Menu_DrawPieces)(x, y, TextRest2h(list), 1); }

}  // namespace

// original 0x59B470, kind 5 step 3: y -= 0x10; below 0x3E, y = 0x3E and step 0.
extern "C" void __cdecl ShopWin_MemberSlideUp(void) { SlideY(-0x10, 0x3E, true, 0x3E); }

// original 0x59B4A0, kind 5 step 4: y += 0x10; above +0xA * 0x36 + 0x3E (the
// y comparing as a signed word, the bound as the 32-bit sum), y = that sum's
// low word and step 0 - the panel for party slot +0xA, 0x36 below the last.
extern "C" void __cdecl ShopWin_MemberSlideDown(void) {
    unsigned char* const w = RecMenuDrawHelpers();
    rv::As<records::WindowRecord>(w)->y = static_cast<unsigned>(rv::As<records::WindowRecord>(w)->y + 0x10);
    const int bound = static_cast<int>(w[0xA]) * 0x36 + 0x3E;
    if (static_cast<int>(S(w + 6)) > bound) {
        rv::As<records::WindowRecord>(w)->y = static_cast<unsigned>(bound);
        w[3] = 0;
    }
}

// original 0x59B4F0, kind 6: the step (ShopWin_EquipSteps: a bare ret,
// MenuWin_SlideOutLeft, ShopWin_EquipSlideIn), then Menu_DrawEquipPanel(+4,
// +6, the character record of party slot +0xA). Record re-read after the step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_EquipRun(void) {
    Step(::bof3::Img(ShopWin_EquipSteps_addr));
    const unsigned char* const w = RecMenuDrawHelpers();
    const unsigned member = Member(w[0xA]);
    menu_draw_helpers::g.equip_panel(static_cast<int>(X(w)), static_cast<int>(Y(w)), member);
}

// original 0x59B530, kind 6 step 2: x += 0x20; above 0xF, x = 0xF and step 0.
extern "C" void __cdecl ShopWin_EquipSlideIn(void) { SlideX(0x20, 0xF, false, 0xF); }

// original 0x59B560, kind 7: the step (ShopWin_BuyListSteps: a bare ret,
// 0x59A5E0 - DH's, ShopWin_BuyListSlideTo84, ShopWin_BuyListSlideTo46), then
// Shop_DrawBuyList(the record, re-read after the step).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_BuyListRun(void) {
    Step(::bof3::Img(ShopWin_BuyListSteps_addr));
    menu_draw_helpers::g.buy_list(RecMenuDrawHelpers());
}

// original 0x59B580 (PSX 0x801E2DBC): a shop's buy list. The window record:
// +4 / +6 x and y, +0xA greyed, +0xB the price percent, +0xC the cursor, dword
// +0x20 the shop's list (a count, then (category, id) pairs). The box (x + 3,
// y + 3, 0xAD, 0x9F) greyed as +0xA; each entry 13 apart: its icon, name and
// "%6dZ" of the price scaled (0x5830D0), colour 7 (and a dim icon) when greyed
// or the price is more than the money 0x904058; the cursor's row 2 higher
// over a shadow in colour 7 (none when greyed or dear). The count is read
// again after each entry. Then the border (x, y, 0x14, 0x13).
extern "C" void __cdecl Shop_DrawBuyList(unsigned char* window) {
    unsigned char* const w = window;
    menu_windows::g.box(rv::As<records::WindowRecord>(w)->x + 3, rv::As<records::WindowRecord>(w)->y + 3, 0xAD, 0x9F, w[0xA], static_cast<unsigned char>(Config_WindowColour));
    const unsigned char* const list = Ptr(Addr(w + 0x20));
    std::uint32_t text_y = static_cast<std::uint16_t>(rv::As<records::WindowRecord>(w)->y + 8);
    std::uint32_t row_y = text_y + 3;
    if (list[0] != 0) {
        unsigned char i = 0;
        do {
            const unsigned char category = list[1 + 2 * i], id = list[2 + 2 * i];
            const unsigned percent = w[0xB];   // pushed before Item_BasePrice, for the scale after it
            const unsigned price = menu_windows::g.item_price(category, id) & 0xFFFF;
            const std::uint32_t cost = menu_windows::g.price_scale(price, percent);
            const unsigned char kind = static_cast<unsigned char>(menu_windows::g.item_kind(category, id));
            const unsigned char* const name = menu_windows::g.item_name(category, id);
            unsigned char colour, dim;
            if (w[0xA] == 0 && Party_Zenny >= cost) {
                colour = 0;
                dim = 0;
            } else {
                colour = 7;
                dim = 1;
            }
            menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmt6dZ.v)), cost);
            if (i == w[0xC]) {
                if (colour == 0) {
                    menu_windows::g.icon8(rv::As<records::WindowRecord>(w)->x + 4, static_cast<int>(row_y), At(::bof3::Img(menu_windows::at::kBuyIcons.v))[kind], 1);
                    const unsigned n = menu_windows::g.char_count(name);
                    menu_windows::g.text_draw_at(rv::As<records::WindowRecord>(w)->x + 0xE, static_cast<int>(text_y), 7, static_cast<int>(n), name);
                    menu_windows::g.font8(rv::As<records::WindowRecord>(w)->x + 0x71, static_cast<int>(row_y), 7,
                            reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
                }
                text_y -= 2;
                row_y -= 2;
            }
            menu_windows::g.icon8(rv::As<records::WindowRecord>(w)->x + 4, static_cast<int>(row_y), At(::bof3::Img(menu_windows::at::kBuyIcons.v))[kind], dim);
            const unsigned n = menu_windows::g.char_count(name);
            menu_windows::g.text_draw_at(rv::As<records::WindowRecord>(w)->x + 0xE, static_cast<int>(text_y), colour, static_cast<int>(n), name);
            menu_windows::g.font8(rv::As<records::WindowRecord>(w)->x + 0x71, static_cast<int>(row_y), colour,
                    reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
            if (i == w[0xC]) {
                text_y += 2;
                row_y += 2;
            }
            text_y += 0xD;
            row_y += 0xD;
            ++i;
        } while (i < list[0]);
    }
    menu_windows::g.border(rv::As<records::WindowRecord>(w)->x, rv::As<records::WindowRecord>(w)->y, 0x14, 0x13);
}

// original 0x59B7B0, kind 7 step 2: x -= 0x20; below 0x84, x = 0x84 and step 0.
extern "C" void __cdecl ShopWin_BuyListSlideTo84(void) { SlideX(-0x20, 0x84, true, 0x84); }

// original 0x59B7E0, kind 7 step 3: x -= 0x20; below 0x46, x = 0x46 and step 0.
extern "C" void __cdecl ShopWin_BuyListSlideTo46(void) { SlideX(-0x20, 0x46, true, 0x46); }

// original 0x59B810, kind 8: Shop_DrawBuyDetail(the record). No step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_BuyDetailRun(void) { menu_draw_helpers::g.buy_detail(RecMenuDrawHelpers()); }

// original 0x59B820 (PSX 0x801E313C): the buy list's detail - the entry at
// +0xB of the list: the box (x + 3, y + 3, 0xAA, 0x3C), its icon, name,
// "%7dZ" of quantity (+0xC) times the price scaled by +0xA percent, "*%2d" of
// the quantity, system messages 0x27 and 0x28 with "*%2d" of the count held
// and equipped (0x5919B0 where 0 and 1), then the frame 7 cells deep.
extern "C" void __cdecl Shop_DrawBuyDetail(unsigned char* window) {
    unsigned char* const w = window;
    const unsigned char* const list = Ptr(Addr(w + 0x20));
    const unsigned sel = w[0xB];
    const unsigned char category = list[1 + 2 * sel], id = list[2 + 2 * sel];
    const unsigned percent = w[0xA];
    const unsigned price = menu_windows::g.item_price(category, id) & 0xFFFF;
    const std::uint32_t cost = menu_windows::g.price_scale(price, percent);
    const unsigned char* const name = menu_windows::g.item_name(category, id);
    menu_windows::g.box(rv::As<records::WindowRecord>(w)->x + 3, rv::As<records::WindowRecord>(w)->y + 3, 0xAA, 0x3C, 0, static_cast<unsigned char>(Config_WindowColour));
    const unsigned kind = menu_windows::g.item_kind(category, id) & 0xFF;
    menu_windows::g.icon8(rv::As<records::WindowRecord>(w)->x + 4, rv::As<records::WindowRecord>(w)->y + 7, At(::bof3::Img(menu_windows::at::kBuyDetailIcons.v))[kind], 0);
    const unsigned n = menu_windows::g.char_count(name);
    menu_windows::g.text_draw_at(rv::As<records::WindowRecord>(w)->x + 0xE, rv::As<records::WindowRecord>(w)->y + 4, 0, static_cast<int>(n), name);
    menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmt7dZ.v)), w[0xC] * cost);
    menu_windows::g.font8(rv::As<records::WindowRecord>(w)->x + 0x69, rv::As<records::WindowRecord>(w)->y + 0x12, 0, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
    menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmtCount.v)), static_cast<unsigned>(w[0xC]));
    menu_windows::g.font8(rv::As<records::WindowRecord>(w)->x + 0x89, rv::As<records::WindowRecord>(w)->y + 7, 0, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
    const unsigned char* const held = menu_windows::g.msg_system_ptr(0x27);
    menu_windows::g.text_draw_at(rv::As<records::WindowRecord>(w)->x + 0xE, rv::As<records::WindowRecord>(w)->y + 0x20, 0, 0xFF, held);
    const unsigned owned = menu_windows::g.count_owned(category, id, 0) & 0xFFFF;
    menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmtCount.v)), owned);
    menu_windows::g.font8(rv::As<records::WindowRecord>(w)->x + 0x89, rv::As<records::WindowRecord>(w)->y + 0x23, 0, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
    const unsigned char* const equipped = menu_windows::g.msg_system_ptr(0x28);
    menu_windows::g.text_draw_at(rv::As<records::WindowRecord>(w)->x + 0xE, rv::As<records::WindowRecord>(w)->y + 0x2D, 0, 0xFF, equipped);
    const unsigned worn = menu_windows::g.count_owned(category, id, 1) & 0xFFFF;
    menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmtCount.v)), worn);
    menu_windows::g.font8(rv::As<records::WindowRecord>(w)->x + 0x89, rv::As<records::WindowRecord>(w)->y + 0x30, 0, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
    DetailFrame(w, 0x38, 6);
}

// original 0x59BB60, kind 9: the step (ShopWin_ItemListSteps: a bare ret,
// 0x59A5E0 - DH's, ShopWin_ItemListSlideIn), then Menu_DrawItemList(the
// record, re-read after the step).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_ItemListRun(void) {
    Step(::bof3::Img(ShopWin_ItemListSteps_addr));
    menu_draw_helpers::g.item_list(RecMenuDrawHelpers());
}

// original 0x59BB80, kind 9 step 2: x -= 0x20; below 0x4B, x = 0x4B and step 0.
extern "C" void __cdecl ShopWin_ItemListSlideIn(void) { SlideX(-0x20, 0x4B, true, 0x4B); }

// original 0x59BBB0, kind 10: Shop_DrawSellDetail(the record). No step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_SellDetailRun(void) { menu_draw_helpers::g.sell_detail(RecMenuDrawHelpers()); }

// original 0x59BBC0: the sell detail - item (+0xA category, +0xB id): the box
// (x + 3, y + 3, 0xAA, 0x1C), its icon, name, "%6dZ" of the price a shop pays
// (0x583100 with +0xD), "%7dZ" of it times the quantity +0xC and "*%2d" of the
// quantity, then the frame 3 cells deep.
extern "C" void __cdecl Shop_DrawSellDetail(unsigned char* window) {
    unsigned char* const w = window;
    const unsigned char id = w[0xB], category = w[0xA], flag = w[0xD];
    const std::uint32_t unit = menu_windows::g.sell_price(category, id, flag);
    const unsigned char* const name = menu_windows::g.item_name(category, id);
    menu_windows::g.box(rv::As<records::WindowRecord>(w)->x + 3, rv::As<records::WindowRecord>(w)->y + 3, 0xAA, 0x1C, 0, static_cast<unsigned char>(Config_WindowColour));
    const unsigned kind = menu_windows::g.item_kind(category, id) & 0xFF;
    menu_windows::g.icon8(rv::As<records::WindowRecord>(w)->x + 7, rv::As<records::WindowRecord>(w)->y + 6, At(::bof3::Img(menu_windows::at::kSellIcons.v))[kind], 0);
    const unsigned n = menu_windows::g.char_count(name);
    menu_windows::g.text_draw_at(rv::As<records::WindowRecord>(w)->x + 0x11, rv::As<records::WindowRecord>(w)->y + 4, 0, static_cast<int>(n), name);
    menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmt6dZ.v)), unit);
    menu_windows::g.font8(rv::As<records::WindowRecord>(w)->x + 0x71, rv::As<records::WindowRecord>(w)->y + 7, 0, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
    menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmt7dZ.v)), w[0xC] * unit);
    menu_windows::g.font8(rv::As<records::WindowRecord>(w)->x + 0x69, rv::As<records::WindowRecord>(w)->y + 0x12, 0, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
    menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmtCount.v)), static_cast<unsigned>(w[0xC]));
    menu_windows::g.font8(rv::As<records::WindowRecord>(w)->x + 0xE, rv::As<records::WindowRecord>(w)->y + 0x12, 0, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
    DetailFrame(w, 0x18, 2);
}

// original 0x59BE50, kind 11: the step (ShopWin_SharedListSteps: a bare ret,
// MenuSlide_RightOff, 0x59AA50 - R2G's, ShopWin_SharedListSlideTo28, 0x596920 -
// R2F's), then SharedList_DrawList(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_SharedListRun(void) {
    StepRest2h("ShopWin_SharedListRun", ::bof3::Img(ShopWin_SharedListSteps_addr), 5);
    SH_CALL(SharedList_DrawList)(RecRest2h());
}

// original 0x59BE70, kind 11 step 3: x += 0x20; above 0x28, x = 0x28 and step 0.
extern "C" void __cdecl ShopWin_SharedListSlideTo28(void) { Slide(4, 0x20, 0x28, false, 0x28); }

// original 0x59BEA0, kind 12: the step (ShopWin_SharedMemberSteps: a bare ret,
// MenuSlide_RightOff, 0x59AA50, 0x59A960, 0x59A9E0 - R2G's), then
// SharedList_DrawMember(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_SharedMemberRun(void) {
    StepRest2h("ShopWin_SharedMemberRun", ::bof3::Img(ShopWin_SharedMemberSteps_addr), 5);
    SH_CALL(SharedList_DrawMember)(RecRest2h());
}

// original 0x59BEC0, kind 13: the step (ShopWin_MemberStatusSteps: a bare ret,
// MenuWin_SlideOutLeft, MenuSlide_RightTo17), then Menu_DrawMemberStatus(+4,
// +6, +0xA, 0) of the record re-read - pushed with a fifth word 0, which the
// callee does not read.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_MemberStatusRun(void) {
    StepRest2h("ShopWin_MemberStatusRun", ::bof3::Img(ShopWin_MemberStatusSteps_addr), 3);
    const unsigned char* const w = RecRest2h();
    using Five = unsigned long (__cdecl*)(int, int, unsigned, unsigned, unsigned);
    reinterpret_cast<Five>(SH_CALL(Menu_DrawMemberStatus))(XRest2h(w), YRest2h(w), w[0xA], 0, 0);
}

// original 0x59BF00, kind 14: ShopWin_DrawRowMenu(the record). No step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_RowMenuRun(void) {
    SH_CALL(ShopWin_DrawRowMenu)(RecRest2h());
}

// original 0x59BF10: the row menu of the window `w`: n = the byte 0x66B37C
// (read for the box and again before and after every row); the box (x + 3,
// y + 3, 0x65, 16 n + 0x19, 0, style); the title (0x66B36C's first pointer) at
// (x + 0x1E, y + 7, 0, 0xFF); the pieces 0x66B30C; for row i below n, the text
// 0x66B36C[0x66B37D[i]] at (x + 7, y + 16 i + 0x1A): the row +0xB lit (colour
// 7, then two up in colour 2), the row +0xA likewise in 7 then 0, the others
// once in 0; the pieces 0x66B330 at (x, y + 16 i + 0x18) and (x, y + 16 i +
// 0x20); last the pieces 0x66B33C at (x, y + 16 n + 0x18).
extern "C" void __cdecl ShopWin_DrawRowMenu(unsigned char* w) {
    const unsigned n0 = Rest2h_RowCount;
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 3, 0x65, static_cast<int>((n0 << 4) + 0x19), 0, style);
    SH_CALL(Text_DrawAt)(XRest2h(w) + 0x1E, YRest2h(w) + 7, 0, 0xFF, AtRest2h(LRest2h(::bof3::Img(rest_2h::at::kRowText.v))));
    Pieces(XRest2h(w), YRest2h(w), ::bof3::Img(rest_2h::at::kRowPiecesA.v));
    unsigned char i = 0;
    if (Rest2h_RowCount != 0) {
        do {
            const unsigned char* const text = AtRest2h(LRest2h(::bof3::Img(rest_2h::at::kRowText.v) + 4u * BRest2h(::bof3::Img(rest_2h::at::kRowOrder.v) + i)));
            const int dy = static_cast<int>(i) << 4;
            if (i == w[0xB]) {
                SH_CALL(Text_DrawAt)(XRest2h(w) + 7, YRest2h(w) + dy + 0x1A, 7, 0xFF, text);
                SH_CALL(Text_DrawAt)(XRest2h(w) + 7, YRest2h(w) + dy + 0x18, 2, 0xFF, text);
            } else if (i == w[0xA]) {
                SH_CALL(Text_DrawAt)(XRest2h(w) + 7, YRest2h(w) + dy + 0x1A, 7, 0xFF, text);
                SH_CALL(Text_DrawAt)(XRest2h(w) + 7, YRest2h(w) + dy + 0x18, 0, 0xFF, text);
            } else {
                SH_CALL(Text_DrawAt)(XRest2h(w) + 7, YRest2h(w) + dy + 0x1A, 0, 0xFF, text);
            }
            SH_CALL(Menu_DrawPieces)(XRest2h(w), YRest2h(w) + dy + 0x18, TextRest2h(::bof3::Img(rest_2h::at::kRowPiecesB.v)), 1);
            SH_CALL(Menu_DrawPieces)(XRest2h(w), ((static_cast<int>(i) + 2) << 4) + YRest2h(w), TextRest2h(::bof3::Img(rest_2h::at::kRowPiecesB.v)), 1);
            ++i;
        } while (i < Rest2h_RowCount);
    }
    SH_CALL(Menu_DrawPieces)(XRest2h(w), (static_cast<int>(i) << 4) + YRest2h(w) + 0x18, TextRest2h(::bof3::Img(rest_2h::at::kRowPiecesC.v)), 1);
}

// original 0x59C110, kind 15: the step (ShopWin_MasterListSteps: a bare ret,
// MasterWin_SlideOut, MasterWin_SlideTo64, 0x59A960 - R2G's), then
// MasterWin_DrawList(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_MasterListRun(void) {
    StepRest2h("ShopWin_MasterListRun", ::bof3::Img(ShopWin_MasterListSteps_addr), 4);
    SH_CALL(MasterWin_DrawList)(RecRest2h());
}

namespace rest_2h {
std::uint32_t g_master_bound = Low32(Widescreen_SlideOperand(at::kMasterBound.v));
// DIVERGENCE DIV-0059: the master list's title centred on the width its pen
// covers, under a Latin overlay (ListTitle_Centring). Set by Rest2H_Inject
// after the fuzz, which compares with the original's 6 * count arithmetic.
bool g_master_title_live = false;
// DIVERGENCE DIV-0083: the pupils box's label (弟子, 0x66A1F8) and its own
// box are the 2001 port's addition - the PlayStation's screen has the
// portrait box alone (the owner's US capture, 2026-10-07). 0 until
// Rest2H_Inject patches it to 1 under a Latin overlay, after the fuzz, under
// the name MasterPupilLabel; BOF3X_ORIGINAL=MasterPupilLabel keeps the box.
unsigned char g_pupil_label_off = 0;
}  // namespace rest_2h

// original 0x59C130, kind 15 step 1: x -= 0x20; below the bound (-120), x = the
// bound and step 0. The bound is the imm32 at 0x59C136, which Widescreen_Inject
// (DIV-0041) moves out by its columns; ours reads the low word of
// widescreen.cpp's value for it (g_master_bound's address) at every call, as
// the original's `cmp word [eax + 4], cx` reads the operand, so BOF3X_ORIGINAL=
// Widescreen still restores -120.
//
// DIVERGENCE DIV-0041 (amended 2026-10-13, the faster slide): decided by that
// bound at every width; in the wide view the box is drawn to the bound moved
// out by the columns (Widescreen_SlideHeld, docs/ultrawide-slides.md).
extern "C" void __cdecl MasterWin_SlideOut(void) {
    const auto bound = Widescreen_SlideLogic(static_cast<short>(WordRest2h(AtRest2h(rest_2h::g_master_bound))));
    const auto x_read = static_cast<short>(rv::As<records::WindowRecord>(RecRest2h())->x);
    Slide(4, -0x20, bound, true, bound);
    Widescreen_SlideHeld(RecRest2h(), x_read, bound);
}

// original 0x59C160, kind 15 step 2: x += 0x20; above 0x64, x = 0x64 and step 0.
extern "C" void __cdecl MasterWin_SlideTo64(void) { Slide(4, 0x20, 0x64, false, 0x64); }

// original 0x59C190, kind 16: the step (ShopWin_MasterCaptionSteps: a bare ret,
// MenuSlide_RightOff, MasterWin_CaptionSlideTo8C), then
// MasterWin_DrawCaption(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_MasterCaptionRun(void) {
    StepRest2h("ShopWin_MasterCaptionRun", ::bof3::Img(ShopWin_MasterCaptionSteps_addr), 3);
    SH_CALL(MasterWin_DrawCaption)(RecRest2h());
}

// original 0x59C1B0, kind 16 step 2: x -= 0x20; below 0x8C, x = 0x8C and step 0.
extern "C" void __cdecl MasterWin_CaptionSlideTo8C(void) { Slide(4, -0x20, 0x8C, true, 0x8C); }

// original 0x59C1E0, kind 17: the step (ShopWin_PupilsSteps: a bare ret,
// MasterWin_PupilsSlideDown, MasterWin_PupilsSlideUp), then
// MasterWin_DrawPupils(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_PupilsRun(void) {
    StepRest2h("ShopWin_PupilsRun", ::bof3::Img(ShopWin_PupilsSteps_addr), 3);
    SH_CALL(MasterWin_DrawPupils)(RecRest2h());
}

// original 0x59C200, kind 17 step 1: y += 0x10; above 0xF0, y = 0xF0 and step 0.
extern "C" void __cdecl MasterWin_PupilsSlideDown(void) { Slide(6, 0x10, 0xF0, false, 0xF0); }

// original 0x59C230, kind 17 step 2: y -= 0x10; below 0x80, y = 0x80 and step 0.
extern "C" void __cdecl MasterWin_PupilsSlideUp(void) { Slide(6, -0x10, 0x80, true, 0x80); }

// original 0x59C260, kind 18: the step (ShopWin_ItemCountSteps: a bare ret,
// MenuSlide_RightOff, ShopWin_ItemCountSlideToD2), then
// SharedList_DrawItemCount(+4, +6) of the record re-read.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_ItemCountRun(void) {
    StepRest2h("ShopWin_ItemCountRun", ::bof3::Img(ShopWin_ItemCountSteps_addr), 3);
    const unsigned char* const w = RecRest2h();
    SH_CALL(SharedList_DrawItemCount)(XRest2h(w), YRest2h(w));
}

// original 0x59C290, kind 18 step 2: x -= 0x20; below 0xD2, x = 0xD2 and step 0.
extern "C" void __cdecl ShopWin_ItemCountSlideToD2(void) { Slide(4, -0x20, 0xD2, true, 0xD2); }

// original 0x59C2C0: the masters' list of the window `w`. Its own list (17
// bytes on its stack): for each of the 17 bits of 0x904657.. that is set, the
// bit's number + 1 - the entry whose place is +0xB also stores its bit number
// in +0xD (0xFF when none is), the entry at +0xC is remembered - then 0s. The
// box (x + 3, y + 3, 0x71, 0x9A, 0, style); Menu_ListScroll(+0xA, &offset,
// &moving, +8); from the answered top while below top + moving + 9, each
// non-empty entry e (an empty one skipped): MasterWin_Available(e - 1); the
// name, system message 0x110 + e, at (x + 0x13, y + offset + 0x1A + 13 row)
// and beside it at x + 5 either the mark 0x66A2D8 (available) or
// Menu_DrawIcon8(x + 8, that + 2, 6, dim) - the +0xB entry drawn lit first
// (colour 7, dim 1) and the rest two up, the +0xC entry in colour 2, others 0.
// The title and count boxes (0x71 wide); the title 0x66A1F0 at (x + 0x3A - 6
// its length, y + 7); the frame pieces; the scroll bar (the list, +0xA, x +
// 0x68, y + 0x18, 9, 0x11, 0x74).
//
// As the original has it: the list is a stack array of 17 zeroed bytes and
// the rows read it by top + row unbounded - past it the original reads its
// own stack. Menu_ListScroll keeps top + moving + 8 inside 17 for a top below
// 9 (menu_windows.cpp); ours aborts past the array (docs/rest_2h.md section
// 7).
extern "C" void __cdecl MasterWin_DrawList(unsigned char* w) {
    unsigned char list[0x11];
    unsigned char lit = 0x7F, picked = 0x7F;
    unsigned char n = 0;
    for (unsigned char i = 0; i < 0x11; ++i) {
        if (!(BRest2h(::bof3::Img(rest_2h::at::kMasterBits.v) + (i >> 3)) & static_cast<unsigned char>(1u << (i & 7)))) continue;
        if (w[0xB] == n) {
            lit = n;
            w[0xD] = i;
        }
        if (w[0xC] == n) picked = n;
        list[n] = static_cast<unsigned char>(i + 1);
        ++n;
    }
    if (n < 0x11) std::memset(list + n, 0, 0x11u - n);
    if (lit == 0x7F) w[0xD] = 0xFF;
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 3, 0x71, 0x9A, 0, style);
    unsigned char offset = 0, moving = 0;
    const unsigned char top = SH_CALL(Menu_ListScroll)(w + 0xA, &offset, &moving, w + 8);
    int text_y = S8Rest2h(offset) + YRest2h(w) + 0x1A;
    int icon_y = text_y + 2;
    const int end = static_cast<int>(moving) + static_cast<int>(top) + 9;
    for (unsigned char i = top; static_cast<int>(i) < end; ++i, text_y += 0xD, icon_y += 0xD) {
        if (i >= 0x11)
            bof3::Fatal("MasterWin_DrawList (0x59C2C0): row %u past its 17 entries (top %u, moving %u) - the original "
                        "reads its own stack there (docs/rest_2h.md section 7)",
                        (unsigned)i, (unsigned)top, (unsigned)moving);
        if (list[i] == 0) continue;
        const unsigned char available = SH_CALL(MasterWin_Available)(static_cast<unsigned char>(list[i] - 1));
        if (lit == i) {
            const unsigned char* const name = Message(list[i] + 0x110u);
            SH_CALL(Text_DrawAt)(XRest2h(w) + 0x13, text_y, 7, 0xFF, name);
            if (available == 0) SH_CALL(Menu_DrawIcon8)(XRest2h(w) + 8, icon_y, 6, 1);
            else SH_CALL(Text_DrawAt)(XRest2h(w) + 5, text_y, 7, 0xFF, TextRest2h(::bof3::Img(rest_2h::at::kMasterMark.v)));
            text_y -= 2;
            icon_y -= 2;
        }
        const int colour = picked == i ? 2 : 0;
        const unsigned char* const name = Message(list[i] + 0x110u);
        SH_CALL(Text_DrawAt)(XRest2h(w) + 0x13, text_y, colour, 0xFF, name);
        if (available == 0) SH_CALL(Menu_DrawIcon8)(XRest2h(w) + 8, icon_y, 6, 0);
        else SH_CALL(Text_DrawAt)(XRest2h(w) + 5, text_y, colour, 0xFF, TextRest2h(::bof3::Img(rest_2h::at::kMasterMark.v)));
        if (lit == i) {
            text_y += 2;
            icon_y += 2;
        }
    }
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 3, 0x71, 0x14, 0, Style());
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 0x92, 0x71, 8, 0, Style());
    // The title is centred on x + 0x3A for 12-unit glyphs: 6 a character
    // back from there. Under DIV-0059 (a Latin overlay; DIV-0064's MSTR is
    // four 8-unit letters) the same centre, less half the real width.
    const int title_y = YRest2h(w) + 7;
    const unsigned char length = SH_CALL(Text_CharCount)(TextRest2h(::bof3::Img(rest_2h::at::kMasterTitle.v)));
    const int title_x = rest_2h::g_master_title_live && ListTitle_Centring()
                            ? XRest2h(w) + 0x3A - static_cast<int>(TextAdvance_Width(TextRest2h(::bof3::Img(rest_2h::at::kMasterTitle.v))) / 2)
                            : XRest2h(w) - 6 * static_cast<int>(length) + 0x3A;
    SH_CALL(Text_DrawAt)(title_x, title_y, 0, 0x10, TextRest2h(::bof3::Img(rest_2h::at::kMasterTitle.v)));
    Pieces(XRest2h(w), YRest2h(w), ::bof3::Img(rest_2h::at::kMasterPiecesA.v));
    Pieces(XRest2h(w), YRest2h(w), ::bof3::Img(rest_2h::at::kMasterPiecesB.v));
    for (int i = 0; i < 5; ++i) Piece(8 * i + 0x28 + XRest2h(w), YRest2h(w), 1);
    for (int i = 0; i < 15; ++i) Piece(XRest2h(w), 8 * i + 0x18 + YRest2h(w), 4);
    Piece(XRest2h(w) + 0x68, YRest2h(w) + 0x18, 0x16);
    for (int i = 0; i < 12; ++i) Piece(XRest2h(w) + 0x68, 8 * i + 0x28 + YRest2h(w), 0x17);
    Piece(XRest2h(w) + 0x68, YRest2h(w) + 0x88, 0x18);
    Piece(XRest2h(w), YRest2h(w) + 0x90, 0x19);
    for (int i = 0; i < 12; ++i) Piece(8 * i + 8 + XRest2h(w), YRest2h(w) + 0x90, 0x1A);
    Piece(XRest2h(w) + 8, YRest2h(w), 0x32);
    Piece(XRest2h(w) + 0x68, YRest2h(w), 0x33);
    SH_CALL(Menu_DrawScrollBar)(list, w[0xA], XRest2h(w) + 0x68, YRest2h(w) + 0x18, 9, 0x11, 0x74);
}

// original 0x59C780: whether master `index` (a byte) is available - al 1 or 0
// (the rest of eax is not set). 0xB: 0x904061 bit 3; 0xC: story flag 0x6C
// (Flags_Test(0x904030, 0x6C)); 0xD: 0x904061 bit 4; 0xE: bit 5 (a jump table
// at 0x59C7FC). Any other: every skill of its list (MasterWin_Requirements
// [index], 0xFF-ended) known (MasterWin_SkillKnown), and 1 for an empty list.
// The table holds 17; the only caller passes an entry - 1 of its 17-entry list,
// so ours aborts past it where the original reads the next .data as pointers.
extern "C" unsigned char __cdecl MasterWin_Available(unsigned index) {
    const unsigned k = index & 0xFF;
    switch (k) {
    case 0xB: return (Cond_Flags[0xd1] & 8) ? 1 : 0;
    case 0xC: return SH_CALL(Flags_Test)(AtRest2h(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x6C) ? 1 : 0;
    case 0xD: return (Cond_Flags[0xd1] & 0x10) ? 1 : 0;
    case 0xE: return (Cond_Flags[0xd1] & 0x20) ? 1 : 0;
    default: break;
    }
    if (k >= rest_2h::at::kMasterCount)
        bof3::Fatal("MasterWin_Available (0x59C780): master %u past the %u lists of 0x%X - the original reads the next "
                    ".data as a pointer (docs/rest_2h.md section 7)",
                    k, rest_2h::at::kMasterCount, (unsigned)MasterWin_Requirements_addr);
    const unsigned char* p = AtRest2h(LRest2h(::bof3::Img(rest_2h::at::kRequirements.v) + 4 * k));
    while (*p != 0xFF) {
        if (SH_CALL(MasterWin_SkillKnown)(*p) == 0) return 0;
        ++p;
    }
    return 1;
}

// original 0x59C810: whether skill `skill` (a byte) is known - in one of the
// ten ability slots (+0x7E) of character records 0..6, or in the shared list
// 0x904574's 128 bytes: al 1, else al 0.
extern "C" unsigned char __cdecl MasterWin_SkillKnown(unsigned skill) {
    const auto s = static_cast<unsigned char>(skill);
    for (U k = 0; k < 7; ++k)
        for (U j = 0; j < 0xA; ++j)
            if (BRest2h(::bof3::Img(Field_ActorStates_addr) + 0x6e + k * rest_2h::at::kRecordStride + j) == s) return 1;
    for (U j = 0; j < 0x80; ++j)
        if (BRest2h(::bof3::Img(rest_2h::at::kSharedList.v) + j) == s) return 1;
    return 0;
}

// original 0x59C870: the master's caption of the window `w`: the box (x + 3,
// y + 3, 0x9A, 0x39, 0, style), the border Menu_DrawBorder(x, y, 0x12, 6), and
// system message 0x100 + +0xA at (x + 0xE, y + 4, 0, 0xFF).
extern "C" void __cdecl MasterWin_DrawCaption(unsigned char* w) {
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 3, 0x9A, 0x39, 0, style);
    SH_CALL(Menu_DrawBorder)(XRest2h(w), YRest2h(w), 0x12, 6);
    const unsigned char* const text = Message(w[0xA] + 0x100u);
    SH_CALL(Text_DrawAt)(XRest2h(w) + 0xE, YRest2h(w) + 4, 0, 0xFF, text);
}

// original 0x59C8F0: the pupils of the window `w`: the box (x + 3, y + 3, 0x72,
// 0x58, 0, style); for each character record 0..7 with +0xB bit 0 whose +0x1F
// is window 1's +0xD (the master the list's cursor is on), its portrait
// MasterWin_DrawPortrait(x + dx + 4, y + dy + 5, +9, 0), dx on by 0x25 and,
// past 0x4A (s16), back to 0 with dy on by 0x2A; then the border (x, y, 0xD,
// 0xA), the label box (x - 0x25, y + 3, 0x22, 0x10, 0, style), its border (x -
// 0x28, y, 3, 1) and the label 0x66A1F8 at (x - 0x20, y + 4, 0, 0xFF).
extern "C" void __cdecl MasterWin_DrawPupils(unsigned char* w) {
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 3, 0x72, 0x58, 0, style);
    int dx = 0, dy = 0;
    for (U k = 0; k < 8; ++k) {
        const unsigned char* const rec = Record(k);
        if (!(rec[0xB] & 1)) continue;
        if (rec[0x1F] != Window1_Master) continue;
        const unsigned portrait = rec[9];
        SH_CALL(MasterWin_DrawPortrait)(dx + XRest2h(w) + 4, dy + YRest2h(w) + 5, portrait, 0);
        dx += 0x25;
        if (static_cast<short>(dx) > 0x4A) {
            dx = 0;
            dy += 0x2A;
        }
    }
    SH_CALL(Menu_DrawBorder)(XRest2h(w), YRest2h(w), 0xD, 0xA);
    // DIVERGENCE DIV-0083: no label box under a Latin overlay.
    if (rest_2h::g_pupil_label_off) return;
    const unsigned char style2 = Style();
    SH_CALL(Menu_DrawBox)(XRest2h(w) - 0x25, YRest2h(w) + 3, 0x22, 0x10, 0, style2);
    SH_CALL(Menu_DrawBorder)(XRest2h(w) - 0x28, YRest2h(w), 3, 1);
    SH_CALL(Text_DrawAt)(XRest2h(w) - 0x20, YRest2h(w) + 4, 0, 0xFF, TextRest2h(::bof3::Img(rest_2h::at::kPupilLabel.v)));
}

// original 0x59CA00: a portrait (x, y, index, shade): index 4 becomes 0xB from
// chapter 8 on (Cond_ByteFA, s8); Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, 0x1E,
// 0), Gfx_CommitPrim(1, 0xC); a sprite at the packet cursor re-read
// (Gpu_SetSprt): the shade's colour (0: 0x80 grey; 1: 0x30; else 0x80, 0x40,
// 0x40), x and y (their words, unsigned) as floats, u / v the index's cell of
// 0x66B470 + 3 / + 5, the CLUT ((cell[3] + 0x1E0) << 6 | cell[2] >> 4), 0x24 x
// 0x28; Gfx_CommitPrim(1, 0x1C). The cell is read in place by the unbounded
// index (12 cells; any byte stays inside .data).
extern "C" void __cdecl MasterWin_DrawPortrait(int x, int y, unsigned index, unsigned shade) {
    unsigned k = index & 0xFF;
    if (k == 4 && Cond_ByteFA >= 8) k = 0xB;
    const unsigned char* const cell = AtRest2h(::bof3::Img(rest_2h::at::kPortraitCells.v) + 4 * k);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x1E, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(p);
    const auto s = static_cast<unsigned char>(shade);
    if (s == 0) {
        p[6] = p[5] = p[4] = 0x80;
    } else if (s == 1) {
        p[6] = p[5] = p[4] = 0x30;
    } else {
        p[6] = 0x80;
        p[5] = p[4] = 0x40;
    }
    PutFloat(p + 8, static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFF));
    PutFloat(p + 0xC, static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFF));
    p[0x14] = static_cast<unsigned char>(cell[0] + 3);
    p[0x15] = static_cast<unsigned char>(cell[1] + 5);
    SetWord(p + 0x18, 0x24);
    SetWord(p + 0x1A, 0x28);
    SetWord(p + 0x16, ((static_cast<U>(cell[3]) + 0x1E0) << 6) | (cell[2] >> 4));
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

void ShopWin2FromRest2h_Inject() {
    rest_2h::g_master_title_live = true;   // DIV-0059, from here on
    // DIVERGENCE DIV-0083: a Latin language overlay only, as DIV-0059's switch.
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("MasterPupilLabel",
                         Low32(&rest_2h::g_pupil_label_off),
                         &was, &is, 1);
        bof3::Log("DIV-0083    the pupils box's label left out: %s", rest_2h::g_pupil_label_off ? "on" : "off");
    }
}

void ShopWin2_Inject() {
    BOF3_INJECT(ShopWin_MemberSlideUp);
    BOF3_INJECT(ShopWin_MemberSlideDown);
    BOF3_INJECT(ShopWin_EquipRun);
    BOF3_INJECT(ShopWin_EquipSlideIn);
    BOF3_INJECT(ShopWin_BuyListRun);
    BOF3_INJECT(Shop_DrawBuyList);
    BOF3_INJECT(ShopWin_BuyListSlideTo84);
    BOF3_INJECT(ShopWin_BuyListSlideTo46);
    BOF3_INJECT(ShopWin_BuyDetailRun);
    BOF3_INJECT(Shop_DrawBuyDetail);
    BOF3_INJECT(ShopWin_ItemListRun);
    BOF3_INJECT(ShopWin_ItemListSlideIn);
    BOF3_INJECT(ShopWin_SellDetailRun);
    BOF3_INJECT(Shop_DrawSellDetail);
    BOF3_INJECT(ShopWin_SharedListRun);
    BOF3_INJECT(ShopWin_SharedListSlideTo28);
    BOF3_INJECT(ShopWin_SharedMemberRun);
    BOF3_INJECT(ShopWin_MemberStatusRun);
    BOF3_INJECT(ShopWin_RowMenuRun);
    BOF3_INJECT(ShopWin_DrawRowMenu);
    BOF3_INJECT(ShopWin_MasterListRun);
    BOF3_INJECT(MasterWin_SlideOut);
    BOF3_INJECT(MasterWin_SlideTo64);
    BOF3_INJECT(ShopWin_MasterCaptionRun);
    BOF3_INJECT(MasterWin_CaptionSlideTo8C);
    BOF3_INJECT(ShopWin_PupilsRun);
    BOF3_INJECT(MasterWin_PupilsSlideDown);
    BOF3_INJECT(MasterWin_PupilsSlideUp);
    BOF3_INJECT(ShopWin_ItemCountRun);
    BOF3_INJECT(ShopWin_ItemCountSlideToD2);
    BOF3_INJECT(MasterWin_DrawList);
    BOF3_INJECT(MasterWin_Available);
    BOF3_INJECT(MasterWin_SkillKnown);
    BOF3_INJECT(MasterWin_DrawCaption);
    BOF3_INJECT(MasterWin_DrawPupils);
    BOF3_INJECT(MasterWin_DrawPortrait);
}
