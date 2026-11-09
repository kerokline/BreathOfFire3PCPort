// Texture-cache invalidation, original 0x59E700: what the renderer built from
// a region of the PSX-VRAM shadow is dropped when that region changes
// (docs/asset-loading-path.md section 2, docs/IDEAS.md I8).
#pragma once

void GfxTexCache_Inject();

namespace gfx_texcache {
extern void(__cdecl* g_original_clone)(const short*, int);   // defined in gfx_texcache.cpp (source-layout B0: promoted for a moved reader)
}  // namespace gfx_texcache
