// The three Direct3D sprite handlers - D3d_DrawSprt 0x5A2300, D3d_DrawSprt8
// 0x5A2520, D3d_DrawSprt16 0x5A2710 - with DIVERGENCE DIV-0010 (the far
// texture edge) inside them. docs/sprt-draw.md.
#pragma once

void SprtDraw_Inject();
