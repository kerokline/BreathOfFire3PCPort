// Moved here by tools/move_funcs.py from rest_4d.cpp (docs/source-layout-round.md).
#include "game/community/commu_name.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/menu/name_grid.h"
#include "game/move_script_bytes.h"
#include "game/rest_4d.h"
#include "game/rest_4d_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::Word;

unsigned char& B(U address) { return *At(address); }

int S8(U address) { return static_cast<signed char>(*At(address)); }

U L(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

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
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * index)))();
}

void SlotPanel(int x, int y, unsigned slot, unsigned flag) {
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kSlotPanel.v)(x, y, slot, flag);
}

void SlotHand(int x, int y, unsigned flag) { SH_AT(void (__cdecl*)(int, int, unsigned), at::kSlotHand.v)(x, y, flag); }

U RandomName() { return SH_AT(U (__cdecl*)(), at::kRandomName.v)(); }

void MemberPanel(int x, int y, unsigned record, unsigned flag) {
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kMemberPanel.v)(x, y, record, flag);
}

void MemberPanels() { SH_AT(void (__cdecl*)(), at::kMemberPanels.v)(); }

unsigned char MemberCount() { return SH_AT(unsigned char (__cdecl*)(), at::kMemberCount.v)(); }

U NthMember(unsigned n) { return SH_AT(U (__cdecl*)(unsigned), at::kNthMember.v)(n); }

void MemberHand(int x, int y, unsigned flag, unsigned six) {
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kMemberHand.v)(x, y, flag, six);
}

void EntryBox(int a, int y, unsigned c, unsigned d) { SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kEntryBox.v)(a, y, c, d); }

void MemberRename() { SH_AT(void (__cdecl*)(), at::kMemberRename.v)(); }

void SlotRename() { SH_AT(void (__cdecl*)(), at::kSlotRename.v)(); }

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

char* Scratch() { return reinterpret_cast<char*>(At(::bof3::Img(at::kTextScratch.v))); }

// A message's text: MessagePools' word at `words + 2 * index` from 0x803580.
const unsigned char* PoolText(U words, int index) {
    return Text(::bof3::Img(at::kPoolWords.v) + Word(At(words + 2u * static_cast<U>(index))));
}

// The panel's slide (0x675F95) as the offsets the states build.
int Slide() { return Rest4d_Count; }

// The title box both games open with: (0x14, 0x10 - 8 * slide), 0x118 by 0x13.
void PanelBox(unsigned char slide) {
    SH_CALL(Menu_DrawPanelBox)(0x14, 0x10 - 8 * static_cast<int>(slide), 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour));
}

// The entry's three draws (BareRet, the box, BareRet), each `per` * the slide
// (read again before each) further down. With the grid (CommuNameGrid), the
// two BareRet are the PlayStation's: the legend (page, 0xB0 + per * slide,
// 0x2B) and the grid (page, 0x18, 0x67 + per * slide, column, row), COMMU02's
// copy.
void EntryDraws(int per, bool grid = false) {
    const int first = 0xB0 + per * Slide();
    if (grid) {
        const name_grid::Cells c = commu_name::GridDrawnCells(commu_name::LiveEntryBytes());
        commu_name::g_api->legend(name_grid::Copy::kRenamer, c.page, first, 0x2B);
    } else {
        Dropped(static_cast<std::uint32_t>(Rest4d_EntryB), static_cast<U>(first), 0x2Bu);
    }
    EntryBox(0x12, 0x61 + per * Slide(), 0x24, 0x11);
    const unsigned char a = Rest4d_EntryA, c = Rest4d_EntryC;
    const int third = 0x67 + per * Slide();
    const unsigned char b = Rest4d_EntryB;
    if (grid) {
        commu_name::g_api->draw(name_grid::Copy::kRenamer, commu_name::GridDrawnCells(commu_name::LiveEntryBytes()), 0x18, third);
        return;
    }
    Dropped(static_cast<U>(b), 0x18u, static_cast<U>(third), static_cast<U>(c), static_cast<U>(a));
}

// A panel that draws the buffer as text (flag 1), drawn while the grid's
// cells are in the buffer: the layer's bytes of the name in their place for
// the draw, the cells back after it.
template <class F> void WithShownName(F draw) {
    const commu_name::EntryBytes e = commu_name::LiveEntryBytes();
    unsigned char kept[commu_name::kBufferBytes], shown[commu_name::kBufferBytes];
    std::memcpy(kept, e.buffer, sizeof kept);
    commu_name::GridShownName(e, shown);
    std::memcpy(e.buffer, shown, sizeof shown);
    draw();
    std::memcpy(e.buffer, kept, sizeof kept);
}

// The slot's panel at (x, 0x32) with the cursor's slot.
void CursorSlotPanel(int x, unsigned flag) {
    const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count));
    SlotPanel(x, 0x32, slot, flag);
}

// The member's panel at (x, 0x2B) with the cursor's record.
void CursorMemberPanel(int x, unsigned flag) {
    const U record = NthMember(static_cast<unsigned char>(Rest4c_Count));
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
                    index, at::kRecordNameCount, static_cast<unsigned>(CommuName_RecordNames_addr),
                    static_cast<unsigned>(CommuName_RecordNames_addr + 4 * index));
    return L(::bof3::Img(at::kRecordNames.v) + 4 * index);
}

}  // namespace

// --- the renamer's grid (docs/new-game-round.md, stage B, group CR) -----------------
//
// The PlayStation's entry (COMMU02) calls three functions the port cut: the
// legend and the grid drawn around the frame, and the input step, whose
// answer the entry's middle step keeps and which moves that step on itself
// (docs/name-entry-restoration.md sections 3 and 4). Ours are NG's
// (game/menu/name_grid.h) over the PC's own cells. What differs from the
// PlayStation's caller is the bytes: its buffer holds the disc's codes, which
// are its text; the PC's names are the layer's text, so the cells are decoded
// from the stored name at entry-in and encoded again when the entry is
// answered, and the panels, which draw the buffer as text, are handed the
// encoded name while the cells are in it.

