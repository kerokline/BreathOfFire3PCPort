// The converted-palette cache: VRAM rows in the display's pixel format.
// Originals 0x59EB00, 0x59EBB0, 0x5A04C0 (docs/asset-loading-path.md section 2).
#pragma once

void GfxClut_Inject();

namespace gfx_clut {
extern bool g_stp_test;   // defined in gfx_clut.cpp (source-layout B0: promoted for a moved reader)
}  // namespace gfx_clut
