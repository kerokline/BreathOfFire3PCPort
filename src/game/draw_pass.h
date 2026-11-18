// The field's draw-order pass, original 0x593060 (docs/sprite-draw-order.md).
#pragma once
#include <cstdint>

// Must run before the modules that own its callees: see the .cpp.
void DrawPass_Inject();

namespace draw_pass {
extern void(__cdecl* g_open)(int);   // defined in draw_pass.cpp (source-layout B0: promoted for a moved reader)
extern unsigned(__cdecl* g_add_records)(std::uint32_t*);   // defined in draw_pass.cpp (source-layout B0: promoted for a moved reader)
extern void(__cdecl* g_draw)();   // defined in draw_pass.cpp (source-layout B0: promoted for a moved reader)
}  // namespace draw_pass
