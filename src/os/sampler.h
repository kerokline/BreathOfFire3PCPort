// bof3::os, another thread held still and read: where a stuck thread is, for
// the stall watch (BOF3X_STALLWATCH, hook/stallwatch.cpp, known-defects D304).
//
// Windows: SuspendThread, GetThreadContext, ResumeThread on a handle the
// thread duplicated for itself. POSIX has no call that reads another thread's
// registers: it takes a signal handler on the held thread and a hand-over of
// its context, a design of its own that is not written. So on POSIX
// CanSampleThreads() is false and a caller refuses its switch there; the other
// two calls answer that no thread can be sampled (null, false) and never
// pretend to have read one.
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdint>

namespace bof3::os {

struct SampledThread;   // a handle on a thread that another thread can hold and read

// Whether this system can hold a thread and read its registers.
bool CanSampleThreads() noexcept;

// A handle on the calling thread, for SampleThread from another thread, kept
// for the process's life. Null on POSIX, or on an error (LastError).
SampledThread* SampleThisThread() noexcept;

// Holds `t` still, hands its program counter and stack pointer to `read`, and
// lets it go. While it is held, `read` must take no lock the held thread may
// hold: no heap, no log, no stdio; MemQuery (os/memory.h) and plain reads of
// committed memory are fine. False, and `read` not called, when the thread
// could not be held or read; LastError then holds the system's number.
bool SampleThread(SampledThread* t, void (*read)(void* ctx, std::uintptr_t ip, std::uintptr_t sp), void* ctx) noexcept;

}  // namespace bof3::os
