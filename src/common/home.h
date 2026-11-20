// The install's home folder (docs/install-sources.md): the one place a
// player's install keeps what the launcher makes - game\ (the game folder the
// Sources page lays out), cache\ (the importer's cache), sources.toml (the
// ordered list of the player's sources) and runtime\ (the importer and the
// Python it runs on, in a release). The log, crash dumps, screenshots,
// bof3x.ini and bof3x.window already live beside the launcher and the DLL,
// which is the home by default, so they need no change.
//
// Every older way of naming a folder still wins over the home's default:
// --game and BOF3_GAME_DIR, BOF3X_CACHE and the ini's cache=. In a source
// checkout the launcher sits in build\, which holds no game\ or cache\, so
// the defaults there are what they were (./bof3 and no cache).
#pragma once

#include <string>

namespace bof3x {

// BOF3X_HOME when the environment sets it, else the folder of the running
// executable (the launcher's; the release keeps everything beside it). No
// trailing separator. Empty only when Windows cannot say where we run.
std::wstring HomeDir();

// <home>\game when it holds a DAT folder (an install the Sources page laid
// out), else empty.
std::wstring HomeGameDefault();

// <home>\cache when it holds manifest.toml (a cache the importer built),
// else empty.
std::wstring HomeCacheDefault();

// The PC folder this home plays in place (docs/exe-retire-round.md X3, X6):
// the path of the first [[source]] row of <home>\sources.toml
// (tools/player_sources.py save_sources) that says `play_in_place = true`,
// or empty. Whether it is a game folder is the caller's to ask
// (InstallGameFolder): launcher/start.cpp StartFindGame takes it after
// HomeGameDefault. The path is a basic string as json.dumps writes it: \" and
// \\ are read, and a row with any other escape is not taken (a Windows path
// holds no control character); a relative path is the home's.
std::wstring HomeInPlaceFolder();

}  // namespace bof3x
