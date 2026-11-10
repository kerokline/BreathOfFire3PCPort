// The Direct3D draw handlers, their state helpers and the two texture lookups
// under them, read to the last instruction with capstone against bof3/BOF3.exe
// (docs/d3d-draw.md). All are the PC port's own: the PlayStation drew these
// primitives on its GPU, so there is no PSX twin to read them against. Faithful
// but for DIVERGENCE DIV-0181 (the cell textures, below) and DIV-0199 (the
// textured quads' far texture edge, game/quad_far_edge.h).
//
//   D3d_PrimColor          0x59FBA0..0x59FC83 (0xE4), jump table 0x59FC84
//   D3d_SetBlend           0x59FCA0..0x59FD68 (0xC9), jump table 0x59FD6C
//   D3d_SetShadeMode       0x59FD80..0x59FDA3 (0x24)
//   D3d_BindTexture        0x59FFE0..0x5A0076 (0x97)
//   D3d_DrawPolyFT4        0x5A0C40..0x5A0E7C (0x23D)  code 0x2C
//   D3d_DrawPolyGT4        0x5A14C0..0x5A1793 (0x2D4)  code 0x3C
//   D3d_DrawLineF2         0x5A17A0..0x5A18AE (0x10F)  code 0x40
//   D3d_DrawLineF4         0x5A1D10..0x5A1E94 (0x185)  code 0x4C
//   D3d_DrawTile           0x5A20D0..0x5A2219 (0x14A)  code 0x60
//   D3d_DrawCellSprite     0x5A2EB0..0x5A3159 (0x2AA)  code 0x84
//   D3d_CellTexture        0x5A3160..0x5A32A3 (0x144)
//
// The arithmetic is x87 in the original and x87 here, instruction for
// instruction (the X87* helpers below): each fmul, fadd and fdiv rounds to the
// control word's precision, and a float that passes through fld / fstp comes
// out with a signalling NaN quieted, so only the same sequence gives the same
// bits under every control word. The fuzz checks three (0x027F, the game's
// measured one, 0x007F and 0x037F).
#include "game/d3d_draw.h"

#include "game/d3d_lines.h"

#include <cstdint>
#include <windows.h>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/d3d_draw_callees.h"
#include "game/d3d_list_callees.h"
#include "game/quad_far_edge.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"
#include "render/fb_grab.h"
#include "render/render_shim.h"

namespace d3d_draw {


namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

// --- the x87 sequences, as the original executes them ------------------------

// --- the device --------------------------------------------------------------

}  // namespace
// DIVERGENCE DIV-0199 (D198): a screen-rectangle quad's texture edges
// (game/quad_far_edge.h), by the draw's filter,
// over the four vertices just written; uv the first corner's u, v bytes and
// `stride` the primitive's corner size. Off (0) until D3dDraw_Inject, after the
// self-tests, and under BOF3X_ORIGINAL=QuadFarEdge.
std::uint8_t g_quad_far = 0;  }  // namespace d3d_draw

using namespace d3d_draw;

// --- the helpers -----------------------------------------------------------

// --- the handlers ------------------------------------------------------------
// Each is reached from one site of the draw 0x59EE50's second jump table and
// returns what DrawPrimitive returned (the caller does not read it). Not kept,
// in all of them: the original lets the colour helper write the diffuse into
// the caller's pushed `prim` slot, which the caller pops unread.

// 0x5A3160. Finds - or builds - the texture composed from `count` SpriteCell
// records from `first` in one CLUT, sets it on stage 0 and returns its index in
// D3d_CellTexCache: 128 entries of 0x28 - u16 used w, h +0, +2; u16 texture w,
// h +4, +6; s16 extent +8..+0xE; u16 CLUT +0x10; u16 count +0x12; u16 used this
// frame +0x14; u16 use counter +0x16; the cells' checksum +0x18; the CLUT row's
// generation +0x1C; surface +0x20; texture +0x24.
//
// One pass: an empty entry (its first dword 0) is built at once. A hit is the
// count and CLUT words equal to the arguments (32-bit compares, words
// zero-extended) and the checksum - the 32-bit sum of the records' 2 * count
// dwords - equal; a hit whose CLUT row has changed is refreshed unless it was
// used this frame already. On the way, the victim: of the entries not used this
// frame, the one with the smallest use counter below 0xFFFF, the first on a tie.
// No hit and no empty entry: the victim is built.
//
// Kept, and D27 in docs/known-defects.md: the victim's index lives in the
// stack slot of the `count` argument, so when no entry qualifies - all 128 used
// this frame - the "victim" is `count` itself, and the build, the counter and
// the in-use word land at D3d_CellTexCache + count * 0x28, past the table.

