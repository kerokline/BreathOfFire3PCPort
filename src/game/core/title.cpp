// Moved here by tools/move_funcs.py from config_text.cpp, draw_emit.cpp, field_c1.cpp, gfx_flush.cpp, gfx_unpack.cpp, item_use.cpp, mode_flow.cpp, mode_tasks.cpp, prim.cpp, rest_4e.cpp, rest_4f.cpp, title_states.cpp (docs/source-layout-round.md).
#include "game/core/title.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/area_data_reload.h"
#include "game/button_text.h"
#include "game/config_text.h"
#include "game/core/title_callees.h"
#include "game/draw_emit.h"
#include "game/faerie_name.h"
#include "game/field_c1.h"
#include "game/field_c1_callees.h"
#include "game/gfx_flush.h"
#include "game/gfx_unpack.h"
#include "game/item_use.h"
#include "game/item_use_callees.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/layering.h"
#include "game/map_rim.h"
#include "game/menu_frame.h"
#include "game/mode_flow.h"
#include "game/mode_flow_callees.h"
#include "game/mode_tasks.h"
#include "game/move_script_bytes.h"
#include "game/prim.h"
#include "game/psp_code.h"
#include "game/rdata_consts.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_4e.h"
#include "game/rest_4e_callees.h"
#include "game/rest_4f.h"
#include "game/rest_4f_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/title_states.h"
#include "game/wide_rand.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = title::at;

}  // namespace

using namespace item_use;

using namespace mode_flow;

namespace {

// DIVERGENCE DIV-0017: the selected row, in the dialogue font at its own
// advance.
//
// The row under the cursor is drawn large: 0x461800 and Config_DrawRowOptions each have a
// branch that goes through Text_DrawAt and its 12-unit quad instead of the
// 8-unit draw. In Chinese that is the same glyph at full size. On the
// PlayStation it is a different FONT - the 8 x 12 dialogue cells, still on an
// 8 advance (owner's screenshots, 2026-09-21: "Background" large spans the
// same ten columns it does small). Our strings name the 8 x 8 UI cells, which
// at full size are a tripled 8 x 8 on an advance of 8: crowded, and - because
// the branch reckons 12 units a character where the pen moves 8 - thrown left
// by four units a character.
//
// So this stands in for Text_DrawAt at those two call sites only (and, since
// DIV-0026, at the controller panel's one, 0x461B43), and redraws
// the string with each UI glyph swapped for the dialogue glyph of the same
// character. The two sets are in the same code order, 100 cells each
// (tools/loc_build.py), so the swap is a constant offset. The blank after the
// UI set stays: it is the space, and its advance is 8 in either draw.
constexpr std::uint32_t kUiGlyphs = 0xA00, kDialogueGlyphs = 0x993, kSetCells = 100;

// DIV-0051: the controller panel's icons - six glyphs loc_build.py puts
// after the UI set's blank (its ICONS_AT): circle, cross, triangle, square
// from the disc's 12 x 12 set, L1 and R1 composed of the dialogue capital
// and the small serifed 1, each doubled to fill the 24 x 24 glyph, so the
// 24 px quad shows the icon at the screen's 2x with its own margins.
constexpr std::uint32_t kIconCircle = kUiGlyphs + kSetCells + 1, kIconCross = kIconCircle + 1,
                        kIconTriangle = kIconCircle + 2, kIconSquare = kIconCircle + 3,
                        kIconL1 = kIconCircle + 4, kIconR1 = kIconCircle + 5;

// The quad's x from the call's x (panel x + 0x5D). Measured off the captures
// (analysis/shots/ctrl_icons, 2026-09-24): the row box's divider sits at
// row x + 0x44 whatever the box's width, and the box ends at row x + width.
// The 24 px quad at x - 0x14 runs from row x + 0x44 to 0x5C - the shapes'
// own margins keep their ink a pixel or two inside - and the box, 0x60 wide,
// ends four past it.
constexpr int kIconX = -0x0C;   // the owner, 2026-09-24: half a glyph right of -0x14

// The end of a 64 KB packet pool less 0x54 bytes of margin - the literal
// 0x7F1BAC in the original, for buffer 0.
constexpr std::uint32_t kPoolLimit = 0x7F1BAC;   // Gfx_PacketPools 0x7E1C00 + 0x10000 - 0x54

using U = std::uint32_t;

}  // namespace

namespace item_use {

using move_script::At;

using move_script::SetWord;

using move_script::Word;

}  // namespace item_use

namespace {

using move_script::At;

using move_script::Word;

// Task 0's state word, +0 of its record: 0 while no task 0 runs.
std::uint16_t TaskZeroState() { return move_script::Word(move_script::At(::bof3::Img(bof3::addr::Task_Records))); }

}  // namespace

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using HandlerRest4e = void (__cdecl*)();

// kSavedTrack: now CommuMusic_SavedTrack (symbols.toml). u8: Music_Track when the list opened
// kSlide: now CommuMusic_Slide (symbols.toml). u8: the list's slide 4..0
// kTrack: now CommuMusic_Track (symbols.toml). s16: the list's row, the track index
// kItemSlide: now CommuItem_Slide (symbols.toml). u8: the item window's slide 0..4
constexpr ::bof3::image::Va kPages{0x675FD8};         // u8 x 3: the three lists' page counts

constexpr ::bof3::image::Va kPage{0x675FDB};          // s8: the page shown, over the three lists

constexpr ::bof3::image::Va kEntryNames{0x9048F0};

constexpr unsigned kEntryCount = 60;

// The three ranked lists: counts 0x9039A0 (entries 0x9039C0.., 7 a page),
// 0x904A90 (entries 0x904F00.., 20 a page), 0x937F80 (pairs 0x904CA0.., 8 a
// page).
// kListA: now Rest4a_RemovedCount (symbols.toml).
constexpr ::bof3::image::Va kListAEntries{0x9039C0};

// kListB: now Rest4b_BusyA (symbols.toml).
constexpr ::bof3::image::Va kListBEntries{0x904F00};

// kListC: now Rest4b_BusyB (symbols.toml).
constexpr ::bof3::image::Va kListCPairs{0x904CA0};

const unsigned char* SlotName(unsigned e) {
    static unsigned char ended[6];
    const unsigned char* const slot = move_script::At(::bof3::Img(kEntryNames.v) + 5 * e);
    return rest_4e::g_faerie_name_end ? faerie_name::Ended(slot, ended) : slot;
}

// WindowRecords 0 and 1 (0x24 each): the item window and its cursor.
constexpr ::bof3::image::Va kWin0{0x803160};

constexpr ::bof3::image::Va kWin1{0x803184};

// kTab: now Window0_ParamA (symbols.toml). u8: the tab 0..3 (Inventory_IdLists' index)
constexpr ::bof3::image::Va kTop{WindowRecords_addr + 0xB}; // u8: the first row shown

// kRow: now Window0_Cursor (symbols.toml). u8: the row
// kTabMove: now Window0_Arrows (symbols.toml). u16: 0x31 / 0x32 when the tab moved
constexpr ::bof3::image::Va kScroll{WindowRecords_addr + 0x12}; // u16: 0xF0 / 0x10 while the list scrolls

constexpr ::bof3::image::Va kTextRecords{0x904CE0};   // Text_Records (0x20 each)

constexpr ::bof3::image::Va kTextBuffer{0x904BA0};    // the text scratch sprintf writes

constexpr ::bof3::image::Va kScriptPool{0x803580};    // Msg_OpenScript's pool: base + u16[base + 2 id]

constexpr ::bof3::image::Va kPairMessages{0x653180};  // 4 bytes a kind: the message id, a "name" word

constexpr ::bof3::image::Va kTilesTop{0x6531A0};

constexpr ::bof3::image::Va kTilesBottom{0x6531C0};

constexpr ::bof3::image::Va kTilesSide{0x6531E0};

constexpr ::bof3::image::Va kPageFormat{0x6531F4};    // "page / pages"

constexpr ::bof3::image::Va kOneGlyph{0x669F08};

unsigned char& BRest4e(U address) { return move_script::At(address)[0]; }

U W(U address) { return move_script::Word(move_script::At(address)); }

void SetW(U address, U v) { move_script::SetWord(move_script::At(address), v); }

U L(U address) { return static_cast<U>(Long(move_script::At(address))); }

void SetL(U address, U v) { SetLong(move_script::At(address), static_cast<std::int32_t>(v)); }

U Key(const void* p) { return Low32(p); }

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

// The entry 0x9039F5 indexes the 60-entry table; past it the original writes
// the save block's bytes after (docs/rest_4e.md section 7). For a write.
unsigned char* EntryRecord(const char* who) {
    const unsigned e = AreaW0c_TailSub;
    if (e >= kEntryCount)
        bof3::Fatal("%s: the entry 0x9039F5 is %u, past the 60 of 0x9046D0 (docs/rest_4e.md section 7)", who, e);
    return move_script::At(::bof3::Img(Community_Records_addr) + 8 * e);
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

namespace {

using HandlerRest4f = void (__cdecl*)();

using Frame = void (__cdecl*)(int, int, int, int);

using DrawText = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);

unsigned char& BRest4f(U a) { return move_script::At(a)[0]; }

U LRest4f(U a) { return static_cast<U>(Long(move_script::At(a))); }

U WRest4f(U a) { return move_script::Word(move_script::At(a)); }

void SetWRest4f(U a, U v) { move_script::SetWord(move_script::At(a), v); }

// Config_DrawControllerPanel's frame rows: the imm8 of its `push 0xF`
// (at::kPadFrameH), which nothing patches - the function's own constant, ours
// since the function is (registered against the code in Rest4F_Inject).
constexpr int kPadFrameRows = 0xF;

// jmp / call [table + byte * 4]: the table's `entries` handlers read in place (the
// fuzz swaps the cells for recorders); a Fatal past them, where the original
// goes through the dword after - the next table's cell or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u handlers of 0x%X - the original jumps through the dword after "
                    "(docs/rest_4f.md section 7)",
                    who, byte, index, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<HandlerRest4f>(static_cast<std::uintptr_t>(LRest4f(table + 4 * index)))();
}

void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

void Backdrop() { SH_CALL(Menu_DrawBackdrop)(Config_Background); }

// The banner box: (0x14, y, 0x118, 0x13) in the window style.
void TitleBox(int y) { SH_CALL(Menu_DrawTitleBox)(0x14, y, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour)); }

// The banner's text, system message 0xBC + the cursor (s8), at (0x1C, 0x13).
void TitleText(U id) {
    const unsigned char* const text = SH_CALL(Msg_SystemPtr)(id);
    SH_CALL(Text_DrawAt)(0x1C, 0x13, 0, 0xFF, text);
}

U CursorId() { return static_cast<std::uint32_t>(MenuLists_Cursor) + 0xBC; }

int CursorS() { return MenuLists_Cursor; }

// The top bar's hand: x = cursor * 48 + 0x76.
void TopHand() { SH_CALL(Menu_DrawHand)(CursorS() * 48 + 0x76, 0x2A, 0); }

U Repeat() { return SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF000u); }

// The cursor's row state for the panel's row draws: the counter byte on the
// row under the cursor (cursor - 2), 0 on the others; 0x929F04 read as a dword.
U RowState(unsigned row) {
    const U d = LRest4f(::bof3::Img(rest_4f::at::kCounter.v));
    const int cursor = static_cast<signed char>(static_cast<unsigned char>(d >> 8)) - 2;
    return cursor == static_cast<int>(row) ? (d & 0xFF) : 0;
}

void SetDefaults() {
    Config_WindowColour = 0;
    Config_Background = 0;
    Rest4f_Setting3 = 0;
    Rest4f_Setting5C = 0;
    BattleWinStates_LabelGate = 0;
    FieldE2_RunDefault = 0;
    ModeFlow_ConfigBytes = 1;
    SetWRest4f(::bof3::Img(rest_4f::at::kButtons.v), 0x23);
    SetWRest4f(::bof3::Img(rest_4f::at::kButtons.v) + 2, 0x80);
    SetWRest4f(::bof3::Img(rest_4f::at::kButtons.v) + 0xC, 0x40);
    SetWRest4f(::bof3::Img(rest_4f::at::kButtons.v) + 4, 0x10);
    SetWRest4f(::bof3::Img(rest_4f::at::kButtons.v) + 6, 8);
    SetWRest4f(::bof3::Img(rest_4f::at::kButtons.v) + 8, 4);
}

// A setting stepped by 0x2000 (up) or 0x8000 (down), wrapping within 0..top
// (signed compares, as the original's jle / jns); `down_first` tests 0x8000
// first (row 2's order).
void StepSetting(U cell, int top, bool down_first) {
    const U p = LRest4f(::bof3::Img(rest_4f::at::kPressedDword.v));
    const auto up = [&] {
        Sound(0x101);
        const unsigned char v = static_cast<unsigned char>(BRest4f(cell) + 1);
        BRest4f(cell) = v;
        if (static_cast<signed char>(v) > top) BRest4f(cell) = 0;
    };
    const auto down = [&] {
        Sound(0x101);
        const unsigned char v = static_cast<unsigned char>(BRest4f(cell) - 1);
        BRest4f(cell) = v;
        if (static_cast<signed char>(v) < 0) BRest4f(cell) = static_cast<unsigned char>(top);
    };
    if (down_first) {
        if (p & 0x8000) down();
        else if (p & 0x2000) up();
    } else {
        if (p & 0x2000) up();
        else if (p & 0x8000) down();
    }
}

// Bytes the handlers touch that have no name (docs/title-states.md has the
// PSX twin of each).
constexpr ::bof3::image::Va kPlayClock{0x9040C8};      // four bytes zeroed starting the demo (PSX 0x80144FBC)

// kScenarioFlags: now Chapter_FlagRow (symbols.toml). the address of the chapter's Cond_Flags dword (PSX 0x80146868)
// kSubState: now ScenaSc7_DrawPool (symbols.toml). the dispatcher 0x56D8B0's bytes (PSX 0x801448E4)
// kSubInitTable: now Scenario_SubInitTable (symbols.toml). 16 bytes Scenario_SubInit copies (PSX 0x801C94A4)
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0xA0, where they go (PSX 0x80144F24)
constexpr ::bof3::image::CodeVa kDemoEntry{bof3::addr::Field_Task};      // task 0's entry for the demo (the field task)

