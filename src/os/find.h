// bof3::os directory search with a handle: the game's save list
// (save_menu::ListFiles through Crt_findfirst / Crt_findnext,
// docs/save-files.md) asks for one match, then the next, into MSVC 6's
// _finddata_t. The record's layout is the game's, so it is spelled here and
// each backend fills it: the C runtime's own search on Windows
// (win32/find.cpp), the portable walk of os/file.h on POSIX (posix/find.cpp).
// For a walk that needs no handle use WalkDirectory (os/file.h).
#pragma once

#include <cstddef>
#include <cstdint>

namespace bof3::os {

// MSVC 6's _finddata_t: 0x118 bytes, the times 32-bit seconds since 1970.
struct FindData {
    std::uint32_t attrib;        // kFindDirectory, kFindArchive and the system's other bits
    std::int32_t time_create;    // -1 where the system keeps none
    std::int32_t time_access;    // -1 where the system keeps none
    std::int32_t time_write;
    std::uint32_t size;          // the low 32 bits
    char name[260];              // the leaf, in the narrow encoding (os/file.h)
};
static_assert(sizeof(FindData) == 0x118 && offsetof(FindData, size) == 0x10 && offsetof(FindData, name) == 0x14,
              "FindData is MSVC 6's _finddata_t");

inline constexpr std::uint32_t kFindDirectory = 0x10;
inline constexpr std::uint32_t kFindArchive = 0x20;

// The first match of `pattern` (a directory part and a leaf with '*' and '?')
// into `found`, and a handle for the next; -1 when nothing matches.
std::int32_t FindFirst(const char* pattern, FindData* found) noexcept;
// The next match into `found`: 0, or -1 when there is none (or no such handle).
int FindNext(std::int32_t handle, FindData* found) noexcept;
// Ends a search: 0, or -1 for a handle that is not one.
int FindClose(std::int32_t handle) noexcept;

}  // namespace bof3::os
