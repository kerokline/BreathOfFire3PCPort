// BOF3X_DRAWORDER: a diagnostic log of the order the frame's primitives are
// drawn in, for a window of frames and a rectangle of the screen
// (docs/sprite-draw-order.md section 18).
//
//   BOF3X_DRAWORDER=F0-F1:X0,Y0,X1,Y1
//
// F0..F1 are recipe frames (the input script's count, the one its `shot`
// lines use) while a recipe plays, else Frame_Counter. X0..Y1 is a rectangle
// in the primitives' own 320 x 240 coordinates. For every rendered frame in
// the window, the walk of the ordering table logs, in draw order, each
// primitive whose screen box overlaps the rectangle: its place in the walk,
// the ordering-table slot it was reached through, its GPU code and box, and
// who put it there - a map cell's handler (layer, view row and column, the
// record's handler byte, the cell bytes, the texture word), a sprite (layer,
// draw key, object), a layer list item (list 0 / 1, the frame-node list, the
// draw table's key, with the view cell the item belongs to), or a bare commit
// (the caller's address).
//
// Instrumentation, not a replacement: unset, every hook below is one test of
// a byte that is never set, and nothing is written. Set, the hooks only read
// game memory and write this module's own tables. It arms after InjectAll's
// self-tests, so no fuzz ever sees it. No DIVERGENCE.md entry: it changes no
// behaviour.
#pragma once

#include <cstdint>

namespace draw_order {

extern bool g_on;   // BOF3X_DRAWORDER was set and parsed

enum Kind : std::uint8_t {
    kNone = 0,
    kCell,        // a map-cell handler's commit (DrawLayer_Open's walk)
    kSprite,      // Sprite_Draw's commit, from the draw pass
    kList0,       // a layer's first list, appended by DrawLayer_Open
    kList1,       // a layer's second list, appended by DrawLayer_Close
    kFrameNode,   // a layer's frame-node list (MapView_LinkPrimAt's rows), appended by the pass
    kTable,       // a draw-table item, merged with the sprites by key
    kRecord,      // a primitive Sprite_AddDrawRecords handed the pass
    kCommit,      // any other Gfx_CommitPrim
};

// What the code running now is drawing for, set around the calls that commit.
struct Context {
    Kind kind;
    unsigned layer;
    unsigned a, b, c, d;   // kCell: row, column, handler byte, b1 << 8 | b0; kSprite: key, object
    std::uint32_t texture; // kCell: MapCell_DrawQuads' texture word, 0 when not known
};
extern Context g_ctx;

// BOF3X_DRAWORDER parsed; call after InjectAll (dllmain).
void Start();

// True when the frame in hand is inside the tagging window (a frame before the
// log window too: the draw shows what the previous logic frame built).
bool Tagging();

void Set(Kind kind, unsigned layer, unsigned a = 0, unsigned b = 0, unsigned c = 0, unsigned d = 0);
void Clear();

// Gfx_CommitPrim's: the primitive at `prim` committed to `slot` under g_ctx.
void TagCommit(std::uint32_t prim, unsigned slot, std::uint32_t caller);
// A linked list appended whole: every node from `first` to `last`.
void TagList(std::uint32_t first, std::uint32_t last, Kind kind, unsigned layer, unsigned slot);
// One node linked alone.
void TagOne(std::uint32_t prim, Kind kind, unsigned layer, unsigned slot, unsigned a, unsigned b);

// Gfx_DrawOTag's, before the walk: logs the walk of `ot` when in the window.
void LogWalk(const unsigned long* ot);

}  // namespace draw_order