constexpr ::bof3::image::CodeVa kMenuEntry{bof3::addr::TitleTask_Run};      // task 0's entry for the title menu

// DIVERGENCE DIV-0131 (D256): on a PSP base (a PSP-only install, the cache's
// base_policy "psp") the title's logo pages are the PSP's own art, laid out
// for the PSP's own code, which Capcom's PlayStation rectangles cut wrongly:
// the text's lower piece starts 11 rows earlier and is 10 wider, the emblem
// carries a caption under it, and the art holds its own TM. These are where
// the PSP's pieces lie in its pages (measured from the pages' pixels,
// 2026-10-17; docs/psp-title-framing.md), and where each goes against
// Capcom's set origins: the upper text piece 3 left of set C's x, the lower
// piece at the upper's x + 224 and y + 5 (the F's strokes join there), the
// emblem 30 right of set B's x. Our measurement, not a copied table. Set while
// the start-up fuzz has finished; the fuzz compares Capcom's layout.
struct PspRect {
    unsigned char u, v;
    std::uint16_t w, h;
};

constexpr PspRect kPspTextUpper = {0, 0, 224, 116};    // "BREATH OF F", the left wing

constexpr PspRect kPspTextLower = {0, 117, 106, 102};  // "IRE", the right wing, the art's TM

constexpr PspRect kPspEmblem = {0, 0, 202, 122};       // the "III" emblem, without the caption below it

constexpr int kPspTextDx = -3, kPspLowerDx = 224, kPspLowerDy = 5, kPspEmblemDx = 30;

// The 16-bit words the handlers count with, as the original's `inc word` /
// `dec word` in memory: wrapping at 0x10000.
// Capcom's sprite `index` (its palette and the rest of its record), its
// rectangle replaced by `r` (DIV-0131).
unsigned char* PspSprite(int x, int y, unsigned index, const PspRect& r, unsigned slot, unsigned semi) {
    unsigned char* const prim = title_states::gTitleStates.sprite(x, y, index, slot, semi);
    prim[0x14] = r.u;
    prim[0x15] = r.v;
    move_script::SetWord(prim + 0x18, r.w);
    move_script::SetWord(prim + 0x1A, r.h);
    return prim;
}

void BumpState() { Title_State = static_cast<std::uint16_t>(Title_State + 1); }

}  // namespace

// original 0x45FE60: CommuItem_Steps[1] - no message open: Window_ResetAll, the
// item window set up (CommuItem_SetupWindow), the slide 4, the step + 1.
extern "C" void __cdecl CommuItem_OpenWindow(void) {
    if (Field_Request == 2) return;
    SH_CALL(Window_ResetAll)();
    SH_CALL(CommuItem_SetupWindow)();
    const unsigned char step = Rest4b_BoardStep;
    CommuItem_Slide = 4;
    Rest4b_BoardStep = static_cast<unsigned char>(step + 1);
}

// original 0x45FE90: CommuItem_Steps[2] - the inventory window: its frame (the
// slide down one), and while window 0 is open (+3 0): window 1 (the cursor)
// placed at window 0's x + 7, y + 13 (row - top + 2); the pad through
// Input_AutoRepeat (& 0xF00C) - the tab left (0x8000, wrapping 0 to 3, +0x10 =
// 0x32) or right (0x2000, past 3 to 0, +0x10 = 0x31), the row up (0x1000; above
// the top: +0x12 = 0xF0) or down (0x4000, to 0x7F; at top + 9: +0x12 = 0x10),
// a page up (4) or down (8) of nine; a row moved: sound 0x100. Not scrolling
// (+0x12 0): cancel - sound 0x106, the windows shut, the step + 1; confirm on
// an empty row - sound 0x107 and nothing drawn; on an item - sound 0x105, the
// item and the tab into the entry's +2 / +3, the windows shut, the step 4. The
// row's item, if any, then has its help line (Item_HelpMessage's system
// message) at (0x1D, 0x15).
extern "C" void __cdecl CommuItem_Choose(void) {
    unsigned char slide = CommuItem_Slide;
    if (slide != 0) CommuItem_Slide = --slide;
    ItemFrame(slide);
    if (WindowRecords[0].state != 0) return;
    const U window_x = L(::bof3::Img(kWin0.v) + 4);
    WindowRecords[1].in_use = 1;
    SetW(::bof3::Img(kWin1.v) + 4, window_x + 7);
    SetW(::bof3::Img(kWin1.v) + 6, 13u * (static_cast<std::uint32_t>(Window0_Cursor) - BRest4e(::bof3::Img(kTop.v)) + 2u) + static_cast<std::uint32_t>(WindowRecords[0].y));
    const unsigned moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & Sigma(0xF00C));
    if (moved & 0x8000) {
        SH_CALL(Sound_PlayEffect)(0x101);
        const auto tab = static_cast<unsigned char>(Window0_ParamA - 1);
        Window0_Arrows = 0x32;
        Window0_ParamA = tab;
        if (static_cast<signed char>(tab) < 0) Window0_ParamA = 3;
    } else if (moved & 0x2000) {
        Window0_Arrows = 0x31;
        SH_CALL(Sound_PlayEffect)(0x101);
        const auto tab = static_cast<unsigned char>(Window0_ParamA + 1);
        Window0_ParamA = tab;
        if (tab > 3) Window0_ParamA = 0;
    }
    unsigned char row = Window0_Cursor;
    const unsigned char was = row;
    if (moved & 0x1000) {
        if (row != 0) Window0_Cursor = --row;
        if (row < BRest4e(::bof3::Img(kTop.v))) SetW(::bof3::Img(kScroll.v), 0xF0);
    } else if (moved & 0x4000) {
        if (row < 0x7F) Window0_Cursor = ++row;
        if (Window0_Cursor >= static_cast<int>(BRest4e(::bof3::Img(kTop.v))) + 9) SetW(::bof3::Img(kScroll.v), 0x10);
    } else if (moved & Sigma(4)) {
        const unsigned char top = BRest4e(::bof3::Img(kTop.v));
        if (top == 0) {
            row = 0;
            Window0_Cursor = 0;
        } else if (top < 9) {
            row = static_cast<unsigned char>(row - top);
            BRest4e(::bof3::Img(kTop.v)) = 0;
            Window0_Cursor = row;
        } else {
            row = static_cast<unsigned char>(row - 9);
            Window0_Cursor = row;
            BRest4e(::bof3::Img(kTop.v)) = static_cast<unsigned char>(top - 9);
        }
    } else if (moved & Sigma(8)) {
        const unsigned char top = BRest4e(::bof3::Img(kTop.v));
        if (top == 0x77) {
            row = 0x7F;
            Window0_Cursor = row;
        } else if (top > 0x6E) {
            BRest4e(::bof3::Img(kTop.v)) = 0x77;
            row = static_cast<unsigned char>(row + (0x77 - top));
            Window0_Cursor = row;
        } else {
            row = static_cast<unsigned char>(row + 9);
            Window0_Cursor = row;
            BRest4e(::bof3::Img(kTop.v)) = static_cast<unsigned char>(top + 9);
        }
    }
    if (was != row) SH_CALL(Sound_PlayEffect)(0x100);
    if (W(::bof3::Img(kScroll.v)) != 0) return;
    const unsigned pressed = Input_Pressed;
    if (Field_CancelButtons & pressed) {
        WindowRecords[1].in_use = 0;
        WindowRecords[0].state = 1;
        SH_CALL(Sound_PlayEffect)(0x106);
        ++Rest4b_BoardStep;
    } else if (Field_ConfirmButtons & pressed) {
        const unsigned char item = ItemList("CommuItem_Choose (0x45FE90)", Window0_ParamA)[Window0_Cursor];
        if (item == 0) {
            SH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        SH_CALL(Sound_PlayEffect)(0x105);
        const unsigned char tab = Window0_ParamA;
        unsigned char* const entry = EntryRecord("CommuItem_Choose (0x45FE90)");
        WindowRecords[0].state = 1;
        entry[2] = item;
        unsigned char* const again = EntryRecord("CommuItem_Choose (0x45FE90)");
        WindowRecords[1].in_use = 0;
        Rest4b_BoardStep = 4;
        again[3] = tab;
    }
    const unsigned tab = Window0_ParamA;
    const unsigned char shown = ItemList("CommuItem_Choose (0x45FE90)", tab)[Window0_Cursor];
    if (shown == 0) return;
    const unsigned help = SH_CALL(Item_HelpMessage)(tab, shown);
    const unsigned char* const line = SH_CALL(Msg_SystemPtr)(help);
    SH_CALL(Text_DrawAt)(0x1D, 0x15, 0, 0xFF, line);
}

// original 0x4601D0: CommuItem_Steps[3] (cancelled) - the window slides away;
// shut: Window_ResetAll, message 0x5F, the mode + 1, Field_Request 2.
extern "C" void __cdecl CommuItem_Cancel(void) {
    if (!ItemClosed()) return;
    SH_CALL(Window_ResetAll)();
    OpenMessage(0x5F);
    const unsigned char mode = Rest4b_BoardState;
    Field_Request = 2;
    Rest4b_BoardState = static_cast<unsigned char>(mode + 1);
}

// original 0x460270: CommuItem_Steps[4] (chosen) - the window slides away;
// shut: Window_ResetAll, then the chosen item's byte (+0x10 of a consumable's
// record, +0x11 of a weapon's, armour's, accessory's - by the entry's +3 low
// nibble 0, 1, 2, else) with bit 3 set: message 0xEB, the step 6,
// Field_Request 2; else its 16-byte name (Item_NamePtr) into Text_Records 0,
// message 0x61, the step + 1, the slide 0, Field_Request 2. Item_NamePtr is
// handed the entry's +3 and +2 as the bytes of dwords whose upper bytes are the
// original's leftovers (it reads the bytes).
extern "C" void __cdecl CommuItem_Confirm(void) {
    if (!ItemClosed()) return;
    SH_CALL(Window_ResetAll)();
    const unsigned char* const entry = move_script::At(::bof3::Img(Community_Records_addr) + 8u * AreaW0c_TailSub);
    const unsigned char tab = entry[3];
    const unsigned item = entry[2];
    unsigned char kind;
    switch (tab & 0xF) {
    case 0: kind = BRest4e(::bof3::Img(bof3::addr::NameTable_Consumables) + 0x10 + 22 * item); break;
    case 1: kind = BRest4e(::bof3::Img(bof3::addr::NameTable_Weapons) + 0x11 + 28 * item); break;
    case 2: kind = BRest4e(::bof3::Img(bof3::addr::NameTable_Armour) + 0x11 + 26 * item); break;
    default: kind = BRest4e(::bof3::Img(bof3::addr::NameTable_Accessories) + 0x11 + 24 * item); break;
    }
    if (kind & 8) {
        OpenMessage(0xEB);
        Rest4b_BoardStep = 6;
        Field_Request = 2;
        return;
    }
    const unsigned char* const name = SH_CALL(Item_NamePtr)(tab, item);
    std::memcpy(move_script::At(::bof3::Img(kTextRecords.v)), name, 16);
    OpenMessage(0x61);
    const unsigned char step = Rest4b_BoardStep;
    CommuItem_Slide = 0;
    Rest4b_BoardStep = static_cast<unsigned char>(step + 1);
    Field_Request = 2;
}

// original 0x4603F0: CommuItem_Steps[5] - no message open: the entry's item
// taken out of the inventory (Inventory_Remove(+3, +2, 1), a fourth 0 pushed),
// bit 4 of its +3 set, the dword 0x904134 into its +4, the mode + 1.
extern "C" void __cdecl CommuItem_Give(void) {
    if (Field_Request == 2) return;
    const unsigned char* const entry = EntryRecord("CommuItem_Give (0x4603F0)");
    SH_CALL(Inventory_Remove)(entry[3], entry[2], 1);
    unsigned char* const again = EntryRecord("CommuItem_Give (0x4603F0)");
    again[3] |= 0x10;
    Put<U>(EntryRecord("CommuItem_Give (0x4603F0)"), 4, Game_BattleCount);
    ++Rest4b_BoardState;
}

// original 0x460460: CommuItem_Steps[6] (refused) - no message open: message
// 0x63, Field_Request 2 (the step stays).
extern "C" void __cdecl CommuItem_Refused(void) {
    if (Field_Request == 2) return;
    OpenMessage(0x63);
    Field_Request = 2;
}

// original 0x460480: WindowRecords 0 and 1 for the item window - 0: in use,
// kind 2, +2 0, +3 2, at (-0xAA, 0x3E), tab, top and row 0, +0xD 0xFF, +8 / +9
// 0, +0x10 / +0x12 0; 1: off, kind 2, +2 1, +3 0.
extern "C" void __cdecl CommuItem_SetupWindow(void) {
    WindowRecords[0].handler = 2;
    WindowRecords[0].kind = 0;
    WindowRecords[0].state = 2;
    WindowRecords[0].in_use = 1;
    WindowRecords[0].x = 0xFF56;
    WindowRecords[0].y = 0x3E;
    BRest4e(::bof3::Img(kWin0.v) + 0xA) = 0;
    BRest4e(::bof3::Img(kWin0.v) + 0xB) = 0;
    BRest4e(::bof3::Img(kWin0.v) + 0xC) = 0;
    BRest4e(::bof3::Img(kWin0.v) + 0xD) = 0xFF;
    BRest4e(::bof3::Img(kWin0.v) + 8) = 0;
    BRest4e(::bof3::Img(kWin0.v) + 9) = 0;
    SetW(::bof3::Img(kWin0.v) + 0x10, 0);
    SetW(::bof3::Img(kWin0.v) + 0x12, 0);
    BRest4e(::bof3::Img(kWin1.v) + 1) = 2;
    BRest4e(::bof3::Img(kWin1.v) + 2) = 1;
    BRest4e(::bof3::Img(kWin1.v) + 3) = 0;
    WindowRecords[1].in_use = 0;
}

// original 0x460500: 0x652AE4[2] (R4B's 0x457510 jumps through it by
// 0x9039F4) - jmp through CommuRank_Modes by the mode 0x939A3E, unchecked
// (ours aborts past its 3).
extern "C" void __cdecl CommuRank_Dispatch(void) {
    Run("CommuRank_Dispatch (0x460500)", Key(CommuRank_Modes), CommuRank_Modes_count, Rest4b_BoardState,
        "the mode 0x939A3E");
}

