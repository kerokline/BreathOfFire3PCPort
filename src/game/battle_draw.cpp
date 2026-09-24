// Group BJ of the seventh takeover round (docs/battle_draw.md): six functions,
// each read to its last instruction with capstone against bof3/BOF3.exe
// (2026-09-23). Faithful: no divergence. The start-up fuzz is
// battle_draw_fuzz.cpp.
//
//   D3d_DrawPolyG3            0x5A0E80..0x5A1047 (0x1C8)  code 0x30   -
//   D3d_DrawLineG2            0x5A18B0..0x5A19FC (0x14D)  code 0x50   -
//   D3d_DrawLineG3            0x5A1B50..0x5A1D0D (0x1BE)  code 0x58   -
//   BattleMenu_DrawItemList   0x59CD00..0x59D1F1 (0x4F2)              - (no pair)
//   BattleMenu_DrawSkillList  0x59D200..0x59D635 (0x436)              - (no pair)
//   AreaMap_TintClut          0x573050..0x573077 (0x28)               PSX 0x801565B4
//
// Every call goes through battle_draw::g (battle_draw_callees.h), so that the
// fuzz can stand recorders in for them - for ours and for the originals'
// copies alike. Where the original reads memory after a call, ours reads it
// after the same call.
//
// Upper halves. The two list draws push many values built with 8- and 16-bit
// operations, whose upper bits are whatever the register or stack slot held
// (a local byte in a dword slot, a 16-bit add into ax). Every callee reads only
// the low 16 bits of a coordinate and the low byte of a colour, flag, count or
// id - the menus' own (menu_windows.cpp's rule), and group BD's by their first
// instructions (docs/battle_draw.md section 3) - so ours passes each value with
// its upper bits as C++ computes them.
#include "game/battle_draw.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_draw_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_draw {

namespace {
template <typename T> T Fn(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    D3d_PrimColor,
    Fn<void (__cdecl*)(unsigned)>(kRetOnly),
    D3d_SetBlend,
    D3d_SetShadeMode,
    Menu_DrawBox,
    Menu_ListScroll,
    Item_CanUse,
    Menu_DrawItemRow,
    Text_CharCount,
    Text_DrawAt,
    Inventory_CountUsed,
    Crt_sprintf,
    Text_DrawFont8,
    Menu_DrawPieces,
    Menu_DrawPiece,
    Menu_DrawScrollBar,
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(kSkillUsable),
    Fn<void (__cdecl*)(int, int, int, unsigned, const unsigned char*, unsigned, int)>(kSkillRow),
    Fn<unsigned (__cdecl*)(unsigned)>(kSkillIcon),
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(kSkillCost),
    Fn<const unsigned char* (__cdecl*)(unsigned, unsigned, unsigned)>(kSkillList),
    Gfx_ClutAdjust,
    AreaMap_ClutCycleStart,
};
Callees g = kOriginals;

}  // namespace battle_draw

