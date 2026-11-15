// tools/move_funcs.py (docs/source-layout-round.md). The functions of commu_name.cpp are declared
// by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

#include "game/menu/name_grid.h"

void CommuName_Inject();

// The renamer's grid (docs/new-game-round.md section 4, stage B, group CR):
// with the switch CommuNameGrid on and a grid loaded, the entry's three cut
// calls (two BareRet, one BareRetZero) are the name grid's draws and input
// step (game/menu/name_grid.h, Copy::kRenamer) over the PC's own cells - the
// buffer 0x675F98, the name cursor 0x675F94, the column 0x675FCC, the row
// 0x675FCA, the page 0x675FCB (docs/name-entry-restoration.md section 3).
// Off, or with no grid, or on a faerie slot under the Japan family (the
// owner's call 6), the entry ends as DIV-0075 has it.
namespace commu_name {

extern unsigned char g_grid;   // CommuNameGrid's byte: set by CommuName_Inject after the self-tests

// The routine's calls the renamer makes. g_api is the live unit's
// (NameGrid_*); the self-test (rest_4d_fuzz.cpp) points it at a synthetic grid.
struct GridApi {
    bool (*available)();
    name_grid::Family (*family)();
    unsigned (*cell_count)();
    unsigned (*first_page)();
    void (*draw)(name_grid::Copy copy, const name_grid::Cells& cells, int x, int y);
    void (*legend)(name_grid::Copy copy, unsigned page, int x, int y);
    unsigned (*input)(name_grid::Copy copy, name_grid::Cells& cells, unsigned room, const unsigned char* fallback);
    unsigned (*encode)(const unsigned char* cells, unsigned char* out, unsigned room);
    bool (*decode)(const unsigned char* stored, unsigned room, unsigned char* out);
};
extern const GridApi kLiveGridApi;
extern const GridApi* g_api;

// The bytes the entry's steps work on: in the game the PC's cells above, in
// the self-test an array of its own. `buffer` is nine bytes: the five cells
// while the entry runs, the layer's bytes and their NUL once it is answered.
struct EntryBytes {
    unsigned char *buffer, *cursor, *column, *row, *page, *done, *step;
};
EntryBytes LiveEntryBytes();

constexpr unsigned kSlotRoom = 5;     // a faerie slot's name, 0x9048F0 + 5 * slot
constexpr unsigned kMemberRoom = 8;   // a character record's name, its NUL at +8
constexpr unsigned kBufferBytes = 9;  // the longest name and its NUL

// The grid runs this entry: the switch on, a grid loaded, and not a slot
// under the Japan family (call 6).
bool GridOn(bool slot);
// The stored field (`room` bytes to a NUL, the PC's 0x20 padding) as five
// cells, 0xFF the blank: each one-byte 0x20 a blank, the trailing ones
// dropped, the rest decoded; then cut to the family's cells and to the
// field's bytes. A name the grid cannot decode opens blank (logged).
void StoredToCells(const unsigned char* stored, unsigned room, unsigned char out[5]);
// Entry-in at slide 0, the grid's: the cells into the buffer, the name cursor,
// column and row 0, the page the grid's first, the step on.
void GridEntryIn(const EntryBytes& e, const unsigned char* stored, unsigned room);
// The entry's input, the grid's: one frame of the routine on the cells; the
// answer byte 1 on a finish, else 0; the step on at a finish or a quit.
void GridEntryInput(const EntryBytes& e, unsigned room);
// Entry-out's answered end, the grid's: the cells encoded into the buffer, or,
// when the name is all blank (buffer byte 0 is 0), `fallback`'s bytes to its
// NUL (at most `fallback_room`); NUL-filled to kBufferBytes. Returns the
// name's bytes.
unsigned GridEntryAnswered(const EntryBytes& e, unsigned room, const unsigned char* fallback, unsigned fallback_room);
// The cells as the draws take them: the PC's bytes, or the open's (the first
// page, row and column 0) while those are off the grid (before the first
// entry-in has set them).
name_grid::Cells GridDrawnCells(const EntryBytes& e);
// The buffer's cells as the layer's bytes into `out` (kBufferBytes), for the
// panels, which draw the buffer as text.
void GridShownName(const EntryBytes& e, unsigned char out[kBufferBytes]);

}  // namespace commu_name

namespace rest_4d {
// rest_4d_fuzz.cpp, BOF3X_SHADOW=rest_4d, run once CommuNameGrid is set:
// the entry's steps with the switch on and no grid (and a slot under the
// Japan family) against Capcom's, DIV-0075's step the only difference; then
// the grid's paths against two synthetic grids.
void GridTest();
}
