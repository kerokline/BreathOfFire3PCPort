// The title menu's row widths from a language overlay, DIV-0014
// (docs/title-menu.md).
#pragma once

#include <cstdint>

// A kind-6 chunk: tag 0, three bytes - the widths in texels of the title
// menu's three rows (new game, load game, options), which the original's draw
// 0x5888D0 holds as immediates. Aborts on any other tag or size, or a zero
// width.
void TitleMenu_SetWidths(std::uint32_t tag, const std::uint8_t* widths, std::uint32_t size);
