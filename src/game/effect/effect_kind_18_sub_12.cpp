// Moved here by tools/move_funcs.py from effect_5c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_12.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5c.h"
#include "game/effect_5c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"

namespace {

namespace at = effect_5c::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

std::int32_t SWEffect5c(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(I(v) >> n); }

// Prim_VertexScratch 0x9037A0: four SVECTORs of 8 bytes (a0, a8, b0, b8).
unsigned char* VSEffect5c(unsigned off) { return reinterpret_cast<unsigned char*>(Prim_VertexScratch) + off; }

const short* VX(unsigned off) { return reinterpret_cast<const short*>(VSEffect5c(off)); }

float* F(unsigned char* p) { return reinterpret_cast<float*>(p); }

void Float(unsigned char* p, std::int32_t v) {
    const float f = static_cast<float>(v);   // fild / fstp: exact for these magnitudes
    std::memcpy(p, &f, 4);
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through the
// dword after - the next table or data.
void DispatchEffect5c(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5c.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// The map: AreaMap_Header's first byte the row length, AreaMap_Bytes a byte a
// cell, AreaMap_Corners a dword a cell - each read again for every store, as
// the originals read them.
unsigned MapWidth() { return AreaMap_Header[0]; }

void SetCell(U offset, unsigned char v) { const_cast<unsigned char*>(AreaMap_Bytes.Native())[offset] = v; }

void SetCorner(U index, U v) { SetUL(At(AddressOf(&AreaMap_Corners) + 4 * index), v); }

// The four vertices in Prim_VertexScratch projected into a quad's four screen
// points at the given offsets (the depth out a local nothing reads).
void Project4Effect5c(unsigned char* prim, unsigned o0, unsigned o1, unsigned o2, unsigned o3) {
    long depth = 0;
    SH_CALL(Gte_RotTransPers4)(VX(0), VX(8), VX(0x10), VX(0x18), F(prim + o0), F(prim + o1), F(prim + o2), F(prim + o3),
                               &depth);
}

bool Sub12Done() { return SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x20) != 0; }

// The map bytes of cells (0x45, 0x3C) and (0x46, 0x3C).
void TileCells(unsigned char v) {
    SetCell(MapWidth() * 60u + 0x45u, v);
    SetCell(MapWidth() * 60u + 0x46u, v);
}

// The fifteen tiles of `table` (x, z, row offset bytes): a horizontal textured
// POLY_FT4 a cell wide at y, z widened by the shade, texture shade << 16 |
// 0xB600B102, linked at the cell (0x48).
void Tiles(U table, U y, U shade) {
    const U texture = (shade << 16) | at::kTextureTiles;
    for (U e = table + 1; e < table + 1 + 3 * at::kTileCount; e += 3) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        const U x = static_cast<U>(At(e - 1)[0]) << 7;
        SetWord(VSEffect5c(0x1C), y);
        SetWord(VSEffect5c(0x14), y);
        SetWord(VSEffect5c(0x10), x - 0x4040u);
        SetWord(VSEffect5c(0), x - 0x4040u);
        const U z = static_cast<U>(At(e)[0]) << 7;
        SetWord(VSEffect5c(0x18), x - 0x3FC0u);
        SetWord(VSEffect5c(8), x - 0x3FC0u);
        SetWord(VSEffect5c(0xC), y);
        SetWord(VSEffect5c(4), y);
        SetWord(VSEffect5c(0xA), z - shade - 0x4040u);
        SetWord(VSEffect5c(2), z - shade - 0x4040u);
        SetWord(VSEffect5c(0x1A), z + shade - 0x4040u);
        SetWord(VSEffect5c(0x12), z + shade - 0x4040u);
        Project4Effect5c(prim, 8, 0x18, 0x28, 0x38);
        SH_CALL(Gte_PrimDepths4_10)(prim);
        SH_CALL(Prim_SetTexture)(texture, prim, 1);
        SH_CALL(MapView_LinkPrimAt)(static_cast<U>(At(e - 1)[0]) << 16, static_cast<U>(At(e)[0]) << 16, At(e + 1)[0], 0x48);
    }
}

// Sub-state 0: done - Cond_ByteFE = 1 and a tail jump to Effect_Release; else
// the cells 0x10, the tiles at -0x100 shade 0x80, +2 up.
template <bool (*Done)(), void (*Draw)(unsigned, unsigned)> void TileStart() {
    if (Done()) {
        Cond_ByteFE = 1;
        SH_CALL(Effect_Release)();
        return;
    }
    TileCells(0x10);
    Draw(0xFFFFFF00u, 0x80);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// Sub-state 1: done - Cond_ByteFE = 1, +0x3C = -0x100, +0xB = 0x80, +9 = 0xA,
// +2 up, the tiles at -0x100 shade 0x80. Else nothing (not even drawn).
template <bool (*Done)(), void (*Draw)(unsigned, unsigned)> void TileWait() {
    if (!Done()) return;
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    Cond_ByteFE = 1;
    so->height = 0xFFFFFF00u;
    s[0xB] = 0x80;
    s[9] = 0xA;
    s[2] = static_cast<unsigned char>(s[2] + 1);
    Draw(0xFFFFFF00u, 0x80);
}

// Sub-state 2: the tiles at +0x3C shade 0x80; +9 down; at 0 the cells 0, +2 up.
template <void (*Draw)(unsigned, unsigned)> void TileOpen() {
    Draw(static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height), 0x80);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    TileCells(0);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// Sub-state 3: the tiles at +0x3C shade +0xB & 0xF8; +0x3C up 1; +0xB down 2;
// above -0xC0 a tail jump to Effect_Release.
template <void (*Draw)(unsigned, unsigned)> void TileSink() {
    unsigned char* s = S();
    Draw(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height), s[0xB] & 0xF8u);
    s = S();
    rv::As<records::EffectObject>(s)->height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) + 1u;
    s[0xB] = static_cast<unsigned char>(s[0xB] - 2);
    if (rv::As<records::EffectObject>(s)->height > -0xC0) SH_CALL(Effect_Release)();
}

