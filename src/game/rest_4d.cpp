// Two of the community's games and the board helpers a third draws with -
// round fourteen, wave four, group R4D: the cut's 60 functions
// 0x45C400..0x45E86E (analysis/round14_cut.tsv), each read with capstone to its
// last instruction (docs/rest_4d.md section 1).
//
// Every call out goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies; a
// table's handler is called by the address the table holds. No divergence: each
// is a faithful replacement. Where the original jumps through a table by a byte
// past the table's own count, ours aborts with a message (section 3): no state
// writes such a byte. Where it dereferences a name pointer past
// CommuName_RecordNames' seven, or loops forever on a limit of 0
// (CommuDraw_RandBelow), ours aborts before the read or the loop (section 5).
// Byte reads past a table (a slot's name, a slot's default, the draw's rows
// with a negative row) read the image in place, as the original does.
//
// Coordinates: the original builds several in a 16-bit register whose upper
// half is the previous call's eax (`movzx ax, byte`); every callee reads those
// arguments' low word (cited in rest_4d_fuzz.cpp's callee rows), so ours passes
// the value with a clean upper half.
#include "game/rest_4d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_4d_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return *At(address); }
int S8(U address) { return static_cast<signed char>(*At(address)); }
unsigned W(U address) { return Word(At(address)); }
U L(U address) { return static_cast<U>(Long(At(address))); }
void PutW(U address, unsigned v) { SetWord(At(address), v); }
void PutL(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
void PutF(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }
const unsigned char* Text(U address) { return At(address); }

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (the states
// its game writes, docs/rest_4d.md section 3). Past it the original jumps
// through the next table's cell or the data after it; no state writes such an
// index, so ours aborts. The entry is called as the cell holds it: Capcom's
// address in the game (Inject's jmp to ours where it is ours), a recorder while
// the fuzz runs.
using Handler = void (__cdecl*)();
void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(table), static_cast<unsigned>(table + 4 * index));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * index)))();
}

// --- the callees of other groups of this wave, by address ---------------------------
void BoardCell(int x, int y, unsigned value) { SH_AT(void (__cdecl*)(int, int, unsigned), at::kBoardCell)(x, y, value); }
void BoardPiece(int x, int y, unsigned piece) { SH_AT(void (__cdecl*)(int, int, unsigned), at::kBoardPiece)(x, y, piece); }
void SlotPanel(int x, int y, unsigned slot, unsigned flag) {
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kSlotPanel)(x, y, slot, flag);
}
void ListPiece(int x, int y, unsigned piece) { SH_AT(void (__cdecl*)(int, int, unsigned), at::kListPiece)(x, y, piece); }
void SlotHand(int x, int y, unsigned flag) { SH_AT(void (__cdecl*)(int, int, unsigned), at::kSlotHand)(x, y, flag); }
U RandomName() { return SH_AT(U (__cdecl*)(), at::kRandomName)(); }
void MemberPanel(int x, int y, unsigned record, unsigned flag) {
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kMemberPanel)(x, y, record, flag);
}
void MemberPanels() { SH_AT(void (__cdecl*)(), at::kMemberPanels)(); }
unsigned char MemberCount() { return SH_AT(unsigned char (__cdecl*)(), at::kMemberCount)(); }
U NthMember(unsigned n) { return SH_AT(U (__cdecl*)(unsigned), at::kNthMember)(n); }
void MemberHand(int x, int y, unsigned flag, unsigned six) {
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kMemberHand)(x, y, flag, six);
}
void EntryBox(int a, int y, unsigned c, unsigned d) { SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kEntryBox)(a, y, c, d); }
void MemberRename() { SH_AT(void (__cdecl*)(), at::kMemberRename)(); }
void SlotRename() { SH_AT(void (__cdecl*)(), at::kSlotRename)(); }

// BareRet (ours, a bare ret) called with the arguments the PSX's name entry
// took; it reads none. BareRetZero likewise with none.
template <typename... A> void Dropped(A... a) {
    using F = void (__cdecl*)(A...);
    SH_CALL(reinterpret_cast<F>(reinterpret_cast<void*>(&::BareRet)))(a...);
}
unsigned char DroppedZero() {
    using F = unsigned char (__cdecl*)();
    return SH_CALL(reinterpret_cast<F>(reinterpret_cast<void*>(&::BareRetZero)))();
}

char* Scratch() { return reinterpret_cast<char*>(At(at::kTextScratch)); }

// A message's text: MessagePools' word at `words + 2 * index` from 0x803580.
const unsigned char* PoolText(U words, int index) {
    return Text(at::kPoolWords + Word(At(words + 2u * static_cast<U>(index))));
}

// The panel's slide (0x675F95) as the offsets the states build.
int Slide() { return B(at::kCount); }

// The title box both games open with: (0x14, 0x10 - 8 * slide), 0x118 by 0x13.
void PanelBox(unsigned char slide) {
    SH_CALL(Menu_DrawPanelBox)(0x14, 0x10 - 8 * static_cast<int>(slide), 0x118, 0x13, B(at::kColour));
}

// The entry's three draws (BareRet, the box, BareRet), each `per` * the slide
// (read again before each) further down.
void EntryDraws(int per) {
    const int first = 0xB0 + per * Slide();
    Dropped(static_cast<U>(B(at::kEntryB)), static_cast<U>(first), 0x2Bu);
    EntryBox(0x12, 0x61 + per * Slide(), 0x24, 0x11);
    const unsigned char a = B(at::kEntryA), c = B(at::kEntryC);
    const int third = 0x67 + per * Slide();
    const unsigned char b = B(at::kEntryB);
    Dropped(static_cast<U>(b), 0x18u, static_cast<U>(third), static_cast<U>(c), static_cast<U>(a));
}

// The slot's panel at (x, 0x32) with the cursor's slot.
void CursorSlotPanel(int x, unsigned flag) {
    const unsigned slot = SH_CALL(CommuName_NthSlot)(B(at::kCursor));
    SlotPanel(x, 0x32, slot, flag);
}
// The member's panel at (x, 0x2B) with the cursor's record.
void CursorMemberPanel(int x, unsigned flag) {
    const U record = NthMember(B(at::kCursor));
    MemberPanel(x, 0x2B, record, flag);
}

// Message `id` opened, Field_Request 2.
void Say(unsigned id) {
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id));
    Field_Request = 2;
}

