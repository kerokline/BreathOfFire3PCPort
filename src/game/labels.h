#pragma once

#include <cstdint>

// DIVERGENCE DIV-0064: the menu's and the battle's short labels - the two
// status words, the four stats, the item types, the skill types - from a
// language overlay. See src/game/labels.cpp and
// docs/dialogue-localisation.md section 8.

// Applies a kind-15 chunk: the tag names the table (1 the status words, 2 the
// menu's stats, 3 the item types, 4 the skill types, 5 the battle's stats),
// the payload is a count and that many NUL-terminated strings, each written
// into its slot in BOF3.exe's .data; an empty string keeps the slot as
// shipped. Aborts loudly on anything else.
void Labels_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// For Text_DrawSmall: the glyph of the 8 x 8 UI set for the one-byte
// character at `text`, when `text` lies in the status words' slots, a chunk
// has written them and the byte is a letter or a digit; 0 otherwise, and the
// draw keeps the original's glyph.
unsigned Labels_SmallGlyph(const unsigned char* text);
