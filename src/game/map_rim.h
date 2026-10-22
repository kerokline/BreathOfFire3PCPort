// The field's view ring grown for the wide picture (DIVERGENCE DIV-0041, the
// wide-ring round; docs/wide-ring-read.md section 6, docs/wide-ring.md).
//
// Capcom's ring - MapView_CellItems 0x937FA0 (56 rows x 28 cells), MapView_Cells
// 0x904F20, the heads MapView_Row / _Column and MapView_Origin - is kept as it
// is at every width: every logic reader of its window (MoveCmd_TestFB / FC,
// AreaMap_ClearCell, Area40_TileLit, the Quake walk) sees Capcom's 56 x 28.
// When the view is wide, a rim of ours holds the 346 cells the PSP's 58 x 33
// ring has beyond Capcom's - one row above, one below, two and a half columns
// either side - read only by the draw.
//
// The grown view's geometry is the PSP's (its Field_ViewReset 0xE560): a grown
// position (gr, gc), gr 0..57 and gc 0..32, shows map cell
//     x = Ox - 3 + gc + (gr + 1) / 2,  z = Oz + 2 + gr / 2 - gc
// with (Ox, Oz) Capcom's MapView_Origin. Capcom's view cell (r, c) is grown
// (r + 1, c + 2 + (r odd)); every other grown position is a rim cell.
//
// Narrow (Widescreen_InsetColumns() 0), with BOF3X_WIDE_RING=0, and in every
// module's start-up self-test, Active() is false and nothing here runs.
#pragma once

#include <cstdint>

namespace bof3 {
class SnapshotWriter;
class SnapshotReader;
}  // namespace bof3

namespace map_rim {

constexpr int kRows = 0x3A;      // 58, the PSP's ring rows
constexpr int kColumns = 0x21;   // 33, the PSP's ring columns
constexpr int kRimCells = kRows * kColumns - 0x38 * 0x1C;   // 346

// The rim is kept and drawn: armed (after every module's self-test) and the
// view wide, or forced by a self-test.
bool Active();

// Capcom's view row of grown row gr is gr - 1; Capcom's view column of grown
// (gr, gc) is gc - 2 - (that row odd). True when that lies in 56 x 28.
bool InWindow(int gr, int gc);
// The 4-byte cell (map x, map y, the draw-item word) at a grown position:
// Capcom's MapView_CellItems slot inside the window, the rim's outside.
unsigned char* Cell(int gr, int gc);
// The run word (MapView_Cells') at a grown position, likewise.
unsigned short RunWord(int gr, int gc);
// Capcom's ring column of a window position (for wide_rand::Row).
int RingColumn(int gr, int gc);
// Capcom's ring row of grown row gr, 1..56 (Capcom rows 0..55).
int RingRow(int gr);

// MapView_CellToMap's arithmetic on the grown frame: the map cell grown
// (gr, gc) shows, or 0, 0 - never Capcom's function at a negative row, whose
// truncating halves put row -1 on row 0's cells.
void GrownCellToMap(int gr, int gc, unsigned char* out);

// The rim kept beside Capcom's writers - each called after Capcom's body, so
// the origin and heads it reads are the new ones.
enum class Shift { kColumnPrev, kColumnNext, kRowsPrev, kRowsNext };
void RimShift(Shift shift);   // the four MapView_Shift* (map_scroll.cpp)
void RimFill(bool keep_items);   // Field_ViewReset / Weretiger_ResetMapView (false), MapView_FillCells (true)
void RimRuns();               // MapView_PlaceRuns' rim half

// The draw walk's grown inset g = max(0, i + 2 - L) (owner's answer 3,
// 2026-10-21): i the original inset, L Widescreen_InsetColumns(). Noted by
// MapView_Build beside the inset byte it wrote; DrawLayer_Open asks with the
// byte it read and gets g, or, when the byte is not the one noted, the byte
// plus 2 (the widest walk the rule can give for it).
void NoteWalk(unsigned grown_inset, unsigned written_inset);
int Walk(unsigned read_inset);

// Layer -1 (grown row 0): a DrawLayers-shaped record of ours, 12 dwords - three
// (first, last) pairs per display buffer, interleaved as Capcom's - and its
// draw-table cells, which DrawTable's layer byte cannot key.
unsigned long* ExtraLayer();
void ExtraReset(unsigned buffer_index);   // DrawLayers_Reset's for the record
void ExtraTablePush(unsigned key, unsigned item);
void ExtraTableSort();
unsigned ExtraTableCount();
unsigned ExtraTableItem(unsigned i);

// MapView_ItemAt / MapCell_DrawFlatFaces outside Capcom's window: the rim's
// draw item for map offset (dx, dz) from MapView_Origin, or 0 outside the
// grown ring (and inside Capcom's window, which the callers read themselves).
unsigned RimItemAt(std::int32_t dx, std::int32_t dz);

// The draw-item pool's and the draw table's peaks over wide builds, logged.
void NoteBuildPeaks(unsigned table_entries);

// The quicksave (the graphics participant, docs/quicksave.md): the rim and
// the extra layer; a load across widths rebuilds (owner's answer 4).
void QuickSave(bof3::SnapshotWriter& w);
void QuickRestore(bof3::SnapshotReader& r);

// Self-test controls for wide_rand's grown walk cases: Active() forced, and a
// rim run word set (or every one cleared).
void ForceForTest(bool on);
void TestSetRun(int gr, int gc, unsigned short word);
void TestClearRuns();

// After every module's self-test (inject_all.cpp): BOF3X_SHADOW=map_rim's
// model test, then BOF3X_WIDE_RING and the width decide Active().
void Arm();

}  // namespace map_rim