namespace commu_name {

unsigned char g_grid = 0;

const GridApi kLiveGridApi{name_grid::NameGrid_Available, name_grid::NameGrid_Family, name_grid::NameGrid_CellCount,
                           name_grid::NameGrid_FirstPage, name_grid::NameGrid_Draw,   name_grid::NameGrid_DrawLegend,
                           name_grid::NameGrid_Input,     name_grid::NameGrid_Encode, name_grid::NameGrid_Decode};
const GridApi* g_api = &kLiveGridApi;

EntryBytes LiveEntryBytes() {
    return {At(::bof3::Img(rest_4d::at::kName.v)), &Rest4d_EntryColumn, &Rest4d_EntryC, &Rest4d_EntryA, &Rest4d_EntryB,
            &Rest4d_EntryDone, &Rest4c_Step};
}

bool GridOn(bool slot) {
    if (g_grid == 0 || !g_api->available()) return false;
    return !(slot && g_api->family() == name_grid::Family::kJapan);   // call 6: DIV-0075's end for a slot
}

namespace {

// The bytes `cells` encode to (the routine's encode into room to spare: five
// cells are ten bytes at most).
unsigned EncodedBytes(const unsigned char cells[5]) {
    unsigned char scratch[32];
    return g_api->encode(cells, scratch, sizeof scratch);
}

// The decoded cells of a run of stored units (no one-byte 0x20 in it) appended
// at `cells[n]`; false when the grid cannot decode it or it passes five cells.
bool AppendDecoded(const unsigned char* run, unsigned bytes, unsigned char cells[5], unsigned& n) {
    if (bytes == 0) return true;
    unsigned char out[5];
    if (!g_api->decode(run, bytes, out)) return false;
    unsigned k = 5;
    while (k > 0 && out[k - 1] == 0xFF) --k;   // the decode's padding (a trailing space inside a run is a blank too)
    if (n + k > 5) return false;
    for (unsigned i = 0; i < k; ++i) cells[n++] = out[i];
    return true;
}

}  // namespace

void StoredToCells(const unsigned char* stored, unsigned room, unsigned char out[5]) {
    if (room > kBufferBytes) bof3::Fatal("commu_name: a stored name of %u bytes (the fields are 5 and 8)", room);
    // The units to the NUL or the field's end: a byte with bit 7 and the one after it, or one byte.
    unsigned start[kBufferBytes], len[kBufferBytes], units = 0;
    for (unsigned i = 0; i < room && stored[i] != 0;) {
        const unsigned n = (stored[i] & 0x80) != 0 && i + 1 < room ? 2u : 1u;
        start[units] = i;
        len[units++] = n;
        i += n;
    }
    const auto blank = [&](unsigned u) { return len[u] == 1 && stored[start[u]] == 0x20; };
    while (units > 0 && blank(units - 1)) --units;   // the PC's padding
    unsigned char cells[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    unsigned n = 0;
    bool ok = true;
    unsigned char run[kBufferBytes];
    unsigned run_bytes = 0;
    for (unsigned u = 0; u < units && ok; ++u) {
        if (blank(u)) {
            ok = AppendDecoded(run, run_bytes, cells, n) && n < 5;
            run_bytes = 0;
            if (ok) cells[n++] = 0xFF;   // the PC's blank byte, the routine's blank
        } else {
            for (unsigned b = 0; b < len[u]; ++b) run[run_bytes++] = stored[start[u] + b];
        }
    }
    if (ok) ok = AppendDecoded(run, run_bytes, cells, n);
    if (!ok) {
        bof3::Log("commu_name: the stored name is not on the grid (%u bytes); the entry opens blank", room);
        std::memset(cells, 0xFF, 5);
    }
    // The family's cells (call 5: four under the Japan family), then the field's bytes.
    for (unsigned i = g_api->cell_count(); i < 5; ++i) cells[i] = 0xFF;
    for (int i = 4; i >= 0 && EncodedBytes(cells) > room; --i) cells[i] = 0xFF;
    std::memcpy(out, cells, 5);
}

void GridEntryIn(const EntryBytes& e, const unsigned char* stored, unsigned room) {
    StoredToCells(stored, room, e.buffer);
    *e.cursor = 0;
    *e.column = 0;
    *e.row = 0;
    *e.page = static_cast<unsigned char>(g_api->first_page());
    *e.step = static_cast<unsigned char>(*e.step + 1);
}

namespace {

name_grid::Cells CellsOf(const EntryBytes& e) {
    name_grid::Cells c{};
    std::memcpy(c.buffer, e.buffer, 6);
    c.cursor = *e.cursor;
    c.column = *e.column;
    c.row = *e.row;
    c.page = *e.page;
    return c;
}

}  // namespace

void GridEntryInput(const EntryBytes& e, unsigned room) {
    name_grid::Cells c = CellsOf(e);
    const unsigned result = g_api->input(name_grid::Copy::kRenamer, c, room, nullptr);
    std::memcpy(e.buffer, c.buffer, 5);   // COMMU02's routine leaves buffer byte 5 alone
    *e.cursor = c.cursor;
    *e.column = c.column;
    *e.row = c.row;
    *e.page = c.page;
    *e.done = (result & name_grid::kFinished) != 0 ? 1 : 0;
    if ((result & (name_grid::kFinished | name_grid::kQuit)) != 0) *e.step = static_cast<unsigned char>(*e.step + 1);
}

unsigned GridEntryAnswered(const EntryBytes& e, unsigned room, const unsigned char* fallback, unsigned fallback_room) {
    unsigned n = 0;
    if (e.buffer[0] == 0) {
        while (n < fallback_room && n < kBufferBytes - 1 && fallback[n] != 0) {
            e.buffer[n] = fallback[n];
            ++n;
        }
    } else {
        unsigned char cells[5];
        std::memcpy(cells, e.buffer, 5);
        n = g_api->encode(cells, e.buffer, room);
    }
    for (unsigned i = n; i < kBufferBytes; ++i) e.buffer[i] = 0;   // the NUL the commits' text record wants
    return n;
}

name_grid::Cells GridDrawnCells(const EntryBytes& e) {
    name_grid::Cells c = CellsOf(e);
    const unsigned first = g_api->first_page();
    if (c.page < first || c.page >= name_grid::detail::kMaxPages || c.row >= name_grid::detail::kRows ||
        c.column >= name_grid::detail::kColumns) {
        c.page = static_cast<unsigned char>(first);
        c.row = 0;
        c.column = 0;
    }
    return c;
}

void GridShownName(const EntryBytes& e, unsigned char out[kBufferBytes]) {
    unsigned char cells[5];
    std::memcpy(cells, e.buffer, 5);
    g_api->encode(cells, out, kBufferBytes);
}

}  // namespace commu_name

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

namespace {

// The draw's search for a pick not yet taken: `a` is kept when the category has
// no picks or when one of its picks differs from it (the original's test: it
// moves on only while every earlier pick equals `a` - docs/rest_4d.md section 5,
// L1); else `a` + 1, wrapping to 0 at the category's count.
unsigned char Unpicked(unsigned category, unsigned char a, unsigned count) {
    const unsigned char limit = B(::bof3::Img(at::kDrawPools.v) + 2 * category + 1);
    for (;;) {
        unsigned i = 0;
        if (static_cast<int>(count) > 0) {
            while (i < count && B(::bof3::Img(at::kName.v) + 10 * category + i) == a) ++i;
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
    if (static_cast<int>(B(::bof3::Img(at::kPicked.v) + B(row))) - 1 > 0) {
        for (;;) {
            if ((static_cast<unsigned>(SH_CALL(Rand)()) & 1) != 0) break;
            ++i;
            if (!(static_cast<int>(i) < static_cast<int>(B(::bof3::Img(at::kPicked.v) + B(row))) - 1)) break;
        }
    }
    return i;
}

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

// The yes / no ask of both confirm steps (0x45D290, 0x45DD10): confirm on the
// second answer - the header 0xF0, sound 0x104, the step back; on the first -
// the header 0xFFFF (none), sounds 0x104 and 0x102, the step on, the slide 0;
// cancel - the header 0xF0, sound 0x106, the step back; else an auto-repeated
// up / down flips the answer (sound 0x100). Answers whether the header is
// still drawn, and draws it with the hand on the answer while it asks.
bool AskYesNo() {
    const unsigned pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0) {
        if (static_cast<unsigned char>(Rest4c_Cursor) == 1) {
            Rest4d_Header = 0xF0;
            SH_CALL(Sound_PlayEffect)(0x104);
            Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep - 1);
        } else {
            Rest4d_Header = 0xFFFF;
            SH_CALL(Sound_PlayEffect)(0x104);
            SH_CALL(Sound_PlayEffect)(0x102);
            const auto step = static_cast<unsigned char>(Rest4b_BoardStep + 1);
            Rest4d_Count = 0;
            Rest4b_BoardStep = step;
        }
    } else if ((Field_CancelButtons & pressed) != 0) {
        Rest4d_Header = 0xF0;
        SH_CALL(Sound_PlayEffect)(0x106);
        Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep - 1);
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0xA000);
        if ((keys & 0xFFFF) != 0) {
            Rest4c_Cursor = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Cursor) ^ 1);
            SH_CALL(Sound_PlayEffect)(0x100);
        }
    }
    if (Rest4d_Header == 0xFFFF) return false;
    SH_CALL(CommuName_DrawHeader)();
    if (Rest4d_Header == 0xF1) SH_CALL(Menu_DrawHand)(0xCE + 36 * Rest4c_Cursor, 0x15, 0);
    return true;
}

