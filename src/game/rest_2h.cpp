// Group R2H of round fourteen (wave two): 36 functions at 0x59AA80..0x5A9874 -
// the cut's 34 rows (analysis/round14_cut.tsv), the start in their span no list
// had (0x59C810) and 0x59E160, a menu draw the catalogue filed under the
// renderer by its address range - each read to its last instruction with
// capstone (2026-10-04) and fuzzed through the scenario harness's field mode
// (rest_2h_fuzz.cpp). docs/rest_2h.md has them one row each.
//
// Most are window kinds: a record of WindowRecords (0x803160, 22 of 0x24) is
// the dword at 0x905B84 while Field_RunTaskRecords runs it; +2 is its kind
// (record handler 7 jumps through Window_Handler7KindTable, handler 8 through
// Window_Handler8KindTable, handler 6 through MenuList_Kinds, all ours), +3 its
// step, +4 / +6 x / y (s16), +0xA.. the kind's bytes. A "run" calls its step
// through a .data step table by +3, then draws the record (re-read); a "slide"
// moves x or y and, past a bound (signed 16-bit compares), stores the bound and
// sets the step to 0 (DI's, docs/menu_draw_helpers.md section 1).
//
// Every one is a faithful replacement. The record pointer is re-read where the
// original re-reads it (after every call; a volatile load). The originals push
// coordinates built by 16-bit moves and bytes built by 8-bit moves in registers
// whose upper bits are left over; every callee reads only the low word or byte
// (docs/rest_2h.md section 3), so ours passes them zero-extended. Where the
// original indexes a step table by +3, which it never bounds, ours aborts past
// the table (round9 doc section 6); where it reads .data in place by an
// unbounded byte that stays inside the image, ours reads the same bytes.
//
// Two call sites inside these bodies are re-aimed by divergences, and ours
// calls whatever the site reaches, read from its rel32, so each divergence and
// its BOF3X_ORIGINAL switch work for ours as for Capcom's body: DIV-0011's
// Menu_DrawFrame at MenuList_ReserveWinDraw's first call (0x59AA98,
// menu_frame.cpp), DIV-0059's ListTitle_DrawAt at BattleEquipWin_DrawItems'
// title (0x59DEFA, battle_draw.cpp, a Latin overlay only). DIV-0041 moves
// MasterWin_SlideOut's bound (0x59C136); ours reads the immediate there.
#include "game/rest_2h.h"

#include <windows.h>
#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2h_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_2h {
std::uint32_t g_master_bound = at::kMasterBound;
}  // namespace rest_2h

namespace {

namespace at = rest_2h::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();

unsigned char& B(U address) { return *At(address); }
U L(U address) { return static_cast<U>(Long(At(address))); }
int S8(unsigned v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// The current window record, re-read at every use (the originals reload
// 0x905B84 after each call).
unsigned char* Rec() { return At(*reinterpret_cast<volatile U*>(static_cast<std::uintptr_t>(at::kCurrent))); }
int X(const unsigned char* w) { return Word(w + 4); }
int Y(const unsigned char* w) { return Word(w + 6); }
unsigned char Style() { return B(at::kStyle); }

// The step: the current record's +3 through a step table, called through the
// word the table holds (the fuzz swaps the words for recorders); past the
// table's own run of handlers the original jumps through whatever follows,
// ours aborts.
void Step(const char* who, U table, unsigned entries) {
    const unsigned index = Rec()[3];
    if (index >= entries)
        bof3::Fatal("%s: the window's step +3 is %u, past the %u entries of 0x%X - the original jumps through the "
                    "dword after (docs/rest_2h.md section 7)",
                    who, index, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * index)))();
}

// A slide: x (+4) or y (+6) moved by `by`; past `bound` (signed 16-bit; `below`
// says which side) it is set to `to` and the step +3 goes to 0. The originals
// re-read 0x905B84 between the stores with nothing called in between.
void Slide(unsigned at_word, int by, short bound, bool below, short to) {
    unsigned char* const w = Rec();
    SetWord(w + at_word, static_cast<unsigned>(Word(w + at_word) + by));
    const auto v = static_cast<short>(Word(w + at_word));
    if (below ? v < bound : v > bound) {
        SetWord(w + at_word, static_cast<std::uint16_t>(to));
        w[3] = 0;
    }
}

// CharacterRecords' record n (0xA4 each), unchecked; a member id's record
// index, 0x66972C[id] read in place (the byte is unbounded, the table is 24).
unsigned char* Record(U n) { return At(at::kRecords + n * at::kRecordStride); }
U RecordOf(U id) { return B(at::kMemberMap + (id & 0xFF)); }

char* PrintBuf() { return reinterpret_cast<char*>(At(at::kPrintBuf)); }
const char* Format(U address) { return reinterpret_cast<const char*>(At(address)); }
const unsigned char* Text(U address) { return At(address); }
const unsigned char* Message(unsigned id) { return SH_CALL(Msg_SystemPtr)(id & 0xFFFF); }

// Where an E8 site reaches now: the site + 5 + its rel32.
U SiteTarget(U site) { return site + 5 + L(site + 1); }

void PutFloat(unsigned char* at, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

void Piece(int x, int y, unsigned id) { SH_CALL(Menu_DrawPiece)(x, y, id, 1); }
void Pieces(int x, int y, U list) { SH_CALL(Menu_DrawPieces)(x, y, Text(list), 1); }

}  // namespace

// ===========================================================================
// MenuList_Kinds' reserve list and kind 20, the gene list
// ===========================================================================

