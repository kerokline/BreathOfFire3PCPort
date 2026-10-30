#include "launcher/config.h"

#include <windows.h>

#include "launcher/suites.h"
#include "common/home.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace bof3x {
namespace {

// Whether `pattern` (FindFirstFileW's) matches a file.
bool AnyMatch(const std::wstring& pattern) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    FindClose(h);
    return true;
}

// The cache's manifest through the one reader the DLL shares
// (common/cache_manifest.h), read again only when the file changes: the
// dialog asks for a layer many times over a manifest of some megabytes.
const CacheManifest& CachedManifest(const std::wstring& cache) {
    static std::wstring s_root;
    static FILETIME s_time{};
    static ULONGLONG s_size = ~0ull;
    static bool s_read = false;
    static CacheManifest s_man;
    WIN32_FILE_ATTRIBUTE_DATA a{};
    const bool there = !cache.empty() &&
                       GetFileAttributesExW((cache + L"\\manifest.toml").c_str(), GetFileExInfoStandard, &a);
    const ULONGLONG size = there ? (static_cast<ULONGLONG>(a.nFileSizeHigh) << 32) | a.nFileSizeLow : ~0ull;
    if (!s_read || cache != s_root || size != s_size || CompareFileTime(&a.ftLastWriteTime, &s_time) != 0) {
        s_read = true;
        s_root = cache;
        s_size = size;
        s_time = a.ftLastWriteTime;
        s_man = there ? ManifestRead(cache) : CacheManifest();
    }
    return s_man;
}

// <cache>\opt\<name>\<part> holds a .DAT (exe: exe\place.toml) - the DLL's
// own test (dat_cache::HasLayerPart), the manifest's listing included.
bool CacheHasLayerPart(const std::wstring& cache, const char* name, const wchar_t* part) {
    if (cache.empty() || !ManifestOffersLayer(CachedManifest(cache), name)) return false;
    std::wstring pattern = cache + L"\\opt\\";
    for (const char* c = name; *c; ++c) pattern += static_cast<wchar_t>(*c);
    pattern += L"\\";
    pattern += part;
    return AnyMatch(pattern + (std::wcscmp(part, L"exe") == 0 ? L"\\place.toml" : L"\\*.DAT"));
}

// DIV-0089: <cache>\<kind>\<name>\dat\ holds a .DAT - the DLL's own test
// (dat_cache::HasLayer). An opt\ layer counts only when the cache's manifest
// lists it, or the cache has no manifest (the code review of 2026-10-12: a
// build that stopped part-way left layers with no rows, which were offered and
// played unverified). `music` is the cache the music reads - BOF3X_CACHE
// whatever BOF3X_CACHE_DATA says (docs/cache-read.md 6) - where a layer's bgm
// part is asked; the other parts are asked in `cache`, empty under
// BOF3X_CACHE_DATA=0. An empty `music` is `cache`.
bool CacheHasLayer(const std::wstring& cache, const wchar_t* kind, const char* name,
                   const std::wstring& music = std::wstring()) {
    const bool opt = std::wcscmp(kind, L"opt") == 0;
    // TG (docs/launcher-suites.md 9): an optional layer of songs, sound files or
    // .data ranges - the DLL's own test (dat_cache::HasLayerPart)
    if (opt && CacheHasLayerPart(music.empty() ? cache : music, name, L"bgm")) return true;
    if (cache.empty()) return false;
    if (opt && !ManifestOffersLayer(CachedManifest(cache), name)) return false;
    std::wstring pattern = cache + L"\\" + kind + L"\\";
    for (const char* c = name; *c; ++c) pattern += static_cast<wchar_t>(*c);
    if (AnyMatch(pattern + L"\\dat\\*.DAT")) return true;
    return opt && (CacheHasLayerPart(cache, name, L"snd") || CacheHasLayerPart(cache, name, L"exe"));
}

// Whole file as bytes, or false if it cannot be opened. Absent is not an error
// to this function's callers; they all have a defined behaviour for it.
bool ReadWhole(const std::wstring& path, std::string& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    out.clear();
    char buf[4096];
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(f, buf, sizeof buf, &got, nullptr)) {
            CloseHandle(f);
            return false;
        }
        if (got == 0) break;
        out.append(buf, got);
    }
    CloseHandle(f);
    return true;
}

bool WriteWhole(const std::wstring& path, const std::string& data) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    const BOOL ok = WriteFile(f, data.data(), static_cast<DWORD>(data.size()), &wrote, nullptr);
    CloseHandle(f);
    return ok && wrote == data.size();
}

// Splits on \n and drops one trailing \r, so a CRLF file and an LF file parse
// the same. A trailing newline does not produce a final empty line - which
// matters for BOF3.CFG, where the line COUNT decides which of 0x5A9860 /
// 0x5A9880 the game runs.
std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : text) {
        if (c == '\n') {
            if (!cur.empty() && cur.back() == '\r') cur.pop_back();
            lines.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        if (cur.back() == '\r') cur.pop_back();
        lines.push_back(cur);
    }
    return lines;
}

std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

}  // namespace

namespace {
// bof3x.ini's lines into `cfg`: the file's reader (ConfigLoad below), and the
// presets' (an ini: switch is one line of it, so a preset's value is read as
// the ini's own line would be). An overload of ConfigLoad on the text, not a
// name of its own: tools/ledger_switches.py reads the ini keys the launcher
// takes from ConfigLoad's first definition.
void ConfigLoad(const std::string& text, Config& cfg, std::string& error) {
    // A file that names any key or pad binding replaces the whole default
    // list, so what the file says is what the game gets - not the defaults
    // with the file's lines appended.
    bool keys_seen = false, pad_seen = false;
    for (const std::string& raw : SplitLines(text)) {
        const std::string line = Trim(raw);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq));
        const std::string value = Trim(line.substr(eq + 1));