// A name drawn at random (R4E's 0x45ED70 writes it at 0x675F98 and answers its
// length + 1), copied into the first text record, message 0xF3, the step's
// step on, Field_Request 2.
void RandomNameAsk() {
    const U n = RandomName();
    std::memmove(At(::bof3::Img(at::kTextRecords.v)), At(::bof3::Img(at::kName.v)), n);
    SH_CALL(Msg_OpenScript)(0xF3);
    const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
    Field_Request = 2;
    Rest4c_Step = step;
}

// The answer to message 0xF3, once it is closed: 0x939A3D not 0 - sound 0x102,
// the step's step on, the slide 0; else that step back to 0 (draw again).
void RandomAnswer() {
    if (Field_Request == 2) return;
    if (Rest4d_Again != 0) {
        SH_CALL(Sound_PlayEffect)(0x102);
        const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
        Rest4d_Count = 0;
        Rest4c_Step = step;
    } else {
        Rest4c_Step = 0;
    }
}

// The answer to message 0xF4 / the entry, once it is closed: 0x939A3D 0 - the
// slide 0, the step's step on; else the slide 4, that step back to 0.
void EntryAnswer() {
    if (Field_Request == 2) return;
    if (Rest4d_Again == 0) {
        Rest4d_Count = 0;
        Rest4c_Step = static_cast<unsigned char>(Rest4c_Step + 1);
    } else {
        Rest4d_Count = 4;
        Rest4c_Step = 0;
    }
}

// The members' hand on the cursor: (0x20 + 136 * (cursor & 1), 0x2B + 56 *
// (cursor >> 1, signed)).
void MemberCursorHand(unsigned flag) {
    MemberPanels();
    const auto cursor = Rest4c_Count;
    const int column = cursor & 1;
    const int row = cursor >> 1;
    MemberHand(0x20 + 136 * column, 0x2B + 56 * row, flag, 6);
}

}  // namespace

#pragma clang attribute pop

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x45C8C0: CommuDraw_States[0x939A3E].
extern "C" void __cdecl CommuDraw_Dispatch(void) { Jump(::bof3::Img(CommuDraw_States_addr), 3, Rest4b_BoardState, "CommuDraw_Dispatch"); }

// original 0x45C8D0, state 0: CommuDraw_OpenSteps[0x939A40].
extern "C" void __cdecl CommuDraw_OpenStep(void) { Jump(::bof3::Img(CommuDraw_OpenSteps_addr), 2, Rest4b_BoardStep, "CommuDraw_OpenStep"); }

// original 0x45C8E0, state 0 step 0: Transition_Start(0), the step on.
extern "C" void __cdecl CommuDraw_FadeOut(void) {
    SH_CALL(Transition_Start)(0);
    Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
}

// original 0x45C900, state 0 step 1: once the wait word is 0 - the playing
// track (Music_Track) kept at 0x675F90, Draw_PassFlags 0, the music faded out
// over 10 and track 0x67 played over 8; state 1, step 0.
extern "C" void __cdecl CommuDraw_MusicIn(void) {
    if (Transition_Busy != 0) return;
    const unsigned char track = Music_Track;
    Draw_PassFlags = 0;
    Rest4d_KeptTrack = track;
    SH_CALL(Music_FadeOutStop)(10);
    SH_CALL(Music_Play)(0x67, 8);
    const auto state = static_cast<unsigned char>(Rest4b_BoardState + 1);
    Rest4b_BoardStep = 0;
    Rest4b_BoardState = state;
}

// original 0x45C950, state 1: CommuDraw_ShowSteps[0x939A40].
extern "C" void __cdecl CommuDraw_ShowStep(void) { Jump(::bof3::Img(CommuDraw_ShowSteps_addr), 5, Rest4b_BoardStep, "CommuDraw_ShowStep"); }

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
    B(::bof3::Img(at::kPicked.v)) = 0;
    Rest4c_Count = row0;
    B(::bof3::Img(at::kPicked.v) + 1) = 0;
    B(::bof3::Img(at::kPicked.v) + 2) = 0;
    B(::bof3::Img(at::kPicked.v) + 3) = 0;
    int row = static_cast<signed char>(row0);
    U record = ::bof3::Img(at::kTextRecords.v);
    for (unsigned i = 0;; ++i) {
        const unsigned category = B(::bof3::Img(at::kDrawRows.v) + static_cast<U>(row * 9 + static_cast<int>(i)));
        if (category == 9) break;
        const unsigned char first = SH_CALL(CommuDraw_RandBelow)(B(::bof3::Img(at::kDrawPools.v) + 2 * category + 1));
        const unsigned count = B(::bof3::Img(at::kPicked.v) + category);
        const unsigned char a = Unpicked(category, first, count);
        B(::bof3::Img(at::kName.v) + 10 * category + count) = a;
        const unsigned char counted = static_cast<unsigned char>(B(::bof3::Img(at::kPicked.v) + category) + 1);
        const unsigned message = B(::bof3::Img(at::kDrawPools.v) + 2 * category) + a;
        B(::bof3::Img(at::kPicked.v) + category) = counted;
        const unsigned char* p = PoolText(::bof3::Img(at::kDrawWords.v), static_cast<int>(message));
        for (unsigned n = 0;;) {
            B(record + n) = *p;
            if (*p == 0 && (p[-1] & 0x80) == 0) break;
            ++p;
            if (++n >= 0x20) break;
        }
        record += 0x20;
        if (record >= ::bof3::Img(at::kTextRecordsEnd.v)) break;
        row = Rest4c_Count;
    }

    const unsigned char roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 7;
    unsigned j = 0;
    while (j < 3 && roll < B(::bof3::Img(at::kDrawPicks.v) + 3 * j)) ++j;
    const unsigned char cursor = static_cast<unsigned char>(Rest4c_Count);
    if (cursor == 6) {
        if (j == 2) j = 3;
    } else if (cursor == 7) {
        if (j == 3) j = 2;
    }
    const U pick = ::bof3::Img(at::kDrawPicks.v) + 3 * j;
    if ((static_cast<unsigned>(SH_CALL(Rand)()) & 1) != 0) {
        const unsigned i = PickOfCategory(pick + 1);
        const unsigned category = B(pick + 1);
        Rest4d_TitleA = static_cast<unsigned char>(B(::bof3::Img(at::kName.v) + 10 * category + i) + B(::bof3::Img(at::kDrawPools.v) + 2 * category));
    } else {
        const unsigned char a = SH_CALL(CommuDraw_RandBelow)(B(::bof3::Img(at::kDrawPools.v) + 2 * B(pick + 1) + 1));
        Rest4d_TitleA = static_cast<unsigned char>(a + B(::bof3::Img(at::kDrawPools.v) + 2 * B(pick + 1)));
    }
    if (B(pick + 2) == 0xFF) {
        Rest4d_TitleB = 0xFF;
    } else if ((static_cast<unsigned>(SH_CALL(Rand)()) & 1) != 0) {
        const unsigned i = PickOfCategory(pick + 2);
        const unsigned category = B(pick + 2);
        Rest4d_TitleB = static_cast<unsigned char>(B(::bof3::Img(at::kName.v) + 10 * category + i) + B(::bof3::Img(at::kDrawPools.v) + 2 * category));
    } else {
        const unsigned char a = SH_CALL(CommuDraw_RandBelow)(B(::bof3::Img(at::kDrawPools.v) + 2 * B(pick + 2) + 1));
        Rest4d_TitleB = static_cast<unsigned char>(a + B(::bof3::Img(at::kDrawPools.v) + 2 * B(pick + 2)));
    }
    const auto step = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Rest4d_Count = 0;
    Rest4b_BoardStep = step;
}

