// DIVERGENCE (ledger entry pending; docs/region-diff.md sections 8 and 10):
// Dauna Mine's minecart area (area 4) gets the walls every build after the
// Japanese one has. The US, French, German and both PSP discs block 72 cells
// that the JP disc - and so the PC port - leaves open: two edges of a raised
// strip and the row below a wall. The Western PSX discs also clear the battle
// placement map under 8 of them. This applies the same change by coordinate
// to whatever AREA004.DAT the player has, right after LoadDatFile has walked
// it, so the result is the same from any source. Armed after every module's
// self-test; BOF3X_AREA4_WALLS=0 leaves the shipped map.
#pragma once

namespace area4_walls {

// 0 until Arm has run (and with BOF3X_AREA4_WALLS=0): LoadDatFile does what
// the original does.
extern unsigned char g_on;

// Called by LoadDatFile after a file and its overlay have been walked: when
// `name` is AREA004.DAT and g_on is set, writes the wall byte into the 72
// cells and the no-placement nibble into the 8 placement cells - only if the
// block is the 90 x 88 map whose cells hold the values the JP data has there
// (each cell is checked first; a map already walled is left alone).
void Apply(const char* name);

// After every module's self-test: reads BOF3X_AREA4_WALLS and sets g_on.
void Arm();

}  // namespace area4_walls
