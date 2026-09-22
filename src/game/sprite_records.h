// A 3D object's quads for the draw-order pass: Sprite_AddDrawRecords
// 0x57BAE0 and its two helpers, Sprite_ObjectMatrix 0x57BFF0 and
// Light_ObjectDirection 0x57BED0 (docs/sprite-draw-order.md section 12).
#pragma once

// Must run before the modules that own any function these call: see the .cpp.
void SpriteRecords_Inject();
