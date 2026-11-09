// Moved here by tools/move_funcs.py from effect_3d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_7d.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_3d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U W(const unsigned char* p) { return Word(p); }

U W(U a) { return Word(At(a)); }

unsigned char& B(U a) { return *At(a); }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

// --- the x87 sequences, as the original's instructions --------------------------
// fild dword v; fstp dword [to]
void Fild(unsigned char* to, std::int32_t v) {
    __asm__ volatile("fildl %1\n\tfstps %0" : "=m"(*reinterpret_cast<float*>(to)) : "m"(v));
}

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, data or the next kind's states.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3d.md section 7)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

void NextState() { S()[1] = static_cast<unsigned char>(S()[1] + 1); }

// +9 down one; true when it reaches 0 (Sprite_Current read for each access).
bool CountDown() {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    return S()[9] == 0;
}

// The cone's rim point at angle a and radius r about the point: (x, z + (cos a r
// >> 4), height + (sin a r << 4)), each product a 32-bit imul; projected. The
// original writes x only for the inner point of each pair (the outer reuses it).
void ConePoint(long* rim, const long* point, U a, unsigned radius, float* out, bool inner) {
    if (inner) rim[0] = point[0];
    const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
    rim[1] = static_cast<long>(static_cast<U>(static_cast<std::int32_t>(c * static_cast<U>(S16(radius))) >> 4) +
                               static_cast<U>(point[1]));
    const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
    rim[2] = static_cast<long>(((sn * static_cast<U>(S16(radius))) << 4) + static_cast<U>(point[2]));
    SH_CALL(EffectGte_ProjectPoint)(rim, out);
}

}  // namespace

// original 0x486640 (Effect_KindHandlers[0x7D], hidden in 0x4864C0): jmp
// [EffectKind7D_States + +1 * 4].
extern "C" void __cdecl EffectKind7D_Run(void) {
    Dispatch("EffectKind7D_Run", AddressOf(EffectKind7D_States), EffectKind7D_States_count, 1);
}

// original 0x486660 (state 0): +1 up, +0x2E 5, +0x4C the address of dial +6
// (0x675DC8 + +6, unchecked: only an address).
extern "C" void __cdecl EffectKind7D_Start(void) {
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 5);
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->anim_data, ::bof3::Img(at::kDials.v) + s[6]);
}

// original 0x486690 (state 1): the dial +0x4C points at turned by the press
// (bit 15 of Input_Pressed up, wrapping 9 to 0; bit 13 down, below 0 to 8;
// Sound_PlayEffect(0x204) when it moved); confirm (Field_ConfirmButtons):
// sound 0x205, the answer 0x9039F6 1, dial +6 the turn, +1 up one; cancel:
// sound 0x206, the answer 0xFF, +1 up two. Then the panel at (+0x2E, +0x30).
extern "C" void __cdecl EffectKind7D_Input(void) {
    unsigned char* dial = P(rv::As<records::EffectObject>(S())->anim_data);
    const unsigned before = *dial;
    if (UL(::bof3::Img(at::kInputPressed.v)) & 0x8000u) {
        *dial = static_cast<unsigned char>(before + 1);
        dial = P(rv::As<records::EffectObject>(S())->anim_data);
        if (*dial >= 9) *dial = 0;
    }
    if (UL(::bof3::Img(at::kInputPressed.v)) & 0x2000u) {
        dial = P(rv::As<records::EffectObject>(S())->anim_data);
        *dial = static_cast<unsigned char>(*dial - 1);
        dial = P(rv::As<records::EffectObject>(S())->anim_data);
        if (static_cast<signed char>(*dial) < 0) *dial = 8;
    }
    if (before != *P(rv::As<records::EffectObject>(S())->anim_data)) SH_CALL(Sound_PlayEffect)(0x204);
    const U pressed = UL(::bof3::Img(at::kInputPressed.v));
    if ((Field_ConfirmButtons & pressed & 0xFFFFu) != 0) {
        SH_CALL(Sound_PlayEffect)(0x205);
        unsigned char* const s = S();
        const unsigned which = s[6];
        const unsigned char turn = *P(rv::As<records::EffectObject>(s)->anim_data);
        SetWord(At(::bof3::Img(at::kDialAnswer.v)), 1);
        if (which >= at::kDialCount)
            bof3::Fatal("EffectKind7D_Input: +6 is %u, past the three dials at 0x675DC8 - the original writes the byte "
                        "after (docs/effect_3d.md section 7)",
                        which);
        B(::bof3::Img(at::kDials.v) + which) = turn;
        s[1] = static_cast<unsigned char>(s[1] + 1);
    } else if ((Field_CancelButtons & pressed & 0xFFFFu) != 0) {
        SH_CALL(Sound_PlayEffect)(0x206);
        unsigned char* const s = S();
        SetWord(At(::bof3::Img(at::kDialAnswer.v)), 0xFF);
        s[1] = static_cast<unsigned char>(s[1] + 2);
    }
    unsigned char* const s = S();
    SH_CALL(EffectKind7D_DrawPanel)(static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e), static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30));
}