// original 0x460510: CommuRank_Modes[0] - each list's page count (0 for an
// empty list, else (n - 1) / per + 1: 7 of 0x9039A0, 20 of 0x904A90, 8 of
// 0x937F80), the page 0, the mode + 1.
extern "C" void __cdecl CommuRank_Pages(void) {
    auto pages = [](unsigned n, unsigned per) -> unsigned char {
        return n == 0 ? 0 : static_cast<unsigned char>((static_cast<int>(n) - 1) / static_cast<int>(per) + 1);
    };
    BRest4e(::bof3::Img(kPages.v)) = pages(Rest4a_RemovedCount, 7);
    BRest4e(::bof3::Img(kPages.v) + 1) = pages(Rest4b_BusyA, 20);
    CommuList_Page = 0;
    BRest4e(::bof3::Img(kPages.v) + 2) = pages(Rest4b_BusyB, 8);
    ++Rest4b_BoardState;
}

// original 0x4605D0: CommuRank_Modes[1] - the page 0x675FDB found in the three
// lists' counts (the first whose count is above what is left), the backdrop
// (CommuRank_DrawBackdrop at (0x20, 0x18)), that list's page at (0x28, 0x28);
// the pad: 0x2000 the page on (from the total back to 0), 0x8000 back (below 0
// to the total - 1); the page number and the total ("%d/%d", 0x6531F4) at
// (0xEE, 0x2C); confirm or cancel: the mode + 1. The original keeps the list
// and the page in stack bytes whose upper bytes it never writes (each callee
// reads the low byte). Every count 0 or the page past their sum: the original
// walks on through 0x675FDB and the bytes after; ours aborts.
extern "C" void __cdecl CommuRank_Show(void) {
    unsigned char left = static_cast<unsigned char>(CommuList_Page);
    unsigned list = 0;
    for (;;) {
        if (list >= 3)
            bof3::Fatal("CommuRank_Show (0x4605D0): the page 0x675FDB is %u, past the three lists' %u + %u + %u pages "
                        "- the original reads on past 0x675FDA (docs/rest_4e.md section 7)",
                        static_cast<unsigned char>(CommuList_Page), BRest4e(::bof3::Img(kPages.v)), BRest4e(::bof3::Img(kPages.v) + 1), BRest4e(::bof3::Img(kPages.v) + 2));
        const unsigned char count = BRest4e(::bof3::Img(kPages.v) + list);
        if (left < count) break;
        left = static_cast<unsigned char>(left - count);
        ++list;
    }
    SH_CALL(CommuRank_DrawBackdrop)(0x20, 0x18);
    if (list == 0)
        SH_CALL(CommuRank_DrawListA)(0x28, 0x28, left);
    else if (list == 1)
        SH_CALL(CommuRank_DrawListB)(0x28, 0x28, left);
    else
        SH_CALL(CommuRank_DrawListC)(0x28, 0x28, left);
    const auto total = static_cast<unsigned char>(BRest4e(::bof3::Img(kPages.v) + 2) + BRest4e(::bof3::Img(kPages.v) + 1) + BRest4e(::bof3::Img(kPages.v)));
    const unsigned pressed = Input_Pressed;
    if (pressed & 0x2000) {
        const auto page = static_cast<unsigned char>(static_cast<unsigned char>(CommuList_Page) + 1);
        CommuList_Page = page;
        if (static_cast<signed char>(page) >= static_cast<int>(total)) CommuList_Page = 0;
    }
    if (pressed & 0x8000) {
        const auto page = static_cast<unsigned char>(static_cast<unsigned char>(CommuList_Page) - 1);
        CommuList_Page = page;
        if (static_cast<signed char>(page) < 0) BRest4e(::bof3::Img(kPage.v)) = static_cast<unsigned char>(total - 1);
    }
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(move_script::At(::bof3::Img(kTextBuffer.v))), reinterpret_cast<const char*>(move_script::At(::bof3::Img(kPageFormat.v))),
                         CommuList_Page + 1, static_cast<unsigned>(total));
    SH_CALL(Text_DrawFont8)(0xEE, 0x2C, 0, move_script::At(::bof3::Img(kTextBuffer.v)));
    if (Input_Pressed & (Field_ConfirmButtons | Field_CancelButtons)) ++Rest4b_BoardState;
}

// original 0x460720: CommuRank_Modes[2] - 0x9039F4 + 1 (the facility table
// 0x652AE4's next entry).
extern "C" void __cdecl CommuRank_Close(void) { ++AreaW0c_TailState; }

// original 0x460730: the first list's page `page` (its low byte) at (x, y):
// its heading (icon 1, message 0x38, the count 0x9039A0), then up to seven
// rows from 0x9039C0 + 7 page, each the name of the entry its low 7 bits name
// at (x + 0x14, y + 0x2C + 16 i) and message 0x3A (bit 7 set) or 0x39 at (x +
// 0x70, ...); the count read again each row.
//
// DIV-0064 group 14 (amended 2026-11-01): x + 0x70 is the Japanese column
// (COMMU05.EMI's twin 0x801D1048 on the JP disc, as the PC); the four Western
// discs draw the message at x + 0x48, where `Due to food shortage` ends on
// the board - from x + 0x70 it ran off its right edge. A Latin language takes
// the West's column; Japanese, Chinese and the original keep 0x70.
extern "C" void __cdecl CommuRank_DrawListA(int x, int y, unsigned page) {
    ListHeading(x, y, 1, 0x38, ::bof3::Img(Rest4a_RemovedCount_addr));
    const int cause_x = x + (rest_4e::g_west_cause_column ? 0x48 : 0x70);
    const unsigned base = 7 * (page & 0xFF);
    for (unsigned i = 0; i < 7; ++i) {
        if (static_cast<int>(base + i) >= Rest4a_RemovedCount) break;
        const int row = 16 * static_cast<int>(i) + y + 0x2C;
        SH_CALL(Text_DrawAt)(x + 0x14, row, 0, 5, SlotName(BRest4e(::bof3::Img(kListAEntries.v) + base + i) & 0x7F));   // DIV-0200
        const unsigned id = BRest4e(::bof3::Img(kListAEntries.v) + base + i) & 0x80 ? 0x3A : 0x39;
        SH_CALL(Text_DrawAt)(cause_x, row, 0, 0xFF, Message(id));
    }
}

// original 0x460890: icon `icon` (its low byte) of a strip of 12 x 12 icons as
// a SPRT at (x, y) (low words signed): u = 12 icon - 0x38 (a byte), v 0xF0,
// clut 0x7849, texture page 0x1E, grey.
extern "C" void __cdecl CommuRank_DrawIcon(int x, int y, unsigned icon) {
    DrawMode(0, 0x1E, nullptr);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(p);
    Put<float>(p, 8, F(S16(static_cast<U>(x))));
    Put<float>(p, 0xC, F(S16(static_cast<U>(y))));
    p[0x14] = static_cast<unsigned char>((icon & 0xFF) * 12 - 0x38);
    p[0x15] = 0xF0;
    Put<std::uint16_t>(p, 0x18, 0xC);
    Put<std::uint16_t>(p, 0x1A, 0xC);
    Put<std::uint16_t>(p, 0x16, 0x7849);
    Grey(p);
    Commit(0x1C);
}

// original 0x460920: the second list's page `page` (its low byte) at (x, y):
// its heading (icon 0, message 0x3B, the count 0x904A90), then up to 20 names,
// three to a row (x + 0x12 + 0x48 (i % 3), y + 0x2C + 16 (i / 3)), entry i of
// the page while 20 page + i is below the count (read again each time) - its
// name the entry the byte 0x904F00 + 15 page + i names: 15, where the count
// steps by 20 (docs/rest_4e.md section 7, L4).
extern "C" void __cdecl CommuRank_DrawListB(int x, int y, unsigned page) {
    ListHeading(x, y, 0, 0x3B, ::bof3::Img(Rest4b_BusyA_addr));
    const unsigned p = page & 0xFF;
    for (unsigned i = 0; i < 20; ++i) {
        if (static_cast<int>(20 * p + i) >= Rest4b_BusyA) break;
        const unsigned entry = BRest4e(::bof3::Img(kListBEntries.v) + 15 * p + i);
        const int column = static_cast<int>(i % 3), line = static_cast<int>(i / 3);
        SH_CALL(Text_DrawAt)(x + 72 * column + 0x12, 16 * line + y + 0x2C, 0, 5, SlotName(entry));   // DIV-0200
    }
}

// original 0x460A40: the third list's page `page` (its low byte) at (x, y):
// script message 0xEC at (x + 0x52, y), then up to eight rows from the pairs
// 0x904CA0 + 16 page (kind, entry) while 8 page + i is below 0x937F80 (read
// again each row): a kind whose word +2 of 0x653180 is not 0 puts the entry's
// five name bytes into Text_Records 0 (a NUL at +5); then the kind's message
// (the word +0 of 0x653180) at (x + 0x14, y + 0x1C + 16 i). 0x653180 has eight
// kinds (0x6531A0 follows) and 0x9048F0 60 names; past them the original reads
// on, and so does ours (reads only).
extern "C" void __cdecl CommuRank_DrawListC(int x, int y, unsigned page) {
    SH_CALL(Text_DrawAt)(x + 0x52, y, 0, 0xFF, Message(0xEC));
    const unsigned p = page & 0xFF;
    for (unsigned i = 0; i < 8; ++i) {
        if (static_cast<int>(8 * p + i) >= Rest4b_BusyB) break;
        const unsigned char* const pair = move_script::At(::bof3::Img(kListCPairs.v) + 16 * p + 2 * i);
        const unsigned kind = pair[0];
        if (W(::bof3::Img(kPairMessages.v) + 4 * kind + 2) != 0) {
            BRest4e(::bof3::Img(kTextRecords.v) + 5) = 0;
            const unsigned n = pair[1];
            SetL(::bof3::Img(kTextRecords.v), L(::bof3::Img(kEntryNames.v) + 5 * n));
            BRest4e(::bof3::Img(kTextRecords.v) + 4) = BRest4e(::bof3::Img(kEntryNames.v) + 5 * n + 4);
        }
        SH_CALL(Text_DrawAt)(x + 0x14, 16 * static_cast<int>(i) + y + 0x1C, 0, 0xFF,
                             Message(W(::bof3::Img(kPairMessages.v) + 4 * kind)));
    }
}

// original 0x460B20: the lists' backdrop at (x, y), texture page 0x1D: a top row
// of 16 tiles of 16 (0x6531A0: u, v pairs), four columns of 64 (x + 64 c) of
// ten rows (y + 16 + 16 r), two wide tiles each (u = (b & 1) << 5, v = (b &
// 0xFE) << 3 of the bytes 0x6531E0 + 2 r, + 1), and a bottom row of 16 at y +
// 0xB0 (0x6531C0).
extern "C" void __cdecl CommuRank_DrawBackdrop(int x, int y) {
    DrawMode(0, 0x1D, nullptr);
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned char* const t = move_script::At(::bof3::Img(kTilesTop.v) + 2 * i);
        SH_CALL(CommuRank_DrawTile)(x + 16 * static_cast<int>(i), y, t[0], t[1], 0);
    }
    for (int c = 0; c < 4; ++c) {
        const int cx = 64 * c + x;
        for (unsigned r = 0; r < 10; ++r) {
            const int ry = 16 * static_cast<int>(r + 1) + y;
            const unsigned char a = BRest4e(::bof3::Img(kTilesSide.v) + 2 * r);
            SH_CALL(CommuRank_DrawTile)(cx, ry, static_cast<unsigned char>((a & 1) << 5),
                                        static_cast<unsigned char>((a & 0xFE) << 3), 1);
            const unsigned char b = BRest4e(::bof3::Img(kTilesSide.v) + 2 * r + 1);
            SH_CALL(CommuRank_DrawTile)(cx + 0x20, ry, static_cast<unsigned char>((b & 1) << 5),
                                        static_cast<unsigned char>((b & 0xFE) << 3), 1);
        }
    }
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned char* const t = move_script::At(::bof3::Img(kTilesBottom.v) + 2 * i);
        SH_CALL(CommuRank_DrawTile)(x + 16 * static_cast<int>(i), y + 0xB0, t[0], t[1], 0);
    }
}

// original 0x460C40: a tile at (x, y) (low words signed) as a SPRT: u, v the
// low bytes of their arguments, 16 high, 16 (wide 0) or 32 (wide 1) across -
// (wide's low byte + 1) x 16 - clut 0x7883, grey.
extern "C" void __cdecl CommuRank_DrawTile(int x, int y, unsigned u, unsigned v, unsigned wide) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(p);
    Put<float>(p, 8, F(S16(static_cast<U>(x))));
    Put<float>(p, 0xC, F(S16(static_cast<U>(y))));
    p[0x15] = static_cast<unsigned char>(v);
    p[0x14] = static_cast<unsigned char>(u);
    Put<std::uint16_t>(p, 0x18, static_cast<std::uint16_t>(((wide & 0xFF) + 1) << 4));
    Put<std::uint16_t>(p, 0x1A, 0x10);
    Put<std::uint16_t>(p, 0x16, 0x7883);
    Grey(p);
    Commit(0x1C);
}

// original 0x460CB0 (ConfigMenu_Body's tail jump; hidden in 0x460C40): jmp
// [ConfigScreen_States + 0x929F02 * 4] - 0 the opening, 1 the top bar, 2 the
// rows, 3 the controller panel, 4 the closing. Unbounded: ours aborts past 5.
extern "C" void __cdecl ConfigScreen_Run(void) {
    Dispatch("ConfigScreen_Run", ::bof3::Img(ConfigScreen_States_addr), rest_4f::at::kConfigStateCount, FieldS_Step, "the state byte 0x929F02");
}

// original 0x460CC0 (ConfigScreen_States[0]): jmp [ConfigScreen_OpenStates +
// 0x929F03 * 4] - 0 a fade started, 1 its wait, 2 the slide in
// (ConfigMenu_Open sets 2). Unbounded: ours aborts past 3.
extern "C" void __cdecl ConfigScreen_OpenRun(void) {
    Dispatch("ConfigScreen_OpenRun", ::bof3::Img(ConfigScreen_OpenStates_addr), rest_4f::at::kConfigOpenCount, Rest2d_MenuSub,
             "the opening byte 0x929F03");
}

