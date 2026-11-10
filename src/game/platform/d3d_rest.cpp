// The renderer's live remainder, read to the last instruction with capstone
// against bof3/BOF3.exe (2026-10-05, docs/d3d-rest.md). All are the PC port's
// own: the PlayStation drew these primitives on its GPU, so there is no PSX twin
// to read them against. Faithful: no divergence.
//
//   Gfx_StoreImage         0x59E930..0x59E99B (0x6C)
//   D3d_SetAlphaModulate   0x59F520..0x59F572 (0x53)
//   D3d_AfterDraw          0x59F580..0x59F83E (0x2BF)
//   D3d_DrawPolyF3         0x59FA50..0x59FB9B (0x14C)  code 0x20
//   D3d_DrawPolyFT3        0x59FDB0..0x59FFD5 (0x226)  code 0x24
//   D3d_FlattenFT3         0x5A0910..0x5A0A30 (0x121)  under POLY_FT3
//   D3d_PageTexel4         0x5A0A40..0x5A0AA9 (0x6A)   under D3d_FlattenFT3
//   D3d_DrawPolyGT3        0x5A1050..0x5A1288 (0x239)  code 0x34
//   D3d_DrawLineG4         0x5A1EA0..0x5A20CE (0x22F)  code 0x5C
//   D3d_DrawTile1          0x5A2220..0x5A22F4 (0xD5)   code 0x68
//
// The arithmetic is x87 in the original and x87 here, instruction for
// instruction (the X87* helpers below, d3d_draw.cpp's idiom): each fmul and fdiv
// rounds to the control word's precision, a float that passes through fld /
// fstp comes out with a signalling NaN quieted, and the CRT's _ftol is a
// truncating fistp to 64 bits under a copy of the control word (area_w3g.cpp's
// Ftol). The fuzz checks three control words.
#include "game/d3d_rest.h"

#include "game/d3d_lines.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/d3d_rest_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace d3d_rest {


namespace {

// --- the x87 sequences, as the original executes them ------------------------

#undef BOF3X_FTOL

// --- the COM objects ---------------------------------------------------------

}  // namespace
}  // namespace d3d_rest

using namespace d3d_rest;

// --- the handlers ------------------------------------------------------------
// Each is reached from one site of Gfx_DrawOTag's Direct3D table (d3d-draw.md
// section 2) and returns what DrawPrimitive returned (the caller does not read
// it). Not kept, in all of them: the original lets the colour helper write the
// diffuse into the caller's pushed `prim` slot, which the caller pops unread.

// --- the walk's two other callees ----------------------------------------------

// --- the VRAM shadow's read-back -------------------------------------------------

void D3dRest_Inject() {
    if (bof3::WantsShadow("d3d_rest")) d3d_rest::SelfTest();
}
