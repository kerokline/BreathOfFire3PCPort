// DIV-0038: F9's pause lines in English (src/game/pause_text.cpp).
#pragma once

#include <cstdint>

// Called when an overlay's advance table is installed (dat_load.cpp, chunk
// kind 4): the English glyphs are in the table from then on, so the four
// pause-line pointers are re-aimed at English strings. Once per process.
// Any kind-4 chunk calls it - the Japanese overlay's too (tools/loc_build.py).
void PauseText_Apply();

// Where our WinMain draws a pause line: centred on 160 by its real width when
// it is one of ours, else `original_x` (100 or 0x70, Capcom's).
int PauseText_X(const unsigned char* line, int original_x);