        if (key == "language") {
            if (ConfigLanguageKnown(value)) {
                cfg.language = value;
                cfg.language_named = true;
            } else if (const char* use = RetiredLanguageReplacement(value.c_str())) {
                // DIV-0005: retired, not mapped; the launcher stops on it.
                error = "bof3x.ini's language=" + value + " is retired; use " + use + " (DIV-0005)";
            }
        } else if (key == "ruby") {
            // the row above the text: kept as written; checked at start (ConfigRubyPlayable)
            cfg.ruby = value;
        } else if (key == "ruby_key") {
            // the study key (DIV-0155): f1..f12 or none; anything else is the default
            const std::string k = ConfigRubyKey(value);
            if (!k.empty()) cfg.ruby_key = k;
        } else if (key == "ruby_ui") {
            // whether Settings shows the Row above the text box; display only
            if (value == "0" || value == "1") {
                cfg.ruby_ui = value == "1";
                cfg.ruby_ui_named = true;
            }
        } else if (key == "opt") {
            // Kept as written, valid or not: dropping a bad value would play
            // the default layers instead, so the launcher refuses it
            // (ConfigOptValid) rather than this file guessing.
            cfg.opt = value;
        } else if (key == "psp") {  // DIV-0108: kept as written, the DLL refuses an unknown name
            cfg.psp = value;
        } else if (key == "off") {  // BOF3X_OFF: kept as written, the game refuses a name it does not read
            cfg.off = value;
        } else if (key == "preset") {  // the presets: a name of kPresets, else the default
            if (const PresetInfo* p = PresetNamed(value)) {
                cfg.preset = p->id;
            } else {
                cfg.preset = Preset::kRecommended;
                std::fprintf(stderr, "bof3x-launcher: bof3x.ini's preset=%s is not recommended, shipped, repaired or "
                             "playstation; the preset is recommended\n", value.c_str());
            }
        } else if (key == "cache") {  // DIV-0087: a path; the dll refuses one that is not a directory
            cfg.cache = ConfigCacheFromIni(value);   // UTF-8 (state 3); an older ini's ANSI bytes once
        } else if (key == "ffmpeg") {  // the importer's, for the PSP's music (docs/install-sources.md)
            cfg.ffmpeg = ConfigCacheFromIni(value);
        } else if (key == "music") {  // DIV-0087, DIV-0150
            if (value == "psx" || value == "psp" || value == "pc" || value == "seq" || value == "mp3" || value.empty())
                cfg.music = value;
        } else if (key == "filter") {
            if (value == "point") cfg.filter = Filter::kPoint;
            else if (value == "linear") cfg.filter = Filter::kLinear;
        } else if (key == "display") {
            if (value == "windowed") cfg.display = Display::kWindowed;
            else if (value == "fullscreen") cfg.display = Display::kFullscreen;
        } else if (key == "renderer") {
            if (value == "0") cfg.renderer = 0;
            else if (value == "1") cfg.renderer = 1;
        } else if (key == "show_launcher") {
            if (value == "0") cfg.show_launcher = false;
            else if (value == "1") cfg.show_launcher = true;
        } else if (key == "background") {
            if (value == "0") cfg.background = false;
            else if (value == "1") cfg.background = true;
        } else if (key == "screen") {
            // "crt" was our own CRT look (DIV-0037), withdrawn 2026-09-27 for
            // SatPixie; an ini that still asks for it gets the surviving CRT.
            if (value == "satpixie" || value == "crt") cfg.satpixie = true;
            else if (value == "clean") cfg.satpixie = false;
        } else if (key.compare(0, 9, "satpixie.") == 0) {
            const std::string name = key.substr(9);
            char* end = nullptr;
            const float v = std::strtof(value.c_str(), &end);
            if (end == value.c_str() || *end) continue;
            // The DLL stops the game on a value outside the preset's range
            // (src/render/satpixie.cpp, kKnobs), so a hand edit outside it is
            // dropped here like any other value this file does not know. The
            // ranges are kKnobs' own; NaN fails every comparison and is dropped too.
            auto in = [v](float lo, float hi) { return v >= lo && v <= hi; };
            auto& sp = cfg.sp;
            if (name == "acc_modulate") { if (in(0.0f, 1.0f)) sp.modulate = v; }
            else if (name == "gamma") { if (in(1.8f, 2.6f)) sp.gamma = v; }
            else if (name == "chroma_strength") { if (in(0.0f, 5.0f)) sp.chroma = v; }
            else if (name == "blur_x") { if (in(0.0f, 5.0f)) sp.blur_x = v; }
            else if (name == "blur_y") { if (in(0.0f, 5.0f)) sp.blur_y = v; }
            else if (name == "natural_vision") sp.natural = v > 0.5f;
            else if (name == "ghosting_on") sp.ghosting = v > 0.5f;
            else if (name == "chroma_on") sp.chroma_on = v > 0.5f;
            else if (name == "vignette_on") sp.vignette = v > 0.5f;
            else if (name == "vignette_aspect") sp.vignette_43 = v > 0.5f;
            else if (name == "wiggle_toggle") sp.wiggle = v > 0.5f;
            else if (name == "scanroll") sp.scanroll = v > 0.5f;
            else if (name == "overscan_crop") sp.overscan = v > 0.5f;
            else if (name == "shadow_mask") { if (in(0.0f, 2.0f)) sp.mask = static_cast<int>(v + 0.5f); }
        } else if (key == "text_speed") {
            if (value == "0") cfg.text_faster = false;
            else if (value == "1") cfg.text_faster = true;
        } else if (key == "unlearned_blue") {
            if (value == "0") cfg.unlearned_blue = false;
            else if (value == "1") cfg.unlearned_blue = true;
        } else if (key == "snap") {
            if (value == "0") cfg.snap = false;
            else if (value == "1") cfg.snap = true;
        } else if (key == "wide") {
            if (value == "0") cfg.wide = 0;
            else if (value == "1") cfg.wide = 1;
            else if (value == "2") cfg.wide = 2;
        } else if (key == "cheat.exp" || key == "cheat.zenny") {
            char* end = nullptr;
            const long v = std::strtol(value.c_str(), &end, 10);
            // DIV-0045: 0..10 since 2026-10-03 (it was 0..50). A larger number,
            // as an ini saved before then may hold, is kept as written and
            // goes out as it is; the DLL clamps it to 10 and logs that
            // (src/game/cheats.cpp), and the Cheats dialog shows and saves 10.
            if (end == value.c_str() || *end || v < 0 || v > 1000000) continue;
            (key == "cheat.exp" ? cfg.cheats.exp : cfg.cheats.zenny) = static_cast<int>(v);
        } else if (key == "cheat.steal") {
            if (value == "0") cfg.cheats.steal = false;
            else if (value == "1") cfg.cheats.steal = true;
        } else if (key == "cheat.enemy_info") {
            if (value == "0") cfg.cheats.enemy_info = false;
            else if (value == "1") cfg.cheats.enemy_info = true;
        } else if (key == "cheat.no_fail") {
            if (value == "0") cfg.cheats.no_fail = false;
            else if (value == "1") cfg.cheats.no_fail = true;
        } else if (key.compare(0, 4, "key.") == 0) {
            if (!keys_seen) cfg.bindings.keys.clear();
            keys_seen = true;
            input::ParseKeys(key.substr(4) + "=" + value, ',', cfg.bindings.keys);
        } else if (key.compare(0, 4, "pad.") == 0) {
            if (!pad_seen) cfg.bindings.pad.clear();
            pad_seen = true;
            input::ParsePad(key.substr(4) + "=" + value, ',', cfg.bindings.pad, cfg.bindings.layout);
        } else if (key == "scale") {
            // D395: 2..8 is named (BOF3X_SCALE sent, 2 too); empty follows the display
            if (value.size() == 1 && value[0] >= '2' && value[0] <= '8') {
                cfg.scale = value[0] - '0';
                cfg.scale_named = true;
            } else if (value.empty()) {
                cfg.scale = 2;
                cfg.scale_named = false;
            }
        }
        // An unknown key is left alone rather than rejected: a newer build's
        // file must not stop an older launcher.
    }
}
}  // namespace

bool ConfigLoad(const std::wstring& path, Config& cfg, std::string& error) {
    error.clear();
    std::string text;
    if (!ReadWhole(path, text)) return false;
    ConfigLoad(text, cfg, error);
    return true;
}

bool ConfigSave(const std::wstring& path, const Config& cfg) {
    std::string out;
    out += "# bof3x-launcher settings.\r\n";
    out += "# Rewritten whenever the launcher's dialog is used; hand-editing works too.\r\n";
    out += "# Run bof3x-launcher --config to reopen the dialog after hiding it.\r\n";
    out += "\r\n[bof3x]\r\n";
    out += "# original | en-US | en-150 | fr-FR | de-DE | ja-JP   (a tag needs tools/loc_build.py to have built it)\r\n";
    // Only a language someone chose (the ini or the dialog, DIV-0102): the
    // Sources page saves before any is chosen, and a written default
    // "original" would then be refused on the disc-only install it builds.
    if (cfg.language_named || cfg.language != kLanguageOriginal)
        out += std::string("language=") + cfg.language + "\r\n";
    else
        out += "# language= not chosen yet: the default (the port's text, else the first language the install holds)\r\n";
    out += "# the row above the text (docs/mods.md): empty none | a reading-row mod's id (furigana) | another\r\n";
    out += "# held language's tag (en-US): the layer ruby-<this>-<language>, added to the layers when built\r\n";
    out += "ruby=" + cfg.ruby + "\r\n";
    out += "# the key that hides and shows the row above the text in play (DIV-0155): f1..f12, or none to\r\n";
    out += "# leave every key free; f4 by default\r\n";
    out += "ruby_key=" + cfg.ruby_key + "\r\n";
    out += "# 1: Settings shows the Row above the text box (Mods, Extra; turned on when a reading-row mod is\r\n";
    out += "# added). Display only: ruby= above is given to the game either way\r\n";
    out += std::string("ruby_ui=") + (cfg.ruby_ui ? "1" : "0") + "\r\n";
    out += "# 1: Msg Speed's Med. plays at Fast and Fast prints each page at once (DIV-0158, Mods, Extras)\r\n";
    out += "# | 0 the original's three speeds\r\n";
    out += std::string("text_speed=") + (cfg.text_faster ? "1" : "0") + "\r\n";
    out += "# 1: in battle, an enemy's skill the party can learn and does not know has its name in blue\r\n";
    out += "# (DIV-0179, Mods, Extras) | 0 white, the original's\r\n";
    out += std::string("unlearned_blue=") + (cfg.unlearned_blue ? "1" : "0") + "\r\n";
    out += "# optional layers, comma-separated, in the order they land (DIV-0086, docs/opt-layers.md): psp-art,\r\n";
    out += "# psp-tiles, psp-maps, psp-names-en-150, psp-names-ja-JP, area4-walls (DIV-0080, from a Western PSX disc);\r\n";
    out += "# tools/importer.py install puts them in the game's DAT folder. Empty: area4-walls when installed;\r\n";
    out += "# none: no layer. The dialog's three suites (graphics, audio, data: recipes/suites.toml) write their\r\n";
    out += "# layers here; a suite is its layers, so this line is the whole record of them\r\n";
    out += "opt=" + cfg.opt + "\r\n";
    out += "# the PSP's battle rules (DIV-0108, docs/psp-battle-changes.md): duel-loss, myria-item, or none;\r\n";
    out += "# empty: on when the cache's base is the PSP's, else off. The data suite writes them here\r\n";
    out += "psp=" + cfg.psp + "\r\n";
    out += "# the changes to turn off: code names from the ledger's Switch: lines, comma-separated (BOF3X_OFF,\r\n";
    out += "# docs/launcher-settings.md). * turns off all code names. -name keeps that change on. Empty: none.\r\n";
    out += "# If a name is not a code name, the game stops and shows a message\r\n";
    out += "off=" + cfg.off + "\r\n";
    out += "# the preset (docs/launcher-settings.md): recommended (the default, this project's changes) | shipped\r\n";
    out += "# (the PC version as it was released) | repaired (only the repairs of defects) | playstation (as the\r\n";
    out += "# PlayStation version). It turns off changes when the game starts. It does not change this file\r\n";
    out += std::string("preset=") + PresetOf(cfg.preset).name + "\r\n";
    out += "# the importer's cache folder; its base\\bgm songs play through the sequencer (DIV-0087, docs/music-seq-engine.md)\r\n";
    out += "cache=" + cfg.cache + "\r\n";
    out += "# ffmpeg.exe for the importer (the PSP's music); empty: BOF3X_FFMPEG or PATH (docs/install-sources.md)\r\n";
    out += "ffmpeg=" + cfg.ffmpeg + "\r\n";
    out += "# seq (the cache's song where it has one, the default) | mp3 (the PC's files always)\r\n";
    out += "music=" + cfg.music + "\r\n";
    out += "# linear (the port's own) | point (DIV-0012)\r\n";
    out += std::string("filter=") + (cfg.filter == Filter::kPoint ? "point" : "linear") + "\r\n";
    out += "# clean | satpixie (the SatPixie CRT, DIV-0043)\r\n";
    out += std::string("screen=") + (cfg.satpixie ? "satpixie" : "clean") + "\r\n";
    out += "# the SatPixie look's parameters, the preset's names (BOF3X_SATPIXIE)\r\n";
    out += SatpixieLine(cfg.sp, "satpixie.", "\r\n");
    out += "# fullscreen (a borderless window, DIV-0032) | windowed   -> line 1 of the game's BOF3.CFG\r\n";
    out += std::string("display=") + (cfg.display == Display::kWindowed ? "windowed" : "fullscreen") +
           "\r\n";
    out += "# 1 the picture at whole multiples of its size (DIV-0042) | 0 stretched to the window's height\r\n";
    out += std::string("snap=") + (cfg.snap ? "1" : "0") + "\r\n";
    out += "# 2..8: the first window's size, 320 x 240 times this, until the game saves its own (bof3x.window).\r\n";
    out += "# Empty: the size follows the display's scale (2 at 100 %, 3 at 150 %, 4 at 200 %)\r\n";
    out += "scale=" + (cfg.scale_named ? std::to_string(cfg.scale) : std::string()) + "\r\n";
    out += "# 1 the 16:9 picture, 426 x 240 | 2 the 21:9 picture, 560 x 240 (DIV-0041) | 0 the original's 320 x 240\r\n";
    out += "wide=" + std::to_string(cfg.wide) + "\r\n";
    out += "# 1 keeps the game running while its window is not in front (DIV-0033) | 0 the original's freeze\r\n";
    out += std::string("background=") + (cfg.background ? "1" : "0") + "\r\n";
    out += "# 1 (shipped default) | 0 -> line 2 of the game's BOF3.CFG\r\n";
    out += std::string("renderer=") + (cfg.renderer ? "1" : "0") + "\r\n";
    out += "# cheats (docs/cheats.md): EXP and zenny won in battle times 0..10, 1 the original's (DIV-0045);\r\n";
    out += "# steal 1 makes Pilfer and Steal take an item whenever the enemy carries one (DIV-0046);\r\n";
    out += "# enemy_info 1 shows every enemy's HP gauge and Identify stats, bosses too (DIV-0187);\r\n";
    out += "# no_fail 1 makes the minigames impossible to fail (DIV-0188; the rope pull today)\r\n";
    out += "cheat.exp=" + std::to_string(cfg.cheats.exp) + "\r\n";
    out += "cheat.zenny=" + std::to_string(cfg.cheats.zenny) + "\r\n";
    out += std::string("cheat.steal=") + (cfg.cheats.steal ? "1" : "0") + "\r\n";
    out += std::string("cheat.enemy_info=") + (cfg.cheats.enemy_info ? "1" : "0") + "\r\n";
    out += std::string("cheat.no_fail=") + (cfg.cheats.no_fail ? "1" : "0") + "\r\n";
    out += "# the keyboard: key.KEY=action (docs/controls.md; actions up down left right cross circle square\r\n";
    out += "# triangle l1 l2 r1 r2 start select, joined with +); at most 32 lines. Replaces the game's own table\r\n";
    out += "# (BOF3.CFG lines 3+) when it differs from the default, which is that table.\r\n";
    out += input::FormatKeys(cfg.bindings.keys, "key.", "\r\n");
    out += "# the pad: pad.INPUT=action, inputs south east west north lb rb lt rt start back guide ls rs\r\n";
    out += "# dpad_up/down/left/right ls_up/down/left/right rs_up/down/left/right; pad.layout positional |\r\n";
    out += "# nintendo (the face pairs swapped) | auto (the pad's own labels)\r\n";
    out += input::FormatPad(cfg.bindings.pad, cfg.bindings.layout, "pad.", "\r\n");
    out += "# 0 hides this launcher's dialog and starts the game straight away\r\n";
    out += std::string("show_launcher=") + (cfg.show_launcher ? "1" : "0") + "\r\n";
    return WriteWhole(path, out);
}

