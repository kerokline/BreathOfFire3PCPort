// bof3::os environment on Windows. See os/env.h.

#include "os/env.h"
#include "os/os.h"

#include <windows.h>

namespace bof3::os {

std::uint32_t GetEnv(const char* name, char* buf, std::uint32_t cap) { return GetEnvironmentVariableA(name, buf, cap); }
std::uint32_t GetEnvW(const wchar_t* name, wchar_t* buf, std::uint32_t cap) { return GetEnvironmentVariableW(name, buf, cap); }

namespace {
bool PathOf(HMODULE m, std::string* utf8) {
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(m, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;
    *utf8 = ToUtf8(path);
    return true;
}
}  // namespace

bool ExecutablePath(std::string* utf8) { return PathOf(nullptr, utf8); }
bool ModulePath(const void* inside, std::string* utf8, std::uintptr_t* base) {
    HMODULE m = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<LPCWSTR>(inside), &m))
        return false;
    if (base) *base = reinterpret_cast<std::uintptr_t>(m);
    return PathOf(m, utf8);
}

std::uint32_t ThreadId() { return GetCurrentThreadId(); }
std::uint32_t ProcessId() { return GetCurrentProcessId(); }

std::uint32_t LastError() { return GetLastError(); }

}  // namespace bof3::os
