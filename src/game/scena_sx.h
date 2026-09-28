// The engine callees the chapter and area groups call by raw address and
// nobody owned (round ten, group SX): the level-up, the ability, key-item,
// inventory and zenny changes, the party's placement, removal, heal and
// palette reload, a CLUT flash, an HP loss, a cell-record search, the view's
// refill, two camera eases, a free sprite slot and a status bit.
// docs/scena_sx.md.
#pragma once

void ScenaSx_Inject();

namespace scena_sx {
// BOF3X_SHADOW=scena_sx: the start-up fuzz, scena_sx_fuzz.cpp. Clones every
// original before ScenaSx_Inject patches it.
void SelfTest();
}  // namespace scena_sx
