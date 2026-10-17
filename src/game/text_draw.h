// The port's string draw, original 0x516B70 (symbols.toml, Text_DrawAt and
// Text_DrawString; docs/dialogue-localisation.md section 2).
#pragma once

#include <cstdint>

#include "game/button_text.h"

void TextDraw_Inject();

// DIVERGENCE DIV-0016: the glyph-index guard follows the loaded table.
//
// The original compares against a flat 0xA00 (0x516C94) and runs a privileged
// instruction above it - a debug trap. The shipped table has 0x993 glyphs, so
// that bound was already loose by 109. A language overlay's table is larger,
// and the guard now tracks it: `max(0xA00, glyphs - 1)`, which is the
// original's number exactly for every shipped file and a tighter, truer bound
// for ours. Called by Font_SetGlyphData with the table it has just taken.
void TextDraw_SetGlyphCount(unsigned glyphs);

// The button unification round's DT-2, ledger draft H (docs/button-dialogue.md
// section 4; the decision is game/button_text.h's). Each reader of the area
// text - Text_DrawString here, MsgBox_Step in msgbox.cpp - asks these at every
// 0x05 it walks, with `p` on the 0x05:
//   ButtonText_Code  a layer's button code `05 Bn 06 05 cc G` (DT-1) whose
//                    drawn button, Sigma(1 << n), is not the one G shows;
//   ButtonText_Cue   the PC's own text with no layer, in a button area: the
//                    colour cue `05 cc G` whose drawn button is not the
//                    colour's.
// True: `out` holds the colour for the `05 cc` and the glyph for G. False
// before ButtonText_Arm, under BOF3X_ORIGINAL=ButtonText, outside the area
// text block, and whenever the drawn button is the one the text shows - so
// with the Config at the text's release's defaults every draw is the
// original's. A code that cannot be drawn honestly stops the game (rule 4).
bool ButtonText_Code(const std::uint8_t* p, button_text::Icon& out);
bool ButtonText_Cue(const std::uint8_t* p, button_text::Icon& out);

// After every module's self-test (inject_all.cpp, after sigma::Arm): the
// self-test (BOF3X_SHADOW=button_text), the layer and the port's stand-ins
// read, the switch ButtonText.
void ButtonText_Arm();
