// Group R4E of round fourteen (wave four): 48 functions at 0x45E870..0x460CAD -
// the cut's 48 rows for R4E (analysis/round14_cut.tsv), every one a function
// (26 hidden starts, each with its own frame and ret, reached through a .data
// table or a tail jmp), each read to its last instruction with capstone
// (2026-10-05) and fuzzed through the scenario harness's field mode
// (rest_4e_fuzz.cpp). docs/rest_4e.md has them one row each.
//
// The community band (the faerie village, areas 175..185) is resident code on
// the PC, reached from the field through the tables 0x652A70 and 0x652AE4
// (their readers are R4B's and R4D's) and by R4B's and R4D's calls. Its state
// bytes 0x939A3E (a mode) and 0x939A40 (a step) and the entry 0x9039F5 are
// the facility's; Field_Request 2 is a message open.
//
// Every one is a faithful replacement. Where the original indexes a table by a
// byte it never bounds, ours aborts with a message (round9 doc section 6; the
// policy is docs/rest_4e.md section 7, said once for the group). Every call
// goes through the harness (SH_CALL / SH_AT), so the start-up fuzz can stand
// recorders in for the callees; the cells the originals read again after a
// call are read again here after it.
#include "game/rest_4e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/faerie_name.h"
#include "game/move_script_bytes.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/rest_4e_callees.h"
#include "game/rdata_consts.h"
#include "game/text_advance.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;
using HandlerRest4e = void (__cdecl*)();

// --- the cells ------------------------------------------------------------------------

// kMode: now Rest4b_BoardState (symbols.toml). u8: the facility's mode (the tables' index)
// kNameMode: now Rest4c_Step (symbols.toml). u8: zeroed by the name commits (R4D's step)
// kStep: now Rest4b_BoardStep (symbols.toml). u8: the mode's step
// kStyle: now Config_WindowColour (symbols.toml). u8: the window style (Menu_DrawBox's colour)
// kFontPage: now Rest4f_Setting3 (symbols.toml). u8: 0 or not, which label CommuMusic_DrawList shows

constexpr ::bof3::image::Va kEntryNames{0x9048F0};
constexpr unsigned kEntryCount = 60;
// kEntryDword: now Game_BattleCount (symbols.toml). u32: copied into the entry's +4 when an item is given

constexpr U kMemberStride = 0xA4;
constexpr unsigned kMemberCount = 7;

}  // namespace
// DIVERGENCE DIV-0200 (faerie_name.h): 1 through PatchBytes("FaerieNameEnd")
// in Rest4E_Inject, after the self-test. SlotName: entry e's name for a draw,
// NUL-ended at its last whole glyph when on, else the table itself; the
// buffer is the draw's, read before the next call.
namespace rest_4e { std::uint8_t g_faerie_name_end = 0; }  namespace {

constexpr ::bof3::image::Va kWinOff{WindowRecords_addr + 3}; // u8: window 0 shut (the slide waits for 0)

constexpr ::bof3::image::Va kTextRecords{0x904CE0};   // Text_Records (0x20 each)
constexpr ::bof3::image::Va kTextBuffer{0x904BA0};    // the text scratch sprintf writes
constexpr ::bof3::image::Va kScriptPool{0x803580};    // Msg_OpenScript's pool: base + u16[base + 2 id]

constexpr ::bof3::image::Va kOneGlyph{0x669F08};

unsigned char& BRest4e(U address) { return move_script::At(address)[0]; }
U W(U address) { return move_script::Word(move_script::At(address)); }
U L(U address) { return static_cast<U>(Long(move_script::At(address))); }
void SetL(U address, U v) { SetLong(move_script::At(address), static_cast<std::int32_t>(v)); }
U Key(const void* p) { return Low32(p); }
template <class T> T Get(const unsigned char* p, unsigned offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof v);
    return v;
}
template <class T> void Put(unsigned char* p, unsigned offset, T v) { std::memcpy(p + offset, &v, sizeof v); }
float F(int v) { return static_cast<float>(v); }
int S16(U v) { return static_cast<short>(v); }
const unsigned char* Message(unsigned id) { return move_script::At(::bof3::Img(kScriptPool.v) + W(::bof3::Img(kScriptPool.v) + 2 * id)); }

// jmp / call [table + 4 * byte]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after.
void Run(const char* who, U table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_4e.md section 7)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<HandlerRest4e>(static_cast<std::uintptr_t>(L(table + 4 * index)))();
}

// --- the callees ------------------------------------------------------------------------
void Commit(unsigned size) { SH_CALL(Gfx_CommitPrim)(1, size); }
void DrawMode(unsigned dtd, unsigned tpage, const short* window) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, static_cast<int>(dtd), tpage,
                             Low32(window));
    Commit(0xC);
}
void Grey(unsigned char* p) { p[4] = p[5] = p[6] = 0x80; }
void OpenMessage(unsigned short id) { SH_CALL(Msg_OpenScript)(id); }
void Box(int x, int y, int w, int h, int flags) { SH_CALL(Menu_DrawBox)(x, y, w, h, flags, static_cast<unsigned char>(Config_WindowColour)); }

}  // namespace

// ===========================================================================
// The panels: an entry's, a member's, and their pieces
// ===========================================================================

// ===========================================================================
// The two name commits
// ===========================================================================

// ===========================================================================
// The track list (CommuMusic_Modes by the mode 0x939A3E)
// ===========================================================================

