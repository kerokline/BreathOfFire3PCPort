// bof3::os::CaseFold on POSIX: the exact name first, then one walk per part
// of a relative name, each folder's listing cached. See os/case_fold.h.
#include "os/case_fold.h"

#include <algorithm>
#include <cerrno>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <sys/stat.h>

#include "os/case_check.h"

namespace bof3::os {

namespace {

std::string Upper(std::string s) {
    for (char& c : s)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    return s;
}

// One folder's listing: each entry's upper-cased name to the entries that
// fold to it, in byte order (the first is the fold's choice, rule 3), and
// what the folder was when it was listed.
struct Listing {
    dev_t dev = 0;
    ino_t ino = 0;
    timespec mtime{};
    timespec ctime{};
    std::unordered_map<std::string, std::vector<std::string>> names;
};

struct State {
    std::mutex lock;
    std::unordered_map<std::string, Listing> folders;   // by the folder's relative name as the disk holds it
    std::size_t walks = 0;
    std::vector<std::string> said;   // names whose line was given
};
// Made at its first use and never destroyed: a name may be converted during
// another unit's start-up or the process's end.
State& Get() {
    static State* const s = new State;
    return *s;
}

bool Same(const timespec& a, const timespec& b) { return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec; }

// The listing of `dir` ("." for the game folder), walked again when the
// folder is another or has changed since. Null when it is not a folder.
const Listing* ListingOf(State& s, const std::string& dir) {
    struct stat st {};
    if (::stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return nullptr;
    const auto known = s.folders.find(dir);
    if (known != s.folders.end()) {
        const Listing& k = known->second;
        if (k.dev == st.st_dev && k.ino == st.st_ino && Same(k.mtime, st.st_mtim) && Same(k.ctime, st.st_ctim)) return &k;
    }
    Listing& l = s.folders[dir];
    l = Listing{};
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec), end;
    if (ec) {
        s.folders.erase(dir);
        return nullptr;
    }
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        std::string n = it->path().filename().native();
        l.names[Upper(n)].push_back(std::move(n));
    }
    for (auto& [key, list] : l.names) std::sort(list.begin(), list.end());
    l.dev = st.st_dev;
    l.ino = st.st_ino;
    l.mtime = st.st_mtim;
    l.ctime = st.st_ctim;
    ++s.walks;
    return &l;
}

bool Wild(const std::string& s) { return s.find_first_of("*?") != std::string::npos; }

std::filesystem::path Fold(const std::filesystem::path& asked) {
    if (asked.empty() || asked.is_absolute() || asked.has_root_path()) return asked;
    for (const std::filesystem::path& part : asked)
        if (part.native() == "..") return asked;
    struct stat st {};
    if (::lstat(asked.c_str(), &st) == 0) return asked;   // rule 1

    State& s = Get();
    const std::lock_guard<std::mutex> hold(s.lock);
    std::filesystem::path found;   // the parts as the disk holds them
    bool differs = false, ended = false;
    for (const std::filesystem::path& part : asked) {
        const std::string& p = part.native();
        if (ended || p.empty() || p == "." || Wild(p)) {
            ended = ended || Wild(p);
            found /= part;
            continue;
        }
        const std::string dir = found.empty() ? std::string(".") : found.native();
        const Listing* l = ListingOf(s, dir);
        const std::vector<std::string>* hit = nullptr;
        if (l) {
            const auto at = l->names.find(Upper(p));
            if (at != l->names.end()) hit = &at->second;
        }
        if (!hit) {
            ended = true;   // not there in any case: it and the rest as asked
            found /= part;
            continue;
        }
        const std::vector<std::string>& same = *hit;
        const bool exact = std::find(same.begin(), same.end(), p) != same.end();
        const std::string& take = exact ? p : same.front();
        differs = differs || !exact;
        found /= take;
    }
    if (!differs) return asked;
    if (CaseCheckOn()) {
        const std::string& a = asked.native();
        if (std::find(s.said.begin(), s.said.end(), a) == s.said.end()) {
            s.said.push_back(a);
            const std::string line = "CASEFOLD asked " + a + " opened " + found.native();
            CaseCheckSay(line.c_str());
        }
    }
    return found;
}

}  // namespace

std::filesystem::path CaseFold(const std::filesystem::path& asked) noexcept {
    const int error = errno;
    std::filesystem::path out;
    try {
        out = Fold(asked);
    } catch (...) {
        try {
            out = asked;
        } catch (...) {
        }
    }
    errno = error;
    return out;
}

std::size_t CaseFoldWalks() noexcept {
    State& s = Get();
    const std::lock_guard<std::mutex> hold(s.lock);
    return s.walks;
}

void CaseFoldForgetForTest() noexcept {
    try {
        State& s = Get();
        const std::lock_guard<std::mutex> hold(s.lock);
        s.folders.clear();
        s.said.clear();
    } catch (...) {
    }
}

}  // namespace bof3::os
