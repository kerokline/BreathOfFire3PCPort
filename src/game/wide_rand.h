// DIV-0041's random split (docs/ultrawide-rand.md, known-defects.md D283): the wide view's map
// walk split between two random streams. DIV-0041 lowers the cell inset by
// Widescreen_InsetColumns, so DrawLayer_Open 0x56FD20 hands MapCell_Handlers
// cells in columns the original never walks. Of the sixteen handlers only
// MapCell_DrawRising 0x570660 draws random numbers (eight Rand calls a cell);
// a cell the original walks keeps Capcom's Rand, a cell only the lowered
// inset adds - a "band" cell - takes them from a generator of ours that the
// rest of the game never reads. Narrow, and with BOF3X_WIDE_INSET=0, no cell
// is a band cell and nothing here changes anything.
#pragma once

#include <cstdint>

namespace bof3 {
class SnapshotWriter;
class SnapshotReader;
}  // namespace bof3

namespace wide_rand {

// map_layers.cpp Inset(), once per MapView_Build: the inset Capcom's code
// computes, and the byte it writes to MapView_Inset 0x905D80 (lowered when the
// view is wide). Inset() is MapView_Inset's only writer and DrawLayer_Open its
// only reader (image scan, docs/ultrawide-rand.md section 1).
void NoteInset(unsigned original, unsigned written);

// One row of DrawLayer_Open's walk: from the inset byte it read, the input
// flag's bit 0, the row and MapView_Column, the columns the original's walk of
// that row visits - the same arithmetic as DrawLayer_Open's, on the noted
// original inset. When the byte read is not the one Inset() last wrote (a
// self-test's, or no build yet), or the two insets are equal, the row is not
// split: every cell is the original's.
class Row {
public:
    Row(unsigned read_inset, unsigned input_flags, int row, int view_column);
    // A cell at `column` of this row is a band cell: the wide walk reaches it,
    // the original's walk of the row does not.
    bool Band(int column) const;
    bool Split() const { return split_; }

private:
    int columns_[28];
    int count_ = 0;
    bool split_ = false;
};

// Set by DrawLayer_Open around each cell's handler calls, cleared after the
// row: true while a band cell's records are being handed over.
void SetBand(bool band);
// What MapCell_DrawRising asks before each of its Rand calls: a band cell, and
// the split not suspended by the self-test's control.
bool Band();

// The draw-only generator: MSVC rand()'s recurrence (Rand 0x5B93D2's) on a
// seed of its own, 1 at start-up, advanced by band cells alone.
int __cdecl DrawOnly();
std::uint32_t* DrawOnlySeed();

// The quicksave (SP-G, docs/quicksave.md section 9): the generator's seed and
// the last inset noted, in the graphics participant's record.
void QuickSave(bof3::SnapshotWriter& w);
void QuickRestore(bof3::SnapshotReader& r);

// The start-up self-test (BOF3X_SHADOW=wide_rand) and the log line. After
// Widescreen_Inject, which sets the live inset columns.
void Inject();

}  // namespace wide_rand
