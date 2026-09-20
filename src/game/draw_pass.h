// The field's draw-order pass, original 0x593060 (docs/sprite-draw-order.md).
#pragma once

// Must run before the modules that own its callees: see the .cpp.
void DrawPass_Inject();
