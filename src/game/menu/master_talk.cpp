// Moved here by tools/move_funcs.py from rest_2c.cpp (docs/source-layout-round.md).
#include "game/menu/master_talk.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2c.h"
#include "game/rest_2c_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_2c::at;

}  // namespace

namespace {

using U = std::uint32_t;

constexpr auto& AtRest2c = move_script::At;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& BRest2c(U address) { return *AtRest2c(address); }

void PutL(U address, U v) { SetLong(AtRest2c(address), static_cast<std::int32_t>(v)); }

void PutW(U address, unsigned v) { SetWord(AtRest2c(address), v); }

float F(U address) {
    float v;
    std::memcpy(&v, AtRest2c(address), sizeof v);
    return v;
}

void PutF(unsigned char* at, float v) { std::memcpy(at, &v, sizeof v); }

U Bits(float v) {
    U u;
    std::memcpy(&u, &v, sizeof u);
    return u;
}

U Key(const void* p) { return Low32(p); }

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

namespace {

// The panels at the slide 0x9398D3 (read again at each draw): per member
// (Field_MemberCount read again each turn) MasterPanel_DrawMember at (0x11 -
// 36 * slide, 0x3E + 54 * slot) and MasterPanel_DrawStats at (0x89 + 40 *
// slide, the same y), each of the member's record index; then the title box
// Menu_DrawPanelBox(0x14, 0x10 - 10 * slide, 0x118, 0x13, the window colour).
void Panels() {
    for (unsigned slot = 0; slot < Field_MemberCount; ++slot) {
        const unsigned char* const member = AtRest2c(::bof3::Img(ObjTrio_addr) + 0x148 + slot * rest_2c::at::kObjStride);
        const int y = static_cast<int>(54 * slot + 0x3E);
        SH_CALL(MasterPanel_DrawMember)(0x11 - 36 * Rest2c_TalkSlide, y, member[0], slot);
        SH_CALL(MasterPanel_DrawStats)(0x89 + 40 * Rest2c_TalkSlide, y, member[0]);
    }
    SH_CALL(Menu_DrawPanelBox)(0x14, 0x10 - 10 * Rest2c_TalkSlide, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour));
}

// One of Menu_DrawPanelBox's four edge quads: Gpu_SetPolyFT4 and
// Gpu_SetSemiTrans(1) at the packet cursor (read again), the corners given as
// float bits and the texture bytes, colour 0xAC grey, the CLUT
// Gpu_GetClut(clut_x, 0x1E1), page 0xF, committed as (1, 0x48).
struct Quad {
    U x[4], y[4];
    unsigned char u[4], v[4];
};

void EdgeQuad(const Quad& q, int clut_x) {
    unsigned char* const p = Gfx_PacketNext.Native();
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    for (unsigned i = 0; i < 4; ++i) {
        std::memcpy(p + 8 + 0x10 * i, &q.x[i], 4);
        std::memcpy(p + 0xC + 0x10 * i, &q.y[i], 4);
        p[0x14 + 0x10 * i] = q.u[i];
        p[0x15 + 0x10 * i] = q.v[i];
    }
    p[4] = 0xAC;
    p[5] = 0xAC;
    p[6] = 0xAC;
    const unsigned clut = SH_CALL(Gpu_GetClut)(clut_x, 0x1E1);
    PutW(Key(p + 0x16), clut);
    PutW(Key(p + 0x26), 0xF);
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// A draw mode with a texture window RECT of its own: 8 bytes taken at the
// packet cursor (x 0, y, w, h), then Gpu_SetDrawMode(cursor, 0, 1, 0xF, the
// RECT) and Gfx_CommitPrim(1, 0xC).
void WindowMode(unsigned y, unsigned size) {
    unsigned char* const rect = Gfx_PacketNext.Native();
    PutL(::bof3::Img(rest_2c::at::kPacketNext.v), Key(rect + 8));
    PutW(Key(rect), 0);
    PutW(Key(rect + 2), y);
    PutW(Key(rect + 4), size);
    PutW(Key(rect + 6), size);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext.Native(), 0, 1, 0xF, Key(rect));
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
}

}  // namespace

