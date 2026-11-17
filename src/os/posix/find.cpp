// bof3::os::FindFirst / FindNext / FindClose on POSIX: the matches of the
// portable walk (os/file.h WalkDirectory: FindFirstFile's matching and
// order), listed once at FindFirst and handed one at a time. A handle is a
// slot's number plus one, so it is never 0 and never -1.
//
// What POSIX does not have is said as MSVC says it for a file system without
// it: no creation time, and no access time that means what Windows' does, so
// both read -1. A directory is kFindDirectory, any other entry kFindArchive
// (what a file a program wrote reads as on Windows).
#include "os/find.h"

#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#include "os/file.h"

namespace bof3::os {

namespace {

struct Search {
    std::vector<FindData> rows;
    std::size_t next = 0;
    bool open = false;
};

std::mutex g_lock;
std::vector<Search> g_searches;

std::int32_t WriteTime(const std::string& path) {
    std::error_code ec;
    const std::filesystem::file_time_type t = std::filesystem::last_write_time(Path(path.c_str()), ec);
    if (ec) return -1;
    const auto since = std::chrono::file_clock::to_sys(t).time_since_epoch();
    return static_cast<std::int32_t>(std::chrono::duration_cast<std::chrono::seconds>(since).count());
}

Search* Find(std::int32_t handle) {
    if (handle < 1 || static_cast<std::size_t>(handle) > g_searches.size()) return nullptr;
    Search* const s = &g_searches[static_cast<std::size_t>(handle) - 1];
    return s->open ? s : nullptr;
}

}  // namespace

std::int32_t FindFirst(const char* pattern, FindData* found) noexcept {
    try {
        const std::string whole(pattern);
        const std::size_t cut = whole.find_last_of("/\\");
        const std::string dir = cut == std::string::npos ? std::string() : whole.substr(0, cut + 1);
        Search s;
        WalkDirectory(pattern, [&](const DirEntry& e) {
            // A leaf is at most NAME_MAX (255) bytes, so it fits name[260] with
            // its NUL; one that did not could not be handed whole and is not
            // handed in part.
            if (e.name.size() >= sizeof FindData::name) return true;
            FindData row{};
            row.attrib = e.is_directory ? kFindDirectory : kFindArchive;
            row.time_create = -1;
            row.time_access = -1;
            row.time_write = WriteTime(dir + e.name);
            row.size = static_cast<std::uint32_t>(e.size);
            std::memcpy(row.name, e.name.c_str(), e.name.size() + 1);
            s.rows.push_back(row);
            return true;
        });
        if (s.rows.empty()) return -1;
        *found = s.rows[0];
        s.next = 1;
        s.open = true;
        const std::lock_guard<std::mutex> hold(g_lock);
        for (std::size_t i = 0; i < g_searches.size(); ++i) {
            if (!g_searches[i].open) {
                g_searches[i] = std::move(s);
                return static_cast<std::int32_t>(i + 1);
            }
        }
        g_searches.push_back(std::move(s));
        return static_cast<std::int32_t>(g_searches.size());
    } catch (...) {
        return -1;
    }
}

int FindNext(std::int32_t handle, FindData* found) noexcept {
    const std::lock_guard<std::mutex> hold(g_lock);
    Search* const s = Find(handle);
    if (!s || s->next >= s->rows.size()) return -1;
    *found = s->rows[s->next++];
    return 0;
}

int FindClose(std::int32_t handle) noexcept {
    const std::lock_guard<std::mutex> hold(g_lock);
    Search* const s = Find(handle);
    if (!s) return -1;
    *s = Search{};
    return 0;
}

}  // namespace bof3::os
