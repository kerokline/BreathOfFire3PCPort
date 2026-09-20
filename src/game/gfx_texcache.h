// Texture-cache invalidation, original 0x59E700: what the renderer built from
// a region of the PSX-VRAM shadow is dropped when that region changes
// (docs/asset-loading-path.md section 2, docs/IDEAS.md I8).
#pragma once

void GfxTexCache_Inject();