// original 0x59AA80: the reserve list's panels for the record `obj` (called by
// 0x59AA30, MenuList_Kinds[19]): the call DIV-0011 re-aims (x, y, 0x12, 0x15)
// with x, y the words +4 / +6; then for each id of the list obj +0x20 points
// at, below the count obj +0xA (re-read each row), at X = x + 7, Y = y + 6 +
// 0x34 row: the member's record (0x66972C[id]); a record with +0xB bit 1 is
// drawn dim (the box's flags 0x81, the portrait's shade 1, every text colour
// 7), else flags 0x80, shade +0x10 bit 7 as 2, the name colour 0. The box (X,
// Y, 0x7D, 0x30, flags, style); the portrait (X + 0x56, Y, +9, shade); the name
// (X + 0x14, Y + 1, colour, 5); the tile Menu_DrawTile16(X + 2, Y + 0x15, 3,
// dim); the level +0xA at (X + 0x18, Y + 0x15); the status in the small font at
// (X + 0x30, Y + 0x15, the colour or 1, 0x10) - 0x66A0E8 for +0x10 bit 7 (with
// bit 5 as well: on Frame_Counter bit 5, else 0x66A0F0), 0x66A0F0 for bit 5
// alone; the tile (X + 2, Y + 0x1D, 0, dim); HP +0x18 / its maximum +0x20 at
// (X + 0x18, Y + 0x1D), the HP's colour 4 with +0x11 bit 5 and 2 at 1 or less,
// the maximum's 4 with +0x1E set (7 both when dim); the tile (X + 2, Y + 0x25,
// 1, dim); AP +0x1A (4 at a quarter of +0x22 or less, 2 at 0) and +0x22 (0, or
// 7 dim) at (X + 0x18, Y + 0x25).
//
// PartyForm_DrawReserve's twin (field_s.cpp, 0x581300) with the dimming added
// and the list behind a pointer. As the original has it: the colours and the
// dim flag go out in dwords whose upper bytes are the stack's (each callee
// reads the byte); obj +4, +6, +0xA and +0x20 are read once after the first
// call, the count again at each row's end.
extern "C" void __cdecl MenuList_ReserveWinDraw(unsigned char* obj) {
    SH_AT(void (__cdecl*)(int, int, int, int), SiteTarget(at::kFrameSite))(X(obj), Y(obj), 0x12, 0x15);
    const int x7 = X(obj) + 7;
    int y = Y(obj) + 6;
    if (obj[0xA] == 0) return;
    const unsigned char* list = At(static_cast<U>(Long(obj + 0x20)));
    int text_y = y + 0x15;
    unsigned char row = 0;
    do {
        unsigned char* const rec = Record(RecordOf(*list));
        const unsigned dim = (rec[0xB] & 2) ? 1u : 0u;
        const int colour = dim ? 7 : 0;
        SH_CALL(Menu_DrawBox)(x7, y, 0x7D, 0x30, static_cast<int>(0x80 + dim), Style());
        const unsigned shade = dim ? 1u : (static_cast<unsigned>(rec[0x10]) >> 6) & 2;
        SH_CALL(Menu_DrawItemIcon)(x7 + 0x56, y, rec[9], static_cast<int>(shade));
        SH_CALL(Text_DrawAt)(x7 + 0x14, text_y - 0x14, colour, 5, rec);
        SH_CALL(Menu_DrawTile16)(x7 + 2, text_y, 3, dim);
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtLevel), static_cast<unsigned>(rec[0xA]));
        const int pen = x7 + 0x18;
        SH_CALL(Text_DrawFont8)(pen, text_y, colour, Text(at::kPrintBuf));
        const unsigned flags = Word(rec + 0x10);
        U status = 0;
        if (flags & 0x80) {
            if (flags & 0x20) status = (Frame_Counter & 0x20) ? at::kStatusA : at::kStatusB;
            else status = at::kStatusA;
        } else if (flags & 0x20) {
            status = at::kStatusB;
        }
        if (status != 0)
            SH_CALL(Text_DrawSmall)(x7 + 0x30, text_y, colour != 0 ? static_cast<unsigned>(colour) : 1u, 0x10, Text(status));
        const int hp_y = text_y + 8;
        SH_CALL(Menu_DrawTile16)(x7 + 2, hp_y, 0, dim);
        int c = colour;
        if (!dim) {
            if (rec[0x11] & 0x20) c = 4;
            if (Word(rec + 0x18) <= 1) c = 2;
        }
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtFigureA), static_cast<unsigned>(Word(rec + 0x18)));
        SH_CALL(Text_DrawFont8)(pen, hp_y, c, Text(at::kPrintBuf));
        c = dim ? 7 : (rec[0x1E] != 0 ? 4 : 0);
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtFigureB), static_cast<unsigned>(Word(rec + 0x20)));
        SH_CALL(Text_DrawFont8)(pen, hp_y, c, Text(at::kPrintBuf));
        const int ap_y = text_y + 0x10;
        SH_CALL(Menu_DrawTile16)(x7 + 2, ap_y, 1, dim);
        c = 0;
        if (dim) {
            c = 7;
        } else {
            const unsigned quarter = (Word(rec + 0x22) >> 2) & 0xFFFF;
            const unsigned ap = Word(rec + 0x1A);
            if (ap <= quarter) c = 4;
            if (ap == 0) c = 2;
        }
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtFigureA), static_cast<unsigned>(Word(rec + 0x1A)));
        SH_CALL(Text_DrawFont8)(pen, ap_y, c, Text(at::kPrintBuf));
        SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtFigureB), static_cast<unsigned>(Word(rec + 0x22)));
        SH_CALL(Text_DrawFont8)(pen, ap_y, dim ? 7 : 0, Text(at::kPrintBuf));
        y += 0x34;
        text_y += 0x34;
        ++list;
        ++row;
    } while (row < obj[0xA]);
}

// original 0x59ADE0, MenuList_Kinds[20]: the step (MenuList_GeneWinSteps by
// +3: a bare ret, MenuSlide_RightOff, 0x59AA50 - R2G's), then
// MenuList_GeneWinDraw(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl MenuList_GeneWinRun(void) {
    Step("MenuList_GeneWinRun", at::kGeneSteps, 3);
    SH_CALL(MenuList_GeneWinDraw)(Rec());
}