std::string SatpixieLine(const Config::Satpixie& sp, const char* prefix, const char* sep) {
    char buf[64];
    std::string out;
    auto num = [&](const char* name, float v) {
        snprintf(buf, sizeof buf, "%s%s=%g%s", prefix, name, static_cast<double>(v), sep);
        out += buf;
    };
    num("acc_modulate", sp.modulate);
    num("gamma", sp.gamma);
    num("chroma_strength", sp.chroma);
    num("blur_x", sp.blur_x);
    num("blur_y", sp.blur_y);
    num("natural_vision", sp.natural ? 1.0f : 0.0f);
    num("ghosting_on", sp.ghosting ? 1.0f : 0.0f);
    num("chroma_on", sp.chroma_on ? 1.0f : 0.0f);
    num("vignette_on", sp.vignette ? 1.0f : 0.0f);
    num("vignette_aspect", sp.vignette_43 ? 1.0f : 0.0f);
    num("wiggle_toggle", sp.wiggle ? 1.0f : 0.0f);
    num("scanroll", sp.scanroll ? 1.0f : 0.0f);
    num("overscan_crop", sp.overscan ? 1.0f : 0.0f);
    num("shadow_mask", static_cast<float>(sp.mask));
    return out;
}

void ConfigApplyEnvironment(const std::wstring& game_dir, const Config& cfg, bool hosted) {
    wchar_t existing[64];
    // DIV-0089: where the DLL will find layers besides DAT\ (before BOF3X_CACHE
    // is set below; the environment's wins over the ini's, as there), and
    // where the music finds a layer's songs (under BOF3X_CACHE_DATA=0 too).
    const std::wstring cache = ConfigCacheDataRoot(cfg), music = ConfigCacheRoot(cfg);

    // Only a language whose overlay is in DAT\ - the dialog offers no other,
    // but a hand-edited or carried-over bof3x.ini can name one never built,
    // and the dll would then lay the shipped text out for an overlay that is
    // not there (the Latin layouts of DIV-0058..0061 over the Chinese).
    if (GetEnvironmentVariableW(L"BOF3X_LANG", existing, 64) == 0 &&
        cfg.language != kLanguageOriginal) {
        const std::vector<std::string> built = ConfigLanguagesAvailable(game_dir, cache);
        if (std::find(built.begin(), built.end(), cfg.language) != built.end())
            SetEnvironmentVariableA("BOF3X_LANG", cfg.language.c_str());
        else
            std::fprintf(stderr, "bof3x-launcher: bof3x.ini asks for language %s, but DAT\\%s.* is not there; "
                         "playing the original text\n", cfg.language.c_str(), cfg.language.c_str());
    }

    // DIV-0086: the layers installed and, for a text layer, of the language
    // played - the one the game is given now, not the ini's: the step above
    // can leave BOF3X_LANG unset, and the environment may already say
    // `original` or another tag, and the DLL refuses a text layer of any
    // other language at start-up. An empty opt= means the default layers
    // installed (kOptDefault, DIV-0080's walls), opt=none none.
    // The row above the text (docs/mods.md 5): ruby= is one more layer, after
    // the ini's, when it is built for the language played.
    if (GetEnvironmentVariableW(L"BOF3X_OPT", existing, 64) == 0) {
        char played[8];  // as the DLL reads it (dat_load.cpp g_lang): longer is none
        const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", played, sizeof played);
        const std::string language = (n == 0 || n >= sizeof played) ? std::string() : std::string(played, n);
        std::string wanted = ConfigOptWanted(game_dir, cfg.opt, cache);
        const std::string ruby = ConfigRubyPlayable(game_dir, cfg.ruby, language, cache, music);
        if (!ruby.empty()) {
            size_t count = wanted.empty() ? 0 : 1, at = 0;
            bool listed = false;
            while (at <= wanted.size() && !wanted.empty()) {
                const size_t end = std::min(wanted.find(',', at), wanted.size());
                listed = listed || wanted.compare(at, end - at, ruby) == 0;
                if (end == wanted.size()) break;
                ++count;
                at = end + 1;
            }
            if (listed) {
                // opt= names it already: once is enough
            } else if (count >= 16) {   // dat_load.cpp kOptMax
                std::fprintf(stderr, "bof3x-launcher: opt= already names 16 layers, the most the game takes; the row "
                             "above the text (%s) is left off\n", ruby.c_str());
            } else {
                wanted += (wanted.empty() ? "" : ",") + ruby;
            }
        }
        if (!wanted.empty()) {
            std::string playable = ConfigOptPlayable(game_dir, wanted, language, cache, music);
            if (hosted) {
                // The hosted build refuses a layer with an exe part at
                // start (dat_load.cpp ReadOptLayers); the launcher knows it is
                // starting it, so such a layer - the data suite's PSP side
                // names one - is left out here rather than stopping every
                // shadow test and A/B run on an ini that chose that suite.
                std::vector<std::string> dropped;
                playable = ConfigOptHosted(playable, cache, dropped);
                std::string names;
                for (const std::string& d : dropped) names += (names.empty() ? "" : ", ") + d;
                if (!dropped.empty())
                    std::fprintf(stderr, "bof3x-launcher: the hosted build runs Capcom's .data as loaded, so it cannot "
                                 "take %s (an exe part: .data ranges only bof3x.exe lays); left out of BOF3X_OPT "
                                 "for this run\n", names.c_str());
            }
            if (!playable.empty()) SetEnvironmentVariableA("BOF3X_OPT", playable.c_str());
        }
    }

    else if (!cfg.ruby.empty()) {
        std::fprintf(stderr, "bof3x-launcher: BOF3X_OPT is set in the environment, so bof3x.ini's ruby=%s is not "
                     "added to it\n", cfg.ruby.c_str());
    }

    // DIV-0155: the reading row's study key, as the ini has it (f4 unless it
    // says otherwise); the DLL reads it whether or not a reading-row layer is
    // played, and an environment's value wins, as for the others.
    if (GetEnvironmentVariableW(L"BOF3X_RUBY_KEY", existing, 64) == 0)
        SetEnvironmentVariableA("BOF3X_RUBY_KEY", cfg.ruby_key.c_str());

    // DIV-0108: the PSP's battle rules as the ini lists them; empty leaves
    // the DLL its default (the cache's base_policy). The DLL checks the names.
    if (GetEnvironmentVariableW(L"BOF3X_PSP", existing, 64) == 0 && !cfg.psp.empty())
        SetEnvironmentVariableA("BOF3X_PSP", cfg.psp.c_str());

    // The player's switch: the ini's off= as BOF3X_OFF, for both hosts (the
    // game checks the names: detour.cpp InjectReport). An environment's
    // BOF3X_OFF wins, as for the others; empty sends nothing.
    if (GetEnvironmentVariableW(L"BOF3X_OFF", nullptr, 0) == 0 && !cfg.off.empty())
        SetEnvironmentVariableA("BOF3X_OFF", cfg.off.c_str());

    // DIV-0087: the cache root and the music source, as the ini has them; the
    // dll checks both.
    // Neither names one: the home's own cache, when the Sources page built it
    // (ConfigCacheRoot).
    if (GetEnvironmentVariableW(L"BOF3X_CACHE", existing, 64) == 0) {
        if (!cfg.cache.empty())
            SetEnvironmentVariableW(L"BOF3X_CACHE", ConfigFromUtf8(cfg.cache).c_str());   // the character, not the bytes
        else if (const std::wstring home = HomeCacheDefault(); !home.empty())
            SetEnvironmentVariableW(L"BOF3X_CACHE", home.c_str());
    }
    // DIV-0150: always a source, the ini's or the default's by the layers the
    // game is handed (ConfigMusicSource; BOF3X_OPT is set above)
    if (GetEnvironmentVariableW(L"BOF3X_MUSIC", existing, 64) == 0) {
        char opt[512];
        const DWORD n = GetEnvironmentVariableA("BOF3X_OPT", opt, sizeof opt);
        SetEnvironmentVariableA("BOF3X_MUSIC",
                                ConfigMusicSource(cfg.music, n > 0 && n < sizeof opt ? opt : cfg.opt.c_str()).c_str());
    }

    if (GetEnvironmentVariableW(L"BOF3X_FILTER", existing, 64) == 0 &&
        cfg.filter == Filter::kPoint)
        SetEnvironmentVariableW(L"BOF3X_FILTER", L"point");

    if (GetEnvironmentVariableW(L"BOF3X_BACKGROUND", existing, 64) == 0 && !cfg.background)
        SetEnvironmentVariableW(L"BOF3X_BACKGROUND", L"0");

    if (GetEnvironmentVariableW(L"BOF3X_PRESENT", existing, 64) == 0 && cfg.satpixie)
        SetEnvironmentVariableW(L"BOF3X_PRESENT", L"satpixie");

    if (cfg.satpixie && GetEnvironmentVariableW(L"BOF3X_SATPIXIE", existing, 64) == 0) {
        std::string line = SatpixieLine(cfg.sp, "", ",");
        if (!line.empty()) line.pop_back();   // the trailing comma
        SetEnvironmentVariableA("BOF3X_SATPIXIE", line.c_str());
    }

    if (GetEnvironmentVariableW(L"BOF3X_TEXT_SPEED", existing, 64) == 0 && cfg.text_faster)
        SetEnvironmentVariableW(L"BOF3X_TEXT_SPEED", L"1");

    if (GetEnvironmentVariableW(L"BOF3X_UNLEARNED_BLUE", existing, 64) == 0 && cfg.unlearned_blue)
        SetEnvironmentVariableW(L"BOF3X_UNLEARNED_BLUE", L"1");

    if (GetEnvironmentVariableW(L"BOF3X_SNAP", existing, 64) == 0 && !cfg.snap)
        SetEnvironmentVariableW(L"BOF3X_SNAP", L"0");

    if (GetEnvironmentVariableW(L"BOF3X_WIDE", existing, 64) == 0 && (cfg.wide == 1 || cfg.wide == 2))
        SetEnvironmentVariableW(L"BOF3X_WIDE", cfg.wide == 2 ? L"2" : L"1");

    // D395: whenever the ini named scale=, 2 included; unnamed, the game follows the display
    if (GetEnvironmentVariableW(L"BOF3X_SCALE", existing, 64) == 0 && cfg.scale_named) {
        const wchar_t k[2] = {static_cast<wchar_t>(L'0' + cfg.scale), 0};
        SetEnvironmentVariableW(L"BOF3X_SCALE", k);
    }

    if (GetEnvironmentVariableW(L"BOF3X_EXP", existing, 64) == 0 && cfg.cheats.exp != 1)
        SetEnvironmentVariableA("BOF3X_EXP", std::to_string(cfg.cheats.exp).c_str());
    if (GetEnvironmentVariableW(L"BOF3X_ZENNY", existing, 64) == 0 && cfg.cheats.zenny != 1)
        SetEnvironmentVariableA("BOF3X_ZENNY", std::to_string(cfg.cheats.zenny).c_str());
    if (GetEnvironmentVariableW(L"BOF3X_STEAL", existing, 64) == 0 && cfg.cheats.steal)
        SetEnvironmentVariableW(L"BOF3X_STEAL", L"1");
    if (GetEnvironmentVariableW(L"BOF3X_ENEMY_INFO", existing, 64) == 0 && cfg.cheats.enemy_info)
        SetEnvironmentVariableW(L"BOF3X_ENEMY_INFO", L"1");
    if (GetEnvironmentVariableW(L"BOF3X_NO_FAIL", existing, 64) == 0 && cfg.cheats.no_fail)
        SetEnvironmentVariableW(L"BOF3X_NO_FAIL", L"1");

    const input::Bindings defaults = input::Bindings::Defaults();
    const bool keys_default = cfg.bindings.keys.size() == defaults.keys.size() &&
        [&] { for (size_t i = 0; i < defaults.keys.size(); ++i)
                  if (cfg.bindings.keys[i].dik != defaults.keys[i].dik || cfg.bindings.keys[i].bits != defaults.keys[i].bits) return false;
              return true; }();
    if (GetEnvironmentVariableW(L"BOF3X_KEYS", existing, 64) == 0 && !keys_default) {
        std::string line = input::FormatKeys(cfg.bindings.keys, "", ",");
        if (!line.empty()) line.pop_back();
        SetEnvironmentVariableA("BOF3X_KEYS", line.c_str());
    }
    const bool pad_default = cfg.bindings.layout == defaults.layout && cfg.bindings.pad.size() == defaults.pad.size() &&
        [&] { for (size_t i = 0; i < defaults.pad.size(); ++i)
                  if (cfg.bindings.pad[i].input != defaults.pad[i].input || cfg.bindings.pad[i].bits != defaults.pad[i].bits) return false;
              return true; }();
    if (GetEnvironmentVariableW(L"BOF3X_PAD", existing, 64) == 0 && !pad_default) {
        std::string line = input::FormatPad(cfg.bindings.pad, cfg.bindings.layout, "", ",");
        if (!line.empty()) line.pop_back();
        SetEnvironmentVariableA("BOF3X_PAD", line.c_str());
    }

    // None is set for its default value: an unset variable is exactly what
    // the DLL treats as "the original's behaviour", and leaving it unset keeps
    // a default run byte-identical to one launched without a settings file.
}

