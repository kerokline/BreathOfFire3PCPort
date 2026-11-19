#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os on POSIX: a function run on a stack of its own (os/own_stack.h).
// The stack is mapped with a guard page under it; the switch is
// swapcontext, whose saved context holds each side's x87 environment and
// MXCSR (glibc's x86 swapcontext: fnstenv / fldenv, stmxcsr / ldmxcsr), as
// Windows' FIBER_FLAG_FLOAT_SWITCH keeps each fiber's. makecontext's frame
// ends the unwind information at the stack's start, so a debugger or a
// backtrace walking up from the function stops there.
//
// While a run is on the stack, this thread's os::CurrentStack() answers the
// stack's range (own_stack_range.h, read by posix/thread.cpp).
#include "os/own_stack.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <new>

#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

#include "os/posix/own_stack_range.h"

#if !defined(__i386__) && !defined(__x86_64__)
#error "os/own_stack: the x87 control word is carried on x86 alone; this architecture needs its own answer"
#endif

namespace bof3::os {

struct OwnStack::State {
    const OwnStackSpec* spec = nullptr;
    void* map = nullptr;           // the guard page and the stack
    std::size_t map_bytes = 0;
    StackRange range;              // [limit, base): the stack above the guard
    ucontext_t own{};              // the own stack, where it last left off
    ucontext_t* caller = nullptr;  // where the running run returns to
    void (*fn)(void*) = nullptr;
    void* arg = nullptr;
    unsigned short cw = 0;
    bool busy = false;
};

namespace {

thread_local const StackRange* t_switched = nullptr;   // the own stack this thread runs on, if any
thread_local OwnStack::State* t_starting = nullptr;    // the state a first switch hands to Entry

void Entry() noexcept {
    OwnStack::State* const s = t_starting;
    for (;;) {
        if (s->spec->callers_x87) __asm__ volatile("fldcw %0" : : "m"(s->cw));
        s->fn(s->arg);
        s->fn = nullptr;
        // Fails only if sigprocmask does; there is no caller to tell, and
        // running on would call a null function: the process ends here.
        if (swapcontext(&s->own, s->caller) != 0) std::abort();
    }
}

std::size_t PageBytes() {
    const long page = sysconf(_SC_PAGESIZE);
    return page > 0 ? static_cast<std::size_t>(page) : 0x1000;
}

// The mapping, the guard and the first context. False with errno set.
bool Make(OwnStack::State* s, std::size_t reserve) {
    const std::size_t page = PageBytes();
    const std::size_t stack = (reserve + page - 1) / page * page;
    s->map_bytes = stack + page;
    void* const map = mmap(nullptr, s->map_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (map == MAP_FAILED) return false;
    if (mprotect(map, page, PROT_NONE) != 0) {
        const int error = errno;
        munmap(map, s->map_bytes);
        errno = error;
        return false;
    }
    s->map = map;
    s->range.limit = static_cast<char*>(map) + page;
    s->range.base = static_cast<char*>(map) + s->map_bytes;
    if (getcontext(&s->own) != 0) return false;
    s->own.uc_stack.ss_sp = s->range.limit;
    s->own.uc_stack.ss_size = stack;
    s->own.uc_link = nullptr;   // Entry never returns
    makecontext(&s->own, reinterpret_cast<void (*)()>(&Entry), 0);
    return true;
}

}  // namespace

bool SwitchedStack(StackRange* out) noexcept {
    if (!t_switched) return false;
    *out = *t_switched;
    return true;
}

const char* OwnStackFailure(OwnStackResult result) noexcept {
    switch (result) {
    case OwnStackResult::Done: return "no failure";
    case OwnStackResult::NoStack: return "the own stack's mmap, mprotect or getcontext failed";
    case OwnStackResult::NoFiber: return "the switch failed";
    case OwnStackResult::NotGivenBack: return "the switch back failed";
    case OwnStackResult::Nested: return "a run of the own stack from itself";
    }
    return "an unknown result";
}

OwnStackResult OwnStack::Run(void (*fn)(void*), void* arg) noexcept {
    bool first = false;
    if (!state_) {
        auto* s = new (std::nothrow) State;
        if (!s) {
            errno = ENOMEM;
            return OwnStackResult::NoStack;
        }
        s->spec = &spec_;
        if (!Make(s, spec_.reserve)) {
            const int error = errno;
            if (s->map) munmap(s->map, s->map_bytes);
            delete s;
            errno = error;
            return OwnStackResult::NoStack;
        }
        state_ = s;
        first = true;
    }
    State* const s = state_;
    if (s->busy) return OwnStackResult::Nested;
    ucontext_t back;
    if (spec_.callers_x87) __asm__ volatile("fnstcw %0" : "=m"(s->cw));
    s->caller = &back;
    s->fn = fn;
    s->arg = arg;
    s->busy = true;
    const StackRange* const outer = t_switched;
    t_switched = &s->range;
    if (first) t_starting = s;
    const int switched = swapcontext(&back, &s->own);
    t_switched = outer;
    s->busy = false;
    if (switched != 0) return OwnStackResult::NoFiber;   // nothing ran: the switch itself failed (errno)
    return OwnStackResult::Done;
}

}  // namespace bof3::os
