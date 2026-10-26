// DIV-0181: an area's own .data put back when its DAT is loaded, as the
// PlayStation's overlay reload did. Area 108's three models (entry 0's flags byte
// and corners moved by the push scene, the cart's faces rewritten by the blast) and area 52's timed
// scene byte (D145). docs/DIVERGENCE.md.
#pragma once

#include <cstdint>

// At inject: a copy of the two areas' bytes as the image holds them before any
// scene ran. Switchable off with BOF3X_ORIGINAL=AreaDataReload.
void AreaDataReload_Inject();

// From Area_Enter, inside the branch that loads the area's DAT (the one place
// the PlayStation loaded the area's overlay too): the copy put back for `area`.
void AreaDataReload_OnLoad(unsigned area);
