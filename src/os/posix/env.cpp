#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os environment on Linux: getenv, /proc/self/exe, dladdr. See os/env.h.

#include "os/env.h"
#include "os/os.h"

#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace bof3::os {

std::uint32_t GetEnv(const char* name, char* buf, std::uint32_t cap) {
    const char* v = std::getenv(name);
    if (!v) return 0;
    const std::size_t n = std::strlen(v);
    if (n + 1 > cap) return static_cast<std::uint32_t>(n + 1);
    std::memcpy(buf, v, n + 1);
    return static_cast<std::uint32_t>(n);
}

std::uint32_t GetEnvW(const wchar_t* name, wchar_t* buf, std::uint32_t cap) {
    const char* v = std::getenv(ToUtf8(name).c_str());
    if (!v) return 0;
    const std::wstring w = ToWide(v);
    if (w.size() + 1 > cap) return static_cast<std::uint32_t>(w.size() + 1);
    std::memcpy(buf, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
    return static_cast<std::uint32_t>(w.size());
}

bool ExecutablePath(std::string* utf8) {
    char path[4096];
    const ssize_t n = readlink("/proc/self/exe", path, sizeof path - 1);
    if (n <= 0) return false;
    utf8->assign(path, static_cast<std::size_t>(n));
    return true;
}

bool ModulePath(const void* inside, std::string* utf8, std::uintptr_t* base) {
    Dl_info info;
    if (dladdr(inside, &info) && info.dli_fname && info.dli_fname[0] == '/') {
        *utf8 = info.dli_fname;
        if (base) *base = reinterpret_cast<std::uintptr_t>(info.dli_fbase);
        return true;
    }
    if (base) *base = 0;
    return ExecutablePath(utf8);   // the code is in the program itself
}

std::uint32_t ThreadId() { return static_cast<std::uint32_t>(syscall(SYS_gettid)); }
std::uint32_t ProcessId() { return static_cast<std::uint32_t>(getpid()); }

std::uint32_t LastError() { return static_cast<std::uint32_t>(errno); }

}  // namespace bof3::os
