// bof3::os, files under a wide name, on Windows: the W calls. See os/wide_file.h.

#include "os/wide_file.h"

#include <windows.h>

namespace bof3::os {

std::filesystem::path PathW(const std::wstring& wide) noexcept {
    try {
        return std::filesystem::path(wide);
    } catch (...) {
        return std::filesystem::path();
    }
}

bool IsFileW(const std::wstring& path) noexcept {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool IsDirectoryW(const std::wstring& path) noexcept {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool AnyMatchW(const std::wstring& pattern) noexcept {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    FindClose(h);
    return true;
}

std::vector<MatchW> MatchesW(const std::wstring& pattern) noexcept {
    std::vector<MatchW> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    try {
        do out.push_back({fd.cFileName, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0});
        while (FindNextFileW(h, &fd));
    } catch (...) {
        out.clear();
    }
    FindClose(h);
    return out;
}

InFile InOpenW(const std::wstring& path, bool others_may_write) noexcept {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | (others_may_write ? FILE_SHARE_WRITE : 0),
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    return f == INVALID_HANDLE_VALUE ? kNoInFile : reinterpret_cast<InFile>(f);
}

bool InRead(InFile f, void* to, std::uint32_t n, std::uint32_t* got) noexcept {
    DWORD g = 0;
    const BOOL ok = ReadFile(reinterpret_cast<HANDLE>(f), to, n, &g, nullptr);
    *got = g;
    return ok != 0;
}

bool InSeek(InFile f, std::uint64_t offset) noexcept {
    LARGE_INTEGER at;
    at.QuadPart = static_cast<LONGLONG>(offset);
    return SetFilePointerEx(reinterpret_cast<HANDLE>(f), at, nullptr, FILE_BEGIN) != 0;
}

bool InSize(InFile f, std::uint64_t* size) noexcept {
    LARGE_INTEGER s{};
    if (!GetFileSizeEx(reinterpret_cast<HANDLE>(f), &s)) return false;
    *size = static_cast<std::uint64_t>(s.QuadPart);
    return true;
}

void InClose(InFile f) noexcept {
    if (f != kNoInFile) CloseHandle(reinterpret_cast<HANDLE>(f));
}

bool WriteWholeW(const std::wstring& path, const std::string& data) noexcept {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0;
    const BOOL ok = WriteFile(f, data.data(), static_cast<DWORD>(data.size()), &wrote, nullptr);
    CloseHandle(f);
    return ok && wrote == data.size();
}

bool StampW(const std::wstring& path, std::uint64_t* size, std::uint64_t* time) noexcept {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a)) return false;
    *size = (static_cast<std::uint64_t>(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
    *time = (static_cast<std::uint64_t>(a.ftLastWriteTime.dwHighDateTime) << 32) | a.ftLastWriteTime.dwLowDateTime;
    return true;
}

bool FullPathW(const std::wstring& path, std::wstring* full) noexcept {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetFullPathNameW(path.c_str(), MAX_PATH, buf, nullptr);
    if (n == 0 || n >= MAX_PATH) return false;
    try {
        *full = buf;
    } catch (...) {
        return false;
    }
    return true;
}

}  // namespace bof3::os
