#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os on POSIX: a run on a stack the caller placed, and what holds a
// range (os/placed.h). The switch is own_stack.cpp's: getcontext, makecontext
// over the caller's range, swapcontext there and back; while the run is on
// it, CurrentStack (posix/thread.cpp) answers its range (own_stack_range.h).
#include "os/placed.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <ucontext.h>

#include "os/posix/own_stack_range.h"

#if !defined(__i386__) && !defined(__x86_64__)
#error "os/placed: written for x86, as os/own_stack is"
#endif

namespace bof3::os {

namespace {

thread_local const StackRange* t_placed = nullptr;   // the placed stack this thread runs on, if any

struct Run {
    void (*fn)(void*);
    void* arg;
    ucontext_t back;
    ucontext_t there;
};
thread_local Run* t_run = nullptr;

void Entry() noexcept {
    Run* const r = t_run;
    r->fn(r->arg);
    // uc_link is null: back by hand, so the caller's context resumes after
    // its swapcontext. Fails only if sigprocmask does; there is no caller to
    // tell, and running on would fall off the context's end.
    swapcontext(&r->there, &r->back);
    __builtin_trap();
}

}  // namespace

bool PlacedStack(StackRange* out) noexcept {
    if (!t_placed) return false;
    *out = *t_placed;
    return true;
}

bool RunOnPlacedStack(void* limit, void* base, void (*fn)(void*), void* arg) noexcept {
    if (!limit || !base || static_cast<char*>(base) <= static_cast<char*>(limit) || !fn || t_placed) {
        errno = EINVAL;
        return false;
    }
    Run r{fn, arg, {}, {}};
    if (getcontext(&r.there) != 0) return false;
    r.there.uc_stack.ss_sp = limit;
    r.there.uc_stack.ss_size = static_cast<std::size_t>(static_cast<char*>(base) - static_cast<char*>(limit));
    r.there.uc_link = nullptr;
    makecontext(&r.there, reinterpret_cast<void (*)()>(&Entry), 0);
    const StackRange range{limit, base};
    Run* const outer_run = t_run;
    t_run = &r;
    t_placed = &range;
    const int switched = swapcontext(&r.back, &r.there);
    t_placed = nullptr;
    t_run = outer_run;
    return switched == 0;
}

std::string MemHolders(std::uintptr_t lo, std::uintptr_t hi) {
    std::string out;
    std::FILE* maps = std::fopen("/proc/self/maps", "r");
    if (!maps) return "(/proc/self/maps cannot be read: " + std::string(std::strerror(errno)) + ")";
    char line[1024];
    while (std::fgets(line, sizeof line, maps)) {
        unsigned long long a = 0, b = 0;
        if (std::sscanf(line, "%llx-%llx", &a, &b) != 2) continue;
        if (b <= lo || a >= hi) continue;
        std::size_t n = std::strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == ' ')) line[--n] = 0;
        if (!out.empty()) out += '\n';
        out += line;
    }
    std::fclose(maps);
    return out;
}

}  // namespace bof3::os
