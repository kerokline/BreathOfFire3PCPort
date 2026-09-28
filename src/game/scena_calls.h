// The chapter call tables' entries (round ten, group CALLS): the 98 functions
// at 0x519890..0x51AC50 that Scenario_CallA / Scenario_CallB reach through the
// per-chapter tables Scena<NN>_CallA / _CallB - each one party change (a party
// set loaded, members joined or sent away, the palettes reloaded).
// docs/scena_calls.md.
#pragma once

void ScenaCalls_Inject();

namespace scena_calls {
// BOF3X_SHADOW=scena_calls: the start-up fuzz, scena_calls_fuzz.cpp. Clones
// every original before ScenaCalls_Inject patches it.
void SelfTest();
}  // namespace scena_calls
