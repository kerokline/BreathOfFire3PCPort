// Two of the map-cell handlers DrawLayer_Open calls through MapCell_Handlers -
// MapCell_DrawQuads 0x570020 and MapCell_DrawRising 0x570660 - with the
// condition test Area_TestCondition 0x56FF00 and the ground's elevation
// AreaMap_Elevation 0x5720C0 (docs/sprite-draw-order.md section 16).
#pragma once

// Must run before the modules that own any function these call: see the .cpp.
void MapCells_Inject();

namespace map_cells {

// DIV-0041's random split (docs/ultrawide-rand.md): the random number MapCell_DrawRising takes
// for each of its eight squares - Capcom's Rand, or for a band cell
// (wide_rand::Band) the draw-only generator, so the wide view's extra cells
// never move the game's stream.
int RisingRand();

// The self-tests' handle on where RisingRand's Capcom calls go: the previous
// function comes back.
using RandFn = int (__cdecl*)(void);
RandFn SwapRand(RandFn rand);

}  // namespace map_cells
