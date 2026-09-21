// The port's string draw, original 0x516B70 (symbols.toml, Text_DrawAt and
// Text_DrawString; docs/dialogue-localisation.md section 2).
#pragma once

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
