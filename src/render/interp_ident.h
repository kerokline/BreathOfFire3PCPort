// PROTOTYPE (interp.h, BOF3X_INTERP): which thing of the game a recorded draw
// is, told by the game's own draw code, for the in-between's matcher
// (docs/frame-interpolation.md section 3, rule 0).
//
// The matcher pairs draws by what they look like. A figure whose cel changes
// is another texture in another box, drawn by the same function for the same
// object (measured 2026-11-21 with BOF3X_DRAWORDER on pathCouple: every
// figure of 510 frames is Sprite_Draw's, cel change or not), so its look says
// nothing and its object says everything. Sprite_Draw names the object when
// it builds the primitive; the cell sprite's handler passes the name on with
// the place the object stands at in the picture, and the matcher moves the
// draw by that place's own step.
//
// Tooling of the in-between and nothing else: with BOF3X_INTERP unset every
// call returns at once and nothing is kept. BOF3X_INTERP_IDENT=0 keeps the
// names (the debug lines print them) and leaves the matcher's rule off, for
// an A/B of the rule. No game memory is written at any setting.
#pragma once

#include <cstdint>

namespace render::interp {

// The in-between is on: names are kept.
bool IdentOn();

// Sprite_Draw's: the primitive at `prim` (its address as the ordering table
// holds it) draws `object`.
void TagPrim(std::uint32_t prim, std::uint32_t object);
// Gfx_BeginFrame's: the packet pool [lo, hi) is about to be filled again, and
// the names of the primitives it held are forgotten.
void ForgetPrims(std::uint32_t lo, std::uint32_t hi);
// The object named for `prim`, 0 for none.
std::uint32_t ObjectOf(std::uint32_t prim);

// The cell sprite handler's: command `index` of the frame being recorded draws
// `object`, which stands at (x, y) in the vertices' own units.
void NoteDraw(std::uint32_t index, std::uint32_t object, float x, float y);
// The shim's ResetFrame: the frame's notes are dropped.
void ForgetDraws();
// How many commands the frame being recorded holds (render_shim.cpp).
std::uint32_t CommandsRecorded();

}  // namespace render::interp
