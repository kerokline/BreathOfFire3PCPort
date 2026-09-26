// The effect library (queue group L): the helpers the BMAGIC overlays call,
// linked after MAGIC226/227 at 0x4FAFF0..0x4FC32F - the buff popup and its
// two tasks, the buff roll, the depth sort, the steps toward a point, the
// proximity tests, the sprite-CLUT helpers, the side's centre and two
// per-actor lookups. Their prototypes are symbols.toml's (and
// docs/magic_lib.md section 2 lists them). docs/magic_lib.md.
#pragma once

void MagicLib_Inject();

namespace magic_lib {
// BOF3X_SHADOW=magic_lib: the start-up fuzz, magic_lib_fuzz.cpp. Clones every
// original before MagicLib_Inject patches it.
void SelfTest();
}  // namespace magic_lib
