// Game Options, Solo character (launcher.rc IDD_PAGE_SOLO).
// Not in this launcher yet: its boxes are not filled or read back, so its
// settings are written as they were read. docs/launcher-portable.md, stage P2.
#include "launcher/ui/pages.h"

namespace bof3x::ui {

void SoloOpen(Pages&) {}
void SoloApply(Pages&) {}
void SoloPage(Pages&) { PageNotYet(); }

}  // namespace bof3x::ui
