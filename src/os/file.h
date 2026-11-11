// Files: what std::filesystem and <fstream> do not, which is little. A UTF-8
// name from the game becomes a std::filesystem::path (Path), and the game's
// "DAT\\AREA*.DAT" directory walk becomes one function (WalkDirectory). Opening,
// reading, sizes, existence, making and removing directories, temp and absolute
// paths are std::ifstream / std::ofstream / std::filesystem at the call site.
//
// Guarantees, on both systems: a backslash in the game's name reads as a
// separator (on POSIX it is turned into a slash; the case of a name is NOT
// folded - that is LB8's, at the source of each name); WalkDirectory hands
// entries in name order, case-insensitive, like NTFS, so "the first match"
// means the same thing on both. Neither throws.
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

// A UTF-8 name as a path.
std::filesystem::path Path(const char* utf8);

// Whether a UTF-8 name is a directory / a regular file. The standard calls
// throw on an error unless given an error_code; these answer false instead.
inline bool IsDirectory(const char* utf8) {
    std::error_code ec;
    return std::filesystem::is_directory(Path(utf8), ec);
}
inline bool IsFile(const char* utf8) {
    std::error_code ec;
    return std::filesystem::is_regular_file(Path(utf8), ec);
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
    std::string name;  // the leaf alone
    bool is_directory = false;
    std::uint64_t size = 0;
};
// Each entry of `pattern` - a directory, a separator and a leaf pattern with
// '*' and '?' ("DAT\\AREA*.DAT"; no directory part means the current one) -
// handed to `each`, which returns false to stop. "." and ".." are never
// handed. Returns how many entries were handed.
std::size_t WalkDirectory(const char* pattern, const std::function<bool(const DirEntry&)>& each);

}  // namespace bof3::os