// original 0x460CD0 (ConfigScreen_OpenStates[0]): Transition_Start(1); the
// cursor 0; 0x929F03 up one (read after the call).
extern "C" void __cdecl ConfigScreen_OpenFade(void) {
    SH_CALL(Transition_Start)(1);
    const unsigned char n = Rest2d_MenuSub;
    MenuLists_Cursor = 0;
    Rest2d_MenuSub = static_cast<unsigned char>(n + 1);
}

// original 0x460CF0 (ConfigScreen_OpenStates[1]): the screen drawn at rest
// (the backdrop, the banner box, the panel at (0x1C, 0x40), the top bar with no
// button lit); once Transition_Busy is 0, the counter and 0x929F03 0 and
// the state on.
extern "C" void __cdecl ConfigScreen_OpenWait(void) {
    Backdrop();
    TitleBox(0x10);
    SH_CALL(Config_DrawPanel)(0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(0xFF, 0x70, 0x26);
    if (Transition_Busy != 0) return;
    const unsigned char st = FieldS_Step;
    BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) = 0;
    Rest2d_MenuSub = 0;
    FieldS_Step = static_cast<unsigned char>(st + 1);
}

// original 0x460D50 (ConfigScreen_OpenStates[2]): the cursor 0; sound 0x102 when
// the counter is 5; the screen drawn slid by the counter c (read again after
// each draw): the banner box at y 0x10 - 16c, the panel at x 0x1C + 64c, the top
// bar at y 0x26 - 16c; the counter down one, at 0 the state on (0x929F03 0).
extern "C" void __cdecl ConfigScreen_SlideIn(void) {
    const unsigned char c0 = BRest4f(::bof3::Img(rest_4f::at::kCounter.v));
    MenuLists_Cursor = 0;
    if (c0 == 5) Sound(0x102);
    Backdrop();
    TitleBox(0x10 - static_cast<int>(BRest4f(::bof3::Img(rest_4f::at::kCounter.v))) * 16);
    SH_CALL(Config_DrawPanel)(static_cast<int>(BRest4f(::bof3::Img(rest_4f::at::kCounter.v))) * 64 + 0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(0xFF, 0x70, 0x26 - static_cast<int>(BRest4f(::bof3::Img(rest_4f::at::kCounter.v))) * 16);
    const unsigned char n = static_cast<unsigned char>(BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) - 1);
    BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) = n;
    if (n != 0) return;
    const unsigned char st = FieldS_Step;
    BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) = 0;
    Rest2d_MenuSub = 0;
    FieldS_Step = static_cast<unsigned char>(st + 1);
}

// original 0x460E10 (ConfigScreen_States[1], and the cell 0x63F298): the top
// bar. The counter 0; the screen drawn, the banner's text system message 0xBC +
// the cursor, the hand at x = cursor * 48 + 0x76. Input_AutoRepeat of
// Input_Pressed & 0xF000: 0xA000 flips the cursor between 0 and 1 (sound
// 0x101); 0x1000 enters the rows at cursor 7, 0x4000 at cursor 2 (sound 0x100).
// Then Input_Pressed (a dword read once): a confirm button on cursor 0 leaves
// (sounds 0x106 and 0x102 unless Game_Step is 4; the state + 3, the counter and
// 0x929F03 0); on cursor 1 the defaults (sound 0x104, the top bar and the hand
// drawn again): the settings 0 (row 0's 1), six button words. Else, at Game_Step
// 4, bit 0x800 leaves without a sound; elsewhere a cancel button leaves with the
// two sounds.
extern "C" void __cdecl ConfigScreen_TopBar(void) {
    const unsigned char bd = Config_Background;
    BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) = 0;
    SH_CALL(Menu_DrawBackdrop)(bd);
    TitleBox(0x10);
    TitleText(CursorId());
    SH_CALL(Config_DrawPanel)(0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(0xFF, 0x70, 0x26);
    TopHand();
    const U rep = Repeat();
    if (rep & 0xA000) {
        Sound(0x101);
        MenuLists_Cursor = static_cast<unsigned char>(MenuLists_Cursor) == 0 ? 1 : 0;
    }
    if (rep & 0x1000) {
        Sound(0x100);
        const unsigned char st = FieldS_Step;
        MenuLists_Cursor = 7;
        FieldS_Step = static_cast<unsigned char>(st + 1);
        return;
    }
    if (rep & 0x4000) {
        Sound(0x100);
        const unsigned char st = FieldS_Step;
        MenuLists_Cursor = 2;
        FieldS_Step = static_cast<unsigned char>(st + 1);
        return;
    }
    const U pressed = LRest4f(::bof3::Img(rest_4f::at::kPressedDword.v));
    if (Field_ConfirmButtons & pressed & 0xFFFFu) {
        if (static_cast<unsigned char>(MenuLists_Cursor) == 0) {
            if (Game_Step != 4) {
                Sound(0x106);
                Sound(0x102);
            }
            const unsigned char st = FieldS_Step;
            Rest2d_MenuSub = 0;
            BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) = 0;
            FieldS_Step = static_cast<unsigned char>(st + 3);
            return;
        }
        Sound(0x104);
        SH_CALL(ConfigScreen_DrawButtons)(static_cast<unsigned char>(MenuLists_Cursor), 0x70, 0x26);
        TopHand();
        SetDefaults();
        return;
    }
    if (Game_Step == 4) {
        if (!(pressed & 0x800)) return;
    } else {
        if (!(Field_CancelButtons & pressed & 0xFFFFu)) return;
        Sound(0x106);
        Sound(0x102);
    }
    const unsigned char st = FieldS_Step;
    BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) = 0;
    Rest2d_MenuSub = 0;
    FieldS_Step = static_cast<unsigned char>(st + 3);
}

// original 0x461070 (ConfigScreen_States[2]): the six rows. The screen drawn,
// the banner's text by the cursor, the top bar with the cursor lit. By the
// cursor - 2 (a jump table of six in the function, bounded): rows 0..2 step
// their setting (0x903A58 in 0..2, 0x903A5A and 0x903A5B in 0..3; row 2 tests
// down first), rows 3 and 4 flip theirs on 0xA000 (Input_Pressed as read before
// the switch), row 5 on a confirm button or 0xA000 opens the controller panel
// (the state + 1, cursor 8). Then Input_AutoRepeat: 0x1000 the cursor down one,
// below 2 back to the top bar (the state - 1, cursor 0); 0x4000 up one, past 7
// the same; either sets the counter to 1. Else a cancel button or 0x800 back to
// the top bar (sound 0x106), and the counter up one while below 3.
extern "C" void __cdecl ConfigScreen_Rows(void) {
    Backdrop();
    TitleBox(0x10);
    TitleText(CursorId());
    SH_CALL(Config_DrawPanel)(0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(static_cast<unsigned char>(MenuLists_Cursor), 0x70, 0x26);
    const U row = static_cast<U>(CursorS() - 2);
    if (row <= 5) {
        const U held = LRest4f(::bof3::Img(rest_4f::at::kPressedDword.v));
        switch (row) {
        case 0: StepSetting(::bof3::Img(ModeFlow_ConfigBytes_addr), 2, false); break;
        case 1: StepSetting(::bof3::Img(Config_WindowColour_addr), 3, false); break;
        case 2: StepSetting(::bof3::Img(Config_Background_addr), 3, true); break;
        case 3:
            if (held & 0xA000) {
                Sound(0x101);
                Rest4f_Setting3 = static_cast<unsigned char>(Rest4f_Setting3 ^ 1);
            }
            break;
        case 4:
            if (held & 0xA000) {
                Sound(0x101);
                FieldE2_RunDefault = static_cast<unsigned char>(FieldE2_RunDefault ^ 1);
            }
            break;
        default: {
            const U buttons = Field_ConfirmButtons | 0xA000u;
            if (Input_Pressed & buttons) {
                Sound(0x101);
                const unsigned char st = FieldS_Step;
                MenuLists_Cursor = 8;
                FieldS_Step = static_cast<unsigned char>(st + 1);
                return;
            }
            break;
        }
        }
    }
    const U rep = Repeat();
    if (rep & 0x1000) {
        Sound(0x100);
        const unsigned char c = static_cast<unsigned char>(static_cast<unsigned char>(MenuLists_Cursor) - 1);
        MenuLists_Cursor = c;
        if (static_cast<signed char>(c) < 2) {
            const unsigned char st = FieldS_Step;
            MenuLists_Cursor = 0;
            FieldS_Step = static_cast<unsigned char>(st - 1);
        }
        BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) = 1;
        return;
    }
    if (rep & 0x4000) {
        Sound(0x100);
        const unsigned char c = static_cast<unsigned char>(static_cast<unsigned char>(MenuLists_Cursor) + 1);
        MenuLists_Cursor = c;
        if (static_cast<signed char>(c) > 7) {
            const unsigned char st = FieldS_Step;
            MenuLists_Cursor = 0;
            FieldS_Step = static_cast<unsigned char>(st - 1);
        }
        BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) = 1;
        return;
    }
    const U cancel = Field_CancelButtons | 0x800u;
    if (Input_Pressed & cancel) {
        Sound(0x106);
        const unsigned char st = FieldS_Step;
        MenuLists_Cursor = 0;
        FieldS_Step = static_cast<unsigned char>(st - 1);
    }
    const unsigned char n = BRest4f(::bof3::Img(rest_4f::at::kCounter.v));
    if (n < 3) BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) = static_cast<unsigned char>(n + 1);
}

// original 0x4613B0 (ConfigScreen_States[3]; in no list of the cut, inside the
// span the catalog gave 0x461070): the controller panel. The screen drawn, the
// banner's text system message 0xBC + the cursor, or on cursor 9 0xC5 / 0xCA by
// row 4's setting; Config_DrawControllerPanel(0xA8, 0x62); the hand at (0xB0,
// 18 * cursor - 0x26). A face button (Input_Pressed's low byte & 0xFC, its lowest
// bit kept; sound 0x103): bits 0 and 1 cleared from the words 0x903580 and
// Field_ConfirmButtons; the row whose word holds the bit takes the cursor row's
// word, the cursor row's word becomes the bit; bits 0 and 1 set again. Then
// Input_AutoRepeat: 0x1000 / 0x4000 the cursor down / up through 8..13,
// wrapping with a second sound; else 0xA000 back to the rows at cursor 7 (sound
// 0x101), 0x800 to the top bar (the state - 2, cursor 0, sound 0x106).
extern "C" void __cdecl ConfigScreen_Controller(void) {
    Backdrop();
    TitleBox(0x10);
    SH_CALL(Config_DrawPanel)(0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(static_cast<unsigned char>(MenuLists_Cursor), 0x70, 0x26);
    const unsigned char cur = static_cast<unsigned char>(MenuLists_Cursor);
    const U id = cur == 9 ? (FieldE2_RunDefault != 0 ? 0xCAu : 0xC5u)
                          : static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(cur))) + 0xBC;
    TitleText(id);
    SH_CALL(Config_DrawControllerPanel)(0xA8, 0x62);
    SH_CALL(Menu_DrawHand)(0xB0, CursorS() * 18 - 0x26, 0);
    U bits = Input_Pressed & 0xFCu;
    if (bits != 0) {
        Sound(0x103);
        SetWRest4f(::bof3::Img(rest_4f::at::kButtons.v), WRest4f(::bof3::Img(rest_4f::at::kButtons.v)) & 0xFFFC);
        Field_ConfirmButtons = static_cast<unsigned short>(Field_ConfirmButtons & 0xFFFC);
        unsigned i = 0;
        for (; i < 16; ++i)
            if (bits & (1u << i)) break;
        if (i < 16)
            for (++i; i < 16; ++i) bits &= ~(1u << i);
        unsigned r = 0;
        for (; r < rest_4f::at::kPadRowCount; ++r)
            if (WRest4f(::bof3::Img(rest_4f::at::kButtons.v) + 2u * BRest4f(::bof3::Img(rest_4f::at::kPadRows.v) + r)) & bits) break;
        const unsigned char c = static_cast<unsigned char>(MenuLists_Cursor);
        const U by_cursor = ::bof3::Img(rest_4f::at::kPadRowsByCursor.v) + static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(c)));
        if (r != rest_4f::at::kPadRowCount) {
            const U from = ::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000 + 2u * BRest4f(by_cursor);
            const U to = ::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000 + 2u * BRest4f(::bof3::Img(rest_4f::at::kPadRows.v) + r);
            SetWRest4f(to, WRest4f(from));
        }
        SetWRest4f(::bof3::Img(rest_4f::at::kButtons.v) + 2u * BRest4f(by_cursor), bits);
        SetWRest4f(::bof3::Img(rest_4f::at::kButtons.v), WRest4f(::bof3::Img(rest_4f::at::kButtons.v)) | 3);
        Field_ConfirmButtons = static_cast<unsigned short>(Field_ConfirmButtons | 3);
        // DIV-0133 (docs/button-sigma.md): words 7 / 8 as Speak's and
        // Action's and the table - nothing until sigma::Arm, after every
        // module's self-test.
        bof3x::input::sigma::Rebound();
    }
    const U rep = Repeat();
    if (rep & 0x1000) {
        Sound(0x100);
        const unsigned char c = static_cast<unsigned char>(static_cast<unsigned char>(MenuLists_Cursor) - 1);
        MenuLists_Cursor = c;
        if (static_cast<signed char>(c) >= 8) return;
        Sound(0x100);
        MenuLists_Cursor = 0xD;
        return;
    }
    if (rep & 0x4000) {
        Sound(0x100);
        const unsigned char c = static_cast<unsigned char>(static_cast<unsigned char>(MenuLists_Cursor) + 1);
        MenuLists_Cursor = c;
        if (static_cast<signed char>(c) <= 0xD) return;
        Sound(0x100);
        MenuLists_Cursor = 8;
        return;
    }
    const U p = LRest4f(::bof3::Img(rest_4f::at::kPressedDword.v));
    if (p & 0xA000) {
        Sound(0x101);
        const unsigned char st = FieldS_Step;
        MenuLists_Cursor = 7;
        FieldS_Step = static_cast<unsigned char>(st - 1);
        return;
    }
    if (p & 0x800) {
        Sound(0x106);
        const unsigned char st = FieldS_Step;
        MenuLists_Cursor = 0;
        FieldS_Step = static_cast<unsigned char>(st + 0xFE);
    }
}

