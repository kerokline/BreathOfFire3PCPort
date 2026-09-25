// DIVERGENCE DIV-0027: the Yes / No chooser Menu_YesNo 0x5747D0 laid out for
// Latin text - the words' spacing and the hand's two stops. Only under a
// language overlay. See src/game/yes_no_layout.cpp and docs/glyph-draw.md.
#pragma once

// DIV-0027, and DIV-0029's save-slot name inset with it. Only with BOF3X_LANG
// set, not "original", and not a full-width language (DIV-0056).
void YesNoLayout_Inject();

// What YesNoLayout_Inject put into Menu_YesNo's body, for our Menu_YesNo
// (src/game/menu_windows.cpp): the line's call target (ours under DIV-0027,
// null for Msg_SystemPtr) and whether the hand's stops moved.
using YesNoLayout_LineFn = const unsigned char* (__cdecl*)(unsigned id);
YesNoLayout_LineFn YesNoLayout_ActiveLine();
bool YesNoLayout_StopsMoved();
