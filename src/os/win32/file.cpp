// bof3::os narrow names on Windows: the code page the A file calls read a name
// in (the ANSI code page, or the OEM one after SetFileApisToOEM). See os/file.h.

#include "os/file.h"

#include <windows.h>

namespace bof3::os {

namespace {
UINT NarrowCodePage() { return AreFileApisANSI() ? CP_ACP : CP_OEMCP; }
}  // namespace

// Flags 0, as CreateFileA and its kin convert: a byte the code page does not
// define becomes some character and the file is then not found; nothing fails.
std::filesystem::path Path(const char* narrow) noexcept {
    try {
        if (!narrow || !*narrow) return std::filesystem::path();
        const int len = static_cast<int>(std::strlen(narrow));
        const UINT cp = NarrowCodePage();
        const int n = MultiByteToWideChar(cp, 0, narrow, len, nullptr, 0);
        if (n <= 0) return std::filesystem::path();
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        if (MultiByteToWideChar(cp, 0, narrow, len, w.data(), n) != n) return std::filesystem::path();
        return std::filesystem::path(std::move(w));
    } catch (...) {
        return std::filesystem::path();
    }
}

// As FindFirstFileA hands a name: a character the code page cannot hold comes
// back as its best fit or the default character, and that name does not open.
std::string Narrow(const std::filesystem::path& path) noexcept {
    try {
        const std::wstring& w = path.native();
        if (w.empty()) return std::string();
        const UINT cp = NarrowCodePage();
        const int n = WideCharToMultiByte(cp, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (n <= 0) return std::string();
        std::string s(static_cast<std::size_t>(n), '\0');
        if (WideCharToMultiByte(cp, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr) != n) return std::string();
        return s;
    } catch (...) {
        return std::string();
    }
}

}  // namespace bof3::os
