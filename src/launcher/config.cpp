#include "launcher/config.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace bof3x {
namespace {

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

bool ConfigLoad(const std::wstring& path, Config& cfg) {
    std::string text;
    if (!ReadWhole(path, text)) return false;

    for (const std::string& raw : SplitLines(text)) {
        const std::string line = Trim(raw);
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq));
        const std::string value = Trim(line.substr(eq + 1));

        if (key == "language") {
            if (value == "en") cfg.language = Language::kEnglish;
            else if (value == "original") cfg.language = Language::kOriginal;
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
            if (value == "crt") cfg.crt = true, cfg.satpixie = false;
            else if (value == "satpixie") cfg.satpixie = true, cfg.crt = false;
            else if (value == "clean") cfg.crt = false, cfg.satpixie = false;
        } else if (key.compare(0, 9, "satpixie.") == 0) {
            const std::string name = key.substr(9);
            char* end = nullptr;
            const float v = std::strtof(value.c_str(), &end);
            if (end == value.c_str() || *end) continue;
            auto& sp = cfg.sp;
            if (name == "acc_modulate") sp.modulate = v;
            else if (name == "gamma") sp.gamma = v;
            else if (name == "chroma_strength") sp.chroma = v;
            else if (name == "blur_x") sp.blur_x = v;
            else if (name == "blur_y") sp.blur_y = v;
            else if (name == "natural_vision") sp.natural = v > 0.5f;
            else if (name == "ghosting_on") sp.ghosting = v > 0.5f;
            else if (name == "chroma_on") sp.chroma_on = v > 0.5f;
            else if (name == "vignette_on") sp.vignette = v > 0.5f;
            else if (name == "vignette_aspect") sp.vignette_43 = v > 0.5f;
            else if (name == "wiggle_toggle") sp.wiggle = v > 0.5f;
            else if (name == "scanroll") sp.scanroll = v > 0.5f;
            else if (name == "overscan_crop") sp.overscan = v > 0.5f;
            else if (name == "shadow_mask") sp.mask = static_cast<int>(v + 0.5f);
        } else if (key == "snap") {
            if (value == "0") cfg.snap = false;
            else if (value == "1") cfg.snap = true;
        } else if (key == "wide") {
            if (value == "0") cfg.wide = false;
            else if (value == "1") cfg.wide = true;
        } else if (key == "scale") {
            if (value.size() == 1 && value[0] >= '2' && value[0] <= '8') cfg.scale = value[0] - '0';
        }
        // An unknown key is left alone rather than rejected: a newer build's
        // file must not stop an older launcher.
    }
    return true;
}

bool ConfigSave(const std::wstring& path, const Config& cfg) {
    std::string out;
    out += "# bof3x-launcher settings.\r\n";
    out += "# Rewritten whenever the launcher's dialog is used; hand-editing works too.\r\n";
    out += "# Run bof3x-launcher --config to reopen the dialog after hiding it.\r\n";
    out += "\r\n[bof3x]\r\n";
    out += "# original | en   (en needs tools/loc_build.py to have been run)\r\n";
    out += std::string("language=") + (cfg.language == Language::kEnglish ? "en" : "original") +
           "\r\n";
    out += "# linear (the port's own) | point (DIV-0012)\r\n";
    out += std::string("filter=") + (cfg.filter == Filter::kPoint ? "point" : "linear") + "\r\n";
    out += "# clean | crt (our scanlines and glow, DIV-0037) | satpixie (the SatPixie CRT, DIV-0043)\r\n";
    out += std::string("screen=") + (cfg.satpixie ? "satpixie" : cfg.crt ? "crt" : "clean") + "\r\n";
    out += "# the SatPixie look's parameters, the preset's names (BOF3X_SATPIXIE)\r\n";
    out += SatpixieLine(cfg.sp, "satpixie.", "\r\n");
    out += "# fullscreen (a borderless window, DIV-0032) | windowed   -> line 1 of the game's BOF3.CFG\r\n";
    out += std::string("display=") + (cfg.display == Display::kWindowed ? "windowed" : "fullscreen") +
           "\r\n";
    out += "# 1 the picture at whole multiples of its size (DIV-0042) | 0 stretched to the window's height\r\n";
    out += std::string("snap=") + (cfg.snap ? "1" : "0") + "\r\n";
    out += "# 2..8: the first window's size, 320 x 240 times this, until the game saves its own (bof3x.window)\r\n";
    out += "scale=" + std::to_string(cfg.scale) + "\r\n";
    out += "# 1 the wide picture, 426 x 240 (DIV-0041, survey build) | 0 the original's 320 x 240\r\n";
    out += std::string("wide=") + (cfg.wide ? "1" : "0") + "\r\n";
    out += "# 1 keeps the game running while its window is not in front (DIV-0033) | 0 the original's freeze\r\n";
    out += std::string("background=") + (cfg.background ? "1" : "0") + "\r\n";
    out += "# 1 (shipped default) | 0 -> line 2 of the game's BOF3.CFG\r\n";
    out += std::string("renderer=") + (cfg.renderer ? "1" : "0") + "\r\n";
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

void ConfigApplyEnvironment(const Config& cfg) {
    wchar_t existing[64];

    if (GetEnvironmentVariableW(L"BOF3X_LANG", existing, 64) == 0 &&
        cfg.language == Language::kEnglish)
        SetEnvironmentVariableW(L"BOF3X_LANG", L"en");

    if (GetEnvironmentVariableW(L"BOF3X_FILTER", existing, 64) == 0 &&
        cfg.filter == Filter::kPoint)
        SetEnvironmentVariableW(L"BOF3X_FILTER", L"point");

    if (GetEnvironmentVariableW(L"BOF3X_BACKGROUND", existing, 64) == 0 && !cfg.background)
        SetEnvironmentVariableW(L"BOF3X_BACKGROUND", L"0");

    if (GetEnvironmentVariableW(L"BOF3X_PRESENT", existing, 64) == 0 && (cfg.crt || cfg.satpixie))
        SetEnvironmentVariableW(L"BOF3X_PRESENT", cfg.satpixie ? L"satpixie" : L"crt");

    if (cfg.satpixie && GetEnvironmentVariableW(L"BOF3X_SATPIXIE", existing, 64) == 0) {
        std::string line = SatpixieLine(cfg.sp, "", ",");
        if (!line.empty()) line.pop_back();   // the trailing comma
        SetEnvironmentVariableA("BOF3X_SATPIXIE", line.c_str());
    }

    if (GetEnvironmentVariableW(L"BOF3X_SNAP", existing, 64) == 0 && !cfg.snap)
        SetEnvironmentVariableW(L"BOF3X_SNAP", L"0");

    if (GetEnvironmentVariableW(L"BOF3X_WIDE", existing, 64) == 0 && cfg.wide)
        SetEnvironmentVariableW(L"BOF3X_WIDE", L"1");

    if (GetEnvironmentVariableW(L"BOF3X_SCALE", existing, 64) == 0 && cfg.scale != 2) {
        const wchar_t k[2] = {static_cast<wchar_t>(L'0' + cfg.scale), 0};
        SetEnvironmentVariableW(L"BOF3X_SCALE", k);
    }

    // Neither is set for its default value: an unset variable is exactly what
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

bool ConfigEnglishAvailable(const std::wstring& game_dir) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((game_dir + L"\\DAT\\en.*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    FindClose(h);
    return true;
}

}  // namespace bof3x
