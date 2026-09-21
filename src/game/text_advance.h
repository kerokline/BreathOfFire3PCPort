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

void TextAdvance_Inject();