// original 0x45CC50, step 1: the title, then a semi-transparent POLY_G4 (abr 2,
// tpage (1, 2, 0x140, 0)) from black at y = 0x675F95 to white at y + 0x10,
// across x 0..320, and a black TILE from y + 0x10, 320 wide, 0xE0 - y high.
// 0x675F95 one more, three more while a button is held; the step on at 0xC8.
// DIV-0041: both run from Widescreen_FillX() over the wide frame (the
// gradient to 320 + the columns, the tile Widescreen_FillWidth() wide); the
// original's 0..320 while the fills are unarmed, as the fuzz compares. The
// PSP widened the tile only (-32, 384 wide) and left the gradient 320 wide;
// ours widens both, so no 16-row strip is left unshaded in the bands.
extern "C" void __cdecl CommuDraw_Reveal(void) {
    SH_CALL(CommuDraw_DrawTitle)();
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(1, 2, 0x140, 0) & 0xFFFF;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(0, 0xC);
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const U left = std::bit_cast<U>(Widescreen_FillX());
    const U right = std::bit_cast<U>(Widescreen_FillRight(320.0f));   // 0x43A00000, 320.0 unarmed
    PutL(p + 8, left);
    PutL(p + 0x18, right);
    PutL(p + 0x28, left);
    PutL(p + 0x38, right);
    PutF(p + 0xC, static_cast<float>(Rest4d_Count));
    PutF(p + 0x1C, static_cast<float>(Rest4d_Count));
    PutF(p + 0x2C, static_cast<float>(Rest4d_Count + 0x10));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0xFF;
    PutF(p + 0x3C, static_cast<float>(Rest4d_Count + 0x10));
    p[0x25] = 0xFF;
    p[0x26] = 0xFF;
    p[0x34] = 0xFF;
    p[0x35] = 0xFF;
    p[0x36] = 0xFF;
    SH_CALL(Gfx_CommitPrim)(0, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    PutL(p + 8, left);
    PutL(p + 0x14, std::bit_cast<U>(Widescreen_FillWidth()));
    PutF(p + 0xC, static_cast<float>(Rest4d_Count + 0x10));
    PutF(p + 0x18, static_cast<float>(0xE0 - Rest4d_Count));
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    SH_CALL(Gfx_CommitPrim)(0, 0x1C);
    const auto count = static_cast<unsigned char>(Rest4d_Count + 1);
    Rest4d_Count = count;
    if (Input_Held != 0) Rest4d_Count = static_cast<unsigned char>(count + 2);
    if (Rest4d_Count >= 0xC8) Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
}

// original 0x45CDC0, step 2: the title; on any press Transition_Start(0) and
// the step on.
extern "C" void __cdecl CommuDraw_WaitKey(void) {
    SH_CALL(CommuDraw_DrawTitle)();
    if (Input_Pressed == 0) return;
    SH_CALL(Transition_Start)(0);
    Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
}

// original 0x45CDF0, step 3: while the wait word is not 0 the title (a tail
// jump); then the music faded out over 10, the kept track played over 8,
// Draw_PassFlags 0x1F, Transition_Start(1), the step on.
extern "C" void __cdecl CommuDraw_MusicBack(void) {
    if (Transition_Busy != 0) {
        SH_CALL(CommuDraw_DrawTitle)();
        return;
    }
    SH_CALL(Music_FadeOutStop)(10);
    SH_CALL(Music_Play)(Rest4d_KeptTrack, 8);
    Draw_PassFlags = 0x1F;
    SH_CALL(Transition_Start)(1);
    Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
}

// original 0x45CE40, step 4: once the wait word is 0, message 0xE2,
// Field_Request 2, the next state, step 0.
extern "C" void __cdecl CommuDraw_Close(void) {
    if (Transition_Busy != 0) return;
    SH_CALL(Msg_OpenScript)(0xE2);
    const auto state = static_cast<unsigned char>(Rest4b_BoardState + 1);
    Field_Request = 2;
    Rest4b_BoardState = state;
    Rest4b_BoardStep = 0;
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

// original 0x45CEA0: the title in the text scratch - '*', message 0x675FC5 of
// the draw's words, message 0x675FC6 unless 0xFF, '+', ' ', the five name bytes
// of the slot the record at 0x939A38 names (+5), a 0 - drawn at (0x32, 0x1E);
// then the row's line (MessagePools word 0x1C6 + 2 * s8 0x675F8C, read after
// the first draw) at (0x32, 0x42).
extern "C" void __cdecl CommuDraw_DrawTitle(void) {
    char* const buf = Scratch();
    buf[0] = 0x2A;
    unsigned char k = AppendMessage(1, PoolText(::bof3::Img(at::kDrawWords.v), Rest4d_TitleA));
    if (Rest4d_TitleB != 0xFF) k = AppendMessage(k, PoolText(::bof3::Img(at::kDrawWords.v), Rest4d_TitleB));
    const unsigned char* const record = reinterpret_cast<const unsigned char*>(Rest4d_Pointer);
    buf[k] = 0x2B;
    buf[static_cast<unsigned char>(k + 1)] = 0x20;
    k = static_cast<unsigned char>(k + 2);
    for (unsigned i = 0; i < 5; ++i) {
        buf[k] = static_cast<char>(B(::bof3::Img(at::kSlotNames.v) + 5u * record[5] + i));
        k = static_cast<unsigned char>(k + 1);
    }
    buf[k] = 0;
    SH_CALL(Text_DrawAt)(0x32, 0x1E, 0, 0xFF, reinterpret_cast<const unsigned char*>(buf));
    SH_CALL(Text_DrawAt)(0x32, 0x42, 0, 0xFF, PoolText(::bof3::Img(at::kTitleWords.v), Rest4c_Count));
}

// original 0x45D040: CommuName_States[0x939A3E].
extern "C" void __cdecl CommuName_Dispatch(void) { Jump(::bof3::Img(CommuName_States_addr), 5, Rest4b_BoardState, "CommuName_Dispatch"); }

// original 0x45D050, state 0: unless a message is open, message 0xEE,
// Field_Request 2, state 1.
extern "C" void __cdecl CommuName_Begin(void) {
    if (Field_Request == 2) return;
    SH_CALL(Msg_OpenScript)(0xEE);
    const auto state = static_cast<unsigned char>(Rest4b_BoardState + 1);
    Field_Request = 2;
    Rest4b_BoardState = state;
}

// original 0x45D080, state 1: CommuName_SlotSteps[0x939A40].
extern "C" void __cdecl CommuName_SlotStep(void) { Jump(::bof3::Img(CommuName_SlotSteps_addr), 9, Rest4b_BoardStep, "CommuName_SlotStep"); }

// original 0x45D090, step 0 of states 1 and 2: unless a message is open, sound
// 0x102, the cursor 0, the slide 4, the header message 0xF0, the step on.
extern "C" void __cdecl CommuName_PanelReset(void) {
    if (Field_Request == 2) return;
    SH_CALL(Sound_PlayEffect)(0x102);
    const auto step = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Rest4c_Count = 0;
    Rest4d_Count = 4;
    Rest4d_Header = 0xF0;
    Rest4b_BoardStep = step;
}

// original 0x45D0D0, state 1 step 1: the slide one less; the title box at the
// slide; the cursor slot's panel at 0x44 - 30 * slide; the slot bar at (0x4A,
// 0x64 - 30 * slide); the step on at slide 0.
extern "C" void __cdecl CommuName_SlotPanelIn(void) {
    const auto slide = static_cast<unsigned char>(Rest4d_Count - 1);
    Rest4d_Count = slide;
    PanelBox(slide);
    const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count));
    SlotPanel(0x44 - 30 * Slide(), 0x32, slot, 0);
    SH_CALL(CommuName_DrawSlotBar)(0x4A, 0x64 - 30 * Slide());
    if (Rest4d_Count == 0) Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
}

