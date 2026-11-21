// bof3::os on Windows: a run on a placed stack is refused, and what holds a
// range is VirtualQuery's answer (os/placed.h).
#include "os/placed.h"

#include <windows.h>

#include <cstdio>

namespace bof3::os {

// Refused, nothing run (os/placed.h): the loader places the main thread's
// stack from the executable's header, and no Windows start-up asks for a
// placed one. The caller turns the false into its Fatal.
bool RunOnPlacedStack(void*, void*, void (*)(void*), void*) noexcept {
    SetLastError(ERROR_NOT_SUPPORTED);
    return false;
}

std::string MemHolders(std::uintptr_t lo, std::uintptr_t hi) {
    std::string out;
    for (std::uintptr_t a = lo; a < hi;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(reinterpret_cast<const void*>(a), &m, sizeof m)) break;
        const auto r_lo = reinterpret_cast<std::uintptr_t>(m.BaseAddress);
        const std::uintptr_t r_hi = r_lo + m.RegionSize;
        if (m.State != MEM_FREE) {
            char name[MAX_PATH] = "";
            if (m.Type == MEM_IMAGE) GetModuleFileNameA(static_cast<HMODULE>(m.AllocationBase), name, sizeof name);
            char line[MAX_PATH + 96];
            std::snprintf(line, sizeof line, "%llx-%llx state 0x%lX type 0x%lX protect 0x%lX%s%s",
                          static_cast<unsigned long long>(r_lo), static_cast<unsigned long long>(r_hi), m.State, m.Type,
                          m.Protect, *name ? " " : "", name);
            if (!out.empty()) out += '\n';
            out += line;
        }
        if (r_hi <= a) break;
        a = r_hi;
    }
    return out;
}

}  // namespace bof3::os
