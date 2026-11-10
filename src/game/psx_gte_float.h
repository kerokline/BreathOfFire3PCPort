// The port's PSX library layer, GTE half: the functions that go through x87,
// originals 0x5A8340, 0x5A8380 and 0x5A9110..0x5A94A0
// (docs/psx-library-layer.md section 3).
#pragma once

void PsxGteFloat_Inject();

namespace psx_gte_float {
extern void(__cdecl* g_perspective_clone)(float*, float*, long*);   // defined in psx_gte_float.cpp (source-layout B0: promoted for a moved reader)
extern long(__cdecl* g_ramp_clone)(long);   // defined in psx_gte_float.cpp (source-layout B0: promoted for a moved reader)
}  // namespace psx_gte_float
