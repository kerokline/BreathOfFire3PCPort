#pragma once

#include <string>

#include "launcher/config.h"

namespace bof3x {

// Shows the settings dialog, modally. Returns true if the player pressed Play,
// with `cfg` updated to what the controls said; false if they closed it -
// closing the window must not save anything, so the caller does not save on
// false. On false `cfg` is not reliably untouched: an OK in the Look options,
// Cheats or Controls dialog has already written cfg.sp / .cheats / .bindings.
//
// `game_dir` is only read from: it decides whether the English option is
// offered at all.
bool ConfigDialogRun(const std::wstring& game_dir, Config& cfg);

}  // namespace bof3x