// original 0x461620 (ConfigScreen_States[4]): the closing. The backdrop; at
// Game_Step 4 (the title's Config) the screen drawn at rest, Transition_Start(0)
// and Game_Step 5; else the screen slid by the counter c (read again after each
// draw, as the slide in) with no button lit, the counter up one, at 5 the
// screen's step 0x929F01 up one (ConfigMenu_Close).
extern "C" void __cdecl ConfigScreen_SlideOut(void) {
    Backdrop();
    if (Game_Step == 4) {
        TitleBox(0x10);
        SH_CALL(Config_DrawPanel)(0x1C, 0x40);
        SH_CALL(ConfigScreen_DrawButtons)(0, 0x70, 0x26);
        SH_CALL(Transition_Start)(0);
        Game_Step = 5;
        return;
    }
    TitleBox(0x10 - static_cast<int>(BRest4f(::bof3::Img(rest_4f::at::kCounter.v))) * 16);
    SH_CALL(Config_DrawPanel)(static_cast<int>(BRest4f(::bof3::Img(rest_4f::at::kCounter.v))) * 64 + 0x1C, 0x40);
    SH_CALL(ConfigScreen_DrawButtons)(0, 0x70, 0x26 - static_cast<int>(BRest4f(::bof3::Img(rest_4f::at::kCounter.v))) * 16);
    const unsigned char n = static_cast<unsigned char>(BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) + 1);
    BRest4f(::bof3::Img(rest_4f::at::kCounter.v)) = n;
    if (n == 5) FieldS_State = static_cast<unsigned char>(FieldS_State + 1);
}

// original 0x4616F0 (cdecl (sel, x, y)): Menu_DrawButtonRow(x, y, set 6, sel,
// the style byte - the callee's unused fifth).
extern "C" void __cdecl ConfigScreen_DrawButtons(int sel, int x, int y) {
    SH_CALL(Menu_DrawButtonRow)(x, y, 6, sel, static_cast<unsigned char>(Config_WindowColour));
}

// original 0x461710 (cdecl (x, y)): the panel. The five settings read first (rows
// 0..4: 0x903A58, 0x903A5A, 0x903A5B, 0x903A59, 0x903A5E; row 5 none); the frame
// (x, y, 0x21, 0xD) through the call at 0x461778 (DIV-0011's Menu_DrawFrame, as
// menu_frame.cpp holds it); then for each row at (x + 5, y + 9 + 15 * row): Config_DrawRowLabel
// and Config_DrawRowOptions, each with the row's state (RowState, read afresh).
extern "C" void __cdecl Config_DrawPanel(int x, int y) {
    const unsigned char settings[6] = {static_cast<unsigned char>(ModeFlow_ConfigBytes), static_cast<unsigned char>(Config_WindowColour), Config_Background, Rest4f_Setting3, FieldE2_RunDefault, 0};
    SH_WORD(Frame, MenuFrame_Draw())(x, y, 0x21, 0xD);
    const int x5 = x + 5;
    int row_y = y + 9;
    for (unsigned row = 0; row < 6; ++row) {
        SH_CALL(Config_DrawRowLabel)(x5, row_y, row, RowState(row));
        const U state = RowState(row);
        SH_CALL(Config_DrawRowOptions)(x5, row_y, row, settings[row], state);
        row_y += 0xF;
    }
}

// original 0x461800 (called by the panel draw Config_DrawPanel for each of the six
// rows; docs/config-screen.md): the row's box Menu_DrawBox(x, y - s, 0xF9,
// 2 s + 0xB, 0, the style byte 0x903A5A), s the fourth argument's byte. Its
// label is one of six strings the original puts on its stack as immediates,
// by the row byte (unchecked: past six it reads its own frame - ours aborts).
// s == 3 (the row under the cursor): the label large, right-aligned -
// Text_DrawAt(x - len * 6 + 0x3A, y - 1, 0, 0xFF, label) - and, unless the
// menu step 0x929F02 is 1, Menu_DrawHand(x + 4, y, 0); else small,
// Text_DrawSmall(x - len * 4 + 0x3A, y + 1, 0, 0xFF, label). Then a line
// primitive at Gfx_PacketNext, read before Gpu_SetLineF2: grey 0x80, from
// (x16 + 0x74, y16 - s + 1) to (x16 + 0x74, y16 + s + 9) as floats, x16 and y16
// the arguments' low words; Gfx_CommitPrim(1, 0x20).
//
// DIV-0015 and DIV-0017 (config_text.cpp) rewrite this function's operands in
// the original's code: the six label immediates, the two 0x3A anchors, the
// large branch's `len * 6` and its call. Ours reads each as config_text.cpp
// holds it (ConfigText_Operands; docs/operand-store.md) at every call, so the
// patches hold for ours unchanged. The
// box's y and height take the fourth argument's low byte where the original
// takes eax's low word with its caller's upper half (the caller's x): the box
// reads the low words only (Menu_DrawBox, menu_windows.cpp).
extern "C" void __cdecl Config_DrawRowLabel(int x, int y, unsigned row, unsigned state) {
    const int s = static_cast<int>(state & 0xFF);
    const unsigned char style = static_cast<unsigned char>(Config_WindowColour);
    SH_CALL(Menu_DrawBox)(x, y - s, 0xF9, 2 * s + 0xB, 0, style);
    const unsigned r = row & 0xFF;
    if (r >= 6)
        bof3::Fatal("Config_DrawRowLabel: row %u - the original reads its own stack frame past the six labels", r);
    const ConfigTextOperands& ops = ConfigText_Operands();   // at::kRowLabelOperands .. kRowAnchorSmall, as config_text.cpp holds them
    const auto* const label = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(ops.label[r]));
    const int len = static_cast<int>(std::strlen(reinterpret_cast<const char*>(label)));
    // DIV-0196 (ops.label_left, ja-JP): the `sub` that takes the width off x is
    // two nops in the original's code, so the label is left-aligned at x + anchor.
    if (s == 3) {
        const int width = ops.label_x4 ? len * 4 : len * 6;   // DIV-0017's shl ecx,2, or the original's lea / shl 1
        const int anchor = ops.anchor;
        const U big = ops.big_draw;
        using Draw = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);
        SH_WORD(Draw, big)(x - (ops.label_left ? 0 : width) + anchor, y - 1, 0, 0xFF, label);
        if (FieldS_Step != 1) SH_CALL(Menu_DrawHand)(x + 4, y, 0);
    } else {
        const int anchor = ops.anchor;
        SH_CALL(Text_DrawSmall)(x - (ops.label_left ? 0 : len * 4) + anchor, y + 1, 0, 0xFF, label);
    }
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    const int xs = static_cast<int>((static_cast<U>(x) & 0xFFFFu) + 0x74);
    const int ys = static_cast<int>(static_cast<U>(y) & 0xFFFFu);
    const float fx = static_cast<float>(xs);
    const float y0 = static_cast<float>(ys - s + 1);
    const float y1 = static_cast<float>(s + ys + 9);
    std::memcpy(prim + 0x14, &fx, 4);
    std::memcpy(prim + 8, &fx, 4);
    prim[4] = prim[5] = 0x80;
    std::memcpy(prim + 0xC, &y0, 4);
    prim[6] = 0x80;
    std::memcpy(prim + 0x18, &y1, 4);
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x461970 (cdecl (x, y, row, setting, state)): a row's options. The
// row's byte names its first record and its count (0x653808 / 0x653810, read in
// place; the count read again each pass); each 16-byte record (a count, an s8
// x, the string): colour 2 for the option the setting names, else 0 (in the row
// argument's low byte); x = the record's x (as a 16-bit word) - half the width +
// x + 0x74. The chosen option with the state 3 is drawn large - the width count
// * 12, or * 8 under DIV-0017 (the `lea` at 0x4619E1, as config_text.cpp holds it), through the
// call at 0x4619F9 (Text_DrawAt, or ConfigText_DrawSelected) at y - 2; the
// rest small (count * 8, Text_DrawSmall) at y + 1.
extern "C" void __cdecl Config_DrawRowOptions(int x, int y, unsigned row, unsigned setting, unsigned state) {
    const U r = row & 0xFF;
    const U first = BRest4f(::bof3::Img(rest_4f::at::kOptionFirst.v) + r);
    if (BRest4f(::bof3::Img(rest_4f::at::kOptionCount.v) + r) == 0) return;
    U rec = ::bof3::Img(rest_4f::at::kOptionRecords.v) + first * 16;
    const unsigned chosen = setting & 0xFF;
    for (unsigned i = 0;;) {
        const U colour = (row & 0xFFFFFF00u) | (chosen == i ? 2u : 0u);
        const unsigned char n = BRest4f(rec);
        const U rx = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<signed char>(BRest4f(rec + 1))));
        const unsigned char* const text = move_script::At(rec + 2);
        if (chosen == i && state == 3) {
            const ConfigTextOperands& ops = ConfigText_Operands();   // at::kOptionWidth, kOptionBigSite
            const U width = ops.option_x8 ? n * 8u : n * 12u;   // DIV-0017's shl eax,1, or the original's lea
            const U xc = rx - width / 2 + static_cast<U>(x) + 0x74;
            SH_WORD(DrawText, ops.big_draw)(static_cast<int>(xc), y - 2, static_cast<int>(colour), n, text);
        } else {
            const U xc = rx - (n * 8u) / 2 + static_cast<U>(x) + 0x74;
            SH_CALL(Text_DrawSmall)(static_cast<int>(xc), y + 1, colour, n, text);
        }
        rec += 16;
        ++i;
        if (static_cast<std::int16_t>(i) >= static_cast<std::int16_t>(BRest4f(::bof3::Img(rest_4f::at::kOptionCount.v) + r))) break;
    }
}

// original 0x461A50 (cdecl (x, y)): the controller panel. The frame (x, y, w, h)
// through the call at 0x461A84 (DIV-0011), w and h the immediates at 0x461A62
// (0xC; DIV-0026 / DIV-0051 make it 0xD; config_text.cpp's value) and 0x461A60 (0xF); then
// for rows 0..5 at y' = y + 18 * row + 6: Config_DrawControllerRow(x + 5, y', the
// row in the y argument's low byte) and Config_DrawControllerCell(x + 0x58, y',
// the button word 0x903580 + 2 * {0, 1, 6, 2, 3, 4}[row] under y's high half).
extern "C" void __cdecl Config_DrawControllerPanel(int x, int y) {
    const int w = ConfigText_Operands().frame_w;   // at::kPadFrameW: 0xC, DIV-0051's 0xD
    const int h = kPadFrameRows;                    // at::kPadFrameH, never patched
    SH_WORD(Frame, MenuFrame_Draw())(x, y, w, h);
    static const unsigned char kWords[6] = {0, 1, 6, 2, 3, 4};
    const U hi = static_cast<U>(y);
    for (unsigned row = 0; row < 6; ++row) {
        const int ry = y + static_cast<int>(row) * 18 + 6;
        SH_CALL(Config_DrawControllerRow)(x + 5, ry, static_cast<int>((hi & 0xFFFFFF00u) | row));
        const U word = WRest4f(::bof3::Img(rest_4f::at::kButtons.v) + 2u * kWords[row]);
        SH_CALL(Config_DrawControllerCell)(x + 0x58, ry, (hi & 0xFFFF0000u) | word);
    }
}

// original 0x461AF0 (cdecl (x, y, row)): one controller row. The row's dark box
// Menu_DrawBox(x, y, w, 0x10, 0, the style byte), w the immediate at 0x461B07
// (0x68; DIV-0051 0x58); the name 0x66A368[row] (ours aborts past six) right
// aligned at x + edge - len * k (edge the imm8 at 0x461B41: 0x20, DIV-0026 0x36;
// k 6, or 4 under DIV-0026's `lea` at 0x461B36) and y + 1 through the call at
// 0x461B43 (Text_DrawAt, or ConfigText_DrawSelected); a grey LINE_F2 at x16 +
// 0x3F from y16 + 1 to y16 + 0xE (x16, y16 the arguments' low words), committed
// (1, 0x20); unless DIV-0051's jump stands at 0x461BAC, a second at x16 + 0x53
// with the first's two y floats copied as dwords.
extern "C" void __cdecl Config_DrawControllerRow(int x, int y, int row) {
    const unsigned char style = static_cast<unsigned char>(Config_WindowColour);
    const ConfigTextOperands& ops = ConfigText_Operands();   // at::kPadBoxWidth .. kPadSecondCell
    SH_CALL(Menu_DrawBox)(x, y, ops.box_width, 0x10, 0, style);
    const U r = static_cast<U>(row) & 0xFF;
    if (r >= rest_4f::at::kPadNameCount)
        bof3::Fatal("Config_DrawControllerRow: row %u, past the six names of 0x%X - the original reads the dword after "
                    "as a string (docs/rest_4f.md section 7)",
                    (unsigned)r, (unsigned)rest_4f::at::kPadNames.v);
    const auto* const name = move_script::At(LRest4f(::bof3::Img(rest_4f::at::kPadNames.v) + 4 * r));
    const U len = static_cast<U>(std::strlen(reinterpret_cast<const char*>(name)));
    const U k = ops.name_x4 ? 4 : 6;   // DIV-0026's lea [ecx+ecx], or the original's [ecx+ecx*2]
    const int edge = ops.name_edge;
    SH_WORD(DrawText, ops.name_draw)(x - static_cast<int>(len * k) + edge, y + 1, 0, 0xFF, name);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    const int xs = static_cast<int>(static_cast<U>(x) & 0xFFFF);
    const int ys = static_cast<int>(static_cast<U>(y) & 0xFFFF);
    const float x0 = static_cast<float>(xs + 0x3F);
    const float y0 = static_cast<float>(ys + 1);
    const float y1 = static_cast<float>(ys + 0xE);
    prim[4] = prim[5] = 0x80;
    std::memcpy(prim + 0x14, &x0, 4);
    std::memcpy(prim + 8, &x0, 4);
    prim[6] = 0x80;
    std::memcpy(prim + 0xC, &y0, 4);
    std::memcpy(prim + 0x18, &y1, 4);
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
    if (!ops.second_cell) return;   // DIV-0051's jmp over the second cell
    unsigned char* const prim2 = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim2);
    const float x1 = static_cast<float>(xs + 0x53);
    std::memcpy(prim2 + 0xC, &y0, 4);
    std::memcpy(prim2 + 0x18, &y1, 4);
    prim2[4] = prim2[5] = 0x80;
    std::memcpy(prim2 + 0x14, &x1, 4);
    std::memcpy(prim2 + 8, &x1, 4);
    prim2[6] = 0x80;
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

