// tools/move_funcs.py (docs/source-layout-round.md). The functions of transition.cpp are declared
// by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void TransitionFromModeFlow_Inject();
void TransitionFromModeRest_Inject();
void TransitionFromModeStates_Inject();
void TransitionFromModeTasks_Inject();
void TransitionFromRest3g_Inject();
