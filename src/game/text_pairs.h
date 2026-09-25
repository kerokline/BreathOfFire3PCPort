// DIVERGENCE DIV-0057: pair codes for Japanese names
// (docs/dialogue-localisation.md section 9).
#pragma once

#include <cstdint>

// Takes a copy of a kind-13 chunk: tag = the glyph index of the first pair
// code, payload = a u16 count, then that many (u16 first, u16 second) glyph
// indices. A pair code is drawn as its two glyphs; the table's own glyph at
// the pair code is a placeholder, so a draw that does not expand pairs shows
// it. Replaces any table set before. Aborts loudly on a malformed chunk.
void TextPairs_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// True, with its two glyphs, when `glyph` is a pair code of the loaded table.
// Always false with no table loaded - the original's behaviour exactly.
bool TextPair_Of(std::uint32_t glyph, std::uint32_t* first, std::uint32_t* second);

// True when the character at `text` (a two-byte code) is a pair code.
bool TextPair_At(const std::uint8_t* text);

// For a draw we do not own: `text` with each pair code replaced by its two
// glyphs' codes, in `buf` (NUL-terminated, at most `cap` bytes with the NUL).
// Returns `text` itself when it holds no pair code - always, with no table
// loaded. The two callers are the enemy name windows, whose 8-unit draw
// 0x516E70 is Capcom's (docs/dialogue-localisation.md section 9).
const std::uint8_t* TextPairs_Expand(const std::uint8_t* text, std::uint8_t* buf, std::uint32_t cap);
