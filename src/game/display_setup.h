// The display set-up 0x5A5160, replaced by the Direct3D 11 backend
// (docs/render-backend.md section 4).
#pragma once

void DisplaySetup_Inject();

// DIV-0036: the scale k of a windowed render target, 320k x 240k - the
// launcher's window size, BOF3X_SCALE, 2 when unset. A borderless window
// instead takes the largest k that fits it, at set-up.
unsigned DisplaySetup_WindowedScale();

// The render target's scale once our set-up has run, else 0.
unsigned DisplaySetup_TargetScale();

// The width of the picture in the game's pixels: 320, or 426 under BOF3X_WIDE=1
// (DIV-0041, src/game/widescreen.h). The height is always 240.
unsigned DisplaySetup_ViewWidth();
