// The dialogue box: its two openers' script side (Msg_OpenScript 0x4976D0),
// the reset 0x497770, the frame task 0x4977F0, the control-code stepper
// MsgBox_Step 0x497840 with its 23-entry table, the eight state handlers
// under MsgBox_StateDispatch 0x497AD0 and everything below them, the box
// effect task 0x4984E0 and its five handlers, the window slot allocator
// Window_Alloc 0x59E2D0, and the two remaining text pens - Text_DrawAt
// 0x516B30 and Text_EmitGlyph 0x516D50. docs/msgbox.md.
#pragma once

void MsgBox_Inject();

// DIV-0140's self-test (text_draw.cpp, ButtonText_Arm): MsgBox_Step run over
// `msg` until `glyphs` characters are drawn, from a box at (0x10, 0x20), every
// draw recorded instead of committed - its pen, its colour byte and the glyph
// bytes it was handed. The stepper's state is put back after. Returns the
// draws made (at most `max` recorded).
struct MsgBoxDraw {
    int x, y, color;
    unsigned char glyph[2];
};
unsigned MsgBox_StepRecord(const unsigned char* msg, unsigned glyphs, MsgBoxDraw* out, unsigned max);