// original 0x486790 (state 2): the dials applied to the map, +1 up (state 3 is
// Effect_StateRelease).
extern "C" void __cdecl EffectKind7D_Apply(void) {
    SH_CALL(EffectKind7D_SetMap)();
    NextState();
}

// original 0x4867A0 (cdecl): the panel at (x, y): three green bars (abr 0), the
// three dials (0x63CA48, 0x63CA87, 0x63CAC6 at x + 9, x + 0x6D, x + 0xD1, y +
// 10, by the dials 0x675DC8..CA), and while Frame_Counter has bit 4 two white
// arrows and a white frame about dial +6.
extern "C" void __cdecl EffectKind7D_DrawPanel(unsigned x, unsigned y) {
    SH_CALL(EffectKind7D_DrawMode)(0);
    SH_CALL(EffectKind7D_FillF4)(x, y + 5, x + 5, y, x, y + 0x55, x + 5, y + 0x5A, 0, 0x80, 0, 1);
    SH_CALL(EffectKind7D_FillF4)(x + 5, y, x + 0x131, y, x + 5, y + 0x5A, x + 0x131, y + 0x5A, 0, 0x80, 0, 1);
    SH_CALL(EffectKind7D_FillF4)(x + 0x131, y, x + 0x136, y + 5, x + 0x131, y + 0x5A, x + 0x136, y + 0x55, 0, 0x80, 0, 1);
    const U xm = x - 1, yd = y + 0xA;
    SH_CALL(EffectKind7D_DrawDial)(xm + 0xA, yd, P(::bof3::Img(at::kDialGrids.v)), B(::bof3::Img(at::kDials.v)));
    SH_CALL(EffectKind7D_DrawDial)(xm + 0x6E, yd, P(::bof3::Img(at::kDialGrids.v) + at::kGridSize), B(::bof3::Img(at::kDials.v) + 1));
    SH_CALL(EffectKind7D_DrawDial)(xm + 0xD2, yd, P(::bof3::Img(at::kDialGrids.v) + 2 * at::kGridSize), B(::bof3::Img(at::kDials.v) + 2));
    if ((Frame_Counter & 0x10u) == 0) return;
    const U ya = y + 0x2D;
    const U l = xm + 100u * S()[6];
    SH_CALL(EffectKind7D_FillF3)(l + 7, ya - 4, l + 3, ya, l + 7, ya + 4, 0xC8, 0xC8, 0xC8, 0);
    const U rgt = xm + 100u * (S()[6] + 1u);
    SH_CALL(EffectKind7D_FillF3)(rgt + 4, ya - 4, rgt + 8, ya, rgt + 4, ya + 4, 0xC8, 0xC8, 0xC8, 0);
    const U left = xm + 100u * S()[6] + 0xA, right = left + 0x5A, bottom = yd + 0x46;
    SH_CALL(EffectKind7D_Line)(left, yd, right, yd, 0xC8, 0xC8, 0xC8, 0);
    SH_CALL(EffectKind7D_Line)(left, yd, left, bottom, 0xC8, 0xC8, 0xC8, 0);
    SH_CALL(EffectKind7D_Line)(left, bottom, right, bottom, 0xC8, 0xC8, 0xC8, 0);
    SH_CALL(EffectKind7D_Line)(right, yd, right, bottom, 0xC8, 0xC8, 0xC8, 0);
}

