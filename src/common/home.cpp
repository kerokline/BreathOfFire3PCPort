#include "common/home.h"

#include <filesystem>
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

}  // namespace bof3x
