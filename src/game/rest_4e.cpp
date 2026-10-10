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
#include "game/move_script_bytes.h"
#include "game/labels.h"
#include "game/rest_4e_callees.h"
#include "game/text_advance.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();

// --- the cells ------------------------------------------------------------------------

constexpr U kMode = 0x939A3E;          // u8: the facility's mode (the tables' index)
constexpr U kNameMode = 0x939A3F;      // u8: zeroed by the name commits (R4D's step)
constexpr U kStep = 0x939A40;          // u8: the mode's step
constexpr U kStyle = 0x903A5A;         // u8: the window style (Menu_DrawBox's colour)
constexpr U kFontPage = 0x903A59;      // u8: 0 or not, which label CommuMusic_DrawList shows

// The band's own cells 0x675F8C..0x675FDB.
constexpr U kCursor = 0x675F8C;        // u8: which name the commits write (the n-th in use)
constexpr U kEdit = 0x675F98;          // u8 x 32: the name being edited (CommuName_MakeRandom clears 32)
constexpr U kEditSize = 0x20;
constexpr U kSavedTrack = 0x675FD0;    // u8: Music_Track when the list opened
constexpr U kSlide = 0x675FD1;         // u8: the list's slide 4..0
constexpr U kTrack = 0x675FD2;         // s16: the list's row, the track index
constexpr U kItemSlide = 0x675FD4;     // u8: the item window's slide 0..4
constexpr U kPages = 0x675FD8;         // u8 x 3: the three lists' page counts
constexpr U kPage = 0x675FDB;          // s8: the page shown, over the three lists

// The 60-entry table (8 bytes an entry: +0 in use, +1 a kind, +2 / +3 an item
// and its tab, +4 a dword) and its 60 names of 5 bytes; the entry 0x9039F5.
constexpr U kEntries = 0x9046D0;
constexpr U kEntryNames = 0x9048F0;
constexpr unsigned kEntryCount = 60;
constexpr U kEntry = 0x9039F5;         // u8: the entry the item and track screens are for
constexpr U kFacility = 0x9039F4;      // u8: the facility table 0x652AE4's index (R4B's reader)
constexpr U kTrackCounts = 0x9048AB;   // u8 at + 8 * kind: the track count of a kind (the records 0x9048B0..)
constexpr U kEntryDword = 0x904134;    // u32: copied into the entry's +4 when an item is given

// The seven character records (CharacterRecords 0x903A70, 0xA4 each; +0 the
// name, +9 the portrait, +0xB bit 0 present).
constexpr U kMembers = 0x903A70;
constexpr U kMemberStride = 0xA4;
constexpr unsigned kMemberCount = 7;

// The three ranked lists: counts 0x9039A0 (entries 0x9039C0.., 7 a page),
// 0x904A90 (entries 0x904F00.., 20 a page), 0x937F80 (pairs 0x904CA0.., 8 a
// page).
constexpr U kListA = 0x9039A0, kListAEntries = 0x9039C0;
constexpr U kListB = 0x904A90, kListBEntries = 0x904F00;
constexpr U kListC = 0x937F80, kListCPairs = 0x904CA0;

// WindowRecords 0 and 1 (0x24 each): the item window and its cursor.
constexpr U kWin0 = 0x803160, kWin1 = 0x803184;
constexpr U kWinOff = kWin0 + 3;       // u8: window 0 shut (the slide waits for 0)
constexpr U kTab = kWin0 + 0xA;        // u8: the tab 0..3 (Inventory_IdLists' index)
constexpr U kTop = kWin0 + 0xB;        // u8: the first row shown
constexpr U kRow = kWin0 + 0xC;        // u8: the row
constexpr U kTabMove = kWin0 + 0x10;   // u16: 0x31 / 0x32 when the tab moved
constexpr U kScroll = kWin0 + 0x12;    // u16: 0xF0 / 0x10 while the list scrolls

constexpr U kTextRecords = 0x904CE0;   // Text_Records (0x20 each)
constexpr U kTextBuffer = 0x904BA0;    // the text scratch sprintf writes
constexpr U kScriptPool = 0x803580;    // Msg_OpenScript's pool: base + u16[base + 2 id]

// Image tables, read in place.
constexpr U kPieces = 0x652EE4;        // 6 bytes a piece: u, v, w, h, clut x, clut row
constexpr U kPortraits = 0x652F38;     // 4 bytes a portrait: u, v, clut x, clut row
constexpr U kPanelPieces = 0x652F5C;   // the member panel's Menu_DrawPieces list
constexpr U kTracks = 0x653098;        // u8 x 40: the list's tracks
constexpr U kLabelA = 0x6530F0, kLabelB = 0x6530F4, kLabelC = 0x6530F8;   // pointers to three labels
constexpr U kGlyphsA = 0x653144, kGlyphsB = 0x653148, kGlyphsC = 0x65314C;
constexpr U kBars = 0x653210;          // 20 bytes an entry: its four bars' lengths
constexpr U kPairMessages = 0x653180;  // 4 bytes a kind: the message id, a "name" word
constexpr U kTilesTop = 0x6531A0, kTilesBottom = 0x6531C0, kTilesSide = 0x6531E0;
constexpr U kPageFormat = 0x6531F4;    // "page / pages"
constexpr U kOneGlyph = 0x669F08;
constexpr U kFour = 0x5C41C8, kEight = 0x5C41CC;   // the floats 4.0, 8.0

unsigned char& B(U address) { return At(address)[0]; }
U W(U address) { return Word(At(address)); }
void SetW(U address, U v) { SetWord(At(address), v); }
U L(U address) { return static_cast<U>(Long(At(address))); }
void SetL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <class T> T Get(const unsigned char* p, unsigned offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof v);
    return v;
}
template <class T> void Put(unsigned char* p, unsigned offset, T v) { std::memcpy(p + offset, &v, sizeof v); }
float F(int v) { return static_cast<float>(v); }
int S16(U v) { return static_cast<short>(v); }
const unsigned char* Message(unsigned id) { return At(kScriptPool + W(kScriptPool + 2 * id)); }

// jmp / call [table + 4 * byte]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after.
void Run(const char* who, U table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_4e.md section 7)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * index)))();
}

// The entry 0x9039F5 indexes the 60-entry table; past it the original writes
// the save block's bytes after (docs/rest_4e.md section 7). For a write.
unsigned char* EntryRecord(const char* who) {
    const unsigned e = B(kEntry);
    if (e >= kEntryCount)
        bof3::Fatal("%s: the entry 0x9039F5 is %u, past the 60 of 0x9046D0 (docs/rest_4e.md section 7)", who, e);
    return At(kEntries + 8 * e);
}

// --- the callees ------------------------------------------------------------------------
void Commit(unsigned size) { SH_CALL(Gfx_CommitPrim)(1, size); }
void DrawMode(unsigned dtd, unsigned tpage, const short* window) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, static_cast<int>(dtd), tpage,
                             static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(window)));
    Commit(0xC);
}
void Grey(unsigned char* p) { p[4] = p[5] = p[6] = 0x80; }
void OpenMessage(unsigned short id) { SH_CALL(Msg_OpenScript)(id); }
void Box(int x, int y, int w, int h, int flags) { SH_CALL(Menu_DrawBox)(x, y, w, h, flags, B(kStyle)); }

// A colour that pulses with Frame_Counter's low byte: bit 3 set,
// ((n & 6) << 5) + 0x3F; clear, ((~n & 6) << 5) + 0x3F.
unsigned char Pulse() {
    const auto n = static_cast<unsigned char>(Frame_Counter);
    const unsigned two = n & 8 ? n & 6 : ~n & 6;
    return static_cast<unsigned char>((two << 5) + 0x3F);
}

}  // namespace

// ===========================================================================
// The panels: an entry's, a member's, and their pieces
// ===========================================================================