// original 0x486990 (cdecl): one dial at (x, y): a grey quad 0x5A by 0x46 (abr
// 2), then (abr 1) ten vertical and eight horizontal grey lines 10 apart, and
// the grid's marks: column r at x + 10 r shows the grid's column (turn + r)
// mod 9, row b at y + 10 b its row b (grid + 9 b). `turn` read as its low byte.
extern "C" void __cdecl EffectKind7D_DrawDial(unsigned x, unsigned y, const unsigned char* grid, unsigned turn) {
    SH_CALL(EffectKind7D_DrawMode)(2);
    SH_CALL(EffectKind7D_FillF4)(x, y, x + 0x5A, y, x, y + 0x46, x + 0x5A, y + 0x46, 0x40, 0x40, 0x40, 1);
    SH_CALL(EffectKind7D_DrawMode)(1);
    for (U b = 0; b < 10; ++b) SH_CALL(EffectKind7D_Line)(x + 10 * b, y, x + 10 * b, y + 0x45, 0x96, 0x96, 0x96, 0);
    for (U b = 0; b < 8; ++b) SH_CALL(EffectKind7D_Line)(x, y + 10 * b, x + 0x59, y + 10 * b, 0x96, 0x96, 0x96, 0);
    unsigned t = turn & 0xFFu;
    for (U r = 0; r < at::kGridColumns; ++r) {
        if (t >= at::kGridColumns)
            bof3::Fatal("EffectKind7D_DrawDial: the turn is %u, past the grid's nine columns - the original reads on into "
                        "the next rows (docs/effect_3d.md section 7)",
                        t);
        const unsigned char* cell = grid + t;
        for (U b = 0; b < at::kGridRows; ++b) {
            SH_CALL(EffectKind7D_DrawMark)(x + 10 * r, y + 10 * b, *cell);
            cell += at::kGridColumns;
        }
        t = (t + 1) & 0xFFu;
        if (t >= at::kGridColumns) t = 0;
    }
}

// original 0x486AB0 (cdecl, twelve words): a POLY_F4 (Gpu_SetPolyF4) at the
// packet cursor, semi-transparent by abe's low byte; its four points the
// coordinates' low words (fild), the colour (r, g, b) bytes; committed (2, 0x38).
extern "C" void __cdecl EffectKind7D_FillF4(unsigned x0, unsigned y0, unsigned x1, unsigned y1, unsigned x2,
                                             unsigned y2, unsigned x3, unsigned y3, unsigned r, unsigned g,
                                             unsigned b, unsigned abe) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, abe & 0xFFu);
    Fild(p + 8, S16(x0));
    Fild(p + 0xC, S16(y0));
    Fild(p + 0x14, S16(x1));
    Fild(p + 0x18, S16(y1));
    Fild(p + 0x20, S16(x2));
    Fild(p + 0x24, S16(y2));
    p[4] = static_cast<unsigned char>(r);
    p[5] = static_cast<unsigned char>(g);
    Fild(p + 0x2C, S16(x3));
    p[6] = static_cast<unsigned char>(b);
    Fild(p + 0x30, S16(y3));
    SH_CALL(Gfx_CommitPrim)(2, 0x38);
}

// original 0x486B70 (cdecl, ten words): the same with a flat triangle (0x5A7570),
// three points; committed (2, 0x2C).
extern "C" void __cdecl EffectKind7D_FillF3(unsigned x0, unsigned y0, unsigned x1, unsigned y1, unsigned x2,
                                             unsigned y2, unsigned r, unsigned g, unsigned b, unsigned abe) {
    unsigned char* const p = Gfx_PacketNext;
    SH_AT(void (__cdecl*)(unsigned char*), at::kPolyF3.v)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, abe & 0xFFu);
    Fild(p + 8, S16(x0));
    Fild(p + 0xC, S16(y0));
    Fild(p + 0x14, S16(x1));
    Fild(p + 0x18, S16(y1));
    p[4] = static_cast<unsigned char>(r);
    p[5] = static_cast<unsigned char>(g);
    Fild(p + 0x20, S16(x2));
    p[6] = static_cast<unsigned char>(b);
    Fild(p + 0x24, S16(y2));
    SH_CALL(Gfx_CommitPrim)(2, 0x2C);
}

// original 0x486C10 (cdecl, eight words): the same with a flat line
// (Gpu_SetLineF2), two points; committed (2, 0x20).
extern "C" void __cdecl EffectKind7D_Line(unsigned x0, unsigned y0, unsigned x1, unsigned y1, unsigned r,
                                           unsigned g, unsigned b, unsigned abe) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, abe & 0xFFu);
    Fild(p + 8, S16(x0));
    Fild(p + 0xC, S16(y0));
    Fild(p + 0x14, S16(x1));
    p[4] = static_cast<unsigned char>(r);
    p[5] = static_cast<unsigned char>(g);
    p[6] = static_cast<unsigned char>(b);
    Fild(p + 0x18, S16(y1));
    SH_CALL(Gfx_CommitPrim)(2, 0x20);
}

