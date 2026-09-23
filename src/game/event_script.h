// The event script interpreter and the flag helpers (docs/event-script.md):
// the area's placement script run by Area_RunPlacement 0x579740 through
// EventScript_Run 0x5797C0 - its op dispatch, the if / switch control ops
// and their skips, the object placements the attract cycle reaches (1x, 2x,
// Bx, Cx) and their helpers - and the bit-flag helpers Flags_Set / Clear /
// Test with the three conditions it reaches, ScriptFlags_Set40 and
// ObjTrio_SetBit40. 28 functions.
#pragma once

void EventScript_Inject();
