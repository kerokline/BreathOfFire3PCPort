// Group R2B's callees that are not ours yet, by raw address (the round's
// rebinding turns them into names once their owners merge). docs/rest_2b.md
// section 6.
#pragma once

#include <cstdint>

namespace rest_2b::at {

// R2C (wave two): model A's draw - record A's (0x9398E0) scale from
// Shisu_ModelScales by 0x9399EB, its colour tinted by the fourth count, drawn
// through Shisu_DrawModel and the colour put back. void(void); reached by
// Shisu_ModelATurn's tail jmp and MasterFigure_States[2..5]'s.
constexpr std::uint32_t kModelADraw = 0x57F340;

// R3G (wave three): the winding of three screen points - (const float *a,
// const float *b, const float *c), each an (x, y) float pair; the cross
// product's z through _ftol (a tail jmp); the callers test ax signed
// (effect_2c_callees.h's kWinding, docs/effect_gte.md section 7).
constexpr std::uint32_t kWinding = 0x4941B0;

}  // namespace rest_2b::at
