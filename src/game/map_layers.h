// The map and draw layers under the field's frame (docs/map-layers.md):
// AreaMap_Frame 0x56E6C0 and the view's rebuild under it - MapView_Build
// 0x56EC00 and MapView_CellTextures 0x56F9B0 - and the area header's pass,
// AreaMap_HeaderPass 0x571AF0, with the handlers of theirs the attract cycle
// reaches: MapCell_DrawWalls 0x571500 (a MapCell_Handlers entry) and
// AreaMap_ClutCycle 0x571B40 (an AreaMap_EntryHandlers entry).
#pragma once

#include <cstdint>

void MapLayers_Inject();

namespace bof3 {
class SnapshotWriter;
class SnapshotReader;
}  // namespace bof3

namespace map_layers {

// The quicksave (SP-G, docs/quicksave.md section 9): DIV-0085's side faces as
// the area file loaded them, in the graphics participant's record.
void QuickSaveSides(bof3::SnapshotWriter& w);
void QuickRestoreSides(bof3::SnapshotReader& r);

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

// MapView_Build's inset (map_layers.cpp Inset()) from the globals it reads -
// Cond_AngleFB, Camera_Distance, Field_InputFlags, Cond_ByteFE - and the
// columns DIV-0041 lowers it by (Widescreen_InsetColumns): `original` is the
// value Capcom's code computes, `lowered` the one written to MapView_Inset.
// Pure: no global is read or written. DIV-0041's random split (docs/ultrawide-rand.md) reads
// both; equal when `wide_columns` is 0.
struct InsetPair {
    std::uint32_t original, lowered;
};
InsetPair ComputeInset(unsigned long angle, short distance, unsigned char input_flags, unsigned char byte_fe,
                       unsigned wide_columns);

}  // namespace map_layers
