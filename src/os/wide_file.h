// bof3::os, files under a wide name, answered as Windows' W calls answer: for
// the launcher's settings model (launcher/config.cpp), its reader of the
// player's exe (launcher/exe_image.cpp) and the start of the game
// (launcher/start.cpp), which hold every path as a std::wstring. The engine's
// own names are narrow and go through os/file.h instead.
//
// Windows: GetFileAttributesW, FindFirstFileW (its matching, 8.3 names
// included), CreateFileW with the share mode the caller names, ReadFile,
// SetFilePointerEx, GetFileSizeEx, GetFileAttributesExW, GetFullPathNameW -
// the calls the launcher made before the seam, with the same arguments.
// POSIX: the wide name as UTF-8 with a backslash read as a separator (as
// os/file.h's Path reads one), std::filesystem, the walk of os/file.h
// (WalkDirectory: '*' and '?', ASCII case not counted in the leaf, as
// FindFirstFile; the folders above it as the system holds them), and open,
// read, lseek, fstat.
//
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace bof3::os {

// The wide name as a path (POSIX: a backslash is a separator). Empty when it
// does not convert.
std::filesystem::path PathW(const std::wstring& wide) noexcept;

// Whether the name is there and is not a folder / is a folder.
bool IsFileW(const std::wstring& path) noexcept;
bool IsDirectoryW(const std::wstring& path) noexcept;

// Whether `pattern` - a folder, a separator, and a leaf with '*' and '?' -
// matches a file or a folder.
bool AnyMatchW(const std::wstring& pattern) noexcept;
struct MatchW {
    std::wstring name;   // the leaf
    bool directory = false;
};
// Every match of `pattern`, in the system's order (Windows) or by name, case
// not counted (POSIX, the walk's order). Empty when there is none. Windows
// hands "." and ".." where the pattern matches them ("*"), as FindFirstFileW
// does; the POSIX walk never hands them.
std::vector<MatchW> MatchesW(const std::wstring& pattern) noexcept;

// A file opened for reading. `others_may_write`: Windows' FILE_SHARE_WRITE
// beside FILE_SHARE_READ (others may always read it); POSIX shares every file.
using InFile = std::intptr_t;
inline constexpr InFile kNoInFile = -1;
InFile InOpenW(const std::wstring& path, bool others_may_write) noexcept;
// Up to `n` bytes from where the file stands into `to`; `*got` the count, 0 at
// its end. False on an error.
bool InRead(InFile f, void* to, std::uint32_t n, std::uint32_t* got) noexcept;
// The next read at `offset` from the start. False on an error.
bool InSeek(InFile f, std::uint64_t offset) noexcept;
bool InSize(InFile f, std::uint64_t* size) noexcept;
void InClose(InFile f) noexcept;

// `data` as the whole of the file, made or made empty first; no other process
// may open the file while it is written (Windows: share mode 0). False when
// the file cannot be made or not all was written.
bool WriteWholeW(const std::wstring& path, const std::string& data) noexcept;

// The file's size and its last write, for a test of "changed since": the time
// in the system's own unit (Windows: FILETIME's 100 ns since 1601; POSIX: ns
// since 1970), compared only with itself. False when it is not there.
bool StampW(const std::wstring& path, std::uint64_t* size, std::uint64_t* time) noexcept;

// The name made absolute against the working folder, "." and ".." taken out,
// as GetFullPathNameW makes it, held to 259 characters (MAX_PATH with its
// terminator) on both systems. False when it is longer or cannot be made.
bool FullPathW(const std::wstring& path, std::wstring* full) noexcept;

}  // namespace bof3::os