// original 0x45D170, state 1 step 2: confirm - the header 0xF1, sound 0x104,
// the step on, the yes / no 0; cancel - sound 0x106, step 8; else the auto-
// repeated up / down (0x8000 / 0x2000, sound 0x100 on any) moves the cursor
// through the slots in use, wrapping. Then the header, the cursor slot's panel
// at 0x44, the slot bar at (0x4A, 0x64), the hand at 0x55 + 8 * cursor.
extern "C" void __cdecl CommuName_SlotChoose(void) {
    const unsigned pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0) {
        Rest4d_Header = 0xF1;
        SH_CALL(Sound_PlayEffect)(0x104);
        const auto step = static_cast<unsigned char>(Rest4b_BoardStep + 1);
        Rest4c_Cursor = 0;
        Rest4b_BoardStep = step;
    } else if ((Field_CancelButtons & pressed) != 0) {
        SH_CALL(Sound_PlayEffect)(0x106);
        Rest4b_BoardStep = 8;
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0xA000);
        if ((keys & 0xFFFF) != 0) SH_CALL(Sound_PlayEffect)(0x100);
        if ((keys & 0x8000) != 0) {
            const auto cursor = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) - 1);
            Rest4c_Count = cursor;
            if ((cursor & 0x80) != 0) {
                const unsigned char n = SH_CALL(CommuName_CountSlots)();
                Rest4c_Count = static_cast<unsigned char>(n - 1);
            }
        } else if ((keys & 0x2000) != 0) {
            Rest4c_Count = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) + 1);
            const unsigned char n = SH_CALL(CommuName_CountSlots)();
            if (!(Rest4c_Count < static_cast<int>(n))) Rest4c_Count = 0;
        }
    }
    SH_CALL(CommuName_DrawHeader)();
    CursorSlotPanel(0x44, 0);
    SH_CALL(CommuName_DrawSlotBar)(0x4A, 0x64);
    SlotHand(0x55 + 8 * Rest4c_Count, 0x6D, 0);
}

// original 0x45D290, state 1 step 3: the ask (above); while the header shows,
// the slot bar at (0x4A, 0x64) and the hand (flag 1); the cursor slot's panel
// at 0x44 always.
extern "C" void __cdecl CommuName_SlotConfirm(void) {
    if (AskYesNo()) {
        SH_CALL(CommuName_DrawSlotBar)(0x4A, 0x64);
        SlotHand(0x55 + 8 * Rest4c_Count, 0x6D, 1);
    }
    CursorSlotPanel(0x44, 0);
}

// original 0x45D3E0, state 1 step 4: the slide one more; the title box at it,
// the slot bar at (0x4A, 0x64 + 40 * slide), the cursor slot's panel at 0x44;
// at slide 4 message 0xF2, the step on, Field_Request 2.
extern "C" void __cdecl CommuName_SlotPanelOut(void) {
    const auto slide = static_cast<unsigned char>(Rest4d_Count + 1);
    Rest4d_Count = slide;
    PanelBox(slide);
    SH_CALL(CommuName_DrawSlotBar)(0x4A, 0x64 + 40 * Slide());
    CursorSlotPanel(0x44, 0);
    if (Rest4d_Count != 4) return;
    SH_CALL(Msg_OpenScript)(0xF2);
    const auto step = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Field_Request = 2;
    Rest4b_BoardStep = step;
}

// original 0x45D480, state 1 step 5: once the message is closed, step 6 when
// 0x939A3C is 0, else the slide 5 and step 7; the cursor slot's panel at 0x44.
extern "C" void __cdecl CommuName_SlotHow(void) {
    if (Field_Request != 2) {
        if (Rest4b_BoardFlag == 0) {
            Rest4b_BoardStep = 6;
        } else {
            Rest4d_Count = 5;
            Rest4b_BoardStep = 7;
        }
    }
    CursorSlotPanel(0x44, 0);
}

// original 0x45D4D0, state 1 step 6: CommuName_SlotRandomSteps[0x939A3F].
extern "C" void __cdecl CommuName_SlotRandomStep(void) {
    Jump(::bof3::Img(CommuName_SlotRandomSteps_addr), 3, Rest4c_Step, "CommuName_SlotRandomStep");
}

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
    Rest4d_Count = static_cast<unsigned char>(Rest4d_Count + 1);
    const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count));
    SlotPanel(0x44 - 30 * Slide(), 0x32, slot, 0);
    if (Rest4d_Count == 4) SlotRename();
}

// original 0x45D600, state 1 step 7: CommuName_SlotEntrySteps[0x939A3F].
extern "C" void __cdecl CommuName_SlotEntryStep(void) {
    Jump(::bof3::Img(CommuName_SlotEntrySteps_addr), 5, Rest4c_Step, "CommuName_SlotEntryStep");
}

// original 0x45D610, step 7.0: the cursor slot's panel at 0x44; the entry's
// draws at 30 * slide; the slide one less; at 0 the slot's five name bytes
// (a 0 byte read as ' ') into 0x675F98, the step's step on, the entry's column
// and the three bytes 0.
extern "C" void __cdecl CommuName_SlotEntryIn(void) {
    const bool grid = commu_name::GridOn(true);
    CursorSlotPanel(0x44, 0);
    EntryDraws(30, grid);
    const auto slide = static_cast<unsigned char>(Rest4d_Count - 1);
    Rest4d_Count = slide;
    if (slide != 0) return;
    if (grid) {   // CommuNameGrid: the slot's five bytes decoded to cells, the page the grid's first
        const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
        commu_name::GridEntryIn(commu_name::LiveEntryBytes(), At(::bof3::Img(at::kSlotNames.v) + 5 * slot), commu_name::kSlotRoom);
        return;
    }
    for (unsigned i = 0; i < 5; ++i) {
        const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
        if (B(::bof3::Img(at::kSlotNames.v) + 5 * slot + i) == 0) {
            B(::bof3::Img(at::kName.v) + i) = 0x20;
        } else {
            const unsigned again = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
            B(::bof3::Img(at::kName.v) + i) = B(::bof3::Img(at::kSlotNames.v) + 5 * again + i);
        }
    }
    const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
    Rest4d_EntryColumn = 0;
    Rest4d_EntryC = 0;
    Rest4d_EntryA = 0;
    Rest4d_EntryB = 0;
    Rest4c_Step = step;
}

