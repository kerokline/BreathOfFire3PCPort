// The Sources page (docs/install-sources.md): the player's own copies of the
// game - the PC game folder, a PlayStation disc image (.cue/.bin, or a .zip of
// them) or a PSP .iso - each checked against the releases fixtures.toml
// catalogues, then built into the home's install (common/home.h): game\ and
// cache\. The checking and the building are tools/importer.py's (`identify`,
// `sources`, `setup`), run on the Python the release carries in runtime\
// (or, in a source checkout, the system's); this page only runs it and shows
// what it says.
#pragma once

#include <windows.h>

#include "launcher/config.h"

namespace bof3x {

// Shows the page, modally, over `owner` (may be null). `first_run`: no install
// was found, so the page says so and offers to make one. Returns true when a
// build finished, so the game folder and the cache may have changed and the
// caller re-reads them. cfg.ffmpeg is updated by Locate...; the caller saves.
bool SourcesDialogRun(HWND owner, Config& cfg, bool first_run);

}  // namespace bof3x