void Draw12(unsigned y, unsigned shade) { SH_CALL(EffectKind18Sub12_DrawTiles)(y, shade); }

// A texture-window RECT {x, 0, w, h} of 8 bytes at the cursor, the cursor past
// it, and a draw mode (page 0x95) with that window committed at slot 7 (0xC).
void WindowMode(U x, U w) {
    unsigned char* const rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetWord(rect, x);
    SetWord(rect + 2, 0);
    SetWord(rect + 6, w);
    SetWord(rect + 4, w);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, AddressOf(rect));
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
}

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32 (the original's 0x20-byte local).
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};

// Math_Cos(((frame + k) << 9) & 0xFFF) * scale >> 11, + 0x80: a shade byte.
unsigned char Wave(U angle, U scale) {
    const int c = SH_CALL(Math_Cos)(static_cast<int>((angle << 9) & 0xFFFu));
    return static_cast<unsigned char>((I(static_cast<U>(c) * scale) >> 11) + 0x80);
}

}  // namespace

// original 0x502B30 (EffectKind18_States[18], hidden in 0x502A00):
// jmp [EffectKind18Sub12_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub12_Run(void) {
    DispatchEffect5c("EffectKind18Sub12_Run", AddressOf(EffectKind18Sub12_States), EffectKind18Sub12_States_count);
}

// original 0x502B50 (sub-state 0).
extern "C" void __cdecl EffectKind18Sub12_Start(void) { TileStart<&Sub12Done, &Draw12>(); }

// original 0x502BC0 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub12_Wait(void) { TileWait<&Sub12Done, &Draw12>(); }

// original 0x502C20 (sub-state 2).
extern "C" void __cdecl EffectKind18Sub12_Open(void) { TileOpen<&Draw12>(); }

// original 0x502C90 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub12_Sink(void) { TileSink<&Draw12>(); }

