// Committing a primitive to the ordering table, original 0x461E50, and the
// draw-order pass's layer close, 0x56FE80 (docs/sprite-draw-order.md).
#pragma once

// Must run before psx_gpu's Inject: see the .cpp.
void DrawEmit_Inject();
