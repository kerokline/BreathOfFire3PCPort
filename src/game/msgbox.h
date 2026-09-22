// The dialogue box: its two openers' script side (Msg_OpenScript 0x4976D0),
// the reset 0x497770, the frame task 0x4977F0, the control-code stepper
// MsgBox_Step 0x497840 with its 23-entry table, the eight state handlers
// under MsgBox_StateDispatch 0x497AD0 and everything below them, the box
// effect task 0x4984E0 and its five handlers, the window slot allocator
// Window_Alloc 0x59E2D0, and the two remaining text pens - Text_DrawAt
// 0x516B30 and Text_EmitGlyph 0x516D50. docs/msgbox.md.
#pragma once

void MsgBox_Inject();
