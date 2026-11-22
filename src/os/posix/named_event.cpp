#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1   // sem_clockwait
#endif
// bof3::os named events on POSIX: a named semaphore "/<name>". See
// os/named_event.h.
//
// The wait's bound is on the monotonic clock. sem_timedwait's deadline is an
// absolute CLOCK_REALTIME time, so a step of the wall clock during the wait
// (NTP, the user, a suspended virtual machine's catch-up) would stretch or cut
// it; sem_clockwait (glibc 2.30) takes the clock. Where the C library has no
// sem_clockwait, the semaphore is polled each millisecond against a monotonic
// deadline: late by up to a millisecond, never moved by the wall clock.
#include "os/named_event.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>

#include <new>
#include <string>

namespace bof3::os {

struct NamedEvent {
    sem_t* sem;
    std::string name;   // "/<name>", for sem_unlink
};

NamedEvent* CreateNamedEvent(const char* name) noexcept {
    try {
        const std::string full = std::string("/") + name;
        sem_unlink(full.c_str());   // a stale one of a process that had this number
        sem_t* s = sem_open(full.c_str(), O_CREAT | O_EXCL, 0600, 0u);
        if (s == SEM_FAILED) return nullptr;
        auto* e = new (std::nothrow) NamedEvent{s, full};
        if (!e) {
            sem_close(s);
            sem_unlink(full.c_str());
            errno = ENOMEM;
        }
        return e;
    } catch (...) {
        errno = ENOMEM;
        return nullptr;
    }
}

void CloseNamedEvent(NamedEvent* e) noexcept {
    if (!e) return;
    sem_close(e->sem);
    sem_unlink(e->name.c_str());
    delete e;
}

void ResetNamedEvent(NamedEvent* e) noexcept {
    while (sem_trywait(e->sem) == 0) {
    }
}

namespace {

timespec MonotonicIn(std::uint32_t ms) noexcept {
    timespec until{};
    clock_gettime(CLOCK_MONOTONIC, &until);
    until.tv_sec += static_cast<time_t>(ms / 1000);
    until.tv_nsec += static_cast<long>(ms % 1000) * 1000000L;
    if (until.tv_nsec >= 1000000000L) {
        until.tv_sec += 1;
        until.tv_nsec -= 1000000000L;
    }
    return until;
}

// BOF3X_NO_SEM_CLOCKWAIT builds the polling form where sem_clockwait is there
// too: for the tests (tools/os_host), which run both.
#if !defined(BOF3X_NO_SEM_CLOCKWAIT) && defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 30)
#define BOF3X_SEM_CLOCKWAIT 1
#endif
#endif

}  // namespace

bool WaitNamedEvent(NamedEvent* e, std::uint32_t ms) noexcept {
    const timespec until = MonotonicIn(ms);
#ifdef BOF3X_SEM_CLOCKWAIT
    for (;;) {
        if (sem_clockwait(e->sem, CLOCK_MONOTONIC, &until) == 0) return true;
        if (errno != EINTR) return false;   // ETIMEDOUT, or a refusal
    }
#else
    for (;;) {
        if (sem_trywait(e->sem) == 0) return true;
        if (errno != EAGAIN && errno != EINTR) return false;
        timespec now{};
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > until.tv_sec || (now.tv_sec == until.tv_sec && now.tv_nsec >= until.tv_nsec)) {
            errno = ETIMEDOUT;
            return false;
        }
        // A millisecond, or what is left of the wait; a signal only ends the nap early.
        timespec nap = now;
        nap.tv_nsec += 1000000L;
        if (nap.tv_nsec >= 1000000000L) {
            nap.tv_sec += 1;
            nap.tv_nsec -= 1000000000L;
        }
        if (nap.tv_sec > until.tv_sec || (nap.tv_sec == until.tv_sec && nap.tv_nsec > until.tv_nsec)) nap = until;
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &nap, nullptr);
    }
#endif
}

const char* NamedEventWaitWay() noexcept {
#ifdef BOF3X_SEM_CLOCKWAIT
    return "sem_clockwait on the monotonic clock";
#else
    return "a poll each millisecond against the monotonic clock";
#endif
}

}  // namespace bof3::os
