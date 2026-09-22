// The port's PSX library layer, GTE half: the matrix product, libgte's three
// one-axis rotations and the two compositions over them (originals
// 0x5A7D70..0x5A80B0), and the camera's matrix load 0x57C070
// (docs/psx-library-layer.md section 4; DIV-0021).
#pragma once

// Must run before the modules that own these functions' callees: see the .cpp.
void PsxGteMatrix_Inject();