// original 0x59AE00: the gene list of the window `w`. The list 0x6BE0B0: for
// each of the 18 bits of 0x904650 that is set, its number + 1, then 0s to 18
// bytes. The box (x + 3, y + 3, 0x99, 0x9A, 0, style); the scroll step
// Menu_ListScroll(+0xA, &offset, &moving, +8); from the answered top, while the
// index is below top + moving + 9 and the entry is not 0: system message
// 0x4171 + the entry at (x + 0x11, y + offset + 0x1A + 13 row) with the
// icon Menu_DrawIcon8(x + 7, that + 2, 6, dim) - the row +0xB drawn twice, lit
// (colour 7, the icon's dim 1) and then two up in colour 0 with dim 0, the
// others once. Then +0xC = the entry at +0xB; the title and count boxes
// (x + 3, y + 3, 0x99, 0x14) and (x + 3, y + 0x92, 0x99, 8); the title 0x66B1F0
// centred (x + 6 (13 - its length), y + 7, 0, 0x10); the frame pieces; the
// scroll bar (0x6BE0B0, +0xA, x + 0x90, y + 0x18, 9, 0x12, 0x74).
//
// As the original has it: the first empty entry ends the rows (the masters'
// list skips one); the list is read past its 18 bytes when the top goes past
// 9, in place; +0xB is re-read after the lit row's calls; the title's y is
// read before Text_CharCount and its x after.
extern "C" void __cdecl MenuList_GeneWinDraw(unsigned char* w) {
    const U genes = L(at::kGenes);
    unsigned char n = 0;
    for (unsigned i = 0; i < 0x12; ++i)
        if (genes & (1u << i)) B(at::kGeneList + n++) = static_cast<unsigned char>(i + 1);
    if (n < 0x12) std::memset(At(at::kGeneList + n), 0, 0x12u - n);
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 3, 0x99, 0x9A, 0, style);
    unsigned char offset = 0, moving = 0;
    const unsigned char top = SH_CALL(Menu_ListScroll)(w + 0xA, &offset, &moving, w + 8);
    int text_y = S8(offset) + Y(w) + 0x1A;
    int icon_y = text_y + 2;
    const int end = static_cast<int>(moving) + static_cast<int>(top) + 9;
    for (unsigned char i = top; static_cast<int>(i) < end; ++i) {
        if (B(at::kGeneList + i) == 0) break;
        if (w[0xB] == i) {
            SH_CALL(Menu_DrawIcon8)(X(w) + 7, icon_y, 6, 1);
            const unsigned char* const lit = Message(B(at::kGeneList + i) + 0x4171u);
            SH_CALL(Text_DrawAt)(X(w) + 0x11, text_y, 7, 0xFF, lit);
            text_y -= 2;
            icon_y -= 2;
        }
        SH_CALL(Menu_DrawIcon8)(X(w) + 7, icon_y, 6, 0);
        const unsigned char* const name = Message(B(at::kGeneList + i) + 0x4171u);
        SH_CALL(Text_DrawAt)(X(w) + 0x11, text_y, 0, 0xFF, name);
        if (w[0xB] == i) {
            text_y += 2;
            icon_y += 2;
        }
        text_y += 0xD;
        icon_y += 0xD;
    }
    w[0xC] = B(at::kGeneList + w[0xB]);
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 3, 0x99, 0x14, 0, Style());
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 0x92, 0x99, 8, 0, Style());
    const int title_y = Y(w) + 7;
    const unsigned char length = SH_CALL(Text_CharCount)(Text(at::kGeneTitle));
    SH_CALL(Text_DrawAt)(6 * (0xD - static_cast<int>(length)) + X(w), title_y, 0, 0x10, Text(at::kGeneTitle));
    Pieces(X(w), Y(w), at::kGenePiecesA);
    Pieces(X(w), Y(w), at::kGenePiecesB);
    for (int i = 0; i < 10; ++i) Piece(8 * i + 0x28 + X(w), Y(w), 1);
    for (int i = 0; i < 15; ++i) Piece(X(w), 8 * i + 0x18 + Y(w), 4);
    Piece(X(w) + 0x90, Y(w) + 0x18, 0x16);
    for (int i = 0; i < 12; ++i) Piece(X(w) + 0x90, 8 * i + 0x28 + Y(w), 0x17);
    Piece(X(w) + 0x90, Y(w) + 0x88, 0x18);
    Piece(X(w), Y(w) + 0x90, 0x19);
    for (int i = 0; i < 17; ++i) Piece(8 * i + 8 + X(w), Y(w) + 0x90, 0x1A);
    Piece(X(w) + 8, Y(w), 0x32);
    Piece(X(w) + 0x90, Y(w), 0x33);
    SH_CALL(Menu_DrawScrollBar)(At(at::kGeneList), w[0xA], X(w) + 0x90, Y(w) + 0x18, 9, 0x12, 0x74);
}

// ===========================================================================
// Window_Handler7KindTable's kinds 11..18 (the shop's set after DI's 0..10)
// ===========================================================================

// original 0x59BE50, kind 11: the step (ShopWin_SharedListSteps: a bare ret,
// MenuSlide_RightOff, 0x59AA50 - R2G's, ShopWin_SharedListSlideTo28, 0x596920 -
// R2F's), then SharedList_DrawList(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_SharedListRun(void) {
    Step("ShopWin_SharedListRun", at::kSharedListSteps, 5);
    SH_CALL(SharedList_DrawList)(Rec());
}

// original 0x59BE70, kind 11 step 3: x += 0x20; above 0x28, x = 0x28 and step 0.
extern "C" void __cdecl ShopWin_SharedListSlideTo28(void) { Slide(4, 0x20, 0x28, false, 0x28); }

