// tools/move_funcs.py (docs/source-layout-round.md). The functions of commu.cpp are declared
// by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void CommuFromRest4d_Inject();
void CommuFromRest4e_Inject();

// Commu_DrawTiledFrame with the original's placement of its texture windows
// (a RECT on the stack) - ours puts them in the packet pool (commu.cpp). For
// rest_4e's self-test alone, which compares this with Capcom's 0x45F1A0.
void Commu_DrawTiledFrameStackWindow(int x, int y, unsigned w, unsigned h);
