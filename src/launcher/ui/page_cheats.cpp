// Game Options, Cheats (launcher.rc IDD_PAGE_GAME, IDD_PAGE_GAME2).
// Not in this launcher yet: its boxes are not filled or read back, so its
// settings are written as they were read. docs/launcher-portable.md, stage P2.
#include "launcher/ui/pages.h"

namespace bof3x::ui {

void CheatsOpen(Pages&) {}
void CheatsApply(Pages&) {}
void CheatsPage1(Pages&) { PageNotYet(); }
void CheatsPage2(Pages&) { PageNotYet(); }

}  // namespace bof3x::ui