// original 0x45E870: an entry's panel at (x, y) - a box 0x48 x 0x26, its frame
// (CommuEntry_DrawPanelFrame), the entry's name (or, `editing` set, the name
// being edited) at (x + 6, y + 4), a black 60 x 16 TILE at (x + 6, y + 0x12)
// and the entry's four bars (lengths 12 x the bytes at 0x653210 + 20 entry,
// colours 0..3) a row of 4 each below it. The original writes its own argument
// slots (scratch for fild), which its callers pop unread.
extern "C" void __cdecl CommuEntry_DrawPanel(int x, int y, unsigned entry, unsigned editing) {
    Box(x, y, 0x48, 0x26, 0x80);
    SH_CALL(CommuEntry_DrawPanelFrame)(x, y);
    const unsigned e = entry & 0xFF;
    const unsigned char* const name = (editing & 0xFF) == 0 ? At(kEntryNames + 5 * e) : At(kEdit);
    SH_CALL(Text_DrawAt)(x + 6, y + 4, 0, 5, name);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    Put<float>(p, 8, F(S16(x) + 6));
    Put<U>(p, 0x14, 0x42700000u);   // 60.0
    Put<U>(p, 0x18, 0x41800000u);   // 16.0
    Put<float>(p, 0xC, F(S16(y) + 0x12));
    p[4] = p[5] = p[6] = 0;
    Commit(0x1C);
    // 0x653210 is the image's (20 bytes an entry); the entry is the caller's
    // index of the 60 (R4D's), read here as a byte
    for (unsigned k = 0; k < 4; ++k)
        SH_CALL(CommuEntry_DrawBar)(x + 6, y + 0x12 + 4 * static_cast<int>(k), 12 * B(kBars + 20 * e + k), k);
}

// original 0x45E9B0: the entry panel's frame of pieces - piece 6 at (x, y);
// seven 7s along the top and 0xCs along the bottom (y + 0x22) from x + 8, 8
// apart; 8 at (x + 0x40, y); four 9s down the left and 0xAs down the right
// (x + 0x40) from y + 8, 6 apart; 0xB and 0xD at the bottom corners.
extern "C" void __cdecl CommuEntry_DrawPanelFrame(int x, int y) {
    SH_CALL(Commu_DrawPiece6)(x, y, 6);
    for (int i = 0; i < 7; ++i) {
        SH_CALL(Commu_DrawPiece6)(x + 8 * i + 8, y, 7);
        SH_CALL(Commu_DrawPiece6)(x + 8 * i + 8, y + 0x22, 0xC);
    }
    SH_CALL(Commu_DrawPiece6)(x + 0x40, y, 8);
    for (int i = 0; i < 4; ++i) {
        SH_CALL(Commu_DrawPiece6)(x, y + 6 * i + 8, 9);
        SH_CALL(Commu_DrawPiece6)(x + 0x40, y + 6 * i + 8, 0xA);
    }
    SH_CALL(Commu_DrawPiece6)(x, y + 0x22, 0xB);
    SH_CALL(Commu_DrawPiece6)(x + 0x40, y + 0x22, 0xD);
}

