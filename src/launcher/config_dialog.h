#pragma once

#include <string>

#include "launcher/config.h"

namespace bof3x {

enum ConfigDialogResult { kDialogClosed = 0, kDialogPlay = 1, kDialogSourcesChanged = 2 };

// Shows the settings dialog, modally. Returns kDialogPlay if the player
// pressed Play, with `cfg` updated to what the controls said; kDialogClosed if
// they closed it - closing the window must not save anything, so the caller
// does not save then. On kDialogClosed `cfg` is not reliably untouched: an OK
// in the Look options, Cheats or Controls dialog has already written cfg.sp /
// .cheats / .bindings. kDialogSourcesChanged: the Sources page rebuilt the
// install (docs/install-sources.md); `cfg` is read back as for Play, and the
// caller saves, finds the game folder and the cache again, and shows the
// dialog anew.
//
// `game_dir` is only read from: it decides whether the English option is
// offered at all.
ConfigDialogResult ConfigDialogRun(const std::wstring& game_dir, Config& cfg);

}  // namespace bof3x
