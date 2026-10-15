#pragma once

#include <string>

#include "launcher/config.h"

namespace bof3x {

enum ConfigDialogResult { kDialogClosed = 0, kDialogPlay = 1, kDialogSourcesChanged = 2, kDialogSave = 3 };

// Shows the launcher's window, modally: the title page, and Configure's tabs
// (docs/launcher-settings.md section 2a). Returns kDialogPlay if the player
// pressed Play, with `cfg` updated to what the pages said; kDialogClosed if
// they left without Play or Back - nothing to save, so the caller does not
// save then. On kDialogClosed `cfg` is not reliably untouched: an OK in the
// Look options dialog has already written cfg.sp. kDialogSave: they left
// after Back to the title page, which applied the pages to `cfg`; the caller
// saves and starts nothing. kDialogSourcesChanged: the Sources page rebuilt
// the install (docs/install-sources.md); `cfg` is read back as for Play, and
// the caller saves, finds the game folder and the cache again, and shows the
// window anew - on the Data page it was left from.
//
// `game_dir` is only read from: it decides whether the English option is
// offered at all.
ConfigDialogResult ConfigDialogRun(const std::wstring& game_dir, Config& cfg);

}  // namespace bof3x