// original 0x45EA60: a bar of `length` at (x, y), four rows high, as two
// POLY_G4s - rows y..y + 2 from a quarter of the colour to the colour, y + 2 ..
// y + 4 back. The colour is 0x80 in the channels a 12-byte table on the
// original's stack gives colour (its low byte) 0..3: red, green, blue,
// green and blue; past 3 the original reads its own frame (ours aborts).
// x, y and length are read as their low words, signed.
extern "C" void __cdecl CommuEntry_DrawBar(int x, int y, int length, unsigned colour) {
    static const unsigned char kChannels[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1};
    const unsigned k = colour & 0xFF;
    if (k > 3)
        bof3::Fatal("CommuEntry_DrawBar (0x45EA60): colour %u, past the four of its stack table - the original "
                    "reads its own frame (docs/rest_4e.md section 7)",
                    k);
    const auto r = static_cast<unsigned char>(kChannels[3 * k] << 7);
    const auto g = static_cast<unsigned char>(kChannels[3 * k + 1] << 7);
    const auto b = static_cast<unsigned char>(kChannels[3 * k + 2] << 7);
    const auto r4 = static_cast<unsigned char>(r >> 2), g4 = static_cast<unsigned char>(g >> 2),
               b4 = static_cast<unsigned char>(b >> 2);
    const int sx = S16(x), sy = S16(y);
    const float x0 = F(sx), x1 = F(sx + S16(length));
    const float y0 = F(sy), y1 = F(sy + 2), y2 = F(sy + 4);
    auto colour_at = [](unsigned char* p, unsigned v, unsigned char cr, unsigned char cg, unsigned char cb) {
        p[4 + 0x10 * v] = cr;
        p[5 + 0x10 * v] = cg;
        p[6 + 0x10 * v] = cb;
    };
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    Put<float>(p, 8, x0);
    Put<float>(p, 0x18, x1);
    Put<float>(p, 0x28, x0);
    Put<float>(p, 0x38, x1);
    Put<float>(p, 0xC, y0);
    Put<float>(p, 0x1C, y0);
    Put<float>(p, 0x2C, y1);
    Put<float>(p, 0x3C, y1);
    colour_at(p, 0, r4, g4, b4);
    colour_at(p, 1, r4, g4, b4);
    colour_at(p, 2, r, g, b);
    colour_at(p, 3, r, g, b);
    Commit(0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    Put<float>(p, 8, x0);
    Put<float>(p, 0x18, x1);
    Put<float>(p, 0x28, x0);
    Put<float>(p, 0x38, x1);
    Put<float>(p, 0xC, y1);
    Put<float>(p, 0x1C, y1);
    Put<float>(p, 0x2C, y2);
    Put<float>(p, 0x3C, y2);
    colour_at(p, 0, r, g, b);
    colour_at(p, 1, r, g, b);
    colour_at(p, 2, r4, g4, b4);
    colour_at(p, 3, r4, g4, b4);
    Commit(0x44);
}

// original 0x45EC00: piece `piece` (its low byte) of the image table 0x652EE4
// (6 bytes: u, v, w, h, clut x, clut row) as a SPRT at (x, y) (low words,
// signed), texture page 0x1E, grey 0x80. The table has 14 pieces (the next
// table starts at 0x652F38); past them the original reads on, and so does ours
// (a read only: docs/rest_4e.md section 7).
extern "C" void __cdecl Commu_DrawPiece6(int x, int y, unsigned piece) {
    const unsigned n = piece & 0xFF;
    DrawMode(0, 0x1E, nullptr);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(p);
    const unsigned char* const t = At(kPieces + 6 * n);
    Put<float>(p, 8, F(S16(x)));
    Put<float>(p, 0xC, F(S16(y)));
    p[0x14] = t[0];
    p[0x15] = t[1];
    Put<std::uint16_t>(p, 0x18, t[2]);
    Put<std::uint16_t>(p, 0x1A, t[3]);
    Put<std::uint16_t>(p, 0x16, static_cast<std::uint16_t>(((t[5] + 0x1E0u) << 6) | (t[4] >> 4)));
    Grey(p);
    Commit(0x1C);
}

// original 0x45ECC0: a small triangle pointing down at (x, y) - (x, y), (x - 4,
// y - 4), (x + 4, y - 4), low words signed - its green steady 0xFF when
// `steady` (its low byte) is set, else pulsing with Frame_Counter; red and
// blue 0. The primitive's header is 0x5A7570's (nobody's yet).
extern "C" void __cdecl CommuCursor_DrawArrow(int x, int y, unsigned steady) {
    const unsigned char green = (steady & 0xFF) != 0 ? 0xFF : Pulse();
    unsigned char* const p = Gfx_PacketNext;
    SH_AT(void (__cdecl*)(unsigned char*), rest_4e::at::kSetPolyF3)(p);
    const int sx = S16(x), sy = S16(y);
    Put<float>(p, 8, F(sx));
    Put<float>(p, 0xC, F(sy));
    Put<float>(p, 0x14, F(sx - 4));
    Put<float>(p, 0x18, F(sy - 4));
    Put<float>(p, 0x20, F(sx + 4));
    Put<float>(p, 0x24, F(sy - 4));
    p[4] = 0;
    p[5] = green;
    p[6] = 0;
    Commit(0x2C);
}

// original 0x45ED70: a name made at random - the 32 bytes 0x675F98 cleared,
// then script message 0x1F0 + (Rand & 0x3F) and message 0x230 + (Rand & 0x3F)
// copied after each other (a byte with bit 7 set copies two), a NUL after
// them; eax the length + 1 (the callers copy that many into Text_Records).
// The original does not bound the copy; ours aborts where it would pass the
// 32 bytes it cleared.
extern "C" unsigned __cdecl CommuName_MakeRandom(void) {
    std::memset(At(kEdit), 0, kEditSize);
    unsigned n = 0;
    auto copy = [&n](const unsigned char* s) {
        for (;;) {
            const unsigned char c = s[0];
            if (c == 0) return;
            const unsigned size = c & 0x80 ? 2 : 1;
            if (n + size >= kEditSize)
                bof3::Fatal("CommuName_MakeRandom (0x45ED70): the two halves pass the 32 bytes of 0x675F98 - the "
                            "original copies on (docs/rest_4e.md section 7)");
            std::memcpy(At(kEdit + n), s, size);
            n += size;
            s += size;
        }
    };
    const unsigned first = static_cast<unsigned>(SH_CALL(Rand)()) & 0x3F;
    copy(Message(0x1F0 + first));
    const unsigned second = static_cast<unsigned>(SH_CALL(Rand)()) & 0x3F;
    copy(Message(0x230 + second));
    B(kEdit + n) = 0;
    return n + 1;
}

// original 0x45EE10: a member's panel at (x, y) - a box 0x7D x 0x2D at (x + 3,
// y + 3) flags 0, the member's portrait (its record's +9) at (x + 6, y + 2),
// its name (or, `editing` set, the name being edited) at (x + 0x3B, y + 0x19),
// then the pieces of 0x652F5C at (x, y). `member` (its low byte) indexes
// CharacterRecords, unbounded: the panels are the seven records'.
extern "C" void __cdecl CommuMember_DrawPanel(int x, int y, unsigned member, unsigned editing) {
    Box(x + 3, y + 3, 0x7D, 0x2D, 0);
    const unsigned char* const record = At(kMembers + kMemberStride * (member & 0xFF));
    SH_CALL(CommuMember_DrawPortrait)(x + 6, y + 2, record[9], 0);
    const unsigned char* const name = (editing & 0xFF) == 0 ? record : At(kEdit);
    SH_CALL(Text_DrawAt)(x + 0x3B, y + 0x19, 0, 5, name);
    SH_CALL(Menu_DrawPieces)(x, y, At(kPanelPieces), 0);
}

// original 0x45EEB0: portrait `kind` (its low byte) of the image table 0x652F38
// (4 bytes: u, v, clut x, clut row) as a 0x28 x 0x30 SPRT at (x, y) - x and y
// their low words UNSIGNED (fild of the word zero-extended) - texture page
// 0x1E with dithering; shade 0 grey 0x80, 1 0x30, else (0x40, 0x40, 0x80).
// The table has nine (the pieces list 0x652F5C follows); past them the
// original reads on, and so does ours (a read only).
extern "C" void __cdecl CommuMember_DrawPortrait(int x, int y, unsigned kind, unsigned shade) {
    const unsigned n = kind & 0xFF;
    DrawMode(1, 0x1E, nullptr);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(p);
    const unsigned s = shade & 0xFF;
    if (s == 0) {
        p[6] = p[5] = p[4] = 0x80;
    } else if (s == 1) {
        p[6] = p[5] = p[4] = 0x30;
    } else {
        p[6] = 0x80;
        p[5] = p[4] = 0x40;
    }
    const unsigned char* const t = At(kPortraits + 4 * n);
    Put<float>(p, 8, F(static_cast<int>(static_cast<U>(x) & 0xFFFF)));
    Put<float>(p, 0xC, F(static_cast<int>(static_cast<U>(y) & 0xFFFF)));
    p[0x14] = t[0];
    p[0x15] = t[1];
    Put<std::uint16_t>(p, 0x18, 0x28);
    Put<std::uint16_t>(p, 0x1A, 0x30);
    Put<std::uint16_t>(p, 0x16, static_cast<std::uint16_t>(((t[3] + 0x1E0u) << 6) | (t[2] >> 4)));
    Commit(0x1C);
}

// original 0x45EF90: every present member's panel (bit 0 of the record's
// +0xB), the n-th present one at (0x20 + 0x88 (n & 1), 0x2B + 0x38 (n >> 1)).
// The original's x and y carry leftovers above their low words, which the
// panel reads alone.
extern "C" void __cdecl CommuMember_DrawAll(void) {
    unsigned shown = 0;
    for (unsigned i = 0; i < kMemberCount; ++i) {
        if ((B(kMembers + kMemberStride * i + 0xB) & 1) == 0) continue;
        const auto n = static_cast<unsigned char>(shown);
        SH_CALL(CommuMember_DrawPanel)(0x88 * (n & 1) + 0x20, 0x38 * (n >> 1) + 0x2B, i, 0);
        ++shown;
    }
}

// original 0x45F000: how many of the seven members are present (bit 0 of
// +0xB); al (the rest of eax the caller's).
extern "C" unsigned char __cdecl CommuMember_Count(void) {
    unsigned char n = 0;
    for (unsigned i = 0; i < kMemberCount; ++i)
        if (B(kMembers + kMemberStride * i + 0xB) & 1) ++n;
    return n;
}

// original 0x45F020: the index of the n-th present member (n its low byte,
// from 0); 0xFF when there are fewer.
extern "C" unsigned char __cdecl CommuMember_Nth(unsigned n) {
    const auto want = static_cast<unsigned char>(n);
    unsigned char seen = 0;
    for (unsigned i = 0; i < kMemberCount; ++i) {
        if ((B(kMembers + kMemberStride * i + 0xB) & 1) == 0) continue;
        if (seen == want) return static_cast<unsigned char>(i);
        ++seen;
    }
    return 0xFF;
}

// original 0x45F050: a member's selection frame at (x, y) (x + 2 and y their
// low words, unsigned): a LINE_F3 down the left corner and a LINE_F4 round the
// rest, (x' + 4, y') .. (x' + 0x7E, y' + 0x34) with x' = x + 2. Its colour:
// each of channels 0..2 (bits of `channels`) on at 0xFF, or pulsing when
// `pulse` (its low byte) is set, else 0 - bit 0 the blue byte, bit 2 the red.
// The original builds the colour in its own argument slot.
extern "C" void __cdecl CommuMember_DrawFrame(int x, int y, unsigned pulse, unsigned channels) {
    const unsigned char on = (pulse & 0xFF) != 0 ? Pulse() : 0xFF;
    unsigned char c[3];
    for (unsigned k = 0; k < 3; ++k) c[k] = (channels & 0xFF) & (1u << k) ? on : 0;
    const int px = static_cast<int>((static_cast<U>(x) + 2) & 0xFFFF);
    const int py = static_cast<int>(static_cast<U>(y) & 0xFFFF);
    const float left = F(px), left4 = F(px + 4), right = F(px + 0x7E);
    const float top = F(py), top4 = F(py + 4), bottom = F(py + 0x34);
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF3)(p);
    p[4] = c[2];
    p[5] = c[1];
    p[6] = c[0];
    Put<float>(p, 8, left4);
    Put<float>(p, 0xC, top);
    Put<float>(p, 0x14, left);
    Put<float>(p, 0x18, top4);
    Put<float>(p, 0x20, left);
    Put<float>(p, 0x24, bottom);
    Commit(0x2C);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF4)(p);
    p[4] = c[2];
    p[5] = c[1];
    p[6] = c[0];
    Put<float>(p, 8, left4);
    Put<float>(p, 0xC, top);
    Put<float>(p, 0x14, right);
    Put<float>(p, 0x18, top);
    Put<float>(p, 0x20, right);
    Put<float>(p, 0x24, bottom);
    Put<float>(p, 0x2C, left);
    Put<float>(p, 0x30, bottom);
    Commit(0x38);
}

