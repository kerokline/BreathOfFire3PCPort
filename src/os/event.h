// Events: Win32's event object, its semantics kept on every system.
//
// Two kinds of user. The game's own: DirectSound's position notifications
// hand the mixer (src/audio/sound_sdl.cpp) the handles the game made through
// its CreateEventA import and waits on through its MsgWaitForMultipleObjects
// import, so the mixer must signal exactly the object the game waits on - on
// Windows a kernel event, whichever way the import slot is bound. And ours:
// the log's writer thread (src/hook/log.cpp) sleeps on one.
//
// The semantics are CreateEvent's: an auto-reset event releases one wait and
// clears itself; a manual-reset event stays set, releasing every wait, until
// EventReset. Setting a set event does nothing more; a set made while nobody
// waits is kept for the next wait.
//
// Windows: CreateEventA, SetEvent, ResetEvent, WaitForSingleObject,
// CloseHandle - the same calls the users made before the seam. POSIX: an
// object of ours, a std::mutex and a std::condition_variable, from a static
// pool, so a handle is an address in the program's own data (the game keeps
// its two in 32-bit words, Music_Events, as it keeps sound_sdl.cpp's buffers).
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdint>

namespace bof3::os {

using Event = void*;   // null: no event

inline constexpr std::uint32_t kWaitForever = 0xFFFFFFFFu;   // INFINITE

enum class WaitResult : std::uint8_t { Signalled, Timeout, Failed };

// An unnamed event. Null when none can be made (LastError says why on Windows;
// on POSIX the pool of kEventPool is full).
Event EventCreate(bool manual_reset, bool initially_set);
// False for a handle that is not a live event.
bool EventSet(Event e);
bool EventReset(Event e);
// Waits up to `ms` milliseconds (0: a poll; kWaitForever: no bound) for the
// event to be set; an auto-reset event that releases the wait is cleared.
WaitResult EventWait(Event e, std::uint32_t ms);
// The event given back. A wait in progress on it is undefined, as on Windows.
void EventClose(Event e);

// POSIX: how many events may live at once.
inline constexpr unsigned kEventPool = 64;

}  // namespace bof3::os
