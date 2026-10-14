// The draw: the port's DrawOTag, 0x59EE50..0x59F29F (0x450 bytes, then its two
// jump tables and their byte index tables to 0x59F518), read to the last
// instruction with capstone against bof3/BOF3.exe (docs/d3d-draw.md section 1).
// Faithful: no divergence. Its handlers are reached through the same addresses
// the original calls, so the ones that are ours (this group's, D3d_DrawGlyph,
// DIV-0010's sprite copies) run as they do under Capcom's draw.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/d3d_list_callees.h"
#include "hook/code_bytes.h"
#include "hook/draw_order.h"
#include "hook/log.h"
#include "render/render_shim.h"

namespace d3d_list {

namespace {
using Handler = long(__cdecl*)(const unsigned char*);
Handler H(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(address)); }
}  // namespace

// The handlers of each table, by entry, as the original's case blocks call them.
const Callees kOriginals = {
    {
        // the software surfaces, first table 0x59F2A0
        H(0x5A3A60), H(0x5A3B60), H(0x5A3D70), H(0x5A3E90), H(0x5A3FB0), H(0x5A40C0), H(0x5A41A0),
        H(0x5A42E0), H(kRetOnly), H(kRetOnly), H(kRetOnly), H(kRetOnly), H(kRetOnly), H(kRetOnly),
        H(0x5A4400), H(0x5A45B0), H(0x5A4500), H(0x5A4900), H(0x5A46E0), H(0x5A47F0), H(0x5A4C40),
    },
    {
        // Direct3D, second table 0x59F3D8
        H(bof3::addr::D3d_DrawPolyF3), H(bof3::addr::D3d_DrawPolyFT3), H(bof3::addr::D3d_DrawPolyF4), H(bof3::addr::D3d_DrawPolyFT4), H(bof3::addr::D3d_DrawPolyG3),
        H(bof3::addr::D3d_DrawPolyGT3), H(bof3::addr::D3d_DrawPolyG4),
        H(bof3::addr::D3d_DrawPolyGT4), H(bof3::addr::D3d_DrawLineF2), H(bof3::addr::D3d_DrawLineF3), H(bof3::addr::D3d_DrawLineF4), H(bof3::addr::D3d_DrawLineG2), H(bof3::addr::D3d_DrawLineG3), H(bof3::addr::D3d_DrawLineG4),
        H(bof3::addr::D3d_DrawTile), H(bof3::addr::D3d_DrawSprt), H(bof3::addr::D3d_DrawTile1), H(bof3::addr::D3d_DrawGlyph), H(bof3::addr::D3d_DrawSprt8), H(bof3::addr::D3d_DrawSprt16), H(bof3::addr::D3d_DrawCellSprite),
    },
    Gfx_MoveImage,
    // by the original's address (group PH, docs/d3d-rest.md): BOF3X_ORIGINAL=<name> restores Capcom's for the walk
    bof3::orig::D3d_SetAlphaModulate,
    bof3::orig::D3d_AfterDraw,
};
Callees g = kOriginals;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
U Word(U address) {
    std::uint16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}

void Scene(U offset) {   // 0x24 BeginScene, 0x28 EndScene; the device re-read each time
    void** device = *reinterpret_cast<void** volatile*>(At(kDevice));
    reinterpret_cast<long(__stdcall*)(void*)>((*reinterpret_cast<void***>(device))[offset / 4])(device);
}

// Codes 0xE0..0xE3 and 0xE8..0xEB, both tables: the PSX draw mode. Byte +6 bit
// 0 to Gfx_DrawEnable (which the NEXT draw tests on entry), bit 1 to 0x7DED16,
// the tpage word +4 to Gfx_DrawTpage, and the 8 bytes a non-null +8 points at to
// Gfx_TexCacheKey.
void DrawMode(U p) {
    At(kDrawEnable)[0] = At(p + 6)[0] & 1;
    At(kDrawEnable - 1)[0] = (At(p + 6)[0] >> 1) & 1;
    PutWord(kDrawTpage, Word(p + 4));
    const U key = Long(p + 8);
    if (key != 0) {
        PutLong(kTexKey, Long(key));
        PutLong(kTexKey + 4, Long(key + 4));
    }
}
// Codes 0xF0..0xF3: Gfx_TexCacheKey from the 8 bytes +8 points at - not
// checked for null, as the original does not.
void TexKey(U p) {
    const U key = Long(p + 8);
    PutLong(kTexKey, Long(key));
    PutLong(kTexKey + 4, Long(key + 4));
}
// Codes 0xEC..0xEF: a VRAM move, the rect at +8, the destination +0x10, +0x14.
// Ledger draft I (render/fb_grab.h), after the original's move and not in
// it: a move out of a display buffer also has the backend grab that part of
// the picture drawn so far, as the PlayStation's GPU would have moved it. The
// rectangle is read before the move, which clips it in place.
void MoveImage(U p) {
    auto* rect = reinterpret_cast<short*>(At(p + 8));
    const int x = rect[0], y = rect[1], w = rect[2], h = rect[3];
    const int to_x = static_cast<int>(Long(p + 0x10)), to_y = static_cast<int>(Long(p + 0x14));
    g.move_image(rect, to_x, to_y);
    render::MoveFromFrame(x, y, w, h, to_x, to_y);
}