// original 0x502CE0: the fifteen tiles of EffectKind18Sub12_Tiles 0x65E15C.
extern "C" void __cdecl EffectKind18Sub12_DrawTiles(unsigned y, unsigned shade) { Tiles(AddressOf(EffectKind18Sub12_Tiles), y, shade); }

// original 0x502E10 (EffectKind18_States[21], hidden in 0x502CE0):
// jmp [EffectKind18Sub15_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub15_Run(void) {
    DispatchEffect5c("EffectKind18Sub15_Run", AddressOf(EffectKind18Sub15_States), EffectKind18Sub15_States_count);
}

// original 0x502E30 (sub-state 0): Camera_Angles' first word 0xFCC0;
// Gte_SetGeomOffset(0xA0, 0x90); +2 up.
extern "C" void __cdecl EffectKind18Sub15_Start(void) {
    SetWord(reinterpret_cast<unsigned char*>(Camera_Angles), 0xFCC0);
    SH_CALL(Gte_SetGeomOffset)(0xA0, 0x90);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x502E60 (sub-state 1): a 32 x 32 texture window at (0xE0, 0) and an
// unshaded textured quad 0..320 x 56..88 committed at slot 7 (0x48); a 256 x
// 256 window and three textured quads scrolled by Frame_Counter / 8 (64 high,
// edges 0x00 / 0xFF / 0x3F in u) committed to layer 15's list; a draw mode
// (page 0xB5, dtd) and four semi-transparent gradient quads (white above,
// black below) across 0..320 whose lower edges rise with Field_Kind2X; a draw
// mode (page 0xB5); then AreaMap_Corners of columns 0x2D..0x30 rebuilt round
// Field_Kind2Z's row: rows row + 3 and row - 3 flat (0x20 each corner), rows
// row + 4 .. row + 13 and row - 4 .. row - 17 a slope from 0x20 up.
//
// DIV-0041 (2026-10-07, the owner's bridge of area 41): under the wide picture
// the haze band and the gradients run from -53 to 373, the strips are clipped
// to those bounds instead of 0..320 with one more strip on the left so the
// scroll never leaves a gap, and a strip that starts past the right bound is
// skipped (the original's x0 > x1 quad, off its picture, was the growing
// sliver in the band). Widescreen_Fill() is 0 until the self-tests have run,
// so the fuzz and every narrow run get the original's packets bit for bit.
extern "C" void __cdecl EffectKind18Sub15_Draw(void) {
    const U wide = Widescreen_Fill();
    const U left_bits = std::bit_cast<U>(Widescreen_FillX());                           // 0.0f narrow
    const U right_bits = std::bit_cast<U>(320.0f + static_cast<float>(wide));           // 0x43A00000 narrow
    WindowMode(0xE0, 0x20);
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 1);
    SetUL(prim + 0xC, 0x42600000u);   // 56.0f
    SetUL(prim + 0x1C, 0x42600000u);
    SetUL(prim + 0x2C, 0x42B00000u);   // 88.0f
    SetUL(prim + 0x3C, 0x42B00000u);
    SetUL(prim + 8, left_bits);
    SetUL(prim + 0x18, right_bits);   // 320.0f
    SetUL(prim + 0x28, left_bits);
    SetUL(prim + 0x38, right_bits);
    prim[0x14] = 0;
    prim[0x15] = 0;
    prim[0x24] = 0xFF;
    prim[0x25] = 0;
    prim[0x34] = 0;
    prim[0x35] = 0x40;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x40;
    SetWord(prim + 0x26, 0x95);
    SetWord(prim + 0x16, 0x7900);
    SH_CALL(Gfx_CommitPrim)(7, 0x48);
    WindowMode(0, 0x100);
    // The wide picture: strips of 256 at phase - 512, - 256, 0 and + 256,
    // each clipped to [-wide, 320 + wide] with the u range the clip leaves
    // (the original's rule: u runs from the clipped-off width on the left,
    // and to the visible width less one on the right).
    for (U col = wide ? 0u : 0x100u; wide && col < 0x400; col += 0x100) {
        const std::int32_t left = static_cast<std::int32_t>((Frame_Counter >> 3) & 0xFFu) + static_cast<std::int32_t>(col) - 0x200;
        const std::int32_t lo = -static_cast<std::int32_t>(wide), hi = 320 + static_cast<std::int32_t>(wide);
        const std::int32_t x0 = left > lo ? left : lo, x1 = left + 0x100 < hi ? left + 0x100 : hi;
        if (x1 <= x0) continue;
        prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 1);
        Float(prim + 8, x0);
        Float(prim + 0x28, x0);
        Float(prim + 0x18, x1);
        Float(prim + 0x38, x1);
        prim[0x14] = prim[0x34] = static_cast<unsigned char>(x0 - left);
        prim[0x24] = prim[0x44] = static_cast<unsigned char>(x1 - left - 1);
        SetUL(prim + 0x1C, 0);
        SetUL(prim + 0xC, 0);
        SetUL(prim + 0x3C, 0x42800000u);   // 64.0f
        SetUL(prim + 0x2C, 0x42800000u);
        prim[0x25] = 0;
        prim[0x15] = 0;
        prim[0x45] = 0x3F;
        prim[0x35] = 0x3F;
        SetWord(prim + 0x26, 0x99);
        SetWord(prim + 0x16, 0x7980);
        SH_CALL(EffectKind18Sub15_LinkLayer)(0x48);
    }
    for (U col = 0; !wide && col < 0x300; col += 0x100) {
        prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 1);
        const U left = ((Frame_Counter >> 3) & 0xFFu) + col - 0x100u;
        if (I(left) < 0) {
            SetUL(prim + 0x28, 0);
            SetUL(prim + 8, 0);
            const unsigned char u = static_cast<unsigned char>(0u - (left & 0xFFu));
            prim[0x34] = u;
            prim[0x14] = u;
            Float(prim + 0x38, I(left + 0x100u));
            Float(prim + 0x18, I(left + 0x100u));
            prim[0x44] = 0xFF;
            prim[0x24] = 0xFF;
        } else {
            Float(prim + 0x28, I(left));
            Float(prim + 8, I(left));
            const U right = left + 0x100u;
            if (I(right) > 0x140) {
                const unsigned char u = static_cast<unsigned char>(0x3Fu - (left & 0xFFu));
                SetUL(prim + 0x38, 0x43A00000u);
                SetUL(prim + 0x18, 0x43A00000u);
                prim[0x34] = 0;
                prim[0x14] = 0;
                prim[0x44] = u;
                prim[0x24] = u;
            } else {
                prim[0x34] = 0;
                prim[0x14] = 0;
                Float(prim + 0x38, I(right));
                Float(prim + 0x18, I(right));
                prim[0x44] = 0xFF;
                prim[0x24] = 0xFF;
            }
        }
        SetUL(prim + 0x1C, 0);
        SetUL(prim + 0xC, 0);
        SetUL(prim + 0x3C, 0x42800000u);   // 64.0f
        SetUL(prim + 0x2C, 0x42800000u);
        prim[0x25] = 0;
        prim[0x15] = 0;
        prim[0x45] = 0x3F;
        prim[0x35] = 0x3F;
        SetWord(prim + 0x26, 0x99);
        SetWord(prim + 0x16, 0x7980);
        SH_CALL(EffectKind18Sub15_LinkLayer)(0x48);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(EffectKind18Sub15_LinkLayer)(0xC);
    const U shift = 0u - Sar(static_cast<U>(Field_Kind2X) - 0x2D0000u, 15);
    // the four quads at the cursor as it was here, 0x44 apart whatever the
    // commits did (the original's esi is never read again from the cursor)
    unsigned char* quad = Gfx_PacketNext;
    for (U k = 0; k < 4; ++k, quad += 0x44) {
        SH_CALL(Gpu_SetPolyG4)(quad);
        SH_CALL(Gpu_SetSemiTrans)(quad, 1);
        const U x0 = Word(At(AddressOf(EffectKind18Sub15_Columns) + 2 * k));
        const U x1 = Word(At(AddressOf(EffectKind18Sub15_Columns) + 2 * k + 2));
        Float(quad + 8, I(x0));
        SetUL(quad + 0xC, 0x42800000u);
        Float(quad + 0x18, I(x1));
        SetUL(quad + 0x1C, 0x42800000u);
        Float(quad + 0x28, I(x0));
        Float(quad + 0x2C, I(At(AddressOf(EffectKind18Sub15_Rows) + k)[0] + shift));
        Float(quad + 0x38, I(x1));
        Float(quad + 0x3C, I(At(AddressOf(EffectKind18Sub15_Rows) + k + 1)[0] + shift));
        quad[4] = 0xFF;
        quad[5] = 0xFF;
        quad[6] = 0xFF;
        quad[0x14] = 0xFF;
        quad[0x15] = 0xFF;
        quad[0x16] = 0xFF;
        quad[0x24] = 0;
        quad[0x25] = 0;
        quad[0x26] = 0;
        quad[0x34] = 0;
        quad[0x35] = 0;
        quad[0x36] = 0;
        SH_CALL(EffectKind18Sub15_LinkLayer)(0x44);
    }
    // The wide picture's bands: a gradient each side, flat at the outer
    // rows' height (the table's 90), from -wide to 0 and from 320 to 320 + wide.
    // They are written at the four quads' cursor, which runs on whether or not
    // a link was refused, so both are skipped - nothing written, nothing
    // linked - unless they fit under this buffer's pool limit (the test
    // EffectKind18Sub15_LinkLayer makes). A refused link above implies the
    // skip: the cursor is then at least 0x44 past Gfx_PacketNext.
    const U bands = 2u * 0x44u;
    const bool bands_fit = wide && (static_cast<U>(Gfx_BufferIndex) << 16) + ::bof3::Img(at::kPoolLimit) > AddressOf(quad) + bands;
    if (wide && !bands_fit && draw_order::Tagging())
        bof3::Log("draworder   packet pool full: the sky's wide bands (%u bytes) skipped (EffectKind18Sub15_Draw)", bands);
    for (U k = 0; bands_fit && k < 2; ++k, quad += 0x44) {
        SH_CALL(Gpu_SetPolyG4)(quad);
        SH_CALL(Gpu_SetSemiTrans)(quad, 1);
        const std::int32_t x0 = k == 0 ? -static_cast<std::int32_t>(wide) : 320;
        const std::int32_t x1 = k == 0 ? 0 : 320 + static_cast<std::int32_t>(wide);
        const std::int32_t row = At(AddressOf(EffectKind18Sub15_Rows) + (k == 0 ? 0u : 4u))[0] + static_cast<std::int32_t>(shift);
        Float(quad + 8, x0);
        SetUL(quad + 0xC, 0x42800000u);
        Float(quad + 0x18, x1);
        SetUL(quad + 0x1C, 0x42800000u);
        Float(quad + 0x28, x0);
        Float(quad + 0x2C, row);
        Float(quad + 0x38, x1);
        Float(quad + 0x3C, row);
        quad[4] = quad[5] = quad[6] = 0xFF;
        quad[0x14] = quad[0x15] = quad[0x16] = 0xFF;
        quad[0x24] = quad[0x25] = quad[0x26] = 0;
        quad[0x34] = quad[0x35] = quad[0x36] = 0;
        SH_CALL(EffectKind18Sub15_LinkLayer)(0x44);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    SH_CALL(EffectKind18Sub15_LinkLayer)(0xC);
    for (U i = 0; i < 0xA; ++i) {
        const U flat = static_cast<U>(SWEffect5c(Word(At(::bof3::Img(at::kKind2ZCell.v))))) + 3u;
        const U lo = static_cast<U>(I(i) / 2) + 0x20u;
        const U hi = static_cast<U>(I(i + 1) / 2) + 0x20u;
        const U slope = lo | (lo << 8) | (hi << 16) | (hi << 24);
        const U row = static_cast<U>(SWEffect5c(Word(At(::bof3::Img(at::kKind2ZCell.v))))) + i + 4u;
        for (U c = 0x2D; c < 0x31; ++c) {
            SetCorner(MapWidth() * flat + c, 0x20202020u);
            SetCorner(MapWidth() * row + c, slope);
        }
    }
    for (U j = 0; j < 0xE; ++j) {
        const U centre = static_cast<U>(SWEffect5c(Word(At(::bof3::Img(at::kKind2ZCell.v)))));
        const U flat = centre - 3u;
        const U lo = static_cast<U>(I(j + 1) / 2) + 0x20u;
        const U hi = static_cast<U>(I(j) / 2) + 0x20u;
        const U slope = lo | (lo << 8) | (hi << 16) | (hi << 24);
        const U row = centre - j - 4u;
        for (U c = 0x2D; c < 0x31; ++c) {
            SetCorner(MapWidth() * flat + c, 0x20202020u);
            SetCorner(MapWidth() * row + c, slope);
        }
    }
}