void ConfigSeedFromGameCfg(const std::wstring& game_dir, Config& cfg) {
    std::string text;
    if (!ReadWhole(game_dir + L"\\BOF3.CFG", text)) return;
    const std::vector<std::string> lines = SplitLines(text);

    // Cfg_Load 0x4FD030 runs each line through atoi, so anything that is not a
    // number reads as 0 there. We only adopt values we recognise; a line we do
    // not understand keeps the default rather than guessing at the game's.
    if (lines.size() >= 1) {
        const std::string l = Trim(lines[0]);
        if (l == "0") cfg.display = Display::kWindowed;
        else if (l == "1") cfg.display = Display::kFullscreen;
    }
    if (lines.size() >= 2) {
        const std::string l = Trim(lines[1]);
        if (l == "0") cfg.renderer = 0;
        else if (l == "1") cfg.renderer = 1;
    }
}

bool ConfigApplyGameCfg(const std::wstring& game_dir, const Config& cfg, std::wstring& error) {
    const std::wstring path = game_dir + L"\\BOF3.CFG";
    const std::string want_line1 = cfg.display == Display::kWindowed ? "0" : "1";
    const std::string want_line2 = cfg.renderer ? "1" : "0";

    std::string text;
    const bool exists = ReadWhole(path, text);
    if (!exists && want_line1 == "1" && want_line2 == "1") return true;   // absent == 1,1

    std::vector<std::string> lines = exists ? SplitLines(text) : std::vector<std::string>{};
    while (lines.size() < 2) lines.push_back("");
    lines[0] = want_line1;
    lines[1] = want_line2;
    // Lines 3+ are the unread integer pairs 0x5A9860 consumes; they pass
    // through untouched, and none is added.

    std::string out;
    for (const std::string& l : lines) out += l + "\r\n";
    if (exists && out == text) return true;   // nothing to say, so nothing written

    if (!WriteWhole(path, out)) {
        error = L"cannot write " + path;
        return false;
    }
    return true;
}

