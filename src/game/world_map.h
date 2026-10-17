// Group 1 of the world-map wave: the map task's frame state machine, the map's
// frame (the dial, the keyboard legend, the compass needle), the sprite draw
// they share, the HUD (the region box and its label), the needle, and two
// map-view helpers (a draw item's half for this buffer; a primitive linked
// into a map-view row). docs/world-map.md section 7.
#pragma once

#include <cstdint>

void WorldMap_Inject();

// DIVERGENCE (button round ledger draft C; docs/world-map-hud.md 1.2, section
// 9): the legend's keys, one helper for the eleven frame draws -
// WorldMap_DrawFrame (area 33's, world_map.cpp) and Area16_, 45_, 65_, 87_,
// 88_, 104_, 115_, 121_, 151_ and 152_DrawFrame (the area_w*.cpp copies).
//
// Rows 1 and 2 draw the key of the Speak line (word 0) and the Action line
// (word 6): under the button round's sigma that is Sigma(circle) and
// Sigma(cross), the buttons the Config holds now (the self-test checks the two
// agree on every Config the rebind can make). On a page with the PlayStation's
// glyph sheet - a disc's own, or the port's with a PSX disc's shapes laid in by
// the importer (tools/importer.py, dial_with_disc_shapes) - the draw is the
// original's to the byte. On the port's own page (a PC install with no PSX
// disc: the sheet blank, the legend naming keys) the key names are painted
// over in VRAM with the panel, and the key is the port's own stand-in, the
// circled numeral or X of the Config screen in the button's colour, through
// the port's small font; row 3 is START in that font, five glyphs narrowed
// to fit where ENTER was. A withheld row's stand-in is dimmed by a disc's
// rule: its colours +56 a channel, through a dim CLUT. Nothing before WorldMapLegend_Arm, and nothing under
// BOF3X_ORIGINAL=WorldMapLegend.
namespace world_map_legend {

using SpriteFn = void (__cdecl*)(int x, int y, unsigned index);

// At the top of each frame draw, after its Draw_PassFlags test: which page is
// in VRAM this frame, and the port's key names painted over when they are there.
void Begin();
// Rows 1 and 2's key: the first of `count` (6 for word 0, 8 for word 6, as the
// original searches) entries of the copy's button table `buttons` whose mask
// has a bit of `word`; the copy's sprite draw `sprite` at (x, y) with the
// entry's sprite index + 1 (lit) or the index (`withheld`), as the original
// does - or, on the port's own page, the stand-in, lit, or `withheld` dimmed
// as a disc's withheld cell is: +56 per channel (a dim CLUT, world_map.cpp DimEntry).
void Key(SpriteFn sprite, std::uint32_t buttons, std::uint32_t count, std::uint32_t word, int x, int y, bool withheld);
// Row 3, after its label would be drawn at (x, y), whether it was or not
// (`withheld`: the label is not shown, the frame's dim row is): nothing on a
// page with the PlayStation's art (the label, or the frame's dim copy,
// carries START); START on the port's own page, dimmed when withheld.
void Start(int x, int y, bool withheld);
// After every module's self-test (inject_all.cpp): the self-test
// (BOF3X_SHADOW=world_map_legend) and the switch.
void Arm();

}  // namespace world_map_legend
