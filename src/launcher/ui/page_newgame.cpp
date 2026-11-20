// Mods, Extras, Page 2: New Game's two screens (launcher.rc IDD_PAGE_EXTRA2).
// Not in this launcher yet: its boxes are not filled or read back, so its
// settings are written as they were read. docs/launcher-portable.md, stage P2.
#include "launcher/ui/pages.h"

namespace bof3x::ui {

void NewGameOpen(Pages&) {}
void NewGameApply(Pages&) {}
void NewGamePage(Pages&) { PageNotYet(); }

}  // namespace bof3x::ui
