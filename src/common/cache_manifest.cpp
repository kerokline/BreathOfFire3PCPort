#include "common/cache_manifest.h"

#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

namespace bof3x {
namespace {

std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return std::string();
    return s.substr(b, s.find_last_not_of(" \t\r") - b + 1);
}

// Every basic string ("...", JSON escapes as toml_str writes them) on `line`,
// in order. \" and \\ are unescaped; any other escape is kept as written, which
// is enough for build ids, layer names and paths.
std::vector<std::string> Strings(const std::string& line) {
    std::vector<std::string> out;
    size_t i = 0;
    while ((i = line.find('"', i)) != std::string::npos) {
        std::string s;
        size_t j = i + 1;
        for (; j < line.size() && line[j] != '"'; ++j) {
            if (line[j] == '\\' && j + 1 < line.size()) {
                const char e = line[++j];
                if (e != '"' && e != '\\') s += '\\';
                s += e;
            } else {
                s += line[j];
            }
        }
        if (j >= line.size()) break;   // unterminated: not a string
        out.push_back(s);
        i = j + 1;
    }
    return out;
}

// `value` as exactly one string, or false.
bool OneString(const std::string& value, std::string& out) {
    if (value.size() < 2 || value.front() != '"') return false;
    const std::vector<std::string> s = Strings(value);
    if (s.size() != 1) return false;
    out = s[0];
    return true;
}

// `value` as a one-line array of strings, or false.
bool StringList(const std::string& value, std::vector<std::string>& out) {
    if (value.size() < 2 || value.front() != '[' || value.back() != ']') return false;
    out = Strings(value);
    // Nothing but strings, commas and blanks between the brackets.
    bool in = false;
    for (size_t i = 1; i + 1 < value.size(); ++i) {
        const char c = value[i];
        if (c == '"' && value[i - 1] != '\\') in = !in;
        else if (!in && c != ',' && c != ' ' && c != '\t') return false;
    }
    return !in;
}

bool Contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

}  // namespace

CacheManifest ManifestParse(const std::string& text) {
    CacheManifest m;
    m.present = true;
    std::string table, array_key, exe_build;
    bool in_array = false, tables_given = false;
    auto list_layer = [&m](const std::string& name) {
        if (!name.empty() && !Contains(m.opt_layers, name)) m.opt_layers.push_back(name);
    };
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = Trim(text.substr(at, end - at));
        at = end + 1;
        if (in_array) {
            if (line == "]") {
                in_array = false;
            } else if (table == "cache" && array_key == "bgm") {
                // ["base/bgm/000.DAT", "psx-us:seq:BGM000.EMI#1", "<sha256>"]
                const std::vector<std::string> s = Strings(line);
                if (m.bgm_build.empty() && s.size() >= 2) m.bgm_build = s[1].substr(0, s[1].find(':'));
            } else if (table == "cache" && array_key == "opt") {
                // ["AREA004.DAT", "opt/area4-walls", 0, "psx-us:copy:...", "<sha256>"]
                const std::vector<std::string> s = Strings(line);
                if (s.size() >= 2 && s[1].compare(0, 4, "opt/") == 0) list_layer(s[1].substr(4));
            } else if (table == "psp_layers" && array_key == "rows") {
                // ["psp-music", "opt/psp-music/bgm/000.DAT", "psp-eu:ppms:...", "<sha256>"]
                const std::vector<std::string> s = Strings(line);
                if (s.size() >= 2 && s[1].compare(0, 4 + s[0].size() + 1, "opt/" + s[0] + "/") == 0) list_layer(s[0]);
            }
            continue;
        }
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            table = line;
            while (!table.empty() && table.front() == '[') table.erase(table.begin());
            while (!table.empty() && table.back() == ']') table.pop_back();
            table = Trim(table);
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
        if (!value.empty() && value.front() == '[' && value.back() != ']') {
            in_array = true;
            array_key = key;
            continue;
        }
        std::string s;
        if (table == "source" && key == "build") {
            if (OneString(value, s) && !Contains(m.sources, s)) m.sources.push_back(s);
        } else if (table == "build" && key == "base_policy") {
            if (!OneString(value, s) || (s != "psp" && s != "psx")) {
                if (m.error.empty())
                    m.error = "manifest.toml's [build] base_policy is " + value + ", neither \"psp\" nor \"psx\"";
            } else {
                m.base_policy = s;
            }
        } else if (table == "build" && key == "exe_tables_from") {
            std::vector<std::string> list;
            if (OneString(value, s)) list.push_back(s);
            else if (!StringList(value, list) && m.error.empty())
                m.error = "manifest.toml's [build] exe_tables_from is " + value + ", neither a build id nor a list of them";
            m.exe_tables_from = list;
            tables_given = true;
        } else if (table == "exe" && key == "build") {
            if (OneString(value, s)) exe_build = s;
        } else if (table == "snd" && key == "build") {
            if (OneString(value, s)) m.snd_build = s;
        }
    }
    if (!tables_given && !exe_build.empty()) m.exe_tables_from.push_back(exe_build);
    return m;
}

CacheManifest ManifestRead(const std::wstring& cache) {
    if (cache.empty()) return CacheManifest();
    HANDLE f = CreateFileW((cache + L"\\manifest.toml").c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return CacheManifest();
    std::string text;
    std::vector<char> buf(65536);   // not on the stack: the DLL reads it on a game thread's
    DWORD got = 0;
    while (ReadFile(f, buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr) && got > 0) text.append(buf.data(), got);
    CloseHandle(f);
    return ManifestParse(text);
}

bool ManifestOffersLayer(const CacheManifest& m, const std::string& layer) {
    return !m.present || Contains(m.opt_layers, layer);
}

std::string ManifestRefusal(const CacheManifest& m, const std::string& where) {
    if (m.error.empty()) return std::string();
    return m.error + " (" + where + "): not guessed - rebuild the cache with tools/importer.py build, or correct "
           "the line";
}

}  // namespace bof3x
