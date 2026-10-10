// bof3x-launcher settings: bof3x.ini, the environment the game inherits, and
// the game's own BOF3.CFG.
//
// Three destinations, because there are three mechanisms:
//
//  - bof3x.ini holds every setting below, for the launcher itself.
//  - Everything ours - language, filter, the looks, the window, the cheats,
//    the bindings - reaches the DLL as BOF3X_* environment variables
//    (ConfigApplyEnvironment), and the game process inherits the launcher's,
//    so setting them here needs no new channel and no DLL change.
//  - Display and renderer are the ORIGINAL program's, read by Cfg_Load
//    0x4FD030 out of a two-line BOF3.CFG in the game directory
//    (docs/windowed-mode.md). We write that file rather than patch anything:
//    it is the port's own documented input, so this is not a divergence.
#pragma once

#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "game/language_tags.h"
#include "input/bindings.h"

namespace bof3x {

// A language overlay the game can load (DIV-0005; French and German
// DIV-0054, Japanese DIV-0056): BOF3X_LANG's tag, and the dialog's label.
// The tag is the BCP 47 tag of the PlayStation release the text came from
// (fixtures.toml's `tag` per build; docs/importer.md section 5): the two
// English discs differ (Sony Europe's terms and two renamed items), so each
// is its own entry. Built locally by tools/loc_build.py or the importer,
// never shipped, so the dialog offers only those whose DAT\<tag>.* files
// exist (ConfigLanguagesAvailable).
struct LanguageInfo {
    const char* code;
    const wchar_t* label;
};
inline constexpr LanguageInfo kLanguages[] = {
    {"en-US", L"English (US PlayStation script)"},
    {"en-150", L"English (European PlayStation script)"},
    {"fr-FR", L"French (PlayStation script)"},
    {"de-DE", L"German (PlayStation script)"},
    {"ja-JP", L"Japanese (PlayStation script)"},
};
// The DLL knows the same tags (game/language_tags.h, which also says which
// optional layer is text of which language); the two lists must agree.
constexpr bool LanguagesAgree() {
    if (std::size(kLanguages) != std::size(kLanguageTags)) return false;
    for (std::size_t i = 0; i < std::size(kLanguages); ++i)
        if (std::string_view(kLanguages[i].code) != kLanguageTags[i]) return false;
    return true;
}
static_assert(LanguagesAgree(), "kLanguages and game/language_tags.h kLanguageTags differ");
// "original" (no overlay, the port's Chinese) or one of kLanguages' tags. A
// bof3x.ini that still says a bare code from before 2026-10-08 (`language=en`,
// fr, de, ja) is refused, not mapped: ConfigLoad reports it and the launcher
// stops, naming the tag to write (RetiredLanguageReplacement,
// game/language_tags.h; DIV-0005).
constexpr const char* kLanguageOriginal = "original";

enum class Filter { kLinear, kPoint };   // the original's, and DIV-0012's
enum class Display { kFullscreen, kWindowed };

struct Config {
    std::string language = kLanguageOriginal;
    // DIV-0086: the optional layers (docs/opt-layers.md), BOF3X_OPT's
    // comma-separated list, in the order they land: psp-art, psp-tiles,
    // psp-maps, psp-names-en-150, psp-names-ja-JP, area4-walls. Built and
    // installed by tools/importer.py from the player's PSP disc (area4-walls:
    // a Western PSX disc). Empty (the default): the default layers that are
    // installed (kOptDefault); "none": no layer; a list: exactly that list.
    // The ini's `opt=` only, kept as written (ConfigOptValid); no dialog box
    // yet, so a save writes back what was read and never pins the default.
    std::string opt;
    // DIV-0087: the importer's cache root (BOF3X_CACHE), whose base\bgm songs
    // play through the sequencer, and the music source (BOF3X_MUSIC: "seq",
    // the cache's song where it has one, or "mp3"). Empty: unset, the default
    // (no cache; seq). The ini's `cache=` and `music=` only; no dialog box.
    std::string cache;
    std::string music;
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
    // DIV-0036 / DIV-0042: the first window's size, 320 x 240 times this,
    // 2..8 (BOF3X_SCALE), used only until the game has saved a placement of
    // its own (bof3x.window beside the dll). Not in the dialog since
    // 2026-09-23: the window is resized instead.
    int scale = 2;
    // DIV-0043: the SatPixie look (BOF3X_PRESENT=satpixie), the Look box's
    // third entry, over the point filter, with its parameters
    // (BOF3X_SATPIXIE). (Our own CRT look, DIV-0037, sat between Sharp and
    // this until 2026-09-27, when the owner withdrew it in SatPixie's
    // favour; an ini's screen=crt now means satpixie.) The preset's
    // defaults, but overscan off (the game's UI runs to the edge) and the
    // vignette over the whole picture (a 4:3 one leaves a wide picture's
    // bands bright).
    bool satpixie = false;
    struct Satpixie {
        float modulate = 0.65f, gamma = 2.3f, chroma = 0.7f, blur_x = 0.0f, blur_y = 0.0f;
        bool natural = true, ghosting = false, chroma_on = true, vignette = true, vignette_43 = false;
        bool wiggle = false, scanroll = true, overscan = false;
        int mask = 0;   // 0 off, 1 brightness lines, 2 colour stripes
    } sp;
    // DIV-0041: the wide picture, 426 x 240 (BOF3X_WIDE=1). Survey build.
    bool wide = false;
    // DIV-0042: the picture on the window at whole multiples of its size
    // (the default) or stretched to the client's height (BOF3X_SNAP=0).
    bool snap = true;
    // DIV-0045 / DIV-0046: the cheats behind the "Cheats..." button
    // (docs/cheats.md). The multipliers, 0..10, go out as BOF3X_EXP /
    // BOF3X_ZENNY when not 1; the steal switch as BOF3X_STEAL=1 when on.
    struct Cheats {
        int exp = 1, zenny = 1;
        bool steal = false;
    } cheats;
    // The physical bindings (docs/controls.md section 4.2, DIV-0050): the
    // keyboard's (scancode -> PlayStation bits) table, which replaces the
    // game's own when it differs from the default (BOF3X_KEYS; unset, the
    // game reads BOF3.CFG lines 3+ or its default as it always did), the pad
    // map and the face-button layout (BOF3X_PAD). Edited in the Controls...
    // dialog; `key.NAME=action` and `pad.NAME=action` lines in the ini.
    input::Bindings bindings = input::Bindings::Defaults();
    // Cleared by the dialog's "Show this window every time" box. --config
    // brings the dialog back whatever this says.
    bool show_launcher = true;
};

// The SatPixie parameters as `prefix name=value sep` for each, in the
// preset's names: the ini's lines, or BOF3X_SATPIXIE's list.
std::string SatpixieLine(const Config::Satpixie& sp, const char* prefix, const char* sep);

// Reads `path` if it is there, and returns whether there was one. Missing key
// and unparsable value both leave the default in place: a settings file is not
// a thing to fail on - with one exception: a retired bare language code
// (`language=en`, fr, de, ja; DIV-0005) sets `error`, the tag to write
// instead, and the launcher stops on it. `error` is empty otherwise.
bool ConfigLoad(const std::wstring& path, Config& cfg, std::string& error);

// Rewrites `path` whole, comments included. Returns false on a write error,
// which the caller reports without refusing to start the game.
bool ConfigSave(const std::wstring& path, const Config& cfg);

// Sets the BOF3X_* variables for every non-default setting in THIS process,
// which the game inherits; a default leaves its variable unset.
// A variable already present in the environment wins: the documented developer
// invocations (`BOF3X_LANG=en-US build/bof3x-launcher.exe`, docs/HANDOFF.md) must
// keep overriding whatever the file says.
// The language is exported only when its overlay is in <game_dir>\DAT
// (ConfigLanguagesAvailable), as the dialog offers it.
void ConfigApplyEnvironment(const std::wstring& game_dir, const Config& cfg);

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

// DIV-0089: the cache the DLL reads DAT\ files from - BOF3X_CACHE when the
// environment has it, else the ini's cache= - or empty when there is none or
// BOF3X_CACHE_DATA=0 (the cache for the music only). Where a layer can come
// from, for the three functions below.
std::wstring ConfigCacheDataRoot(const Config& cfg);

// The tags of kLanguages whose DAT\<tag>.* overlays exist in `game_dir`, or
// (DIV-0089) whose <cache>\loc\<tag>\dat\ holds a .DAT, in kLanguages' order -
// i.e. which languages tools/loc_build.py has built. The dialog offers only
// these.
std::vector<std::string> ConfigLanguagesAvailable(const std::wstring& game_dir,
                                                  const std::wstring& cache = std::wstring());

// The layers of `opt` (comma-separated) that can be played: those whose
// DAT\<layer>.* files exist in `game_dir` or (DIV-0089) whose
// <cache>\opt\<layer>\dat\ holds a .DAT, and of the text layers
// (LayerLanguage, game/language_tags.h) only those of `language`'s primary
// language - `language` being what the game is actually given as BOF3X_LANG,
// empty or "original" for none, under which no text layer plays. Each layer
// dropped is said on stderr; the DLL refuses either case at start-up.
std::string ConfigOptPlayable(const std::wstring& game_dir, const std::string& opt, const std::string& language,
                              const std::wstring& cache = std::wstring());

// The layers played when the ini's `opt=` is empty and BOF3X_OPT is unset,
// each only when its DAT\<layer>.*.DAT is installed or (DIV-0089) the cache
// has the layer: DIV-0080's walls, on by
// default since 2026-10-10 (the owner's word). `opt=none` turns them off, as
// does an `opt=` list that does not name them.
inline constexpr const char* kOptDefault[] = {"area4-walls"};

// The list the game is offered for an ini's `opt=` value (before
// ConfigOptPlayable): "none" -> empty; empty -> the kOptDefault layers
// installed in `game_dir`; anything else -> itself.
std::string ConfigOptWanted(const std::wstring& game_dir, const std::string& opt,
                            const std::wstring& cache = std::wstring());

// Whether an ini's `opt=` value can be honoured: empty, "none", or a
// comma-separated list of distinct names of letters, digits and '-' (at most
// 8, each 1..23 characters, as the DLL's ReadOptLayers takes them), none of
// them "none" or "original". On false `why` says what is wrong; the launcher
// refuses to start rather than fall back to the default layers.
bool ConfigOptValid(const std::string& opt, std::string& why);

// True for "original" or a tag in kLanguages.
bool ConfigLanguageKnown(const std::string& code);

// False, with `error` naming the tag to use, when the environment's
// BOF3X_LANG is a retired bare code (en, fr, de, ja; DIV-0005): the launcher
// stops on it before the game starts, as the DLL would at injection.
bool ConfigCheckEnvironmentLanguage(std::string& error);

}  // namespace bof3x
