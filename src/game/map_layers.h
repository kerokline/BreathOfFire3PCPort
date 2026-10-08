// The map and draw layers under the field's frame (docs/map-layers.md):
// AreaMap_Frame 0x56E6C0 and the view's rebuild under it - MapView_Build
// 0x56EC00 and MapView_CellTextures 0x56F9B0 - and the area header's pass,
// AreaMap_HeaderPass 0x571AF0, with the handlers of theirs the attract cycle
// reaches: MapCell_DrawWalls 0x571500 (a MapCell_Handlers entry) and
// AreaMap_ClutCycle 0x571B40 (an AreaMap_EntryHandlers entry).
#pragma once

void MapLayers_Inject();

namespace map_layers {

// DIV-0085 (docs/known-defects.md D239): a cell whose file heights give it one
// side face, and which is created with both because something moved the
// heights since (the sea bridge's sky effect), draws both with the one side
// word its tile carries - not the next tile's word for the second.
//
// Called by LoadDatFile after a file that carried the area block (kind 0, tag
// 0xC8000): which sides each cell's heights give it, as loaded - MapView_Build's
// two tests on the file's corners.
void SnapshotSides();

// After every module's self-test: reads BOF3X_SIDE_DUP (on unless 0).
void ArmSideDup();

}  // namespace map_layers
