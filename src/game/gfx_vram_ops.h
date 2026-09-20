// ClearImage and MoveImage on the VRAM shadow, originals 0x59E650, 0x59E9A0
// and the row copier 0x5AA5D6 (docs/asset-loading-path.md section 2).
#pragma once

void GfxVramOps_Inject();