std::wstring ConfigCacheDataRoot(const Config& cfg) {
    wchar_t text[4];
    const DWORD m = GetEnvironmentVariableW(L"BOF3X_CACHE_DATA", text, 4);
    if (m == 1 && text[0] == L'0') return std::wstring();  // the cache for the music only
    return ConfigCacheRoot(cfg);
}

std::wstring ConfigCacheRoot(const Config& cfg) {
    wchar_t root[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_CACHE", root, MAX_PATH);
    std::wstring out;
    if (n > 0 && n < MAX_PATH) out.assign(root, n);
    else if (n == 0 && !cfg.cache.empty()) {
        // Config::cache is UTF-8 (ConfigCacheFromIni), as the launcher hands
        // it to the game wide - not the ANSI code page, which could not hold
        // every folder name (docs/code-page-audit.md section 4).
        out = ConfigFromUtf8(cfg.cache);
    } else if (n == 0) {
        // Neither names one: the home's own cache, when the Sources page has
        // built it (common/home.h, docs/install-sources.md). Never written to
        // the ini, so a moved install folder still finds its own.
        out = HomeCacheDefault();
    }
    while (out.size() > 1 && (out.back() == L'\\' || out.back() == L'/')) out.pop_back();
    return out;
}

std::wstring ConfigFromUtf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string ConfigToUtf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n > 0 ? n : 0), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::string ConfigCacheFromIni(const std::string& raw) {
    if (raw.empty()) return raw;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, raw.data(), static_cast<int>(raw.size()), nullptr, 0) > 0)
        return raw;   // valid UTF-8 (ASCII included): as written
    const int n = MultiByteToWideChar(CP_ACP, 0, raw.data(), static_cast<int>(raw.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_ACP, 0, raw.data(), static_cast<int>(raw.size()), w.data(), n);
    return ConfigToUtf8(w);
}

std::vector<std::string> ConfigLanguagesAvailable(const std::wstring& game_dir, const std::wstring& cache) {
    std::vector<std::string> out;
    for (const LanguageInfo& lang : kLanguages) {
        std::wstring pattern = game_dir + L"\\DAT\\";
        for (const char* c = lang.code; *c; ++c) pattern += static_cast<wchar_t>(*c);
        pattern += L".*";
        if (!AnyMatch(pattern) && !CacheHasLayer(cache, L"loc", lang.code)) continue;  // DIV-0089
        out.push_back(lang.code);
    }
    return out;
}

std::string ConfigOptPlayable(const std::wstring& game_dir, const std::string& opt, const std::string& language,
                              const std::wstring& cache, const std::wstring& music) {
    std::string out;
    size_t at = 0;
    while (at <= opt.size()) {
        size_t end = opt.find(',', at);
        if (end == std::string::npos) end = opt.size();
        const std::string layer = opt.substr(at, end - at);
        at = end + 1;
        if (layer.empty()) continue;
        std::wstring pattern = game_dir + L"\\DAT\\";
        for (char c : layer) pattern += static_cast<wchar_t>(c);
        pattern += L".*";
        if (!AnyMatch(pattern) && !CacheHasLayer(cache, L"opt", layer.c_str(), music)) {  // DIV-0089: or the cache's
            std::fprintf(stderr, "bof3x-launcher: bof3x.ini asks for layer %s, but DAT\\%s.* is not there%s\n",
                         layer.c_str(), layer.c_str(),
                         !cache.empty()   ? ", nor in the cache's opt\\ as its manifest lists it"
                         : !music.empty() ? " (BOF3X_CACHE_DATA=0: of the cache's layers only a music part is read)"
                                          : "");
            continue;
        }
        // The DLL's own test (dat_load.cpp ReadOptLayers), so what passes
        // here never stops the game at start-up.
        const char* tag = LayerLanguage(layer.c_str());
        if (tag && !SamePrimaryLanguage(language.c_str(), tag)) {
            std::fprintf(stderr, "bof3x-launcher: layer %s is %s text, not the language played (%s); left off\n",
                         layer.c_str(), tag, language.empty() ? kLanguageOriginal : language.c_str());
            continue;
        }
        if (!out.empty()) out += ',';
        out += layer;
    }
    return out;
}

bool ConfigOptInstalled(const std::wstring& game_dir, const char* layer, const std::wstring& cache,
                        const std::wstring& music) {
    std::wstring pattern = game_dir + L"\\DAT\\";
    for (const char* c = layer; *c; ++c) pattern += static_cast<wchar_t>(*c);
    pattern += L".*.DAT";   // the DLL's own pattern (dat_load.cpp ReadOptLayers)
    return AnyMatch(pattern) || CacheHasLayer(cache, L"opt", layer, music);  // DIV-0089: or the cache's
}

std::vector<std::string> ConfigOptDefaultInstalled(const std::wstring& game_dir, const std::wstring& cache) {
    std::vector<std::string> out;
    for (const char* layer : kOptDefault)
        if (ConfigOptInstalled(game_dir, layer, cache)) out.push_back(layer);
    return out;
}

std::string ConfigOptWanted(const std::wstring& game_dir, const std::string& opt, const std::wstring& cache) {
    if (opt == "none") return std::string();
    if (!opt.empty()) return opt;
    std::string out;
    for (const std::string& layer : ConfigOptDefaultInstalled(game_dir, cache)) {
        std::fprintf(stderr, "bof3x-launcher: layer %s is installed and on by default; opt=none in bof3x.ini "
                     "turns it off\n", layer.c_str());
        if (!out.empty()) out += ',';
        out += layer;
    }
    return out;
}

std::string ConfigOptEdit(const std::string& opt, const std::vector<std::string>& defaults,
                          const std::vector<std::pair<std::string, bool>>& boxes) {
    // What `opt` plays, as ConfigOptWanted reads it.
    std::vector<std::string> played;
    if (opt.empty()) {
        played = defaults;
    } else if (opt != "none") {
        size_t at = 0;
        while (at <= opt.size()) {
            size_t end = opt.find(',', at);
            if (end == std::string::npos) end = opt.size();
            played.push_back(opt.substr(at, end - at));
            at = end + 1;
        }
    }
    auto has = [&](const std::string& n) { return std::find(played.begin(), played.end(), n) != played.end(); };
    bool changed = false;
    for (const auto& [name, on] : boxes) {
        if (on == has(name)) continue;
        changed = true;
        if (on) played.push_back(name);
        else played.erase(std::find(played.begin(), played.end(), name));
    }
    if (!changed) return opt;
    if (played.empty()) return "none";
    // The suites' order, then kOptDefault's, then the rest as `opt` had them.
    std::vector<std::string> order = SuiteLayers();
    for (const char* l : kOptDefault) order.push_back(l);
    std::string out;
    auto put = [&](const std::string& n) {
        if (!out.empty()) out += ',';
        out += n;
    };
    for (const std::string& n : order)
        if (has(n)) put(n);
    for (const std::string& n : played)
        if (std::find(order.begin(), order.end(), n) == order.end()) put(n);
    return out;
}

std::string ConfigMusicSource(const std::string& music, const std::string& opt) {
    if (music == "psx" || music == "psp" || music == "pc") return music;
    if (music == "mp3") return "pc";
    std::size_t at = 0;
    while (at <= opt.size()) {
        const std::size_t comma = opt.find(',', at);
        const std::size_t end = comma == std::string::npos ? opt.size() : comma;
        if (opt.compare(at, end - at, "psp-music") == 0) return "psp";
        at = end + 1;
    }
    return "psx";
}

bool ConfigOptValid(const std::string& opt, std::string& why) {
    if (opt.empty() || opt == "none") return true;
    std::vector<std::string> names;
    size_t at = 0;
    for (;;) {
        const size_t end = std::min(opt.find(',', at), opt.size());
        const std::string name = opt.substr(at, end - at);
        if (name.empty()) {
            why = "an empty layer name (two commas, or one at an end)";
            return false;
        }
        if (name.size() > 23) {   // dat_load.cpp kOptName - 1
            why = "layer \"" + name + "\" is longer than 23 characters";
            return false;
        }
        if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") !=
            std::string::npos) {
            why = "layer \"" + name + "\" has a character other than a letter, digit or -";
            return false;
        }
        if (name == "none" || name == "original") {
            why = "\"" + name + "\" in a list; opt=none alone turns every layer off";
            return false;
        }
        if (std::find(names.begin(), names.end(), name) != names.end()) {
            why = "layer " + name + " named twice";
            return false;
        }
        names.push_back(name);
        if (end == opt.size()) break;
        at = end + 1;
    }
    if (names.size() > 16) {   // dat_load.cpp kOptMax (8 until the suites' engine half, 2026-10-12)
        why = "more than 16 layers";
        return false;
    }
    return true;
}

