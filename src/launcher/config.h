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
//  - The window size is ours too since the Direct3D 11 backend: BOF3X_SCALE,
//    the render target's integer scale of 320 x 240 in a window (DIV-0036).
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
    // BOF3.CFG line 2, Cfg_RenderMode: Capcom's set-up's device index. 0 is
    // the synthetic "Software Render" record - its set-up takes 0x5A60E0's
    // software branch through 0x5AA671 and the MMX probe 0x5A9A30 - and 1 the
    // Direct3D HAL (traced 2026-09-23, docs/window-modes.md 4a). Only Capcom's
    // set-up reads it (BOF3X_ORIGINAL=Display_Setup): ours draws with
    // Direct3D 11 whatever it says (DIV-0031).
    int renderer = 1;
    // DIV-0033: the game keeps running while its window is not in front.
    // Off, the original's freeze and replay. BOF3X_BACKGROUND=0 in the
    // environment is the same switch for scripts.
    bool background = true;
    // DIV-0036: a window's render target is 320 x 240 times this, 2..8
    // (BOF3X_SCALE). A borderless window ignores it and takes the largest
    // that fits the monitor.
    int scale = 2;
    // DIV-0037: the CRT look in the present (BOF3X_PRESENT=crt). The dialog
    // offers it as the filter box's third entry, over the point filter.
    bool crt = false;
    // DIV-0041: the wide picture, 426 x 240 (BOF3X_WIDE=1). Survey build.
    bool wide = false;
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
