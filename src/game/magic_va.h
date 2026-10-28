// The spell harness's address accessors for a VA held as a
// bof3::image::Va (common/image_base.h; docs/relocatable-image-design.md 2.2):
// each places the VA through Img and calls the integer accessor, so a spell
// unit's module constant typed Va reads as before - Mem(kS + k) - and a site
// that would use one as a value does not compile until it says which it is.
//
// Kept beside magic_harness.h rather than in it: the harness header is shared
// with the area, boss and scenario harnesses, and these overloads are the spell
// units' (relocatable-image round, stage B group RB1). A unit includes this
// before its `using magic_harness::Mem;` so the using-declaration takes both.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"

namespace magic_harness {

inline unsigned char* Mem(::bof3::image::Va va) { return Mem(::bof3::Img(va)); }
inline unsigned char* Pointer(::bof3::image::Va va) { return Pointer(::bof3::Img(va)); }
inline void SetPointer(::bof3::image::Va va, const void* p) { SetPointer(::bof3::Img(va), p); }

}  // namespace magic_harness
