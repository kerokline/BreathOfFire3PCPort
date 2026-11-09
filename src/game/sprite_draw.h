// A sprite object as one code-0x84 primitive and a run of 8-byte cell records:
// Sprite_Draw 0x5935B0, Sprite_ClutWord 0x593860, SpriteCell_Add 0x5A6790 and
// SpriteCell_Reset 0x5A6780 (docs/sprite-draw-order.md section 14).
#pragma once

#include <cstdint>

// Must run before PsxGpu_Inject and DrawEmit_Inject: see the .cpp.
void SpriteDraw_Inject();

// DIV-0142's amendment (issue #191): the VRAM shadow's (x, y, w, h) zeroed,
// clipped to VRAM, and the textures over it dropped - a capture slot made as
// empty as the PlayStation's page is before something reads it.
void SpriteCapture_ClearSlot(int x, int y, int w, int h);

namespace sprite_draw {
void Capture(unsigned char* object);   // defined in sprite_draw.cpp (source-layout B0: promoted, one definition)
extern void(__cdecl* g_draw_clone)();   // defined in sprite_draw.cpp (source-layout B0: promoted for a moved reader)
extern unsigned g_capture_logged;   // defined in sprite_draw.cpp (source-layout B0: promoted for a moved reader)
extern int g_draw_columns;   // defined in sprite_draw.cpp (source-layout B0: promoted for a moved reader)
extern int g_flag_columns;   // defined in sprite_draw.cpp (source-layout B0: promoted for a moved reader)
extern std::uint16_t g_canvas[0x140][0x140];   // defined in sprite_draw.cpp (source-layout B0: promoted for a moved reader)
extern unsigned long g_tails[8];   // defined in sprite_draw.cpp (source-layout B0: promoted for a moved reader)
struct State;   // the self-test's snapshot, defined in sprite_draw.cpp (its size needs the file's constants)
extern State g_live;   // defined in sprite_draw.cpp (source-layout B0: promoted for a moved reader)
}  // namespace sprite_draw
