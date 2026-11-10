// Moved here by tools/move_funcs.py from effect_5c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_11.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
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

using move_script::SetWord;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U AddressOf(const void* p) { return Low32(p); }

// Prim_VertexScratch 0x9037A0: four SVECTORs of 8 bytes (a0, a8, b0, b8).
unsigned char* VSEffect5c(unsigned off) { return reinterpret_cast<unsigned char*>(Prim_VertexScratch) + off; }

const short* VX(unsigned off) { return reinterpret_cast<const short*>(VSEffect5c(off)); }

float* F(unsigned char* p) { return reinterpret_cast<float*>(p); }

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

// The four vertices in Prim_VertexScratch projected into a quad's four screen
// points at the given offsets (the depth out a local nothing reads).
void Project4Effect5c(unsigned char* prim, unsigned o0, unsigned o1, unsigned o2, unsigned o3) {
    long depth = 0;
    SH_CALL(Gte_RotTransPers4)(VX(0), VX(8), VX(0x10), VX(0x18), F(prim + o0), F(prim + o1), F(prim + o2), F(prim + o3),
                               &depth);
}

// Sub-kind 0x11's end: story flag 0x22, or 0x21 with Cond_ByteFA (s8) at most
// 7 - in the original's order; sub-kind 0x12's: story flag 0x20.
bool Sub11Done() {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x22)) return true;
    if (!SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x21)) return false;
    return Cond_ByteFA <= 7;
}

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

void Draw11(unsigned y, unsigned shade) { SH_CALL(EffectKind18Sub11_DrawTiles)(y, shade); }

}  // namespace

// original 0x502810 (EffectKind18_States[17], hidden in 0x502670):
// jmp [EffectKind18Sub11_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub11_Run(void) {
    DispatchEffect5c("EffectKind18Sub11_Run", AddressOf(EffectKind18Sub11_States), EffectKind18Sub11_States_count);
}

// original 0x502830 (sub-state 0).
extern "C" void __cdecl EffectKind18Sub11_Start(void) { TileStart<&Sub11Done, &Draw11>(); }

// original 0x5028C0 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub11_Wait(void) { TileWait<&Sub11Done, &Draw11>(); }

// original 0x502940 (sub-state 2).
extern "C" void __cdecl EffectKind18Sub11_Open(void) { TileOpen<&Draw11>(); }

// original 0x5029B0 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub11_Sink(void) { TileSink<&Draw11>(); }

// original 0x502A00: the fifteen tiles of EffectKind18Sub11_Tiles 0x65E11C.
extern "C" void __cdecl EffectKind18Sub11_DrawTiles(unsigned y, unsigned shade) { Tiles(AddressOf(EffectKind18Sub11_Tiles), y, shade); }

void EffectKind18Sub11_Inject() {
    BOF3_INJECT(EffectKind18Sub11_Run);
    BOF3_INJECT(EffectKind18Sub11_Start);
    BOF3_INJECT(EffectKind18Sub11_Wait);
    BOF3_INJECT(EffectKind18Sub11_Open);
    BOF3_INJECT(EffectKind18Sub11_Sink);
    BOF3_INJECT(EffectKind18Sub11_DrawTiles);
}
