// The port's PSX library layer, GTE half: the functions that go through x87,
// originals 0x5A8340, 0x5A8380 and 0x5A9110..0x5A94A0
// (docs/psx-library-layer.md section 3).
#pragma once

void PsxGteFloat_Inject();
