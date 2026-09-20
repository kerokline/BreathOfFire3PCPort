// The port's PSX library layer, GTE half: the transform commands and libgte's
// functions over them, originals 0x5A8250..0x5A8F60, the two vector
// normalisations, ScaleMatrix and the cosine (docs/psx-library-layer.md).
#pragma once

// Must run before the modules that own these functions' callees: see the .cpp.
void PsxGteTransform_Inject();
