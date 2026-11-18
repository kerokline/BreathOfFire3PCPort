// bof3::os named events on POSIX: a named semaphore "/<name>". See
// os/named_event.h.
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
};

NamedEvent* CreateNamedEvent(const char* name) noexcept {
    try {
        const std::string full = std::string("/") + name;
        sem_unlink(full.c_str());   // a stale one of a process that had this number
        sem_t* s = sem_open(full.c_str(), O_CREAT | O_EXCL, 0600, 0u);
        if (s == SEM_FAILED) return nullptr;
        auto* e = new (std::nothrow) NamedEvent{s};
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

void ResetNamedEvent(NamedEvent* e) noexcept {
    while (sem_trywait(e->sem) == 0) {
    }
}

bool WaitNamedEvent(NamedEvent* e, std::uint32_t ms) noexcept {
    timespec until{};
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += static_cast<time_t>(ms / 1000);
    until.tv_nsec += static_cast<long>(ms % 1000) * 1000000L;
    if (until.tv_nsec >= 1000000000L) {
        until.tv_sec += 1;
        until.tv_nsec -= 1000000000L;
    }
    for (;;) {
        if (sem_timedwait(e->sem, &until) == 0) return true;
        if (errno != EINTR) return false;
    }
}

}  // namespace bof3::os
