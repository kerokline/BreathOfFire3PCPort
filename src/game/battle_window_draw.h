// The battle windows' draw helpers: the four window openers, the digit strip,
// the shaded quads, tiles, bars, cells and lines the battle window handlers
// draw with, and the skill helpers under the skill list (the seventh round's
// group BD, docs/battle_window_draw.md).
#pragma once

#include <cstdint>

void BattleWindowDraw_Inject();

namespace battle_window_draw {
extern std::uint8_t g_draw_signed;   // defined in battle_window_draw.cpp (source-layout B0: promoted for a moved reader)
}  // namespace battle_window_draw
