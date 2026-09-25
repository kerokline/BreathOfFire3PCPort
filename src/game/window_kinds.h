// The window-kind handlers (originals 0x597A80..0x597E60, 0x597F60): the
// three enemy HP gauge states Window_DispatchKind picks between, the battle
// banner window and the battle message window the window task's battle
// handler 0x596FA0 runs as its kinds 6 and 7, with their states, and record
// handler 4 of Field_RunTaskRecords. docs/window_kinds.md.
#pragma once

void WindowKinds_Inject();
