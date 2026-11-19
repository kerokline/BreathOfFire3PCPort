// bof3::os, an event another process can set by name: the frozen shot's
// release (BOF3X_SHOT_WAIT, hook/input_script.cpp), which a driver such as
// tools/input_run.py sets after it has grabbed the window.
//
// Windows: an auto-reset event (CreateEventW, not signalled at first) named
// "Local\<name>", ResetEvent, WaitForSingleObject. POSIX: a named semaphore
// "/<name>" (sem_open, 0 at first), drained to 0 for a reset and waited for
// against the monotonic clock, as Windows counts a wait's milliseconds
// (sem_clockwait; where the C library has none, a poll each millisecond - not
// sem_timedwait, whose deadline follows the wall clock and its steps); a
// driver sets it with sem_post. A set event a wait has
// not taken yet stays set on both (POSIX counts each set, and the reset before
// each wait drains them). The name lives with the process on Windows; on
// POSIX it stays in /dev/shm until someone unlinks it (sem_unlink), and a
// stale one of the same name is unlinked before it is made again.
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdint>

namespace bof3::os {

struct NamedEvent;   // kept for the process's life

// `name`: letters, digits and '_' (the backend adds the system's prefix).
// Null on an error (LastError).
NamedEvent* CreateNamedEvent(const char* name) noexcept;
// Not set any more.
void ResetNamedEvent(NamedEvent* e) noexcept;
// True when the event was set within `ms` milliseconds (and is taken: an
// auto-reset event), false when the time ran out or the wait failed.
bool WaitNamedEvent(NamedEvent* e, std::uint32_t ms) noexcept;

// How this build waits, in words. For the tests, which say what was run.
const char* NamedEventWaitWay() noexcept;

}  // namespace bof3::os
