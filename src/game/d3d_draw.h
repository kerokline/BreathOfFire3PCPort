// The Direct3D draw handlers of the draw 0x59EE50's second jump table that were
// still Capcom's - POLY_FT4, POLY_GT4, LINE_F2, LINE_F4, TILE and the port's
// own cell sprite (code 0x84) - their state helpers and the two texture
// lookups under them. docs/d3d-draw.md.
#pragma once

void D3dDraw_Inject();
