#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os clocks on Linux: clock_gettime(CLOCK_MONOTONIC). See os/clock.h.

#include "os/clock.h"
#include "os/os.h"

#include <time.h>

namespace bof3::os {

namespace {
std::uint64_t Nanos() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<std::uint64_t>(ts.tv_nsec);
}
}  // namespace

std::uint64_t TickMs64() { return Nanos() / 1000000ull; }
std::uint32_t TickMs32() { return static_cast<std::uint32_t>(TickMs64()); }
std::uint64_t Counter() { return Nanos(); }
std::uint64_t CounterFrequency() { return 1000000000ull; }

}  // namespace bof3::os
