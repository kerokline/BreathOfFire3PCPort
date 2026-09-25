// Per-glyph pen advance for the dialogue box, DIV-0006
// (docs/dialogue-localisation.md section 5).
#pragma once

#include <cstdint>

// Takes a copy of a kind-4 chunk: one byte a glyph, the pen advance in PSX
// pixels, indexed as the glyph table is; `space` is the advance of byte 0x20.
// Replaces any table set before.
void TextAdvance_Set(const std::uint8_t* advances, std::uint32_t count, std::uint32_t space);

// The advance of the character at `text` - its first byte, and its second if
// the first has bit 7 set. 12, the original's constant, when no table is
// loaded or the byte is not a glyph.
int TextAdvance_Of(const std::uint8_t* text);

// The advance of glyph `glyph`; 12 when no table is loaded or it is past it.
int TextAdvance_OfGlyph(std::uint32_t glyph);

// DIV-0006 for the dialogue box's own pen: Text_DrawAt, then MsgBox_PenX
// moved by (advance - 12) so that the stepper's `+ 12` lands the pen by the
// glyph's advance. Identical to Text_DrawAt with no advance table loaded.
// MsgBox_Step (src/game/msgbox.cpp) calls this where the original called
// Text_DrawAt; the RetargetCall below puts the same call into Capcom's body,
// which is what BOF3X_ORIGINAL=MsgBox_Step runs.
extern "C" const unsigned char* __cdecl MsgBox_DrawChar(int x, int y, int color, int count,
                                                        const unsigned char* text);

void TextAdvance_Inject();