// The record pointer CommuName_MemberEntryOut reads: CommuName_RecordNames[index].
U RecordName(unsigned index) {
    if (index >= at::kRecordNameCount)
        bof3::Fatal("CommuName_MemberEntryOut: record %u is past CommuName_RecordNames' %u at 0x%X; the original reads a pointer at 0x%X",
                    index, at::kRecordNameCount, static_cast<unsigned>(at::kRecordNames),
                    static_cast<unsigned>(at::kRecordNames + 4 * index));
    return L(at::kRecordNames + 4 * index);
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ===========================================================================
// The board (R4C's states draw it: 0x675F98 five bytes a row, 0x675F8C the row)
// ===========================================================================

// original 0x45C400: the board's box (x, y) 0x52 by 0x7A, its frame, then eight
// rows 14 apart. A row k's bytes start at 0x675F98 + (5k + 3) & 0xFF. Below the
// cursor row (s8 0x675F8C, read again each row), and at it when `shown`'s byte
// is not 0: the first three bytes as one line (format 0x653088) in colour 0, the
// fourth and the fifth as numbers in colours 2 and 1; at it with `shown` 0: the
// blank formats in colours 0, 2, 1; past it: the blanks in colour 7. Every row
// then the dash 0x653084 in Font8 at (x + 0x3B, row y + 9), colour 7 past the
// cursor, else 0.
extern "C" void __cdecl CommuBoard_DrawRows(int x, int y, unsigned shown) {
    char* const buf = Scratch();
    const auto text = reinterpret_cast<const unsigned char*>(buf);
    SH_CALL(Menu_DrawBox)(x, y, 0x52, 0x7A, 0, B(at::kColour));
    SH_CALL(CommuBoard_DrawFrame)(x, y);
    for (int row = 0; row < 8; ++row) {
        const unsigned k = static_cast<unsigned char>(row * 5 + 3);
        const int ry = y + 14 * row;
        const int cursor = S8(at::kCursor);
        int colour = 0;
        if (row == cursor && (shown & 0xFF) == 0) {
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(at::kBoardBlankB)));
            SH_CALL(Text_DrawFont12)(x + 6, ry + 7, 0, text);
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(at::kBoardBlankA)));
            SH_CALL(Text_DrawFont12)(x + 0x2F, ry + 7, 2, text);
            SH_CALL(Text_DrawFont12)(x + 0x43, ry + 7, 1, text);
        } else if (row <= cursor) {
            const unsigned a = B(at::kName + k), b = B(at::kName + k + 1), c = B(at::kName + k + 2);
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(at::kBoardFmt3)), a, b, c);
            SH_CALL(Text_DrawFont12)(x + 6, ry + 7, 0, text);
            const unsigned d = B(at::kName + k + 3);
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(at::kNumberFmt)), d);
            SH_CALL(Text_DrawFont12)(x + 0x2F, ry + 7, 2, text);
            const unsigned e = B(at::kName + k + 4);
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(at::kNumberFmt)), e);
            SH_CALL(Text_DrawFont12)(x + 0x43, ry + 7, 1, text);
        } else {
            colour = 7;
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(at::kBoardBlankB)));
            SH_CALL(Text_DrawFont12)(x + 6, ry + 7, 7, text);
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(at::kBoardBlankA)));
            SH_CALL(Text_DrawFont12)(x + 0x2F, ry + 7, 7, text);
            SH_CALL(Text_DrawFont12)(x + 0x43, ry + 7, 7, text);
        }
        SH_CALL(Text_DrawFont8)(x + 0x3B, ry + 9, colour, Text(at::kBoardText));
    }
}

// original 0x45C700: the board's frame in R4C's pieces (0x45B400) after one
// draw-mode packet (tpage 0x1D) committed: the top-left corner 0xB at (x, y),
// nine top pieces 0xD at y - 1, the top-right 0xC at x + 0x4F; fourteen rows of
// the sides 0xE / 0xF at x and x + 0x50; the bottom-left 0x10 at y + 0x78, nine
// 0xD, the bottom-right 0x11.
extern "C" void __cdecl CommuBoard_DrawFrame(int x, int y) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1D, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    BoardPiece(x, y, 0xB);
    for (int i = 0; i < 9; ++i) BoardPiece(x + i * 8 + 8, y - 1, 0xD);
    BoardPiece(x + 0x4F, y, 0xC);
    for (int i = 0; i < 14; ++i) {
        const int yi = y + i * 8 + 8;
        BoardPiece(x, yi, 0xE);
        BoardPiece(x + 0x50, yi, 0xF);
    }
    const int bottom = y + 0x78;
    BoardPiece(x, bottom, 0x10);
    for (int i = 0; i < 9; ++i) BoardPiece(x + i * 8 + 8, bottom, 0xD);
    BoardPiece(x + 0x50, bottom, 0x11);
}

// original 0x45C7D0: row `row`'s first three bytes (0x675F98 + (5 * row + 3) &
// 0xFF) as R4C's cells 32 apart from x; cell i 8 higher when the column byte
// (s8 0x675F8D, read again each cell) is i and `lift`'s byte is not 0.
extern "C" void __cdecl CommuBoard_DrawRowCells(int x, int y, unsigned row, unsigned lift) {
    const unsigned k = static_cast<unsigned char>(row * 5 + 3);
    for (int i = 0; i < 3; ++i) {
        const bool up = S8(at::kYes) == i && (lift & 0xFF) != 0;
        BoardCell(x + i * 32, up ? y - 8 : y, B(at::kName + k + static_cast<U>(i)));
    }
}

// original 0x45C850: the first row's three bytes as cells 32 apart when
// `shown`'s byte is not 0, else three blank cells (0xFF).
extern "C" void __cdecl CommuBoard_DrawCells(int x, int y, unsigned shown) {
    if ((shown & 0xFF) != 0) {
        for (int i = 0; i < 3; ++i) BoardCell(x + i * 32, y, B(at::kName + static_cast<U>(i)));
    } else {
        for (int i = 0; i < 3; ++i) BoardCell(x + i * 32, y, 0xFF);
    }
}

// ===========================================================================
// CommuDraw: 0x652A70[7] (CommuDraw_States 0x652DC4, by 0x939A3E)
// ===========================================================================

// original 0x45C8C0: CommuDraw_States[0x939A3E].
extern "C" void __cdecl CommuDraw_Dispatch(void) { Jump(at::kDrawStates, 3, B(at::kState), "CommuDraw_Dispatch"); }

// original 0x45C8D0, state 0: CommuDraw_OpenSteps[0x939A40].
extern "C" void __cdecl CommuDraw_OpenStep(void) { Jump(at::kDrawOpenSteps, 2, B(at::kStep), "CommuDraw_OpenStep"); }

