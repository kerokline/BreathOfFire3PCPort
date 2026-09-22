// The map view's scrolling and its reset (docs/map-scroll.md): the four shifts
// of the view's cell ring that AreaMap_Frame calls - MapView_ShiftColumnPrev
// 0x56E9A0, MapView_ShiftColumnNext 0x56EA30, MapView_ShiftRowsPrev 0x56EAB0,
// MapView_ShiftRowsNext 0x56EB50 - and MapView_PlaceRuns 0x571FF0 after them;
// Field_ViewReset 0x56F670, the view's set-up for an area, with its three
// callees AreaMap_BakePatches 0x56FAD0, AreaMap_SetupEntries 0x571720 and
// AreaMap_ClutCycleStart 0x5717B0.
#pragma once

void MapScroll_Inject();
