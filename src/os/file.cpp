// bof3::os files (os/file.h): the directory walk, portable, over
// std::filesystem. Path and Narrow - the narrow encoding - are each backend's
// (win32/file.cpp, posix/file.cpp).
#include "os/file.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include "os/case_check.h"
#include "os/env.h"

namespace bof3::os {

namespace {

// The system's own character (wchar_t on Windows, char on POSIX): the walk
// matches and orders names as the system holds them, so a multi-byte narrow
// name is never compared byte by byte.
using Char = std::filesystem::path::value_type;
using Native = std::filesystem::path::string_type;

Char Upper(Char c) { return c >= 'a' && c <= 'z' ? static_cast<Char>(c - 32) : c; }

// FindFirstFile's matching: ASCII case does not count, '?' is one character,
// '*' any run, and "*.*" every name. With `exact` case counts (the case
// check's second look, os/case_check.h).
bool Match(const Char* pat, const Char* name, bool exact = false) {
    if (pat[0] == '*' && pat[1] == '.' && pat[2] == '*' && pat[3] == 0) pat += 2;
    const Char *star = nullptr, *resume = nullptr;
    while (*name) {
        if (*pat == '*') {
            star = pat++;
            resume = name;
        } else if (*pat == '?' || (exact ? *pat == *name : Upper(*pat) == Upper(*name))) {
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
    const bool check = CaseCheckOn();
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        const Native n = it->path().filename().native();
        if (n == Native{Char('.')} || n == Native{Char('.'), Char('.')}) continue;
        if (Match(leaf.c_str(), n.c_str())) {
            out.names.push_back(n);
            if (check) CaseCheckMatch(pattern, leaf, n);
        }
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

// ---- The case check (os/case_check.h): the portable half. StoredSpelling is
// each backend's.

namespace {

// -1 not read yet, 0 off, 1 on.
std::atomic<int> g_case_on{-1};

// Made at its first use, not at the module's start: a name may be converted
// by another unit's start-up before this one's would have run; and never
// destroyed, since a name may be converted during the process's end too.
struct CaseState {
    std::mutex lock;
    CaseCheckSinkFn sink = nullptr;
    std::set<std::string> said;       // each line once
    std::vector<std::string> kept;    // lines made before the sink is set
};
CaseState& Case() {
    static CaseState* const s = new CaseState;
    return *s;
}
constexpr std::size_t kCaseKept = 1000;

std::string Utf8Of(const std::filesystem::path& p) {
    try {
        const std::u8string u = p.u8string();
        return std::string(u.begin(), u.end());
    } catch (...) {
        return std::string("(a name that is not UTF-8)");
    }
}

void Say(std::string line) {
    CaseState& c = Case();
    CaseCheckSinkFn sink;
    {
        const std::lock_guard<std::mutex> hold(c.lock);
        if (!c.said.insert(line).second) return;
        sink = c.sink;
        if (!sink) {
            if (c.kept.size() < kCaseKept) c.kept.push_back(std::move(line));
            return;
        }
    }
    sink(line.c_str());   // outside the lock: the sink may take its own
}

bool Wild(const Native& s) { return s.find_first_of(Native{Char('*'), Char('?')}) != Native::npos; }

}  // namespace

bool CaseCheckOn() noexcept {
    int on = g_case_on.load(std::memory_order_relaxed);
    if (on < 0) {
        char v[8];
        const std::uint32_t n = GetEnv("BOF3X_CASECHECK", v, sizeof v);
        on = n == 1 && v[0] == '1';
        g_case_on.store(on, std::memory_order_relaxed);
    }
    return on != 0;
}

void CaseCheckSink(CaseCheckSinkFn sink) noexcept {
    try {
        CaseState& c = Case();
        std::vector<std::string> kept;
        {
            const std::lock_guard<std::mutex> hold(c.lock);
            c.sink = sink;
            if (sink) kept.swap(c.kept);
        }
        for (const std::string& line : kept) sink(line.c_str());
    } catch (...) {
    }
}

void CaseCheckForTest(bool on) noexcept {
    g_case_on.store(on ? 1 : 0, std::memory_order_relaxed);
    try {
        CaseState& c = Case();
        const std::lock_guard<std::mutex> hold(c.lock);
        c.said.clear();
        c.kept.clear();
    } catch (...) {
    }
}

void CaseCheck(const std::filesystem::path& asked) noexcept {
    if (!CaseCheckOn() || asked.empty()) return;
    try {
        // Each part is asked of the folder under the disk's spelling found so
        // far, so a case-sensitive system finds the parts after a wrong one.
        std::filesystem::path found = asked.root_path();
        bool differs = false;
        for (const std::filesystem::path& part : asked.relative_path()) {
            const Native& s = part.native();
            if (s.empty() || s == Native{Char('.')} || s == Native{Char('.'), Char('.')} || Wild(s)) {
                found /= part;
                continue;
            }
            Native stored;
            if (!StoredSpelling(found / part, &stored)) return;
            // Another spelling is a name equal but for ASCII case. Windows also
            // answers a short alias ("RUNNER~1") with the long name it stands
            // for: that is another name, not another case, and it opens on
            // no other system anyway, so the part stays as asked and gives no line.
            if (stored == s || UpperAll(stored) != UpperAll(s)) {
                found /= part;
                continue;
            }
            differs = true;
            found /= std::filesystem::path(stored);
        }
        if (differs) Say("CASECHECK asked " + Utf8Of(asked) + " found " + Utf8Of(found));
    } catch (...) {
        // out of memory: the check is a developer's aid and says nothing
    }
}

void CaseCheckNarrow(const char* narrow) noexcept {
    if (!CaseCheckOn() || !narrow || !*narrow) return;
    (void)Path(narrow);   // Path checks every name it converts (win32/file.cpp, posix/file.cpp)
}

void CaseCheckSay(const char* line) noexcept {
    if (!CaseCheckOn() || !line) return;
    try {
        Say(line);
    } catch (...) {
    }
}

void CaseCheckMatch(const char* where, const Native& pattern, const Native& found) noexcept {
    if (!CaseCheckOn() || Match(pattern.c_str(), found.c_str(), true)) return;
    try {
        Say(std::string("CASECHECK walk ") + (where ? where : "") + " found " + Utf8Of(std::filesystem::path(found)));
    } catch (...) {
    }
}

}  // namespace bof3::os
