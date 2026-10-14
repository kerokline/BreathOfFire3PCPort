// Widescreen (DIVERGENCE DIV-0041, docs/widescreen.md, docs/ultrawide-frame.md):
// the picture widened by extra columns each side of the game's 320 x 240 view,
// the view centred in it - the PSP's approach, every primitive shifted right by
// the columns on the way to the screen, and the culls and edge-anchored
// elements that the wider view exposes re-authored. Two widths:
// BOF3X_WIDE=1 is 16:9, 426 x 240 (53 columns a side); BOF3X_WIDE=2 is 21:9,
// 560 x 240 (120 a side). Unset or 0, nothing here changes anything.
#pragma once

#include <cstdint>

// Extra columns each side of the 320-wide view: 53 under BOF3X_WIDE=1, 120
// under BOF3X_WIDE=2, 0 otherwise. Read once from the environment; any other
// value is Fatal.
unsigned Widescreen_Columns();

// The rules every width-dependent value is derived by, in the columns c: each
// gives the 16:9 build's constant at c = 53 (Widescreen_CheckRules proves it at
// start-up, and the static_asserts in widescreen.cpp at compile time), so 16:9
// is what it was and 21:9 follows from the same rule. No per-width table.
namespace widescreen_rule {
// The columns a side for a picture `num`:`den` at 240 rows: 240 num / den
// rounded down to an even width, less 320, halved - 53 for 16:9 (426), 120 for
// 21:9 (560, exact).
constexpr unsigned ColumnsForAspect(unsigned num, unsigned den) { return ((240u * num / den) / 2u * 2u - 320u) / 2u; }
// The terrain cull's margin past the original's [-50, 370]: the columns plus
// the 47 the owner's eye settled on at 53 (100 = 53 + 47, 2026-09-23). Tuned
// by eye at 53: the value at 120 (167) is owed the owner's eye.
constexpr unsigned TerrainMargin(unsigned c) { return c + 47u; }
// The cell inset lowered by a ring column (two map cells, about 20 screen px at
// the field camera's usual distance) per 20 columns, rounded up: 3 at 53.
constexpr unsigned InsetColumns(unsigned c) { return (c + 19u) / 20u; }
// Menu_DrawBackdrop's extra 64-px column pairs each side: enough to cover c.
constexpr unsigned BackdropPairs(unsigned c) { return (c + 63u) / 64u; }
// A slide-off bound moved outward by the columns: a negative one (off the left)
// down, a positive one (off the right) up.
constexpr int SlideBound(int bound, unsigned c) {
    return bound < 0 ? bound - static_cast<int>(c) : bound + static_cast<int>(c);
}
// The area-map frame pass's x ranges moved outward by c - 1 (the PSP's 31 for
// its 32): for c > 0 only.
constexpr float RangeLo(float lo, unsigned c) { return lo - static_cast<float>(c - 1u); }
constexpr float RangeHi(float hi, unsigned c) { return hi + static_cast<float>(c - 1u); }
}  // namespace widescreen_rule

// The start-up property check (docs/ultrawide-frame.md): every rule above at
// 53 columns against the constant the 16:9 build held, and at 120 the values
// the 21:9 build takes, logged. Fatal on a difference. Run by Widescreen_Inject
// whatever the width.
void Widescreen_CheckRules();

// The columns each side once Widescreen_Inject has run - 0 before it, so a
// start-up fuzz that runs earlier compares the original's layout - and 0
// for good when the view is not wide. What our own drawing code reads.
unsigned Widescreen_Live();

// The terrain cull of MapView_Build 0x56EC00 (ours, src/game/map_layers.cpp):
// the kept screen-x interval, -50..370 as the original has it, widened by
// Widescreen_Inject by widescreen_rule::TerrainMargin when the view is wide
// (-150..470 at 53 columns, -217..537 at 120).
extern float Widescreen_TerrainLo, Widescreen_TerrainHi;

// Columns the view's cell inset (map_layers.cpp Inset(), MapView_Inset
// 0x905D80, read by MapView_Build and DrawLayer_Open only - image scan
// 2026-10-07, two references) is lowered by each side when the view is wide:
// the inset trims the 28-column ring to what the 320-wide picture needs, and
// the 53 extra columns a side fell outside it as a stair-stepped notch (the
// owner's bridge, 2026-10-06). 0 before Widescreen_Inject and when the view
// is not wide; widescreen_rule::InsetColumns otherwise (3 at 53 columns, 6 at
// 120); BOF3X_WIDE_INSET=N moves it for tuning. The original's largest inset is
// 6 (map_layers.cpp Inset(): (0x160 - 0) / 50 - 1), so from 6 on the whole
// 28-column ring is walked and a wider picture needs a bigger ring (IDEAS I37).
unsigned Widescreen_InsetColumns();
// Menu_DrawBackdrop's extra 64-px column pairs each side: 0 until
// Widescreen_Inject has run and when the view is not wide, else
// widescreen_rule::BackdropPairs (1 at 53 columns, 2 at 120).
unsigned Widescreen_BackdropPairs();