// original 0x45F1A0: a frame of `w` x `h` cells of 8 (their low bytes) at (x,
// y) - five SPRTs of texture page 0x1D, each through its own texture window
// (a RECT on the original's stack: the top edge (0x70, 0x98), the bottom (0x98,
// 0xA0), the left (0xB0, 0x98), the right (0xB0, 0xA0), the middle (0x70,
// 0xA0), 8 x 8 each), clut Gpu_GetClut(0xB0, 0x1E1); the window put back to
// (0, 0, 0x100, 0x100); then the four corners (Menu_DrawPiece 0x23, 0x26, 0x27,
// 0x29). The SPRTs' sizes are 16-bit stores of 8 w - 0x20, 8 h - 0x20, 8 w -
// 0x10, 8 h - 0x10; the original computes two of them from a dword whose upper
// half is uninitialised stack, which the word stores drop.
extern "C" void __cdecl Commu_DrawTiledFrame(int x, int y, unsigned w, unsigned h) {
    const int sx = S16(static_cast<U>(x)), sy = S16(static_cast<U>(y));
    const int a = static_cast<int>(w & 0xFF), b = static_cast<int>(h & 0xFF);
    short window[4];
    auto mode = [&window](short wx, short wy, short ww, short wh) {
        window[0] = wx;
        window[1] = wy;
        window[2] = ww;
        window[3] = wh;
        DrawMode(0, 0x1D, window);
    };
    auto sprite = [](float fx, float fy, int sw, int sh) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetSprt)(p);
        Put<float>(p, 8, fx);
        Put<float>(p, 0xC, fy);
        p[0x14] = 0;
        p[0x15] = 0;
        Put<std::uint16_t>(p, 0x18, static_cast<std::uint16_t>(sw));
        Put<std::uint16_t>(p, 0x1A, static_cast<std::uint16_t>(sh));
        const unsigned clut = SH_CALL(Gpu_GetClut)(0xB0, 0x1E1);
        Put<std::uint16_t>(p, 0x16, static_cast<std::uint16_t>(clut));
        Grey(p);
        Commit(0x1C);
    };
    mode(0x70, 0x98, 8, 8);
    sprite(F(sx + 0x10), F(sy), 8 * a - 0x20, 8);
    mode(0x98, 0xA0, 8, 8);
    sprite(F(sx + 0x10), F(sy + 8 * b - 8), 8 * a - 0x20, 8);
    mode(0xB0, 0x98, 8, 8);
    sprite(F(sx), F(sy + 0x10), 8, 8 * b - 0x20);
    mode(0xB0, 0xA0, 8, 8);
    sprite(F(sx + 8 * a - 8), F(sy + 0x10), 8, 8 * b - 0x20);
    mode(0x70, 0xA0, 8, 8);
    sprite(F(sx + 8), F(sy + 8), 8 * a - 0x10, 8 * b - 0x10);
    mode(0, 0, 0x100, 0x100);
    const int right = x + 8 * a - 0x10, bottom = y + 8 * b - 0x10;
    SH_CALL(Menu_DrawPiece)(x, y, 0x23, 1);
    SH_CALL(Menu_DrawPiece)(right, y, 0x26, 1);
    SH_CALL(Menu_DrawPiece)(x, bottom, 0x27, 1);
    SH_CALL(Menu_DrawPiece)(right, bottom, 0x29, 1);
}

// ===========================================================================
// The two name commits
// ===========================================================================

// original 0x45F5A0 (R4D's 0x45E130 and 0x45E520 jmp to it): the edited name
// into the member the cursor 0x675F8C names (CommuMember_Nth, asked again for
// each byte) - its old nine bytes first into Text_Records 0 (+0, +4, +8) and
// the new eight beside them (+0x20, +0x24), then the eight bytes 0x675F98..
// into the record's name; message 0xF5, Field_Request 2, the mode 3, the step
// and 0x939A3F 0. No member (0xFF): the original writes 0xA4 x 255 past the
// records; ours aborts.
extern "C" void __cdecl CommuName_CommitMember(void) {
    auto member = []() -> unsigned char* {
        const unsigned i = SH_CALL(CommuMember_Nth)(B(kCursor)) & 0xFF;
        if (i >= kMemberCount)
            bof3::Fatal("CommuName_CommitMember (0x45F5A0): CommuMember_Nth answered %u, past the seven records - "
                        "the original writes past them (docs/rest_4e.md section 7)",
                        i);
        return At(kMembers + kMemberStride * i);
    };
    const unsigned char* const old = member();
    SetL(kTextRecords, Get<U>(old, 0));
    SetL(kTextRecords + 0x20, L(kEdit));
    SetL(kTextRecords + 4, Get<U>(old, 4));
    SetL(kTextRecords + 0x24, L(kEdit + 4));
    B(kTextRecords + 8) = old[8];
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const record = member();
        record[k] = B(kEdit + k);
    }
    OpenMessage(0xF5);
    Field_Request = 2;
    B(kMode) = 3;
    B(kStep) = 0;
    B(kNameMode) = 0;
}

// original 0x45F650 (R4D's 0x45D5B0 and 0x45D990 jmp to it): the edited name
// into the entry the cursor 0x675F8C names (R4D's 0x45E6D0, asked again for
// each byte). Text_Records 0: the entry's five name bytes with its trailing
// spaces as NULs, a NUL at +5, the eight edited bytes at +0x20; then the five
// bytes 0x675F98.. into the entry's name, a NUL as a space; message 0xF5,
// Field_Request 2, the mode 3, the step and 0x939A3F 0. No entry (0xFF): the
// original writes 5 x 255 past the names; ours aborts.
extern "C" void __cdecl CommuName_CommitEntry(void) {
    auto name = []() -> unsigned char* {
        const unsigned i = SH_AT(unsigned char (__cdecl*)(unsigned), rest_4e::at::kEntryNth)(B(kCursor)) & 0xFF;
        if (i >= kEntryCount)
            bof3::Fatal("CommuName_CommitEntry (0x45F650): 0x45E6D0 answered %u, past the 60 entries - the "
                        "original writes past their names (docs/rest_4e.md section 7)",
                        i);
        return At(kEntryNames + 5 * i);
    };
    bool kept = false;
    for (int k = 4; k >= 0; --k) {
        const unsigned char c = name()[k];
        if (c == 0x20 && !kept) {
            B(kTextRecords + k) = 0;
        } else {
            const unsigned char d = name()[k];
            kept = true;
            B(kTextRecords + k) = d;
        }
    }
    B(kTextRecords + 5) = 0;
    SetL(kTextRecords + 0x20, L(kEdit));
    SetL(kTextRecords + 0x24, L(kEdit + 4));
    for (unsigned k = 0; k < 5; ++k) {
        if (B(kEdit + k) == 0) {
            name()[k] = 0x20;
        } else {
            unsigned char* const n = name();
            n[k] = B(kEdit + k);
        }
    }
    OpenMessage(0xF5);
    Field_Request = 2;
    B(kMode) = 3;
    B(kStep) = 0;
    B(kNameMode) = 0;
}

// ===========================================================================
// The track list (CommuMusic_Modes by the mode 0x939A3E)
// ===========================================================================

// original 0x45F750: 0x652A70[11] - jmp through CommuMusic_Modes by the mode
// 0x939A3E, unchecked (ours aborts past its 3).
extern "C" void __cdecl CommuMusic_Dispatch(void) {
    Run("CommuMusic_Dispatch (0x45F750)", Key(CommuMusic_Modes), CommuMusic_Modes_count, B(kMode),
        "the mode 0x939A3E");
}

// original 0x45F760: CommuMusic_Modes[0] - jmp through CommuMusic_OpenSteps by
// the step 0x939A40, unchecked (ours aborts past its 2).
extern "C" void __cdecl CommuMusic_OpenDispatch(void) {
    Run("CommuMusic_OpenDispatch (0x45F760)", Key(CommuMusic_OpenSteps), CommuMusic_OpenSteps_count,
        B(kStep), "the step 0x939A40");
}

