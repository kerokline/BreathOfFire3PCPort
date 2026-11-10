// Moved here by tools/move_funcs.py from rest_4d.cpp, rest_4e.cpp (docs/source-layout-round.md).
#include "game/community/commu.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/faerie_name.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/rdata_consts.h"
#include "game/rest_4d.h"
#include "game/rest_4d_callees.h"
#include "game/rest_4e.h"
#include "game/rest_4e_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_4d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Word;

unsigned char& B(U address) { return *At(address); }

const unsigned char* Text(U address) { return At(address); }

void ListPiece(int x, int y, unsigned piece) { SH_AT(void (__cdecl*)(int, int, unsigned), at::kListPiece.v)(x, y, piece); }

// A message's text: MessagePools' word at `words + 2 * index` from 0x803580.
const unsigned char* PoolText(U words, int index) {
    return Text(::bof3::Img(at::kPoolWords.v) + Word(At(words + 2u * static_cast<U>(index))));
}

using move_script::Long;

using move_script::SetLong;

// The band's own cells 0x675F8C..0x675FDB.
// kCursor: now Rest4c_Count (symbols.toml). u8: which name the commits write (the n-th in use)
constexpr ::bof3::image::Va kEdit{0x675F98};          // u8 x 32: the name being edited (CommuName_MakeRandom clears 32)

constexpr U kEditSize = 0x20;

constexpr ::bof3::image::Va kEntryNames{0x9048F0};

constexpr unsigned kEntryCount = 60;

// The seven character records (CharacterRecords 0x903A70, 0xA4 each; +0 the
// name, +9 the portrait, +0xB bit 0 present).
constexpr ::bof3::image::Va kMembers{0x903A70};

constexpr U kMemberStride = 0xA4;

constexpr unsigned kMemberCount = 7;

constexpr ::bof3::image::Va kTextRecords{0x904CE0};   // Text_Records (0x20 each)

constexpr ::bof3::image::Va kScriptPool{0x803580};    // Msg_OpenScript's pool: base + u16[base + 2 id]

// Image tables, read in place.
constexpr ::bof3::image::Va kPieces{0x652EE4};        // 6 bytes a piece: u, v, w, h, clut x, clut row

constexpr ::bof3::image::Va kPortraits{0x652F38};     // 4 bytes a portrait: u, v, clut x, clut row

constexpr ::bof3::image::Va kPanelPieces{0x652F5C};   // the member panel's Menu_DrawPieces list

constexpr ::bof3::image::Va kBars{0x653210};          // 20 bytes an entry: its four bars' lengths

unsigned char& BRest4e(U address) { return move_script::At(address)[0]; }

U W(U address) { return move_script::Word(move_script::At(address)); }

U L(U address) { return static_cast<U>(Long(move_script::At(address))); }

void SetL(U address, U v) { SetLong(move_script::At(address), static_cast<std::int32_t>(v)); }

template <class T> T Get(const unsigned char* p, unsigned offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof v);
    return v;
}

template <class T> void Put(unsigned char* p, unsigned offset, T v) { std::memcpy(p + offset, &v, sizeof v); }

float F(int v) { return static_cast<float>(v); }

int S16(U v) { return static_cast<short>(v); }

const unsigned char* Message(unsigned id) { return move_script::At(::bof3::Img(kScriptPool.v) + W(::bof3::Img(kScriptPool.v) + 2 * id)); }

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

