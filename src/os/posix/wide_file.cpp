// bof3::os, files under a wide name, on POSIX: the name as UTF-8, a backslash
// read as a separator, and the walk of os/file.h. See os/wide_file.h.

#include "os/wide_file.h"
#include "os/file.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace bof3::os {

std::filesystem::path PathW(const std::wstring& wide) noexcept {
    try {
        std::wstring w = wide;
        for (wchar_t& c : w)
            if (c == L'\\') c = L'/';
        return std::filesystem::path(w);   // libstdc++: UTF-32 to UTF-8, whatever the locale; throws on a lone surrogate
    } catch (...) {
        return std::filesystem::path();
    }
}

bool IsFileW(const std::wstring& path) noexcept {
    const std::filesystem::path p = PathW(path);
    std::error_code ec;
    const auto s = std::filesystem::status(p, ec);
    return !p.empty() && !ec && std::filesystem::exists(s) && !std::filesystem::is_directory(s);
}

bool IsDirectoryW(const std::wstring& path) noexcept {
    const std::filesystem::path p = PathW(path);
    std::error_code ec;
    return !p.empty() && std::filesystem::is_directory(p, ec);
}

std::vector<MatchW> MatchesW(const std::wstring& pattern) noexcept {
    std::vector<MatchW> out;
    try {
        const std::string narrow = PathW(pattern).native();
        if (narrow.empty()) return out;
        WalkDirectory(narrow.c_str(), [&out](const DirEntry& e) {
            try {
                out.push_back({std::filesystem::path(e.name).wstring(), e.is_directory});
            } catch (...) {
                // a name that is not UTF-8 has no wide form: not handed, as a name that is not there
            }
            return true;
        });
    } catch (...) {
        out.clear();
    }
    return out;
}

bool AnyMatchW(const std::wstring& pattern) noexcept {
    try {
        const std::string narrow = PathW(pattern).native();
        return !narrow.empty() && WalkDirectory(narrow.c_str(), [](const DirEntry&) { return false; }) > 0;
    } catch (...) {
        return false;
    }
}

InFile InOpenW(const std::wstring& path, bool) noexcept {
    const std::filesystem::path p = PathW(path);
    if (p.empty()) return kNoInFile;
    const int fd = open(p.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return kNoInFile;
    struct stat st;
    if (fstat(fd, &st) != 0 || S_ISDIR(st.st_mode)) {   // CreateFileW does not open a folder for reading
        close(fd);
        errno = EISDIR;
        return kNoInFile;
    }
    return fd;
}

bool InRead(InFile f, void* to, std::uint32_t n, std::uint32_t* got) noexcept {
    for (;;) {
        const ssize_t r = read(static_cast<int>(f), to, n);
        if (r < 0 && errno == EINTR) continue;
        *got = r > 0 ? static_cast<std::uint32_t>(r) : 0;
        return r >= 0;
    }
}

bool InSeek(InFile f, std::uint64_t offset) noexcept {
    return lseek(static_cast<int>(f), static_cast<off_t>(offset), SEEK_SET) >= 0;
}

bool InSize(InFile f, std::uint64_t* size) noexcept {
    struct stat st;
    if (fstat(static_cast<int>(f), &st) != 0) return false;
    *size = static_cast<std::uint64_t>(st.st_size);
    return true;
}

void InClose(InFile f) noexcept {
    if (f != kNoInFile) close(static_cast<int>(f));
}

bool WriteWholeW(const std::wstring& path, const std::string& data) noexcept {
    const std::filesystem::path p = PathW(path);
    if (p.empty()) return false;
    const int fd = open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (fd < 0) return false;
    const char* b = data.data();
    std::size_t left = data.size();
    bool ok = true;
    while (left > 0) {
        const ssize_t w = write(fd, b, left);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) {
            ok = false;
            break;
        }
        b += w;
        left -= static_cast<std::size_t>(w);
    }
    return close(fd) == 0 && ok;
}

bool StampW(const std::wstring& path, std::uint64_t* size, std::uint64_t* time) noexcept {
    const std::filesystem::path p = PathW(path);
    struct stat st;
    if (p.empty() || stat(p.c_str(), &st) != 0) return false;
    *size = static_cast<std::uint64_t>(st.st_size);
    *time = static_cast<std::uint64_t>(st.st_mtim.tv_sec) * 1000000000ull + static_cast<std::uint64_t>(st.st_mtim.tv_nsec);
    return true;
}

bool FullPathW(const std::wstring& path, std::wstring* full) noexcept {
    try {
        std::filesystem::path p = PathW(path);
        if (p.empty()) return false;
        std::error_code ec;
        if (p.is_relative()) {
            const std::filesystem::path cwd = std::filesystem::current_path(ec);
            if (ec) return false;
            p = cwd / p;
        }
        // GetFullPathNameW's road: "." and ".." by the name alone, no link followed;
        // a trailing separator kept only for the root.
        p = p.lexically_normal();
        std::wstring w = p.wstring();
        while (w.size() > 1 && w.back() == L'/') w.pop_back();
        if (w.size() >= 260) return false;
        *full = w;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace bof3::os
