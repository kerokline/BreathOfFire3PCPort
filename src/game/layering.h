// DIVERGENCE DIV-0071: ground that is not above a sprite's feet does not cover
// them (docs/sprite-draw-order.md section 19). BOF3X_LAYERING picks the mode;
// unset is mode 1 (the default since 2026-10-03), 0 is the original's order.
//
// The original's draw pass is a painter's order by diagonal row: every cell of
// a nearer row is drawn after a sprite, flat floor included, so the floor of
// the next row cuts the shadow and sometimes a foot wherever they reach past
// the sprite's own row on screen (known-defects.md D199; the PlayStation's
// order is the same).
//
// Mode 1 draws a sprite up to three layers later, for as long as everything it
// would then be drawn over, where its feet are, is floor: a cell's own quad
// whose lowest corner is not above the feet. A wall, a side triangle, a cell
// record, a raised cell or another sprite in the way stops it at the layer
// before, so whatever hid the feet in the original still does. A cell record
// is in the way when its map cell is within two of the sprite's, or when what
// its run drew in the last pass reaches the sprite's body (amended
// 2026-10-14: the camp's fire, a record three cells from Garr, drawn under
// him; docs/campfire-and-crystal.md).
//
// It arms after InjectAll's self-tests, as DIV-0069's and DIV-0041's fills do,
// so every fuzz compared the original's order.
#pragma once

namespace layering {

enum Mode : int {
    kOff = 0,
    kFloor = 1,       // a sprite is drawn after the nearer rows' floor under its feet
    kLayerLater = 2,  // every sprite's key one layer later, whatever is there (the comparison build; not the fix)
};
extern int g_mode;   // kOff until Arm()

void Arm();   // BOF3X_LAYERING parsed; after every module's self-test

// Sprite_DrawPass's, mode kFloor. Defer: after the draw list is sorted, each
// sprite's key moved to the layer it is drawn in; true when any moved, and the
// list is then to be sorted again. Restore: after the pass, the keys as
// Sprite_UpdateScreen left them.
bool Defer();
void Restore();

// What each map cell's record run drew (docs/campfire-and-crystal.md section
// 4). DrawLayer_Open's, mode kFloor: RunBegin before a run's first record is
// handed to its handler, RunEnd after its last. Gfx_CommitPrim's, while
// g_in_run: RunPrim for each primitive committed, whose points are read at
// RunEnd (a handler may write them after the commit). The union of the run's
// primitives' screen boxes is kept for the next pass's Defer, which stops a
// sprite at a layer whose runs drew anything over its body last pass.
extern bool g_in_run;
void RunBegin(const unsigned char* run);
void RunPrim(const unsigned char* prim, unsigned size);
void RunEnd();

}  // namespace layering