// A colour that pulses with Frame_Counter's low byte: bit 3 set,
// ((n & 6) << 5) + 0x3F; clear, ((~n & 6) << 5) + 0x3F.
unsigned char Pulse() {
    const auto n = static_cast<unsigned char>(Frame_Counter);
    const unsigned two = n & 8 ? n & 6 : ~n & 6;
    return static_cast<unsigned char>((two << 5) + 0x3F);
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x45E6A0, the last state of CommuDraw_States, CommuName_States and
// two tables of other groups: once the message is closed, the community's game
// byte 0x9039F4 one on.
extern "C" void __cdecl Commu_LeaveWhenClosed(void) {
    if (Field_Request == 2) return;
    AreaW0c_TailState = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
}

// original 0x45E6B0: the 8-byte cells 0x9046D0..0x9048AF whose first byte is
// not 0, counted in al (the original clears al alone; callers read al).
extern "C" unsigned char __cdecl CommuName_CountSlots(void) {
    unsigned char n = 0;
    for (U cell = ::bof3::Img(Community_Records_addr); cell < ::bof3::Img(at::kSlotsEnd); cell += 8)
        if (B(cell) != 0) n = static_cast<unsigned char>(n + 1);
    return n;
}

// original 0x45E6D0: the index (0..59, in eax) of the cell in use whose count
// before it is `n`'s byte; 0xFF when there is none.
extern "C" unsigned __cdecl CommuName_NthSlot(unsigned n) {
    unsigned char seen = 0;
    unsigned index = 0;
    for (U cell = ::bof3::Img(Community_Records_addr); cell < ::bof3::Img(at::kSlotsEnd); cell += 8, ++index) {
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
    SH_CALL(Menu_DrawBox)(x, y, 0xAC, 0x1A, 0x80, static_cast<unsigned char>(Config_WindowColour));
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
    SH_CALL(Menu_DrawPanelBox)(0x14, 0x10, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour));
    const unsigned id = Rest4d_Header;
    if (id == 0xFFFF) return;
    SH_CALL(Text_DrawAt)(0x1B, 0x13, 0, 0xFF, PoolText(::bof3::Img(at::kPoolWords.v), static_cast<int>(id)));
}

#pragma clang attribute pop

// DIVERGENCE DIV-0200 (faerie_name.h): 1 through PatchBytes("FaerieNameEnd")
// in Rest4E_Inject, after the self-test. SlotName: entry e's name for a draw,
// NUL-ended at its last whole glyph when on, else the table itself; the
// buffer is the draw's, read before the next call.
namespace rest_4e { std::uint8_t g_faerie_name_end = 0; }

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
    // DIVERGENCE DIV-0200 (faerie_name.h): a slot's name NUL-ended at its last
    // whole glyph; the name being edited is the editor's own, NUL-ended.
    unsigned char ended[6];
    const unsigned char* name = (editing & 0xFF) == 0 ? At(::bof3::Img(kEntryNames.v) + 5 * e) : At(::bof3::Img(kEdit.v));
    if ((editing & 0xFF) == 0 && rest_4e::g_faerie_name_end) name = faerie_name::Ended(name, ended);
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
        SH_CALL(CommuEntry_DrawBar)(x + 6, y + 0x12 + 4 * static_cast<int>(k), 12 * BRest4e(::bof3::Img(kBars.v) + 20 * e + k), k);
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
    const unsigned char* const t = At(::bof3::Img(kPieces.v) + 6 * n);
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
    SH_AT(void (__cdecl*)(unsigned char*), rest_4e::at::kSetPolyF3.v)(p);
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
    std::memset(At(::bof3::Img(kEdit.v)), 0, kEditSize);
    unsigned n = 0;
    auto copy = [&n](const unsigned char* s) {
        for (;;) {
            const unsigned char c = s[0];
            if (c == 0) return;
            const unsigned size = c & 0x80 ? 2 : 1;
            if (n + size >= kEditSize)
                bof3::Fatal("CommuName_MakeRandom (0x45ED70): the two halves pass the 32 bytes of 0x675F98 - the "
                            "original copies on (docs/rest_4e.md section 7)");
            std::memcpy(At(::bof3::Img(kEdit.v) + n), s, size);
            n += size;
            s += size;
        }
    };
    const unsigned first = static_cast<unsigned>(SH_CALL(Rand)()) & 0x3F;
    copy(Message(0x1F0 + first));
    const unsigned second = static_cast<unsigned>(SH_CALL(Rand)()) & 0x3F;
    copy(Message(0x230 + second));
    BRest4e(::bof3::Img(kEdit.v) + n) = 0;
    return n + 1;
}

// original 0x45EE10: a member's panel at (x, y) - a box 0x7D x 0x2D at (x + 3,
// y + 3) flags 0, the member's portrait (its record's +9) at (x + 6, y + 2),
// its name (or, `editing` set, the name being edited) at (x + 0x3B, y + 0x19),
// then the pieces of 0x652F5C at (x, y). `member` (its low byte) indexes
// CharacterRecords, unbounded: the panels are the seven records'.
extern "C" void __cdecl CommuMember_DrawPanel(int x, int y, unsigned member, unsigned editing) {
    Box(x + 3, y + 3, 0x7D, 0x2D, 0);
    const unsigned char* const record = At(::bof3::Img(CharacterRecords_addr) + kMemberStride * (member & 0xFF));
    SH_CALL(CommuMember_DrawPortrait)(x + 6, y + 2, record[9], 0);
    const unsigned char* const name = (editing & 0xFF) == 0 ? record : At(::bof3::Img(kEdit.v));
    SH_CALL(Text_DrawAt)(x + 0x3B, y + 0x19, 0, 5, name);
    SH_CALL(Menu_DrawPieces)(x, y, At(::bof3::Img(kPanelPieces.v)), 0);
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
    const unsigned char* const t = At(::bof3::Img(kPortraits.v) + 4 * n);
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
        if ((BRest4e(::bof3::Img(kMembers.v) + kMemberStride * i + 0xB) & 1) == 0) continue;
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
        if (BRest4e(::bof3::Img(kMembers.v) + kMemberStride * i + 0xB) & 1) ++n;
    return n;
}

