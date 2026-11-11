// bof3::os files (os/file.h): portable, over std::filesystem.
#include "os/file.h"

#include <algorithm>
#include <cstring>
#include <system_error>
#include <vector>

namespace bof3::os {

namespace {

char Upper(char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c; }

// FindFirstFile's matching: ASCII case does not count, '?' is one character,
// '*' any run, and "*.*" every name.
bool Match(const char* pat, const char* name) {
    if (std::strcmp(pat, "*.*") == 0) pat = "*";
    const char *star = nullptr, *resume = nullptr;
    while (*name) {
        if (*pat == '*') {
            star = pat++;
            resume = name;
        } else if (*pat == '?' || Upper(*pat) == Upper(*name)) {
            ++pat;
            ++name;
        } else if (star) {
            pat = star + 1;
            name = ++resume;
        } else {
            return false;
        }
    }
    while (*pat == '*') ++pat;
    return *pat == 0;
}

std::string UpperAll(std::string s) {
    for (char& c : s) c = Upper(c);
    return s;
}

std::string Utf8(const std::filesystem::path& p) {
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

}  // namespace

std::filesystem::path Path(const char* utf8) {
    std::string s(utf8 ? utf8 : "");
    if (std::filesystem::path::preferred_separator == '/') std::replace(s.begin(), s.end(), '\\', '/');
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

std::size_t WalkDirectory(const char* pattern, const std::function<bool(const DirEntry&)>& each) {
    const std::string full = Utf8(Path(pattern));
    const std::size_t cut = full.find_last_of("/\\");
    const std::string dir = cut == std::string::npos ? "." : cut == 0 ? "/" : full.substr(0, cut);
    const std::string leaf = cut == std::string::npos ? full : full.substr(cut + 1);
    std::error_code ec;
    std::filesystem::directory_iterator it(Path(dir.c_str()), ec), end;
    if (ec) return 0;
    std::vector<std::string> names;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        const std::string n = Utf8(it->path().filename());
        if (n == "." || n == "..") continue;
        if (Match(leaf.c_str(), n.c_str())) names.push_back(n);
    }
    // NTFS lists a folder in upper-cased order; callers that take the first
    // match must see the same one on every system.
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
        const std::string la = UpperAll(a), lb = UpperAll(b);
        return la != lb ? la < lb : a < b;
    });
    std::size_t handed = 0;
    for (const std::string& n : names) {
        DirEntry e;
        e.name = n;
        const std::filesystem::path p = Path(dir.c_str()) / std::filesystem::path(std::u8string(n.begin(), n.end()));
        std::error_code sec;
        e.is_directory = std::filesystem::is_directory(p, sec);
        if (!e.is_directory) {
            const std::uintmax_t sz = std::filesystem::file_size(p, sec);
            e.size = sec ? 0 : static_cast<std::uint64_t>(sz);
        }
        ++handed;
        if (!each(e)) break;
    }
    return handed;
}

}  // namespace bof3::os