// original 0x59BEA0, kind 12: the step (ShopWin_SharedMemberSteps: a bare ret,
// MenuSlide_RightOff, 0x59AA50, 0x59A960, 0x59A9E0 - R2G's), then
// SharedList_DrawMember(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_SharedMemberRun(void) {
    Step("ShopWin_SharedMemberRun", at::kSharedMemberSteps, 5);
    SH_CALL(SharedList_DrawMember)(Rec());
}

// original 0x59BEC0, kind 13: the step (ShopWin_MemberStatusSteps: a bare ret,
// MenuWin_SlideOutLeft, MenuSlide_RightTo17), then Menu_DrawMemberStatus(+4,
// +6, +0xA, 0) of the record re-read - pushed with a fifth word 0, which the
// callee does not read.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_MemberStatusRun(void) {
    Step("ShopWin_MemberStatusRun", at::kMemberStatusSteps, 3);
    const unsigned char* const w = Rec();
    using Five = unsigned long (__cdecl*)(int, int, unsigned, unsigned, unsigned);
    reinterpret_cast<Five>(SH_CALL(Menu_DrawMemberStatus))(X(w), Y(w), w[0xA], 0, 0);
}

// original 0x59BF00, kind 14: ShopWin_DrawRowMenu(the record). No step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_RowMenuRun(void) {
    SH_CALL(ShopWin_DrawRowMenu)(Rec());
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
    const unsigned n0 = B(at::kRowCount);
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 3, 0x65, static_cast<int>((n0 << 4) + 0x19), 0, style);
    SH_CALL(Text_DrawAt)(X(w) + 0x1E, Y(w) + 7, 0, 0xFF, At(L(at::kRowText)));
    Pieces(X(w), Y(w), at::kRowPiecesA);
    unsigned char i = 0;
    if (B(at::kRowCount) != 0) {
        do {
            const unsigned char* const text = At(L(at::kRowText + 4u * B(at::kRowOrder + i)));
            const int dy = static_cast<int>(i) << 4;
            if (i == w[0xB]) {
                SH_CALL(Text_DrawAt)(X(w) + 7, Y(w) + dy + 0x1A, 7, 0xFF, text);
                SH_CALL(Text_DrawAt)(X(w) + 7, Y(w) + dy + 0x18, 2, 0xFF, text);
            } else if (i == w[0xA]) {
                SH_CALL(Text_DrawAt)(X(w) + 7, Y(w) + dy + 0x1A, 7, 0xFF, text);
                SH_CALL(Text_DrawAt)(X(w) + 7, Y(w) + dy + 0x18, 0, 0xFF, text);
            } else {
                SH_CALL(Text_DrawAt)(X(w) + 7, Y(w) + dy + 0x1A, 0, 0xFF, text);
            }
            SH_CALL(Menu_DrawPieces)(X(w), Y(w) + dy + 0x18, Text(at::kRowPiecesB), 1);
            SH_CALL(Menu_DrawPieces)(X(w), ((static_cast<int>(i) + 2) << 4) + Y(w), Text(at::kRowPiecesB), 1);
            ++i;
        } while (i < B(at::kRowCount));
    }
    SH_CALL(Menu_DrawPieces)(X(w), (static_cast<int>(i) << 4) + Y(w) + 0x18, Text(at::kRowPiecesC), 1);
}

// original 0x59C110, kind 15: the step (ShopWin_MasterListSteps: a bare ret,
// MasterWin_SlideOut, MasterWin_SlideTo64, 0x59A960 - R2G's), then
// MasterWin_DrawList(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_MasterListRun(void) {
    Step("ShopWin_MasterListRun", at::kMasterListSteps, 4);
    SH_CALL(MasterWin_DrawList)(Rec());
}

// original 0x59C130, kind 15 step 1: x -= 0x20; below the bound (-120), x = the
// bound and step 0. The bound is the imm32 at 0x59C136, which Widescreen_Inject
// (DIV-0041) moves out by its columns; ours reads its low word there at every
// call, as the original's `cmp word [eax + 4], cx` does, so BOF3X_ORIGINAL=
// Widescreen still restores -120 (Rest2H_Inject checks the opcode).
extern "C" void __cdecl MasterWin_SlideOut(void) {
    const auto bound = static_cast<short>(Word(At(rest_2h::g_master_bound)));
    Slide(4, -0x20, bound, true, bound);
}

// original 0x59C160, kind 15 step 2: x += 0x20; above 0x64, x = 0x64 and step 0.
extern "C" void __cdecl MasterWin_SlideTo64(void) { Slide(4, 0x20, 0x64, false, 0x64); }

// original 0x59C190, kind 16: the step (ShopWin_MasterCaptionSteps: a bare ret,
// MenuSlide_RightOff, MasterWin_CaptionSlideTo8C), then
// MasterWin_DrawCaption(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_MasterCaptionRun(void) {
    Step("ShopWin_MasterCaptionRun", at::kMasterCaptionSteps, 3);
    SH_CALL(MasterWin_DrawCaption)(Rec());
}

// original 0x59C1B0, kind 16 step 2: x -= 0x20; below 0x8C, x = 0x8C and step 0.
extern "C" void __cdecl MasterWin_CaptionSlideTo8C(void) { Slide(4, -0x20, 0x8C, true, 0x8C); }

// original 0x59C1E0, kind 17: the step (ShopWin_PupilsSteps: a bare ret,
// MasterWin_PupilsSlideDown, MasterWin_PupilsSlideUp), then
// MasterWin_DrawPupils(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_PupilsRun(void) {
    Step("ShopWin_PupilsRun", at::kPupilsSteps, 3);
    SH_CALL(MasterWin_DrawPupils)(Rec());
}

// original 0x59C200, kind 17 step 1: y += 0x10; above 0xF0, y = 0xF0 and step 0.
extern "C" void __cdecl MasterWin_PupilsSlideDown(void) { Slide(6, 0x10, 0xF0, false, 0xF0); }

