// The menu and shop windows: the menu box and its pieces, the small fonts,
// the button row, the Yes / No chooser, the item list, the equipment and
// shop panels (the sixth round's group Y, docs/menu-windows.md).
#pragma once

#include <cstdint>

void MenuWindows_Inject();

namespace menu_windows {
extern std::uint8_t g_signed_xy;   // defined in menu_windows.cpp (source-layout B0: promoted for a moved reader)
}  // namespace menu_windows
