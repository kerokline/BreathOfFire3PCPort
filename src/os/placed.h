// bof3::os, memory at a place the original fixes: the calling thread moved
// onto a stack the caller has mapped at a fixed address, and what holds a
// range of the address space when a reservation there is refused. For the
// Linux start-up (hook/exe_host_posix.cpp; docs/linux-round.md, "B1 as
// run"): on Windows the main thread's stack lies at 0x60000..0x160000 by the
// executable's header (hook/exe_layout.h kStackReserve; x64-build-design.md
// 7.1), where the task slices' 32-bit stack words can hold it; a Linux
// process's main stack lies near the top of the address space, so the
// start-up maps the same range and runs the engine on it.
//
// RunOnPlacedStack: `fn(arg)` on [limit, base), memory already mapped
// read-write by the caller, on the calling thread; it returns when `fn`
// returns. While `fn` runs, os::CurrentStack() on this thread answers
// [limit, base), as it does for an os::OwnStack (stack.h): the harnesses'
// guarded reads and the stall watch then see the stack the code runs on.
// POSIX: getcontext, makecontext over the range, swapcontext there and back
// (each side's x87 and SSE control state kept, as own_stack.h says). False
// with nothing run when the switch fails (LastError). Windows: refused, false
// with ERROR_NOT_SUPPORTED and nothing run - its loader places the main
// thread's stack from the executable's header, so no Windows start-up asks
// for this; a caller there is a mistake to be stopped loudly by its Fatal.
//
// MemHolders: the mappings that overlap [lo, hi), one per line ("lo-hi
// protection name"), for a refusal's message. POSIX: /proc/self/maps.
// Windows: VirtualQuery's regions, a module's file name for an image.
// Empty when nothing holds any of it.
//
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdint>
#include <string>

namespace bof3::os {

bool RunOnPlacedStack(void* limit, void* base, void (*fn)(void*), void* arg) noexcept;

std::string MemHolders(std::uintptr_t lo, std::uintptr_t hi);

}  // namespace bof3::os