// original 0x486C90 (cdecl): a draw mode (page Gpu_GetTPage(0, abr's low byte,
// 0x380, 0x100), dfe 0, dtd 0) committed (2, 0xC).
extern "C" void __cdecl EffectKind7D_DrawMode(unsigned abr) {
    const U page = SH_CALL(Gpu_GetTPage)(0, abr & 0xFFu, 0x380, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
}

// original 0x486CE0 (cdecl): a grid cell's mark at (x, y) by its byte - 1 a red
// cross ((x+2, y+5) - (x+8, y+5) and (x+5, y+2) - (x+5, y+8)), 0xFF a blue bar
// ((x+2, y+5) - (x+8, y+5)), anything else nothing.
extern "C" void __cdecl EffectKind7D_DrawMark(unsigned x, unsigned y, unsigned mark) {
    const unsigned m = mark & 0xFFu;
    if (m == 1) {
        SH_CALL(EffectKind7D_Line)(x + 2, y + 5, x + 8, y + 5, 0xC8, 0, 0, 0);
        SH_CALL(EffectKind7D_Line)(x + 5, y + 2, x + 5, y + 8, 0xC8, 0, 0, 0);
    } else if (m == 0xFF) {
        SH_CALL(EffectKind7D_Line)(x + 2, y + 5, x + 8, y + 5, 0, 0, 0xC8, 0);
    }
}

// original 0x486D60 (0x1A6 bytes, PSX twin 0x801F3994; called by Area170_Init
// and kind 0x7D's state 2): the 7 x 9 cells from (0x16, 0x8E) on the map. A
// cell is on when story flag 0x7E is clear and any of the three grids, turned
// by its dial, marks it (grid i's row, column (dial + column) - 9 when that is
// 9 or more). Each cell: AreaMap_SetByte(column, row, the height 0x654BCC
// holds when on, else 0); the low byte of the area block's dword the cell word
// names (AreaMap_Header: width w, depth d, base; the word at 0x8CB5AC + 2 (w
// row + column + 2 base), the dword (d w + 1) / 2 + base + that word) set from
// 0x654B4C (off or on); and the draw item MapView_ItemAt answers there, if any,
// retextured from that dword (Prim_SetTexture, 2).
extern "C" void __cdecl EffectKind7D_SetMap(void) {
    const bool open = SH_CALL(Flags_Test)(P(::bof3::Img(Cond_Flags_addr) + 0xa0), at::kDialsSolvedFlag) == 0;
    for (U row = 0; row < at::kGridRows; ++row) {
        for (U column = 0; column < at::kGridColumns; ++column) {
            unsigned on = 0;
            if (open) {
                unsigned sum = 0;
                for (U i = 0; i < at::kDialCount; ++i) {
                    unsigned turned = (B(::bof3::Img(at::kDials.v) + i) + column) & 0xFFu;
                    if (turned >= 9) turned = (turned - 9) & 0xFFu;
                    if (turned >= at::kGridColumns)
                        bof3::Fatal("EffectKind7D_SetMap: dial %u is %u, past the grid's nine columns - the original "
                                    "reads on into the next rows (docs/effect_3d.md section 7)",
                                    (unsigned)i, (unsigned)B(::bof3::Img(at::kDials.v) + i));
                    sum = (sum + B(::bof3::Img(at::kDialGrids.v) + at::kGridSize * i + at::kGridColumns * row + turned)) & 0xFFu;
                }
                on = sum != 0 ? 1u : 0u;
            }
            const unsigned height = (B(::bof3::Img(at::kMapHeights.v) + at::kGridColumns * row + column) * on) & 0xFFu;
            SH_CALL(AreaMap_SetByte)(column + at::kMapColumn0, row + at::kMapRow0, height);
            const U width = AreaMap_Header[0];
            const U z = row + at::kMapRow0;
            const U base = W(::bof3::Img(at::kAreaHeader.v) + 2);
            const U word = W(::bof3::Img(at::kAreaWords.v) + 2 * (width * z + column + 2 * base));
            const U index = (AreaMap_Header[1] * width + 1) / 2 + base + word;
            unsigned char* const cell = P(::bof3::Img(AreaMap_Header_addr) + 4 * index);
            SetUL(cell, UL(cell) & 0xFFFFFF00u);
            SetUL(cell, UL(cell) | B(::bof3::Img(at::kMapBits.v) + at::kGridColumns * (at::kGridRows * on + row) + column));
            const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(static_cast<long>(column + at::kMapColumn0),
                                                                 static_cast<long>(z)));
            if (item != 0)
                SH_CALL(Prim_SetTexture)(UL(cell), draw_pool::Items() + item * at::kDrawItemStride, 2);
        }
    }
}

