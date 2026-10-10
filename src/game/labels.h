#pragma once

#include <cstdint>

// DIVERGENCE DIV-0064: the menu's and the battle's short labels - the two
// status words, the four stats, the item types, the skill types - from a
// language overlay. See src/game/labels.cpp and
// docs/dialogue-localisation.md section 8.

// Applies a kind-15 chunk: the tag names the table (1 the status words, 2 the
// menu's stats, 3 the item types, 4 the skill types, 5 the battle's stats, ...
// 12 the gene window's tabs, 13 the faerie village's lists, 14 its words -
// the list in src/game/labels.cpp),
// the payload is a count and that many NUL-terminated strings, each written
// into its slot in BOF3.exe's .data; an empty string keeps the slot as
// shipped. Aborts loudly on anything else.
void Labels_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// For Text_DrawSmall: the glyph of the 8 x 8 UI set for the one-byte
// character at `text`, when `text` lies in the status words' slots, a chunk
// has written them and the byte is a letter or a digit; 0 otherwise, and the
// draw keeps the original's glyph.
unsigned Labels_SmallGlyph(const unsigned char* text);

// Whether every slot of the 8 px group `tag` (1 the status words, 10 the
// formation names) has been written by the overlay: its text is then one
// byte a letter.
bool Labels_SmallWritten(std::uint32_t tag);

// Whether every slot of group `tag` has been written by the overlay. A draw
// whose original passes Text_DrawAt a glyph count sized for the Chinese
// word (two or three glyphs) passes 0xFF instead once the group is written:
// the overlay's strings are NUL-ended, one byte a letter, and longer than
// the count (DIV-0064, groups 13 and 14).
bool Labels_Written(std::uint32_t tag);

// Slot `i` of group `tag` as the draw should read it: the overlay's string in
// our buffer once a chunk has written it, else the shipped slot in
// BOF3.exe's .data. For the groups our own draws read (14, the village's
// words), whose slots are not rewritten in place.
const unsigned char* Labels_Slot(std::uint32_t tag, std::uint32_t i);

// The widest string of group `tag` as the pen covers it (TextAdvance_Width),
// over the slots a chunk has written; 0 before any. For a box whose width
// the original sized for the Chinese word (the village's list boxes, 0x30
// for three glyphs; DIV-0064 group 13).
unsigned Labels_MaxWidth(std::uint32_t tag);
