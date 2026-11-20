// SatPixie's options (launcher.rc IDD_SATPIXIE; DIV-0043).
// Not in this launcher yet: its boxes are not filled or read back, so its
// settings are written as they were read. docs/launcher-portable.md, stage P2.
#include "launcher/ui/pages.h"

namespace bof3x::ui {

void SatpixieOpen(Pages&) {}
void SatpixiePage(Pages&) { PageNotYet(); }

}  // namespace bof3x::ui