// original 0x59C230, kind 17 step 2: y -= 0x10; below 0x80, y = 0x80 and step 0.
extern "C" void __cdecl MasterWin_PupilsSlideUp(void) { Slide(6, -0x10, 0x80, true, 0x80); }

// original 0x59C260, kind 18: the step (ShopWin_ItemCountSteps: a bare ret,
// MenuSlide_RightOff, ShopWin_ItemCountSlideToD2), then
// SharedList_DrawItemCount(+4, +6) of the record re-read.
extern "C" __attribute__((disable_tail_calls)) void __cdecl ShopWin_ItemCountRun(void) {
    Step("ShopWin_ItemCountRun", at::kItemCountSteps, 3);
    const unsigned char* const w = Rec();
    SH_CALL(SharedList_DrawItemCount)(X(w), Y(w));
}

// original 0x59C290, kind 18 step 2: x -= 0x20; below 0xD2, x = 0xD2 and step 0.
extern "C" void __cdecl ShopWin_ItemCountSlideToD2(void) { Slide(4, -0x20, 0xD2, true, 0xD2); }

// ===========================================================================
// The masters
// ===========================================================================

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
        if (!(B(at::kMasterBits + (i >> 3)) & static_cast<unsigned char>(1u << (i & 7)))) continue;
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
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 3, 0x71, 0x9A, 0, style);
    unsigned char offset = 0, moving = 0;
    const unsigned char top = SH_CALL(Menu_ListScroll)(w + 0xA, &offset, &moving, w + 8);
    int text_y = S8(offset) + Y(w) + 0x1A;
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
            SH_CALL(Text_DrawAt)(X(w) + 0x13, text_y, 7, 0xFF, name);
            if (available == 0) SH_CALL(Menu_DrawIcon8)(X(w) + 8, icon_y, 6, 1);
            else SH_CALL(Text_DrawAt)(X(w) + 5, text_y, 7, 0xFF, Text(at::kMasterMark));
            text_y -= 2;
            icon_y -= 2;
        }
        const int colour = picked == i ? 2 : 0;
        const unsigned char* const name = Message(list[i] + 0x110u);
        SH_CALL(Text_DrawAt)(X(w) + 0x13, text_y, colour, 0xFF, name);
        if (available == 0) SH_CALL(Menu_DrawIcon8)(X(w) + 8, icon_y, 6, 0);
        else SH_CALL(Text_DrawAt)(X(w) + 5, text_y, colour, 0xFF, Text(at::kMasterMark));
        if (lit == i) {
            text_y += 2;
            icon_y += 2;
        }
    }
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 3, 0x71, 0x14, 0, Style());
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 0x92, 0x71, 8, 0, Style());
    const int title_y = Y(w) + 7;
    const unsigned char length = SH_CALL(Text_CharCount)(Text(at::kMasterTitle));
    SH_CALL(Text_DrawAt)(X(w) - 6 * static_cast<int>(length) + 0x3A, title_y, 0, 0x10, Text(at::kMasterTitle));
    Pieces(X(w), Y(w), at::kMasterPiecesA);
    Pieces(X(w), Y(w), at::kMasterPiecesB);
    for (int i = 0; i < 5; ++i) Piece(8 * i + 0x28 + X(w), Y(w), 1);
    for (int i = 0; i < 15; ++i) Piece(X(w), 8 * i + 0x18 + Y(w), 4);
    Piece(X(w) + 0x68, Y(w) + 0x18, 0x16);
    for (int i = 0; i < 12; ++i) Piece(X(w) + 0x68, 8 * i + 0x28 + Y(w), 0x17);
    Piece(X(w) + 0x68, Y(w) + 0x88, 0x18);
    Piece(X(w), Y(w) + 0x90, 0x19);
    for (int i = 0; i < 12; ++i) Piece(8 * i + 8 + X(w), Y(w) + 0x90, 0x1A);
    Piece(X(w) + 8, Y(w), 0x32);
    Piece(X(w) + 0x68, Y(w), 0x33);
    SH_CALL(Menu_DrawScrollBar)(list, w[0xA], X(w) + 0x68, Y(w) + 0x18, 9, 0x11, 0x74);
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
    case 0xB: return (B(at::kPartyBits) & 8) ? 1 : 0;
    case 0xC: return SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x6C) ? 1 : 0;
    case 0xD: return (B(at::kPartyBits) & 0x10) ? 1 : 0;
    case 0xE: return (B(at::kPartyBits) & 0x20) ? 1 : 0;
    default: break;
    }
    if (k >= at::kMasterCount)
        bof3::Fatal("MasterWin_Available (0x59C780): master %u past the %u lists of 0x%X - the original reads the next "
                    ".data as a pointer (docs/rest_2h.md section 7)",
                    k, at::kMasterCount, (unsigned)at::kRequirements);
    const unsigned char* p = At(L(at::kRequirements + 4 * k));
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
            if (B(at::kSkillSlots + k * at::kRecordStride + j) == s) return 1;
    for (U j = 0; j < 0x80; ++j)
        if (B(at::kSharedList + j) == s) return 1;
    return 0;
}

