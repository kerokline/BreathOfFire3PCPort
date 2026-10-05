// Group R2B's callees of other groups, by address: ours since their owners
// merged, named by symbol, the values unchanged (R3G's rebinding and round
// fourteen's, docs/round-14-cleanup.md). docs/rest_2b.md section 6.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"  // R3G's rebinding: Screen_TriangleWinding, the value unchanged

namespace rest_2b::at {

// R2C (wave two): model A's draw - record A's (0x9398E0) scale from
// Shisu_ModelScales by 0x9399EB, its colour tinted by the fourth count, drawn
// through Shisu_DrawModel and the colour put back. void(void); reached by
// Shisu_ModelATurn's tail jmp and MasterFigure_States[2..5]'s.
constexpr std::uint32_t kModelADraw = bof3::addr::MasterFigure_DrawFaded;

// R3G (wave three): the winding of three screen points - (const float *a,
// const float *b, const float *c), each an (x, y) float pair; the cross
// product's z through _ftol (a tail jmp); the callers test ax signed
// (effect_2c_callees.h's kWinding, docs/effect_gte.md section 7).
constexpr std::uint32_t kWinding = bof3::addr::Screen_TriangleWinding;

}  // namespace rest_2b::at
