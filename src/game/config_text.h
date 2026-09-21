#pragma once

#include <cstdint>

// DIVERGENCE DIV-0015: the in-game Config screen's text from a language
// overlay. See src/game/config_text.cpp and docs/config-screen.md.

// Applies a kind-7 chunk: six row labels, seventeen option records, six
// controller-panel names. Aborts loudly on a payload that does not describe
// exactly that, since every write below is into BOF3.exe's own .data.
void ConfigText_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// Moves the screen's own layout for Latin text: the label column right to the
// edge the PlayStation release uses, and every row's text down two pixels.
// Only with BOF3X_LANG set. See config_text.cpp.
void ConfigText_Inject();
