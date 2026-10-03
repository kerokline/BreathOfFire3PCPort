// DIVERGENCE DIV-0071: ground that is not above a sprite's feet does not cover
// them (docs/sprite-draw-order.md section 19). BOF3X_LAYERING picks the mode;
// unset or 0 is the original's order.
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
// before, so whatever hid the feet in the original still does.
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

}  // namespace layering
