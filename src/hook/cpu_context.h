// A thread's CONTEXT by width (docs/x64-build-round.md 2: what differs by
// width lives in a hook or common header, never in a game file): the x86
// record names its program counter and stack pointer Eip / Esp, the x86-64
// one Rip / Rsp. These read either as a native integer, as wide as the
// register. The readers: the gap watch, the stall watch, the call trace and
// the write watch (gapwatch.cpp, stallwatch.cpp, calltrace.cpp,
// write_watch.cpp), each of which includes <windows.h> itself.
//
// The readers are templates over the record's type, so this header names no
// Windows type and needs no Windows header (docs/linux-round.md LB7): a
// caller passes its CONTEXT, and only the caller's file is a Windows file.
// The crash report reads its registers from the catcher's own copy instead
// (crash_report.h).
#pragma once

#include <cstdint>
#include <cstdio>

namespace bof3 {

// A register's value as hex text, as wide as the register: 8 digits on
// i686, 16 on x86-64 - for a game file's log line, which prints it with %s
// and so carries no width of its own (the exit trace, win_main.cpp).
struct CtxText {
    char text[2 * sizeof(std::uintptr_t) + 1];
};
inline CtxText CtxHex(std::uintptr_t v) {
    CtxText t;
    std::snprintf(t.text, sizeof t.text, "%0*llX", static_cast<int>(2 * sizeof(std::uintptr_t)), static_cast<unsigned long long>(v));
    return t;
}

template <class Context>
[[gnu::always_inline]] inline std::uintptr_t CtxIp(const Context& c) {
#if defined(__i386__)
    return c.Eip;
#elif defined(__x86_64__)
    return c.Rip;
#else
#error "cpu_context: no program counter for this architecture"
#endif
}

template <class Context>
[[gnu::always_inline]] inline std::uintptr_t CtxSp(const Context& c) {
#if defined(__i386__)
    return c.Esp;
#elif defined(__x86_64__)
    return c.Rsp;
#else
#error "cpu_context: no stack pointer for this architecture"
#endif
}

// The program counter written back: a handler that resumes elsewhere (the
// call trace's int3 at a traced entry resumes at the entry itself).
template <class Context>
[[gnu::always_inline]] inline void CtxSetIp(Context& c, std::uintptr_t ip) {
#if defined(__i386__)
    c.Eip = ip;
#elif defined(__x86_64__)
    c.Rip = ip;
#else
#error "cpu_context: no program counter for this architecture"
#endif
}

}  // namespace bof3
