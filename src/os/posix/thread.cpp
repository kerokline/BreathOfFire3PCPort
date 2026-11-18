#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
// bof3::os on POSIX: the calling thread's stack (os/stack.h), the thread
// sampler's answer that there is none (os/sampler.h), the local time of day
// (os/local_time.h).
#include "os/local_time.h"
#include "os/posix/own_stack_range.h"
#include "os/sampler.h"
#include "os/stack.h"

#include <errno.h>
#include <pthread.h>
#include <time.h>

namespace bof3::os {

StackRange CurrentStack() noexcept {
    // On a stack of the program's own (os/own_stack.h), that stack's range.
    StackRange own;
    if (SwitchedStack(&own)) return own;
    // Asked once per thread: for the main thread glibc reads /proc/self/maps
    // to answer, and the harnesses ask for every byte they guard.
    thread_local StackRange cached;
    thread_local bool asked = false;
    if (!asked) {
        asked = true;
        pthread_attr_t attr;
        if (pthread_getattr_np(pthread_self(), &attr) == 0) {
            void* addr = nullptr;
            std::size_t size = 0;
            if (pthread_attr_getstack(&attr, &addr, &size) == 0 && addr) {
                cached.limit = addr;
                cached.base = static_cast<char*>(addr) + size;
            }
            pthread_attr_destroy(&attr);
        }
    }
    return cached;
}

// No sampler on POSIX (os/sampler.h): every call says so.
bool CanSampleThreads() noexcept { return false; }

SampledThread* SampleThisThread() noexcept {
    errno = ENOSYS;
    return nullptr;
}

bool SampleThread(SampledThread*, void (*)(void*, std::uintptr_t, std::uintptr_t), void*) noexcept {
    errno = ENOSYS;
    return false;
}

LocalTime LocalNow() noexcept {
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    tm t{};
    const time_t secs = ts.tv_sec;
    localtime_r(&secs, &t);
    LocalTime r;
    r.hour = static_cast<unsigned>(t.tm_hour);
    r.minute = static_cast<unsigned>(t.tm_min);
    r.second = static_cast<unsigned>(t.tm_sec);
    r.millisecond = static_cast<unsigned>(ts.tv_nsec / 1000000);
    return r;
}

}  // namespace bof3::os
