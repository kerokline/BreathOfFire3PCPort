// The top-level task flow (originals 0x495040..0x495900, 0x4967F0..0x496C90,
// 0x594E60, 0x595160, 0x59E330 and the small resets 0x454810..0x454AB0,
// 0x461E10): the boot task and the title's loader, the field task with its
// modes 0 and 1 and the play clock, the area-entry hub, and the screen
// transition task with its 21 kinds. docs/mode-flow.md.
#pragma once

void ModeFlow_Inject();
