// The map and draw layers under the field's frame (docs/map-layers.md):
// AreaMap_Frame 0x56E6C0 and the view's rebuild under it - MapView_Build
// 0x56EC00 and MapView_CellTextures 0x56F9B0 - and the area header's pass,
// AreaMap_HeaderPass 0x571AF0, with the handlers of theirs the attract cycle
// reaches: MapCell_DrawWalls 0x571500 (a MapCell_Handlers entry) and
// AreaMap_ClutCycle 0x571B40 (an AreaMap_EntryHandlers entry).
#pragma once

void MapLayers_Inject();
