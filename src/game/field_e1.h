// Round twelve group FE1: the field engine's resident code 0x52D080..0x533BA0
// (the cut's "event script, first seven runs") - the panel draws, the
// leader's states 6, 8, 11 and 12 with their steps, a passage's event, the
// zenny found, the cells around a sprite, the gateway exit, the party's
// placements and the pending jump's members. docs/field_e1.md.
#pragma once

void FieldE1_Inject();

namespace field_e1 {
// BOF3X_SHADOW=field_e1: the start-up fuzz, field_e1_fuzz.cpp. Clones every
// original before FieldE1_Inject patches it.
void SelfTest();
}  // namespace field_e1