// original 0x486F10 (Effect_KindHandlers[0x7F], hidden in 0x486D60): jmp
// [EffectKind7F_States + +1 * 4].
extern "C" void __cdecl EffectKind7F_Run(void) {
    Dispatch("EffectKind7F_Run", AddressOf(EffectKind7F_States), EffectKind7F_States_count, 1);
}

// original 0x486F30 (state 0): +0x2E 0, +0x30 0x100, +9 0x28, +1 up;
// Sound_PlayEffect(0x206).
extern "C" void __cdecl EffectKind7F_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0x100);
    S()[9] = 0x28;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x206);
}

// original 0x486F70 (state 1): the cone at the record's point (+0x34) between
// the radii +0x2E and +0x30, both then 0x40 wider; +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind7F_Grow(void) {
    unsigned char* const s = S();
    SH_CALL(EffectKind7F_DrawCone)(reinterpret_cast<const long*>(s + 0x34), static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e), static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e) + 0x40);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30) + 0x40);
    if (CountDown()) NextState();
}

// original 0x486FC0 (0x2A1 bytes, cdecl; PSX twin 0x801F2DA0): a draw mode (page
// Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1) committed (2, 0xC); the map camera;
// then 32 Gouraud quads round the point at angles 0x80 k between the inner rim
// (radius r1) and the outer (r2), grey 0x80 inside and black outside, each with
// a draw mode before it, both linked at (x, the previous outer point's z).
// r1 and r2 read as their low words.
extern "C" void __cdecl EffectKind7F_DrawCone(const long* point, unsigned r1, unsigned r2) {
    const U page = SH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    long rim[3];
    float inner[3], outer[3];
    ConePoint(rim, point, 0, r1, inner, true);
    ConePoint(rim, point, 0, r2, outer, false);
    U angle = 0;
    for (int n = 0x20; n != 0; --n) {
        const U lx = static_cast<U>(rim[0]), lz = static_cast<U>(rim[1]);
        const U pg = SH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, pg & 0xFFFFu, 0);
        SH_CALL(MapView_LinkPrimAt)(lx, lz, 0, 0xC);
        unsigned char* const q = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(q);
        SH_CALL(Gpu_SetSemiTrans)(q, 1);
        std::memcpy(q + 8, inner, 12);
        std::memcpy(q + 0x18, outer, 12);
        angle += 0x80;
        const U a = angle & 0xFFFFu;
        ConePoint(rim, point, a, r1, inner, true);
        ConePoint(rim, point, a, r2, outer, false);
        std::memcpy(q + 0x28, inner, 12);
        std::memcpy(q + 0x38, outer, 12);
        q[4] = 0x80;
        q[5] = 0x80;
        q[6] = 0x80;
        q[0x14] = 0;
        q[0x15] = 0;
        q[0x16] = 0;
        q[0x24] = 0x80;
        q[0x25] = 0x80;
        q[0x26] = 0x80;
        q[0x34] = 0;
        q[0x35] = 0;
        q[0x36] = 0;
        SH_CALL(MapView_LinkPrimAt)(lx, lz, 0, 0x44);
    }
}

void EffectKind7d_Inject() {
    BOF3_INJECT(EffectKind7D_Run);
    BOF3_INJECT(EffectKind7D_Start);
    BOF3_INJECT(EffectKind7D_Input);
    BOF3_INJECT(EffectKind7D_Apply);
    BOF3_INJECT(EffectKind7D_DrawPanel);
    BOF3_INJECT(EffectKind7D_DrawDial);
    BOF3_INJECT(EffectKind7D_FillF4);
    BOF3_INJECT(EffectKind7D_FillF3);
    BOF3_INJECT(EffectKind7D_Line);
    BOF3_INJECT(EffectKind7D_DrawMode);
    BOF3_INJECT(EffectKind7D_DrawMark);
    BOF3_INJECT(EffectKind7D_SetMap);
    BOF3_INJECT(EffectKind7F_Run);
    BOF3_INJECT(EffectKind7F_Start);
    BOF3_INJECT(EffectKind7F_Grow);
    BOF3_INJECT(EffectKind7F_DrawCone);
}
