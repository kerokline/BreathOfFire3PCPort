// DIVERGENCE DIV-0027: the Yes / No chooser Menu_YesNo 0x5747D0 laid out for
// Latin text - the words' spacing and the hand's two stops. Only under a
// language overlay. See src/game/yes_no_layout.cpp and docs/glyph-draw.md.
#pragma once

#include <cstdint>

// DIV-0027, and DIV-0029's save-slot name inset with it. Only with BOF3X_LANG
// set, not "original", and not a full-width language (DIV-0056). Also the
// master's "Is this OK?" prompt (MasterScreen_AskYesNo 0x586D20, two calls re-aimed,
// BOF3X_ORIGINAL=MasterAskLayout).
void YesNoLayout_Inject();

// What YesNoLayout_Inject put into Menu_YesNo's body, for our Menu_YesNo
// (src/game/menu_windows.cpp): the line's call target (ours under DIV-0027,
// null for Msg_SystemPtr) and whether the hand's stops moved.
using YesNoLayout_LineFn = const unsigned char* (__cdecl*)(unsigned id);
YesNoLayout_LineFn YesNoLayout_ActiveLine();
bool YesNoLayout_StopsMoved();

// The rest of what YesNoLayout_Inject put into Capcom's code, as values of
// ours (docs/operand-store.md), for our reimplementations of the functions
// that held it: what MasterScreen_AskYesNo's two calls reach (0x586E78:
// Text_DrawAt 0x516B30 or MasterAsk_Line; 0x586E97: Menu_DrawHand 0x5905D0 or
// MasterAsk_Hand; rest_2d.cpp), and the save slot's name inset (the disp8 at
// 0x576A48: 0x13, DIV-0029's 0x15; field_o.cpp).
std::uint32_t YesNoLayout_MasterLineDraw();
std::uint32_t YesNoLayout_MasterHandDraw();
unsigned char YesNoLayout_SaveNameInset();

// DIV-0027 (amended 2026-10-03, group YN): the same layout for a prompt that
// carries its own answers - a question, spaces, then the two answer words at
// the line's end ("Will that be all?" + 10 spaces + "Yes No"). The line with
// three spaces moved from before the first answer into the gap between them,
// and the hand's two stops: two units left of each answer as the line,
// drawn at `x`, places it (the pen's own advances, DIV-0006) - the load /
// save screen's offsets. Aborts naming `who` on any other shape. The line is
// a static buffer, good until the next call.
struct YesNoTail {
    const unsigned char* line;
    int stop[2];    // [0] the first answer (Yes), [1] the second (No)
    int width[2];   // each answer's pen width on the line (the faerie board's tile, DIV-0027 amended 2026-10-26)
};
YesNoTail YesNoLayout_Tail(const unsigned char* s, int x, const char* who);

// DIV-0027 (amended 2026-10-03): the shop's yes / no - the help window's
// prompt with system message 0xF drawn over it (ShopWin_TitleRun, help ids
// 0x36, 0x49, 0x4A, 0x52) and the hand the shop's steps place
// (shop_states2.cpp's YesNoFrame, field_s.cpp's SharedList_UseItem). The
// line as Menu_YesNo's DIV-0027 re-spaces it.
const unsigned char* YesNoLayout_SystemLine(const unsigned char* line);

// The help line's yes / no hand x over that line drawn at `line_x`: `answer`
// 1 is Yes, 0 No, as 0x929F0B holds it; two units left of each word as the
// pen places it, and linear in the answer as the original's 0xE8 - 36 *
// answer is.
int YesNoLayout_ShopHandX(int line_x, int answer);