namespace {

// The controller panel's cell, original 0x461C00 (docs/controls.md section 2;
// disasm 2026-10-17, 0x461C00..0x461E07). cdecl (x, y, word). The original
// tests the word's bits in the order circle 0x20, cross 0x40, triangle 0x10,
// square 0x80, L1 0x04, R1 0x08 and, for the first it finds, draws
// Text_DrawAt(x + 4, y, colour, 3, 0x66A2FC[i]) - the port's glyph, the
// pointer read in place: colours 2, 1, 6, 5, 0, 0 - then Text_DrawAt(x - 0x10,
// y, 0, count, letter): the default key table's letter, hard-coded at
// 0x65382C, 0x653828, 0x653824, 0x653820, 0x65381C (count 4) and 0x653818
// (known-defects D58). With none of those, select 0x100 (`test ah, 1`) draws
// the three cells of a SELECT label: Gpu_SetDrawMode(Gfx_PacketNext, 0, 0,
// 0xF, 0), Gfx_CommitPrim(1, 0xC), then for u = 6, 7, 8 at x + 1, x + 9,
// x + 0x11: Menu_DrawCell8(x', y + 3, u, 2, Gpu_GetClut(0, 0x1E0), 0x80) - the
// shade the 0x80 the clut call's caller left on the stack. Nothing else.
//
// Under DIV-0051 ours draws one thing, the PlayStation's icon, in that colour,
// where the letter was: the PlayStation screen has one column. Count 3 as
// the original passes it. L2 and R2 have no icon on either side, as before,
// and select none under DIV-0051 (the rebinding's `and ecx, 0xFC` never gives
// a line the bit; a save could).
extern "C" void __cdecl Config_DrawControllerCell(int x, int y, unsigned int word) {
    if (config_text::g_icons) {
        struct Icon { unsigned bit; std::uint32_t glyph; int color; };
        static const Icon kIcons[] = {
            {0x20, kIconCircle, 2}, {0x40, kIconCross, 1},  {0x10, kIconTriangle, 6},
            {0x80, kIconSquare, 5}, {0x04, kIconL1, 0},     {0x08, kIconR1, 0},
        };
        for (const Icon& i : kIcons) {
            if (!(word & i.bit)) continue;
            const unsigned char code[3] = {static_cast<unsigned char>(0x80 | (i.glyph >> 8)),
                                           static_cast<unsigned char>(i.glyph), 0};
            SH_CALL(Text_DrawAt)(x + kIconX, y, i.color, 3, code);
            return;
        }
        return;
    }
    struct Cell { unsigned bit; int color; std::uint32_t letter; int letter_count; };
    static const Cell kCells[] = {
        {0x20, 2, 0x65382C, 3}, {0x40, 1, 0x653828, 3}, {0x10, 6, 0x653824, 3},
        {0x80, 5, 0x653820, 3}, {0x04, 0, 0x65381C, 4}, {0x08, 0, 0x653818, 3},
    };
    constexpr ::bof3::image::Va kGlyphPointers{0x66A2FC};   // the six one-byte glyph strings 0x66A2E0..
    for (unsigned i = 0; i < sizeof kCells / sizeof kCells[0]; ++i) {
        const Cell& c = kCells[i];
        if (!(word & c.bit)) continue;
        const auto* glyph = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(
            *reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(kGlyphPointers.v) + 4 * i))));
        // DIV-0051's 2026-10-21 amendment: under ja-JP the stand-in behind
        // 0x66A2FC is empty (no Japanese layer writes it); the Japan disc's
        // own icon, in the same colour at the same place.
        unsigned char ja[3];
        if (config_text::g_ja_icons) {
            const unsigned g = i < 4 ? button_text::kJapaneseIcons + i : button_text::kJapaneseShoulders + (i - 4);
            if (g >= TextDraw_GlyphCount())
                bof3::Fatal("ConfigJapaneseIcons: glyph 0x%X, the font loaded has %u (no ja-JP layer's icons)", g,
                            TextDraw_GlyphCount());
            ja[0] = static_cast<unsigned char>(0x80 | (g >> 8));
            ja[1] = static_cast<unsigned char>(g);
            ja[2] = 0;
            glyph = ja;
        }
        SH_CALL(Text_DrawAt)(x + 4, y, c.color, 3, glyph);
        SH_CALL(Text_DrawAt)(x - 0x10, y, 0, c.letter_count,
                             reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(::bof3::Img(c.letter))));
        return;
    }
    if (!(word & 0x100)) return;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xF, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    for (unsigned u = 6; u <= 8; ++u) {
        const unsigned clut = SH_CALL(Gpu_GetClut)(0, 0x1E0);
        SH_CALL(Menu_DrawCell8)(static_cast<unsigned>(x) + 1 + 8 * (u - 6), static_cast<unsigned>(y) + 3, u, 2, clut,
                                0x80);
    }
}

}  // namespace

// original 0x461E10 (PSX 0x8014E458): a RECT {x, y, w, h} of 16-bit words on
// the stack, cleared through Gfx_ClearImage with colour 0.
//
// As the original has it: each argument's low 16 bits; a third zero is
// pushed that Gfx_ClearImage does not read (the PSX ClearImage's blue).
extern "C" void __cdecl Gfx_ClearRect(int x, int y, int w, int h) {
    short rect[4] = {static_cast<short>(x), static_cast<short>(y), static_cast<short>(w), static_cast<short>(h)};
    mode_flow::g.clear_image(rect, 0, 0);
}

// original 0x461E50: the primitive just built at Gfx_PacketNext joins ordering
// -table slot `slot`, and the packet pointer moves on by `size` - if the pool
// has room for it, and silently not at all if it has not. Both arguments are
// bytes. 640,316 calls a whole attract cycle.
//
// As the original has it: the slot is not checked against the eight there are.
extern "C" void __cdecl Gfx_CommitPrim(unsigned slot, unsigned size) {
    size &= 0xFF;
    const std::uint32_t limit = (static_cast<std::uint32_t>(Gfx_BufferIndex) << 16) + ::bof3::Img(kPoolLimit);
    const std::uint32_t next = Low32(Gfx_PacketNext.Native());
    if (limit <= next + size) {
        // Diagnostic only (BOF3X_DRAWORDER): the original's silent skip.
        if (draw_order::Tagging())
            bof3::Log("draworder   packet pool full: a %u-byte primitive for slot %u skipped (next %08X, limit %08X) from %08X",
                      size, slot & 0xFF, next, limit, Low32(__builtin_return_address(0)));
        return;
    }
    slot &= 0xFF;
    if (draw_order::g_on)   // BOF3X_DRAWORDER, diagnostic only
        draw_order::TagCommit(next, slot, Low32(__builtin_return_address(0)));
    // DIV-0071 (layering.h): what a map cell's record run draws, for the next
    // pass's verdicts; only while DrawLayer_Open runs a record under mode 1.
    if (layering::g_in_run) layering::RunPrim(Gfx_PacketNext, size);
    Gpu_LinkPrim(Gfx_OtPointers[slot], next);
    Gfx_OtPointers[slot] = reinterpret_cast<unsigned long*>(Gfx_PacketNext.Native());
    Gfx_PacketNext = Gfx_PacketNext + size;
}

// original 0x461EB0 (PSX 0x8014E534): a new press among the bits asked for
// starts a 12-frame wait and latches Input_Held; while a latched bit stays
// held the wait counts down and re-fires every 3 frames. The answer is the
// dword at Input_Held - Input_Previous in the upper half, as the original
// loads it - with the low word 0 when nothing fires.
extern "C" unsigned __cdecl Input_AutoRepeat(unsigned pressed) {
    const std::uint32_t held = static_cast<std::uint32_t>(move_script::Long(reinterpret_cast<unsigned char*>(&Input_Held)));
    if (static_cast<std::uint16_t>(pressed) != 0) {
        ItemUse_RepeatTimer = 0xC;
        item_use::SetWord(item_use::At(::bof3::Img(item_use::at::kRepeatLatch.v)), held);
        return held;
    }
    if ((item_use::Word(item_use::At(::bof3::Img(item_use::at::kRepeatLatch.v))) & held & 0xFFFF) != 0) {
        const auto timer = static_cast<std::uint16_t>(ItemUse_RepeatTimer - 1);
        ItemUse_RepeatTimer = timer;
        if (timer == 0) {
            ItemUse_RepeatTimer = 3;
            item_use::SetWord(item_use::At(::bof3::Img(item_use::at::kRepeatLatch.v)), held);
            return held;
        }
    }
    return held & 0xFFFF0000u;
}

// original 0x461F00. Each queued record: s16 w, s16 h, u32 kind, then data.
// Kind 1 is 5-bit packed, kind 2 LZSS, anything else raw cells.
//
// As the original has it: the index is a byte; the count is read again after
// every record rather than once; and nothing bounds either against the 20
// slots the arrays have (docs/known-defects.md D4 - DIV-0004 is the fix, and
// it is in Gfx_BeginFrame, not here).
extern "C" void __cdecl Gfx_FlushUploadQueue(void) {
    for (std::uint8_t i = 0; i < Gfx_UploadQueueCount; ++i) {
        const unsigned char* record = Gfx_UploadQueueRecord[i];
        short rect[4];
        rect[0] = static_cast<short>(Gfx_UploadQueueX[i]);
        rect[1] = static_cast<short>(Gfx_UploadQueueY[i]);
        std::memcpy(&rect[2], record, 4);
        std::uint32_t kind;
        std::memcpy(&kind, record + 4, 4);
        {   // BOF3X_POSELOG=1: every upload (the facing investigation, issue #142's side thread)
            static int on = -1;
            if (on < 0) { char v[8]; on = GetEnvironmentVariableA("BOF3X_POSELOG", v, sizeof v) != 0 && v[0] == '1'; }
            if (on == 1)
                bof3::Log("poselog     frame %lu upload %u/%u at %d,%d size %dx%d kind %lu record %08lX", static_cast<unsigned long>(Frame_Counter),
                          static_cast<unsigned>(i), static_cast<unsigned>(Gfx_UploadQueueCount), rect[0], rect[1], rect[2], rect[3],
                          static_cast<unsigned long>(kind), static_cast<unsigned long>(Low32(record)));
        }
        if (kind == 1) Gfx_UploadPacked5(record + 8, rect);
        else if (kind == 2) Gfx_UploadLzss(record + 8, rect);
        else Gfx_LoadImage(rect, record + 8);
    }
    Gfx_UploadQueueCount = 0;
}

// original 0x461FC0. Six 5-bit values to a dword, bit 15 unused, one byte out
// for each.
//
// As the original has it: whole dwords are unpacked, so up to four bytes past
// the total are written; the total is signed, and so is the remainder that
// rounds it - a negative total (one of w, h negative) unpacks nothing and
// moves Gfx_UnpackNext BACK.
extern "C" void __cdecl Gfx_UploadPacked5(const void* packed, const short* rect) {
    int total = (rect[3] * rect[2]) << 1;
    if (total > 0) {
        unsigned char* out = Gfx_UnpackNext;
        auto* in = static_cast<const std::uint8_t*>(packed);
        for (std::uint32_t groups = (static_cast<std::uint32_t>(total) + 5) / 6; groups != 0; --groups) {
            std::uint32_t v;
            std::memcpy(&v, in, 4);
            in += 4;
            out[0] = v & 0x1F; v >>= 5;
            out[1] = v & 0x1F; v >>= 5;
            out[2] = v & 0x1F; v >>= 6;
            out[3] = v & 0x1F; v >>= 5;
            out[4] = v & 0x1F; v >>= 5;
            out[5] = v & 0x1F;
            out += 6;
        }
    }
    Gfx_LoadImage(rect, Gfx_UnpackNext);
    const int rem = total % 4;   // truncating, as the original's and/dec/or/inc idiom
    if (rem != 0) total = total - rem + 4;
    Gfx_UnpackNext += total;
}

// original 0x462070. LZSS with a 512-byte window: a flag byte, low bit first,
// then for each bit a literal byte (1) or a two-byte match (0) - window offset
// b0 | (b1 & 0xF0) << 4, length (b1 & 0x0F) + 3. The first four bytes of the
// packed data are skipped.
//
// As the original has it: a match is copied whole even if it runs past the
// total, so up to 17 bytes more than the total are written; the total is
// compared unsigned, and a negative one never ends.
//
// One thing is NOT as the original has it, because the original does not have
// it: it zeroes 0x1EE bytes of the window and leaves the last 18 as whatever
// the stack held, so a stream that reads those before writing them decodes
// differently from call to call (docs/asset-loading-path.md section 2). Here
// they are zero. A valid stream cannot tell. Not in the ledger for that reason:
// there is no original behaviour to differ from - if shipped data is ever
// found that reads them, that changes, and this is where to look.
extern "C" void __cdecl Gfx_UploadLzss(const void* packed, const short* rect) {
    std::uint8_t window[0x200] = {};
    std::uint32_t total = static_cast<std::uint32_t>((rect[3] * rect[2]) << 1);
    auto* in = static_cast<const std::uint8_t*>(packed) + 4;
    unsigned char* out = Gfx_UnpackNext;
    std::uint32_t r = 0x1EE, flags = 0, count = 0;

    if (total != 0) {
        do {
            flags = (flags & 0xFFFF) >> 1;
            if ((flags & 0x100) == 0) flags = *in++ | 0xFF00u;
            if (flags & 1) {
                const std::uint8_t c = *in++;
                *out++ = c;
                ++count;
                window[r] = c;
                r = (r + 1) & 0x1FF;
            } else {
                const std::uint32_t b0 = in[0], b1 = in[1];
                in += 2;
                const std::uint32_t offset = b0 | ((b1 & 0xF0) << 4);
                const std::uint32_t last = (b1 & 0x0F) + 2;
                count += last + 1;
                for (std::uint32_t k = 0; k <= last; ++k) {
                    const std::uint8_t c = window[(offset + k) & 0x1FF];
                    *out++ = c;
                    window[r] = c;
                    r = (r + 1) & 0x1FF;
                }
            }
        } while (count < total);
    }

    Gfx_LoadImage(rect, Gfx_UnpackNext);
    if (total & 3) total += 4 - (total & 3);
    Gfx_UnpackNext += total;
}

