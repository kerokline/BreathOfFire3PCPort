#include "common/home.h"

#include <filesystem>
#include <system_error>
#include <cstdint>

#include "os/os.h"

namespace bof3x {
namespace {

constexpr std::uint32_t kMaxPath = 260;   // the limit this folder's paths were always read under

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
    const std::wstring game = home + L"\\game";
    return IsDirectory(game + L"\\DAT") ? game : std::wstring();
}

std::wstring HomeCacheDefault() {
    const std::wstring home = HomeDir();
    if (home.empty()) return std::wstring();
    const std::wstring cache = home + L"\\cache";
    return IsFile(cache + L"\\manifest.toml") ? cache : std::wstring();
}

}  // namespace bof3x