// original 0x59C870: the master's caption of the window `w`: the box (x + 3,
// y + 3, 0x9A, 0x39, 0, style), the border Menu_DrawBorder(x, y, 0x12, 6), and
// system message 0x100 + +0xA at (x + 0xE, y + 4, 0, 0xFF).
extern "C" void __cdecl MasterWin_DrawCaption(unsigned char* w) {
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 3, 0x9A, 0x39, 0, style);
    SH_CALL(Menu_DrawBorder)(X(w), Y(w), 0x12, 6);
    const unsigned char* const text = Message(w[0xA] + 0x100u);
    SH_CALL(Text_DrawAt)(X(w) + 0xE, Y(w) + 4, 0, 0xFF, text);
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
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 3, 0x72, 0x58, 0, style);
    int dx = 0, dy = 0;
    for (U k = 0; k < 8; ++k) {
        const unsigned char* const rec = Record(k);
        if (!(rec[0xB] & 1)) continue;
        if (rec[0x1F] != B(at::kWindow1Master)) continue;
        const unsigned portrait = rec[9];
        SH_CALL(MasterWin_DrawPortrait)(dx + X(w) + 4, dy + Y(w) + 5, portrait, 0);
        dx += 0x25;
        if (static_cast<short>(dx) > 0x4A) {
            dx = 0;
            dy += 0x2A;
        }
    }
    SH_CALL(Menu_DrawBorder)(X(w), Y(w), 0xD, 0xA);
    const unsigned char style2 = Style();
    SH_CALL(Menu_DrawBox)(X(w) - 0x25, Y(w) + 3, 0x22, 0x10, 0, style2);
    SH_CALL(Menu_DrawBorder)(X(w) - 0x28, Y(w), 3, 1);
    SH_CALL(Text_DrawAt)(X(w) - 0x20, Y(w) + 4, 0, 0xFF, Text(at::kPupilLabel));
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
    const unsigned char* const cell = At(at::kPortraitCells + 4 * k);
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

// ===========================================================================
// Window_Handler8KindTable's kinds 3..5 and BattleMenuWin_ItemListSteps[3]
// ===========================================================================

// original 0x59CB90, BattleMenuWin_ItemListSteps[3]: x -= 0x20; below 0x53,
// x = 0x52 and step 0. As the original has it: the test against 0x53 and the
// store 0x52, the pair of its twin BattleMenuWin_ItemListSlideRight (D88).
extern "C" void __cdecl BattleMenuWin_ItemListSlideLeft(void) { Slide(4, -0x20, 0x53, true, 0x52); }

// original 0x59CC10, handler 8 kind 3: the step (BattleMenuWin_EquipSteps: a
// bare ret, 0x596920 - R2F's, MenuSlide_RightTo17, BattleMenuWin_EquipSlideTo62),
// then BattleEquipWin_Draw(the member 0x66972C[0x904065[+0xC]], +4, +6, +0x20,
// +0xD, the record) of the record re-read.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMenuWin_EquipRun(void) {
    Step("BattleMenuWin_EquipRun", at::kEquipSteps, 4);
    unsigned char* const w = Rec();
    const U member = RecordOf(B(at::kBattleParty + w[0xC]));
    SH_CALL(BattleEquipWin_Draw)(member, static_cast<unsigned>(X(w)), static_cast<unsigned>(Y(w)),
                                 static_cast<U>(Long(w + 0x20)), w[0xD], Key(w));
}

// original 0x59CC60, kind 3 step 3: x += 0x20; above 0x62, x = 0x62 and step 0.
extern "C" void __cdecl BattleMenuWin_EquipSlideTo62(void) { Slide(4, 0x20, 0x62, false, 0x62); }

// original 0x59CC90, handler 8 kind 4: the step (BattleMenuWin_EquipItemsSteps:
// a bare ret, BattleMenuWin_EquipItemsSlideTo98, MenuSlide_RightOff, 0x59A610 -
// R2G's), then BattleEquipWin_DrawItems(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMenuWin_EquipItemsRun(void) {
    Step("BattleMenuWin_EquipItemsRun", at::kEquipItemsSteps, 4);
    SH_CALL(BattleEquipWin_DrawItems)(Rec());
}

// original 0x59CCB0, kind 4 step 1: x += 0x20; above 0x98, x = 0x98 and step 0.
extern "C" void __cdecl BattleMenuWin_EquipItemsSlideTo98(void) { Slide(4, 0x20, 0x98, false, 0x98); }

// original 0x59CCE0, handler 8 kind 5: Menu_DrawVerbPair(+4, +6, +0xB) of the
// record. No step.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMenuWin_VerbPairRun(void) {
    const unsigned char* const w = Rec();
    SH_CALL(Menu_DrawVerbPair)(X(w), Y(w), w[0xB]);
}

// original 0x59DB70: one 8 x 8 sprite of a stat bar (x, y, u, v, clut, shade):
// at the packet cursor (read once) Gpu_SetSprt8; x and y (their words,
// unsigned) as floats; u << 3, v << 3 (bytes); the CLUT word; the shade byte
// as its grey; Gpu_SetSemiTrans(.., 0); Gfx_CommitPrim(1, 0x18).
// BattleEquipWin_Draw (battle_e7.cpp) and 0x585DC0 (R2C's) call it.
extern "C" void __cdecl BattleEquipWin_DrawBar(int x, int y, unsigned u, unsigned v, unsigned clut, unsigned shade) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt8)(p);
    PutFloat(p + 8, static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFF));
    PutFloat(p + 0xC, static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFF));
    p[0x15] = static_cast<unsigned char>(v << 3);
    p[0x14] = static_cast<unsigned char>(u << 3);
    SetWord(p + 0x16, clut);
    p[6] = p[5] = p[4] = static_cast<unsigned char>(shade);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0x18);
}

