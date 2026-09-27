// The engine-side helpers the scenario chapters share (round ten, group SE):
// the event battle's start, a party placed without sprites, the event
// script's placement op 0x and its facing, an effect object at a cell, and
// chapter 8's party. docs/scena_se.md.
#pragma once

void ScenaSe_Inject();

namespace scena_se {
// BOF3X_SHADOW=scena_se: the start-up fuzz, scena_se_fuzz.cpp. Clones every
// original before ScenaSe_Inject patches it.
void SelfTest();
}  // namespace scena_se