// original 0x4621C0: task 1's body once 0x496C90 has loaded DAT 0x226 and
// jumped here (PSX GAME.EMI section 1, FUN_801D0C04). A coroutine: it runs
// on task 1's own stack and never returns - Task_Sleep yields to the
// scheduler and comes back a frame later, and state 7's handler ends the task
// from inside the call below (a tail jump to 0x5A99AD), leaving this frame
// behind on a stack the next Task_Create(1, ..) starts afresh.
//
// As the original has it: the state word indexes Title_States unchecked,
// read afresh after Title_CheckStart (which may have moved it).
extern "C" void __cdecl Title_Task(void) {
    Title_State = 0;
    Title_Step = 0;
    mode_tasks::g.clear_private();
    for (;;) {
        mode_tasks::g.sleep(1);
        mode_tasks::g.check_start();
        reinterpret_cast<void (__cdecl*)()>(Title_States[Title_State].Native())();
        mode_tasks::g.backdrop();
        mode_tasks::g.logo();
    }
}

// original 0x462200: Title_States entry 0 (PSX 0x801D0C90). Loads the title's
// DAT, then starts the backdrop's fade-in and the title music.
//
// As the original has it: Port_DroppedCall(2) first (the PSX's call there,
// emptied by the port); the wait on File_LoadDone sleeps a frame at a time,
// asked first before any sleep (on the PC it is always done); the CLUT strip
// restored and Gfx_ClutStripDirty counted up - not set to 1 - after the
// load; the state word read after the calls.
extern "C" void __cdecl Title_StateLoad(void) {
    title_states::gTitleStates.dropped(2);
    title_states::gTitleStates.load_dat(0x136);
    while (title_states::gTitleStates.load_done() == 0) title_states::gTitleStates.sleep(1);
    title_states::gTitleStates.clut_restore();
    Gfx_ClutStripDirty = static_cast<unsigned char>(Gfx_ClutStripDirty + 1);
    title_states::gTitleStates.music_play(0x8D, 8);
    BumpState();
    Title_Fade = 1;
    Title_Scroll = 0xC8;
    Title_Bright = 0;
    Title_LogoState = 0;
    Title_LogoShadeA = 0;
    Title_LogoShadeB = 0;
    Title_StartArmed = 1;
}

// original 0x462290: entry 1 (PSX 0x801D0D5C). Waits for the backdrop's
// fade-in to finish (Title_DrawBackdrop sets fade 2), then holds 360 frames.
extern "C" void __cdecl Title_StateFadeIn(void) {
    if (Title_Fade != 2) return;
    BumpState();
    Title_Timer = 0x168;
}

// original 0x4622B0: entry 2 (PSX 0x801D0D94). Counts the hold down; at 0 the
// backdrop fades out, the logo in, and the logo holds 900 frames.
//
// As the original has it: the timer is decremented as a u16 before the test,
// so a timer of 0 wraps to 0xFFFF and holds 65,535 frames more
// (Title_CheckStart sets it to 1 to cut the hold short, never 0).
extern "C" void __cdecl Title_StateBackdrop(void) {
    Title_Timer = static_cast<std::uint16_t>(Title_Timer - 1);
    if (Title_Timer != 0) return;
    BumpState();
    Title_Fade = 3;
    Title_LogoState = 1;
    Title_Timer = 0x384;
}

// original 0x4622E0: entry 3 (PSX 0x801D0DF0). Counts the logo's hold down; at
// 0 starts the screen transition and fades the music.
//
// As the original has it: the state word is read after the two calls.
extern "C" void __cdecl Title_StateLogo(void) {
    Title_Timer = static_cast<std::uint16_t>(Title_Timer - 1);
    if (Title_Timer != 0) return;
    title_states::gTitleStates.transition(0);
    title_states::gTitleStates.fade_music();
    BumpState();
}

// original 0x462300: entry 4 (PSX 0x801D0E54). Once no transition holds the
// wait word, starts the demo: the title's fades off, the music stopped, the
// effect objects cleared, the play clock and the draw-pass flags zeroed,
// scenario chapter 0x10 started and task 0 created on the field task.
//
// As the original has it: Port_DroppedCall(0)'s pushed argument stays on the
// stack under Scenario_Start's until one `add esp, 0x10` - no callee reads
// past its own. Scenario_Start runs on task 1's stack and may sleep there
// while its DAT loads (on the PC it never does).
extern "C" void __cdecl Title_StateStartDemo(void) {
    if (Transition_Busy != 0) return;
    Title_Fade = 0;
    Title_LogoState = 0;
    Title_StartArmed = 0;
    title_states::gTitleStates.stop_music();
    title_states::gTitleStates.dropped(0);
    title_states::gTitleStates.effect_clear();
    move_script::At(::bof3::Img(kPlayClock.v))[3] = 0;
    move_script::At(::bof3::Img(kPlayClock.v))[2] = 0;
    move_script::At(::bof3::Img(kPlayClock.v))[1] = 0;
    move_script::At(::bof3::Img(kPlayClock.v))[0] = 0;
    Draw_PassFlags = 0;
    title_states::gTitleStates.scenario_start(0x10);
    title_states::gTitleStates.task_create(0, reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(kDemoEntry.v))));
    BumpState();
}

// original 0x462370: entry 5 (PSX 0x801D0F00), every frame of the demo. When
// task 0 has ended, back to the title (state 0). Start pressed during it (with
// the load done and no wait) ends the demo: MoveScript_Var7 1, transition 4,
// the music faded out. Then the corner badge (a tail jump in the original).
//
// As the original has it: the state set to 0 returns without the badge; the
// pad is read as the dword at Input_Pressed and only bit 0x800 tested.
extern "C" void __cdecl Title_StateDemo(void) {
    if (TaskZeroState() == 0) {
        Title_State = 0;
        return;
    }
    if (title_states::gTitleStates.load_done() != 0 && Transition_Busy == 0 && (Input_Pressed & 0x800) != 0) {
        MoveScript_Var7 = 1;
        title_states::gTitleStates.transition(4);
        title_states::gTitleStates.music_fade_stop(8);
        BumpState();
    }
    title_states::gTitleStates.badge();
}

// original 0x4623D0: entry 6 (PSX 0x801D0FB8), after Start ended the demo:
// when task 0 is gone, the party combination byte 0xFF and back to state 0;
// until then the badge (a tail jump).
extern "C" void __cdecl Title_StateDemoEnd(void) {
    if (TaskZeroState() == 0) {
        AreaW0b_PartySet = 0xFF;
        Title_State = 0;
        return;
    }
    title_states::gTitleStates.badge();
}

// original 0x4623F0: entry 7 (PSX 0x801D1000), after Start on the logo: once
// no transition holds the wait word, stops the music, creates task 0 on the
// title menu 0x588E70 and ends task 1.
//
// As the original has it: Task_Exit is a tail jump that never returns - it
// clears this task's record and enters the scheduler on the scheduler's own
// stack, abandoning task 1's (this frame included). A stand-in that returns
// makes this return, as the original's jump would to its caller.
extern "C" void __cdecl Title_StateMenu(void) {
    if (Transition_Busy != 0) return;
    title_states::gTitleStates.stop_music();
    title_states::gTitleStates.dropped(0);
    title_states::gTitleStates.task_create(0, reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(kMenuEntry.v))));
    title_states::gTitleStates.task_exit();
}

// original 0x462420: Start on the title (PSX FUN_801D104C). Pressed while the
// backdrop still fades in or holds (states 0..2), it cuts the hold short:
// the timer to 1, state 2 - the next frame's state-2 handler ends it. Pressed
// once the logo is up (state 3 or later, the backdrop's fade done, the logo's
// state 2), it leaves for the title menu: state 7.
//
// As the original has it: the checks in this order, each a return -
// File_LoadDone (always 1 on the PC) is asked only once Start is down; task 0
// running (the demo) makes Start this function's no-op, the demo's own state
// handler reads it instead. The state test is "at most 2" (the PSX's "below
// 3"). The original pushes Transition_Start's 0 and leaves it on the stack
// under Sound_PlayEffect's 0x105 until one `add esp, 8` - a stray second
// argument Sound_PlayEffect never reads (it reads [esp + 4] only).
extern "C" void __cdecl Title_CheckStart(void) {
    if (Title_StartArmed == 0) return;
    if (!(Input_Pressed & 0x800)) return;
    if (mode_tasks::g.load_done() == 0) return;
    if (Transition_Busy != 0) return;
    if (TaskZeroState() != 0) return;
    if (Title_State <= 2) {
        Title_Timer = 1;
        Title_State = 2;
        return;
    }
    if (Title_Fade != 0) return;
    if (Title_LogoState != 2) return;
    mode_tasks::g.transition(0);
    mode_tasks::g.fade_music();
    mode_tasks::g.effect(0x105);
    Title_StartArmed = 0;
    Title_State = 7;
}

// original 0x4624B0: fade the music out over 0x10 frames, unless none plays
// (PSX 0x801D1134, which fades through a per-track table).
extern "C" void __cdecl Title_FadeMusic(void) {
    if (Music_Track != 0xFF) title_states::gTitleStates.music_fade_out(0x10);
}

// original 0x4624D0: fade out and stop over 0xA frames, and forget the track,
// unless none plays (PSX 0x801D1184).
//
// As the original has it: 0xFF is stored after the call, whatever the call
// left there.
extern "C" void __cdecl Title_StopMusic(void) {
    if (Music_Track == 0xFF) return;
    title_states::gTitleStates.music_fade_stop(0xA);
    Music_Track = 0xFF;
}

// original 0x4624F0: the demo's corner badge (PSX 0x801D11E4) - sprite 0xA at
// (0xC0, 4) while Title_BadgeOn and bit-anything Draw_PassFlags are set. The
// badge goes off for good once the field runs its area change (Field_Request
// 5 in Game_Mode 2) with no transition waiting.
//
// As the original has it: the three tests in that order, Field_Request and
// Title_BadgeOn as bytes, Game_Mode and the wait word as words; the packet
// cursor read for the draw mode at the call. The PSX asks a video-mode
// function for the tpage; the port always takes 0x9D.
extern "C" void __cdecl Title_DrawBadge(void) {
    if (Field_Request == 5 && Game_Mode == 2 && Transition_Busy == 0) Title_BadgeOn = 0;
    if (Draw_PassFlags == 0 || Title_BadgeOn == 0) return;
    title_states::gTitleStates.draw_mode(Gfx_PacketNext, 0, 0, 0x9D, 0);
    title_states::gTitleStates.commit(2, 0xC);
    title_states::gTitleStates.sprite(0xC0, 4, 0xA, 2, 0);
}

// original 0x462560: one title sprite (PSX FUN_801D1760): a SPRT at the
// packet cursor, entry `index` of Title_Sprites for its texture, committed to
// OT slot `slot`. Returns the primitive, for the caller to shade.
//
// As the original has it: x and y are taken as s16 and converted exactly to
// floats (the PC's primitive holds floats where the PSX's held s16); index
// and semi are taken as bytes, slot whole (Gfx_CommitPrim masks it itself);
// the clut word is entry +8 shifted left 6, into 16 bits. The primitive is
// the cursor as it was on entry, whatever the commit did to it. The original
// also writes its converted x and y back over its own argument slots - dead
// stores no caller reads (all five callers only pop them).
extern "C" unsigned char* __cdecl Title_Sprite(int x, int y, unsigned index, unsigned slot, unsigned semi) {
    unsigned char* const prim = Gfx_PacketNext;
    mode_tasks::g.set_sprt(prim);
    mode_tasks::g.semi_trans(prim, semi & 0xFF);
    prim[4] = 0x80;
    prim[5] = 0x80;
    prim[6] = 0x80;
    const float fx = static_cast<float>(static_cast<short>(x));
    const float fy = static_cast<float>(static_cast<short>(y));
    std::memcpy(prim + 8, &fx, sizeof fx);
    std::memcpy(prim + 0xC, &fy, sizeof fy);
    const unsigned char* const entry = Title_Sprites + (index & 0xFF) * 10u;
    prim[0x14] = entry[0];
    prim[0x15] = entry[2];
    rv::As<records::GpuSprt>(prim)->w = move_script::Word(entry + 4);
    rv::As<records::GpuSprt>(prim)->h = move_script::Word(entry + 6);
    rv::As<records::GpuSprt>(prim)->clut = static_cast<unsigned>(move_script::Word(entry + 8)) << 6;
    mode_tasks::g.commit(slot, 0x1C);
    return prim;
}