#pragma clang attribute pop

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x585A00 (FieldTail_LoadBank's step 1): the talk's mode, step,
// 0x9398D0, 0x9398CD and the slide 0.
extern "C" void __cdecl MasterTalk_Reset(void) {
    AreaW0c_AnswerMark = 0;
    AreaW3f_AnswerMarkD1 = 0;
    Rest2c_TalkD0 = 0;
    Rest2c_TalkCD = 0;
    Rest2c_TalkSlide = 0;
}

// original 0x585A20, pick step 0 (and R2D's mode 4's): Sound_PlayEffect(0x102),
// the slide 4, the step up (read after the sound).
extern "C" void __cdecl MasterTalk_PanelsOpen(void) {
    SH_CALL(Sound_PlayEffect)(0x102);
    const auto step = static_cast<unsigned char>(AreaW3f_AnswerMarkD1 + 1);
    Rest2c_TalkSlide = 4;
    AreaW3f_AnswerMarkD1 = step;
}

// original 0x585A50, pick step 1 (the panels sliding in): the slide down one,
// the panels; at 0 the pick 0x9398CE = 0 and the step up.
extern "C" void __cdecl MasterTalk_PanelsIn(void) {
    Rest2c_TalkSlide = static_cast<unsigned char>(Rest2c_TalkSlide - 1);
    Panels();
    if (Rest2c_TalkSlide != 0) return;
    const auto step = static_cast<unsigned char>(AreaW3f_AnswerMarkD1 + 1);
    Rest2d_Cursor = 0;
    AreaW3f_AnswerMarkD1 = step;
}

// original 0x585B20, pick step 4 (sliding out): the slide up one, the panels;
// at 4 the step up.
extern "C" void __cdecl MasterTalk_PanelsOut(void) {
    Rest2c_TalkSlide = static_cast<unsigned char>(Rest2c_TalkSlide + 1);
    Panels();
    if (Rest2c_TalkSlide == 4) ++AreaW3f_AnswerMarkD1;
}

// original 0x585BE0 (PSX 0x801D144C): the stats box of record r (its byte; 0x903A70 + 0xA4 r,
// read in place for any byte): Menu_DrawBox(x + 5, y + 3, 0x94, 0x2D, 0, the
// window colour); the four labels (DIV-0064's slots, read where they lie) with
// their words +0x24, +0x26, +0x2A, +0x28 through the number format in the 8
// px font; with a master (+0x1F not 0xFF) text record 0 = system message 0x111
// + it (8 long) and system message 0x34 at (x + 0xB, y + 0x22); the box's
// pieces: 0x664468, sixteen of 0x664494 8 apart, 0x6644A0.
extern "C" void __cdecl MasterPanel_DrawStats(int x, int y, unsigned record) {
    SH_CALL(Menu_DrawBox)(x + 5, y + 3, 0x94, 0x2D, 0, static_cast<unsigned char>(Config_WindowColour));
    const U r = ::bof3::Img(CharacterRecords_addr) + (record & 0xFF) * rest_2c::at::kRecordStride;
    char* const buffer = reinterpret_cast<char*>(AtRest2c(::bof3::Img(rest_2c::at::kPath.v)));
    const auto* const text = reinterpret_cast<const unsigned char*>(buffer);
    const auto* const format = reinterpret_cast<const char*>(AtRest2c(::bof3::Img(rest_2c::at::kFmtNumber.v)));
    struct Stat { int lx, ly, nx, ny; U label, word; };
    const Stat stats[4] = {
        {x + 0xB, y + 8, x + 0x30, y + 0xA, ::bof3::Img(rest_2c::at::kStatLabels.v), 0x24},
        {x + 0xB, y + 0x15, x + 0x30, y + 0x17, ::bof3::Img(rest_2c::at::kStatLabels.v) + 8, 0x26},
        {x + 0x53, y + 8, x + 0x78, y + 0xA, ::bof3::Img(rest_2c::at::kStatLabels.v) + 0x10, 0x2A},
        {x + 0x53, y + 0x15, x + 0x78, y + 0x17, ::bof3::Img(rest_2c::at::kStatLabels.v) + 0x18, 0x28},
    };
    for (const Stat& s : stats) {
        SH_CALL(Text_DrawAt)(s.lx, s.ly, 0, 0xFF, AtRest2c(s.label));
        SH_CALL(Crt_sprintf)(buffer, format, static_cast<unsigned>(Word(AtRest2c(r + s.word))));
        SH_CALL(Text_DrawFont8)(s.nx, s.ny, 0, text);
    }
    const unsigned char master = BRest2c(r + 0x1F);
    if (master != 0xFF) {
        const unsigned char* const name = SH_CALL(Msg_SystemPtr)(master + 0x111u);
        SH_CALL(TextRecord_Set)(0, 8, name);
        const unsigned char* const line = SH_CALL(Msg_SystemPtr)(0x34);
        SH_CALL(Text_DrawAt)(x + 0xB, y + 0x22, 0, 0xFF, line);
    }
    SH_CALL(Menu_DrawPieces)(x, y, AtRest2c(::bof3::Img(rest_2c::at::kStatsPieces.v)), 0);
    for (int k = 0; k < 16; ++k) SH_CALL(Menu_DrawPieces)(x + 8 * k, y, AtRest2c(::bof3::Img(rest_2c::at::kStatsRow.v)), 0);
    SH_CALL(Menu_DrawPieces)(x, y, AtRest2c(::bof3::Img(rest_2c::at::kStatsEnd.v)), 0);
}

// original 0x585DC0: the member panel of record r (its byte) in party slot s:
// Menu_DrawBox(x + 3, y + 3, 0x7D, 0x30, 0, the window colour); the face
// (MasterPanel_DrawFace at (x + 0x56, y + 2), the record's id +9, shade 2 when
// its status +0x10 has bit 7); the name (5 characters, Text_DrawAt at (x +
// 0x14, y + 3)); the level +0xA (the number format, 8 px font, (x + 0x16, y +
// 0x17)); the status words by +0x10 (bit 7 with bit 5: 0x66A0E8 on
// Frame_Counter's bit 5, else 0x66A0F0; bit 7 alone 0x66A0E8; bit 5 alone
// 0x66A0F0) in the small font at (x + 0x2E, y + 0x17); HP +0x18 / +0x20 at y
// + 0x1F, coloured 4 with the status's 0x2000, 2 at 1 or less; AP +0x1A /
// +0x22 at y + 0x27, coloured 4 at a quarter of the maximum or less, 2 at 0;
// the pieces 0x6643D8; the experience bar (MasterPanel_DrawExpBar at (x +
// 0x14, y + 0x12) of the record's level and +0xC); a draw mode (0, 0, 0xF, 0)
// committed; R2H's 0x59DB70 at (x + 9, y + 5): the cell (s + 3, 0x1E), the
// CLUT Gpu_GetClut(0x10, 0x1E0), shade 0x80.
extern "C" void __cdecl MasterPanel_DrawMember(int x, int y, unsigned record, unsigned slot) {
    SH_CALL(Menu_DrawBox)(x + 3, y + 3, 0x7D, 0x30, 0, static_cast<unsigned char>(Config_WindowColour));
    const U r = ::bof3::Img(CharacterRecords_addr) + (record & 0xFF) * rest_2c::at::kRecordStride;
    const unsigned char id = rv::As<records::CharRecord>(AtRest2c(r))->char_id;
    const unsigned shade = (BRest2c(r + 0x10) >> 6) & 2;
    SH_CALL(MasterPanel_DrawFace)(x + 0x56, y + 2, id, shade);
    SH_CALL(Text_DrawAt)(x + 0x14, y + 3, 0, 5, AtRest2c(r));
    char* const buffer = reinterpret_cast<char*>(AtRest2c(::bof3::Img(rest_2c::at::kPath.v)));
    const auto* const text = reinterpret_cast<const unsigned char*>(buffer);
    SH_CALL(Crt_sprintf)(buffer, reinterpret_cast<const char*>(AtRest2c(::bof3::Img(rest_2c::at::kFmtNumber.v))), static_cast<unsigned>(BRest2c(r + 0xA)));
    SH_CALL(Text_DrawFont8)(x + 0x16, y + 0x17, 0, text);
    const unsigned status = Word(AtRest2c(r + 0x10));
    U words = 0;
    if (status & 0x80) words = (status & 0x20) && (BRest2c(::bof3::Img(rest_2c::at::kFrameCounter.v)) & 0x20) == 0 ? ::bof3::Img(rest_2c::at::kStatusB.v) : ::bof3::Img(rest_2c::at::kStatusA.v);
    else if (status & 0x20) words = ::bof3::Img(rest_2c::at::kStatusB.v);
    if (words != 0) SH_CALL(Text_DrawSmall)(x + 0x2E, y + 0x17, 1, 0x10, AtRest2c(words));
    const auto* const left = reinterpret_cast<const char*>(AtRest2c(::bof3::Img(rest_2c::at::kFmtLeft.v)));
    const auto* const right = reinterpret_cast<const char*>(AtRest2c(::bof3::Img(rest_2c::at::kFmtRight.v)));
    int colour = (BRest2c(r + 0x11) & 0x20) ? 4 : 0;
    const unsigned hp = rv::As<records::CharRecord>(AtRest2c(r))->hp;
    if (hp <= 1) colour = 2;
    SH_CALL(Crt_sprintf)(buffer, left, hp);
    SH_CALL(Text_DrawFont8)(x + 0x16, y + 0x1F, colour, text);
    SH_CALL(Crt_sprintf)(buffer, right, static_cast<unsigned>(Word(AtRest2c(r + 0x20))));
    SH_CALL(Text_DrawFont8)(x + 0x16, y + 0x1F, 0, text);
    const unsigned ap = rv::As<records::CharRecord>(AtRest2c(r))->ap;
    colour = ap <= static_cast<unsigned>(Word(AtRest2c(r + 0x22)) >> 2) ? 4 : 0;
    if (ap == 0) colour = 2;
    SH_CALL(Crt_sprintf)(buffer, left, ap);
    SH_CALL(Text_DrawFont8)(x + 0x16, y + 0x27, colour, text);
    SH_CALL(Crt_sprintf)(buffer, right, static_cast<unsigned>(Word(AtRest2c(r + 0x22))));
    SH_CALL(Text_DrawFont8)(x + 0x16, y + 0x27, 0, text);
    SH_CALL(Menu_DrawPieces)(x, y, AtRest2c(::bof3::Img(rest_2c::at::kMemberPieces.v)), 0);
    const U exp = static_cast<U>(rv::As<records::CharRecord>(AtRest2c(r))->exp);
    const unsigned level = BRest2c(r + 0xA);
    SH_CALL(MasterPanel_DrawExpBar)(x + 0x14, y + 0x12, record, level, exp);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext.Native(), 0, 0, 0xF, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned clut = SH_CALL(Gpu_GetClut)(0x10, 0x1E0);
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned, unsigned, unsigned), rest_2c::at::kGlyph.v)(
        x + 9, y + 5, static_cast<unsigned char>(slot + 3), 0x1E, clut, 0x80);
}

