// bof3::os, a function run on a stack of its own: call and return, the
// stack made at the first run and kept for the process's life
// (docs/linux-round.md, wave 2, group R7).
//
// Two engine paths need it, for one reason: the game runs its tasks on
// slices of the main thread's own stack (16 KB on i686, task_sched.cpp), and
// a call made from there must not run deep into a slice. The renderer's
// present, set-up and frame save (render_d3d11.cpp, RunOnFiber: a DXGI
// present wants far more than 16 KB, docs/render-backend.md) and the
// quicksave's take and restore (snapshot.cpp, OnOwnStack: a take copies
// slice 0 as the tasks left it, and a restore must not write into it after,
// docs/quicksave-census.md 2.1).
//
// Windows: the fibers as they were before the seam. The thread is made a
// fiber for the run (ConvertThreadToFiberEx, FIBER_FLAG_FLOAT_SWITCH) unless
// it is one already, the stack is a fiber (CreateFiberEx with `commit` and
// `reserve`, FIBER_FLAG_FLOAT_SWITCH), and each run is a SwitchToFiber there
// and back. The thread block's stack bounds follow the fiber, so
// os::CurrentStack() answers for the own stack while the function runs.
//
// POSIX: `reserve` bytes mapped, a page under them left without access (a
// guard: an overrun faults, it never wanders), and a switch by swapcontext,
// which keeps each side's x87 and SSE control state as the fiber's float
// switch does. `commit` has no meaning there: the system gives a page when
// it is first touched. While the function runs, os::CurrentStack() on this
// thread answers the own stack's range; after it, the thread's again. A
// fault on the own stack (its guard page) is a signal like any other: the
// crash catcher's alternate stack (hook/crash_posix.cpp) takes it.
//
// Nesting: a function on one own stack may run another own stack, and the
// inner run returns to the outer stack. A run asked of a stack while that
// stack is running refuses (Nested): it would switch to itself.
//
// One thread at a time: a stack may be run from any thread, but never from
// two at once. The function must not throw: an exception that leaves it ends
// the process (the fiber's start or makecontext's frame has no handler).
//
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstddef>

namespace bof3::os {

struct OwnStackSpec {
    std::size_t commit;     // Windows: committed when the stack is made (CreateFiberEx's first size)
    std::size_t reserve;    // the stack's size
    bool callers_x87;       // the caller's x87 control word loaded on the own stack at every run (else the stack keeps its own)
    bool give_thread_back;  // Windows: a thread made a fiber for a run is made a thread again after it (else it stays a fiber)
};

enum class OwnStackResult {
    Done,           // the function ran and returned
    NoStack,        // the stack could not be made (LastError)
    NoFiber,        // Windows: the thread could not be made a fiber; POSIX: the switch failed, nothing ran (LastError)
    NotGivenBack,   // Windows: the function ran, and the fiber could not be made a thread again (LastError); not on POSIX
    Nested,         // asked while this stack runs: refused, nothing ran
};

// What failed, as the system call's name: "CreateFiberEx failed" and so on;
// "a run of the own stack from itself" for Nested. Static text.
const char* OwnStackFailure(OwnStackResult result) noexcept;

class OwnStack {
public:
    constexpr explicit OwnStack(const OwnStackSpec& spec) noexcept : spec_(spec) {}
    OwnStack(const OwnStack&) = delete;
    OwnStack& operator=(const OwnStack&) = delete;

    // `fn(arg)` on this stack; returns when it returns.
    OwnStackResult Run(void (*fn)(void*), void* arg) noexcept;
    // Whether the stack has been made (by a first run).
    bool Made() const noexcept { return state_ != nullptr; }

    struct State;   // the backend's

private:
    OwnStackSpec spec_;
    State* state_ = nullptr;   // made at the first run, never freed
};

}  // namespace bof3::os
