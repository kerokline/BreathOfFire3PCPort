// bof3::os, the calling thread's stack: where it begins and ends, for a
// harness that asks whether an address is a caller's local (the self-tests'
// guarded reads, game/boss_harness.cpp and game/scenario_harness.cpp) and for
// the stall watch's scan (hook/stallwatch.cpp).
//
// Windows: the TIB's StackLimit and StackBase (NtCurrentTeb), which follow a
// fiber the thread switched to. POSIX: pthread_getattr_np's stack of the
// thread, asked once per thread; while the thread runs on a stack of the
// program's own (os/own_stack.h), that stack's range instead: the renderer's,
// the quicksave's and the MP3 decoder's (audio/mp3.cpp) are all os::OwnStack,
// so both backends answer for each. A stack switched to by any other means
// would not be known here - on it the answer would still be the thread's -
// and the code has none. (The game's task slices lie inside the thread's own
// stack, so the thread's answer holds for them.)
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

namespace bof3::os {

// [limit, base): the stack grows down from base. Windows' limit is the lowest
// committed page (it moves down as the stack grows); POSIX's is the lowest
// byte the thread's stack may use. Both null when the system does not say.
struct StackRange {
    void* limit = nullptr;
    void* base = nullptr;
};
StackRange CurrentStack() noexcept;

}  // namespace bof3::os
