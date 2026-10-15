#include "common/home.h"

#include <windows.h>

namespace bof3x {
namespace {

bool IsDirectory(const std::wstring& path) {
    const DWORD at = GetFileAttributesW(path.c_str());
    return at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY);
}

bool IsFile(const std::wstring& path) {
    const DWORD at = GetFileAttributesW(path.c_str());
    return at != INVALID_FILE_ATTRIBUTES && !(at & FILE_ATTRIBUTE_DIRECTORY);
}

}  // namespace

std::wstring HomeDir() {
    std::wstring home;
    wchar_t env[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_HOME", env, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        wchar_t full[MAX_PATH];
        const DWORD m = GetFullPathNameW(env, MAX_PATH, full, nullptr);
        home = m > 0 && m < MAX_PATH ? std::wstring(full, m) : std::wstring(env, n);
    } else {
        wchar_t self[MAX_PATH];
        const DWORD m = GetModuleFileNameW(nullptr, self, MAX_PATH);
        if (m == 0 || m >= MAX_PATH) return std::wstring();
        home.assign(self, m);
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