// DIV-0075, set by Rest4D_Inject after the self-test: the two entry steps move
// the step's step on, where the original's never does.
namespace rest_4d { unsigned char g_entry_abandons = 0; }

// original 0x45D730, step 7.1: the cursor slot's panel (flag 1), the column's
// grey line at (0x4A + 12 * column, 0x42), the entry's draws, 0x675F96 =
// BareRetZero (0). The original stops here: the PlayStation's input step
// moved the step's step on and the port's BareRetZero does not, so the entry
// never leaves this step. DIVERGENCE DIV-0075: ours moves it on, as the
// PlayStation's input does when the entry is abandoned, and step 7.2 takes
// its unanswered branch (message 0xF7, state 4).
//
// CommuNameGrid: the PlayStation's input step in BareRetZero's place (NG's,
// COMMU02's copy, the slot's 5 bytes), which answers 1 on a finish and moves
// the step on at a finish or a quit, as the PlayStation's does.
extern "C" void __cdecl CommuName_SlotEntry(void) {
    if (commu_name::GridOn(true)) {
        WithShownName([] { CursorSlotPanel(0x44, 1); });
        SH_CALL(Menu_DrawGreyHLine)(0x4A + 12 * Rest4d_EntryColumn, 0x42, 0xC, 0);
        EntryDraws(0, true);
        commu_name::GridEntryInput(commu_name::LiveEntryBytes(), commu_name::kSlotRoom);
        return;
    }
    CursorSlotPanel(0x44, 1);
    SH_CALL(Menu_DrawGreyHLine)(0x4A + 12 * Rest4d_EntryColumn, 0x42, 0xC, 0);
    EntryDraws(0);
    Rest4d_EntryDone = DroppedZero();
    if (rest_4d::g_entry_abandons) Rest4c_Step = static_cast<unsigned char>(Rest4c_Step + 1);       // DIV-0075
}

// original 0x45D7C0, step 7.2: the slide one more; the cursor slot's panel at
// 0x44 (answered) or 0x44 - 30 * slide; the entry's draws at 30 * slide. At
// slide 4: answered - an empty name takes the slot's default (0x653200, 20 a
// slot, read in place), message 0xF4, the step's step on; unanswered - message
// 0xF7, state 4, step 0. Field_Request 2 either way.
//
// CommuNameGrid: the panels drawn with the name's bytes in the cells' place;
// answered, the cells encoded into 0x675F98 (or the default's five bytes) and
// NUL-filled, so that the commit and its text record take the layer's bytes.
extern "C" void __cdecl CommuName_SlotEntryOut(void) {
    if (commu_name::GridOn(true)) {
        const unsigned char done = Rest4d_EntryDone;
        Rest4d_Count = static_cast<unsigned char>(Rest4d_Count + 1);
        WithShownName([done] {
            if (done != 0) {
                CursorSlotPanel(0x44, 1);
            } else {
                const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count));
                SlotPanel(0x44 - 30 * Slide(), 0x32, slot, 1);
            }
        });
        EntryDraws(30, true);
        if (Rest4d_Count != 4) return;
        if (Rest4d_EntryDone != 0) {
            const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
            commu_name::GridEntryAnswered(commu_name::LiveEntryBytes(), commu_name::kSlotRoom,
                                          At(::bof3::Img(at::kSlotDefaults.v) + 20 * slot), commu_name::kSlotRoom);
            SH_CALL(Msg_OpenScript)(0xF4);
            const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
            Field_Request = 2;
            Rest4c_Step = step;
        } else {
            Say(0xF7);
            Rest4b_BoardState = 4;
            Rest4b_BoardStep = 0;
        }
        return;
    }
    const unsigned char done = Rest4d_EntryDone;
    Rest4d_Count = static_cast<unsigned char>(Rest4d_Count + 1);
    if (done != 0) {
        CursorSlotPanel(0x44, 1);
    } else {
        const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count));
        SlotPanel(0x44 - 30 * Slide(), 0x32, slot, 1);
    }
    EntryDraws(30);
    if (Rest4d_Count != 4) return;
    if (Rest4d_EntryDone != 0) {
        if (B(::bof3::Img(at::kName.v)) == 0) {
            for (unsigned i = 0; i < 5; ++i) {
                const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
                B(::bof3::Img(at::kName.v) + i) = B(::bof3::Img(at::kSlotDefaults.v) + 20 * slot + i);
            }
        }
        SH_CALL(Msg_OpenScript)(0xF4);
        const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
        Field_Request = 2;
        Rest4c_Step = step;
    } else {
        Say(0xF7);
        Rest4b_BoardState = 4;
        Rest4b_BoardStep = 0;
    }
}

// original 0x45D930, step 7.3: the cursor slot's panel (flag 1); the answer.
extern "C" void __cdecl CommuName_SlotEntryAsk(void) {
    CursorSlotPanel(0x44, 1);
    EntryAnswer();
}

// original 0x45D990, step 7.4: as 6.2 with flag 1.
extern "C" void __cdecl CommuName_SlotEntryDone(void) {
    Rest4d_Count = static_cast<unsigned char>(Rest4d_Count + 1);
    const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count));
    SlotPanel(0x44 - 30 * Slide(), 0x32, slot, 1);
    if (Rest4d_Count == 4) SlotRename();
}

// original 0x45D9E0, state 1 step 8: the slide one more; the title box, the
// cursor slot's panel at 0x20 - 30 * slide, the slot bar at (0x4A, 0x64 - 30 *
// slide); at 4 message 0xF7, state 4, step 0.
extern "C" void __cdecl CommuName_SlotClose(void) {
    const auto slide = static_cast<unsigned char>(Rest4d_Count + 1);
    Rest4d_Count = slide;
    PanelBox(slide);
    const unsigned slot = SH_CALL(CommuName_NthSlot)(static_cast<unsigned char>(Rest4c_Count));
    SlotPanel(0x20 - 30 * Slide(), 0x32, slot, 0);
    SH_CALL(CommuName_DrawSlotBar)(0x4A, 0x64 - 30 * Slide());
    if (Rest4d_Count != 4) return;
    Say(0xF7);
    Rest4b_BoardState = 4;
    Rest4b_BoardStep = 0;
}

// original 0x45DAA0, state 2: CommuName_MemberSteps[0x939A40].
extern "C" void __cdecl CommuName_MemberStep(void) { Jump(::bof3::Img(CommuName_MemberSteps_addr), 9, Rest4b_BoardStep, "CommuName_MemberStep"); }

