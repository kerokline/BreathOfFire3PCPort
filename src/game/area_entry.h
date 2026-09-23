// The area's link list and the drop-in party placement: Area_LinkAt (the
// cell-to-destination list GameMode_Enter and the field's event code walk),
// Party_DropIn (the placement Area_Enter and 390-odd scenario call sites
// make), and the three it calls - the members' set-up, the slot swap and the
// swap of the records' fields. docs/area-entry.md.
#pragma once

void AreaEntry_Inject();
