// The engine callees the chapter and area groups call by raw address and
// nobody owned after group SX (round ten, group SX2): a flag toggle, a
// timed story flag, a height-layer byte, a member's state, a key item out, the
// ability list for an id's type, the formation offset of a slot, two camera
// turn steps and a degrees wrapper, every sound channel stopped, a cue's
// volume, and the 16 x 16 SPRT set-up. docs/scena_sx2.md.
#pragma once

void ScenaSx2_Inject();

namespace scena_sx2 {
// BOF3X_SHADOW=scena_sx2: the start-up fuzz, scena_sx2_fuzz.cpp. Clones every
// original before ScenaSx2_Inject patches it.
void SelfTest();
}  // namespace scena_sx2
