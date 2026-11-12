// bof3::os files (os/file.h): the directory walk, portable, over
// std::filesystem. Path and Narrow - the narrow encoding - are each backend's
// (win32/file.cpp, posix/file.cpp).
#include "os/file.h"

#include <algorithm>
#include <system_error>
#include <vector>

namespace bof3::os {

namespace {

// The system's own character (wchar_t on Windows, char on POSIX): the walk
// matches and orders names as the system holds them, so a multi-byte narrow
// name is never compared byte by byte.
using Char = std::filesystem::path::value_type;
using Native = std::filesystem::path::string_type;

Char Upper(Char c) { return c >= 'a' && c <= 'z' ? static_cast<Char>(c - 32) : c; }

// FindFirstFile's matching: ASCII case does not count, '?' is one character,
// '*' any run, and "*.*" every name.
bool Match(const Char* pat, const Char* name) {
    if (pat[0] == '*' && pat[1] == '.' && pat[2] == '*' && pat[3] == 0) pat += 2;
    const Char *star = nullptr, *resume = nullptr;
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

Native UpperAll(Native s) {
    for (Char& c : s) c = Upper(c);
    return s;
}

// The directory of `pattern` and its matching leaves, in order. Throws what the
// library throws (out of memory); WalkDirectory answers that as "nothing there".
struct Listing {
    std::filesystem::path dir;
    std::vector<Native> names;
};
Listing List(const char* pattern) {
    const Native full = Path(pattern).native();
    const std::size_t cut = full.find_last_of(Native{Char('/'), Char('\\')});
    Listing out;
    out.dir = cut == Native::npos ? std::filesystem::path(Native{Char('.')})
              : cut == 0          ? std::filesystem::path(full.substr(0, 1))
                                  : std::filesystem::path(full.substr(0, cut));
    const Native leaf = cut == Native::npos ? full : full.substr(cut + 1);
    std::error_code ec;
    std::filesystem::directory_iterator it(out.dir, ec), end;
    if (ec) return out;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        const Native n = it->path().filename().native();
        if (n == Native{Char('.')} || n == Native{Char('.'), Char('.')}) continue;
        if (Match(leaf.c_str(), n.c_str())) out.names.push_back(n);
    }
    // NTFS lists a folder in upper-cased order; callers that take the first
    // match must see the same one on every system.
    std::sort(out.names.begin(), out.names.end(), [](const Native& a, const Native& b) {
        const Native la = UpperAll(a), lb = UpperAll(b);
        return la != lb ? la < lb : a < b;
    });
    return out;
}

// One entry: its narrow name, and its kind and size asked of the system now -
// only for an entry that is handed, so a caller that stops at the first pays
// for one.
bool Describe(const Listing& l, const Native& n, DirEntry* e) {
    try {
        const std::filesystem::path leaf(n);
        const std::filesystem::path p = l.dir / leaf;
        e->name = Narrow(leaf);
        std::error_code ec;
        e->is_directory = std::filesystem::is_directory(p, ec);
        e->size = 0;
        if (!e->is_directory) {
            const std::uintmax_t sz = std::filesystem::file_size(p, ec);
            e->size = ec ? 0 : static_cast<std::uint64_t>(sz);
        }
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

std::size_t WalkDirectory(const char* pattern, const std::function<bool(const DirEntry&)>& each) {
    Listing l;
    try {
        l = List(pattern);
    } catch (...) {
        return 0;
    }
    std::size_t handed = 0;
    for (const Native& n : l.names) {
        DirEntry e;
        if (!Describe(l, n, &e)) break;
        ++handed;
        if (!each(e)) break;
    }
    return handed;
}

}  // namespace bof3::os
