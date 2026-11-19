// bof3::os::FindFirst / FindNext / FindClose on Windows: the C runtime's
// _findfirst32 family, as the game's save list called it before the seam. Its
// _finddata32_t is MSVC 6's _finddata_t (checked below); its _finddata_t is
// not (64-bit times).
//
// The handle. The runtime's is an intptr_t; the game keeps a 32-bit word.
// Where a pointer is 32 bits the word is the runtime's handle itself, as
// before the seam. Where it is 64 the runtime's handle is an address the
// system chose, which nothing holds below 4 GB (the engine's own process has
// a low heap and never showed it; tools/os_host's does not, and narrowing it
// there made _findnext32 fault), so the word is the number of a slot that
// holds the handle, plus one. The game's own callers never close a search
// (the original's leak, kept), so the slots are not a fixed table.
#include "os/find.h"

#include <io.h>
#include <windows.h>

#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "os/case_check.h"
#include "os/file.h"

namespace bof3::os {

static_assert(offsetof(_finddata32_t, attrib) == offsetof(FindData, attrib) && sizeof(_finddata32_t::attrib) == 4, "attrib");
static_assert(offsetof(_finddata32_t, time_create) == offsetof(FindData, time_create) && sizeof(_finddata32_t::time_create) == 4,
              "time_create");
static_assert(offsetof(_finddata32_t, time_access) == offsetof(FindData, time_access) && sizeof(_finddata32_t::time_access) == 4,
              "time_access");
static_assert(offsetof(_finddata32_t, time_write) == offsetof(FindData, time_write) && sizeof(_finddata32_t::time_write) == 4,
              "time_write");
static_assert(offsetof(_finddata32_t, size) == offsetof(FindData, size) && sizeof(_finddata32_t::size) == 4, "size");
static_assert(offsetof(_finddata32_t, name) == offsetof(FindData, name) && sizeof(_finddata32_t::name) == 260, "name");
static_assert(sizeof(_finddata32_t) == sizeof(FindData), "_finddata32_t is MSVC 6's _finddata_t");

namespace {

constexpr bool kWordHoldsHandle = sizeof(intptr_t) == sizeof(std::int32_t);
constexpr intptr_t kNoHandle = -1;

std::mutex g_lock;
std::vector<intptr_t> g_handles;   // slot n is word n + 1; kNoHandle marks a closed one

// The runtime's handle a word names, or kNoHandle.
intptr_t Handle(std::int32_t word) {
    if (kWordHoldsHandle) return static_cast<intptr_t>(word);
    const std::lock_guard<std::mutex> hold(g_lock);
    if (word < 1 || static_cast<std::size_t>(word) > g_handles.size()) return kNoHandle;
    return g_handles[static_cast<std::size_t>(word) - 1];
}

// The case check (os/case_check.h), only when it is on: the pattern of each
// open search, by the runtime's handle, so that each match FindNext hands is
// held against it as FindFirst's is.
struct Asked {
    std::string pattern;      // as the caller gave it
    std::wstring leaf;      // its leaf pattern, as the system holds names
};
std::mutex g_case_lock;
std::map<intptr_t, Asked> g_case_asked;

// The runtime hands the leaf in the narrow encoding: widened here as Path
// widens a name (win32/file.cpp), but without Path's own check, which would
// ask again of the directory part the pattern's line has named already.
void CheckMatch(const Asked& a, const char* name) {
    const UINT cp = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
    const int len = static_cast<int>(std::strlen(name));
    const int n = len ? MultiByteToWideChar(cp, 0, name, len, nullptr, 0) : 0;
    if (n <= 0) return;
    std::wstring found(static_cast<std::size_t>(n), L'\0');
    if (MultiByteToWideChar(cp, 0, name, len, found.data(), n) != n) return;
    CaseCheckMatch(a.pattern.c_str(), a.leaf, found);
}

void CheckFirst(const char* pattern, intptr_t handle, const char* name) noexcept {
    try {
        Asked a;
        a.pattern = pattern;
        a.leaf = Path(pattern).filename().native();   // Path checks the directory part
        CheckMatch(a, name);
        const std::lock_guard<std::mutex> hold(g_case_lock);
        g_case_asked[handle] = std::move(a);
    } catch (...) {
    }
}

void CheckNext(intptr_t handle, const char* name) noexcept {
    try {
        Asked a;
        {
            const std::lock_guard<std::mutex> hold(g_case_lock);
            const auto it = g_case_asked.find(handle);
            if (it == g_case_asked.end()) return;
            a = it->second;
        }
        CheckMatch(a, name);
    } catch (...) {
    }
}

void CheckClose(intptr_t handle) noexcept {
    try {
        const std::lock_guard<std::mutex> hold(g_case_lock);
        g_case_asked.erase(handle);
    } catch (...) {
    }
}

}  // namespace

std::int32_t FindFirst(const char* pattern, FindData* found) noexcept {
    const intptr_t handle = _findfirst32(pattern, reinterpret_cast<_finddata32_t*>(found));
    if (handle != kNoHandle && CaseCheckOn()) CheckFirst(pattern, handle, found->name);
    if (kWordHoldsHandle || handle == kNoHandle) return static_cast<std::int32_t>(handle);
    try {
        const std::lock_guard<std::mutex> hold(g_lock);
        for (std::size_t i = 0; i < g_handles.size(); ++i) {
            if (g_handles[i] == kNoHandle) {
                g_handles[i] = handle;
                return static_cast<std::int32_t>(i + 1);
            }
        }
        g_handles.push_back(handle);
        return static_cast<std::int32_t>(g_handles.size());
    } catch (...) {
        _findclose(handle);   // no memory for one more slot: the search is not one
        return -1;
    }
}

int FindNext(std::int32_t handle, FindData* found) noexcept {
    const intptr_t h = Handle(handle);
    if (!kWordHoldsHandle && h == kNoHandle) return -1;
    const int r = _findnext32(h, reinterpret_cast<_finddata32_t*>(found));
    if (r == 0 && CaseCheckOn()) CheckNext(h, found->name);
    return r;
}

int FindClose(std::int32_t handle) noexcept {
    const intptr_t h = Handle(handle);
    if (CaseCheckOn()) CheckClose(h);
    if (!kWordHoldsHandle) {
        if (h == kNoHandle) return -1;
        const std::lock_guard<std::mutex> hold(g_lock);
        g_handles[static_cast<std::size_t>(handle) - 1] = kNoHandle;
    }
    return _findclose(h);
}

}  // namespace bof3::os
