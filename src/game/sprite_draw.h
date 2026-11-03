// A sprite object as one code-0x84 primitive and a run of 8-byte cell records:
// Sprite_Draw 0x5935B0, Sprite_ClutWord 0x593860, SpriteCell_Add 0x5A6790 and
// SpriteCell_Reset 0x5A6780 (docs/sprite-draw-order.md section 14).
#pragma once

// Must run before PsxGpu_Inject and DrawEmit_Inject: see the .cpp.
void SpriteDraw_Inject();

// DIV-0142's amendment (issue #191): the VRAM shadow's (x, y, w, h) zeroed,
// clipped to VRAM, and the textures over it dropped - a capture slot made as
// empty as the PlayStation's page is before something reads it.
void SpriteCapture_ClearSlot(int x, int y, int w, int h);
