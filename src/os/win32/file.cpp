// bof3::os narrow names on Windows: the code page the A file calls read a name
// in (the ANSI code page, or the OEM one after SetFileApisToOEM). See os/file.h.

#include "os/file.h"

#include <windows.h>

#include "os/case_check.h"

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
        std::filesystem::path p(std::move(w));
        if (CaseCheckOn()) CaseCheck(p);
        return p;
    } catch (...) {
        return std::filesystem::path();
    }
}

// As exe_image.cpp opened its files before the seam: MB_ERR_INVALID_CHARS, so a
// byte sequence that is not UTF-8 is refused rather than read as U+FFFD.
std::filesystem::path PathOfUtf8(const char* utf8) noexcept {
    try {
        if (!utf8 || !*utf8) return std::filesystem::path();
        const int len = static_cast<int>(std::strlen(utf8));
        const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, len, nullptr, 0);
        if (n <= 0) return std::filesystem::path();
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, len, w.data(), n) != n) return std::filesystem::path();
        std::filesystem::path p(std::move(w));
        if (CaseCheckOn()) CaseCheck(p);
        return p;
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

// The case check's question (os/case_check.h): FindFirstFileExW on the exact
// name hands back the entry as its folder stores it, without opening it. A
// root, and a name with a wildcard, are not asked. The thread's last error is
// left as the call before it left it, so a check after an open changes
// nothing its caller reads.
bool StoredSpelling(const std::filesystem::path& existing, std::wstring* stored) noexcept {
    const DWORD error = GetLastError();
    bool there = false;
    try {
        const std::wstring& w = existing.native();
        if (existing.has_relative_path() && existing.has_filename() && w.find_first_of(L"*?") == std::wstring::npos) {
            WIN32_FIND_DATAW found;
            const HANDLE h = FindFirstFileExW(w.c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);
            if (h != INVALID_HANDLE_VALUE) {
                ::FindClose(h);   // the system's, not os/find.h's
                stored->assign(found.cFileName);
                there = true;
            }
        }
    } catch (...) {
        there = false;
    }
    SetLastError(error);
    return there;
}

}  // namespace bof3::os
