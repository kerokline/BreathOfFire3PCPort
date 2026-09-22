// Two of the map-cell handlers DrawLayer_Open calls through MapCell_Handlers -
// MapCell_DrawQuads 0x570020 and MapCell_DrawRising 0x570660 - with the
// condition test Area_TestCondition 0x56FF00 and the ground's elevation
// AreaMap_Elevation 0x5720C0 (docs/sprite-draw-order.md section 16).
#pragma once

// Must run before the modules that own any function these call: see the .cpp.
void MapCells_Inject();