// original 0x45F770: CommuMusic_OpenSteps[0] - the seven members' nine name
// bytes into Text_Records 0..6 (+0, +4, +8), Music_Track kept at 0x675FD0, the
// row 0, the slide 4, the step + 1.
extern "C" void __cdecl CommuMusic_Open(void) {
    for (unsigned i = 0; i < kMemberCount; ++i) {
        const unsigned char* const record = At(kMembers + kMemberStride * i);
        unsigned char* const text = At(kTextRecords + 0x20 * i);
        Put<U>(text, 0, Get<U>(record, 0));
        Put<U>(text, 4, Get<U>(record, 4));
        text[8] = record[8];
    }
    B(kSavedTrack) = Music_Track;
    SetW(kTrack, 0);
    B(kSlide) = 4;
    ++B(kStep);
}

// original 0x45F7D0: CommuMusic_OpenSteps[1] - the slide down one, the list
// drawn at (0x32, 0x28 - 50 slide); at 0 the step 0 and the mode + 1. The
// original's y carries leftovers above its low word, which the list reads
// alone.
extern "C" void __cdecl CommuMusic_SlideIn(void) {
    const auto slide = static_cast<unsigned char>(B(kSlide) - 1);
    B(kSlide) = slide;
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28 - 50 * slide);
    if (B(kSlide) == 0) {
        const unsigned char mode = B(kMode);
        B(kStep) = 0;
        B(kMode) = static_cast<unsigned char>(mode + 1);
    }
}

// original 0x45F820: CommuMusic_Modes[1] - jmp through CommuMusic_BrowseSteps
// by the step 0x939A40, unchecked (ours aborts past its 2).
extern "C" void __cdecl CommuMusic_BrowseDispatch(void) {
    Run("CommuMusic_BrowseDispatch (0x45F820)", Key(CommuMusic_BrowseSteps), CommuMusic_BrowseSteps_count,
        B(kStep), "the step 0x939A40");
}

// The list's row, an index of the 40 tracks 0x653098 read signed; outside
// them the original reads the bytes there, and so does ours (a read only).
U TrackIndex() { return static_cast<U>(static_cast<int>(static_cast<short>(W(kTrack)))); }

// original 0x45F830: CommuMusic_BrowseSteps[0] - the pad: Input_AutoRepeat of
// the pressed word & 0xA000 moves the row (down 0x2000 past the count wraps to
// 0, up 0x8000 below 0 to the count; the count the byte 0x9048AB + 8 kind, the
// kind the entry's +1); nothing moved, Input_Pressed is read again: 0x20 stops
// a playing track (Music_FadeOutStop(10)) and, the row's track the one playing,
// leaves Music_Track 0xFF, else the step + 1 (play it); 0x40 the step 0 and the
// mode + 1 (close). The list drawn at (0x32, 0x28) on every path.
extern "C" void __cdecl CommuMusic_Browse(void) {
    const unsigned pressed = Input_Pressed & 0xA000u;
    const unsigned char count = B(kTrackCounts + 8u * B(kEntries + 1 + 8u * B(kEntry)));
    const unsigned moved = SH_CALL(Input_AutoRepeat)(pressed);
    if ((moved & 0xFFFF) != 0) {
        if (moved & 0x2000) {
            const auto row = static_cast<std::uint16_t>(W(kTrack) + 1);
            SetW(kTrack, row);
            if (static_cast<short>(row) > static_cast<short>(count)) {
                SetW(kTrack, 0);
                SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
                return;
            }
        } else if (moved & 0x8000) {
            const auto row = static_cast<std::uint16_t>(W(kTrack) - 1);
            SetW(kTrack, row);
            if (static_cast<short>(row) < 0) {
                SetW(kTrack, count);
                SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
                return;
            }
        }
    } else {
        const unsigned now = Input_Pressed & 0xFF;
        if (now & 0x20) {
            if (Music_Track != 0xFF) SH_CALL(Music_FadeOutStop)(10);
            const unsigned track = B(kTracks + TrackIndex());
            if (Music_Track == track) {
                Music_Track = 0xFF;
            } else {
                ++B(kStep);
            }
            SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
            return;
        }
        if (now & 0x40) {
            const unsigned char mode = B(kMode);
            B(kStep) = 0;
            B(kMode) = static_cast<unsigned char>(mode + 1);
        }
    }
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
}

// original 0x45F950: CommuMusic_BrowseSteps[1] - once File_LoadDone: the row's
// track played (Music_Play(track, 8)) and the step 0; the list drawn.
extern "C" void __cdecl CommuMusic_Play(void) {
    if (SH_CALL(File_LoadDone)() != 0) {
        const unsigned track = B(kTracks + TrackIndex());
        SH_CALL(Music_Play)(track, 8);
        B(kStep) = 0;
    }
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
}

// original 0x45F990: CommuMusic_Modes[2] - jmp through CommuMusic_CloseSteps by
// the step 0x939A40, unchecked (ours aborts past its 4).
extern "C" void __cdecl CommuMusic_CloseDispatch(void) {
    Run("CommuMusic_CloseDispatch (0x45F990)", Key(CommuMusic_CloseSteps), CommuMusic_CloseSteps_count,
        B(kStep), "the step 0x939A40");
}

// original 0x45F9A0: CommuMusic_CloseSteps[0] - a track playing that is not the
// one kept: Music_FadeOutStop(10); the list drawn; the step + 1.
extern "C" void __cdecl CommuMusic_CloseFade(void) {
    const unsigned char track = Music_Track;
    if (track != 0xFF && track != B(kSavedTrack)) SH_CALL(Music_FadeOutStop)(10);
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
    ++B(kStep);
}

// original 0x45F9E0: CommuMusic_CloseSteps[1] - once File_LoadDone: the kept
// track played again (Music_Play(kept, 8)), the step + 1, the slide 0; the list
// drawn.
extern "C" void __cdecl CommuMusic_CloseRestore(void) {
    if (SH_CALL(File_LoadDone)() != 0) {
        SH_CALL(Music_Play)(B(kSavedTrack), 8);
        const unsigned char step = B(kStep);
        B(kSlide) = 0;
        B(kStep) = static_cast<unsigned char>(step + 1);
    }
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28);
}

// original 0x45FA20: CommuMusic_CloseSteps[2] - the slide up one, the list
// drawn at (0x32, 0x28 - 50 slide); at 4 message 0x5A, the step + 1,
// Field_Request 2 (CommuMusic_CloseSteps[3] is R4D's 0x45E6A0).
extern "C" void __cdecl CommuMusic_SlideOut(void) {
    const auto slide = static_cast<unsigned char>(B(kSlide) + 1);
    B(kSlide) = slide;
    SH_CALL(CommuMusic_DrawList)(0x32, 0x28 - 50 * slide);
    if (B(kSlide) == 4) {
        OpenMessage(0x5A);
        const unsigned char step = B(kStep);
        Field_Request = 2;
        B(kStep) = static_cast<unsigned char>(step + 1);
    }
}

