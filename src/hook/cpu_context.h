// A thread's CONTEXT by width (docs/x64-build-round.md 2: what differs by
// width lives in a hook or common header, never in a game file): the x86
// record names its program counter and stack pointer Eip / Esp, the x86-64
// one Rip / Rsp. These read either as a native integer, as wide as the
// register. The readers: the crash reporter, the gap watch, the stall watch,
// the call trace and the exit trace (crash.cpp, gapwatch.cpp, stallwatch.cpp,
// calltrace.cpp, win_main.cpp).
#pragma once

#include <windows.h>

#include <cstdint>

namespace bof3 {

[[gnu::always_inline]] inline std::uintptr_t CtxIp(const CONTEXT& c) {
#if defined(__i386__)
    return c.Eip;
#elif defined(__x86_64__)
    return c.Rip;
#else
#error "cpu_context: no program counter for this architecture"
#endif
}

[[gnu::always_inline]] inline std::uintptr_t CtxSp(const CONTEXT& c) {
#if defined(__i386__)
    return c.Esp;
#elif defined(__x86_64__)
    return c.Rsp;
#else
#error "cpu_context: no stack pointer for this architecture"
#endif
}

}  // namespace bof3
