// Widescreen (DIVERGENCE DIV-0041, docs/widescreen.md): the picture 426 x 240
// with the game's 320 x 240 view centred in it - the PSP's approach, every
// primitive shifted right by 53 columns on the way to the screen, and the
// culls and edge-anchored elements that the wider view exposes re-authored.
// BOF3X_WIDE=1 turns it on; off, nothing here changes anything.
#pragma once

// Extra columns each side of the 320-wide view: 53 under BOF3X_WIDE=1, 0
// otherwise. Read once from the environment; any other value is Fatal.
unsigned Widescreen_Columns();

// The columns each side once Widescreen_Inject has run - 0 before it, so a
// start-up fuzz that runs earlier compares the original's layout - and 0
// for good when the view is not wide. What our own drawing code reads.
unsigned Widescreen_Live();

// The terrain cull of MapView_Build 0x56EC00 (ours, src/game/map_layers.cpp):
// the kept screen-x interval, -50..370 as the original has it, widened by
// Widescreen_Inject to -150..470 when the view is wide.
extern float Widescreen_TerrainLo, Widescreen_TerrainHi;

// Re-aims the four x-range constants of AreaMapBD_BuildView 0x510780 (ours,
// R3G's; it reads them back) at wider copies, moves the menu boxes' fourteen slide-off bounds
// outward by the columns added, and widens the terrain cull above. Placed
// after the modules whose fuzz compares the original bounds; modules added
// below it fuzz against the widened ones, so ours of a patched site reads the
// operand (menu_lists, menu_draw_helpers, battle_e7). Does nothing unless the
// view is wide.
void Widescreen_Inject();

// The columns each side for a full-frame fill - a fade, a tint, a flash, a
// shade, the loss screen's black: 0 until Widescreen_ArmFills has run, which
// InjectAll does after every module's self-test, so a fuzz of any module,
// whichever side of Widescreen_Inject it sits, compares the original's
// (0, 0) 320 x 240; then Widescreen_Live's columns. A fill draws at
// (Widescreen_FillX(), 0) Widescreen_FillWidth() x 240 (DIV-0041 section 3c).
unsigned Widescreen_Fill();
float Widescreen_FillX();       // 0.0f - columns: not -columns, which is -0.0f when narrow
float Widescreen_FillWidth();   // 320.0f + 2 * columns
void Widescreen_ArmFills();
