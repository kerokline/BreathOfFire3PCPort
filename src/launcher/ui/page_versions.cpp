// Game Options: the three suites (launcher.rc IDD_PAGE_VERSIONS, IDD_PAGE_VERSIONS2; docs/launcher-suites.md).
// Not in this launcher yet: its boxes are not filled or read back, so its
// settings are written as they were read. docs/launcher-portable.md, stage P2.
#include "launcher/ui/pages.h"

namespace bof3x::ui {

void VersionsOpen(Pages&) {}
void VersionsApply(Pages&) {}
void VersionsGraphicsPage(Pages&) { PageNotYet(); }
void VersionsDataPage(Pages&) { PageNotYet(); }

}  // namespace bof3x::ui