// original 0x45FA80: the track list at (x, y): a box 0xDC x 0x6E flags 0xF0,
// three notched outlines, the row's title (script message 0x271 + row) at (x +
// 0x14, y + 0x17) in colour 2 when its track is the one playing, else 0; a box
// and outline 0x7C x 0xE at (x + 0x48, y + 0x3C); two labels of the font quads
// (CommuMusic_DrawLabel; the first by 0x903A59) and four Text_DrawFont8 strings
// at y + 0x54; then two POLY_FT4s at (x + 0x49, y + 0x3F + 4 k), whose width
// the original adds from two zeroed ints of its stack - 0: both quads are a
// line (docs/rest_4e.md section 7, L3).
extern "C" void __cdecl CommuMusic_DrawList(int x, int y) {
    Box(x, y, 0xDC, 0x6E, 0xF0);
    SH_CALL(Menu_DrawOutlineNotched)(x + 4, y + 4, 0xD2, 0x66, 0);
    SH_CALL(Menu_DrawOutlineNotched)(x + 8, y + 8, 0xCA, 0x5E, 0);
    SH_CALL(Menu_DrawOutlineNotched)(x + 0x10, y + 0x12, 0xBC, 0x22, 1);
    const U row = TrackIndex();
    const int colour = Music_Track == B(kTracks + row) ? 2 : 0;
    SH_CALL(Text_DrawAt)(x + 0x14, y + 0x17, colour, 0xFF, At(kScriptPool + W(kScriptPool + 2 * (0x271 + row))));
    Box(x + 0x48, y + 0x3C, 0x7C, 0xE, 1);
    SH_CALL(Menu_DrawOutlineNotched)(x + 0x48, y + 0x3C, 0x7C, 0xE, 1);
    const U label = B(kFontPage) == 0 ? L(kLabelA) : L(kLabelB);
    SH_CALL(CommuMusic_DrawLabel)(x + 0x10, y + 0x40, 0, At(label));
    SH_CALL(CommuMusic_DrawLabel)(x + 0x20, y + 0x54, 0, At(L(kLabelC)));
    SH_CALL(Text_DrawFont8)(x + 0x44, y + 0x54, 0, At(kGlyphsC));
    SH_CALL(Text_DrawFont8)(x + 0x5C, y + 0x54, 3, At(kGlyphsB));
    SH_CALL(Text_DrawFont8)(x + 0x94, y + 0x54, 0, At(kGlyphsC));
    SH_CALL(Text_DrawFont8)(x + 0xAC, y + 0x54, 3, At(kGlyphsA));
    const float left = F(S16(static_cast<U>(x)) + 0x49);
    const int widths[2] = {0, 0};
    int top = S16(static_cast<U>(y)) + 0x3F;
    for (unsigned k = 0; k < 2; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        const float right = F(widths[k]) + left;
        Put<float>(p, 8, left);
        Put<float>(p, 0x28, left);
        Grey(p);
        p[0x14] = 0x98;
        Put<float>(p, 0x18, right);
        Put<float>(p, 0x38, right);
        const float fy = F(top);
        p[0x24] = 0x9C;
        p[0x34] = 0x98;
        p[0x44] = 0x9C;
        p[0x15] = 0xD8;
        Put<float>(p, 0xC, fy);
        Put<float>(p, 0x1C, fy);
        const float fy4 = fy + Get<float>(At(kFour), 0);
        p[0x25] = 0xD8;
        p[0x35] = 0xE0;
        p[0x45] = 0xE0;
        Put<std::uint16_t>(p, 0x16, 0x780B);
        Put<std::uint16_t>(p, 0x26, 0xF);
        Put<float>(p, 0x2C, fy4);
        Put<float>(p, 0x3C, fy4);
        Commit(0x48);
        top += 4;
    }
}

// original 0x45FCE0: a label in the 8 x 8 font quads at (x, y): each byte of
// `text` to its NUL a POLY_FT4 8 x 8 slanted 4 to the left at the bottom
// ((x + 4, y), (x + 0xC, y), (x, y + 8), (x + 8, y + 8)), u = ((c - 0x20) %
// 32) 8, v = ((c - 0x20) / 32) 8 (C's signed division), clut 0x7800 | colour &
// 0x3F, texture page 0xF; a space draws nothing; each next byte 8 to the right
// (x read again as its low word, signed, each time).
extern "C" void __cdecl CommuMusic_DrawLabel(int x, int y, unsigned colour, const unsigned char* text) {
    const auto clut = static_cast<std::uint16_t>(0x7800 | (colour & 0x3F));
    const float four = Get<float>(At(kFour), 0), eight = Get<float>(At(kEight), 0);
    for (int pen = x; text[0] != 0; ++text, pen += 8) {
        if (text[0] == 0x20) continue;
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        const unsigned char c = text[0];
        const float fx = F(S16(static_cast<U>(pen)) + 4);
        Put<float>(p, 8, fx);
        const float fx8 = fx + eight;
        Put<float>(p, 0x18, fx8);
        Put<float>(p, 0x28, fx - four);
        Put<float>(p, 0x38, fx8 - four);
        const float fy = F(S16(static_cast<U>(y)));
        Put<float>(p, 0xC, fy);
        Put<float>(p, 0x1C, fy);
        Put<float>(p, 0x2C, fy + eight);
        Put<float>(p, 0x3C, fy + eight);
        const int code = static_cast<int>(c) - 0x20;
        const auto u = static_cast<unsigned char>((code % 32) << 3);
        const auto v = static_cast<unsigned char>((code / 32) << 3);
        p[0x14] = u;
        p[0x24] = static_cast<unsigned char>(u + 7);
        p[0x34] = u;
        p[0x44] = static_cast<unsigned char>(u + 7);
        p[0x15] = v;
        p[0x25] = v;
        p[0x35] = static_cast<unsigned char>(v + 8);
        p[0x45] = static_cast<unsigned char>(v + 8);
        Grey(p);
        Put<std::uint16_t>(p, 0x16, clut);
        Put<std::uint16_t>(p, 0x26, 0xF);
        Commit(0x48);
    }
}

// ===========================================================================
// The item handed to an entry (CommuItem_Modes by the mode 0x939A3E)
// ===========================================================================

// original 0x45FE00: 0x652A70[18] - call through CommuItem_Modes by the mode
// 0x939A3E, unchecked (ours aborts past its 2); then, Field_Request 0 (read
// after the call), Field_RunTaskRecords (a tail jmp).
extern "C" void __cdecl CommuItem_Frame(void) {
    Run("CommuItem_Frame (0x45FE00)", Key(CommuItem_Modes), CommuItem_Modes_count, B(kMode),
        "the mode 0x939A3E");
    if (Field_Request == 0) SH_CALL(Field_RunTaskRecords)();
}

// original 0x45FE20: CommuItem_Modes[0] - jmp through CommuItem_Steps by the
// step 0x939A40, unchecked (ours aborts past its 7).
extern "C" void __cdecl CommuItem_StepDispatch(void) {
    Run("CommuItem_StepDispatch (0x45FE20)", Key(CommuItem_Steps), CommuItem_Steps_count, B(kStep),
        "the step 0x939A40");
}

// original 0x45FE30: CommuItem_Steps[0] - no message open: message 0x60, the
// step + 1, Field_Request 2.
extern "C" void __cdecl CommuItem_Prompt(void) {
    if (Field_Request == 2) return;
    OpenMessage(0x60);
    const unsigned char step = B(kStep);
    Field_Request = 2;
    B(kStep) = static_cast<unsigned char>(step + 1);
}

// original 0x45FE60: CommuItem_Steps[1] - no message open: Window_ResetAll, the
// item window set up (CommuItem_SetupWindow), the slide 4, the step + 1.
extern "C" void __cdecl CommuItem_OpenWindow(void) {
    if (Field_Request == 2) return;
    SH_CALL(Window_ResetAll)();
    SH_CALL(CommuItem_SetupWindow)();
    const unsigned char step = B(kStep);
    B(kItemSlide) = 4;
    B(kStep) = static_cast<unsigned char>(step + 1);
}

// The item window's frame at its slide: a box 0x118 x 0x13 flags 0xF2 at (0x14,
// 0x12 - 12 slide) and its outline 0x113 x 0xF at (0x16, 0x14 - 12 slide), the
// slide read again for the outline (the original's y carries leftovers above
// its low word).
void ItemFrame(unsigned char slide) {
    Box(0x14, 0x12 - 12 * slide, 0x118, 0x13, 0xF2);
    SH_CALL(Menu_DrawOutlineNotched)(0x16, 0x14 - 12 * B(kItemSlide), 0x113, 0xF, 0);
}