// original 0x462600: the title's backdrop (PSX FUN_801D1880): four columns
// 255 wide, from texture pages 5, 7, 9 and 11, sliding left two pixels a
// frame while Title_Fade is set, coloured by Title_Bright.
//
// As the original has it:
//   - a column is skipped while scroll / 640 (s16, truncated) exceeds its
//     index - the scroll re-read for every column;
//   - the fade steps once per column DRAWN, not once per frame: up to four
//     steps a frame, fewer as columns slide out;
//   - fade 1 raises the brightness, and on reaching 0x80 (s16 compare) sets
//     0x80 and fade 2 - the column is still drawn, and the brightness kept in
//     a register is not clamped, only the stored word;
//   - fade 3 lowers it; at 0 or below (s16) both words go to 0 and the column
//     is not drawn - and every later column that frame then takes the "any
//     other fade" branch, which stores 0x80 into the brightness it just
//     zeroed before seeing fade 0 and skipping;
//   - the fade and the brightness are read back from memory after each
//     column is drawn; the colour bytes are the stored word's low byte.
extern "C" void __cdecl Title_DrawBackdrop(void) {
    unsigned char fade = Title_Fade;
    if (fade == 0) return;
    const auto scroll = static_cast<std::uint16_t>(Title_Scroll + 2);
    const auto left = static_cast<std::uint16_t>(0x140 - scroll);
    Title_Scroll = scroll;
    auto bright = static_cast<std::uint16_t>(Title_Bright);
    unsigned column = 0;
    for (unsigned page = 0x140; page < 0x340; page += 0x80, ++column) {
        if (static_cast<short>(Title_Scroll) / 640 > static_cast<int>(column)) continue;
        if (fade == 1) {
            ++bright;
            Title_Bright = bright;
            if (static_cast<short>(bright) >= 0x80) {
                Title_Bright = 0x80;
                Title_Fade = 2;
            }
        } else if (fade == 3) {
            --bright;
            Title_Bright = bright;
            if (static_cast<short>(bright) <= 0) {
                bright = 0;
                fade = 0;
                Title_Bright = 0;
                Title_Fade = 0;
                continue;
            }
        } else {
            bright = 0x80;
            Title_Bright = 0x80;
            if (fade == 0) continue;
        }
        mode_tasks::g.draw_mode(Gfx_PacketNext, 0, 0, ((page & 0x3C0) | 0x2000) >> 6, 0);
        mode_tasks::g.commit(2, 0xC);
        unsigned char* const prim = mode_tasks::g.sprite(static_cast<int>(column * 255 + left), 0x18, column + 0xB, 2, 0);
        prim[4] = static_cast<unsigned char>(Title_Bright);
        prim[5] = static_cast<unsigned char>(Title_Bright);
        prim[6] = static_cast<unsigned char>(Title_Bright);
        bright = Title_Bright;
        fade = Title_Fade;
    }
}

// original 0x462740: the logo (PSX FUN_801D1A88). Title_LogoState 1 fades
// two shades in - A by 4 a frame, B by 2 - each clamped at 0x80 with its
// flag cleared, and state 2 once both are; the flags are the three sets'
// semi-transparency while fading.
//
// As the original has it: a state other than 0, 1 and 2 draws with both
// flags set and moves nothing; the clamps are s16 compares; each shade is
// passed as the byte read from memory after the previous set was drawn.
extern "C" void __cdecl Title_DrawLogo(void) {
    unsigned char flag_a = 1, flag_b = 1;
    const unsigned char state = Title_LogoState;
    if (state == 1) {
        Title_LogoShadeA = static_cast<std::uint16_t>(Title_LogoShadeA + 4);
        if (static_cast<short>(Title_LogoShadeA) >= 0x80) {
            flag_a = 0;
            Title_LogoShadeA = 0x80;
        }
        Title_LogoShadeB = static_cast<std::uint16_t>(Title_LogoShadeB + 2);
        if (static_cast<short>(Title_LogoShadeB) >= 0x80) {
            flag_b = 0;
            Title_LogoShadeB = 0x80;
        }
        if (flag_a == 0 && flag_b == 0) Title_LogoState = 2;
    } else if (state == 2) {
        flag_a = 0;
        flag_b = 0;
        Title_LogoShadeA = 0x80;
        Title_LogoShadeB = 0x80;
    } else if (state == 0) {
        return;
    }
    mode_tasks::g.set_a(flag_b, static_cast<unsigned char>(Title_LogoShadeB));
    mode_tasks::g.set_b(0x1A, 0x18, flag_b, static_cast<unsigned char>(Title_LogoShadeB));
    mode_tasks::g.set_c(-6, 0x1C, flag_a, static_cast<unsigned char>(Title_LogoShadeA));
}

// original 0x462820: the logo's first set (PSX 0x801D12CC): sprite 1 through
// OT slot 1, sprites 8 and 9 through slot 2, each semi-transparent by `flag`
// and shaded `shade`; then, once the backdrop is gone and the logo shown,
// sprite 7 blinking - the "press Start" prompt, by where it appears
// (hypothesis) - on Title_BlinkCount, a 64-frame triangle from 0 to 0x7C and
// back from 0x80 to 4.
//
// As the original has it: flag and shade are dwords whose low byte alone is
// read (Title_Sprite masks the one, Prim_SetShade stores the other as a
// byte; ours are masked here because a Capcom caller leaves the upper bytes
// stale); the packet cursor is read again for the second draw mode, after
// the first sprite's commit moved it; Title_Fade and Title_LogoState are read
// after the three sprites; the counter is incremented as a word in memory and
// its low byte read back; the blinking sprite is opaque and not shaded
// through Prim_SetShade - its three colour bytes are written here.
extern "C" void __cdecl Title_DrawSetA(unsigned flag, unsigned shade) {
    const unsigned semi = flag & 0xFF;
    const auto level = static_cast<unsigned char>(shade);
    if (!title_states::g_psp_title) {   // DIV-0131: the PSP's art carries its own TM; the mark sprite is not drawn
        title_states::gTitleStates.draw_mode(Gfx_PacketNext, 0, 0, 0x2F, 0);
        title_states::gTitleStates.commit(1, 0xC);
        title_states::gTitleStates.shade(title_states::gTitleStates.sprite(0x106, 0x82, 1, 1, semi), level);
    }
    title_states::gTitleStates.draw_mode(Gfx_PacketNext, 0, 0, 0xBD, 0);
    title_states::gTitleStates.commit(2, 0xC);
    title_states::gTitleStates.shade(title_states::gTitleStates.sprite(0x14, 0xD0, 8, 2, semi), level);
    title_states::gTitleStates.shade(title_states::gTitleStates.sprite(0xA4, 0xD0, 9, 2, semi), level);
    if (Title_Fade != 0 || Title_LogoState != 2) {
        Title_BlinkCount = 0;
        return;
    }
    unsigned char* const prim = title_states::gTitleStates.sprite(0x30, 0xB8, 7, 2, 0);
    Title_BlinkCount = static_cast<std::uint16_t>(Title_BlinkCount + 1);
    const unsigned count = Title_BlinkCount & 0xFFu;
    const unsigned step = (count & 0x1Fu) << 2;
    const auto blink = static_cast<unsigned char>((count & 0x20u) ? 0x80u - step : step);
    prim[4] = blink;
    prim[5] = blink;
    prim[6] = blink;
}

// original 0x462930: the logo's third set (PSX 0x801D1494): while `flag`
// (the logo still fading), sprites 0xF and 0x10 semi-transparent at x and
// x + 0xE0 under tpage 0xD9; always sprites 4 and 5 there under tpage 0xB9,
// semi-transparent by `flag`; all four shaded `shade`.
//
// As the original has it: the flag's low byte decides the first pair (`test
// bl, bl`); x + 0xE0 is 32-bit arithmetic, taken as s16 by Title_Sprite; the
// packet cursor read at each draw mode.
extern "C" void __cdecl Title_DrawSetC(int x, int y, unsigned flag, unsigned shade) {
    const unsigned semi = flag & 0xFF;
    const auto level = static_cast<unsigned char>(shade);
    if (title_states::g_psp_title) {   // DIV-0131: the PSP's pieces, same sprites and pages
        const int ux = x + kPspTextDx, lx = ux + kPspLowerDx, ly = y + kPspLowerDy;
        if (semi != 0) {
            title_states::gTitleStates.draw_mode(Gfx_PacketNext, 0, 0, 0xD9, 0);
            title_states::gTitleStates.commit(2, 0xC);
            title_states::gTitleStates.shade(PspSprite(ux, y, 0xF, kPspTextUpper, 2, 1), level);
            title_states::gTitleStates.shade(PspSprite(lx, ly, 0x10, kPspTextLower, 2, 1), level);
        }
        title_states::gTitleStates.draw_mode(Gfx_PacketNext, 0, 0, 0xB9, 0);
        title_states::gTitleStates.commit(2, 0xC);
        title_states::gTitleStates.shade(PspSprite(ux, y, 4, kPspTextUpper, 2, semi), level);
        title_states::gTitleStates.shade(PspSprite(lx, ly, 5, kPspTextLower, 2, semi), level);
        return;
    }
    const int right = static_cast<int>(static_cast<unsigned>(x) + 0xE0u);
    if (semi != 0) {
        title_states::gTitleStates.draw_mode(Gfx_PacketNext, 0, 0, 0xD9, 0);
        title_states::gTitleStates.commit(2, 0xC);
        title_states::gTitleStates.shade(title_states::gTitleStates.sprite(x, y, 0xF, 2, 1), level);
        title_states::gTitleStates.shade(title_states::gTitleStates.sprite(right, y, 0x10, 2, 1), level);
    }
    title_states::gTitleStates.draw_mode(Gfx_PacketNext, 0, 0, 0xB9, 0);
    title_states::gTitleStates.commit(2, 0xC);
    title_states::gTitleStates.shade(title_states::gTitleStates.sprite(x, y, 4, 2, semi), level);
    title_states::gTitleStates.shade(title_states::gTitleStates.sprite(right, y, 5, 2, semi), level);
}

// original 0x462A00: the logo's second set (PSX 0x801D1664): sprites 2 at
// (x, y) and 3 at (x + 0xF0, y + 0x70) under tpage 0xBB, semi-transparent by
// `flag`, shaded `shade`.
extern "C" void __cdecl Title_DrawSetB(int x, int y, unsigned flag, unsigned shade) {
    const unsigned semi = flag & 0xFF;
    const auto level = static_cast<unsigned char>(shade);
    title_states::gTitleStates.draw_mode(Gfx_PacketNext, 0, 0, 0xBB, 0);
    title_states::gTitleStates.commit(2, 0xC);
    if (title_states::g_psp_title) {   // DIV-0131: the emblem alone; the PSP's page has nothing under sprite 3's rectangle
        title_states::gTitleStates.shade(PspSprite(x + kPspEmblemDx, y, 2, kPspEmblem, 2, semi), level);
        return;
    }
    title_states::gTitleStates.shade(title_states::gTitleStates.sprite(x, y, 2, 2, semi), level);
    title_states::gTitleStates.shade(title_states::gTitleStates.sprite(static_cast<int>(static_cast<unsigned>(x) + 0xF0u), static_cast<int>(static_cast<unsigned>(y) + 0x70u),
                     3, 2, semi),
            level);
}

// original 0x462A70. One level to bytes +4, +5 and +6 - where a PSX GPU
// primitive keeps r0, g0, b0 - leaving +7, the primitive's code, alone.
extern "C" void __cdecl Prim_SetShade(unsigned char* prim, unsigned char level) {
    prim[4] = level;
    prim[5] = level;
    prim[6] = level;
}

void Title_InjectFromDrawEmit() {
    BOF3_INJECT(Gfx_CommitPrim);
}

void Title_InjectFromFieldC1() {
    BOF3_INJECT(Config_DrawRowLabel);
}

void Title_InjectFromGfxFlush() {
    BOF3_INJECT(Gfx_FlushUploadQueue);
}

void Title_InjectFromGfxUnpack() {
    BOF3_INJECT(Gfx_UploadPacked5);
    BOF3_INJECT(Gfx_UploadLzss);
}

void Title_InjectFromItemUse() {
    BOF3_INJECT(Input_AutoRepeat);
}

void Title_InjectFromModeFlow() {
    BOF3_INJECT(Gfx_ClearRect);
}

void Title_InjectFromModeTasks() {
    BOF3_INJECT(Title_Task);
    BOF3_INJECT(Title_CheckStart);
    BOF3_INJECT(Title_Sprite);
    BOF3_INJECT(Title_DrawBackdrop);
    BOF3_INJECT(Title_DrawLogo);
}

void Title_InjectFromPrim() {
    BOF3_INJECT(Prim_SetShade);
}

void Title_InjectFromRest4e() {
    BOF3_INJECT(CommuItem_OpenWindow);
    BOF3_INJECT(CommuItem_Choose);
    BOF3_INJECT(CommuItem_Cancel);
    BOF3_INJECT(CommuItem_Confirm);
    BOF3_INJECT(CommuItem_Give);
    BOF3_INJECT(CommuItem_Refused);
    BOF3_INJECT(CommuItem_SetupWindow);
    BOF3_INJECT(CommuRank_Dispatch);
    BOF3_INJECT(CommuRank_Pages);
    BOF3_INJECT(CommuRank_Show);
    BOF3_INJECT(CommuRank_Close);
    BOF3_INJECT(CommuRank_DrawListA);
    BOF3_INJECT(CommuRank_DrawIcon);
    BOF3_INJECT(CommuRank_DrawListB);
    BOF3_INJECT(CommuRank_DrawListC);
    BOF3_INJECT(CommuRank_DrawBackdrop);
    BOF3_INJECT(CommuRank_DrawTile);
}

void Title_InjectFromRest4f() {
    BOF3_INJECT(ConfigScreen_Run);
    BOF3_INJECT(ConfigScreen_OpenRun);
    BOF3_INJECT(ConfigScreen_OpenFade);
    BOF3_INJECT(ConfigScreen_OpenWait);
    BOF3_INJECT(ConfigScreen_SlideIn);
    BOF3_INJECT(ConfigScreen_TopBar);
    BOF3_INJECT(ConfigScreen_Rows);
    BOF3_INJECT(ConfigScreen_Controller);
    BOF3_INJECT(ConfigScreen_SlideOut);
    BOF3_INJECT(ConfigScreen_DrawButtons);
    BOF3_INJECT(Config_DrawPanel);
    BOF3_INJECT(Config_DrawRowOptions);
    BOF3_INJECT(Config_DrawControllerPanel);
    BOF3_INJECT(Config_DrawControllerRow);
}

void Title_InjectFromTitleStates() {
    BOF3_INJECT(Title_StateLoad);
    BOF3_INJECT(Title_StateFadeIn);
    BOF3_INJECT(Title_StateBackdrop);
    BOF3_INJECT(Title_StateLogo);
    BOF3_INJECT(Title_StateStartDemo);
    BOF3_INJECT(Title_StateDemo);
    BOF3_INJECT(Title_StateDemoEnd);
    BOF3_INJECT(Title_StateMenu);
    BOF3_INJECT(Title_FadeMusic);
    BOF3_INJECT(Title_StopMusic);
    BOF3_INJECT(Title_DrawBadge);
    BOF3_INJECT(Title_DrawSetA);
    BOF3_INJECT(Title_DrawSetC);
    BOF3_INJECT(Title_DrawSetB);
}