// original 0x5032C0: the primitive at the cursor (size & 0xFF bytes) appended
// to draw layer 15's third list for this buffer (0x802594 + 8 *
// Gfx_BufferIndex: Gpu_LinkPrim from its last pointer, which then becomes the
// primitive) and the cursor moved on - if the pool has room, as Gfx_CommitPrim
// tests it, and silently not at all if it has not. The buffer byte is not
// checked, as Gfx_CommitPrim's slot is not.
extern "C" void __cdecl EffectKind18Sub15_LinkLayer(unsigned size) {
    unsigned char* const next = Gfx_PacketNext;
    const unsigned buffer = Gfx_BufferIndex;
    const U bytes = size & 0xFFu;
    const U limit = (static_cast<U>(buffer) << 16) + ::bof3::Img(at::kPoolLimit);
    if (limit <= AddressOf(next) + bytes) {
        if (draw_order::Tagging())
            bof3::Log("draworder   packet pool full: the sky's %u-byte primitive skipped (EffectKind18Sub15_LinkLayer)", bytes);
        return;
    }
    SH_CALL(Gpu_LinkPrim)(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(UL(At(::bof3::Img(DrawLayers_addr) + 0x2f4 + 8 * buffer)))),
                          AddressOf(next));
    unsigned char* const now = Gfx_PacketNext;
    SetUL(At(::bof3::Img(DrawLayers_addr) + 0x2f4 + 8u * Gfx_BufferIndex), AddressOf(now));
    Gfx_PacketNext = now + bytes;
}

