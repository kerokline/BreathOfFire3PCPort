// Clocks: a millisecond tick and a high-resolution counter.
//
// Not std::chrono::steady_clock: game_clock.cpp (DIV-0022, DIV-0047) must keep
// its numbers on Windows exactly, and those come from GetTickCount64 (which
// steps 15.6 ms at a time), GetTickCount (which wraps at 49.7 days) and
// QueryPerformanceCounter at its own frequency. steady_clock would change the
// step, the wrap and the frequency. On POSIX the same four calls run on
// CLOCK_MONOTONIC. Every clock here is monotonic and never steps back.
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdint>

namespace bof3::os {

std::uint64_t TickMs64();  // milliseconds since boot (GetTickCount64)
std::uint32_t TickMs32();  // its low 32 bits (GetTickCount)
std::uint64_t Counter();   // ticks of CounterFrequency() per second (QueryPerformanceCounter)
std::uint64_t CounterFrequency();

}  // namespace bof3::os
