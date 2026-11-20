#include "common/home.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <cstdint>

#include "os/os.h"

namespace bof3x {
namespace {

constexpr std::uint32_t kMaxPath = 260;   // the limit this folder's paths were always read under

// The system's own separator: a backslash on Windows, as these names were
// always built, so every name below is the same there; a slash on POSIX, where
// a backslash is a character of a name (docs/linux-round.md, C4).
constexpr wchar_t kSep = static_cast<wchar_t>(std::filesystem::path::preferred_separator);

bool IsDirectory(const std::wstring& path) { return bof3::os::IsDirectory(path.c_str()); }

bool IsFile(const std::wstring& path) { return bof3::os::IsFile(path.c_str()); }

}  // namespace

std::wstring HomeDir() {
    std::wstring home;
    wchar_t env[kMaxPath];
    const std::uint32_t n = bof3::os::GetEnvW(L"BOF3X_HOME", env, kMaxPath);
    if (n > 0 && n < kMaxPath) {
        std::error_code ec;
        const std::filesystem::path full = std::filesystem::absolute(std::filesystem::path(env), ec).lexically_normal();
        home = ec ? std::wstring(env, n) : full.wstring();
    } else {
        std::string self;
        if (!bof3::os::ExecutablePath(&self)) return std::wstring();
        home = bof3::os::ToWide(self.c_str());
        home = home.substr(0, home.find_last_of(L"\\/"));
    }
    while (home.size() > 3 && (home.back() == L'\\' || home.back() == L'/')) home.pop_back();
    return home;
}

std::wstring HomeGameDefault() {
    const std::wstring home = HomeDir();
    if (home.empty()) return std::wstring();
    const std::wstring game = home + kSep + L"game";
    return IsDirectory(game + kSep + L"DAT") ? game : std::wstring();
}

std::wstring HomeCacheDefault() {
    const std::wstring home = HomeDir();
    if (home.empty()) return std::wstring();
    const std::wstring cache = home + kSep + L"cache";
    return IsFile(cache + kSep + L"manifest.toml") ? cache : std::wstring();
}

std::wstring HomeInPlaceFolder() {
    const std::wstring home = HomeDir();
    if (home.empty()) return std::wstring();
    std::ifstream f(std::filesystem::path(home + kSep + L"sources.toml"), std::ios::binary);
    if (!f) return std::wstring();
    std::string path, line;
    bool in_row = false, path_ok = false, in_place = false;
    auto row_done = [&]() -> std::wstring {
        if (!(in_row && path_ok && in_place && !path.empty())) return std::wstring();
        std::wstring p = bof3::os::ToWide(path.c_str());
        if (!std::filesystem::path(p).is_absolute()) p = home + kSep + p;
        return p;
    };
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        const std::size_t b = line.find_first_not_of(" \t");
        if (b == std::string::npos || line[b] == '#') continue;
        line.erase(0, b);
        if (line[0] == '[') {
            if (const std::wstring got = row_done(); !got.empty()) return got;
            in_row = line == "[[source]]";
            path.clear();
            path_ok = in_place = false;
            continue;
        }
        if (!in_row) continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        const std::size_t v = value.find_first_not_of(" \t");
        value = v == std::string::npos ? std::string() : value.substr(v);
        if (key == "play_in_place") {
            in_place = value == "true";
        } else if (key == "path") {
            path.clear();
            path_ok = value.size() >= 2 && value.front() == '"' && value.back() == '"';
            for (std::size_t i = 1; path_ok && i + 1 < value.size(); ++i) {
                char c = value[i];
                if (c == '"') {
                    path_ok = false;   // a second string, or a quote not escaped: not one path
                } else if (c == '\\') {
                    ++i;               // the escaped character, never the closing quote
                    c = i + 1 < value.size() ? value[i] : '\0';
                    if (c == '"' || c == '\\') path += c;
                    else path_ok = false;
                } else {
                    path += c;
                }
            }
        }
    }
    return row_done();
}

}  // namespace bof3x