// ---------------------------------------------------------------------------
// DIV-0181: cell textures follow VRAM uploads. The port's cell
// texture cache (D3d_CellTexture) keys an entry on the cell records and the
// CLUT and refreshes it only when the CLUT row's generation moves - never when
// the VRAM under the cells is rewritten, so a sprite whose image changes while
// its frame's parts stay the same keeps drawing the old composition. Here each
// built entry records the VRAM footprint of its cells, and an upload that
// overlaps it frees the entry (D3d_FreeCellTexture), so the next draw rebuilds
// it from the new VRAM. BOF3X_ORIGINAL=CellTextureVram keeps the port's.
// BOF3X_POSELOG=1 logs builds and frees (the facing investigation).
void D3d_FreeCellTexture(int slot);
namespace {
std::uint8_t g_cell_vram = 0;   // 1 through PatchBytes("CellTextureVram") in D3dDraw_Inject
}  // namespace
namespace d3d_draw { CellFoot g_cell_foot[128]; }  namespace {
}  namespace d3d_draw { bool PoseLogOn() {
    static int on = -1;
    if (on < 0) { char v[8]; on = GetEnvironmentVariableA("BOF3X_POSELOG", v, sizeof v) != 0 && v[0] == '1'; }
    return on == 1;
} }  void D3dDraw_CellsShadowWritten(int x, int y, int w, int h) {
    if (!g_cell_vram) return;
    const int x1 = x + w - 1, y1 = y + h - 1;
    for (U i = 0; i < kCellEntries; ++i) {
        CellFoot& f = d3d_draw::g_cell_foot[i];
        if (!f.valid) continue;
        if (f.x1 < x || f.x0 > x1 || f.y1 < y || f.y0 > y1) continue;
        if (Long(At(::bof3::Img(D3d_CellTexCache_addr) + i * kCellEntry)) != 0) {
            if (d3d_draw::PoseLogOn())
                bof3::Log("poselog     frame %lu cell free entry %u (upload %d,%d %dx%d over foot %d,%d..%d,%d)", static_cast<unsigned long>(Frame_Counter),
                          static_cast<unsigned>(i), x, y, w, h, f.x0, f.y0, f.x1, f.y1);
            D3d_FreeCellTexture(static_cast<int>(i));
        }
        f.valid = 0;
    }
}

void D3dDraw_Inject() {
    if (bof3::WantsShadow("d3d_draw")) {
        d3d_draw::SelfTest();
        d3d_list::SelfTest();
    }
    if (bof3::WantsShadow("fb_grab")) render::fb_grab::SelfTest();   // DIV-0125
    {
        // DIV-0181: cell textures follow VRAM uploads. After the self-tests, whose
        // fuzzed builds noted footprints over fake entries: those are forgotten here.
        std::memset(d3d_draw::g_cell_foot, 0, sizeof d3d_draw::g_cell_foot);
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("CellTextureVram", Low32(&g_cell_vram), &was, &is, 1);
        bof3::Log("DIV-0181    a cell texture is rebuilt after an upload over its cells (on unless the line above says OFF)");
    }
    {
        // DIV-0199: the textured quads' far texture edge, after the self-tests
        // (which compare Capcom's texture coordinates).
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("QuadFarEdge", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&d3d_draw::g_quad_far)), &was, &is, 1);
        bof3::Log("DIV-0199    a screen-rectangle textured quad samples the texels the PlayStation draws: %s",
                  d3d_draw::g_quad_far ? "on" : "OFF (BOF3X_ORIGINAL=QuadFarEdge)");
    }
    d3d_list::Operands();   // Gfx_DrawOTag's byte tables, ours, against the code's (docs/operand-store.md)
    BOF3_INJECT(Gfx_DrawOTag);
}
