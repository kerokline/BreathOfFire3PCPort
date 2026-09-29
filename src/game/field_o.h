// Group FO of round twelve, wave two: 41 functions of the field engine's
// resident code in 0x5738A0..0x57CD89 - eight menu panels (the stats, EXP,
// icon wheel, tile, equipment compare, ability, item and save-slot panels),
// the movement script's ops 87, 88, DB, E9 and F9 with their state handlers,
// five event-script placement ops, thirteen event-script conditions,
// EventScript_SkipIf and ObjTrio_ClearBit40 - through the scenario harness's
// field mode (scenario_harness.h, Group::field). docs/field_o.md.
#pragma once

void FieldO_Inject();

namespace field_o {
// BOF3X_SHADOW=field_o: the start-up fuzz, field_o_fuzz.cpp. Clones every
// original before FieldO_Inject patches it.
void SelfTest();
}  // namespace field_o
