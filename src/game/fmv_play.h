// The FMV player Fmv_Play 0x59E360, replaced: the AVI decoded by our own
// decoders (src/video, DIV-0205) into the game's window at an integer scale,
// no MCI, no second DirectDraw, no exclusive mode (DIV-0035,
// docs/window-modes.md, docs/replacing-mci.md section 8).
#pragma once

void FmvPlay_Inject();
