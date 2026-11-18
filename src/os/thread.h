// A thread that can be asked whether it is gone, and the process's abrupt end.
//
// std::thread cannot answer "has this thread ended": the log (src/hook/log.cpp)
// asks it at exit, where Windows' ExitProcess has already ended every other
// thread - its writer among them, perhaps in the middle of a write it must not
// wait for. And the standard's own ends (std::exit, std::_Exit, std::abort)
// run DLL detach code or raise a signal on Windows; Fatal wants
// TerminateProcess, which runs nothing of the process's.
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdint>

namespace bof3::os {

using Thread = void*;   // null: no thread

// `fn(arg)` on a new thread (CreateThread / pthread_create, detached). Null
// when the thread cannot be made. The handle lives as long as the process.
Thread StartThread(void (*fn)(void*), void* arg);

// True once the thread has returned from `fn` or been ended by the system
// (Windows: its handle is signalled; POSIX: it returned - a POSIX process ends
// no thread before it ends itself).
bool ThreadEnded(Thread t);

// The process ended now with `code`: no exit handlers, no static destructors,
// no DLL detach (TerminateProcess on the process itself; POSIX _exit).
[[noreturn]] void EndProcess(std::uint32_t code);

}  // namespace bof3::os
