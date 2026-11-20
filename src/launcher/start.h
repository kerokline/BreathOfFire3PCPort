// The start of the game with no window (docs/launcher-portable.md, stage P0):
// what launcher.cpp did between its arguments and CreateProcessW, less every
// window - finding the game folder, the checks of BOF3.exe, reading and
// checking bof3x.ini, the cache's manifest, the image of a disc-built install,
// the preset, BOF3.CFG and the environment (ConfigApplyGameCfg,
// ConfigApplyEnvironment), BOF3X_EXEIMAGE, and the start of bof3x. Every call
// here is the seam's (src/os) or the C++ library's, so it compiles for Linux.
//
// Two mains use it: launcher.cpp (Windows, the windows and the hosted build
// around these steps) and headless.cpp (no window, any system). Each main
// defines StartDie: the message box is launcher.cpp's alone.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "launcher/config.h"

namespace bof3::os {
struct Child;
}

namespace bof3x {

// Every refusal of these steps: `message` on stderr (and, by launcher.cpp, in
// a message box), and the launcher ends with exit code 1. Defined by each main.
[[noreturn]] void StartDie(const std::wstring& message);

// A narrow text as the wide message text a "%hs" made of it (os/wide_text.h).
std::wstring StartWide(const std::string& narrow);
// "bof3x-launcher: " and `line` on stderr; `line` ends with its own new line.
void StartSay(const std::wstring& line);

// BOF3X_SELFTEST_ONLY: the game ends once its self-tests have run, and the
// launcher waits for it and ends with its exit code (launcher.cpp's header).
bool StartSelfTestOnly();

// A part of a path written with backslashes, with the system's separator.
std::wstring StartSep(const wchar_t* part);
bool StartFileExists(const std::wstring& path);
// The folder of this program; StartDie when the system does not say.
std::wstring StartOwnDirectory();
// The name made absolute (GetFullPathNameW's road); StartDie when it cannot be.
std::wstring StartFullPath(const std::wstring& path);

// The game folder for this pass - `named` (--game or BOF3_GAME_DIR), the
// working folder when it is a game folder, the home's game folder, the PC
// folder the home's sources.toml says is played in place (play_in_place =
// true) when it is a game folder, else ./bof3, made absolute - and the path of
// its BOF3.exe. True when this start
// reads BOF3.exe (docs/exe-retire-round.md, X2): only the developer's - the
// hosted build, or BOF3X_EXEFILE=1 - with the file there; then it must be the
// build this project targets, with the engine's default key table, or
// StartDie. Hosted, BOF3.exe must be there. A player's start never opens,
// hashes or tests for the file: false, and the working folder is a game
// folder by InstallGameFolder (the developer's start: by BOF3.exe, as before).
// A BOF3X_EXEFILE other than 0 or 1 is a StartDie.
bool StartFindGame(const std::wstring& named, bool hosted, std::wstring* game_dir, std::wstring* exe);

// What the Sources page sets up before a player's start
// (docs/exe-retire-round.md X3). The launcher never asks BOF3.exe whose folder
// it is: the page hands a folder to the importer, whose `identify` says.
enum class StartSetUp {
    kNone,         // nothing: the game starts, or StartApply refuses (StartRefuseNoImage)
    kFirstRun,     // the page's first run: no install at all, or the home's own game folder with no image
                   // (a disc-built game\ whose cache was deleted: its source list builds it again)
    kThisFolder,   // a game folder elsewhere with no image (a PC folder played in place): the page adds it
                   // as a source and the importer builds the home's cache from it; the folder stays the game
                   // folder, read and not changed, so its saves and BOF3.CFG stay where the game finds them
};

// The facts StartSetUpFor decides on; StartReadSettings fills them.
struct StartSetUpAsk {
    bool hosted = false;          // the hosted build: never
    bool exe_read = false;        // StartFindGame's answer (BOF3X_EXEFILE=1 with the file): never
    bool self_test = false;       // BOF3X_SELFTEST_ONLY: never
    int want_dialog = -1;         // 0 (--no-config, the windowless launcher): never
    bool named = false;           // --game or BOF3_GAME_DIR named the game folder
    bool game_folder = false;     // InstallGameFolder(game folder): DAT\FIRST.DAT
    bool home_game = false;       // the game folder is the home's game\ (HomeGameDefault)
    bool cache = false;           // ConfigCacheDataRoot: a cache for .data
    bool cache_image = false;     // that cache holds base\exe (an image of .data)
    bool cache_chosen = false;    // BOF3X_CACHE, cache= or BOF3X_CACHE_DATA=0 choose the cache: not the home's to build
    bool exe_image_set = false;   // BOF3X_EXEIMAGE names the image: never
};

// The rule. No install at all (no game folder, no cache, not named): the
// first run, as before the round. A game folder with no image and the home's
// cache to build: the home's own game\ gets the first run, any other folder
// is set up in place. Everything else: nothing.
inline StartSetUp StartSetUpFor(const StartSetUpAsk& a) {
    if (a.hosted || a.exe_read || a.self_test || a.want_dialog == 0 || a.exe_image_set) return StartSetUp::kNone;
    if (!a.game_folder) return !a.named && !a.cache ? StartSetUp::kFirstRun : StartSetUp::kNone;
    if (a.cache_image || a.cache_chosen) return StartSetUp::kNone;
    return a.home_game && !a.named ? StartSetUp::kFirstRun : StartSetUp::kThisFolder;
}

// bof3x.ini into `cfg` and its checks, before any window (DIV-0005, DIV-0102,
// DIV-0086, DIV-0108): StartDie on a refusal. Returns what the Sources page
// is to set up first (StartSetUpFor; never under a self-test or `want_dialog`
// 0, the windowless launcher's always); then DIV-0102's language check waits
// for the install the page makes. `exe_read`: StartFindGame's answer.
StartSetUp StartReadSettings(const std::wstring& ini, const std::wstring& game_dir, const std::wstring& named,
                             bool hosted, bool exe_read, int want_dialog, Config& cfg);

// A player's start with no image of .data: no cache, or a cache with no
// base\exe. StartDie with the sentence that names the set-up (the Sources
// page; the importer's setup for a developer, or BOF3X_EXEFILE=1). With the
// launcher's windows the set-up runs first (StartSetUpFor); this is the
// refusal under --no-config, a self-test, the windowless launcher and a cache
// the player chose.
[[noreturn]] void StartRefuseNoImage(const std::wstring& game_dir, const std::wstring& cache);

// After the settings: the cache's manifest checked (StartDie with the DLL's
// sentence); .data's source - for every start that does not read BOF3.exe
// (`exe_read` false, not hosted) BOF3X_EXEIMAGE, the cache's image, whatever
// built it, or StartRefuseNoImage; then the preset, BOF3.CFG (not under a
// self-test) and the environment the game inherits. `hosted`: no layer the
// hosted build refuses.
void StartApply(const std::wstring& game_dir, Config& cfg, bool hosted, bool exe_read);

// bof3x started from `program` with `args` (each passed in double quotes on
// Windows, as the launcher always passed them) in `game_dir`, with
// BOF3X_LAUNCHER=1. `while_suspended`, when not null, is the developer
// launcher's fence of a moved image (Windows only): the child starts suspended,
// the call runs, then the child runs. Under a self-test: the pid line, the wait
// and the exit code line, `*waited` true and the game's exit code returned.
// Else the "started" line on stdout and 0.
std::uint32_t StartGame(const std::wstring& program, const std::vector<std::wstring>& args,
                        const std::wstring& game_dir, void (*while_suspended)(bof3::os::Child&), bool* waited);

}  // namespace bof3x
