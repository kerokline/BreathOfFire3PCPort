// DIVERGENCE DIV-0027: the Yes / No chooser Menu_YesNo 0x5747D0 laid out for
// Latin text - the words' spacing and the hand's two stops. Only under a
// language overlay. See src/game/yes_no_layout.cpp and docs/glyph-draw.md.
#pragma once

// DIV-0027, and DIV-0029's save-slot name inset with it. Only with BOF3X_LANG
// set, not "original", and not a full-width language (DIV-0056). Also the
// master's "Is this OK?" prompt (Capcom's 0x586D20, two calls re-aimed,
// BOF3X_ORIGINAL=MasterAskLayout).
void YesNoLayout_Inject();

// What YesNoLayout_Inject put into Menu_YesNo's body, for our Menu_YesNo
// (src/game/menu_windows.cpp): the line's call target (ours under DIV-0027,
// null for Msg_SystemPtr) and whether the hand's stops moved.
using YesNoLayout_LineFn = const unsigned char* (__cdecl*)(unsigned id);
YesNoLayout_LineFn YesNoLayout_ActiveLine();
bool YesNoLayout_StopsMoved();

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
    int stop[2];   // [0] the first answer (Yes), [1] the second (No)
};
YesNoTail YesNoLayout_Tail(const unsigned char* s, int x, const char* who);