// original 0x45DAB0, state 2 step 1: the slide one less; the title box; the
// grid at x 0x20 - 40 * slide and 0xA8 + 40 * slide; the step on at 0.
extern "C" void __cdecl CommuName_MemberPanelIn(void) {
    const auto slide = static_cast<unsigned char>(Rest4d_Count - 1);
    Rest4d_Count = slide;
    PanelBox(slide);
    unsigned char n = 0;
    unsigned record = 0;
    for (U flags = ::bof3::Img(CharacterRecords_addr) + 0xb; flags < ::bof3::Img(at::kRecordFlagsEnd); flags += at::kRecordStride, ++record) {
        if ((B(flags) & 1) == 0) continue;
        const int y = 0x2B + 56 * (n >> 1);
        const int x = (n & 1) == 0 ? 0x20 - 40 * Slide() : 0xA8 + 40 * Slide();
        MemberPanel(x, y, record, 0);
        n = static_cast<unsigned char>(n + 1);
    }
    if (Rest4d_Count == 0) Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
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
        Rest4d_Header = 0xF1;
        SH_CALL(Sound_PlayEffect)(0x104);
        const auto step = static_cast<unsigned char>(Rest4b_BoardStep + 1);
        Rest4c_Cursor = 0;
        Rest4b_BoardStep = step;
    } else if ((Field_CancelButtons & pressed) != 0) {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(Sound_PlayEffect)(0x102);
        Rest4d_Count = 0;
        Rest4b_BoardStep = 8;
    } else {
        const auto last = static_cast<unsigned char>(MemberCount() - 1);
        const unsigned keys = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF000);
        if ((keys & 0xFFFF) != 0) SH_CALL(Sound_PlayEffect)(0x100);
        if ((keys & 0xA000) != 0) {
            const auto cursor = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) ^ 1);
            Rest4c_Count = cursor;
            if (static_cast<signed char>(cursor) > static_cast<int>(last)) Rest4c_Count = static_cast<unsigned char>(cursor ^ 1);
        } else if ((keys & 0x1000) != 0) {
            const auto cursor = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) + 0xFE);
            Rest4c_Count = cursor;
            if ((cursor & 0x80) != 0) B(::bof3::Img(at::kCursor.v)) = static_cast<unsigned char>((cursor | 0xFE) & last);
        } else if ((keys & 0x4000) != 0) {
            const auto cursor = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) + 2);
            Rest4c_Count = cursor;
            const int now = static_cast<signed char>(cursor);
            const int bound = static_cast<unsigned char>(cursor | 0xFE) & last;
            if (now > bound) Rest4c_Count = static_cast<unsigned char>(B(::bof3::Img(at::kCursor.v)) & 1);
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
    const auto slide = static_cast<unsigned char>(Rest4d_Count + 1);
    Rest4d_Count = slide;
    PanelBox(slide);
    unsigned char n = 0;
    unsigned record = 0;
    for (U flags = ::bof3::Img(CharacterRecords_addr) + 0xb; flags < ::bof3::Img(at::kRecordFlagsEnd); flags += at::kRecordStride, ++record) {
        if ((B(flags) & 1) == 0) continue;
        if (static_cast<int>(n) != S8(::bof3::Img(at::kCursor.v))) {
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
    if (Rest4d_Count != 4) return;
    SH_CALL(Msg_OpenScript)(0xF2);
    const auto step = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Field_Request = 2;
    Rest4b_BoardStep = step;
}

// original 0x45E000, state 2 step 5: as the slot's (the slide 4 for step 7);
// the cursor member's panel at 0x20.
extern "C" void __cdecl CommuName_MemberHow(void) {
    if (Field_Request != 2) {
        if (Rest4b_BoardFlag == 0) {
            Rest4b_BoardStep = 6;
        } else {
            Rest4d_Count = 4;
            Rest4b_BoardStep = 7;
        }
    }
    CursorMemberPanel(0x20, 0);
}

// original 0x45E050, state 2 step 6: CommuName_MemberRandomSteps[0x939A3F].
extern "C" void __cdecl CommuName_MemberRandomStep(void) {
    Jump(::bof3::Img(CommuName_MemberRandomSteps_addr), 3, Rest4c_Step, "CommuName_MemberRandomStep");
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
    Rest4d_Count = static_cast<unsigned char>(Rest4d_Count + 1);
    const U record = NthMember(static_cast<unsigned char>(Rest4c_Count));
    MemberPanel(0x20 - 40 * Slide(), 0x2B, record, 0);
    if (Rest4d_Count == 4) MemberRename();
}

// original 0x45E180, state 2 step 7: CommuName_MemberEntrySteps[0x939A3F].
extern "C" void __cdecl CommuName_MemberEntryStep(void) {
    Jump(::bof3::Img(CommuName_MemberEntrySteps_addr), 5, Rest4c_Step, "CommuName_MemberEntryStep");
}

// original 0x45E190, step 7.0: the slide one less; the cursor member's panel
// at 0x20; the entry's draws at 30 * slide; at 0 the record's first five bytes
// (0 read as ' ') into 0x675F98, the step's step on, the column and the three
// bytes 0.
extern "C" void __cdecl CommuName_MemberEntryIn(void) {
    const bool grid = commu_name::GridOn(false);
    Rest4d_Count = static_cast<unsigned char>(Rest4d_Count - 1);
    CursorMemberPanel(0x20, 0);
    EntryDraws(30, grid);
    if (Rest4d_Count != 0) return;
    if (grid) {   // CommuNameGrid: the record's name (8 bytes to its NUL) decoded to cells
        const unsigned record = NthMember(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
        if (record >= at::kRecordNameCount)
            bof3::Fatal("CommuName_MemberEntryIn: record %u is past the renamer's %u members", record, at::kRecordNameCount);
        commu_name::GridEntryIn(commu_name::LiveEntryBytes(), At(::bof3::Img(CharacterRecords_addr) + at::kRecordStride * record),
                                commu_name::kMemberRoom);
        return;
    }
    for (unsigned i = 0; i < 5; ++i) {
        const unsigned record = NthMember(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
        if (B(::bof3::Img(CharacterRecords_addr) + at::kRecordStride * record + i) == 0) {
            B(::bof3::Img(at::kName.v) + i) = 0x20;
        } else {
            const unsigned again = NthMember(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
            B(::bof3::Img(at::kName.v) + i) = B(::bof3::Img(CharacterRecords_addr) + at::kRecordStride * again + i);
        }
    }
    const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
    Rest4d_EntryColumn = 0;
    Rest4d_EntryC = 0;
    Rest4d_EntryA = 0;
    Rest4d_EntryB = 0;
    Rest4c_Step = step;
}

// original 0x45E2C0, step 7.1: the cursor member's panel (flag 1), the
// column's grey line at (0x5B + 12 * column, 0x50), the entry's draws,
// 0x675F96 = BareRetZero; the step's step on as the slot's (DIV-0075).
// CommuNameGrid: the input step as the slot's, the record's 8 bytes (four
// cells under the Japan family, call 5).
extern "C" void __cdecl CommuName_MemberEntry(void) {
    if (commu_name::GridOn(false)) {
        WithShownName([] { CursorMemberPanel(0x20, 1); });
        SH_CALL(Menu_DrawGreyHLine)(0x5B + 12 * Rest4d_EntryColumn, 0x50, 0xC, 0);
        EntryDraws(0, true);
        commu_name::GridEntryInput(commu_name::LiveEntryBytes(), commu_name::kMemberRoom);
        return;
    }
    CursorMemberPanel(0x20, 1);
    SH_CALL(Menu_DrawGreyHLine)(0x5B + 12 * Rest4d_EntryColumn, 0x50, 0xC, 0);
    EntryDraws(0);
    Rest4d_EntryDone = DroppedZero();
    if (rest_4d::g_entry_abandons) Rest4c_Step = static_cast<unsigned char>(Rest4c_Step + 1);       // DIV-0075
}

// original 0x45E350, step 7.2: as the slot's, the panel at 0x20 (answered) or
// 0x20 - 30 * slide, an empty name taking the record's name from
// CommuName_RecordNames (seven; ours aborts past them, section 5).
//
// CommuNameGrid: as the slot's; an all-blank name takes the record's default
// name from Char_DefaultRecords (0x64B390 + 0xA4 * record, the name New Game
// gives, the layer's under DIV-0020) to its NUL, at most 8 bytes, where the
// port's path reads CommuName_RecordNames, the exe's own text, which no layer
// replaces (docs/exe-chinese-remaining.md: "read only by the renamer the port
// cut").
extern "C" void __cdecl CommuName_MemberEntryOut(void) {
    if (commu_name::GridOn(false)) {
        const unsigned char done = Rest4d_EntryDone;
        Rest4d_Count = static_cast<unsigned char>(Rest4d_Count + 1);
        WithShownName([done] {
            if (done != 0) {
                CursorMemberPanel(0x20, 1);
            } else {
                const U record = NthMember(static_cast<unsigned char>(Rest4c_Count));
                MemberPanel(0x20 - 30 * Slide(), 0x2B, record, 1);
            }
        });
        EntryDraws(30, true);
        if (Rest4d_Count != 4) return;
        if (Rest4d_EntryDone != 0) {
            const unsigned record = NthMember(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
            if (record >= at::kRecordNameCount)
                bof3::Fatal("CommuName_MemberEntryOut: record %u is past the renamer's %u members", record, at::kRecordNameCount);
            commu_name::GridEntryAnswered(commu_name::LiveEntryBytes(), commu_name::kMemberRoom,
                                          At(::bof3::Img(Char_DefaultRecords_addr) + at::kRecordStride * record), commu_name::kMemberRoom);
            SH_CALL(Msg_OpenScript)(0xF4);
            const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
            Field_Request = 2;
            Rest4c_Step = step;
        } else {
            Say(0xF7);
            Rest4b_BoardState = 4;
            Rest4b_BoardStep = 0;
        }
        return;
    }
    const unsigned char done = Rest4d_EntryDone;
    Rest4d_Count = static_cast<unsigned char>(Rest4d_Count + 1);
    if (done != 0) {
        CursorMemberPanel(0x20, 1);
    } else {
        const U record = NthMember(static_cast<unsigned char>(Rest4c_Count));
        MemberPanel(0x20 - 30 * Slide(), 0x2B, record, 1);
    }
    EntryDraws(30);
    if (Rest4d_Count != 4) return;
    if (Rest4d_EntryDone != 0) {
        if (B(::bof3::Img(at::kName.v)) == 0) {
            for (unsigned i = 0; i < 5; ++i) {
                const unsigned record = NthMember(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
                const U name = RecordName(record);
                B(::bof3::Img(at::kName.v) + i) = *At(name + i);
            }
        }
        SH_CALL(Msg_OpenScript)(0xF4);
        const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
        Field_Request = 2;
        Rest4c_Step = step;
    } else {
        Say(0xF7);
        Rest4b_BoardState = 4;
        Rest4b_BoardStep = 0;
    }
}

// original 0x45E4C0, step 7.3: the cursor member's panel (flag 1); the answer.
extern "C" void __cdecl CommuName_MemberEntryAsk(void) {
    CursorMemberPanel(0x20, 1);
    EntryAnswer();
}

// original 0x45E520, step 7.4: the slide one more, the cursor member's panel
// at 0x20 - 30 * slide (flag 1); at 4 R4E's 0x45F5A0.
//
// CommuNameGrid: the commit copies eight bytes, so a name of eight (four kana,
// or five Latin cells with three of two bytes) has no NUL among them; ours
// writes it after the commit, at the record's +8 and after the text record's
// eight (Text_Records + 0x28), for message 0xF5 and every later draw.
extern "C" void __cdecl CommuName_MemberEntryDone(void) {
    Rest4d_Count = static_cast<unsigned char>(Rest4d_Count + 1);
    const U record = NthMember(static_cast<unsigned char>(Rest4c_Count));
    MemberPanel(0x20 - 30 * Slide(), 0x2B, record, 1);
    if (Rest4d_Count != 4) return;
    MemberRename();
    if (!commu_name::GridOn(false)) return;
    const unsigned committed = NthMember(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
    if (committed >= at::kRecordNameCount)
        bof3::Fatal("CommuName_MemberEntryDone: record %u is past the renamer's %u members", committed, at::kRecordNameCount);
    B(::bof3::Img(CharacterRecords_addr) + at::kRecordStride * committed + commu_name::kMemberRoom) = 0;
    B(::bof3::Img(at::kTextRecords.v) + 0x20 + commu_name::kMemberRoom) = 0;
}

// original 0x45E570, state 2 step 8: the slide one more; the title box; the
// grid at x 0x18 - 40 * slide and 0xA8 + 40 * slide, y 0x30 + 56 * row; at 4
// message 0xF7, state 4, step 0.
extern "C" void __cdecl CommuName_MemberClose(void) {
    const auto slide = static_cast<unsigned char>(Rest4d_Count + 1);
    Rest4d_Count = slide;
    PanelBox(slide);
    unsigned char n = 0;
    unsigned record = 0;
    for (U flags = ::bof3::Img(CharacterRecords_addr) + 0xb; flags < ::bof3::Img(at::kRecordFlagsEnd); flags += at::kRecordStride, ++record) {
        if ((B(flags) & 1) == 0) continue;
        const int y = 0x30 + 56 * (n >> 1);
        const int x = (n & 1) == 0 ? 0x18 - 40 * Slide() : 0xA8 + 40 * Slide();
        MemberPanel(x, y, record, 0);
        n = static_cast<unsigned char>(n + 1);
    }
    if (Rest4d_Count != 4) return;
    Say(0xF7);
    Rest4b_BoardState = 4;
    Rest4b_BoardStep = 0;
}

// original 0x45E670, state 3: once the message is closed and at step 0,
// message 0xF6, Field_Request 2.
extern "C" void __cdecl CommuName_End(void) {
    if (Field_Request == 2 || Rest4b_BoardStep != 0) return;
    Say(0xF6);
}

#pragma clang attribute pop

void CommuName_Inject() {
    // DIVERGENCE DIV-0075: after the self-test, which compares Capcom's entry steps.
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("CommuNameEntryAbandons",
                         Low32(&rest_4d::g_entry_abandons), &was, &is, 1);
        bof3::Log("DIV-0075    the community's name entry ends unanswered where the original never leaves its step");
    }
    // CommuNameGrid (the New Game round's CR; its ledger entry is stage C's):
    // the entry is the name grid's where a layer has the grid (kind 22),
    // DIV-0075's end where not. After rest_4d's self-test, which compares
    // Capcom's entry steps with the byte 0.
    {
        static const std::uint8_t was = 0, is = 1;
        if (bof3::PatchBytes("CommuNameGrid", Low32(&commu_name::g_grid), &was, &is, 1))
            bof3::Log("CommuNameGrid the community's name entry runs the name grid where the language layer has one");
    }
    if (bof3::WantsShadow("rest_4d")) {
        rest_4d::EntryAbandonTest();   // DIV-0075's row: the switch on, before the inject
        rest_4d::GridTest();           // CommuNameGrid's rows: no grid, call 6, the grid's paths
    }
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
}