// original 0x586030: a flat line (Gpu_SetLineF2, Gpu_SetSemiTrans(0)) at the
// packet cursor from (x, y) - each the argument's low 16 bits as a float - to
// (x + 57 * (exp - ExpForLevel(level)) / (ExpForLevel(level + 1) -
// ExpForLevel(level)), y), colour (0x80, 0, 0), committed as (1, 0x20). The
// level + 1 is the byte's increment (level 0xFF gives 0); the product is a
// 32-bit wrap and the division unsigned, the sum the low 16 bits of x plus
// the quotient, made a float through a 64-bit integer. Equal totals (a level
// past 99 gives -1 for both, the table's equal steps 0): the bar's end is x
// itself.
extern "C" void __cdecl MasterPanel_DrawExpBar(int x, int y, unsigned record, unsigned level, unsigned exp) {
    unsigned char* const prim = Gfx_PacketNext.Native();
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 0);
    const U x16 = static_cast<U>(x) & 0xFFFF;
    const auto fx = static_cast<float>(static_cast<int>(x16));
    PutF(prim + 8, fx);
    const auto fy = static_cast<float>(static_cast<int>(static_cast<U>(y) & 0xFFFF));
    PutF(prim + 0xC, fy);
    const U here = static_cast<U>(SH_CALL(MasterPanel_ExpForLevel)(record, level));
    const U next = static_cast<U>(SH_CALL(MasterPanel_ExpForLevel)(record, (level & 0xFFFFFF00u) | ((level + 1) & 0xFF)));
    const U span = next - here;
    if (span == 0) {
        PutF(prim + 0x14, fx);
    } else {
        const U end = (57u * exp - 57u * here) / span + x16;
        PutF(prim + 0x14, static_cast<float>(static_cast<double>(static_cast<std::uint64_t>(end))));
    }
    PutF(prim + 0x18, fy);
    prim[4] = 0x80;
    prim[5] = 0;
    prim[6] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x586110 (Char_ExpForLevel's body, instruction for instruction):
// the experience to reach `level` (a byte): -1 above 99; else the sum of the
// first `level` u16 steps of the member's (a byte) row of Char_ExpTable, 0x318
// bytes a member and 8 a step (read in place for any member byte).
extern "C" int __cdecl MasterPanel_ExpForLevel(unsigned record, unsigned level) {
    const unsigned n = level & 0xFF;
    if (n > 0x63) return -1;
    U sum = 0;
    const U row = ::bof3::Img(rest_2c::at::kExpTable.v) + (record & 0xFF) * 0x318u;
    for (unsigned i = 0; i < n; ++i) sum += Word(AtRest2c(row + 8 * i));
    return static_cast<int>(sum);
}

// original 0x586160: a panel box at (x, y), w by h, colour c (a byte). A draw
// mode with the texture window (0, 0xF0, 16, 16); four FT4 edge quads from
// the 16 x 16 cell at CLUT (c * 32 + 16, 0x1E1), page 0xF, grey 0xAC - the
// left (x .. x + 2), the middle's two halves (x + 2 .. x + 2 + hw, u to hw's
// byte; then .. x + 2 + 2 hw + odd, u to odd + hw, with hw = ((w + 1) &
// 0xFFFF - 4) >> 1 (arithmetic) as a short and odd = (w + 1) & 1), the right
// (x + w - 1 .. x + w + 1). Menu_DrawTitleBox's body (0x574AB0, ours in
// menu_windows.cpp) to the instruction, a second copy Capcom's compiler
// kept. Each corner's float made as the original makes it (x and y are the
// arguments' low 16 bits; y + 2.0, (y + h + 1) - 3.0 and - 1.0 from the
// constants at 0x5C41C0, 0x5C41BC, 0x5C41B8 read in place); then the window
// (0, 0, 256, 256) and Menu_DrawOutline(x + 2, y + 2, w - 4, h - 4, 0) with
// the arguments' whole words. The original also writes h - 2's byte and
// x + w - 1 / its float over its own argument slots, which no caller reads.
extern "C" void __cdecl Menu_DrawPanelBox(int x, int y, int w, int h, unsigned colour) {
    WindowMode(0xF0, 0x10);
    const U w1 = static_cast<U>(w) + 1, h1 = static_cast<U>(h) + 1;
    const U x16 = static_cast<U>(x) & 0xFFFF, y16 = static_cast<U>(y) & 0xFFFF;
    const float fx = static_cast<float>(static_cast<int>(x16));
    const float fy = static_cast<float>(static_cast<int>(y16));
    const float fx2 = static_cast<float>(static_cast<int>(x16 + 2));
    const float fbottom = static_cast<float>(static_cast<int>((h1 & 0xFFFF) + y16));
    const float ftop2 = static_cast<float>(static_cast<double>(fy) + static_cast<double>(F(rest_2c::at::kBoxAdd)));
    const float fbottom3 = static_cast<float>(static_cast<double>(fbottom) - static_cast<double>(F(rest_2c::at::kBoxSub)));
    const auto vh = static_cast<unsigned char>(h1);
    const int clut_x = static_cast<int>(((colour & 0xFF) << 5) + 0x10);
    // the left edge
    EdgeQuad({{Bits(fx), Bits(fx2), Bits(fx), Bits(fx2)},
              {Bits(ftop2), Bits(fy), Bits(fbottom3), Bits(fbottom)},
              {0, 2, 0, 2},
              {2, 0, static_cast<unsigned char>(vh - 3), vh}},
             clut_x);
    // the middle's first half
    const U w16 = w1 & 0xFFFF;
    const int half = static_cast<int>(w16 - 4) >> 1;
    const unsigned char odd = static_cast<unsigned char>(w1 & 1);
    const int half_s = static_cast<short>(half);
    const float fmid = static_cast<float>(static_cast<int>(x16) + half_s + 2);
    const auto u1 = static_cast<unsigned char>(half);
    EdgeQuad({{Bits(fx2), Bits(fmid), Bits(fx2), Bits(fmid)},
              {Bits(fy), Bits(fy), Bits(fbottom), Bits(fbottom)},
              {0, u1, 0, u1},
              {0, 0, vh, vh}},
             clut_x);
    // the middle's second half
    const float fend = static_cast<float>(static_cast<int>(x16) + static_cast<short>(odd) + 2 * half_s + 2);
    const auto u2 = static_cast<unsigned char>(odd + static_cast<unsigned char>(half));
    EdgeQuad({{Bits(fmid), Bits(fend), Bits(fmid), Bits(fend)},
              {Bits(fy), Bits(fy), Bits(fbottom), Bits(fbottom)},
              {0, u2, 0, u2},
              {0, 0, vh, vh}},
             clut_x);
    // the right edge
    const U right = x16 + w16;
    const float fr0 = static_cast<float>(static_cast<int>(right - 2));
    const float fr1 = static_cast<float>(static_cast<int>(right));
    const float fbottom1 = static_cast<float>(static_cast<double>(fbottom) - static_cast<double>(F(rest_2c::at::kBoxSubBottom)));
    EdgeQuad({{Bits(fr0), Bits(fr1), Bits(fr0), Bits(fr1)},
              {Bits(fy), Bits(ftop2), Bits(fbottom1), Bits(fbottom3)},
              {0, 2, 0, 2},
              {0, 2, static_cast<unsigned char>(vh - 1), static_cast<unsigned char>(vh - 3)}},
             clut_x);
    WindowMode(0, 0x100);
    SH_CALL(Menu_DrawOutline)(x + 2, y + 2, static_cast<int>(w1 - 5), static_cast<int>(h1 - 5), 0);
}

#pragma clang attribute pop

void MasterTalk_Inject() {
    BOF3_INJECT(MasterTalk_Reset);
    BOF3_INJECT(MasterTalk_PanelsOpen);
    BOF3_INJECT(MasterTalk_PanelsIn);
    BOF3_INJECT(MasterTalk_PanelsOut);
    BOF3_INJECT(MasterPanel_DrawStats);
    BOF3_INJECT(MasterPanel_DrawMember);
    BOF3_INJECT(MasterPanel_DrawExpBar);
    BOF3_INJECT(MasterPanel_ExpForLevel);
    BOF3_INJECT(Menu_DrawPanelBox);
}
