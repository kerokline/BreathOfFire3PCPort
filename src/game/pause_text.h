// DIV-0038: F9's pause lines in the overlay's language (src/game/pause_text.cpp).
#pragma once

#include <cstdint>

// Chunk kind 14 (dat_load.cpp): four NUL-terminated lines in the overlay's
// own encoding - in game F9 / other key, then on the title - written after
// the overlay's glyphs. Re-aims the four pause-line pointers at copies of
// them. Once per process.
void PauseText_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// Where our WinMain draws a pause line: centred on 160 by its real width when
// it is one of ours, else `original_x` (100 or 0x70, Capcom's).
int PauseText_X(const unsigned char* line, int original_x);
