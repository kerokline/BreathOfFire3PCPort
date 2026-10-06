// The renderer's live remainder (the platform round's group PH): the five
// Direct3D primitive handlers of Gfx_DrawOTag's second table that were still
// Capcom's - POLY_F3, POLY_FT3 with its two helpers, POLY_GT3, LINE_G4, TILE_1 -
// D3d_SetAlphaModulate and D3d_AfterDraw, which the walk calls, and
// Gfx_StoreImage, the VRAM shadow's read-back. docs/d3d-rest.md.
#pragma once

void D3dRest_Inject();
