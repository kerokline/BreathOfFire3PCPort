// PROTOTYPE (BOF3X_INTERP=1, unledgered, off by default): one in-between
// frame. The game shows each recorded frame one period after it is recorded
// (Gpu_PutDispEnv presents, then Gfx_DrawOTag records the next), so between
// two presents the shim already holds the next frame whole. Half a period
// after frame N-1 is shown, the loop shows frame N's draws with their
// vertices halfway back to the matching draws of frame N-1; then frame N at
// its own time, as ever. Nothing the game computes changes, and no frame is
// shown later than it was.
//
// Matching is by what a draw is, not where it sits in the list: first the
// same surface, state and texture coordinates (untextured: colours), nearest
// first; then, for what is left, the same shape of draw nearby (a sprite
// whose cel changed). A draw with no match, or one that moved further than
// the cap, is shown where frame N has it.
#pragma once

#include "render/render_shim.h"

namespace render::interp {

struct Stats {
    U draws;       // kDraw commands in the frame
    U exact;       // matched on surface, state and texture coordinates
    U loose;       // matched on shape alone
    U moved;       // of the matched, those whose vertices differ
    U far_;        // a candidate existed but moved beyond the cap
    U unmatched;   // no candidate
    U followed;    // of the last two, those moved with a matched neighbour
};

// BOF3X_INTERP set and not 0.
bool On();

// Keeps `frame` as the frame the next Build moves back towards.
void Keep(const Frame& frame);

// Writes frame.n_vertices vertices to `out`: frame's own, the matched ones
// with x, y, z and rhw halfway to the kept frame's. max_move is the cap in
// the vertices' own units (target pixels). False when no frame is kept.
bool Build(const Frame& frame, Vertex* out, float max_move, Stats* stats);

}  // namespace render::interp
