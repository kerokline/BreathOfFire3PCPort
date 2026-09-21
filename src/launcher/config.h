// bof3x-launcher settings: bof3x.ini, the environment the game inherits, and
// the game's own BOF3.CFG.
//
// Three destinations, because there are three mechanisms:
//
//  - Language and texture filter are ours. The DLL reads them from the
//    environment (BOF3X_LANG, BOF3X_FILTER), and the game process inherits the
//    launcher's, so setting them here needs no new channel and no DLL change.
//  - Display and renderer are the ORIGINAL program's, read by Cfg_Load
//    0x4FD030 out of a two-line BOF3.CFG in the game directory
//    (docs/windowed-mode.md). We write that file rather than patch anything:
//    it is the port's own documented input, so this is not a divergence.
//  - Resolution has no mechanism yet. 640x480 is welded into the presentation
//    layer (docs/IDEAS.md I8); the dialog shows the control disabled rather
//    than pretending otherwise.
#pragma once

#include <string>

namespace bof3x {

enum class Language { kOriginal, kEnglish };
enum class Filter { kLinear, kPoint };   // the original's, and DIV-0012's
enum class Display { kFullscreen, kWindowed };

struct Config {
    Language language = Language::kOriginal;
    Filter filter = Filter::kLinear;
    Display display = Display::kFullscreen;
    // BOF3.CFG line 2. Renderer select by association - 0x5A5160 holds the only
    // reference to the "Software Render" string - but which value is which is
    // not established, so it is offered as the shipped default and "the other
    // one" and labelled that way.
    int renderer = 1;
    // Cleared by the dialog's "Show this window every time" box. --config
    // brings the dialog back whatever this says.
    bool show_launcher = true;
};

// Reads `path` if it is there, and returns whether there was one. Missing key
// and unparsable value both leave the default in place: a settings file is not
// a thing to fail on.
bool ConfigLoad(const std::wstring& path, Config& cfg);

// Rewrites `path` whole, comments included. Returns false on a write error,
// which the caller reports without refusing to start the game.
bool ConfigSave(const std::wstring& path, const Config& cfg);

// Sets BOF3X_LANG / BOF3X_FILTER in THIS process, which the game inherits.
// A variable already present in the environment wins: the documented developer
// invocations (`BOF3X_LANG=en build/bof3x-launcher.exe`, docs/HANDOFF.md) must
// keep overriding whatever the file says.
void ConfigApplyEnvironment(const Config& cfg);

// Reads display and renderer OUT of <game_dir>\BOF3.CFG into `cfg`. Called
// when there is no settings file yet, so that a first run adopts whatever the
// player already had rather than imposing the defaults over it - without this
// the first launch silently turned a hand-written windowed BOF3.CFG back to
// fullscreen. Absent or unparsable file leaves `cfg` alone.
void ConfigSeedFromGameCfg(const std::wstring& game_dir, Config& cfg);

// Writes lines 1-2 of <game_dir>\BOF3.CFG, preserving every later line: lines
// 3+ are integer pairs consumed by 0x5A9860 and are not ours to discard
// (docs/windowed-mode.md, Open). Writes nothing at all when the file is absent
// and the settings are the ones a missing file already means. `error` is set
// on failure.
bool ConfigApplyGameCfg(const std::wstring& game_dir, const Config& cfg, std::wstring& error);

// True when DAT\en.* exists in `game_dir`, i.e. tools/loc_build.py has been
// run. The English option is offered only then.
bool ConfigEnglishAvailable(const std::wstring& game_dir);

}  // namespace bof3x