// original 0x59DBF0: the battle equipment window's item list of the window
// `w`. +8 = the category the slot +9 names (1..3: 2, the armour; 4, 5: 3;
// else 1). For each of the category's 128 items (Inventory_IdLists /
// _CountLists by +8) that Item_CanUse(4, the member 0x66972C[0x904065[+0xC]],
// +8, item) allows - and, for the armour, whose Item_IconKind(2, item) is +9 +
// 1 - its id and count into 0x6BE0C4 / 0x6BE144, then 0s to 128 bytes. +0xD =
// the id at +0xB. The box (x + 3, y + 3, 0x99, 0x82, 0, style);
// Menu_ListScroll(+0xA, &offset, &moving, +0x10); from the answered top, rows r
// below moving + 7 at y + offset + 0x1A + 13 r, each non-empty one
// Menu_DrawItemRow(x + 7, that, colour, +8, id, count, dim) - the row whose
// +0xA (re-read) + r is +0xB lit (7, dim 1) and then two up in 0, the others in
// 0. The title and count boxes (x + 3, y + 3, 0x99, 0x14), (x + 3, y + 0x7A,
// 0x99, 8); the category's title (0x66B5C4[+8]) centred (x + 6 (13 - its
// length), y + 7, 0, 0x10) - through the call DIV-0059 re-aims; the count "n /
// 128" (0x6639C8) at (x + 0x55, y + 0x7A); the frame pieces; the scroll bar
// (0x6BE0C4, +0xA, x + 0x90, y + 0x18, 7, 0x80, 0x5C).
//
// As the original has it: the rows read the lists by top + r past their 128
// bytes, in place; the title's pointer is read in place (labels.cpp retargets
// the table's words under a language overlay).
extern "C" void __cdecl BattleEquipWin_DrawItems(unsigned char* w) {
    const unsigned slot = static_cast<unsigned>(w[9]) - 1u;
    if (slot <= 2) w[8] = 2;
    else if (slot <= 4) w[8] = 3;
    else w[8] = 1;
    const unsigned cat = w[8];
    const unsigned char* ids = At(L(at::kIdLists + 4 * cat));
    const unsigned char* counts = At(L(at::kCountLists + 4 * cat));
    unsigned char n = 0;
    for (unsigned k = 0; k < 0x80; ++k, ++ids, ++counts) {
        const unsigned id = *ids;
        const U member = RecordOf(B(at::kBattleParty + w[0xC]));
        const unsigned char allowed = SH_CALL(Item_CanUse)(4, member, w[8], id);
        if (allowed == 0) continue;
        if (w[8] == 2) {
            const unsigned kind = SH_CALL(Item_IconKind)(2, *ids) & 0xFF;
            if (static_cast<int>(w[9]) - 1 != static_cast<int>(kind) - 2) continue;
        }
        B(at::kUseIds + n) = *ids;
        B(at::kUseCounts + n) = *counts;
        ++n;
    }
    if (n < 0x80) {
        std::memset(At(at::kUseCounts + n), 0, 0x80u - n);
        std::memset(At(at::kUseIds + n), 0, 0x80u - n);
    }
    w[0xD] = B(at::kUseIds + w[0xB]);
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 3, 0x99, 0x82, 0, style);
    unsigned char offset = 0, moving = 0;
    const unsigned char top = SH_CALL(Menu_ListScroll)(w + 0xA, &offset, &moving, w + 0x10);
    int text_y = S8(offset) + Y(w) + 0x1A;
    const U row_ids = at::kUseIds + top;
    const U row_counts = at::kUseCounts + top;
    for (unsigned char r = 0; static_cast<int>(r) < static_cast<int>(moving) + 7; ++r, text_y += 0xD) {
        const unsigned char id = B(row_ids + r);
        if (id == 0) continue;
        if (static_cast<unsigned>(w[0xA]) + r == w[0xB]) {
            SH_CALL(Menu_DrawItemRow)(X(w) + 7, text_y, 7, w[8], id, B(row_counts + r), 1);
            SH_CALL(Menu_DrawItemRow)(X(w) + 7, text_y - 2, 0, w[8], B(row_ids + r), B(row_counts + r), 0);
        } else {
            SH_CALL(Menu_DrawItemRow)(X(w) + 7, text_y, 0, w[8], id, B(row_counts + r), 0);
        }
    }
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 3, 0x99, 0x14, 0, Style());
    SH_CALL(Menu_DrawBox)(X(w) + 3, Y(w) + 0x7A, 0x99, 8, 0, Style());
    const unsigned title_cat = w[8];
    const int title_y = Y(w) + 7;
    const unsigned char* const title = At(L(at::kUseTitles + 4 * title_cat));
    const unsigned char length = SH_CALL(Text_CharCount)(title);
    using DrawAt = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);
    SH_AT(DrawAt, SiteTarget(at::kTitleSite))(6 * (0xD - static_cast<int>(length)) + X(w), title_y, 0, 0x10, title);
    SH_CALL(Crt_sprintf)(PrintBuf(), Format(at::kFmtCount), static_cast<unsigned>(n), 0x80u);
    SH_CALL(Text_DrawFont8)(X(w) + 0x55, Y(w) + 0x7A, 0, Text(at::kPrintBuf));
    Pieces(X(w), Y(w), at::kUsePiecesA);
    Pieces(X(w), Y(w), at::kUsePiecesB);
    for (int i = 0; i < 10; ++i) Piece(8 * i + 0x28 + X(w), Y(w), 1);
    for (int i = 0; i < 12; ++i) Piece(X(w), 8 * i + 0x18 + Y(w), 4);
    Piece(X(w) + 0x90, Y(w) + 0x18, 0x16);
    for (int i = 0; i < 9; ++i) Piece(X(w) + 0x90, 8 * i + 0x28 + Y(w), 0x17);
    Piece(X(w) + 0x90, Y(w) + 0x70, 0x18);
    Piece(X(w), Y(w) + 0x78, 0x19);
    for (int i = 0; i < 9; ++i) Piece(8 * i + 8 + X(w), Y(w) + 0x78, 0x1A);
    Piece(X(w) + 0x50, Y(w) + 0x78, 0x1B);
    for (int i = 0; i < 6; ++i) Piece(8 * i + 0x58 + X(w), Y(w) + 0x78, 0x1C);
    Piece(X(w) + 0x88, Y(w) + 0x78, 0x1D);
    Piece(X(w) + 8, Y(w), 0x32);
    Piece(X(w) + 0x90, Y(w), 0x33);
    SH_CALL(Menu_DrawScrollBar)(At(at::kUseIds), w[0xA], X(w) + 0x90, Y(w) + 0x18, 7, 0x80, 0x5C);
}