// The list's row, an index of the 40 tracks 0x653098 read signed; outside
// them the original reads the bytes there, and so does ours (a read only).
U TrackIndex() { return CommuMusic_Track; }

// ===========================================================================
// The item handed to an entry (CommuItem_Modes by the mode 0x939A3E)
// ===========================================================================

// The item window's frame at its slide: a box 0x118 x 0x13 flags 0xF2 at (0x14,
// 0x12 - 12 slide) and its outline 0x113 x 0xF at (0x16, 0x14 - 12 slide), the
// slide read again for the outline (the original's y carries leftovers above
// its low word).
void ItemFrame(unsigned char slide) {
    Box(0x14, 0x12 - 12 * slide, 0x118, 0x13, 0xF2);
    SH_CALL(Menu_DrawOutlineNotched)(0x16, 0x14 - 12 * CommuItem_Slide, 0x113, 0xF, 0);
}

// Inventory_IdLists[tab]: five lists (0..3 by tab, then the key items); past
// them the original reads a pointer out of the table after.
const unsigned char* ItemList(const char* who, unsigned tab) {
    if (tab >= 5)
        bof3::Fatal("%s: the tab 0x80316A is %u, past the five of Inventory_IdLists (docs/rest_4e.md section 7)", who,
                    tab);
    return At(L(::bof3::Img(bof3::addr::Inventory_IdLists) + 4 * tab));
}

// The item window's slide up one (to 4), its frame, and whether window 0 has
// shut (+3 0).
bool ItemClosed() {
    unsigned char slide = CommuItem_Slide;
    if (slide < 4) CommuItem_Slide = ++slide;
    ItemFrame(slide);
    return WindowRecords[0].state == 0;
}

// ===========================================================================
// The ranked lists (CommuRank_Modes by the mode 0x939A3E)
// ===========================================================================

// A list's heading: script message 0x37 at (x + 0x58, y); the icon (0 or 1) at
// (x + 8, y + 0x14); a message at (x + 0x18, y + 0x14); the count ("%2d"-like,
// Boss26Fx_CountFormat) in the 12-point font at (x + 0x6C, y + 0x14) and the
// glyph 0x669F08 after it. The count is read after the second message.
//
// DIV-0064 group 14 (2026-10-10): once the overlay has written the village's
// words, the heading is centred where the Chinese one sat - the shipped
// message is four glyphs at x + 0x58, so its middle is x + 0x70, and the US
// `Population change` drawn from x + 0x58 would run under the page counter
// at 0xEE - and the word after the count is the overlay's `faeries`, or
// `faery` for a count of one (the US disc holds the pair), drawn whole.
void ListHeading(int x, int y, unsigned icon, unsigned title, U count) {
    const bool words = Labels_Written(14);
    const unsigned char* const heading = Message(0x37);
    const int hx = words ? x + 0x70 - static_cast<int>(TextAdvance_Width(heading)) / 2 : x + 0x58;
    SH_CALL(Text_DrawAt)(hx, y, 0, 0xFF, heading);
    SH_CALL(CommuRank_DrawIcon)(x + 8, y + 0x14, icon);
    const unsigned char* const row = Message(title);
    SH_CALL(Text_DrawAt)(x + 0x18, y + 0x14, 0, 0xFF, row);
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(kTextBuffer.v))),
                         reinterpret_cast<const char*>(At(Key(Boss26Fx_CountFormat))), BRest4e(count));
    // The count's column is x + 0x6C, sized for the five-glyph Chinese row
    // label; a disc's longer label (the French `Taux de natalité`, the German
    // `Geburtenrate`) runs under it, so under a written group the column
    // moves right of the label by a letter's gap (the owner's word, 2026-10-10).
    int cx = x + 0x6C;
    if (words) {
        const int past = x + 0x18 + static_cast<int>(TextAdvance_Width(row)) + 8;
        if (past > cx) cx = past;
    }
    SH_CALL(Text_DrawFont12)(cx, y + 0x14, 0, At(::bof3::Img(kTextBuffer.v)));
    if (words)
        SH_CALL(Text_DrawAt)(cx + 0x20, y + 0x14, 0, 0xFF, Labels_Slot(14, BRest4e(count) == 1 ? 2 : 1));   // a letter's gap after the count, as the US screen has
    else
        SH_CALL(Text_DrawAt)(x + 0x84, y + 0x14, 0, 1, At(::bof3::Img(kOneGlyph.v)));
}

// DIVERGENCE DIV-0064 group 14 (amended 2026-11-01, issue #177): set by
// Rest4E_Inject after the self-test, for a Latin language (DIV-0056).
namespace rest_4e { bool g_west_cause_column = false; }

void Rest4E_Inject() {
    if (bof3::WantsShadow("rest_4e")) rest_4e::SelfTest();
    {
        // DIV-0200, after the self-test (which compares Capcom's draws).
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("FaerieNameEnd", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&rest_4e::g_faerie_name_end)), &was, &is, 1);
    }
    // DIVERGENCE DIV-0064 group 14 (amended 2026-11-01): after the self-test,
    // which compares Capcom's column; a Latin language only (DIV-0056).
    if (Lang_Latin()) {
        rest_4e::g_west_cause_column = true;
        bof3::Log("DIV-0064    the village's deaths page: the cause at x + 0x48, the Western discs' column");
    }
}
