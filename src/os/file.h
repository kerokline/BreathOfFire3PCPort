// Files: what std::filesystem and <fstream> do not, which is little. A narrow
// name from the game becomes a std::filesystem::path (Path), and the game's
// "DAT\\AREA*.DAT" directory walk becomes one function (WalkDirectory). Opening,
// reading, sizes, existence, making and removing directories, temp and absolute
// paths are std::ifstream / std::ofstream / std::filesystem at the call site.
//
// THE NARROW ENCODING. A `char` name in the engine - one the game builds, one
// GetEnv returns, one handed to the game's own Crt_fopen - is in the system's
// narrow file-name encoding: on Windows the code page the A file calls use (the
// ANSI code page; a Chinese, Japanese or Western one, per machine), on POSIX
// the bytes the kernel takes (UTF-8 by convention). It is NOT UTF-8 on
// Windows. Path and Narrow are the only two conversions, each backend's own
// (win32/file.cpp, posix/file.cpp), so a name that came from GetEnv or from
// WalkDirectory can be printed into a longer name and opened by either this
// seam or the game's C runtime. os/text.h's ToUtf8 / ToWide are a different
// pair (UTF-8 and wide) and must not be fed a narrow name or feed one.
//
// Guarantees, on both systems: a backslash in the game's name reads as a
// separator (on POSIX it is turned into a slash; the case of a name is NOT
// folded - that is LB8's, at the source of each name); WalkDirectory hands
// entries in name order, case-insensitive, like NTFS, so "the first match"
// means the same thing on both. None of these throws: a name that does not
// convert, or that the system refuses, is a name that is not there.
// Part of the operating-system seam (docs/linux-round.md, stage LB1). Where the C++20
// standard library covers a service, the engine uses it; this folder holds only
// what the library cannot do. No Windows or POSIX type appears in a header.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>

namespace bof3::os {

// The longest path the game's own names were sized for (Windows' MAX_PATH): a
// name the game builds is held to it on every system, so one buffer size fits.
inline constexpr std::size_t kMaxPath = 260;

// A narrow name as a path, and a path as a narrow name (the inverse; on Windows
// a character the code page cannot hold comes back as FindFirstFileA gave it,
// a best fit or '?', and that name does not open). Empty on any failure.
std::filesystem::path Path(const char* narrow) noexcept;
std::string Narrow(const std::filesystem::path& path) noexcept;

// Whether a narrow name is a directory / a regular file. The standard calls
// throw on an error unless given an error_code; these answer false instead.
inline bool IsDirectory(const char* narrow) noexcept {
    std::error_code ec;
    return std::filesystem::is_directory(Path(narrow), ec);
}
inline bool IsFile(const char* narrow) noexcept {
    std::error_code ec;
    return std::filesystem::is_regular_file(Path(narrow), ec);
}
// The same of a wide name (the launcher's and common/'s paths).
inline bool IsDirectory(const wchar_t* wide) noexcept {
    try {
        std::error_code ec;
        return std::filesystem::is_directory(std::filesystem::path(wide ? wide : L""), ec);
    } catch (...) {
        return false;
    }
}
inline bool IsFile(const wchar_t* wide) noexcept {
    try {
        std::error_code ec;
        return std::filesystem::is_regular_file(std::filesystem::path(wide ? wide : L""), ec);
    } catch (...) {
        return false;
    }
}

// Two names equal when they differ only in the case of ASCII letters: how
// Windows compares a file name (lstrcmpiA, CompareStringA NORM_IGNORECASE) for
// the names the game builds. SameNameN compares the first `n` characters.
inline bool SameNameN(const char* a, const char* b, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; };
        if (lower(a[i]) != lower(b[i])) return false;
        if (a[i] == 0) return true;
    }
    return true;
}
inline bool SameName(const char* a, const char* b) { return SameNameN(a, b, std::strlen(a) + 1) && std::strlen(a) == std::strlen(b); }

struct DirEntry {
    std::string name;  // the leaf alone, in the narrow encoding, as the pattern came
    bool is_directory = false;
    std::uint64_t size = 0;
};
// Each entry of `pattern` - a directory, a separator and a leaf pattern with
// '*' and '?' ("DAT\\AREA*.DAT"; no directory part means the current one) -
// handed to `each`, which returns false to stop. "." and ".." are never
// handed. Returns how many entries were handed. The whole folder is listed and
// ordered before the first entry is handed; an entry's kind and size are asked
// only when it is handed, so "is there one" costs the listing and one entry.
std::size_t WalkDirectory(const char* pattern, const std::function<bool(const DirEntry&)>& each);

}  // namespace bof3::os