// Inventory_IdLists[tab]: five lists (0..3 by tab, then the key items); past
// them the original reads a pointer out of the table after.
const unsigned char* ItemList(const char* who, unsigned tab) {
    if (tab >= 5)
        bof3::Fatal("%s: the tab 0x80316A is %u, past the five of Inventory_IdLists (docs/rest_4e.md section 7)", who,
                    tab);
    return At(L(bof3::addr::Inventory_IdLists + 4 * tab));
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
    unsigned char slide = B(kItemSlide);
    if (slide != 0) B(kItemSlide) = --slide;
    ItemFrame(slide);
    if (B(kWinOff) != 0) return;
    const U window_x = L(kWin0 + 4);
    B(kWin1) = 1;
    SetW(kWin1 + 4, window_x + 7);
    SetW(kWin1 + 6, 13u * (static_cast<U>(B(kRow)) - B(kTop) + 2u) + W(kWin0 + 6));
    const unsigned moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF00Cu);
    if (moved & 0x8000) {
        SH_CALL(Sound_PlayEffect)(0x101);
        const auto tab = static_cast<unsigned char>(B(kTab) - 1);
        SetW(kTabMove, 0x32);
        B(kTab) = tab;
        if (static_cast<signed char>(tab) < 0) B(kTab) = 3;
    } else if (moved & 0x2000) {
        SetW(kTabMove, 0x31);
        SH_CALL(Sound_PlayEffect)(0x101);
        const auto tab = static_cast<unsigned char>(B(kTab) + 1);
        B(kTab) = tab;
        if (tab > 3) B(kTab) = 0;
    }
    unsigned char row = B(kRow);
    const unsigned char was = row;
    if (moved & 0x1000) {
        if (row != 0) B(kRow) = --row;
        if (row < B(kTop)) SetW(kScroll, 0xF0);
    } else if (moved & 0x4000) {
        if (row < 0x7F) B(kRow) = ++row;
        if (static_cast<int>(B(kRow)) >= static_cast<int>(B(kTop)) + 9) SetW(kScroll, 0x10);
    } else if (moved & 4) {
        const unsigned char top = B(kTop);
        if (top == 0) {
            row = 0;
            B(kRow) = 0;
        } else if (top < 9) {
            row = static_cast<unsigned char>(row - top);
            B(kTop) = 0;
            B(kRow) = row;
        } else {
            row = static_cast<unsigned char>(row - 9);
            B(kRow) = row;
            B(kTop) = static_cast<unsigned char>(top - 9);
        }
    } else if (moved & 8) {
        const unsigned char top = B(kTop);
        if (top == 0x77) {
            row = 0x7F;
            B(kRow) = row;
        } else if (top > 0x6E) {
            B(kTop) = 0x77;
            row = static_cast<unsigned char>(row + (0x77 - top));
            B(kRow) = row;
        } else {
            row = static_cast<unsigned char>(row + 9);
            B(kRow) = row;
            B(kTop) = static_cast<unsigned char>(top + 9);
        }
    }
    if (was != row) SH_CALL(Sound_PlayEffect)(0x100);
    if (W(kScroll) != 0) return;
    const unsigned pressed = Input_Pressed;
    if (Field_CancelButtons & pressed) {
        B(kWin1) = 0;
        B(kWinOff) = 1;
        SH_CALL(Sound_PlayEffect)(0x106);
        ++B(kStep);
    } else if (Field_ConfirmButtons & pressed) {
        const unsigned char item = ItemList("CommuItem_Choose (0x45FE90)", B(kTab))[B(kRow)];
        if (item == 0) {
            SH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        SH_CALL(Sound_PlayEffect)(0x105);
        const unsigned char tab = B(kTab);
        unsigned char* const entry = EntryRecord("CommuItem_Choose (0x45FE90)");
        B(kWinOff) = 1;
        entry[2] = item;
        unsigned char* const again = EntryRecord("CommuItem_Choose (0x45FE90)");
        B(kWin1) = 0;
        B(kStep) = 4;
        again[3] = tab;
    }
    const unsigned tab = B(kTab);
    const unsigned char shown = ItemList("CommuItem_Choose (0x45FE90)", tab)[B(kRow)];
    if (shown == 0) return;
    const unsigned help = SH_CALL(Item_HelpMessage)(tab, shown);
    const unsigned char* const line = SH_CALL(Msg_SystemPtr)(help);
    SH_CALL(Text_DrawAt)(0x1D, 0x15, 0, 0xFF, line);
}

// The item window's slide up one (to 4), its frame, and whether window 0 has
// shut (+3 0).
bool ItemClosed() {
    unsigned char slide = B(kItemSlide);
    if (slide < 4) B(kItemSlide) = ++slide;
    ItemFrame(slide);
    return B(kWinOff) == 0;
}

// original 0x4601D0: CommuItem_Steps[3] (cancelled) - the window slides away;
// shut: Window_ResetAll, message 0x5F, the mode + 1, Field_Request 2.
extern "C" void __cdecl CommuItem_Cancel(void) {
    if (!ItemClosed()) return;
    SH_CALL(Window_ResetAll)();
    OpenMessage(0x5F);
    const unsigned char mode = B(kMode);
    Field_Request = 2;
    B(kMode) = static_cast<unsigned char>(mode + 1);
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
    const unsigned char* const entry = At(kEntries + 8u * B(kEntry));
    const unsigned char tab = entry[3];
    const unsigned item = entry[2];
    unsigned char kind;
    switch (tab & 0xF) {
    case 0: kind = B(bof3::addr::NameTable_Consumables + 0x10 + 22 * item); break;
    case 1: kind = B(bof3::addr::NameTable_Weapons + 0x11 + 28 * item); break;
    case 2: kind = B(bof3::addr::NameTable_Armour + 0x11 + 26 * item); break;
    default: kind = B(bof3::addr::NameTable_Accessories + 0x11 + 24 * item); break;
    }
    if (kind & 8) {
        OpenMessage(0xEB);
        B(kStep) = 6;
        Field_Request = 2;
        return;
    }
    const unsigned char* const name = SH_CALL(Item_NamePtr)(tab, item);
    std::memcpy(At(kTextRecords), name, 16);
    OpenMessage(0x61);
    const unsigned char step = B(kStep);
    B(kItemSlide) = 0;
    B(kStep) = static_cast<unsigned char>(step + 1);
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
    Put<U>(EntryRecord("CommuItem_Give (0x4603F0)"), 4, L(kEntryDword));
    ++B(kMode);
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
    B(kWin0 + 1) = 2;
    B(kWin0 + 2) = 0;
    B(kWin0 + 3) = 2;
    B(kWin0) = 1;
    SetW(kWin0 + 4, 0xFF56);
    SetW(kWin0 + 6, 0x3E);
    B(kWin0 + 0xA) = 0;
    B(kWin0 + 0xB) = 0;
    B(kWin0 + 0xC) = 0;
    B(kWin0 + 0xD) = 0xFF;
    B(kWin0 + 8) = 0;
    B(kWin0 + 9) = 0;
    SetW(kWin0 + 0x10, 0);
    SetW(kWin0 + 0x12, 0);
    B(kWin1 + 1) = 2;
    B(kWin1 + 2) = 1;
    B(kWin1 + 3) = 0;
    B(kWin1) = 0;
}

// ===========================================================================
// The ranked lists (CommuRank_Modes by the mode 0x939A3E)
// ===========================================================================

// original 0x460500: 0x652AE4[2] (R4B's 0x457510 jumps through it by
// 0x9039F4) - jmp through CommuRank_Modes by the mode 0x939A3E, unchecked
// (ours aborts past its 3).
extern "C" void __cdecl CommuRank_Dispatch(void) {
    Run("CommuRank_Dispatch (0x460500)", Key(CommuRank_Modes), CommuRank_Modes_count, B(kMode),
        "the mode 0x939A3E");
}