// original 0x45F020: the index of the n-th present member (n its low byte,
// from 0); 0xFF when there are fewer.
extern "C" unsigned char __cdecl CommuMember_Nth(unsigned n) {
    const auto want = static_cast<unsigned char>(n);
    unsigned char seen = 0;
    for (unsigned i = 0; i < kMemberCount; ++i) {
        if ((BRest4e(::bof3::Img(kMembers.v) + kMemberStride * i + 0xB) & 1) == 0) continue;
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

// original 0x45F5A0 (R4D's 0x45E130 and 0x45E520 jmp to it): the edited name
// into the member the cursor 0x675F8C names (CommuMember_Nth, asked again for
// each byte) - its old nine bytes first into Text_Records 0 (+0, +4, +8) and
// the new eight beside them (+0x20, +0x24), then the eight bytes 0x675F98..
// into the record's name; message 0xF5, Field_Request 2, the mode 3, the step
// and 0x939A3F 0. No member (0xFF): the original writes 0xA4 x 255 past the
// records; ours aborts.
extern "C" void __cdecl CommuName_CommitMember(void) {
    auto member = []() -> unsigned char* {
        const unsigned i = SH_CALL(CommuMember_Nth)(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
        if (i >= kMemberCount)
            bof3::Fatal("CommuName_CommitMember (0x45F5A0): CommuMember_Nth answered %u, past the seven records - "
                        "the original writes past them (docs/rest_4e.md section 7)",
                        i);
        return At(::bof3::Img(CharacterRecords_addr) + kMemberStride * i);
    };
    const unsigned char* const old = member();
    SetL(::bof3::Img(kTextRecords.v), Get<U>(old, 0));
    SetL(::bof3::Img(kTextRecords.v) + 0x20, L(::bof3::Img(kEdit.v)));
    SetL(::bof3::Img(kTextRecords.v) + 4, Get<U>(old, 4));
    SetL(::bof3::Img(kTextRecords.v) + 0x24, L(::bof3::Img(kEdit.v) + 4));
    BRest4e(::bof3::Img(kTextRecords.v) + 8) = old[8];
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const record = member();
        record[k] = BRest4e(::bof3::Img(kEdit.v) + k);
    }
    OpenMessage(0xF5);
    Field_Request = 2;
    Rest4b_BoardState = 3;
    Rest4b_BoardStep = 0;
    Rest4c_Step = 0;
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
        const unsigned i = SH_AT(unsigned char (__cdecl*)(unsigned), rest_4e::at::kEntryNth.v)(static_cast<unsigned char>(Rest4c_Count)) & 0xFF;
        if (i >= kEntryCount)
            bof3::Fatal("CommuName_CommitEntry (0x45F650): 0x45E6D0 answered %u, past the 60 entries - the "
                        "original writes past their names (docs/rest_4e.md section 7)",
                        i);
        return At(::bof3::Img(kEntryNames.v) + 5 * i);
    };
    bool kept = false;
    for (int k = 4; k >= 0; --k) {
        const unsigned char c = name()[k];
        if (c == 0x20 && !kept) {
            BRest4e(::bof3::Img(kTextRecords.v) + k) = 0;
        } else {
            const unsigned char d = name()[k];
            kept = true;
            BRest4e(::bof3::Img(kTextRecords.v) + k) = d;
        }
    }
    BRest4e(::bof3::Img(kTextRecords.v) + 5) = 0;
    SetL(::bof3::Img(kTextRecords.v) + 0x20, L(::bof3::Img(kEdit.v)));
    SetL(::bof3::Img(kTextRecords.v) + 0x24, L(::bof3::Img(kEdit.v) + 4));
    for (unsigned k = 0; k < 5; ++k) {
        if (BRest4e(::bof3::Img(kEdit.v) + k) == 0) {
            name()[k] = 0x20;
        } else {
            unsigned char* const n = name();
            n[k] = BRest4e(::bof3::Img(kEdit.v) + k);
        }
    }
    OpenMessage(0xF5);
    Field_Request = 2;
    Rest4b_BoardState = 3;
    Rest4b_BoardStep = 0;
    Rest4c_Step = 0;
}

void CommuFromRest4d_Inject() {
    BOF3_INJECT(Commu_LeaveWhenClosed);
    BOF3_INJECT(CommuName_CountSlots);
    BOF3_INJECT(CommuName_NthSlot);
    BOF3_INJECT(CommuName_DrawSlotBar);
    BOF3_INJECT(CommuName_DrawSlotFrame);
    BOF3_INJECT(CommuName_DrawHeader);
}

void CommuFromRest4e_Inject() {
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
    {
        // DIV-0200, after the self-test (which compares Capcom's draws).
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("FaerieNameEnd", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&rest_4e::g_faerie_name_end)), &was, &is, 1);
    }
}
