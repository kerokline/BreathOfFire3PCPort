// bof3::os clocks on Windows: GetTickCount64, QueryPerformanceCounter. See os/clock.h.

#include "os/clock.h"
#include "os/os.h"

#include <windows.h>

namespace bof3::os {

std::uint64_t TickMs64() { return GetTickCount64(); }
std::uint32_t TickMs32() { return GetTickCount(); }
std::uint64_t Counter() {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return static_cast<std::uint64_t>(q.QuadPart);
}
std::uint64_t CounterFrequency() {
    LARGE_INTEGER q;
    QueryPerformanceFrequency(&q);
    return static_cast<std::uint64_t>(q.QuadPart);
}

}  // namespace bof3::os