// original 0x503320 (EffectKind18_States[22], hidden in 0x5032C0; the code past
// its jmp over padding at 0x503360 is its own, nothing else reaches it): on +2
// = 0 the point (+0x34, +0x38, +0x3C) = (-0x3900, -0x3880, -0x7F0) and +2 up.
// Then each frame: the GTE matrix pushed; a draw mode (page 0x95, dtd)
// committed at slot 4; the point turned by the camera (Gte_RotTrans into the
// translation, a zero rotation times Camera_Matrix) and set; sixteen shaded
// POLY_GT4 quads a ribbon 16 wide each along x, waving in y with Math_Sin of
// Frame_Counter, shaded by Math_Cos, committed at slot 4 (0x54); a draw mode
// (page 0x95); the matrix popped.
extern "C" void __cdecl EffectKind18Sub16_Run(void) {
    unsigned char* s = S();
    if (s[2] == 0) {
        rv::As<records::EffectObject>(s)->x = 0xFFFFC700u;
        rv::As<records::EffectObject>(s)->z = 0xFFFFC780u;
        rv::As<records::EffectObject>(s)->height = 0xFFFFF810u;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    s = S();
    const short angles[3] = {0, 0, 0};
    const short point[3] = {static_cast<short>(rv::As<records::EffectObject>(s)->x_frac), static_cast<short>(rv::As<records::EffectObject>(s)->z_frac),
                            static_cast<short>(rv::As<records::EffectObject>(s)->height_frac)};
    Matrix m = {};
    SH_CALL(Gte_RotTrans)(point, m.t);
    SH_CALL(Gte_RotMatrix)(angles, m.m);
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
    for (U b = 0; b < 0x10; ++b) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyGT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        const U half = static_cast<U>(I(b) / 2);
        const unsigned char u0 = static_cast<unsigned char>(b * 0xFEu - half + 0x28u);
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyGT4>(prim)->tpage, 0x1B);
        const unsigned char u1 = static_cast<unsigned char>(0u - half - (b & 1u) - (b << 1) + 0x26u);
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyGT4>(prim)->clut, 0x78CF);
        prim[0x14] = u0;
        prim[0x15] = 0x70;
        prim[0x28] = u1;
        prim[0x29] = 0x70;
        prim[0x3C] = u0;
        prim[0x3D] = 0x90;
        prim[0x50] = u1;
        prim[0x51] = 0x90;
        const U before = b ? b - 1u : 0u;
        unsigned char shade = Wave(before + Frame_Counter, before);
        prim[6] = shade;
        prim[5] = shade;
        prim[4] = shade;
        shade = Wave(Frame_Counter + b, b);
        prim[0x2E] = shade;
        prim[0x2D] = shade;
        prim[0x2C] = shade;
        prim[0x1A] = shade;
        prim[0x19] = shade;
        prim[0x18] = shade;
        shade = Wave(Frame_Counter + b + 1u, b + 1u);
        prim[0x42] = shade;
        prim[0x41] = shade;
        prim[0x40] = shade;
        SetWord(VSEffect5c(0x10), static_cast<U>(I(b << 8) / 16));
        SetWord(VSEffect5c(0), static_cast<U>(I(b << 8) / 16));
        SetWord(VSEffect5c(0x18), static_cast<U>(I((b + 1u) << 8) / 16));
        SetWord(VSEffect5c(8), static_cast<U>(I((b + 1u) << 8) / 16));
        const U back = b ? b - 1u : 0u;
        int sine = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter - back) << 9) & 0xFFFu));
        SetWord(VSEffect5c(2), static_cast<U>(I(static_cast<U>(sine) * back) >> 12));
        sine = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter - b) << 9) & 0xFFFu));
        const U mid = static_cast<U>(I(static_cast<U>(sine) * b) >> 12);
        const U next = Frame_Counter - b - 1u;
        SetWord(VSEffect5c(0x12), mid);
        SetWord(VSEffect5c(0xA), mid);
        sine = SH_CALL(Math_Sin)(static_cast<int>((next << 9) & 0xFFFu));
        SetWord(VSEffect5c(0x1A), static_cast<U>(I(static_cast<U>(sine) * (b + 1u)) >> 12));
        SetWord(VSEffect5c(0xC), 0xFF40);
        SetWord(VSEffect5c(4), 0xFF40);
        SetWord(VSEffect5c(0x1C), 0);
        SetWord(VSEffect5c(0x14), 0);
        Project4Effect5c(prim, 8, 0x1C, 0x30, 0x44);
        SH_CALL(Gte_PrimDepths4_14)(prim);
        SH_CALL(Gfx_CommitPrim)(4, 0x54);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    SH_CALL(Gte_PopMatrix)();
}

void EffectKind18Sub12_Inject() {
    BOF3_INJECT(EffectKind18Sub12_Run);
    BOF3_INJECT(EffectKind18Sub12_Start);
    BOF3_INJECT(EffectKind18Sub12_Wait);
    BOF3_INJECT(EffectKind18Sub12_Open);
    BOF3_INJECT(EffectKind18Sub12_Sink);
    BOF3_INJECT(EffectKind18Sub12_DrawTiles);
    BOF3_INJECT(EffectKind18Sub15_Run);
    BOF3_INJECT(EffectKind18Sub15_Start);
    BOF3_INJECT(EffectKind18Sub15_Draw);
    BOF3_INJECT(EffectKind18Sub15_LinkLayer);
    BOF3_INJECT(EffectKind18Sub16_Run);
}
