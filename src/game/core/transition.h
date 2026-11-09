// tools/move_funcs.py (docs/source-layout-round.md). The functions of transition.cpp are declared
// by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void Transition_InjectFromModeFlow();
void Transition_InjectFromModeRest();
void Transition_InjectFromModeStates();
void Transition_InjectFromModeTasks();
void Transition_InjectFromRest3g();
