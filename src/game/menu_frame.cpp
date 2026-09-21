#include "game/menu_frame.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// DIV-0011: draw the panel frame the PC build left out.
//
// The Config screen's panel (0x461710) and its controller sub-panel
// (0x461A50) each open with a call of (x, y, w, h) - (.., 0x21, 0x0D) and
// (.., 0x0C, 0x0F) - to 0x4DF820, which is a bare `ret` shared by 25 call
// sites of 0, 1 and 4 arguments: several empty functions folded into one. On
// the PlayStation the same call, from the same function (STATUS.EMI
// 0x801E2548, the same six (row, setting) pairs built on the stack), goes to
// 0x801DF56C, read 2026-09-20 from the owner's Japanese disc:
//
//   page GetTPage(0, 0, 0x340, 0x100), clut GetClut(0xB0, 0x1E1), colour 0x80,
//   w and h in 8 pixel cells. Five sprites, each under a DR_MODE whose
//   TEXTURE WINDOW is one 8 x 8 tile, so the tile repeats across the sprite:
//     top     (x + 16, y)               (w - 4) * 8 by 8     tile (0x70, 0x98)
//     bottom  (x + 16, y + (h - 1) * 8) (w - 4) * 8 by 8     tile (0x98, 0xA0)
//     left    (x, y + 16)               8 by (h - 4) * 8     tile (0xB0, 0x98)
//     right   (x + (w - 1) * 8, y + 16) 8 by (h - 4) * 8     tile (0xB0, 0xA0)
//     fill    (x + 8, y + 8)            (w - 2) * 8 by (h - 2) * 8, tile (0x70, 0xA0)
//   then a DR_MODE with the window back to the whole page, and four corner
//   pieces through 0x801B02A0(x, y, id, 1): 0x23 at (x, y), 0x26 at
//   (x + (w - 2) * 8, y), 0x27 at (x, y + (h - 2) * 8), 0x29 at the far corner.
//
// The port's renderer has no texture window (Gpu_SetDrawMode's is passed 0
// everywhere read so far), which is the likely reason the body is empty -
// unread, and it does not matter to what follows.
//
// Ours draws the same thing a tile at a time through Menu_DrawPiece 0x57D860,
// the PC's 0x801B02A0. Its flagged rectangle table 0x663C8C still holds all
// nine pieces - the corners 0x23 / 0x26 / 0x27 / 0x29 as 16 x 16, and the five
// tiles as 8 x 8 at exactly the PlayStation's window origins: 0x24 (0x70, 0x98),
// 0x28 (0x98, 0xA0), 0x2A (0xB0, 0x98), 0x2B (0xB0, 0xA0), 0x25 (0x70, 0xA0).
// Same order as the PlayStation's, so what overlaps what is the same.
//
// Cost: (w - 2) * (h - 2) + 2 * (w - 4) + 2 * (h - 4) + 4 sprites of 0x1C
// bytes - 421 for Config's panel, 172 for the controller's, 370 for the
// reserve list - out of a 64 KB
// packet pool that Gfx_CommitPrim guards by dropping what does not fit.
//
// cdecl, four stack arguments, as the call sites push them. x and y arrive as
// whole registers holding 16-bit values; w and h are immediates.

namespace {

constexpr std::uint32_t kEmptyFunction = 0x4DF820;
constexpr std::uint32_t kConfigPanelSite = 0x461778;       // in 0x461710: push 0xD, push 0x21, y, x
constexpr std::uint32_t kControllerPanelSite = 0x461A84;   // in 0x461A50: push 0xF, push 0xC, y, x
// The reserve list of "change party members", 0x12 by 0x15 cells. The
// PlayStation's is 0x801EA99C(obj): the frame at obj's s16 +4 / +6, then an
// entry a member through 0x801AF3F0(x + 7, y + 6, 0x7D, 0x30, ..). The PC has
// the list twice - 0x59AA80(obj), the same shape, and 0x581300(x, y) - both
// opening with the empty call, both followed by the entries (the PC's
// 0x801AF3F0 is 0x57CF60, its entry body 0x573560). In 0x59AA80 x and y are
// pushed from 16-bit loads, so the top halves of the arguments are not ours
// to read.
constexpr std::uint32_t kReserveListSite = 0x581313;       // in 0x581300: push 0x15, push 0x12, y, x
constexpr std::uint32_t kReserveListObjSite = 0x59AA98;    // in 0x59AA80: push 0x15, push 0x12, [obj+6], [obj+4]

constexpr unsigned kTop = 0x24, kFill = 0x25, kBottom = 0x28, kLeft = 0x2A, kRight = 0x2B;
constexpr unsigned kCornerTL = 0x23, kCornerTR = 0x26, kCornerBL = 0x27, kCornerBR = 0x29;

void Tiles(int x, int y, int columns, int rows, unsigned id) {
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < columns; ++c) Menu_DrawPiece(x + c * 8, y + r * 8, id, 1);
}

void __cdecl Menu_DrawFrame(int x_in, int y_in, int w_in, int h_in) {
    const int x = static_cast<std::int16_t>(x_in), y = static_cast<std::int16_t>(y_in);
    const int w = static_cast<std::uint8_t>(w_in), h = static_cast<std::uint8_t>(h_in);

    Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, Gpu_GetTPage(0, 0, 0x340, 0x100) & 0xFFFF, 0);
    Gfx_CommitPrim(1, 0xC);

    Tiles(x + 16, y, w - 4, 1, kTop);
    Tiles(x + 16, y + (h - 1) * 8, w - 4, 1, kBottom);
    Tiles(x, y + 16, 1, h - 4, kLeft);
    Tiles(x + (w - 1) * 8, y + 16, 1, h - 4, kRight);
    Tiles(x + 8, y + 8, w - 2, h - 2, kFill);

    Menu_DrawPiece(x, y, kCornerTL, 1);
    Menu_DrawPiece(x + (w - 2) * 8, y, kCornerTR, 1);
    Menu_DrawPiece(x, y + (h - 2) * 8, kCornerBL, 1);
    Menu_DrawPiece(x + (w - 2) * 8, y + (h - 2) * 8, kCornerBR, 1);
}

}  // namespace

void MenuFrame_Inject() {
    bof3::RetargetCall("Menu_DrawFrame", kConfigPanelSite, kEmptyFunction, reinterpret_cast<void*>(&Menu_DrawFrame));
    bof3::RetargetCall("Menu_DrawFrame", kControllerPanelSite, kEmptyFunction, reinterpret_cast<void*>(&Menu_DrawFrame));
    bof3::RetargetCall("Menu_DrawFrame", kReserveListSite, kEmptyFunction, reinterpret_cast<void*>(&Menu_DrawFrame));
    bof3::RetargetCall("Menu_DrawFrame", kReserveListObjSite, kEmptyFunction, reinterpret_cast<void*>(&Menu_DrawFrame));
}
