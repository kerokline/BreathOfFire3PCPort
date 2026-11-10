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
#include "hook/stp_census.h"
#include "render/render_shim.h"

namespace d3d_list {

namespace {
using Handler = long(__cdecl*)(const unsigned char*);
Handler H(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(::bof3::CodePlace(address))); }
}  // namespace


namespace {

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

