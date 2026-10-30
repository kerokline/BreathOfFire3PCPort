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

#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "game/language_tags.h"
#include "input/bindings.h"
#include "launcher/ledger_registry.h"

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

// The presets (the owner's call 2 of 2026-10-27, docs/ledger-fields-round.md
// section 5; docs/launcher-settings.md "preset="): one choice that turns off a
// set of the ledger's entries at launch, on top of bof3x.ini. Recommended (the
// default) turns off nothing. The rules are PresetClassify's, below; the
// reference they must agree with is `python tools/ledger_registry.py presets`.
enum class Preset { kRecommended, kShipped, kRepaired, kPlayStation };

struct Config {
    std::string language = kLanguageOriginal;
    // Whether bof3x.ini named the language (a language= line) or the dialog
    // chose it, as against the default above: on a disc-only install the
    // default becomes the first language held, a named "original" is refused
    // (ConfigResolveLanguage, DIV-0102).
    bool language_named = false;
    // The row above the text (the reading-row round, docs/mods.md 5): empty
    // (the default) none; else the middle of the optional text layer
    // ruby-<this>-<language played> - an installed reading-row mod's id
    // (furigana) or another held language's tag (en-US over ja-JP). Kept as
    // written; at start the layer is added to BOF3X_OPT when it is there
    // (ConfigRubyPlayable), else said on stderr and no row is drawn, as for a
    // language= whose overlay is missing.
    std::string ruby;
    // The reading row's study key (DIV-0155, docs/reading-row.md 3.5): the key
    // that hides and shows the reading rows' glyphs in play, BOF3X_RUBY_KEY's
    // value - f1..f12 as src/hook/input_script.cpp spells them, or none (the
    // key left free). F4 by default, a key Capcom's Game_WndProc leaves
    // unbound. Read in any case and kept lower case; any other value is
    // dropped to the default, as filter= and music= are, since the DLL stops
    // at start on a value it does not know (src/game/msgbox.cpp).
    std::string ruby_key = "f4";
    // Whether Settings shows the "Row above the text" box (the owner's call,
    // 2026-10-22: hidden by default, shown by a toggle on the Mods tab's Extra
    // sub-page; installing a reading-row mod there turns it on). Display only:
    // ruby= is kept and given to the game whether the box shows or not.
    // ruby_ui=0|1; any other value is dropped to the default, as snap= is.
    // ruby_ui_named: the ini had a valid ruby_ui= line - without one (an
    // older ini) ConfigRubyUiDefault turns it on when a reading-row mod is
    // installed.
    bool ruby_ui = false;
    bool ruby_ui_named = false;
    // DIV-0158, the faster text (Mods, Extras): Msg Speed's Med. at Fast's
    // delays and Fast a page at once. text_speed=0|1 -> BOF3X_TEXT_SPEED; any
    // other value is dropped to the default (0, the original's), as snap= is.
    bool text_faster = false;
    // DIV-0179 (Mods, Extras): an enemy's skill the party can still learn has
    // its battle banner in blue. unlearned_blue=0|1 -> BOF3X_UNLEARNED_BLUE;
    // any other value is dropped to the default (0, the original's white).
    bool unlearned_blue = false;
    // DIV-0086: the optional layers (docs/opt-layers.md), BOF3X_OPT's
    // comma-separated list, in the order they land: psp-art, psp-tiles,
    // psp-maps, psp-names-en-150, psp-names-ja-JP, area4-walls. Built and
    // installed by tools/importer.py from the player's PSP disc (area4-walls:
    // a Western PSX disc). Empty (the default): the default layers that are
    // installed (kOptDefault); "none": no layer; a list: exactly that list.
    // Kept as written (ConfigOptValid). The dialog's three suites
    // (launcher/suites.h, recipes/suites.toml; they replaced the "PSP extras"
    // boxes) change it only when a suite's choice is changed
    // (SuitesOptEdit -> ConfigOptEdit), so a save after an untouched dialog
    // writes back what was read and never pins the default.
    std::string opt;
    // DIV-0108 (src/game/psp_code.h): the PSP's two battle rules,
    // BOF3X_PSP's list - duel-loss, myria-item - or "none". Empty (the
    // default): on when the cache's base is the PSP's, off otherwise; the DLL
    // decides. Kept as written; the DLL refuses a name it does not know. The
    // data suite's two code members change it only when the suite's choice is
    // changed (SuitesPspEdit).
    std::string psp;
    // The player's switch (docs/launcher-settings.md, "off="): BOF3X_OFF's
    // list of code names - the ledger's `Switch:` names that a PatchBytes or a
    // RetargetCall reads (python tools/ledger_switches.py) - in
    // BOF3X_ORIGINAL's list syntax: `*` every code name, `-name` takes one
    // back out. Empty (the default): none off. Kept as written; the game
    // refuses a name it does not read (detour.cpp InjectReport). Not a
    // hosted-only switch: both hosts read it.
    std::string off;
    // The preset (preset=recommended|shipped|repaired|playstation; the
    // Configure page's Preset box): applied at launch on top of every other
    // key, which it does not rewrite (ConfigPresetPlan / ConfigPresetApply).
    // Any other value is dropped to the default, as filter= is, and said on
    // stderr; BOF3X_PRESET in the environment wins (ConfigPresetFromEnvironment).
    Preset preset = Preset::kRecommended;
    // DIV-0087: the importer's cache root (BOF3X_CACHE), whose base\bgm songs
    // play through the sequencer, and the music source (BOF3X_MUSIC; DIV-0150:
    // "psx", "psp" or "pc", the source played first - ConfigMusicSource; the
    // older "seq" and "mp3" kept as written). Empty: unset, the default
    // (no cache; the PlayStation's). The dialog's Music box and Cache folder box (since
    // 2026-10-10) write the same `music=` / `cache=` lines a hand edit does:
    // an untouched box writes back what was read.
    std::string cache;
    std::string music;
    // ffmpeg.exe as the Sources page's Locate... found it (UTF-8), handed to
    // the importer as BOF3X_FFMPEG for the PSP's music (tools/at3.py); empty:
    // BOF3X_FFMPEG or PATH. The game never reads it (docs/install-sources.md).
    std::string ffmpeg;
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
    // 2026-09-23: the window is resized instead. scale_named: the ini had a
    // scale= line with a value 2..8, which goes to the game as BOF3X_SCALE,
    // 2 included; an empty or missing scale= sends nothing, and the game then
    // takes k from the display's scale (DIV-0036's 2026-10-11 amendment). Until
    // 2026-10-28 scale=2 sent nothing and ConfigSave wrote scale=2 back for a
    // player who named none, so the ini could neither fix k = 2 nor say
    // "follow the display" (D395).
    int scale = 2;
    bool scale_named = false;
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
    // DIV-0041: the picture's width - 0 the original's 320 x 240, 1 the 16:9
    // picture 426 x 240 (BOF3X_WIDE=1), 2 the 21:9 picture 560 x 240
    // (BOF3X_WIDE=2; docs/ultrawide-frame.md).
    int wide = 0;
    // DIV-0042: the picture on the window at whole multiples of its size
    // (the default) or stretched to the client's height (BOF3X_SNAP=0).
    bool snap = true;
    // DIV-0045 / DIV-0046 / DIV-0187: the cheats on the Cheats sub-page
    // (docs/cheats.md). The multipliers, 0..10, go out as BOF3X_EXP /
    // BOF3X_ZENNY when not 1; the steal switch as BOF3X_STEAL=1 when on; the
    // enemy-information switch as BOF3X_ENEMY_INFO=1 when on; the no-fail
    // switch (DIV-0188, the minigames) as BOF3X_NO_FAIL=1 when on.
    struct Cheats {
        int exp = 1, zenny = 1;
        bool steal = false;
        bool enemy_info = false;
        bool no_fail = false;
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
// `hosted`: the launcher is starting the hosted build (--host bof3, or a
// hosted-only switch), which refuses a layer with an exe part (the exe tables:
// bof3x.exe lays .data ranges); every such layer is left out of the BOF3X_OPT
// it sets (ConfigOptHosted), with one line naming them. A BOF3X_OPT already in
// the environment is the DLL's to refuse, as before.
void ConfigApplyEnvironment(const std::wstring& game_dir, const Config& cfg, bool hosted = false);

// The parts of an optional layer the hosted build cannot take: "exe" (the exe tables,
// dat_load.cpp ReadOptLayers' refusal). The engine's rule, by part kind.
inline constexpr const char* kHostedRefusesParts[] = {"exe"};

// `list` (comma-separated, BOF3X_OPT's form) less every layer the hosted build
// refuses: one whose part in recipes/psp-layers.toml (suites.gen.h
// kLayerParts) is in kHostedRefusesParts, or whose cache tree has such a part
// (the DLL's own test, dat_cache::HasLayerPart; a layer no recipe names).
// `dropped` gets their names, in order.
std::string ConfigOptHosted(const std::string& list, const std::wstring& cache, std::vector<std::string>& dropped);

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

// The cache the game is given at all - BOF3X_CACHE when the environment has
// it, else the ini's cache=, else the home's own cache\ when the Sources page
// built one (common/home.h) - whatever BOF3X_CACHE_DATA says: the music's
// (DIV-0087; a layer's bgm part) and the manifest's, whose base_policy
// the DLL reads under either setting.
std::wstring ConfigCacheRoot(const Config& cfg);

// The cache path's encoding (docs/state3-design.md section 9, the owner's
// "both halves", 2026-10-11): Config::cache holds UTF-8, in the ini and in
// memory; the launcher hands it to the game wide (SetEnvironmentVariableW),
// and bof3x.exe, whose manifest makes its code page UTF-8, reads it back as
// the same bytes. The hosted build (BOF3.exe, the ANSI code page) still
// receives it through the code page, as before.
std::wstring ConfigFromUtf8(const std::string& s);
std::string ConfigToUtf8(const std::wstring& w);
// An ini's cache= value as read: UTF-8 kept; bytes that are not valid UTF-8
// are an ini an earlier launcher wrote in the ANSI code page, read once as
// that code page and held (and saved) as UTF-8 from then on.
std::string ConfigCacheFromIni(const std::string& raw);

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
// `music` (ConfigCacheRoot; empty: `cache`) is where a layer's bgm part is
// looked for: under BOF3X_CACHE_DATA=0 `cache` is empty and the cache is
// still the music's. A cache's layer counts only when its manifest
// lists it (or it has none).
std::string ConfigOptPlayable(const std::wstring& game_dir, const std::string& opt, const std::string& language,
                              const std::wstring& cache = std::wstring(), const std::wstring& music = std::wstring());

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

// Whether DAT\<layer>.*.DAT is in `game_dir` (the DLL's own test,
// dat_load.cpp ReadOptLayers).
bool ConfigOptInstalled(const std::wstring& game_dir, const char* layer,
                        const std::wstring& cache = std::wstring(),    // DIV-0089: or the cache holds it
                        const std::wstring& music = std::wstring());   // its bgm part, as ConfigOptPlayable

// The kOptDefault layers installed in `game_dir`, in kOptDefault's order -
// what an empty `opt=` names.
std::vector<std::string> ConfigOptDefaultInstalled(const std::wstring& game_dir,
                                                   const std::wstring& cache = std::wstring());

// `opt` after the dialog's suites (SuitesOptEdit, launcher/suites.h; until
// 2026-10-12 the "PSP extras" boxes): `boxes` holds each layer a changed
// suite moves and whether it is now on; `defaults` is
// ConfigOptDefaultInstalled. When no box differs from what `opt` already
// plays, `opt` itself, byte for byte (an untouched dialog never rewrites the
// line). Otherwise the layers `opt` played (ConfigOptWanted's list, so an
// empty `opt=` contributes the default layers installed and keeps them on)
// with each box's change made: "none" when nothing is left, else a list - the
// suites' layers in recipes/suites.toml's order (SuiteLayers), then the
// kOptDefault layers, then any other name `opt` had, in its order. A name no
// box moves (not installed, a mod's) is kept as it was. area4-walls
// (DIV-0080) is in no suite - a repair, on by default - and keeps its state.
std::string ConfigOptEdit(const std::string& opt, const std::vector<std::string>& defaults,
                          const std::vector<std::pair<std::string, bool>>& boxes);

// Whether an ini's `opt=` value can be honoured: empty, "none", or a
// comma-separated list of distinct names of letters, digits and '-' (at most
// 16, each 1..23 characters, as the DLL's ReadOptLayers takes them), none of
// them "none" or "original". On false `why` says what is wrong; the launcher
// refuses to start rather than fall back to the default layers.
bool ConfigOptValid(const std::string& opt, std::string& why);

// DIVERGENCE DIV-0150: the music source the game is handed (BOF3X_MUSIC):
// "psx", "psp" or "pc" as music= names it; music=mp3 is "pc"; an empty music=
// or music=seq is "psp" when `opt` (the list the game is handed) names psp-music (the audio suite's PSP side
// before DIV-0150 moved the music to the Music box), else "psx".
std::string ConfigMusicSource(const std::string& music, const std::string& opt);

// Whether an ini's `psp=` value (the PSP's battle rules) can be honoured, by the DLL's
// own rule (psp_code.cpp FromEnvironment): empty (the default by the cache's
// base), "none", or a comma-separated list of distinct items of the engine's
// (recipes/suites.toml's code members: duel-loss, myria-item). On false `why`
// says what is wrong; the launcher refuses to start, as for opt=, rather than
// start a game that stops on it inside.
bool ConfigPspValid(const std::string& psp, std::string& why);

// True for "original" or a tag in kLanguages.
bool ConfigLanguageKnown(const std::string& code);

// DIV-0102 (docs/disc-only-install.md): whether "original", the port's
// Chinese text, can be played: `game_dir` is a PC install (BOF3.exe there -
// its DAT\ is the port's; `importer.py install --base` refuses such a
// directory) or `cache` (ConfigCacheDataRoot) holds loc\zh-CN, built from the
// PC's DAT\. The DLL's rule too (dat_load.cpp RefuseOriginalWithoutChinese).
bool ConfigOriginalAvailable(const std::wstring& game_dir, const std::wstring& cache);

// DIV-0102: the language on an install without the Chinese text, before
// and after the dialog. A PC install (ConfigOriginalAvailable): true, nothing
// changed. Otherwise false with `error` naming the languages held when
// BOF3X_LANG says "original" or a tag not held, when bof3x.ini's language=
// (or the dialog) names "original" or a tag not held, or when none is held;
// with neither the environment nor the ini naming one, the language becomes
// the first held in kLanguages' order (said on stderr).
bool ConfigResolveLanguage(const std::wstring& game_dir, Config& cfg, std::string& error);

// False, with `error` naming the tag to use, when the environment's
// BOF3X_LANG is a retired bare code (en, fr, de, ja; DIV-0005): the launcher
// stops on it before the game starts, as the DLL would at injection.
bool ConfigCheckEnvironmentLanguage(std::string& error);

// ---- Mods and the row above the text (docs/mods.md) ---------------------------

// A mod as <home>\mods.toml lists it (tools/mods.py writes the file; this only
// reads it). `base` is a reading-row mod's; empty for another kind.
struct ModInfo {
    std::string id, name, version, kind, base;
    std::vector<std::string> required;   // mod.toml's requires
};
// The mods <home>\mods.toml lists, in its order; none when there is no file.
std::vector<ModInfo> ConfigModsInstalled(const std::wstring& home);

// The start's rule for an ini without a ruby_ui= line (written before the
// toggle existed): the toggle comes up on when <home>\mods.toml lists a
// reading-row mod, so a player who installed the furigana mod before the box
// was hidden still sees it. A named ruby_ui= is left as it is.
void ConfigRubyUiDefault(Config& cfg, const std::wstring& home);

// ruby_key='s value as the DLL reads BOF3X_RUBY_KEY: "none" or "f1".."f12"
// (any case in, lower case out), or empty when `value` is neither.
std::string ConfigRubyKey(const std::string& value);

// The optional text layer a ruby= value names under `language` (a tag; empty or
// "original" for the port's Chinese): ruby-<ruby>-<language>, or empty when
// either is empty or the Chinese text is played.
std::string ConfigRubyLayer(const std::string& ruby, const std::string& language);

// One entry of the Settings page's "Row above the text" box: the ruby= value
// it writes ("" for none), its label, and why it cannot be played here (empty:
// it can). Every source-dependent entry is listed, greyed with its reason
// (the owner's rule, 2026-10-10).
struct RubyChoice {
    std::string id;
    std::wstring label, why;
};
// None; every installed reading-row mod (greyed unless its base is `language`
// and its layer is built); every other held language (greyed unless its
// second-language layer ruby-<tag>-<language> is built); then any other
// ruby-*-<language> layer the cache or DAT\ holds. Driven by what exists:
// <home>\mods.toml, ConfigLanguagesAvailable and the layers themselves.
std::vector<RubyChoice> ConfigRubyChoices(const std::wstring& game_dir, const std::string& language,
                                          const std::wstring& cache, const std::wstring& music,
                                          const std::wstring& home);

// The layer to add to BOF3X_OPT for cfg.ruby under `language` (what the game
// is given as BOF3X_LANG), or empty: ruby= empty, or (said on stderr, no row
// drawn) a value that is not a layer name's part, the Chinese text played, or
// the layer not built here.
std::string ConfigRubyPlayable(const std::wstring& game_dir, const std::string& ruby, const std::string& language,
                               const std::wstring& cache = std::wstring(), const std::wstring& music = std::wstring());

// ---- The presets (docs/launcher-settings.md "preset=") -------------------------
//
// A preset reads the ledger's switch fields (launcher/ledger_registry.h,
// generated from docs/DIVERGENCE.md) and only ever turns entries off, by their
// Switch: names: a code name goes into BOF3X_OFF, an ini: value overrides what
// the launcher sends for that key, an env: value is set when the environment
// has none. The rules (docs/ledger-fields-round.md section 5, "D1 as run"):
//   Recommended  nothing.
//   As shipped   every entry a player can turn off (Player yes, Default on, active).
//   Repaired     those of them whose tier is Sensible or Extension; Intent and Forced stay.
//   PlayStation  those of them whose Toward: names a release and not psx (the
//                owner's call of 2026-10-28: Toward none stays, as psx does).
// An entry whose Languages: omits the language played is skipped; a Switch:
// lang entry is the language pick's, never a preset's; Player costly is listed
// as "this build cannot"; a Default: off entry the preset would keep on
// (Repaired: Intent and Forced, PlayStation: psx) is listed as "also available".

// The preset's ini value, the box's label and the sentence under the box.
struct PresetInfo {
    Preset id;
    const char* name;      // preset=, BOF3X_PRESET
    const wchar_t* label;  // the Preset box's entry
    const wchar_t* note;   // what the preset does, under the box
};
inline constexpr PresetInfo kPresets[] = {
    {Preset::kRecommended, "recommended", L"Recommended - the changes of this project (the default)",
     L"The game uses the settings of the other pages. This preset turns off no change."},
    {Preset::kShipped, "shipped", L"As shipped - the PC version as it was released",
     L"This preset turns off all the changes that this build can turn off. Some changes stay on: the list "
     L"below shows them."},
    {Preset::kRepaired, "repaired", L"Repaired - only the repairs of defects",
     L"This preset keeps the repairs of defects. It turns off the other changes that this build can turn off: "
     L"the looks, the conveniences and the extensions."},
    {Preset::kPlayStation, "playstation", L"PlayStation - as the PlayStation version",
     L"This preset turns off the changes that make the game different from the PlayStation version. The repairs "
     L"and the other changes stay on."},
};
// The preset an ini or BOF3X_PRESET value names, or nullptr.
const PresetInfo* PresetNamed(std::string_view name);
const PresetInfo& PresetOf(Preset preset);

// What a preset does with one entry, under one language. The names are
// tools/ledger_registry.py's actions (PresetActionName), so the launcher host
// tests compare the two (tools/launcher_host/presets.tsv).
enum class PresetAction {
    kNone,             // the preset has nothing to do with the entry
    kOff,              // turned off by its Switch: names
    kSkippedLanguage,  // its Languages: omits the language played: inert there
    kLangGoverned,     // Switch: lang - the language pick's, which no preset changes
    kCannot,           // Player costly: the original is only Capcom's function
    kAlsoAvailable,    // Default off, and the preset would keep it on: the player turns it on
    kNever,            // Player never: Forced or one-way (left alone)
    kAlreadyOff,       // Default off: already off (left alone)
    kInactive,         // State withdrawn or superseded (left alone)
};
const char* PresetActionName(PresetAction action);
// The Languages: bit of a language played: "original" (or empty) or a tag of
// kLanguages; 0 for any other value, under which no listed entry acts.
std::uint8_t PresetLanguageBit(std::string_view language);
PresetAction PresetClassify(Preset preset, const ledger::Entry& entry, std::uint8_t language);

// What a preset does at launch on this install, for the language played.
struct PresetPlan {
    Preset preset = Preset::kRecommended;
    std::string language;   // "original" or a tag
    struct Item {
        const ledger::Entry* entry;
        PresetAction action;
        // kOff only: each Switch: name the launch does not apply, with why -
        // the install cannot take it ("not on this install"), or a setting of
        // the player's or the environment's decides it ("kept").
        std::vector<std::pair<std::string, std::string>> not_here, kept;
    };
    std::vector<Item> items;   // every entry the preset concerns, the ledger's order
    std::string off;           // the code names, comma-separated, distinct, the ledger's order
    std::vector<std::pair<std::string, std::string>> ini;   // key, value: what the launcher sends instead
    std::vector<std::pair<std::string, std::string>> env;   // variable, value: set where the environment has none
};
// The plan of `preset` for `language` (ConfigLanguagePlayed) on the install
// in `game_dir` with the settings `cfg`. Reads the environment (a variable it
// has is kept) and the install (the install rules: music=pc needs BGM\*.DAT;
// music=psx and psp a cache with those songs; BOF3X_CACHE_DATA=0 and
// BOF3X_EXEIMAGE_STRICT=1 a PC install, BOF3.exe in `game_dir`); an
// opt= value only overrides an empty opt= (the launcher's default list, the
// case the ledger's ini:opt=none is written for); an env: value is kept where
// an ini value of the same plan decides that variable (music=pc and
// BOF3X_MUSIC).
PresetPlan ConfigPresetPlan(const std::wstring& game_dir, const Config& cfg, Preset preset,
                            const std::string& language);
// Whether the plan overrides the ini key `key` (the dialog greys its control).
bool PresetOverrides(const PresetPlan& plan, std::string_view key);
// The settings the game is given: `cfg` with the plan's ini values, its other
// keys as read, and off= the plan's code names then the player's own list
// (BOF3X_OFF's syntax: a player's -name keeps that change on).
Config ConfigPresetApply(const Config& cfg, const PresetPlan& plan);
// BOF3X_PRESET (the engine logs it) and the plan's env values, each where the
// environment has none: before ConfigApplyEnvironment, which then keeps them.
void ConfigPresetEnvironment(const PresetPlan& plan);
// One block on stderr: the preset, the language, the counts, every switch
// applied and every one not.
void ConfigPresetReport(const PresetPlan& plan);
// The language the game is given (ConfigApplyEnvironment's rule): BOF3X_LANG
// when set, else the ini's when its overlay is built, else "original".
std::string ConfigLanguagePlayed(const std::wstring& game_dir, const Config& cfg);
// BOF3X_PRESET in the environment wins over preset=, as every variable does;
// false with `error` set when it names no preset.
bool ConfigPresetFromEnvironment(Config& cfg, std::string& error);

}  // namespace bof3x