// original 0x59E160: two verb buttons (x, y, selected): the box (x, y, 0x2D,
// 0x14, 0, style), verb 0x66A228 at (x + 0xA, y + 3, colour, 0x10) - colour 7
// unless selected is 0xFF or 0 - and the pieces 0x66B500 when selected is 0,
// else 0x66B4F8; then the same 0x30 to the right for verb 0x66A240, its colour 7
// unless selected is 0xFF or 1, its pieces 0x66B500 when selected is 1. The
// catalogue filed it under the renderer by its address; it is game code, in no
// group, and only BattleMenuWin_VerbPairRun calls it.
extern "C" void __cdecl Menu_DrawVerbPair(int x, int y, unsigned selected) {
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(x, y, 0x2D, 0x14, 0, style);
    const auto s = static_cast<unsigned char>(selected);
    SH_CALL(Text_DrawAt)(x + 0xA, y + 3, s == 0xFF || s == 0 ? 0 : 7, 0x10, Text(at::kVerbA));
    SH_CALL(Menu_DrawPieces)(x, y, Text(s == 0 ? at::kVerbPiecesOn : at::kVerbPiecesOff), 1);
    const int x2 = x + 0x30;
    const unsigned char style2 = Style();
    SH_CALL(Menu_DrawBox)(x2, y, 0x2D, 0x14, 0, style2);
    SH_CALL(Text_DrawAt)(x2 + 0xA, y + 3, s == 0xFF || s == 1 ? 0 : 7, 0x10, Text(at::kVerbB));
    SH_CALL(Menu_DrawPieces)(x2, y, Text(s == 1 ? at::kVerbPiecesOn : at::kVerbPiecesOff), 1);
}

// ===========================================================================
// The platform strays
// ===========================================================================

// original 0x5A9620: the EnumDevices callback the original DInput_Init hands
// IDirectInput (stdcall (instance, context)). CreateDevice(DInput_Object,
// instance->guidInstance, &DInput_Joystick, 0): failed, answer 1 (go on); else
// QueryInterface(DInput_Joystick re-read, the IID 0x5C4718, &DInput_Joystick2),
// and when the instance's product name equals 0x66C7B0's case-blind (the C
// runtime's _stricmp), DInput_JoystickFound = 1; answer 0 (stop). Ours never
// enumerates (DIV-0050: DInput_Init is ours, pad_read.cpp), so only
// BOF3X_ORIGINAL=DInput_Init reaches it; faithful for that.
extern "C" int __stdcall DInput_EnumJoystick(const void* instance, void* context) {
    (void)context;
    auto* const di = static_cast<IDirectInputA*>(DInput_Object);
    const auto* const inst = static_cast<const unsigned char*>(instance);
    const HRESULT created =
        di->CreateDevice(*reinterpret_cast<const GUID*>(inst + 4), reinterpret_cast<LPDIRECTINPUTDEVICEA*>(&DInput_Joystick), nullptr);
    if (created != 0) return 1;
    auto* const joy = static_cast<IUnknown*>(DInput_Joystick);
    joy->QueryInterface(*reinterpret_cast<const IID*>(static_cast<std::uintptr_t>(at::kJoystickIid)), &DInput_Joystick2);
    const int differs = SH_AT(int (__cdecl*)(const char*, const char*), at::kStricmp)(
        reinterpret_cast<const char*>(inst + 0x12C), reinterpret_cast<const char*>(At(at::kProductName)));
    if (differs == 0) DInput_JoystickFound = 1;
    return 0;
}

// original 0x5A9860: Cfg_Load's key table, 32 dwords from `table` into
// Key_Table (rep movsd, forward).
extern "C" void __cdecl Cfg_SetKeyTable(const void* table) {
    const auto* const from = static_cast<const unsigned char*>(table);
    auto* const to = reinterpret_cast<unsigned char*>(Key_Table);
    for (unsigned i = 0; i < 0x20; ++i) {
        std::uint32_t v;
        std::memcpy(&v, from + 4 * i, sizeof v);
        std::memcpy(to + 4 * i, &v, sizeof v);
    }
}

// ===========================================================================

void Rest2H_Inject() {
    // MasterWin_SlideOut reads its bound back from the original's `mov ecx,
    // imm32` (B9): anything else there means the site moved.
    if (B(at::kMasterBoundMov) != 0xB9)
        bof3::Fatal("rest_2h: 0x%X holds 0x%02X, not MasterWin_SlideOut's mov ecx", (unsigned)at::kMasterBoundMov,
                    B(at::kMasterBoundMov));
    bof3::Log("rest_2h: MasterWin_SlideOut's bound %d (0x59C136; -120 unless DIV-0041 widened it)",
              static_cast<int>(static_cast<short>(Word(At(at::kMasterBound)))));
    if (bof3::WantsShadow("rest_2h")) rest_2h::SelfTest();
    BOF3_INJECT(MenuList_ReserveWinDraw);
    BOF3_INJECT(MenuList_GeneWinRun);
    BOF3_INJECT(MenuList_GeneWinDraw);
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
    BOF3_INJECT(BattleMenuWin_ItemListSlideLeft);
    BOF3_INJECT(BattleMenuWin_EquipRun);
    BOF3_INJECT(BattleMenuWin_EquipSlideTo62);
    BOF3_INJECT(BattleMenuWin_EquipItemsRun);
    BOF3_INJECT(BattleMenuWin_EquipItemsSlideTo98);
    BOF3_INJECT(BattleMenuWin_VerbPairRun);
    BOF3_INJECT(BattleEquipWin_DrawBar);
    BOF3_INJECT(BattleEquipWin_DrawItems);
    BOF3_INJECT(Menu_DrawVerbPair);
    BOF3_INJECT(DInput_EnumJoystick);
    BOF3_INJECT(Cfg_SetKeyTable);
}