// original 0x460510: CommuRank_Modes[0] - each list's page count (0 for an
// empty list, else (n - 1) / per + 1: 7 of 0x9039A0, 20 of 0x904A90, 8 of
// 0x937F80), the page 0, the mode + 1.
extern "C" void __cdecl CommuRank_Pages(void) {
    auto pages = [](unsigned n, unsigned per) -> unsigned char {
        return n == 0 ? 0 : static_cast<unsigned char>((static_cast<int>(n) - 1) / static_cast<int>(per) + 1);
    };
    B(kPages) = pages(B(kListA), 7);
    B(kPages + 1) = pages(B(kListB), 20);
    B(kPage) = 0;
    B(kPages + 2) = pages(B(kListC), 8);
    ++B(kMode);
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
    unsigned char left = B(kPage);
    unsigned list = 0;
    for (;;) {
        if (list >= 3)
            bof3::Fatal("CommuRank_Show (0x4605D0): the page 0x675FDB is %u, past the three lists' %u + %u + %u pages "
                        "- the original reads on past 0x675FDA (docs/rest_4e.md section 7)",
                        B(kPage), B(kPages), B(kPages + 1), B(kPages + 2));
        const unsigned char count = B(kPages + list);
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
    const auto total = static_cast<unsigned char>(B(kPages + 2) + B(kPages + 1) + B(kPages));
    const unsigned pressed = Input_Pressed;
    if (pressed & 0x2000) {
        const auto page = static_cast<unsigned char>(B(kPage) + 1);
        B(kPage) = page;
        if (static_cast<signed char>(page) >= static_cast<int>(total)) B(kPage) = 0;
    }
    if (pressed & 0x8000) {
        const auto page = static_cast<unsigned char>(B(kPage) - 1);
        B(kPage) = page;
        if (static_cast<signed char>(page) < 0) B(kPage) = static_cast<unsigned char>(total - 1);
    }
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(kTextBuffer)), reinterpret_cast<const char*>(At(kPageFormat)),
                         static_cast<int>(static_cast<signed char>(B(kPage))) + 1, static_cast<unsigned>(total));
    SH_CALL(Text_DrawFont8)(0xEE, 0x2C, 0, At(kTextBuffer));
    if (Input_Pressed & (Field_ConfirmButtons | Field_CancelButtons)) ++B(kMode);
}

// original 0x460720: CommuRank_Modes[2] - 0x9039F4 + 1 (the facility table
// 0x652AE4's next entry).
extern "C" void __cdecl CommuRank_Close(void) { ++B(kFacility); }

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
    SH_CALL(Text_DrawAt)(x + 0x18, y + 0x14, 0, 0xFF, Message(title));
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(kTextBuffer)),
                         reinterpret_cast<const char*>(At(Key(Boss26Fx_CountFormat))), B(count));
    SH_CALL(Text_DrawFont12)(x + 0x6C, y + 0x14, 0, At(kTextBuffer));
    if (words)
        SH_CALL(Text_DrawAt)(x + 0x8C, y + 0x14, 0, 0xFF, Labels_Slot(14, B(count) == 1 ? 2 : 1));   // a letter's gap after the count, as the US screen has
    else
        SH_CALL(Text_DrawAt)(x + 0x84, y + 0x14, 0, 1, At(kOneGlyph));
}

// original 0x460730: the first list's page `page` (its low byte) at (x, y):
// its heading (icon 1, message 0x38, the count 0x9039A0), then up to seven
// rows from 0x9039C0 + 7 page, each the name of the entry its low 7 bits name
// at (x + 0x14, y + 0x2C + 16 i) and message 0x3A (bit 7 set) or 0x39 at (x +
// 0x70, ...); the count read again each row.
extern "C" void __cdecl CommuRank_DrawListA(int x, int y, unsigned page) {
    ListHeading(x, y, 1, 0x38, kListA);
    const unsigned base = 7 * (page & 0xFF);
    for (unsigned i = 0; i < 7; ++i) {
        if (static_cast<int>(base + i) >= static_cast<int>(B(kListA))) break;
        const int row = 16 * static_cast<int>(i) + y + 0x2C;
        SH_CALL(Text_DrawAt)(x + 0x14, row, 0, 5, At(kEntryNames + 5 * (B(kListAEntries + base + i) & 0x7F)));
        const unsigned id = B(kListAEntries + base + i) & 0x80 ? 0x3A : 0x39;
        SH_CALL(Text_DrawAt)(x + 0x70, row, 0, 0xFF, Message(id));
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
    ListHeading(x, y, 0, 0x3B, kListB);
    const unsigned p = page & 0xFF;
    for (unsigned i = 0; i < 20; ++i) {
        if (static_cast<int>(20 * p + i) >= static_cast<int>(B(kListB))) break;
        const unsigned entry = B(kListBEntries + 15 * p + i);
        const int column = static_cast<int>(i % 3), line = static_cast<int>(i / 3);
        SH_CALL(Text_DrawAt)(x + 72 * column + 0x12, 16 * line + y + 0x2C, 0, 5, At(kEntryNames + 5 * entry));
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
        if (static_cast<int>(8 * p + i) >= static_cast<int>(B(kListC))) break;
        const unsigned char* const pair = At(kListCPairs + 16 * p + 2 * i);
        const unsigned kind = pair[0];
        if (W(kPairMessages + 4 * kind + 2) != 0) {
            B(kTextRecords + 5) = 0;
            const unsigned n = pair[1];
            SetL(kTextRecords, L(kEntryNames + 5 * n));
            B(kTextRecords + 4) = B(kEntryNames + 5 * n + 4);
        }
        SH_CALL(Text_DrawAt)(x + 0x14, 16 * static_cast<int>(i) + y + 0x1C, 0, 0xFF,
                             Message(W(kPairMessages + 4 * kind)));
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
        const unsigned char* const t = At(kTilesTop + 2 * i);
        SH_CALL(CommuRank_DrawTile)(x + 16 * static_cast<int>(i), y, t[0], t[1], 0);
    }
    for (int c = 0; c < 4; ++c) {
        const int cx = 64 * c + x;
        for (unsigned r = 0; r < 10; ++r) {
            const int ry = 16 * static_cast<int>(r + 1) + y;
            const unsigned char a = B(kTilesSide + 2 * r);
            SH_CALL(CommuRank_DrawTile)(cx, ry, static_cast<unsigned char>((a & 1) << 5),
                                        static_cast<unsigned char>((a & 0xFE) << 3), 1);
            const unsigned char b = B(kTilesSide + 2 * r + 1);
            SH_CALL(CommuRank_DrawTile)(cx + 0x20, ry, static_cast<unsigned char>((b & 1) << 5),
                                        static_cast<unsigned char>((b & 0xFE) << 3), 1);
        }
    }
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned char* const t = At(kTilesBottom + 2 * i);
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

void Rest4E_Inject() {
    if (bof3::WantsShadow("rest_4e")) rest_4e::SelfTest();
    BOF3_INJECT(CommuEntry_DrawPanel);
    BOF3_INJECT(CommuEntry_DrawPanelFrame);
    BOF3_INJECT(CommuEntry_DrawBar);
    BOF3_INJECT(Commu_DrawPiece6);
    BOF3_INJECT(CommuCursor_DrawArrow);
    BOF3_INJECT(CommuName_MakeRandom);
    BOF3_INJECT(CommuMember_DrawPanel);
    BOF3_INJECT(CommuMember_DrawPortrait);
    BOF3_INJECT(CommuMember_DrawAll);
    BOF3_INJECT(CommuMember_Count);
    BOF3_INJECT(CommuMember_Nth);
    BOF3_INJECT(CommuMember_DrawFrame);
    BOF3_INJECT(Commu_DrawTiledFrame);
    BOF3_INJECT(CommuName_CommitMember);
    BOF3_INJECT(CommuName_CommitEntry);
    BOF3_INJECT(CommuMusic_Dispatch);
    BOF3_INJECT(CommuMusic_OpenDispatch);
    BOF3_INJECT(CommuMusic_Open);
    BOF3_INJECT(CommuMusic_SlideIn);
    BOF3_INJECT(CommuMusic_BrowseDispatch);
    BOF3_INJECT(CommuMusic_Browse);
    BOF3_INJECT(CommuMusic_Play);
    BOF3_INJECT(CommuMusic_CloseDispatch);
    BOF3_INJECT(CommuMusic_CloseFade);
    BOF3_INJECT(CommuMusic_CloseRestore);
    BOF3_INJECT(CommuMusic_SlideOut);
    BOF3_INJECT(CommuMusic_DrawList);
    BOF3_INJECT(CommuMusic_DrawLabel);
    BOF3_INJECT(CommuItem_Frame);
    BOF3_INJECT(CommuItem_StepDispatch);
    BOF3_INJECT(CommuItem_Prompt);
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
