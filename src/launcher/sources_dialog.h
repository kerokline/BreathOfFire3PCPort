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

#include <string>
#include <vector>

#include "launcher/config.h"

namespace bof3x {

// Shows the page, modally, over `owner` (may be null). `first_run`: no install
// was found, so the page says so and offers to make one. `rebuild`: the
// player agreed to rebuild a stale install (SourcesAskRebuild), so the build
// starts at once. Returns true when a build finished, so the game folder and
// the cache may have changed and the caller re-reads them. cfg.ffmpeg is
// updated by Locate...; the caller saves.
bool SourcesDialogRun(HWND owner, Config& cfg, bool first_run, bool rebuild = false);

// The home's install against this release (HANDOFF item 11,
// docs/install-staleness.md): `importer.py stale --home H`, which judges each
// piece of the cache by what it was measured to read when it was built. Run
// once for each launcher and importer the install meets - a fresh answer is
// remembered in <home>\cache\stale-checked.txt by their file sizes and
// times, so an unchanged release asks nothing at the next start.
struct StaleCheck {
    bool stale = false;                  // a piece would be made differently: offer the rebuild
    std::vector<std::wstring> pieces;    // each, as the player reads it (the step and why)
    std::wstring error;                  // the check could not run: said, and the game starts
};
StaleCheck SourcesCheckStale();

// Asks the player (a message box naming the pieces) whether to rebuild them
// now: true for yes, and the caller opens the Sources page with `rebuild`; no
// starts nothing (the owner's call, 2026-10-17: prompt, then rebuild).
bool SourcesAskRebuild(const StaleCheck& check);

}  // namespace bof3x
