// The Direct3D draw handlers of the draw 0x59EE50's second jump table that were
// still Capcom's - POLY_FT4, POLY_GT4, LINE_F2, LINE_F4, TILE and the port's
// own cell sprite (code 0x84) - their state helpers and the two texture
// lookups under them. docs/d3d-draw.md.
#pragma once

#include <cstdint>

void D3dDraw_Inject();
// DIV-0181: frees the cell textures whose cells an upload of VRAM rect (x, y, w, h) covers.
void D3dDraw_CellsShadowWritten(int x, int y, int w, int h);

namespace d3d_draw {
bool PoseLogOn();   // defined in d3d_draw.cpp (source-layout B0: promoted, one definition)
extern std::uint8_t g_quad_far;   // defined in d3d_draw.cpp (source-layout B0: promoted for a moved reader)
}  // namespace d3d_draw