std::string ConfigOptHosted(const std::string& list, const std::wstring& cache, std::vector<std::string>& dropped) {
    std::string out;
    size_t at = 0;
    while (at <= list.size()) {
        size_t end = list.find(',', at);
        if (end == std::string::npos) end = list.size();
        const std::string layer = list.substr(at, end - at);
        at = end + 1;
        if (layer.empty()) continue;
        bool refused = false;
        for (const char* part : kHostedRefusesParts) {
            // the recipe's part kind (recipes/psp-layers.toml), then the cache's
            // tree as the DLL tests it (a layer no recipe of ours names)
            for (const suites_gen::LayerPart& lp : suites_gen::kLayerParts)
                refused = refused || (layer == lp.layer && std::strcmp(lp.part, part) == 0);
            std::wstring wpart;
            for (const char* c = part; *c; ++c) wpart += static_cast<wchar_t>(*c);
            refused = refused || CacheHasLayerPart(cache, layer.c_str(), wpart.c_str());
        }
        if (refused) {
            dropped.push_back(layer);
            continue;
        }
        if (!out.empty()) out += ',';
        out += layer;
    }
    return out;
}

bool ConfigPspValid(const std::string& psp, std::string& why) {
    if (psp.empty() || psp == "none") return true;
    if (psp.size() > 127) {   // psp_code.cpp FromEnvironment's buffer
        why = "longer than 127 characters";
        return false;
    }
    std::vector<std::string> known, seen;
    for (const suites_gen::MemberInfo& m : suites_gen::kMembers)
        if (m.code) known.push_back(m.code);
    std::string items;
    for (const std::string& k : known) items += (items.empty() ? "" : ", ") + k;
    size_t at = 0;
    for (;;) {
        const size_t end = std::min(psp.find(',', at), psp.size());
        const std::string name = psp.substr(at, end - at);
        if (std::find(known.begin(), known.end(), name) == known.end()) {
            why = name.empty() ? std::string("an empty item (two commas, or one at an end)")
                               : "\"" + name + "\" is not one of the PSP's changes (" + items + ")";
            return false;
        }
        if (std::find(seen.begin(), seen.end(), name) != seen.end()) {
            why = name + " named twice";
            return false;
        }
        seen.push_back(name);
        if (end == psp.size()) break;
        at = end + 1;
    }
    return true;
}

bool ConfigLanguageKnown(const std::string& code) {
    if (code == kLanguageOriginal) return true;
    for (const LanguageInfo& lang : kLanguages)
        if (code == lang.code) return true;
    return false;
}

bool ConfigOriginalAvailable(const std::wstring& game_dir, const std::wstring& cache) {
    return AnyMatch(game_dir + L"\\BOF3.exe") || CacheHasLayer(cache, L"loc", "zh-CN");
}

