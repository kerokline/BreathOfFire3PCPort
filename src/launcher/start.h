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
// working folder when it is a game folder, the home's game folder, else
// ./bof3, made absolute - and the path of its BOF3.exe. True when this start
// reads BOF3.exe (docs/exe-retire-round.md, X2): only the developer's - the
// hosted build, or BOF3X_EXEFILE=1 - with the file there; then it must be the
// build this project targets, with the engine's default key table, or
// StartDie. Hosted, BOF3.exe must be there. A player's start never opens,
// hashes or tests for the file: false, and the working folder is a game
// folder by InstallGameFolder (the developer's start: by BOF3.exe, as before).
// A BOF3X_EXEFILE other than 0 or 1 is a StartDie.
bool StartFindGame(const std::wstring& named, bool hosted, std::wstring* game_dir, std::wstring* exe);

// bof3x.ini into `cfg` and its checks, before any window (DIV-0005, DIV-0102,
// DIV-0086, DIV-0108): StartDie on a refusal. True when there is no install at
// all and the Sources page is to make one (never under a self-test or
// `want_dialog` 0, the windowless launcher's always). `exe_read`:
// StartFindGame's answer.
bool StartReadSettings(const std::wstring& ini, const std::wstring& game_dir, const std::wstring& named, bool hosted,
                       bool exe_read, int want_dialog, Config& cfg);

// A player's start with no image of .data: no cache, or a cache with no
// base\exe. StartDie with the sentence that names the set-up (the Sources
// page; the importer's setup for a developer, or BOF3X_EXEFILE=1).
// docs/exe-retire-round.md X3 puts the set-up before its one call, in
// StartApply.
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