namespace {

using namespace battle_draw;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
float Float(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }
unsigned char Byte(U address) { return At(address)[0]; }
std::int32_t S8(U v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }
std::int32_t S16(U v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

// --- the x87 sequences, as the handlers execute them (d3d_draw.cpp and
// field_misc.cpp have the same; each rounds to the control word's precision
// exactly as Capcom's) ---
// fld a; fmul b; fstp
inline float X87Mul(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfmuls %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}
// fld a; fdiv b; fstp
inline float X87Div(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfdivs %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// D3d_Device, read afresh before every COM call as the original does.
void** Device() { return *reinterpret_cast<void** volatile*>(At(kDevice)); }
void* Method(void** device, U offset) { return (*reinterpret_cast<void***>(device))[offset / 4]; }
using Com3 = long(__stdcall*)(void*, U, U);
using Com6 = long(__stdcall*)(void*, U, U, U, U, U);
long SetTexture(U stage, U texture) {
    void** device = Device();
    return reinterpret_cast<Com3>(Method(device, 0x98))(device, stage, texture);
}
// DrawPrimitive(type, D3DFVF_TLVERTEX, D3d_Vertices, count, 0).
long DrawVertices(U type, U count) {
    void** device = Device();
    return reinterpret_cast<Com6>(Method(device, 0x70))(device, type, 0x1C4, kVertices, count, 0);
}
unsigned char* Vertex(U i) { return At(kVertices + i * 0x20); }
U DrawMode() { return Long(At(kDrawTpage)) & 0xFFFF; }
// sx, sy = the scales times the corner's float x, y; sz its z as it is (a mov);
// rhw = 0.1 / z; the diffuse as given. Specular, tu and tv are not written.
void PutCorner(unsigned char* out, const unsigned char* xyz, U diffuse) {
    PutFloat(out + 0x00, X87Mul(Float(At(kScaleX)), Float(xyz)));
    PutFloat(out + 0x04, X87Mul(Float(At(kScaleY)), Float(xyz + 4)));
    PutLong(out + 0x08, Long(xyz + 8));
    PutFloat(out + 0x0C, X87Div(Float(At(kRhwNumerator)), Float(xyz + 8)));
    PutLong(out + 0x10, diffuse);
}

// The Gouraud handlers' shared shape: n colours (r, g, b at +4 + i * 0x10, the
// code byte +7 and the mode read again for each), all before any corner; then
// n corners of 0x10 bytes from +8 (float x, y, z).
void GouraudCorners(const unsigned char* prim, U n) {
    unsigned long diffuse[3];
    for (U i = 0; i < n; ++i) {
        const unsigned char* const rgb = prim + 4 + i * 0x10;
        g.prim_color(rgb[0], rgb[1], rgb[2], prim[7], DrawMode(), &diffuse[i], nullptr);
    }
    for (U i = 0; i < n; ++i) PutCorner(Vertex(i), prim + 8 + i * 0x10, static_cast<U>(diffuse[i]));
}

}  // namespace

// --- the Direct3D handlers -------------------------------------------------------
// Each is reached from one site of Gfx_DrawOTag's Direct3D table (d3d-draw.md
// section 2) and returns what DrawPrimitive returned (the caller does not read
// it). Untextured and Gouraud (shade mode 2): SetTexture(0, NULL); the blend
// mode Gfx_DrawTpage's low word, read for each colour and again for the blend;
// the colour helper gets a null specular pointer, so the vertices' specular,
// tu and tv stay as the last draw left them. The colours go into locals, as
// D3d_DrawPolyG4's do. The PC port's own: no PSX twin (the PlayStation drew
// primitives on its GPU).

// original 0x5A0E80, table entry 4 (code 0x30, called at 0x59F0E2): a Gouraud
// triangle - three corners, a triangle list of 3. And, as the original has it,
// the fourth vertex's z (0x7CA9C0) set to 0 - a store no draw of this handler
// reads (three vertices), which the next four-vertex handler overwrites.
extern "C" long __cdecl D3d_DrawPolyG3(const unsigned char* prim) {
    GouraudCorners(prim, 3);
    PutLong(Vertex(3) + 8, 0);
    SetTexture(0, 0);
    g.ret_only(0);
    g.ret_only(0);
    g.set_blend(prim[7], DrawMode());
    g.set_shade(2);   // Gouraud
    return DrawVertices(4, 3);   // TRIANGLELIST
}

// original 0x5A18B0, table entry 11 (code 0x50, called at 0x59F144): a Gouraud
// line - two corners, a line strip of 2. The bare-ret calls get 0 and 1, as
// the other lines' do.
extern "C" long __cdecl D3d_DrawLineG2(const unsigned char* prim) {
    GouraudCorners(prim, 2);
    SetTexture(0, 0);
    g.ret_only(0);
    g.ret_only(1);
    g.set_blend(prim[7], DrawMode());
    g.set_shade(2);
    return DrawVertices(3, 2);   // LINESTRIP
}

// original 0x5A1B50, table entry 12 (code 0x58, called at 0x59F152): a Gouraud
// two-segment polyline - three corners, a line strip of 3.
extern "C" long __cdecl D3d_DrawLineG3(const unsigned char* prim) {
    GouraudCorners(prim, 3);
    SetTexture(0, 0);
    g.ret_only(0);
    g.ret_only(1);
    g.set_blend(prim[7], DrawMode());
    g.set_shade(2);
    return DrawVertices(3, 3);   // LINESTRIP
}

// --- the battle menu's lists -------------------------------------------------------
// Both are the drawing half of a window-task handler (0x59CB40 and 0x59CBC0,
// entries 1 and 2 of the handler table 0x66B534, each of which first runs a
// step from its own table by the record's state byte +3, then tail-calls this
// with the current record 0x905B84). Their shape is Menu_DrawItemList's
// (0x5759C0), with seven rows and a smaller frame. Neither returns anything its
// caller reads.

// original 0x59CD00 (no PSX pair): the battle's item list. The window record:
// +4 / +6 x and y, +8 the Item_CanUse mode, +9 greyed (and the boxes' flags),
// +0xA the category, +0xB the top index, +0xC a marked index, +0xD the cursor,
// word +0x10 two arrow flags and a countdown in its high nibble, +0x12 the
// scroll state.
//
// The box (x + 3, y + 3, 0x99, 0x82); the scroll step, whose offset (s8)
// shifts the rows and whose moving flag draws an eighth; each row 13 apart
// whose item is not 0: dim when greyed or when Item_CanUse says no for the
// member of the battle party list 0x904065 the menu is on (0x929F06), colour
// 7 when dim, 2 on the cursor (top + row == +0xD), 0 else; the count 1 for
// category 4, else the counts array's; the cursor's and the marked row drawn 2
// higher over a colour-7 dim shadow (none when the colour is 7). Then the
// title and footer boxes, the category's title (0x66B58C) centred by its
// character count, "%3d/%3d" of Inventory_CountUsed and the room (0x20 for
// category 4, else 0x80), the arrows by +0x10's bits 1 and 0, the countdown
// stepped down, the frame's pieces and the scroll bar over 0x80 entries.
extern "C" void __cdecl BattleMenu_DrawItemList(unsigned char* w) {
    g.box(Word(w + 4) + 3, Word(w + 6) + 3, 0x99, 0x82, w[9], Byte(kColour));
    unsigned char offset = 0, moving = 0;
    const unsigned char top = g.list_scroll(w + 0xB, &offset, &moving, w + 0x12);
    const U category = w[0xA];
    U items = Long(At(kInventory + category * 4)) + top;
    U counts = Long(At(kCounts + category * 4)) + top;   // 0 + top for key items: not read then
    U row = static_cast<std::uint16_t>(S8(offset) + Word(w + 6)) + 0x1Au;
    const unsigned rows = moving + 7u;
    for (unsigned char i = 0; i < rows; ++i) {
        const unsigned char item = At(items)[0];
        if (item != 0) {
            unsigned char dim = 1;
            if (w[9] == 0) {
                const unsigned member = Byte(kMemberRecord + Byte(kBattleParty + S8(Byte(kMenuMember))));
                dim = g.can_use(w[8], member, w[0xA], item) ? 0 : 1;
            }
            unsigned char colour = dim ? 7 : 0;
            const U here = static_cast<U>(i) + w[0xB];
            const U cursor = w[0xD];
            if (here == cursor) colour = 2;
            const unsigned char row_category = w[0xA];
            const unsigned char count = row_category == 4 ? 1 : At(counts)[0];
            const int x = static_cast<int>(Word(w + 4)) + 7;
            if (here == cursor || here == w[0xC]) {
                if (colour != 7) g.item_row(x, static_cast<int>(row), 7, row_category, At(items)[0], count, 1);
                g.item_row(static_cast<int>(Word(w + 4)) + 7, static_cast<int>(row) - 2, colour, w[0xA],
                           At(items)[0], count, dim);
            } else {
                g.item_row(x, static_cast<int>(row), colour, row_category, At(items)[0], count, dim);
            }
        }
        ++items;
        ++counts;
        row += 0xD;
    }

    g.box(Word(w + 4) + 3, Word(w + 6) + 3, 0x99, 0x14, w[9], Byte(kColour));
    g.box(Word(w + 4) + 3, Word(w + 6) + 0x7A, 0x99, 8, w[9], Byte(kColour));
    {
        const unsigned char grey = w[9] ? 7 : 0;
        const unsigned char* const label = At(Long(At(kItemLabels + w[0xA] * 4u)));
        const int y = static_cast<int>(Word(w + 6)) + 7;
        const unsigned n = g.char_count(label);
        g.text_draw_at(static_cast<int>(6 * (13 - static_cast<int>(n)) + static_cast<int>(Word(w + 4))), y, grey,
                       0x10, label);
    }
    {
        const unsigned char cat = w[0xA];
        const unsigned room = cat != 4 ? 0x80 : 0x20;
        const unsigned have = g.count_used(cat) & 0xFFu;
        g.sprintf_(reinterpret_cast<char*>(At(kPrintBuf)), reinterpret_cast<const char*>(At(kFmt3d3d)), have, room);
        const unsigned char grey = w[9] ? 7 : 0;
        g.font8(Word(w + 4) + 0x55, Word(w + 6) + 0x7A, grey, At(kPrintBuf));
    }
    g.pieces(Word(w + 4), Word(w + 6), At((w[0x10] & 2) ? kTitleBit1On : kTitleBit1Off), 1);
    g.pieces(Word(w + 4), Word(w + 6), At((w[0x10] & 1) ? kTitleBit0On : kTitleBit0Off), 1);
    {
        const U t = Word(w + 0x10);
        PutWord(w + 0x10, (t & 0xF0) ? t - 0x10 : 0);
    }
    for (int b = 0; b < 10; ++b) g.piece(Word(w + 4) + b * 8 + 0x28, Word(w + 6), 1, 1);
    for (int b = 0; b < 12; ++b) g.piece(Word(w + 4), Word(w + 6) + b * 8 + 0x18, 4, 1);
    g.piece(Word(w + 4) + 0x90, Word(w + 6) + 0x18, 0x16, 1);
    for (int b = 0; b < 9; ++b) g.piece(Word(w + 4) + 0x90, Word(w + 6) + b * 8 + 0x28, 0x17, 1);
    g.piece(Word(w + 4) + 0x90, Word(w + 6) + 0x70, 0x18, 1);
    g.piece(Word(w + 4), Word(w + 6) + 0x78, 0x19, 1);
    for (int b = 0; b < 9; ++b) g.piece(Word(w + 4) + b * 8 + 8, Word(w + 6) + 0x78, 0x1A, 1);
    g.piece(Word(w + 4) + 0x50, Word(w + 6) + 0x78, 0x1B, 1);
    for (int b = 0; b < 6; ++b) g.piece(Word(w + 4) + b * 8 + 0x58, Word(w + 6) + 0x78, 0x1C, 1);
    g.piece(Word(w + 4) + 0x88, Word(w + 6) + 0x78, 0x1D, 1);
    g.scroll_bar(At(Long(At(kInventory + w[0xA] * 4u))), w[0xB], Word(w + 4) + 0x90, Word(w + 6) + 0x18, 7, 0x80,
                 0x5C);
}

// original 0x59D200 (no PSX pair): the battle's skill list. The window record:
// +4 / +6 x and y, +8 the usable test's mode, +9 two arrow flags and a
// countdown in its high nibble, +0xA the member and +0xB the list's kind
// (group BD's 0x591E50 with 1: a 10-byte list of skill ids in the member's
// battle record), +0xC a marked index, +0xD the cursor, +0x10 the top index
// (a byte to the scroll step; read back as an s16 per row), +0x12 the scroll
// state, word +0x14 non-zero for the list titled 0x66A220 instead of by kind
// (docs/dialogue-localisation.md names that string the skill list's header).
//
// The box (x + 3, y + 3, 0x99, 0x82) with flags 0; the scroll step; each row
// 13 apart whose id is not 0: 0x57DA70(+8, 0x929F06, id) - dim when it answers
// 0; colour 7 when dim, else 0, and 2 when the ROW INDEX is +0xD; the cost
// 0x591DB0(0x929F06, id, 1) and the icon kind 0x5918A0(id); the row through
// 0x57DC90 with the skill's record 0x65C4C8 + 0x18 * id; the cursor's and the
// marked row - top (s16 +0x10) + row index equal to +0xD or +0xC - drawn 2
// higher over a colour-7 dim shadow (none when the colour is 7). The colour
// test and the raise test disagree once the list is scrolled (docs/battle_draw.md
// section 5). Then the title box, the title centred by its byte length (strlen,
// where the item list counts characters), the arrows by +9's bits 1 and 0, the
// countdown in +9 stepped down, the frame's pieces (two more with word +0x14)
// and the scroll bar over the list's 10 entries.
extern "C" void __cdecl BattleMenu_DrawSkillList(unsigned char* w) {
    g.box(Word(w + 4) + 3, Word(w + 6) + 3, 0x99, 0x82, 0, Byte(kColour));
    unsigned char offset = 0, moving = 0;
    const unsigned char top = g.list_scroll(w + 0x10, &offset, &moving, w + 0x12);
    U ids = Addr(g.skill_list(w[0xA], w[0xB], 1)) + top;
    U row = static_cast<std::uint16_t>(S8(offset) + Word(w + 6)) + 0x1Au;
    const unsigned rows = moving + 7u;
    for (unsigned char i = 0; i < rows; ++i) {
        const unsigned char id = At(ids)[0];
        if (id != 0) {
            const unsigned char usable = static_cast<unsigned char>(g.skill_usable(w[8], Byte(kMenuMember), id));
            const unsigned char dim = usable == 0 ? 1 : 0;
            unsigned char colour = dim ? 7 : 0;
            if (i == w[0xD]) colour = 2;
            const unsigned cost = g.skill_cost(Byte(kMenuMember), At(ids)[0], 1) & 0xFFu;
            const unsigned icon = g.skill_icon(At(ids)[0]) & 0xFFu;
            const std::int32_t here = static_cast<std::int32_t>(i) + S16(Word(w + 0x10));
            const unsigned char* const record = At(kSkillRecords + At(ids)[0] * 0x18u);
            if (here == static_cast<std::int32_t>(w[0xD]) || here == static_cast<std::int32_t>(w[0xC])) {
                if (colour != 7) g.skill_row(static_cast<int>(Word(w + 4)) + 7, static_cast<int>(row), 7, icon, record, cost, 1);
                g.skill_row(static_cast<int>(Word(w + 4)) + 7, static_cast<int>(row) - 2, colour, icon, record, cost,
                            dim);
            } else {
                g.skill_row(static_cast<int>(Word(w + 4)) + 7, static_cast<int>(row), colour, icon, record, cost, dim);
            }
        }
        ++ids;
        row += 0xD;
    }

    g.box(Word(w + 4) + 3, Word(w + 6) + 3, 0x99, 0x14, 0, Byte(kColour));
    {
        const unsigned char* const label =
            At(Word(w + 0x14) != 0 ? Long(At(kSkillLabelAlt)) : Long(At(kSkillLabels + w[0xB] * 4u)));
        const int y = static_cast<int>(Word(w + 6)) + 7;
        const U length = static_cast<U>(std::strlen(reinterpret_cast<const char*>(label)));
        g.text_draw_at(static_cast<int>(6 * (13 - length) + Word(w + 4)), y, 0, 0x10, label);
    }
    g.pieces(Word(w + 4), Word(w + 6), At((w[9] & 2) ? kTitleBit1On : kTitleBit1Off), 1);
    g.pieces(Word(w + 4), Word(w + 6), At((w[9] & 1) ? kTitleBit0On : kTitleBit0Off), 1);
    {
        const unsigned char t = w[9];
        w[9] = (t & 0xF0) ? static_cast<unsigned char>(t - 0x10) : 0;
    }
    for (int b = 0; b < 10; ++b) g.piece(Word(w + 4) + b * 8 + 0x28, Word(w + 6), 1, 1);
    for (int b = 0; b < 12; ++b) g.piece(Word(w + 4), Word(w + 6) + b * 8 + 0x18, 4, 1);
    g.piece(Word(w + 4) + 0x90, Word(w + 6) + 0x18, 0x16, 1);
    for (int b = 0; b < 9; ++b) g.piece(Word(w + 4) + 0x90, Word(w + 6) + b * 8 + 0x28, 0x17, 1);
    g.piece(Word(w + 4) + 0x90, Word(w + 6) + 0x70, 0x18, 1);
    g.piece(Word(w + 4), Word(w + 6) + 0x78, 0x19, 1);
    for (int b = 0; b < 17; ++b) g.piece(Word(w + 4) + b * 8 + 8, Word(w + 6) + 0x78, 0x1A, 1);
    if (Word(w + 0x14) != 0) {
        g.piece(Word(w + 4) + 8, Word(w + 6), 0x32, 1);
        g.piece(Word(w + 4) + 0x90, Word(w + 6), 0x33, 1);
    }
    {
        // x, y and the top read before the list lookup, as the original's
        const int x = static_cast<int>(Word(w + 4)) + 0x90, y = static_cast<int>(Word(w + 6)) + 0x18;
        const unsigned char first = w[0x10];
        const unsigned char* const list = g.skill_list(w[0xA], w[0xB], 1);
        g.scroll_bar(list, first, x, y, 7, 0xA, 0x5C);
    }
}

// --- the area palette ---------------------------------------------------------------

// original 0x573050 (PSX 0x801565B4, the same two calls): unless bit 0 of
// Field_StatusBits 0x8034E1, every area CLUT (rows 3..14, every column) tinted
// by level on all three channels - Gfx_ClutAdjust(0xFFFF, 0xFFF, level, level,
// level) - then AreaMap_ClutCycleStart, a tail jump. Three callers (rel32
// scan), each a state step reached through a pointer table that counts a
// byte down or up by one and passes it sign-extended (0x4B51E0, 0x4FB010,
// 0x4FB070) and returns what this leaves in eax. That is the caller's own eax
// on the early exit and AreaMap_ClutCycleStart's (ours, void) on the other,
// so nothing can rely on it: ours is void.
extern "C" void __cdecl AreaMap_TintClut(int level) {
    if (Byte(kStatusBits) & 1) return;
    g.clut_adjust(0xFFFF, 0xFFF, level, level, level);
    g.clut_cycle();
}

void BattleDraw_Inject() {
    if (bof3::WantsShadow("battle_draw")) battle_draw::SelfTest();
    BOF3_INJECT(D3d_DrawPolyG3);
    BOF3_INJECT(D3d_DrawLineG2);
    BOF3_INJECT(D3d_DrawLineG3);
    BOF3_INJECT(BattleMenu_DrawItemList);
    BOF3_INJECT(BattleMenu_DrawSkillList);
    BOF3_INJECT(AreaMap_TintClut);
}
