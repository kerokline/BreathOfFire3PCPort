// Text: a path or a name between UTF-8 `std::string` and `std::wstring`, through
// std::filesystem::path (so Windows gets UTF-16 and POSIX UTF-32 wchar_t, as
// each system keeps them). A name that does not convert comes back empty.
//
// UTF-8 here is UTF-8 on both systems, and that is NOT the engine's narrow
// file-name encoding on Windows (os/file.h: the ANSI code page there). These
// two are for a wide path carried as bytes and back (ExecutablePath and
// ModulePath, a log line); a name for Path, IsFile, WalkDirectory, GetEnv or
// the game's Crt_fopen goes through os/file.h's Path and Narrow instead.
// Part of the operating-system seam (docs/linux-round.md, stage LB1). Where the C++20
// standard library covers a service, the engine uses it; this folder holds only
// what the library cannot do. No Windows or POSIX type appears in a header.
#pragma once

#include <cstring>
#include <filesystem>
#include <string>

namespace bof3::os {

inline std::string ToUtf8(const wchar_t* wide) {
    try {
        const std::u8string u = std::filesystem::path(wide ? wide : L"").u8string();
        return std::string(u.begin(), u.end());
    } catch (...) {
        return std::string();
    }
}

inline std::wstring ToWide(const char* utf8) {
    try {
        const char* s = utf8 ? utf8 : "";
        return std::filesystem::path(std::u8string(s, s + std::strlen(s))).wstring();
    } catch (...) {
        return std::wstring();
    }
}

}  // namespace bof3::os