// DIV-0041's operands as this build holds them (docs/operand-store.md): what
// our functions that held them read, never Capcom's code. Fatal for an address
// that is not one of the fourteen slide bounds / the four range operands.
//   Widescreen_SlideOperand: the bound at `at` as an int32 (an imm16 site's in
//     its low word), for a reader that takes its bound through an address (a
//     fuzz points that at a clone's immediate); Widescreen_SlideBound its low
//     word, as the originals' `cmp word`, `cmp cx` / `cmp ax` read it. Always
//     Capcom's bound since DIV-0041's 2026-10-13 amendment (the faster slide,
//     docs/ultrawide-slides.md): the window decides by it at every width, and
//     only the drawn x reaches the wide one (Widescreen_SlideHeld below).
//   Widescreen_RangeOperand: the address of the float the `fcomp` at `at`
//     names - .rdata's, or this file's wider copy once Widescreen_Inject has
//     patched (still .rdata's under BOF3X_ORIGINAL=Widescreen).
const std::int32_t* Widescreen_SlideOperand(unsigned at);
inline short Widescreen_SlideBound(unsigned at) { return static_cast<short>(*Widescreen_SlideOperand(at)); }
std::uint32_t Widescreen_RangeOperand(unsigned at);

// ---------------------------------------------------------------------------
// The faster slide (DIV-0041 amended 2026-10-13, docs/ultrawide-slides.md).
//
// The fourteen slide-off states step a window record's x (+4) by 0x20 a frame
// and hold it at Capcom's bound (+3 = 0) or free the window past it. They
// decide by Capcom's bound at every width, so the frame the box holds or is
// freed on, and every x the game reads, are Capcom's. The picture gets a
// second x: the box is drawn on a line from where its slide started to the
// wide bound (widescreen_rule::SlideBound), reaching it on the frame Capcom's
// x reaches Capcom's bound - a faster slide that clears the wide picture. The
// drawn x sits in +4 only while the record's own handler draws it:
// Field_RunTaskRecords (window_task.cpp) puts Capcom's x back after the
// handler returns. All of it is off until Widescreen_ArmFills (so every fuzz
// compares Capcom's code), when the view is narrow, under
// BOF3X_ORIGINAL=Widescreen, and when Field_RunTaskRecords is Capcom's.

// The bound a slide-off state decides by, given the one it read: that one.
// BOF3X_SL_CONTROL=1 is the fuzz control: the bound moved out by the columns,
// which is the late free the build before this amendment had; a fuzz at
// BOF3X_WIDE=1 or 2 must refuse it.
short Widescreen_SlideLogic(short bound);
// A holding slide (step, then hold at `bound` with +3 0), after its step:
// `x_read` the x it read before stepping, +4 the x it left. Records Capcom's
// x and writes the drawn one into +4 for the draw that follows.
void Widescreen_SlideHeld(unsigned char* rec, short x_read, short bound);
// A freeing slide (draw, then free at `bound` or step by `step`): around its
// draw. Draw writes the drawn x for the x +4 holds; Undraw puts that x back
// before the free test reads it.
void Widescreen_SlideDraw(unsigned char* rec, short bound, int step);
void Widescreen_SlideUndraw(unsigned char* rec);
// Field_RunTaskRecords' calls around each record's handler, and for a record
// it skips as free.
void Widescreen_SlideBefore(unsigned char* rec);
void Widescreen_SlideAfter(unsigned char* rec);
void Widescreen_SlideIdle(unsigned char* rec);

// Re-aims the four x-range constants of AreaMapBD_BuildView 0x510780 (ours,
// R3G's; it reads them back) at wider copies, turns the faster slide's switch
// on, and widens the terrain cull above. Placed after the modules whose fuzz
// compares the original ranges. Does nothing unless the view is wide.
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
// True once Widescreen_ArmFills has run - the self-tests are over and the
// game is the player's, wide or narrow. A change to a packet that the fuzz
// must still see as Capcom's, and that narrow play wants too, gates on this
// (DIV-0127, the ending's sky seam).
bool Widescreen_FillsArmed();

// The right edge of a full-frame fill the original drew to `edge` (320, or
// 319 for the fills drawn to the last column - the five magic flashes the
// `320.0` scan missed, psp-widescreen-placements.md 2.2): edge + the fill
// columns, as the PSP's 319 -> 351 for 32. Exactly `edge` while the fills
// are unarmed, so narrow play and every fuzz see the original's float.
float Widescreen_FillRight(float edge);

// The faster slide's property check (docs/ultrawide-slides.md section 4): at
// 0, 53 and 120 columns, every one of the fourteen states from every start x
// in a sweep, through the handler cycle Field_RunTaskRecords gives it (the
// nine holding states are ours, called; the five freeing ones Capcom's step as
// read): the frame it holds or frees on and the x outside its handler each
// frame equal the narrow run's, and the drawn x reaches the wide bound on that
// frame. Fatal on a difference. Run by Widescreen_ArmFills.
void Widescreen_CheckSlides();

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