bool ConfigResolveLanguage(const std::wstring& game_dir, Config& cfg, std::string& error) {
    const std::wstring cache = ConfigCacheDataRoot(cfg);
    if (ConfigOriginalAvailable(game_dir, cache)) return true;
    const std::vector<std::string> held = ConfigLanguagesAvailable(game_dir, cache);
    std::string list;
    for (const std::string& tag : held) list += (list.empty() ? "" : ", ") + tag;
    const std::string why =
        "This is a disc-only install: no BOF3.exe in the game directory and no loc\\zh-CN layer in the cache" +
        std::string(cache.empty() ? " (no cache= or BOF3X_CACHE)" : "") +
        ", so the port's Chinese text is not there and the game would play without text (DIV-0102).\n\n" +
        (held.empty() ? std::string("It holds no language layer: build one (tools/importer.py build --lang <tag>, "
                                    "install --lang <tag>).")
                      : "The languages it holds: " + list + ".");
    char lang[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", lang, sizeof lang);
    if (n > 0) {
        if (n >= sizeof lang || std::string(lang) == kLanguageOriginal) {
            error = std::string("BOF3X_LANG=") + (n >= sizeof lang ? "(too long, read as original)" : lang) +
                    " asks for the port's Chinese text.\n\n" + why;
            return false;
        }
        if (std::find(held.begin(), held.end(), lang) != held.end()) return true;
        error = std::string("BOF3X_LANG=") + lang + " is not built here.\n\n" + why;
        return false;
    }
    if (cfg.language_named) {
        if (std::find(held.begin(), held.end(), cfg.language) != held.end()) return true;
        error = "bof3x.ini's language=" + cfg.language +
                (cfg.language == kLanguageOriginal ? " asks for the port's Chinese text.\n\n" : " is not built here.\n\n") +
                why;
        return false;
    }
    if (held.empty()) {
        error = why;
        return false;
    }
    cfg.language = held.front();
    std::fprintf(stderr, "bof3x-launcher: a disc-only install, no Chinese text: language %s, the first it holds "
                 "(DIV-0102)\n", cfg.language.c_str());
    return true;
}

// ---- Mods and the row above the text (docs/mods.md) ---------------------------

namespace {

// A value of mods.toml's, as tools/mods.py writes it (json.dumps): a quoted
// string with JSON's escapes. Empty when it is not one.
std::string TomlString(const std::string& v, size_t& at) {
    std::string out;
    if (at >= v.size() || v[at] != '"') return out;
    for (++at; at < v.size() && v[at] != '"'; ++at) {
        if (v[at] != '\\' || at + 1 >= v.size()) {
            out += v[at];
            continue;
        }
        const char e = v[++at];
        if (e == 'n') out += '\n';
        else if (e == 't') out += '\t';
        else if (e == 'u' && at + 4 < v.size()) {
            const unsigned cp = static_cast<unsigned>(std::strtoul(v.substr(at + 1, 4).c_str(), nullptr, 16));
            at += 4;
            const std::wstring w(1, static_cast<wchar_t>(cp));
            out += ConfigToUtf8(w);
        } else {
            out += e;   // \" \\ \/
        }
    }
    if (at < v.size()) ++at;   // the closing quote
    return out;
}

bool IsTag(const std::string& s) {
    for (const LanguageInfo& lang : kLanguages)
        if (s == lang.code) return true;
    return false;
}

std::wstring Wide(const std::string& s) { return ConfigFromUtf8(s); }

}  // namespace

std::vector<ModInfo> ConfigModsInstalled(const std::wstring& home) {
    std::vector<ModInfo> out;
    std::string text;
    if (home.empty() || !ReadWhole(home + L"\\mods.toml", text)) return out;
    for (const std::string& raw : SplitLines(text)) {
        const std::string line = Trim(raw);
        if (line == "[[mod]]") {
            out.emplace_back();
            continue;
        }
        const size_t eq = line.find('=');
        if (out.empty() || line.empty() || line[0] == '#' || eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
        ModInfo& m = out.back();
        size_t at = 0;
        if (key == "requires" && !value.empty() && value[0] == '[') {
            at = 1;
            while (at < value.size()) {
                while (at < value.size() && (value[at] == ' ' || value[at] == ',')) ++at;
                if (at >= value.size() || value[at] != '"') break;
                m.required.push_back(TomlString(value, at));
            }
            continue;
        }
        const std::string v = TomlString(value, at);
        if (key == "id") m.id = v;
        else if (key == "name") m.name = v;
        else if (key == "version") m.version = v;
        else if (key == "kind") m.kind = v;
        else if (key == "base") m.base = v;
    }
    out.erase(std::remove_if(out.begin(), out.end(), [](const ModInfo& m) { return m.id.empty(); }), out.end());
    return out;
}

std::string ConfigRubyKey(const std::string& value) {
    std::string k;
    for (char c : value) k += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    if (k == "none") return k;
    // f1..f12 exactly as input_script.cpp's key names spell them: no leading zero, no sign
    static const char* const kNames[] = {"f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12"};
    for (const char* n : kNames)
        if (k == n) return k;
    return std::string();
}

void ConfigRubyUiDefault(Config& cfg, const std::wstring& home) {
    if (cfg.ruby_ui_named) return;
    for (const ModInfo& m : ConfigModsInstalled(home))
        if (m.kind == "reading-row") {
            cfg.ruby_ui = true;
            return;
        }
}

std::string ConfigRubyLayer(const std::string& ruby, const std::string& language) {
    if (ruby.empty() || language.empty() || language == kLanguageOriginal) return std::string();
    return "ruby-" + ruby + "-" + language;
}

std::vector<RubyChoice> ConfigRubyChoices(const std::wstring& game_dir, const std::string& language,
                                          const std::wstring& cache, const std::wstring& music,
                                          const std::wstring& home) {
    std::vector<RubyChoice> out;
    out.push_back({std::string(), L"None", std::wstring()});
    const bool chinese = language.empty() || language == kLanguageOriginal;
    const std::vector<std::string> held = ConfigLanguagesAvailable(game_dir, cache);
    auto built = [&](const std::string& id) {
        const std::string layer = ConfigRubyLayer(id, language);
        return !layer.empty() && ConfigOptInstalled(game_dir, layer.c_str(), cache, music);
    };
    for (const ModInfo& m : ConfigModsInstalled(home)) {
        if (m.kind != "reading-row") continue;
        RubyChoice c{m.id, Wide(m.name) + L" (mod " + Wide(m.version) + L")", std::wstring()};
        std::string lack;
        for (const std::string& t : m.required)
            if (std::find(held.begin(), held.end(), t) == held.end()) lack += (lack.empty() ? "" : ", ") + t;
        if (!lack.empty()) c.why = L"needs " + Wide(lack) + L" text: add that disc on Data, Sources";
        else if (chinese || m.base != language) c.why = L"only with " + Wide(m.base) + L" text: select that language";
        else if (!built(m.id)) c.why = L"not built: build the install on Data, Sources";
        out.push_back(c);
    }
    for (const std::string& tag : held) {
        if (tag == language) continue;
        RubyChoice c{tag, std::wstring(), std::wstring()};
        for (const LanguageInfo& lang : kLanguages)
            if (tag == lang.code) c.label = lang.label;
        if (chinese) c.why = L"not with Chinese text: select a different language";
        else if (!built(tag)) c.why = L"not built in this install";
        out.push_back(c);
    }
    // Any other ruby-<x>-<language> layer there is (a layer of a release newer
    // than this launcher, or one built by hand): offered under its own name.
    if (!chinese) {
        const std::string suffix = "-" + language;
        std::vector<std::string> found;
        WIN32_FIND_DATAW fd;
        const std::wstring patterns[] = {cache.empty() ? std::wstring() : cache + L"\\opt\\ruby-*",
                                         game_dir + L"\\DAT\\ruby-*.DAT"};
        for (const std::wstring& pattern : patterns) {
            if (pattern.empty()) continue;
            HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) continue;
            do {
                std::string name = ConfigToUtf8(fd.cFileName);
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) name = name.substr(0, name.find('.'));
                if (name.size() > 5 + suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
                    found.push_back(name.substr(5, name.size() - 5 - suffix.size()));
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        std::sort(found.begin(), found.end());
        found.erase(std::unique(found.begin(), found.end()), found.end());
        for (const std::string& id : found) {
            if (std::any_of(out.begin(), out.end(), [&](const RubyChoice& c) { return c.id == id; }) || !built(id))
                continue;
            out.push_back({id, Wide(id) + L" (layer " + Wide(ConfigRubyLayer(id, language)) + L")", std::wstring()});
        }
    }
    return out;
}

std::string ConfigRubyPlayable(const std::wstring& game_dir, const std::string& ruby, const std::string& language,
                               const std::wstring& cache, const std::wstring& music) {
    if (ruby.empty()) return std::string();
    if (ruby.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != std::string::npos ||
        ruby.front() == '-' || ruby.back() == '-') {
        std::fprintf(stderr, "bof3x-launcher: bof3x.ini's ruby=%s is not a mod's id or a language tag; no row above "
                     "the text\n", ruby.c_str());
        return std::string();
    }
    const std::string layer = ConfigRubyLayer(ruby, language);
    if (layer.empty()) {
        std::fprintf(stderr, "bof3x-launcher: bof3x.ini asks for a row above the text (ruby=%s), but the Chinese "
                     "text is played; no row above it\n", ruby.c_str());
        return std::string();
    }
    if (layer.size() > 23 || !ConfigOptInstalled(game_dir, layer.c_str(), cache, music)) {   // dat_load.cpp kOptName - 1
        std::fprintf(stderr, "bof3x-launcher: bof3x.ini asks for the row above the text ruby=%s, but layer %s is not "
                     "there%s; playing without one\n", ruby.c_str(), layer.c_str(),
                     IsTag(ruby) ? " (the second-language layer is not built)" : " (no such mod built for this language)");
        return std::string();
    }
    return layer;
}

bool ConfigCheckEnvironmentLanguage(std::string& error) {
    char lang[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", lang, sizeof lang);
    if (n == 0 || n >= sizeof lang) return true;
    if (const char* use = RetiredLanguageReplacement(lang)) {
        error = std::string("BOF3X_LANG=") + lang + " is retired; use " + use + " (DIV-0005)";
        return false;
    }
    return true;
}

// ---- The presets (config.h; docs/launcher-settings.md "preset=") ---------------

const PresetInfo* PresetNamed(std::string_view name) {
    for (const PresetInfo& p : kPresets)
        if (name == p.name) return &p;
    return nullptr;
}

const PresetInfo& PresetOf(Preset preset) {
    for (const PresetInfo& p : kPresets)
        if (p.id == preset) return p;
    return kPresets[0];   // unreachable: kPresets holds every Preset
}

const char* PresetActionName(PresetAction action) {
    switch (action) {
    case PresetAction::kNone: return "none";
    case PresetAction::kOff: return "off";
    case PresetAction::kSkippedLanguage: return "skipped-language";
    case PresetAction::kLangGoverned: return "lang-governed";
    case PresetAction::kCannot: return "cannot";
    case PresetAction::kAlsoAvailable: return "also-available";
    case PresetAction::kNever: return "never";
    case PresetAction::kAlreadyOff: return "already-off";
    case PresetAction::kInactive: return "inactive";
    }
    return "none";
}

std::uint8_t PresetLanguageBit(std::string_view language) {
    if (language.empty() || language == kLanguageOriginal) return ledger::kLangOriginal;
    constexpr std::pair<std::string_view, std::uint8_t> kBits[] = {
        {"en-US", ledger::kLangEnUs}, {"en-150", ledger::kLangEn150}, {"fr-FR", ledger::kLangFrFr},
        {"de-DE", ledger::kLangDeDe}, {"ja-JP", ledger::kLangJaJp}};
    static_assert(std::size(kBits) == std::size(kLanguages), "a language without its Languages: bit");
    for (const auto& [tag, bit] : kBits)
        if (language == tag) return bit;
    return 0;
}

namespace {

// The preset's own wish, before the entry's fields say whether it can have it
// (tools/ledger_registry.py wants_off, wants_on).
bool PresetWantsOff(Preset preset, const ledger::Entry& e) {
    switch (preset) {
    case Preset::kRecommended: return false;
    case Preset::kShipped: return true;
    case Preset::kRepaired: return e.tier == ledger::Tier::Sensible || e.tier == ledger::Tier::Extension;
    case Preset::kPlayStation:
        // the owner's call of 2026-10-28: Toward none is a generic repair, tied
        // to no release, and stays; an entry naming a release and not psx goes
        return e.toward != 0 && (e.toward & ledger::kTowardPsx) == 0;
    }
    return false;
}

bool PresetWantsOn(Preset preset, const ledger::Entry& e) {
    if (preset == Preset::kRepaired) return e.tier == ledger::Tier::Intent || e.tier == ledger::Tier::Forced;
    if (preset == Preset::kPlayStation) return (e.toward & ledger::kTowardPsx) != 0;
    return false;
}

bool IsDir(const std::wstring& d) {
    const DWORD a = GetFileAttributesW(d.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

// Why this install cannot take an ini: value, or empty when it can. The Music
// box's own test (config_dialog.cpp MusicRefresh) for music=; any other key
// can be set on any install.
std::string IniNotHere(const std::wstring& game_dir, const Config& cfg, const std::string& key,
                       const std::string& value) {
    if (key != "music") return std::string();
    const std::wstring cache = ConfigCacheRoot(cfg), data = ConfigCacheDataRoot(cfg);
    const bool psp_base = ManifestPspBase(CachedManifest(cache));
    const bool songs = !cache.empty() && IsDir(cache + L"\\base\\bgm");
    if ((value == "pc" || value == "mp3") && !AnyMatch(game_dir + L"\\BGM\\*.DAT"))
        return "no BGM\\ folder with the PC's MP3 music in the game folder";
    if (value == "psx" && !(songs && !psp_base)) return "the cache has no PlayStation music";
    if (value == "psp" && !(psp_base ? songs : ConfigOptInstalled(game_dir, "psp-music", data, cache)))
        return "the cache has no PSP music";
    return std::string();
}

// Why this install cannot take an env: value, or empty when it can: the two
// that stop a disc-built install (no BOF3.exe), whose DAT\ is not the PC's
// and whose .data has gaps (DIV-0089, DIV-0101).
std::string EnvNotHere(const std::wstring& game_dir, const std::string& name, const std::string& value) {
    const bool pc = AnyMatch(game_dir + L"\\BOF3.exe");
    if (name == "BOF3X_CACHE_DATA" && value == "0" && !pc)
        return "a disc-built install (no BOF3.exe) has no PC DAT folder to read in place of the cache";
    if (name == "BOF3X_EXEIMAGE_STRICT" && value != "0" && !pc)
        return "a disc-built install (no BOF3.exe) has gaps in its .data, which the strict switch refuses";
    return std::string();
}

// The variable the launcher sends for an ini key (ConfigApplyEnvironment).
const char* IniVariable(std::string_view key) {
    constexpr std::pair<std::string_view, const char*> kFeeds[] = {
        {"language", "BOF3X_LANG"},       {"opt", "BOF3X_OPT"},       {"ruby", "BOF3X_OPT"},
        {"ruby_key", "BOF3X_RUBY_KEY"},   {"psp", "BOF3X_PSP"},       {"off", "BOF3X_OFF"},
        {"cache", "BOF3X_CACHE"},         {"music", "BOF3X_MUSIC"},   {"filter", "BOF3X_FILTER"},
        {"background", "BOF3X_BACKGROUND"}, {"screen", "BOF3X_PRESENT"}, {"text_speed", "BOF3X_TEXT_SPEED"},
        {"unlearned_blue", "BOF3X_UNLEARNED_BLUE"}, {"snap", "BOF3X_SNAP"}, {"wide", "BOF3X_WIDE"},
        {"scale", "BOF3X_SCALE"},         {"cheat.exp", "BOF3X_EXP"}, {"cheat.zenny", "BOF3X_ZENNY"},
        {"cheat.steal", "BOF3X_STEAL"},   {"cheat.enemy_info", "BOF3X_ENEMY_INFO"},
        {"cheat.no_fail", "BOF3X_NO_FAIL"}};
    for (const auto& [k, v] : kFeeds)
        if (key == k) return v;
    return nullptr;
}

bool EnvironmentHas(const std::string& name) { return GetEnvironmentVariableA(name.c_str(), nullptr, 0) > 0; }

void AddOnce(std::vector<std::pair<std::string, std::string>>& list, const std::string& k, const std::string& v) {
    for (const auto& kv : list)
        if (kv.first == k) return;
    list.emplace_back(k, v);
}

}  // namespace

PresetAction PresetClassify(Preset preset, const ledger::Entry& e, std::uint8_t language) {
    const bool off = PresetWantsOff(preset, e), on = PresetWantsOn(preset, e);
    if (!off && !(on && !e.default_on)) return PresetAction::kNone;
    if (e.state != ledger::State::Active) return PresetAction::kInactive;
    if (!ledger::ActsUnder(e, language)) return PresetAction::kSkippedLanguage;
    if (!off) return PresetAction::kAlsoAvailable;
    if (!e.default_on) return PresetAction::kAlreadyOff;
    for (const ledger::Switch* s = ledger::SwitchesBegin(e); s != ledger::SwitchesEnd(e); ++s)
        if (s->form == ledger::SwitchForm::Lang) return PresetAction::kLangGoverned;
    if (e.player == ledger::Player::Never) return PresetAction::kNever;
    if (e.player == ledger::Player::Costly) return PresetAction::kCannot;
    return PresetAction::kOff;   // Player yes (and the retired cheap, read as yes, as the tool does)
}

PresetPlan ConfigPresetPlan(const std::wstring& game_dir, const Config& cfg, Preset preset,
                            const std::string& language) {
    PresetPlan plan;
    plan.preset = preset;
    plan.language = language.empty() ? std::string(kLanguageOriginal) : language;
    const std::uint8_t bit = PresetLanguageBit(plan.language);
    std::vector<std::string> names;
    for (const ledger::Entry& e : ledger::kEntries) {
        const PresetAction action = PresetClassify(preset, e, bit);
        if (action == PresetAction::kNone) continue;
        PresetPlan::Item item{&e, action, {}, {}};
        if (action == PresetAction::kOff) {
            for (const ledger::Switch* s = ledger::SwitchesBegin(e); s != ledger::SwitchesEnd(e); ++s) {
                const std::string key(ledger::Key(*s)), value(ledger::Value(*s)), text(s->text);
                if (s->form == ledger::SwitchForm::Name) {
                    if (std::find(names.begin(), names.end(), key) == names.end()) names.push_back(key);
                } else if (s->form == ledger::SwitchForm::Ini) {
                    if (const std::string why = IniNotHere(game_dir, cfg, key, value); !why.empty())
                        item.not_here.emplace_back(text, why);
                    else if (key == "opt" && !cfg.opt.empty() && cfg.opt != value)
                        item.kept.emplace_back(text, "bof3x.ini's opt= names its own list of layers");
                    else
                        AddOnce(plan.ini, key, value);
                }
            }
        }
        plan.items.push_back(std::move(item));
    }
    // The env: values after every ini: value, so that an ini value of this plan
    // decides its own variable (music=pc and BOF3X_MUSIC=seq: the PC's MP3s).
    for (PresetPlan::Item& item : plan.items) {
        if (item.action != PresetAction::kOff) continue;
        for (const ledger::Switch* s = ledger::SwitchesBegin(*item.entry); s != ledger::SwitchesEnd(*item.entry); ++s) {
            if (s->form != ledger::SwitchForm::Env) continue;
            const std::string name(ledger::Key(*s)), value(ledger::Value(*s)), text(s->text);
            std::string by_ini;
            for (const auto& [k, v] : plan.ini)
                if (const char* var = IniVariable(k); var && name == var) by_ini = k + "=" + v;
            if (EnvironmentHas(name))
                item.kept.emplace_back(text, "the environment sets " + name);
            else if (const std::string why = EnvNotHere(game_dir, name, value); !why.empty())
                item.not_here.emplace_back(text, why);
            else if (!by_ini.empty())
                item.kept.emplace_back(text, "the preset's " + by_ini + " decides " + name);
            else if (!value.empty())   // an empty value is unset, which the environment already is
                AddOnce(plan.env, name, value);
        }
    }
    for (const std::string& n : names) plan.off += (plan.off.empty() ? "" : ",") + n;
    return plan;
}

bool PresetOverrides(const PresetPlan& plan, std::string_view key) {
    for (const auto& kv : plan.ini)
        if (kv.first == key) return true;
    return false;
}

Config ConfigPresetApply(const Config& cfg, const PresetPlan& plan) {
    Config out = cfg;
    for (const auto& [key, value] : plan.ini) {
        std::string error;
        ConfigLoad(key + "=" + value + "\n", out, error);   // read as the ini's own line would be
    }
    if (!plan.off.empty()) out.off = plan.off + (cfg.off.empty() ? "" : "," + cfg.off);
    return out;
}

void ConfigPresetEnvironment(const PresetPlan& plan) {
    if (!EnvironmentHas("BOF3X_PRESET")) SetEnvironmentVariableA("BOF3X_PRESET", PresetOf(plan.preset).name);
    for (const auto& [name, value] : plan.env)
        if (!EnvironmentHas(name)) SetEnvironmentVariableA(name.c_str(), value.c_str());
}

void ConfigPresetReport(const PresetPlan& plan) {
    int count[9] = {};
    for (const PresetPlan::Item& item : plan.items) ++count[static_cast<int>(item.action)];
    int not_here = 0, kept = 0;
    for (const PresetPlan::Item& item : plan.items) {
        not_here += item.not_here.empty() ? 0 : 1;
        kept += item.kept.empty() ? 0 : 1;
    }
    std::fprintf(stderr, "bof3x-launcher: preset %s, language %s: %d off, %d this build cannot turn off, %d not "
                 "under this language, %d follow the language, %d also available; %d with a switch not on this "
                 "install, %d with a switch kept\n",
                 PresetOf(plan.preset).name, plan.language.c_str(), count[static_cast<int>(PresetAction::kOff)],
                 count[static_cast<int>(PresetAction::kCannot)],
                 count[static_cast<int>(PresetAction::kSkippedLanguage)],
                 count[static_cast<int>(PresetAction::kLangGoverned)],
                 count[static_cast<int>(PresetAction::kAlsoAvailable)], not_here, kept);
    if (!plan.off.empty()) std::fprintf(stderr, "bof3x-launcher: preset: code names to BOF3X_OFF: %s\n", plan.off.c_str());
    std::string line;
    for (const auto& [k, v] : plan.ini) line += (line.empty() ? "" : ", ") + k + "=" + v;
    if (!line.empty()) std::fprintf(stderr, "bof3x-launcher: preset: the game gets %s in place of bof3x.ini's\n", line.c_str());
    line.clear();
    for (const auto& [k, v] : plan.env) line += (line.empty() ? "" : ", ") + k + "=" + v;
    if (!line.empty()) std::fprintf(stderr, "bof3x-launcher: preset: set where the environment has none: %s\n", line.c_str());
    for (const PresetPlan::Item& item : plan.items) {
        for (const auto& [sw, why] : item.not_here)
            std::fprintf(stderr, "bof3x-launcher: preset: DIV-%04u %s not on this install: %s\n",
                         static_cast<unsigned>(item.entry->id), sw.c_str(), why.c_str());
        for (const auto& [sw, why] : item.kept)
            std::fprintf(stderr, "bof3x-launcher: preset: DIV-%04u %s kept: %s\n", static_cast<unsigned>(item.entry->id),
                         sw.c_str(), why.c_str());
    }
}

std::string ConfigLanguagePlayed(const std::wstring& game_dir, const Config& cfg) {
    char lang[8];   // as the DLL reads it (dat_load.cpp g_lang): longer is none
    const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", lang, sizeof lang);
    if (n > 0) return n >= sizeof lang || std::strcmp(lang, kLanguageOriginal) == 0 ? kLanguageOriginal : lang;
    if (cfg.language != kLanguageOriginal) {
        const std::vector<std::string> built = ConfigLanguagesAvailable(game_dir, ConfigCacheDataRoot(cfg));
        if (std::find(built.begin(), built.end(), cfg.language) != built.end()) return cfg.language;
    }
    return kLanguageOriginal;
}

bool ConfigPresetFromEnvironment(Config& cfg, std::string& error) {
    char name[32];
    const DWORD n = GetEnvironmentVariableA("BOF3X_PRESET", name, sizeof name);
    if (n == 0) return true;
    const PresetInfo* p = n < sizeof name ? PresetNamed(name) : nullptr;
    if (!p) {
        error = std::string("BOF3X_PRESET=") + (n < sizeof name ? name : "(too long)") +
                " is not a preset. Use recommended, shipped, repaired or playstation (docs/launcher-settings.md).";
        return false;
    }
    if (p->id != cfg.preset)
        std::fprintf(stderr, "bof3x-launcher: BOF3X_PRESET=%s in the environment, in place of bof3x.ini's preset=%s\n",
                     p->name, PresetOf(cfg.preset).name);
    cfg.preset = p->id;
    return true;
}

}  // namespace bof3x
