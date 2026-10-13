// Widescreen (DIVERGENCE DIV-0041, docs/widescreen.md): the picture 426 x 240
// with the game's 320 x 240 view centred in it - the PSP's approach, every
// primitive shifted right by 53 columns on the way to the screen, and the
// culls and edge-anchored elements that the wider view exposes re-authored.
// BOF3X_WIDE=1 turns it on; off, nothing here changes anything.
#pragma once

#include <cstdint>

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

// Columns the view's cell inset (map_layers.cpp Inset(), MapView_Inset
// 0x905D80, read by MapView_Build and DrawLayer_Open only - image scan
// 2026-10-07, two references) is lowered by each side when the view is wide:
// the inset trims the 28-column ring to what the 320-wide picture needs, and
// the 53 extra columns a side fell outside it as a stair-stepped notch (the
// owner's bridge, 2026-10-06). 0 before Widescreen_Inject and when the view
// is not wide; BOF3X_WIDE_INSET=N moves it for tuning.
unsigned Widescreen_InsetColumns();

// DIV-0041's operands as this build holds them (docs/operand-store.md): what
// our functions that held them read, never Capcom's code. Both are the
// original's until Widescreen_Inject patches, then the widened ones (still the
// original's under BOF3X_ORIGINAL=Widescreen). Fatal for an address that is
// not one of the fourteen slide bounds / the four range operands.
//   Widescreen_SlideOperand: the bound at `at` as an int32 (an imm16 site's in
//     its low word), for a reader that takes its bound through an address (a
//     fuzz points that at a clone's immediate); Widescreen_SlideBound its low
//     word, as the originals' `cmp word`, `cmp cx` / `cmp ax` read it.
//   Widescreen_RangeOperand: the address of the float the `fcomp` at `at`
//     names - .rdata's, or this file's wider copy.
const std::int32_t* Widescreen_SlideOperand(unsigned at);
inline short Widescreen_SlideBound(unsigned at) { return static_cast<short>(*Widescreen_SlideOperand(at)); }
std::uint32_t Widescreen_RangeOperand(unsigned at);

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

// The right edge of a full-frame fill the original drew to `edge` (320, or
// 319 for the fills drawn to the last column - the five magic flashes the
// `320.0` scan missed, psp-widescreen-placements.md 2.2): edge + the fill
// columns, as the PSP's 319 -> 351 for 32. Exactly `edge` while the fills
// are unarmed, so narrow play and every fuzz see the original's float.
float Widescreen_FillRight(float edge);

// The columns an edge-anchored element moves outward by in the wide picture
// (the PSP's +-32 for its 32 columns; psp-widescreen-placements.md 2.6, the
// owner's answers of 2026-10-12 in 6.1): the fill columns, so it is 0 while
// any self-test runs and 0 for good when the view is narrow. A right-anchored
// x adds it, a left-anchored one subtracts it; a centred one never reads it.
int Widescreen_Anchor();

// The battle window's full-frame tile: entry 11 of the size table at
// 0x64E268 ((321, 241) on the PC), which BattleWin_DrawTile, _DrawTileRgb and
// _DrawTileTint read and their callers pass with x 0 (the bosses' death
// flashes, rest_3a's flashes). The PSP widened the entry to 385 and moved the
// callers' x to -32 (psp-widescreen-placements.md 2.8); the same rule at the
// table's readers, derived, not copied: for size 11 the width grows by twice
// the fill columns and an x of 0 becomes -columns. Unarmed or narrow, nothing
// changes. `x` is the s16 the reader converts, `w` the table's u16.
constexpr unsigned kWidescreenFullFrameTile = 11;
void Widescreen_BattleTile(unsigned size, int& x, int& w);
