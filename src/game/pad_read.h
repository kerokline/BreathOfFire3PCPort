// The pad word: the keyboard through the game's own DirectInput keyboard and
// key table, the pad through SDL3 (DIV-0050). Originals DInput_Init 0x5A94C0,
// Pad_Read 0x5A9700, DInput_Shutdown 0x5A9690 (docs/controls.md).
#pragma once

void PadRead_Inject();

// Whether any input of the pad is down right now - a button, a trigger, a
// direction - bound or not; false with no pad or SDL not started. Polls the
// pad first, so it is for the main thread between latches (the FMV player's
// skip, DIV-0066).
bool PadRead_AnyInputDown();