// A primitive's entry by its index, (code & 0xFC) - 0x20: the switch the
// compiler made the two byte tables of (kSoftBytes, 0xD1 bytes; kD3dBytes,
// 0xD5) - the draw's logic, ours since the function is (docs/operand-store.md).
// Entries 0..20 are the handlers of each table, by the PlayStation's GPU code:
// the eight polygons 0x20..0x3C (F3, FT3, F4, FT4, G3, GT3, G4, GT4), the six
// lines 0x40 and 0x48..0x5C (F2, F3, F4, G2, G3, G4), the rectangles 0x60,
// 0x64, 0x68, 0x6C, 0x74 and 0x7C (tile, sprite, 1 x 1 tile, the glyph, 8 x 8
// and 16 x 16 sprites) and the port's cell sprite 0x84; 21 the draw mode (0xE0
// and 0xE8), 22 the VRAM move (0xEC), 23 the texture key (0xF0) and, Direct3D
// only, 24 the alpha modulate (0xF4). Every other index is none: 24 in the
// software table, 25 in Direct3D's.
U Entry(U index, bool software) {
    switch (index) {
    case 0x00: case 0x04: case 0x08: case 0x0C: case 0x10: case 0x14: case 0x18: case 0x1C:
        return index / 4;   // the polygons
    case 0x20: return 8;
    case 0x28: return 9;
    case 0x2C: return 10;
    case 0x30: return 11;
    case 0x38: return 12;
    case 0x3C: return 13;
    case 0x40: return 14;
    case 0x44: return 15;
    case 0x48: return 16;
    case 0x4C: return 17;
    case 0x54: return 18;
    case 0x5C: return 19;
    case 0x64: return 20;
    case 0xC0: case 0xC8: return 21;
    case 0xCC: return 22;
    case 0xD0: return 23;
    case 0xD4: return 24;   // past the software table's 0xD1 bytes: Dispatch never asks it there
    default: return software ? 24 : 25;
    }
}

// One primitive: (code & 0xFC) - 0x20, unsigned, through the byte table to an
// entry. Entries 0..20 are handlers; the software table's 21..23 and the
// Direct3D table's 21..24 are the cases above; the last entry of each is none.
void Dispatch(U p, bool software) {
    const U index = (At(p + 7)[0] & 0xFCu) - 0x20u;
    if (index > (software ? 0xD0u : 0xD4u)) return;
    const U entry = Entry(index, software);
    if (entry < kHandlers) {
        (software ? g.soft : g.d3d)[entry](At(p));
        return;
    }
    switch (entry) {
    case 21: DrawMode(p); return;
    case 22: MoveImage(p); return;
    case 23: TexKey(p); return;
    case 24:
        if (!software) g.alpha_modulate(At(p + 8)[0] ^ 1u);
        return;
    case 25:
        if (!software) return;
        break;
    default: break;
    }
    bof3::Fatal("Gfx_DrawOTag: byte table entry %u at code 0x%02X", (unsigned)entry, (unsigned)At(p + 7)[0]);
}

}  // namespace

void Operands() {
    static std::uint8_t soft[0xD1], d3d[0xD5];
    for (U i = 0; i < sizeof soft; ++i) soft[i] = static_cast<std::uint8_t>(Entry(i, true));
    for (U i = 0; i < sizeof d3d; ++i) d3d[i] = static_cast<std::uint8_t>(Entry(i, false));
    bof3::Operand("Gfx_DrawOTag software byte table", kSoftBytes, soft, sizeof soft);
    bof3::Operand("Gfx_DrawOTag Direct3D byte table", kD3dBytes, d3d, sizeof d3d);
}
}  // namespace d3d_list

using namespace d3d_list;

// 0x59EE50, called once a rendered frame from WinMain (0x4FCE6F) with the
// ordering table. Nothing at all unless Gfx_DrawEnable. Gfx_RenderFlags bit 0
// (read once) picks the software surfaces' table; otherwise the walk is inside
// BeginScene / EndScene and uses the Direct3D table. The walk: from the table
// pointer, while the link word is not -1, follow it - a link with its top byte
// set is masked to 24 bits and the node it reaches is NOT drawn (known-defects
// D4), a clean one is drawn - and read the drawn node's link after its handler.
// Then every glyph texture and every cell texture is marked unused, and if
// anything set the word 0x7CADEA during the draw (it is cleared on entry),
// D3d_AfterDraw 0x59F580 runs (a tail jump in the original; it reads no
// argument).
void Gfx_DrawOTag(unsigned long* ot) {
    if (At(kDrawEnable)[0] == 0) return;
    if (draw_order::g_on) draw_order::LogWalk(ot);   // BOF3X_DRAWORDER, diagnostic only: reads the table, draws nothing
    if (Word(kAfterDrawFlag) != 0) {
        // Whether D3d_AfterDraw's request ever fires is unmeasured (display-setup.md section 7): say so once.
        static bool seen;
        if (!seen) {
            seen = true;
            bof3::Log("Gfx_DrawOTag: D3d_AfterDrawRequest was set on entry (first time)");
        }
        PutWord(kAfterDrawFlag, 0);
    }
    const bool software = (At(kRenderFlags)[0] & 1) != 0;
    if (!software) Scene(0x24);
    U node = static_cast<U>(reinterpret_cast<std::uintptr_t>(ot));
    while (Long(node) != 0xFFFFFFFFu) {
        U p = Long(node);
        if (p & 0xFF000000u) p &= 0xFFFFFFu;
        else Dispatch(p, software);
        node = p;
    }
    if (!software) Scene(0x28);
    for (U a = kFontInUse; a < kFontInUseEnd; a += 0x14) PutWord(a, 0);
    for (U a = kCellInUse; a < kCellInUseEnd; a += 0x28) PutWord(a, 0);
    if (Word(kAfterDrawFlag) != 0) g.after_draw();
}
