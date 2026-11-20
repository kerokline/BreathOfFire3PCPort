// Music: the song source (launcher.rc IDD_PAGE_MUSIC; DIV-0150).
// Not in this launcher yet: its boxes are not filled or read back, so its
// settings are written as they were read. docs/launcher-portable.md, stage P2.
#include "launcher/ui/pages.h"

namespace bof3x::ui {

void MusicOpen(Pages&) {}
void MusicApply(Pages&) {}
void MusicPage(Pages&) { PageNotYet(); }

}  // namespace bof3x::ui