// original 0x45C8E0, state 0 step 0: Transition_Start(0), the step on.
extern "C" void __cdecl CommuDraw_FadeOut(void) {
    SH_CALL(Transition_Start)(0);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x45C900, state 0 step 1: once the wait word is 0 - the playing
// track (Music_Track) kept at 0x675F90, Draw_PassFlags 0, the music faded out
// over 10 and track 0x67 played over 8; state 1, step 0.
extern "C" void __cdecl CommuDraw_MusicIn(void) {
    if (MoveScript_WaitWordDA != 0) return;
    const unsigned char track = Music_Track;
    Draw_PassFlags = 0;
    B(at::kKeptTrack) = track;
    SH_CALL(Music_FadeOutStop)(10);
    SH_CALL(Music_Play)(0x67, 8);
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    B(at::kStep) = 0;
    B(at::kState) = state;
}

// original 0x45C950, state 1: CommuDraw_ShowSteps[0x939A40].
extern "C" void __cdecl CommuDraw_ShowStep(void) { Jump(at::kDrawShowSteps, 5, B(at::kStep), "CommuDraw_ShowStep"); }

namespace {

// The draw's search for a pick not yet taken: `a` is kept when the category has
// no picks or when one of its picks differs from it (the original's test: it
// moves on only while every earlier pick equals `a` - docs/rest_4d.md section 5,
// L1); else `a` + 1, wrapping to 0 at the category's count.
unsigned char Unpicked(unsigned category, unsigned char a, unsigned count) {
    const unsigned char limit = B(at::kDrawPools + 2 * category + 1);
    for (;;) {
        unsigned i = 0;
        if (static_cast<int>(count) > 0) {
            while (i < count && B(at::kName + 10 * category + i) == a) ++i;
            if (i < count) return a;
        }
        if (static_cast<unsigned char>(count) == 0) return a;
        a = static_cast<unsigned char>(a + 1);
        if (a >= limit) a = 0;
    }
}

// One of a category's picks: the first of them, then each next one in turn
// while Rand's bit 0 is 0, up to the category's count less one (read again
// after each Rand).
unsigned PickOfCategory(U row) {
    unsigned i = 0;
    if (static_cast<int>(B(at::kPicked + B(row))) - 1 > 0) {
        for (;;) {
            if ((static_cast<unsigned>(SH_CALL(Rand)()) & 1) != 0) break;
            ++i;
            if (!(static_cast<int>(i) < static_cast<int>(B(at::kPicked + B(row))) - 1)) break;
        }
    }
    return i;
}

}  // namespace

// original 0x45C960, state 1 step 0: the draw. The row (Rand & 7) to
// 0x675F8C, the four counts 0x675FC0.. cleared; then for each of the nine text
// records (0x904CE0, 0x20 apart) the row's next category byte of the 8 x 9
// table 0x652DEC (row read again as s8, the table read in place; 9 ends): a pick
// CommuDraw_RandBelow(the category's count) brought to one not taken (above),
// kept in the category's ten 0x675F98 + 10c and counted; the message
// (category's first + pick) copied into the record from MessagePools - each
// byte copied, the copy ending after a 0 whose byte before has no bit 7, or at
// 0x20. Then the title's two: Rand & 7 against 0x652E3C's thresholds picks a row
// j (the first threshold not above it, else 3; row 6 turns j 2 into 3 and row 7
// j 3 into 2); each of its two categories (the second may be 0xFF: none) gives
// a message by Rand's bit 0 - one of the picks (above) or a fresh
// CommuDraw_RandBelow - plus the category's first, into 0x675FC5 / 0x675FC6.
// The step on, 0x675F95 0.
extern "C" void __cdecl CommuDraw_Pick(void) {
    const unsigned char row0 = static_cast<unsigned char>(SH_CALL(Rand)()) & 7;
    B(at::kPicked) = 0;
    B(at::kCursor) = row0;
    B(at::kPicked + 1) = 0;
    B(at::kPicked + 2) = 0;
    B(at::kPicked + 3) = 0;
    int row = static_cast<signed char>(row0);
    U record = at::kTextRecords;
    for (unsigned i = 0;; ++i) {
        const unsigned category = B(at::kDrawRows + static_cast<U>(row * 9 + static_cast<int>(i)));
        if (category == 9) break;
        const unsigned char first = SH_CALL(CommuDraw_RandBelow)(B(at::kDrawPools + 2 * category + 1));
        const unsigned count = B(at::kPicked + category);
        const unsigned char a = Unpicked(category, first, count);
        B(at::kName + 10 * category + count) = a;
        const unsigned char counted = static_cast<unsigned char>(B(at::kPicked + category) + 1);
        const unsigned message = B(at::kDrawPools + 2 * category) + a;
        B(at::kPicked + category) = counted;
        const unsigned char* p = PoolText(at::kDrawWords, static_cast<int>(message));
        for (unsigned n = 0;;) {
            B(record + n) = *p;
            if (*p == 0 && (p[-1] & 0x80) == 0) break;
            ++p;
            if (++n >= 0x20) break;
        }
        record += 0x20;
        if (record >= at::kTextRecordsEnd) break;
        row = S8(at::kCursor);
    }

    const unsigned char roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 7;
    unsigned j = 0;
    while (j < 3 && roll < B(at::kDrawPicks + 3 * j)) ++j;
    const unsigned char cursor = B(at::kCursor);
    if (cursor == 6) {
        if (j == 2) j = 3;
    } else if (cursor == 7) {
        if (j == 3) j = 2;
    }
    const U pick = at::kDrawPicks + 3 * j;
    if ((static_cast<unsigned>(SH_CALL(Rand)()) & 1) != 0) {
        const unsigned i = PickOfCategory(pick + 1);
        const unsigned category = B(pick + 1);
        B(at::kTitleA) = static_cast<unsigned char>(B(at::kName + 10 * category + i) + B(at::kDrawPools + 2 * category));
    } else {
        const unsigned char a = SH_CALL(CommuDraw_RandBelow)(B(at::kDrawPools + 2 * B(pick + 1) + 1));
        B(at::kTitleA) = static_cast<unsigned char>(a + B(at::kDrawPools + 2 * B(pick + 1)));
    }
    if (B(pick + 2) == 0xFF) {
        B(at::kTitleB) = 0xFF;
    } else if ((static_cast<unsigned>(SH_CALL(Rand)()) & 1) != 0) {
        const unsigned i = PickOfCategory(pick + 2);
        const unsigned category = B(pick + 2);
        B(at::kTitleB) = static_cast<unsigned char>(B(at::kName + 10 * category + i) + B(at::kDrawPools + 2 * category));
    } else {
        const unsigned char a = SH_CALL(CommuDraw_RandBelow)(B(at::kDrawPools + 2 * B(pick + 2) + 1));
        B(at::kTitleB) = static_cast<unsigned char>(a + B(at::kDrawPools + 2 * B(pick + 2)));
    }
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    B(at::kCount) = 0;
    B(at::kStep) = step;
}

// original 0x45CC50, step 1: the title, then a semi-transparent POLY_G4 (abr 2,
// tpage (1, 2, 0x140, 0)) from black at y = 0x675F95 to white at y + 0x10,
// across x 0..320, and a black TILE from y + 0x10, 320 wide, 0xE0 - y high.
// 0x675F95 one more, three more while a button is held; the step on at 0xC8.
extern "C" void __cdecl CommuDraw_Reveal(void) {
    SH_CALL(CommuDraw_DrawTitle)();
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(1, 2, 0x140, 0) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(0, 0xC);
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutL(p + 8, 0);
    PutL(p + 0x18, 0x43A00000u);   // 320.0
    PutL(p + 0x28, 0);
    PutL(p + 0x38, 0x43A00000u);
    PutF(p + 0xC, static_cast<float>(static_cast<int>(B(at::kCount))));
    PutF(p + 0x1C, static_cast<float>(static_cast<int>(B(at::kCount))));
    PutF(p + 0x2C, static_cast<float>(static_cast<int>(B(at::kCount)) + 0x10));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0xFF;
    PutF(p + 0x3C, static_cast<float>(static_cast<int>(B(at::kCount)) + 0x10));
    p[0x25] = 0xFF;
    p[0x26] = 0xFF;
    p[0x34] = 0xFF;
    p[0x35] = 0xFF;
    p[0x36] = 0xFF;
    SH_CALL(Gfx_CommitPrim)(0, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    PutL(p + 8, 0);
    PutL(p + 0x14, 0x43A00000u);
    PutF(p + 0xC, static_cast<float>(static_cast<int>(B(at::kCount)) + 0x10));
    PutF(p + 0x18, static_cast<float>(0xE0 - static_cast<int>(B(at::kCount))));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    SH_CALL(Gfx_CommitPrim)(0, 0x1C);
    const auto count = static_cast<unsigned char>(B(at::kCount) + 1);
    B(at::kCount) = count;
    if (Input_Held != 0) B(at::kCount) = static_cast<unsigned char>(count + 2);
    if (B(at::kCount) >= 0xC8) B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x45CDC0, step 2: the title; on any press Transition_Start(0) and
// the step on.
extern "C" void __cdecl CommuDraw_WaitKey(void) {
    SH_CALL(CommuDraw_DrawTitle)();
    if (Input_Pressed == 0) return;
    SH_CALL(Transition_Start)(0);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x45CDF0, step 3: while the wait word is not 0 the title (a tail
// jump); then the music faded out over 10, the kept track played over 8,
// Draw_PassFlags 0x1F, Transition_Start(1), the step on.
extern "C" void __cdecl CommuDraw_MusicBack(void) {
    if (MoveScript_WaitWordDA != 0) {
        SH_CALL(CommuDraw_DrawTitle)();
        return;
    }
    SH_CALL(Music_FadeOutStop)(10);
    SH_CALL(Music_Play)(B(at::kKeptTrack), 8);
    Draw_PassFlags = 0x1F;
    SH_CALL(Transition_Start)(1);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x45CE40, step 4: once the wait word is 0, message 0xE2,
// Field_Request 2, the next state, step 0.
extern "C" void __cdecl CommuDraw_Close(void) {
    if (MoveScript_WaitWordDA != 0) return;
    SH_CALL(Msg_OpenScript)(0xE2);
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    Field_Request = 2;
    B(at::kState) = state;
    B(at::kStep) = 0;
}

// original 0x45CE80: Rand & 0x7F, less Rand & 0xF (bytes, wrapping) while not
// below `limit`'s byte. With a limit of 0 the original never returns; ours
// aborts (no caller passes 0: the counts of 0x652E34 are not 0).
extern "C" unsigned char __cdecl CommuDraw_RandBelow(unsigned limit) {
    const unsigned char bound = static_cast<unsigned char>(limit);
    if (bound == 0) bof3::Fatal("CommuDraw_RandBelow: a limit of 0; the original loops forever");
    unsigned char r = static_cast<unsigned char>(SH_CALL(Rand)()) & 0x7F;
    while (r >= bound) r = static_cast<unsigned char>(r - (static_cast<unsigned char>(SH_CALL(Rand)()) & 0xF));
    return r;
}

namespace {

// A message's bytes appended at the text scratch's `k` (a byte index): each
// byte, and a byte with bit 7 with the byte after it, to the message's 0.
unsigned char AppendMessage(unsigned char k, const unsigned char* p) {
    char* const buf = Scratch();
    while (*p != 0) {
        buf[k] = static_cast<char>(*p);
        const unsigned char c = *p;
        k = static_cast<unsigned char>(k + 1);
        if ((c & 0x80) != 0) {
            buf[k] = static_cast<char>(p[1]);
            ++p;
            k = static_cast<unsigned char>(k + 1);
        }
        ++p;
    }
    return k;
}

}  // namespace

// original 0x45CEA0: the title in the text scratch - '*', message 0x675FC5 of
// the draw's words, message 0x675FC6 unless 0xFF, '+', ' ', the five name bytes
// of the slot the record at 0x939A38 names (+5), a 0 - drawn at (0x32, 0x1E);
// then the row's line (MessagePools word 0x1C6 + 2 * s8 0x675F8C, read after
// the first draw) at (0x32, 0x42).
extern "C" void __cdecl CommuDraw_DrawTitle(void) {
    char* const buf = Scratch();
    buf[0] = 0x2A;
    unsigned char k = AppendMessage(1, PoolText(at::kDrawWords, B(at::kTitleA)));
    if (B(at::kTitleB) != 0xFF) k = AppendMessage(k, PoolText(at::kDrawWords, B(at::kTitleB)));
    const unsigned char* const record = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(L(at::kPointer)));
    buf[k] = 0x2B;
    buf[static_cast<unsigned char>(k + 1)] = 0x20;
    k = static_cast<unsigned char>(k + 2);
    for (unsigned i = 0; i < 5; ++i) {
        buf[k] = static_cast<char>(B(at::kSlotNames + 5u * record[5] + i));
        k = static_cast<unsigned char>(k + 1);
    }
    buf[k] = 0;
    SH_CALL(Text_DrawAt)(0x32, 0x1E, 0, 0xFF, reinterpret_cast<const unsigned char*>(buf));
    SH_CALL(Text_DrawAt)(0x32, 0x42, 0, 0xFF, PoolText(at::kTitleWords, S8(at::kCursor)));
}

// ===========================================================================
// CommuName: 0x652A70[8] (CommuName_States 0x652E48, by 0x939A3E)
// ===========================================================================

// original 0x45D040: CommuName_States[0x939A3E].
extern "C" void __cdecl CommuName_Dispatch(void) { Jump(at::kNameStates, 5, B(at::kState), "CommuName_Dispatch"); }

// original 0x45D050, state 0: unless a message is open, message 0xEE,
// Field_Request 2, state 1.
extern "C" void __cdecl CommuName_Begin(void) {
    if (Field_Request == 2) return;
    SH_CALL(Msg_OpenScript)(0xEE);
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    Field_Request = 2;
    B(at::kState) = state;
}

// original 0x45D080, state 1: CommuName_SlotSteps[0x939A40].
extern "C" void __cdecl CommuName_SlotStep(void) { Jump(at::kSlotSteps, 9, B(at::kStep), "CommuName_SlotStep"); }

// original 0x45D090, step 0 of states 1 and 2: unless a message is open, sound
// 0x102, the cursor 0, the slide 4, the header message 0xF0, the step on.
extern "C" void __cdecl CommuName_PanelReset(void) {
    if (Field_Request == 2) return;
    SH_CALL(Sound_PlayEffect)(0x102);
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    B(at::kCursor) = 0;
    B(at::kCount) = 4;
    PutW(at::kHeader, 0xF0);
    B(at::kStep) = step;
}

// original 0x45D0D0, state 1 step 1: the slide one less; the title box at the
// slide; the cursor slot's panel at 0x44 - 30 * slide; the slot bar at (0x4A,
// 0x64 - 30 * slide); the step on at slide 0.
extern "C" void __cdecl CommuName_SlotPanelIn(void) {
    const auto slide = static_cast<unsigned char>(B(at::kCount) - 1);
    B(at::kCount) = slide;
    PanelBox(slide);
    const unsigned slot = SH_CALL(CommuName_NthSlot)(B(at::kCursor));
    SlotPanel(0x44 - 30 * Slide(), 0x32, slot, 0);
    SH_CALL(CommuName_DrawSlotBar)(0x4A, 0x64 - 30 * Slide());
    if (B(at::kCount) == 0) B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x45D170, state 1 step 2: confirm - the header 0xF1, sound 0x104,
// the step on, the yes / no 0; cancel - sound 0x106, step 8; else the auto-
// repeated up / down (0x8000 / 0x2000, sound 0x100 on any) moves the cursor
// through the slots in use, wrapping. Then the header, the cursor slot's panel
// at 0x44, the slot bar at (0x4A, 0x64), the hand at 0x55 + 8 * cursor.
extern "C" void __cdecl CommuName_SlotChoose(void) {
    const unsigned pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0) {
        PutW(at::kHeader, 0xF1);
        SH_CALL(Sound_PlayEffect)(0x104);
        const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
        B(at::kYes) = 0;
        B(at::kStep) = step;
    } else if ((Field_CancelButtons & pressed) != 0) {
        SH_CALL(Sound_PlayEffect)(0x106);
        B(at::kStep) = 8;
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0xA000);
        if ((keys & 0xFFFF) != 0) SH_CALL(Sound_PlayEffect)(0x100);
        if ((keys & 0x8000) != 0) {
            const auto cursor = static_cast<unsigned char>(B(at::kCursor) - 1);
            B(at::kCursor) = cursor;
            if ((cursor & 0x80) != 0) {
                const unsigned char n = SH_CALL(CommuName_CountSlots)();
                B(at::kCursor) = static_cast<unsigned char>(n - 1);
            }
        } else if ((keys & 0x2000) != 0) {
            B(at::kCursor) = static_cast<unsigned char>(B(at::kCursor) + 1);
            const unsigned char n = SH_CALL(CommuName_CountSlots)();
            if (!(S8(at::kCursor) < static_cast<int>(n))) B(at::kCursor) = 0;
        }
    }
    SH_CALL(CommuName_DrawHeader)();
    CursorSlotPanel(0x44, 0);
    SH_CALL(CommuName_DrawSlotBar)(0x4A, 0x64);
    SlotHand(0x55 + 8 * S8(at::kCursor), 0x6D, 0);
}

namespace {

// The yes / no ask of both confirm steps (0x45D290, 0x45DD10): confirm on the
// second answer - the header 0xF0, sound 0x104, the step back; on the first -
// the header 0xFFFF (none), sounds 0x104 and 0x102, the step on, the slide 0;
// cancel - the header 0xF0, sound 0x106, the step back; else an auto-repeated
// up / down flips the answer (sound 0x100). Answers whether the header is
// still drawn, and draws it with the hand on the answer while it asks.
bool AskYesNo() {
    const unsigned pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0) {
        if (B(at::kYes) == 1) {
            PutW(at::kHeader, 0xF0);
            SH_CALL(Sound_PlayEffect)(0x104);
            B(at::kStep) = static_cast<unsigned char>(B(at::kStep) - 1);
        } else {
            PutW(at::kHeader, 0xFFFF);
            SH_CALL(Sound_PlayEffect)(0x104);
            SH_CALL(Sound_PlayEffect)(0x102);
            const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
            B(at::kCount) = 0;
            B(at::kStep) = step;
        }
    } else if ((Field_CancelButtons & pressed) != 0) {
        PutW(at::kHeader, 0xF0);
        SH_CALL(Sound_PlayEffect)(0x106);
        B(at::kStep) = static_cast<unsigned char>(B(at::kStep) - 1);
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0xA000);
        if ((keys & 0xFFFF) != 0) {
            B(at::kYes) = static_cast<unsigned char>(B(at::kYes) ^ 1);
            SH_CALL(Sound_PlayEffect)(0x100);
        }
    }
    if (W(at::kHeader) == 0xFFFF) return false;
    SH_CALL(CommuName_DrawHeader)();
    if (W(at::kHeader) == 0xF1) SH_CALL(Menu_DrawHand)(0xCE + 36 * S8(at::kYes), 0x15, 0);
    return true;
}

}  // namespace

// original 0x45D290, state 1 step 3: the ask (above); while the header shows,
// the slot bar at (0x4A, 0x64) and the hand (flag 1); the cursor slot's panel
// at 0x44 always.
extern "C" void __cdecl CommuName_SlotConfirm(void) {
    if (AskYesNo()) {
        SH_CALL(CommuName_DrawSlotBar)(0x4A, 0x64);
        SlotHand(0x55 + 8 * S8(at::kCursor), 0x6D, 1);
    }
    CursorSlotPanel(0x44, 0);
}

// original 0x45D3E0, state 1 step 4: the slide one more; the title box at it,
// the slot bar at (0x4A, 0x64 + 40 * slide), the cursor slot's panel at 0x44;
// at slide 4 message 0xF2, the step on, Field_Request 2.
extern "C" void __cdecl CommuName_SlotPanelOut(void) {
    const auto slide = static_cast<unsigned char>(B(at::kCount) + 1);
    B(at::kCount) = slide;
    PanelBox(slide);
    SH_CALL(CommuName_DrawSlotBar)(0x4A, 0x64 + 40 * Slide());
    CursorSlotPanel(0x44, 0);
    if (B(at::kCount) != 4) return;
    SH_CALL(Msg_OpenScript)(0xF2);
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    Field_Request = 2;
    B(at::kStep) = step;
}

// original 0x45D480, state 1 step 5: once the message is closed, step 6 when
// 0x939A3C is 0, else the slide 5 and step 7; the cursor slot's panel at 0x44.
extern "C" void __cdecl CommuName_SlotHow(void) {
    if (Field_Request != 2) {
        if (B(at::kHow) == 0) {
            B(at::kStep) = 6;
        } else {
            B(at::kCount) = 5;
            B(at::kStep) = 7;
        }
    }
    CursorSlotPanel(0x44, 0);
}

// original 0x45D4D0, state 1 step 6: CommuName_SlotRandomSteps[0x939A3F].
extern "C" void __cdecl CommuName_SlotRandomStep(void) {
    Jump(at::kSlotRandomSteps, 3, B(at::kStep2), "CommuName_SlotRandomStep");
}

namespace {

// A name drawn at random (R4E's 0x45ED70 writes it at 0x675F98 and answers its
// length + 1), copied into the first text record, message 0xF3, the step's
// step on, Field_Request 2.
void RandomNameAsk() {
    const U n = RandomName();
    std::memmove(At(at::kTextRecords), At(at::kName), n);
    SH_CALL(Msg_OpenScript)(0xF3);
    const auto step = static_cast<unsigned char>(B(at::kStep2) + 1);
    Field_Request = 2;
    B(at::kStep2) = step;
}

// The answer to message 0xF3, once it is closed: 0x939A3D not 0 - sound 0x102,
// the step's step on, the slide 0; else that step back to 0 (draw again).
void RandomAnswer() {
    if (Field_Request == 2) return;
    if (B(at::kAgain) != 0) {
        SH_CALL(Sound_PlayEffect)(0x102);
        const auto step = static_cast<unsigned char>(B(at::kStep2) + 1);
        B(at::kCount) = 0;
        B(at::kStep2) = step;
    } else {
        B(at::kStep2) = 0;
    }
}

// The answer to message 0xF4 / the entry, once it is closed: 0x939A3D 0 - the
// slide 0, the step's step on; else the slide 4, that step back to 0.
void EntryAnswer() {
    if (Field_Request == 2) return;
    if (B(at::kAgain) == 0) {
        B(at::kCount) = 0;
        B(at::kStep2) = static_cast<unsigned char>(B(at::kStep2) + 1);
    } else {
        B(at::kCount) = 4;
        B(at::kStep2) = 0;
    }
}

}  // namespace

// original 0x45D4E0, step 6.0: the random name and its message (above); the
// cursor slot's panel at 0x44.
extern "C" void __cdecl CommuName_SlotRandomPick(void) {
    RandomNameAsk();
    CursorSlotPanel(0x44, 0);
}

// original 0x45D550, step 6.1: the answer (above); the cursor slot's panel.
extern "C" void __cdecl CommuName_SlotRandomAsk(void) {
    RandomAnswer();
    CursorSlotPanel(0x44, 0);
}

// original 0x45D5B0, step 6.2: the slide one more, the cursor slot's panel at
// 0x44 - 30 * slide; at 4 R4E's 0x45F650 (the slot's name written, a tail jump).
extern "C" void __cdecl CommuName_SlotRandomOut(void) {
    B(at::kCount) = static_cast<unsigned char>(B(at::kCount) + 1);
    const unsigned slot = SH_CALL(CommuName_NthSlot)(B(at::kCursor));
    SlotPanel(0x44 - 30 * Slide(), 0x32, slot, 0);
    if (B(at::kCount) == 4) SlotRename();
}

// original 0x45D600, state 1 step 7: CommuName_SlotEntrySteps[0x939A3F].
extern "C" void __cdecl CommuName_SlotEntryStep(void) {
    Jump(at::kSlotEntrySteps, 5, B(at::kStep2), "CommuName_SlotEntryStep");
}

// original 0x45D610, step 7.0: the cursor slot's panel at 0x44; the entry's
// draws at 30 * slide; the slide one less; at 0 the slot's five name bytes
// (a 0 byte read as ' ') into 0x675F98, the step's step on, the entry's column
// and the three bytes 0.
extern "C" void __cdecl CommuName_SlotEntryIn(void) {
    CursorSlotPanel(0x44, 0);
    EntryDraws(30);
    const auto slide = static_cast<unsigned char>(B(at::kCount) - 1);
    B(at::kCount) = slide;
    if (slide != 0) return;
    for (unsigned i = 0; i < 5; ++i) {
        const unsigned slot = SH_CALL(CommuName_NthSlot)(B(at::kCursor)) & 0xFF;
        if (B(at::kSlotNames + 5 * slot + i) == 0) {
            B(at::kName + i) = 0x20;
        } else {
            const unsigned again = SH_CALL(CommuName_NthSlot)(B(at::kCursor)) & 0xFF;
            B(at::kName + i) = B(at::kSlotNames + 5 * again + i);
        }
    }
    const auto step = static_cast<unsigned char>(B(at::kStep2) + 1);
    B(at::kEntryColumn) = 0;
    B(at::kEntryC) = 0;
    B(at::kEntryA) = 0;
    B(at::kEntryB) = 0;
    B(at::kStep2) = step;
}

// original 0x45D730, step 7.1: the cursor slot's panel (flag 1), the column's
// grey line at (0x4A + 12 * column, 0x42), the entry's draws, 0x675F96 =
// BareRetZero (0: the PC's entry always ends unanswered).
extern "C" void __cdecl CommuName_SlotEntry(void) {
    CursorSlotPanel(0x44, 1);
    SH_CALL(Menu_DrawGreyHLine)(0x4A + 12 * static_cast<int>(B(at::kEntryColumn)), 0x42, 0xC, 0);
    EntryDraws(0);
    B(at::kEntryDone) = DroppedZero();
}

// original 0x45D7C0, step 7.2: the slide one more; the cursor slot's panel at
// 0x44 (answered) or 0x44 - 30 * slide; the entry's draws at 30 * slide. At
// slide 4: answered - an empty name takes the slot's default (0x653200, 20 a
// slot, read in place), message 0xF4, the step's step on; unanswered - message
// 0xF7, state 4, step 0. Field_Request 2 either way.
extern "C" void __cdecl CommuName_SlotEntryOut(void) {
    const unsigned char done = B(at::kEntryDone);
    B(at::kCount) = static_cast<unsigned char>(B(at::kCount) + 1);
    if (done != 0) {
        CursorSlotPanel(0x44, 1);
    } else {
        const unsigned slot = SH_CALL(CommuName_NthSlot)(B(at::kCursor));
        SlotPanel(0x44 - 30 * Slide(), 0x32, slot, 1);
    }
    EntryDraws(30);
    if (B(at::kCount) != 4) return;
    if (B(at::kEntryDone) != 0) {
        if (B(at::kName) == 0) {
            for (unsigned i = 0; i < 5; ++i) {
                const unsigned slot = SH_CALL(CommuName_NthSlot)(B(at::kCursor)) & 0xFF;
                B(at::kName + i) = B(at::kSlotDefaults + 20 * slot + i);
            }
        }
        SH_CALL(Msg_OpenScript)(0xF4);
        const auto step = static_cast<unsigned char>(B(at::kStep2) + 1);
        Field_Request = 2;
        B(at::kStep2) = step;
    } else {
        Say(0xF7);
        B(at::kState) = 4;
        B(at::kStep) = 0;
    }
}

// original 0x45D930, step 7.3: the cursor slot's panel (flag 1); the answer.
extern "C" void __cdecl CommuName_SlotEntryAsk(void) {
    CursorSlotPanel(0x44, 1);
    EntryAnswer();
}

// original 0x45D990, step 7.4: as 6.2 with flag 1.
extern "C" void __cdecl CommuName_SlotEntryDone(void) {
    B(at::kCount) = static_cast<unsigned char>(B(at::kCount) + 1);
    const unsigned slot = SH_CALL(CommuName_NthSlot)(B(at::kCursor));
    SlotPanel(0x44 - 30 * Slide(), 0x32, slot, 1);
    if (B(at::kCount) == 4) SlotRename();
}

// original 0x45D9E0, state 1 step 8: the slide one more; the title box, the
// cursor slot's panel at 0x20 - 30 * slide, the slot bar at (0x4A, 0x64 - 30 *
// slide); at 4 message 0xF7, state 4, step 0.
extern "C" void __cdecl CommuName_SlotClose(void) {
    const auto slide = static_cast<unsigned char>(B(at::kCount) + 1);
    B(at::kCount) = slide;
    PanelBox(slide);
    const unsigned slot = SH_CALL(CommuName_NthSlot)(B(at::kCursor));
    SlotPanel(0x20 - 30 * Slide(), 0x32, slot, 0);
    SH_CALL(CommuName_DrawSlotBar)(0x4A, 0x64 - 30 * Slide());
    if (B(at::kCount) != 4) return;
    Say(0xF7);
    B(at::kState) = 4;
    B(at::kStep) = 0;
}

// original 0x45DAA0, state 2: CommuName_MemberSteps[0x939A40].
extern "C" void __cdecl CommuName_MemberStep(void) { Jump(at::kMemberSteps, 9, B(at::kStep), "CommuName_MemberStep"); }

namespace {

// The members' hand on the cursor: (0x20 + 136 * (cursor & 1), 0x2B + 56 *
// (cursor >> 1, signed)).
void MemberCursorHand(unsigned flag) {
    MemberPanels();
    const auto cursor = static_cast<signed char>(B(at::kCursor));
    const int column = cursor & 1;
    const int row = cursor >> 1;
    MemberHand(0x20 + 136 * column, 0x2B + 56 * row, flag, 6);
}

}  // namespace

// original 0x45DAB0, state 2 step 1: the slide one less; the title box; the
// grid at x 0x20 - 40 * slide and 0xA8 + 40 * slide; the step on at 0.
extern "C" void __cdecl CommuName_MemberPanelIn(void) {
    const auto slide = static_cast<unsigned char>(B(at::kCount) - 1);
    B(at::kCount) = slide;
    PanelBox(slide);
    unsigned char n = 0;
    unsigned record = 0;
    for (U flags = at::kRecordFlags; flags < at::kRecordFlagsEnd; flags += at::kRecordStride, ++record) {
        if ((B(flags) & 1) == 0) continue;
        const int y = 0x2B + 56 * (n >> 1);
        const int x = (n & 1) == 0 ? 0x20 - 40 * Slide() : 0xA8 + 40 * Slide();
        MemberPanel(x, y, record, 0);
        n = static_cast<unsigned char>(n + 1);
    }
    if (B(at::kCount) == 0) B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x45DB90, state 2 step 2: confirm - the header 0xF1, sound 0x104,
// the step on, the yes / no 0; cancel - sounds 0x106 and 0x102, the slide 0,
// step 8; else the auto-repeated keys (0xF000; sound 0x100 on any): left /
// right (0xA000) flips the column unless that passes the last record, up
// (0x1000) two back (from the top: the last row's), down (0x4000) two on
// (past the last: the top of the column). Then the members' panels, the hand
// (flag 1), the header.
extern "C" void __cdecl CommuName_MemberChoose(void) {
    const unsigned pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0) {
        PutW(at::kHeader, 0xF1);
        SH_CALL(Sound_PlayEffect)(0x104);
        const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
        B(at::kYes) = 0;
        B(at::kStep) = step;
    } else if ((Field_CancelButtons & pressed) != 0) {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(Sound_PlayEffect)(0x102);
        B(at::kCount) = 0;
        B(at::kStep) = 8;
    } else {
        const auto last = static_cast<unsigned char>(MemberCount() - 1);
        const unsigned keys = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF000);
        if ((keys & 0xFFFF) != 0) SH_CALL(Sound_PlayEffect)(0x100);
        if ((keys & 0xA000) != 0) {
            const auto cursor = static_cast<unsigned char>(B(at::kCursor) ^ 1);
            B(at::kCursor) = cursor;
            if (static_cast<signed char>(cursor) > static_cast<int>(last)) B(at::kCursor) = static_cast<unsigned char>(cursor ^ 1);
        } else if ((keys & 0x1000) != 0) {
            const auto cursor = static_cast<unsigned char>(B(at::kCursor) + 0xFE);
            B(at::kCursor) = cursor;
            if ((cursor & 0x80) != 0) B(at::kCursor) = static_cast<unsigned char>((cursor | 0xFE) & last);
        } else if ((keys & 0x4000) != 0) {
            const auto cursor = static_cast<unsigned char>(B(at::kCursor) + 2);
            B(at::kCursor) = cursor;
            const int now = static_cast<signed char>(cursor);
            const int bound = static_cast<unsigned char>(cursor | 0xFE) & last;
            if (now > bound) B(at::kCursor) = static_cast<unsigned char>(B(at::kCursor) & 1);
        }
    }
    MemberCursorHand(1);
    SH_CALL(CommuName_DrawHeader)();
}

// original 0x45DD10, state 2 step 3: the ask (as the slot's), then the members'
// panels and the hand (flag 0) always.
extern "C" void __cdecl CommuName_MemberConfirm(void) {
    AskYesNo();
    MemberCursorHand(0);
}

// original 0x45DE60, state 2 step 4: the slide one more; the title box; the
// grid as the panel's slide in, but the cursor's member (the n-th shown equal to
// s8 0x675F8C, read again each record) moved toward (0x20, 0x2B):
// x = 0x20 + (34 * column) * (4 - slide), y = 0x2B + ((14 * row) & 0xFF) *
// (4 - slide); at 4 message 0xF2, the step on, Field_Request 2.
extern "C" void __cdecl CommuName_MemberPanelOut(void) {
    const auto slide = static_cast<unsigned char>(B(at::kCount) + 1);
    B(at::kCount) = slide;
    PanelBox(slide);
    unsigned char n = 0;
    unsigned record = 0;
    for (U flags = at::kRecordFlags; flags < at::kRecordFlagsEnd; flags += at::kRecordStride, ++record) {
        if ((B(flags) & 1) == 0) continue;
        if (static_cast<int>(n) != S8(at::kCursor)) {
            const int y = 0x2B + 56 * (n >> 1);
            const int x = (n & 1) == 0 ? 0x20 - 40 * Slide() : 0xA8 + 40 * Slide();
            MemberPanel(x, y, record, 0);
        } else {
            const int left = 4 - Slide();
            const int y = static_cast<int>(static_cast<unsigned char>(14 * (n >> 1))) * left + 0x2B;
            const int x = static_cast<int>(static_cast<unsigned char>(34 * (n & 1))) * left + 0x20;
            MemberPanel(x, y, record, 0);
        }
        n = static_cast<unsigned char>(n + 1);
    }
    if (B(at::kCount) != 4) return;
    SH_CALL(Msg_OpenScript)(0xF2);
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    Field_Request = 2;
    B(at::kStep) = step;
}

// original 0x45E000, state 2 step 5: as the slot's (the slide 4 for step 7);
// the cursor member's panel at 0x20.
extern "C" void __cdecl CommuName_MemberHow(void) {
    if (Field_Request != 2) {
        if (B(at::kHow) == 0) {
            B(at::kStep) = 6;
        } else {
            B(at::kCount) = 4;
            B(at::kStep) = 7;
        }
    }
    CursorMemberPanel(0x20, 0);
}

// original 0x45E050, state 2 step 6: CommuName_MemberRandomSteps[0x939A3F].
extern "C" void __cdecl CommuName_MemberRandomStep(void) {
    Jump(at::kMemberRandomSteps, 3, B(at::kStep2), "CommuName_MemberRandomStep");
}

// original 0x45E060, step 6.0: the random name and its message; the cursor
// member's panel at 0x20.
extern "C" void __cdecl CommuName_MemberRandomPick(void) {
    RandomNameAsk();
    CursorMemberPanel(0x20, 0);
}

// original 0x45E0D0, step 6.1: the answer; the cursor member's panel.
extern "C" void __cdecl CommuName_MemberRandomAsk(void) {
    RandomAnswer();
    CursorMemberPanel(0x20, 0);
}

// original 0x45E130, step 6.2: the slide one more, the cursor member's panel
// at 0x20 - 40 * slide; at 4 R4E's 0x45F5A0 (the record's name written, a tail
// jump).
extern "C" void __cdecl CommuName_MemberRandomOut(void) {
    B(at::kCount) = static_cast<unsigned char>(B(at::kCount) + 1);
    const U record = NthMember(B(at::kCursor));
    MemberPanel(0x20 - 40 * Slide(), 0x2B, record, 0);
    if (B(at::kCount) == 4) MemberRename();
}

// original 0x45E180, state 2 step 7: CommuName_MemberEntrySteps[0x939A3F].
extern "C" void __cdecl CommuName_MemberEntryStep(void) {
    Jump(at::kMemberEntrySteps, 5, B(at::kStep2), "CommuName_MemberEntryStep");
}

// original 0x45E190, step 7.0: the slide one less; the cursor member's panel
// at 0x20; the entry's draws at 30 * slide; at 0 the record's first five bytes
// (0 read as ' ') into 0x675F98, the step's step on, the column and the three
// bytes 0.
extern "C" void __cdecl CommuName_MemberEntryIn(void) {
    B(at::kCount) = static_cast<unsigned char>(B(at::kCount) - 1);
    CursorMemberPanel(0x20, 0);
    EntryDraws(30);
    if (B(at::kCount) != 0) return;
    for (unsigned i = 0; i < 5; ++i) {
        const unsigned record = NthMember(B(at::kCursor)) & 0xFF;
        if (B(at::kRecords + at::kRecordStride * record + i) == 0) {
            B(at::kName + i) = 0x20;
        } else {
            const unsigned again = NthMember(B(at::kCursor)) & 0xFF;
            B(at::kName + i) = B(at::kRecords + at::kRecordStride * again + i);
        }
    }
    const auto step = static_cast<unsigned char>(B(at::kStep2) + 1);
    B(at::kEntryColumn) = 0;
    B(at::kEntryC) = 0;
    B(at::kEntryA) = 0;
    B(at::kEntryB) = 0;
    B(at::kStep2) = step;
}

// original 0x45E2C0, step 7.1: the cursor member's panel (flag 1), the
// column's grey line at (0x5B + 12 * column, 0x50), the entry's draws,
// 0x675F96 = BareRetZero.
extern "C" void __cdecl CommuName_MemberEntry(void) {
    CursorMemberPanel(0x20, 1);
    SH_CALL(Menu_DrawGreyHLine)(0x5B + 12 * static_cast<int>(B(at::kEntryColumn)), 0x50, 0xC, 0);
    EntryDraws(0);
    B(at::kEntryDone) = DroppedZero();
}

// original 0x45E350, step 7.2: as the slot's, the panel at 0x20 (answered) or
// 0x20 - 30 * slide, an empty name taking the record's name from
// CommuName_RecordNames (seven; ours aborts past them, section 5).
extern "C" void __cdecl CommuName_MemberEntryOut(void) {
    const unsigned char done = B(at::kEntryDone);
    B(at::kCount) = static_cast<unsigned char>(B(at::kCount) + 1);
    if (done != 0) {
        CursorMemberPanel(0x20, 1);
    } else {
        const U record = NthMember(B(at::kCursor));
        MemberPanel(0x20 - 30 * Slide(), 0x2B, record, 1);
    }
    EntryDraws(30);
    if (B(at::kCount) != 4) return;
    if (B(at::kEntryDone) != 0) {
        if (B(at::kName) == 0) {
            for (unsigned i = 0; i < 5; ++i) {
                const unsigned record = NthMember(B(at::kCursor)) & 0xFF;
                const U name = RecordName(record);
                B(at::kName + i) = *At(name + i);
            }
        }
        SH_CALL(Msg_OpenScript)(0xF4);
        const auto step = static_cast<unsigned char>(B(at::kStep2) + 1);
        Field_Request = 2;
        B(at::kStep2) = step;
    } else {
        Say(0xF7);
        B(at::kState) = 4;
        B(at::kStep) = 0;
    }
}

// original 0x45E4C0, step 7.3: the cursor member's panel (flag 1); the answer.
extern "C" void __cdecl CommuName_MemberEntryAsk(void) {
    CursorMemberPanel(0x20, 1);
    EntryAnswer();
}

// original 0x45E520, step 7.4: the slide one more, the cursor member's panel
// at 0x20 - 30 * slide (flag 1); at 4 R4E's 0x45F5A0.
extern "C" void __cdecl CommuName_MemberEntryDone(void) {
    B(at::kCount) = static_cast<unsigned char>(B(at::kCount) + 1);
    const U record = NthMember(B(at::kCursor));
    MemberPanel(0x20 - 30 * Slide(), 0x2B, record, 1);
    if (B(at::kCount) == 4) MemberRename();
}

// original 0x45E570, state 2 step 8: the slide one more; the title box; the
// grid at x 0x18 - 40 * slide and 0xA8 + 40 * slide, y 0x30 + 56 * row; at 4
// message 0xF7, state 4, step 0.
extern "C" void __cdecl CommuName_MemberClose(void) {
    const auto slide = static_cast<unsigned char>(B(at::kCount) + 1);
    B(at::kCount) = slide;
    PanelBox(slide);
    unsigned char n = 0;
    unsigned record = 0;
    for (U flags = at::kRecordFlags; flags < at::kRecordFlagsEnd; flags += at::kRecordStride, ++record) {
        if ((B(flags) & 1) == 0) continue;
        const int y = 0x30 + 56 * (n >> 1);
        const int x = (n & 1) == 0 ? 0x18 - 40 * Slide() : 0xA8 + 40 * Slide();
        MemberPanel(x, y, record, 0);
        n = static_cast<unsigned char>(n + 1);
    }
    if (B(at::kCount) != 4) return;
    Say(0xF7);
    B(at::kState) = 4;
    B(at::kStep) = 0;
}

// original 0x45E670, state 3: once the message is closed and at step 0,
// message 0xF6, Field_Request 2.
extern "C" void __cdecl CommuName_End(void) {
    if (Field_Request == 2 || B(at::kStep) != 0) return;
    Say(0xF6);
}

// original 0x45E6A0, the last state of CommuDraw_States, CommuName_States and
// two tables of other groups: once the message is closed, the community's game
// byte 0x9039F4 one on.
extern "C" void __cdecl Commu_LeaveWhenClosed(void) {
    if (Field_Request == 2) return;
    B(at::kGame) = static_cast<unsigned char>(B(at::kGame) + 1);
}

// original 0x45E6B0: the 8-byte cells 0x9046D0..0x9048AF whose first byte is
// not 0, counted in al (the original clears al alone; callers read al).
extern "C" unsigned char __cdecl CommuName_CountSlots(void) {
    unsigned char n = 0;
    for (U cell = at::kSlots; cell < at::kSlotsEnd; cell += 8)
        if (B(cell) != 0) n = static_cast<unsigned char>(n + 1);
    return n;
}

// original 0x45E6D0: the index (0..59, in eax) of the cell in use whose count
// before it is `n`'s byte; 0xFF when there is none.
extern "C" unsigned __cdecl CommuName_NthSlot(unsigned n) {
    unsigned char seen = 0;
    unsigned index = 0;
    for (U cell = at::kSlots; cell < at::kSlotsEnd; cell += 8, ++index) {
        if (B(cell) == 0) continue;
        if (seen == static_cast<unsigned char>(n)) return index;
        seen = static_cast<unsigned char>(seen + 1);
    }
    return 0xFF;
}

// original 0x45E700: the slot bar - a box (x, y) 0xAC by 0x1A (flags 0x80, the
// window colour), its frame, and R4E's piece 1 at (x + 8i + 7, y + 9) for each
// cell in use.
extern "C" void __cdecl CommuName_DrawSlotBar(int x, int y) {
    SH_CALL(Menu_DrawBox)(x, y, 0xAC, 0x1A, 0x80, B(at::kColour));
    SH_CALL(CommuName_DrawSlotFrame)(x, y);
    const unsigned char n = SH_CALL(CommuName_CountSlots)();
    for (int i = 0; i < static_cast<int>(n); ++i) ListPiece(x + i * 8 + 7, y + 9, 1);
}

// original 0x45E770: the bar's frame in R4E's pieces: 6 at (x, y); twenty of 7
// along y and of 0xC along y + 0x16; 8 at (x + 0xA8, y); two rows of 9 / 0xA
// at y + 8 and y + 0xE; 0xB and 0xD at y + 0x16.
extern "C" void __cdecl CommuName_DrawSlotFrame(int x, int y) {
    ListPiece(x, y, 6);
    const int bottom = y + 0x16;
    for (int i = 0; i < 20; ++i) {
        const int xi = x + i * 8 + 8;
        ListPiece(xi, y, 7);
        ListPiece(xi, bottom, 0xC);
    }
    const int right = x + 0xA8;
    ListPiece(right, y, 8);
    for (int i = 0; i < 2; ++i) {
        const int yi = y + i * 6 + 8;
        ListPiece(x, yi, 9);
        ListPiece(right, yi, 0xA);
    }
    ListPiece(x, bottom, 0xB);
    ListPiece(right, bottom, 0xD);
}

// original 0x45E820: the header - the title box at (0x14, 0x10), and unless
// 0x675F92 is 0xFFFF (read after the box) the message it names at (0x1B, 0x13).
extern "C" void __cdecl CommuName_DrawHeader(void) {
    SH_CALL(Menu_DrawPanelBox)(0x14, 0x10, 0x118, 0x13, B(at::kColour));
    const unsigned id = W(at::kHeader);
    if (id == 0xFFFF) return;
    SH_CALL(Text_DrawAt)(0x1B, 0x13, 0, 0xFF, PoolText(at::kPoolWords, static_cast<int>(id)));
}

#pragma clang attribute pop

void Rest4D_Inject() {
    if (bof3::WantsShadow("rest_4d")) rest_4d::SelfTest();
    BOF3_INJECT(CommuBoard_DrawRows);
    BOF3_INJECT(CommuBoard_DrawFrame);
    BOF3_INJECT(CommuBoard_DrawRowCells);
    BOF3_INJECT(CommuBoard_DrawCells);
    BOF3_INJECT(CommuDraw_Dispatch);
    BOF3_INJECT(CommuDraw_OpenStep);
    BOF3_INJECT(CommuDraw_FadeOut);
    BOF3_INJECT(CommuDraw_MusicIn);
    BOF3_INJECT(CommuDraw_ShowStep);
    BOF3_INJECT(CommuDraw_Pick);
    BOF3_INJECT(CommuDraw_Reveal);
    BOF3_INJECT(CommuDraw_WaitKey);
    BOF3_INJECT(CommuDraw_MusicBack);
    BOF3_INJECT(CommuDraw_Close);
    BOF3_INJECT(CommuDraw_RandBelow);
    BOF3_INJECT(CommuDraw_DrawTitle);
    BOF3_INJECT(CommuName_Dispatch);
    BOF3_INJECT(CommuName_Begin);
    BOF3_INJECT(CommuName_SlotStep);
    BOF3_INJECT(CommuName_PanelReset);
    BOF3_INJECT(CommuName_SlotPanelIn);
    BOF3_INJECT(CommuName_SlotChoose);
    BOF3_INJECT(CommuName_SlotConfirm);
    BOF3_INJECT(CommuName_SlotPanelOut);
    BOF3_INJECT(CommuName_SlotHow);
    BOF3_INJECT(CommuName_SlotRandomStep);
    BOF3_INJECT(CommuName_SlotRandomPick);
    BOF3_INJECT(CommuName_SlotRandomAsk);
    BOF3_INJECT(CommuName_SlotRandomOut);
    BOF3_INJECT(CommuName_SlotEntryStep);
    BOF3_INJECT(CommuName_SlotEntryIn);
    BOF3_INJECT(CommuName_SlotEntry);
    BOF3_INJECT(CommuName_SlotEntryOut);
    BOF3_INJECT(CommuName_SlotEntryAsk);
    BOF3_INJECT(CommuName_SlotEntryDone);
    BOF3_INJECT(CommuName_SlotClose);
    BOF3_INJECT(CommuName_MemberStep);
    BOF3_INJECT(CommuName_MemberPanelIn);
    BOF3_INJECT(CommuName_MemberChoose);
    BOF3_INJECT(CommuName_MemberConfirm);
    BOF3_INJECT(CommuName_MemberPanelOut);
    BOF3_INJECT(CommuName_MemberHow);
    BOF3_INJECT(CommuName_MemberRandomStep);
    BOF3_INJECT(CommuName_MemberRandomPick);
    BOF3_INJECT(CommuName_MemberRandomAsk);
    BOF3_INJECT(CommuName_MemberRandomOut);
    BOF3_INJECT(CommuName_MemberEntryStep);
    BOF3_INJECT(CommuName_MemberEntryIn);
    BOF3_INJECT(CommuName_MemberEntry);
    BOF3_INJECT(CommuName_MemberEntryOut);
    BOF3_INJECT(CommuName_MemberEntryAsk);
    BOF3_INJECT(CommuName_MemberEntryDone);
    BOF3_INJECT(CommuName_MemberClose);
    BOF3_INJECT(CommuName_End);
    BOF3_INJECT(Commu_LeaveWhenClosed);
    BOF3_INJECT(CommuName_CountSlots);
    BOF3_INJECT(CommuName_NthSlot);
    BOF3_INJECT(CommuName_DrawSlotBar);
    BOF3_INJECT(CommuName_DrawSlotFrame);
    BOF3_INJECT(CommuName_DrawHeader);
}
